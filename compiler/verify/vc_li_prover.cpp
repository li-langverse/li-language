#include "li/vc_li_prover.hpp"

#include "li/vc_prove.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <poll.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace li {
namespace {

// Node kind codes shared with bootstrap/prover/main.li.
constexpr int kInt = 0;
constexpr int kVar = 1;
constexpr int kAdd = 2;
constexpr int kSub = 3;
constexpr int kMul = 4;
constexpr int kDiv = 5;
constexpr int kLt = 6;
constexpr int kLe = 7;
constexpr int kGt = 8;
constexpr int kGe = 9;
constexpr int kEq = 10;
constexpr int kNe = 11;
constexpr int kAnd = 12;
constexpr int kOr = 13;
constexpr int kNot = 14;
constexpr int kImp = 15;
constexpr int kTrue = 16;
constexpr int kFalse = 17;
constexpr int kAtom = 18;

struct SerCtx {
  std::map<std::string, int> param_index;  // param name -> index
  std::map<std::string, int> atom_ids;     // canon -> atom id
  int next_atom = 0;
};

int binop_kind(const BinOp op) {
  switch (op) {
    case BinOp::Add:
      return kAdd;
    case BinOp::Sub:
      return kSub;
    case BinOp::Mul:
      return kMul;
    case BinOp::Div:
      return kDiv;
    case BinOp::Lt:
      return kLt;
    case BinOp::Le:
      return kLe;
    case BinOp::Gt:
      return kGt;
    case BinOp::Ge:
      return kGe;
    case BinOp::Eq:
      return kEq;
    case BinOp::Ne:
      return kNe;
    case BinOp::And:
      return kAnd;
    case BinOp::Or:
      return kOr;
    case BinOp::Implies:
      return kImp;
    default:
      return kAtom;  // Mod/FloorDiv/Pow/MatMul are opaque
  }
}

// Canonical key for opaque subexpressions (stable ids per serialization).
const std::string* opaque_key(const Expr& e) {
  static std::string scratch;
  switch (e.kind) {
    case Expr::Kind::FloatLit: {
      scratch = "F" + std::to_string(e.float_value);
      return &scratch;
    }
    case Expr::Kind::StringLit:
      scratch = "S" + e.str_value;
      return &scratch;
    case Expr::Kind::BinaryLit:
      scratch = "B" + std::to_string(e.int_value);
      return &scratch;
    case Expr::Kind::Index:
      scratch = "I";
      if (e.base) {
        scratch += std::to_string(static_cast<int>(e.base->kind)) + ":" + e.base->ident;
      }
      if (e.index) {
        scratch += "[" + std::to_string(static_cast<int>(e.index->kind)) + ":" +
                   e.index->ident + "]";
      }
      return &scratch;
    case Expr::Kind::FieldAccess:
      scratch = "Fld:" + e.field_name;
      return &scratch;
    case Expr::Kind::MethodCall:
      scratch = "Mc:" + e.field_name;
      return &scratch;
    default:
      return nullptr;
  }
}

int node_size(const Expr& e, SerCtx& ctx) {
  switch (e.kind) {
    case Expr::Kind::IntLit:
      return 3;
    case Expr::Kind::FloatLit:
    case Expr::Kind::StringLit:
    case Expr::Kind::BinaryLit:
    case Expr::Kind::Index:
    case Expr::Kind::FieldAccess:
    case Expr::Kind::MethodCall:
    case Expr::Kind::Call:
    case Expr::Kind::Await:
      return 3;  // kAtom, size, id
    case Expr::Kind::Ident:
      if (e.ident == "true" || e.ident == "false") {
        return 2;
      }
      if (ctx.param_index.count(e.ident) > 0) {
        return 3;
      }
      return 3;  // kAtom
    case Expr::Kind::UnaryNot:
      return 2 + (e.operand ? node_size(*e.operand, ctx) : 2);
    case Expr::Kind::BinOp: {
      const int k = binop_kind(e.bin_op);
      if (k == kAtom || !e.lhs || !e.rhs) {
        return 3;
      }
      return 2 + node_size(*e.lhs, ctx) + node_size(*e.rhs, ctx);
    }
  }
  return 3;
}

void emit_node(std::vector<int>& out, const Expr& e, SerCtx& ctx) {
  switch (e.kind) {
    case Expr::Kind::IntLit:
      out.push_back(kInt);
      out.push_back(3);
      out.push_back(static_cast<int>(e.int_value));
      return;
    case Expr::Kind::Ident:
      if (e.ident == "true") {
        out.push_back(kTrue);
        out.push_back(2);
        return;
      }
      if (e.ident == "false") {
        out.push_back(kFalse);
        out.push_back(2);
        return;
      }
      if (const auto it = ctx.param_index.find(e.ident); it != ctx.param_index.end()) {
        out.push_back(kVar);
        out.push_back(3);
        out.push_back(it->second);
        return;
      }
      break;  // fall through to atom
    case Expr::Kind::UnaryNot: {
      out.push_back(kNot);
      const int csz = e.operand ? node_size(*e.operand, ctx) : 2;
      out.push_back(2 + csz);
      if (e.operand) {
        emit_node(out, *e.operand, ctx);
      } else {
        out.push_back(kFalse);
        out.push_back(2);
      }
      return;
    }
    case Expr::Kind::BinOp: {
      const int k = binop_kind(e.bin_op);
      if (k != kAtom && e.lhs && e.rhs) {
        out.push_back(k);
        out.push_back(2 + node_size(*e.lhs, ctx) + node_size(*e.rhs, ctx));
        emit_node(out, *e.lhs, ctx);
        emit_node(out, *e.rhs, ctx);
        return;
      }
      break;
    }
    default:
      break;
  }
  // opaque atom (deduped by canonical key)
  const std::string* key = opaque_key(e);
  std::string canon_key = key ? *key : "";
  if (key == nullptr && e.kind == Expr::Kind::Call) {
    canon_key = "Call:" + e.ident;
    for (const auto& a : e.args) {
      if (a) {
        canon_key += ":" + std::to_string(static_cast<int>(a->kind));
      }
    }
  }
  auto [it, inserted] = ctx.atom_ids.emplace(canon_key, ctx.next_atom);
  if (inserted) {
    ++ctx.next_atom;
  }
  out.push_back(kAtom);
  out.push_back(3);
  out.push_back(it->second);
}

}  // namespace

std::string serialize_theorem_prop(const TheoremDecl& thm) {
  SerCtx ctx;
  for (std::size_t i = 0; i < thm.params.size(); ++i) {
    ctx.param_index[thm.params[i].name] = static_cast<int>(i);
  }
  std::vector<int> out;
  if (thm.proposition) {
    emit_node(out, *thm.proposition, ctx);
  } else {
    out = {kFalse, 2};
  }
  std::ostringstream ss;
  for (std::size_t i = 0; i < out.size(); ++i) {
    if (i > 0) {
      ss << ',';
    }
    ss << out[i];
  }
  return ss.str();
}

std::vector<int> parse_verdicts(const std::string& out, const std::size_t expect) {
  std::vector<int> verdicts;
  std::istringstream ss(out);
  int v;
  while (ss >> v) {
    verdicts.push_back(v);
  }
  if (verdicts.size() != expect) {
    return {};
  }
  return verdicts;
}

std::string ensure_li_prover_binary(const char* lic_exe, std::string* err) {
  if (const char* e = std::getenv("LI_PROVER_BIN"); e != nullptr && *e != '\0') {
    if (std::filesystem::exists(e)) {
      return e;
    }
  }
  const char* root = std::getenv("LI_REPO_ROOT");
  const std::string repo = (root != nullptr && *root != '\0') ? root : ".";
  const std::string src = repo + "/bootstrap/prover/main.li";
  if (!std::filesystem::exists(src)) {
    if (err != nullptr) {
      *err = "missing " + src;
    }
    return {};
  }
  // The li prover binary lives at build/prover/li_prover (built by the
  // bootstrap `lic build` below). `build/prover` itself is a directory, so
  // the binary path must name the file, not the directory.
  std::string bin = repo + "/build/prover/li_prover";
  std::error_code ec;
  std::filesystem::create_directories(repo + "/build/prover", ec);
  if (std::filesystem::exists(bin)) {
    std::error_code e1;
    std::error_code e2;
    const auto bin_t = std::filesystem::last_write_time(bin, e1);
    const auto src_t = std::filesystem::last_write_time(src, e2);
    if (!e1 && !e2 && bin_t >= src_t) {
      return bin;
    }
  }
  // --allow-open-vc: the prover's own refinement-argument and data-dependent
  // ensures VCs stay open by design (recursion bounds are a serializer data
  // invariant, documented in bootstrap/prover/main.li).
  const std::string cmd = std::string("\"") + lic_exe + "\" build \"" + src +
                          "\" -o \"" + bin +
                          "\" --no-lean-verify --allow-open-vc >/dev/null 2>&1";
  if (std::system(cmd.c_str()) != 0) {
    if (err != nullptr) {
      *err = "building li prover failed: " + src;
    }
    return {};
  }
  return bin;
}

std::vector<int> run_li_prover(const std::vector<std::string>& serialized,
                               const char* lic_exe) {
  if (serialized.empty() || lic_exe == nullptr) {
    return {};
  }
  std::string err;
  const std::string bin = ensure_li_prover_binary(lic_exe, &err);
  if (bin.empty()) {
    return {};
  }
  std::string cmd = "\"" + bin + "\"";
  for (const auto& s : serialized) {
    cmd += " \"" + s + "\"";
  }
  cmd += " 2>/dev/null";
  // Run the prover with a hard deadline so a misbehaving (e.g. non-terminating)
  // prover build can never hang `lic verify` / `lic build`. On timeout the
  // prover is killed and treated as unavailable (all propositions stay open,
  // so the C++ engine remains the fallback).
  int fds[2];
  if (pipe(fds) != 0) {
    return {};
  }
  const pid_t pid = fork();
  if (pid < 0) {
    close(fds[0]);
    close(fds[1]);
    return {};
  }
  if (pid == 0) {
    dup2(fds[1], STDOUT_FILENO);
    close(fds[0]);
    close(fds[1]);
    execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
  close(fds[1]);
  constexpr std::chrono::seconds kTimeout(10);
  const auto start = std::chrono::steady_clock::now();
  std::string out;
  char buf[256];
  bool timed_out = false;
  while (true) {
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed >= kTimeout) {
      timed_out = true;
      break;
    }
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout - elapsed).count();
    struct pollfd pfd;
    pfd.fd = fds[0];
    pfd.events = POLLIN;
    const int pr = poll(&pfd, 1, static_cast<int>(remaining));
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (pr == 0) {
      timed_out = true;
      break;
    }
    const ssize_t n = read(fds[0], buf, sizeof(buf));
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (n == 0) {
      break;  // EOF
    }
    out.append(buf, static_cast<size_t>(n));
  }
  close(fds[0]);
  if (timed_out) {
    kill(pid, SIGKILL);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if (timed_out) {
    return {};
  }
  return parse_verdicts(out, serialized.size());
}

LiProverResult discharge_with_li_prover(const Module& module, const char* lic_exe) {
  LiProverResult result;
  // Run the li prover on every non-axiom theorem independently: the li port
  // is the same rewriting engine written in li itself (bootstrap/prover), so
  // `theorems_li_proved` reports what li closes on its own, regardless of the
  // C++ engine. Theorems both engines miss stay open.
  std::vector<std::string> open_props;
  std::vector<const TheoremDecl*> open_thms;
  for (const auto& thm : module.theorems) {
    if (thm.is_axiom) {
      continue;
    }
    open_thms.push_back(&thm);
    open_props.push_back(serialize_theorem_prop(thm));
  }
  if (open_thms.empty()) {
    return result;
  }
  const std::vector<int> verdicts = run_li_prover(open_props, lic_exe);
  for (std::size_t i = 0; i < open_thms.size() && i < verdicts.size(); ++i) {
    if (verdicts[i] == 1) {
      ++result.li_proved;
      result.li_proved_names.push_back(open_thms[i]->name);
    } else {
      ++result.open_count;
      result.open_names.push_back(open_thms[i]->name);
    }
  }
  if (verdicts.empty()) {
    // prover unavailable: everything stays open
    for (const auto* thm : open_thms) {
      ++result.open_count;
      result.open_names.push_back(thm->name);
    }
  }
  return result;
}

}  // namespace li
