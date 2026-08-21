#include "li/vc_prove.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace li {
namespace {

// ---------------------------------------------------------------------------
// Layer 2 native discharge engine.
//
// Propositions are discharged by rewriting the arithmetic fragment:
//   - expansion of products over sums (distributivity),
//   - associative/commutative normalization of + and * with constant folding,
//   - identity rules (x*1 = x, x+0 = x, x-x = 0, x*0 = 0, x/1 = x),
//   - boolean simplification of `and` / `or` / `not` / `implies`,
//   - equality by canonical form, comparisons by constant folding.
//
// Idents are opaque atoms: a theorem `∀ params, prop` closes iff `prop` is a
// tautology of its atoms under these rewrites. This mirrors the ℝ/ring axioms
// of proof-db/math (float stands in for ℝ; see discrepancy-policy.md).
// ---------------------------------------------------------------------------

using Node = std::unique_ptr<Expr>;

Node mk_bin(BinOp op, Node l, Node r) {
  auto n = std::make_unique<Expr>();
  n->kind = Expr::Kind::BinOp;
  n->bin_op = op;
  n->lhs = std::move(l);
  n->rhs = std::move(r);
  return n;
}

Node clone_expr(const Expr& e) {
  auto n = std::make_unique<Expr>();
  n->kind = e.kind;
  n->span = e.span;
  n->int_value = e.int_value;
  n->float_value = e.float_value;
  n->str_value = e.str_value;
  n->ident = e.ident;
  n->bin_op = e.bin_op;
  n->field_name = e.field_name;
  n->lit_suffix = e.lit_suffix;
  if (e.lhs) {
    n->lhs = clone_expr(*e.lhs);
  }
  if (e.rhs) {
    n->rhs = clone_expr(*e.rhs);
  }
  if (e.operand) {
    n->operand = clone_expr(*e.operand);
  }
  if (e.base) {
    n->base = clone_expr(*e.base);
  }
  if (e.index) {
    n->index = clone_expr(*e.index);
  }
  for (const auto& a : e.args) {
    n->args.push_back(a ? clone_expr(*a) : nullptr);
  }
  return n;
}

bool is_addlike(const Expr& e) {
  return e.kind == Expr::Kind::BinOp &&
         (e.bin_op == BinOp::Add || e.bin_op == BinOp::Sub);
}

bool is_num(const Expr& e) {
  return e.kind == Expr::Kind::IntLit || e.kind == Expr::Kind::FloatLit;
}

bool num_value(const Expr& e, bool* is_float, double* v) {
  if (e.kind == Expr::Kind::IntLit) {
    *is_float = false;
    *v = static_cast<double>(e.int_value);
    return true;
  }
  if (e.kind == Expr::Kind::FloatLit) {
    *is_float = true;
    *v = e.float_value;
    return true;
  }
  return false;
}

bool num_is_zero(const Expr& e) {
  bool f = false;
  double v = 0.0;
  return num_value(e, &f, &v) && v == 0.0;
}

bool num_is_one(const Expr& e) {
  bool f = false;
  double v = 0.0;
  return num_value(e, &f, &v) && v == 1.0;
}

}  // namespace

// Constant-fold an arithmetic expression over numeric literals.
//
// Pure-integer expressions fold with exact int64 arithmetic (returning
// `ok == false` on overflow, so a fold can never be unsound). Expressions
// involving a float literal fold with IEEE double arithmetic. Integer
// division folds only when exact, and division by zero never folds
// (inf/NaN are left open rather than approximated).
FoldVal fold_const(const Expr& e) {
  if (e.kind == Expr::Kind::IntLit) {
    FoldVal v;
    v.ok = true;
    v.iv = e.int_value;
    return v;
  }
  if (e.kind == Expr::Kind::FloatLit) {
    FoldVal v;
    v.ok = true;
    v.is_float = true;
    v.fv = e.float_value;
    return v;
  }
  if (e.kind != Expr::Kind::BinOp || !e.lhs || !e.rhs) {
    return FoldVal{};
  }
  const BinOp op = e.bin_op;
  if (op != BinOp::Add && op != BinOp::Sub && op != BinOp::Mul && op != BinOp::Div) {
    return FoldVal{};
  }
  const FoldVal l = fold_const(*e.lhs);
  if (!l.ok) {
    return FoldVal{};
  }
  const FoldVal r = fold_const(*e.rhs);
  if (!r.ok) {
    return FoldVal{};
  }
  FoldVal v;
  v.ok = true;
  if (!l.is_float && !r.is_float) {
    // Exact integer arithmetic, guarded against overflow and non-exact
    // division so the folded value is always what the runtime computes.
    const __int128 a = l.iv;
    const __int128 b = r.iv;
    __int128 res = 0;
    switch (op) {
      case BinOp::Add:
        res = a + b;
        break;
      case BinOp::Sub:
        res = a - b;
        break;
      case BinOp::Mul:
        res = a * b;
        break;
      case BinOp::Div:
        if (b == 0 || (l.iv == INT64_MIN && r.iv == -1)) {
          return FoldVal{};
        }
        if (l.iv % r.iv != 0) {
          return FoldVal{};
        }
        res = a / b;
        break;
      default:
        return FoldVal{};
    }
    if (res < INT64_MIN || res > INT64_MAX) {
      return FoldVal{};
    }
    v.iv = static_cast<std::int64_t>(res);
    return v;
  }
  // Float semantics once either operand is a float.
  v.is_float = true;
  const double a = l.num();
  const double b = r.num();
  switch (op) {
    case BinOp::Add:
      v.fv = a + b;
      return v;
    case BinOp::Sub:
      v.fv = a - b;
      return v;
    case BinOp::Mul:
      v.fv = a * b;
      return v;
    case BinOp::Div:
      if (b == 0.0) {
        return FoldVal{};
      }
      v.fv = a / b;
      return v;
    default:
      return FoldVal{};
  }
}

namespace {

// Distribute `a * b` over sums/differences on either side.
Node distribute(Node a, Node b) {
  if (is_addlike(*a)) {
    const BinOp op = a->bin_op;
    return mk_bin(op, distribute(std::move(a->lhs), clone_expr(*b)),
                  distribute(std::move(a->rhs), clone_expr(*b)));
  }
  if (is_addlike(*b)) {
    const BinOp op = b->bin_op;
    return mk_bin(op, distribute(clone_expr(*a), std::move(b->lhs)),
                  distribute(clone_expr(*a), std::move(b->rhs)));
  }
  return mk_bin(BinOp::Mul, std::move(a), std::move(b));
}

// Fully expand products over sums (used before + canonicalization).
Node expand(const Expr& e) {
  switch (e.kind) {
    case Expr::Kind::BinOp:
      if (e.bin_op == BinOp::Add && e.lhs && e.rhs) {
        return mk_bin(BinOp::Add, expand(*e.lhs), expand(*e.rhs));
      }
      if (e.bin_op == BinOp::Sub && e.lhs && e.rhs) {
        return mk_bin(BinOp::Sub, expand(*e.lhs), expand(*e.rhs));
      }
      if (e.bin_op == BinOp::Mul && e.lhs && e.rhs) {
        return distribute(expand(*e.lhs), expand(*e.rhs));
      }
      if (e.bin_op == BinOp::Div && e.lhs && e.rhs) {
        return mk_bin(BinOp::Div, expand(*e.lhs), expand(*e.rhs));
      }
      return clone_expr(e);
    default:
      return clone_expr(e);
  }
}

std::string canon(const Expr& e);

std::string float_key(const double v) {
  char buf[32];
  std::uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(v), "double must be 64-bit");
  std::memcpy(&bits, &v, sizeof(bits));
  std::snprintf(buf, sizeof(buf), "F%016llx", static_cast<unsigned long long>(bits));
  return buf;
}

std::string key(const std::string& kind, const std::string& body) {
  return kind + "(" + body + ")";
}

struct AddTerm {
  int sign = 1;
  std::string key;
};

void flatten_add(const Expr& e, const int sign, std::vector<AddTerm>& out, double* const_sum,
                 bool* has_float) {
  if (e.kind == Expr::Kind::BinOp && (e.bin_op == BinOp::Add || e.bin_op == BinOp::Sub) &&
      e.lhs && e.rhs) {
    flatten_add(*e.lhs, sign, out, const_sum, has_float);
    flatten_add(*e.rhs, e.bin_op == BinOp::Add ? sign : -sign, out, const_sum, has_float);
    return;
  }
  if (is_num(e)) {
    bool f = false;
    double v = 0.0;
    num_value(e, &f, &v);
    *const_sum += static_cast<double>(sign) * v;
    if (f) {
      *has_float = true;
    }
    return;
  }
  out.push_back({sign, canon(e)});
}

void flatten_mul(const Expr& e, std::vector<std::string>& factors, bool* zero) {
  if (e.kind == Expr::Kind::BinOp && e.bin_op == BinOp::Mul && e.lhs && e.rhs) {
    flatten_mul(*e.lhs, factors, zero);
    flatten_mul(*e.rhs, factors, zero);
    return;
  }
  if (is_num(e)) {
    if (num_is_zero(e)) {
      *zero = true;
    } else if (!num_is_one(e)) {
      factors.push_back(canon(e));
    }
    return;
  }
  factors.push_back(canon(e));
}

// Fully normalize an arithmetic expression: expand products over sums, then
// canonicalize as Sum-of-Products (or Prod / Div when no sums remain).
// `a * (b + c)` and `a * b + a * c` therefore share one canonical form.
std::string canon_arith(const Expr& e) {
  const Node ex = expand(e);
  if (ex->kind == Expr::Kind::BinOp &&
      (ex->bin_op == BinOp::Add || ex->bin_op == BinOp::Sub)) {
    std::vector<AddTerm> terms;
    double const_sum = 0.0;
    bool has_float = false;
    flatten_add(*ex, 1, terms, &const_sum, &has_float);
    // cancel opposite-sign copies of the same term (x - x = 0)
    std::map<std::string, int> net;
    for (const auto& t : terms) {
      net[t.key] += t.sign;
    }
    std::vector<std::pair<int, std::string>> kept;
    for (const auto& [k, s] : net) {
      if (s != 0) {
        kept.emplace_back(s, k);
      }
    }
    std::sort(kept.begin(), kept.end(),
              [](const std::pair<int, std::string>& a,
                 const std::pair<int, std::string>& b) { return a.second < b.second; });
    // identity normalization: x + 0 = x, 0 - a = Neg(a), a - a = 0
    if (kept.empty()) {
      if (const_sum == 0.0 && !has_float) {
        return "I0";
      }
      return key("Sum", std::string("|") + float_key(const_sum));
    }
    if (kept.size() == 1 && const_sum == 0.0 && !has_float) {
      if (kept[0].first > 0) {
        return kept[0].second;
      }
      return key("Neg", kept[0].second);
    }
    std::string body;
    for (const auto& [s, k] : kept) {
      body += (s > 0 ? "+" : "-") + k;
    }
    if (const_sum != 0.0 || (has_float && kept.empty())) {
      body += "|" + float_key(const_sum);
    }
    return key("Sum", body);
  }
  if (ex->kind == Expr::Kind::BinOp && ex->bin_op == BinOp::Mul) {
    std::vector<std::string> factors;
    bool zero = false;
    flatten_mul(*ex, factors, &zero);
    if (zero) {
      return "I0";
    }
    std::sort(factors.begin(), factors.end());
    // identity normalization: x * 1 = x
    if (factors.empty()) {
      return "I1";
    }
    if (factors.size() == 1) {
      return factors[0];
    }
    std::string body;
    for (const auto& f : factors) {
      body += f;
    }
    return key("Prod", body);
  }
  if (ex->kind == Expr::Kind::BinOp && ex->bin_op == BinOp::Div) {
    if (is_num(*ex->rhs) && num_is_one(*ex->rhs)) {
      return canon(*ex->lhs);
    }
    if (is_num(*ex->lhs) && num_is_zero(*ex->lhs)) {
      return "I0";
    }
    return key("Div", canon(*ex->lhs) + "," + canon(*ex->rhs));
  }
  return canon(*ex);
}

const char* cmp_tag(const BinOp op) {
  switch (op) {
    case BinOp::Le:
      return "Le";
    case BinOp::Lt:
      return "Lt";
    case BinOp::Ge:
      return "Ge";
    case BinOp::Gt:
      return "Gt";
    case BinOp::Eq:
      return "Eq";
    case BinOp::Ne:
      return "Ne";
    case BinOp::And:
      return "And";
    case BinOp::Or:
      return "Or";
    case BinOp::Implies:
      return "Imp";
    default:
      return "Op";
  }
}

std::string canon(const Expr& e) {
  switch (e.kind) {
    case Expr::Kind::IntLit:
      return "I" + std::to_string(e.int_value);
    case Expr::Kind::FloatLit:
      return float_key(e.float_value);
    case Expr::Kind::Ident:
      return "V" + e.ident;
    case Expr::Kind::StringLit:
      return key("Str", e.str_value);
    case Expr::Kind::BinaryLit:
      return "I" + std::to_string(e.int_value);
    case Expr::Kind::UnaryNot:
      return key("Not", e.operand ? canon(*e.operand) : std::string());
    case Expr::Kind::UnaryMinus:
      return key("Neg", e.operand ? canon(*e.operand) : std::string());
    case Expr::Kind::Index:
      return key("Idx", (e.base ? canon(*e.base) : std::string()) + "," +
                            (e.index ? canon(*e.index) : std::string()));
    case Expr::Kind::FieldAccess:
      return key("Fld", (e.base ? canon(*e.base) : std::string()) + "." + e.field_name);
    case Expr::Kind::Call: {
      std::string body = e.ident + "(";
      for (std::size_t n = 0; n < e.args.size(); ++n) {
        if (n > 0) {
          body += ",";
        }
        body += e.args[n] ? canon(*e.args[n]) : std::string();
      }
      body += ")";
      return key("Call", body);
    }
    case Expr::Kind::MethodCall:
      return key("MCall", e.field_name);
    case Expr::Kind::Await:
      return key("Await", e.operand ? canon(*e.operand) : std::string());
    case Expr::Kind::BinOp: {
      const BinOp op = e.bin_op;
      if (!e.lhs || !e.rhs) {
        return key("Bad", cmp_tag(op));
      }
      if (op == BinOp::Add || op == BinOp::Sub || op == BinOp::Mul || op == BinOp::Div) {
        return canon_arith(e);
      }
      return key(cmp_tag(op), canon(*e.lhs) + "," + canon(*e.rhs));
    }
  }
  return key("?", std::string());
}

enum class Tri { F, T, U };

Tri tri_not(const Tri t) {
  switch (t) {
    case Tri::T:
      return Tri::F;
    case Tri::F:
      return Tri::T;
    default:
      return Tri::U;
  }
}

Tri fold_cmp(const BinOp op, const double l, const double r) {
  switch (op) {
    case BinOp::Lt:
      return l < r ? Tri::T : Tri::F;
    case BinOp::Le:
      return l <= r ? Tri::T : Tri::F;
    case BinOp::Gt:
      return l > r ? Tri::T : Tri::F;
    case BinOp::Ge:
      return l >= r ? Tri::T : Tri::F;
    case BinOp::Eq:
      return l == r ? Tri::T : Tri::F;
    case BinOp::Ne:
      return l != r ? Tri::T : Tri::F;
    default:
      return Tri::U;
  }
}

// ---------------------------------------------------------------------------
// Linear integer arithmetic (Fourier-Motzkin, exact rational arithmetic).
//
// A comparison goal over linear expressions of atoms closes when its negation
// together with the assumed context constraints is infeasible. Fourier-Motzkin
// elimination is exact over reals, and real-infeasibility implies
// integer-infeasibility, so discharges here are sound (never unsound; possibly
// incomplete — integer-only infeasibilities are missed).
// ---------------------------------------------------------------------------

struct Rat {
  __int128 n = 0;  // numerator
  __int128 d = 1;  // denominator, d > 0
};

Rat rat_norm(const Rat a) {
  Rat r = a;
  if (r.d < 0) {
    r.n = -r.n;
    r.d = -r.d;
  }
  if (r.n == 0) {
    return {0, 1};
  }
  __int128 g = r.n < 0 ? -r.n : r.n;
  __int128 dd = r.d;
  while (dd != 0) {
    __int128 t = dd;
    dd = g % dd;
    g = t;
  }
  r.n /= g;
  r.d /= g;
  return r;
}

Rat rat_add(const Rat a, const Rat b) { return rat_norm({a.n * b.d + b.n * a.d, a.d * b.d}); }
Rat rat_sub(const Rat a, const Rat b) { return rat_norm({a.n * b.d - b.n * a.d, a.d * b.d}); }
Rat rat_mul(const Rat a, const Rat b) { return rat_norm({a.n * b.n, a.d * b.d}); }

int rat_cmp(const Rat a, const Rat b) {
  const __int128 lhs = a.n * b.d;
  const __int128 rhs = b.n * a.d;
  return (lhs > rhs) - (lhs < rhs);
}

// Σ a·x <= rhs  (a per atom-key, rhs rational)
struct LinCon {
  std::vector<std::pair<std::string, __int128>> coeffs;
  Rat rhs;
};

// expr = Σ a·x + c  (c rational constant)
struct LinExpr {
  std::vector<std::pair<std::string, __int128>> coeffs;
  Rat c;
};

void lin_add_coeff(LinExpr* e, const std::string& key, const __int128 v) {
  if (v == 0) {
    return;
  }
  for (auto& [k, c] : e->coeffs) {
    if (k == key) {
      c += v;
      return;
    }
  }
  e->coeffs.emplace_back(key, v);
}

// Linearize an int expression over atoms (canon keys); false if non-linear
// (products of non-constants, division, floats, calls, ...).
bool linearize(const Expr& e, LinExpr* out) {
  switch (e.kind) {
    case Expr::Kind::IntLit:
      out->c = rat_add(out->c, {e.int_value, 1});
      return true;
    case Expr::Kind::Ident:
      lin_add_coeff(out, canon(e), 1);
      return true;
    case Expr::Kind::BinOp:
      if (!e.lhs || !e.rhs) {
        return false;
      }
      switch (e.bin_op) {
        case BinOp::Add:
          return linearize(*e.lhs, out) && linearize(*e.rhs, out);
        case BinOp::Sub: {
          LinExpr r;
          if (!linearize(*e.lhs, out) || !linearize(*e.rhs, &r)) {
            return false;
          }
          for (auto& [k, c] : r.coeffs) {
            lin_add_coeff(out, k, -c);
          }
          out->c = rat_sub(out->c, r.c);
          return true;
        }
        case BinOp::Mul: {
          if (e.lhs->kind == Expr::Kind::IntLit &&
              (e.rhs->kind == Expr::Kind::IntLit || e.rhs->kind == Expr::Kind::Ident ||
               (e.rhs->kind == Expr::Kind::BinOp &&
                (e.rhs->bin_op == BinOp::Add || e.rhs->bin_op == BinOp::Sub)))) {
            LinExpr r;
            if (!linearize(*e.rhs, &r)) {
              return false;
            }
            const __int128 k = e.lhs->int_value;
            for (auto& [ck, cc] : r.coeffs) {
              lin_add_coeff(out, ck, k * cc);
            }
            out->c = rat_add(out->c, rat_mul({k, 1}, r.c));
            return true;
          }
          if (e.rhs->kind == Expr::Kind::IntLit &&
              (e.lhs->kind == Expr::Kind::IntLit || e.lhs->kind == Expr::Kind::Ident ||
               (e.lhs->kind == Expr::Kind::BinOp &&
                (e.lhs->bin_op == BinOp::Add || e.lhs->bin_op == BinOp::Sub)))) {
            LinExpr l;
            if (!linearize(*e.lhs, &l)) {
              return false;
            }
            const __int128 k = e.rhs->int_value;
            for (auto& [ck, cc] : l.coeffs) {
              lin_add_coeff(out, ck, k * cc);
            }
            out->c = rat_add(out->c, rat_mul({k, 1}, l.c));
            return true;
          }
          return false;
        }
        default:
          return false;
      }
    default:
      return false;
  }
}

void lincon_normalize(LinCon* c) {
  std::vector<std::pair<std::string, __int128>> kept;
  for (auto& [k, v] : c->coeffs) {
    if (v != 0) {
      kept.emplace_back(k, v);
    }
  }
  c->coeffs = std::move(kept);
  c->rhs = rat_norm(c->rhs);
}

// constraint: expr(l) - expr(r) <= upper
bool lincon_from_diff(const Expr& l, const Expr& r, const __int128 upper, LinCon* out) {
  LinExpr el;
  LinExpr er;
  if (!linearize(l, &el) || !linearize(r, &er)) {
    return false;
  }
  for (auto& [k, c] : el.coeffs) {
    bool merged = false;
    for (auto& [nk, nv] : out->coeffs) {
      if (nk == k) {
        nv += c;
        merged = true;
        break;
      }
    }
    if (!merged) {
      out->coeffs.emplace_back(k, c);
    }
  }
  for (auto& [k, c] : er.coeffs) {
    bool merged = false;
    for (auto& [nk, nv] : out->coeffs) {
      if (nk == k) {
        nv -= c;
        merged = true;
        break;
      }
    }
    if (!merged) {
      out->coeffs.emplace_back(k, -c);
    }
  }
  out->rhs = rat_sub({upper, 1}, rat_sub(el.c, er.c));
  lincon_normalize(out);
  return true;
}

// Exact Fourier-Motzkin feasibility. Returns true iff the system is
// infeasible (no real solution, hence no integer solution).
bool lia_infeasible(std::vector<LinCon> cons) {
  while (true) {
    for (const auto& c : cons) {
      if (c.coeffs.empty() && rat_cmp(c.rhs, {0, 1}) < 0) {
        return true;
      }
    }
    std::string x;
    for (const auto& c : cons) {
      if (!c.coeffs.empty()) {
        x = c.coeffs[0].first;
        break;
      }
    }
    if (x.empty()) {
      return false;
    }
    std::vector<LinCon> uppers;
    std::vector<LinCon> lowers;
    std::vector<LinCon> rest;
    for (const auto& c : cons) {
      __int128 cx = 0;
      for (const auto& [k, v] : c.coeffs) {
        if (k == x) {
          cx = v;
        }
      }
      if (cx > 0) {
        uppers.push_back(c);
      } else if (cx < 0) {
        lowers.push_back(c);
      } else {
        rest.push_back(c);
      }
    }
    if (uppers.empty() || lowers.empty()) {
      // x is unbounded in one direction; its constraints add no information.
      cons = std::move(rest);
      continue;
    }
    cons = std::move(rest);
    for (const auto& u : uppers) {
      __int128 cu = 0;
      for (const auto& [k, v] : u.coeffs) {
        if (k == x) {
          cu = v;
        }
      }
      for (const auto& l : lowers) {
        __int128 a = 0;
        for (const auto& [k, v] : l.coeffs) {
          if (k == x) {
            a = -v;
          }
        }
        LinCon nc;
        for (const auto& [k, v] : l.coeffs) {
          if (k != x) {
            nc.coeffs.emplace_back(k, cu * v);
          }
        }
        for (const auto& [k, v] : u.coeffs) {
          if (k != x) {
            bool merged = false;
            for (auto& [nk, nv] : nc.coeffs) {
              if (nk == k) {
                nv += a * v;
                merged = true;
                break;
              }
            }
            if (!merged) {
              nc.coeffs.emplace_back(k, a * v);
            }
          }
        }
        nc.rhs = rat_add(rat_mul({a, 1}, u.rhs), rat_mul({cu, 1}, l.rhs));
        lincon_normalize(&nc);
        if (nc.coeffs.empty() && rat_cmp(nc.rhs, {0, 1}) < 0) {
          return true;
        }
        cons.push_back(std::move(nc));
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Assumption context. When discharging `P -> Q`, the conjuncts of `P` are
// added as known facts: equalities and strict/non-strict order edges. An
// order goal then closes by path reachability over the assumed edges
// (transitivity), e.g. `(a < b and b < c) -> a < c`.
// ---------------------------------------------------------------------------

struct OrderEdge {
  std::string from;
  std::string to;
  bool strict = false;
};

struct Ctx {
  std::vector<std::string> eq_pairs;  // canonical "lhs|rhs" equalities
  std::vector<OrderEdge> edges;
  std::vector<LinCon> lia_cons;  // linear form of the assumed facts
};

bool ctx_has_eq(const Ctx& ctx, const std::string& a, const std::string& b) {
  const std::string k1 = a + "|" + b;
  const std::string k2 = b + "|" + a;
  for (const auto& p : ctx.eq_pairs) {
    if (p == k1 || p == k2) {
      return true;
    }
  }
  return false;
}

void ctx_add_fact(Ctx& ctx, const Expr& fact);

void ctx_add_conjuncts(Ctx& ctx, const Expr& fact) {
  if (fact.kind == Expr::Kind::BinOp && fact.bin_op == BinOp::And && fact.lhs && fact.rhs) {
    ctx_add_conjuncts(ctx, *fact.lhs);
    ctx_add_conjuncts(ctx, *fact.rhs);
    return;
  }
  ctx_add_fact(ctx, fact);
}

void ctx_add_fact(Ctx& ctx, const Expr& fact) {
  if (fact.kind != Expr::Kind::BinOp || !fact.lhs || !fact.rhs) {
    return;
  }
  const std::string lk = canon(*fact.lhs);
  const std::string rk = canon(*fact.rhs);
  LinCon lc;
  switch (fact.bin_op) {
    case BinOp::Eq:
      ctx.eq_pairs.push_back(lk + "|" + rk);
      // a == b  <=>  a - b <= 0  and  b - a <= 0
      if (lincon_from_diff(*fact.lhs, *fact.rhs, 0, &lc)) {
        ctx.lia_cons.push_back(lc);
      }
      if (lincon_from_diff(*fact.rhs, *fact.lhs, 0, &lc)) {
        ctx.lia_cons.push_back(lc);
      }
      break;
    case BinOp::Lt:
      ctx.edges.push_back({lk, rk, true});
      // a < b  <=>  a - b <= -1
      if (lincon_from_diff(*fact.lhs, *fact.rhs, -1, &lc)) {
        ctx.lia_cons.push_back(lc);
      }
      break;
    case BinOp::Le:
      ctx.edges.push_back({lk, rk, false});
      if (lincon_from_diff(*fact.lhs, *fact.rhs, 0, &lc)) {
        ctx.lia_cons.push_back(lc);
      }
      break;
    case BinOp::Gt:
      ctx.edges.push_back({rk, lk, true});
      if (lincon_from_diff(*fact.rhs, *fact.lhs, -1, &lc)) {
        ctx.lia_cons.push_back(lc);
      }
      break;
    case BinOp::Ge:
      ctx.edges.push_back({rk, lk, false});
      if (lincon_from_diff(*fact.rhs, *fact.lhs, 0, &lc)) {
        ctx.lia_cons.push_back(lc);
      }
      break;
    default:
      break;
  }
}

// Is `from` reachable to `to` over assumed order edges, with `need_strict`
// requiring at least one strict edge on the path (for `<` / `>` goals)?
bool order_reachable(const Ctx& ctx, const std::string& from, const std::string& to,
                     const bool need_strict) {
  struct Node {
    std::string name;
    bool strict = false;
  };
  std::vector<Node> frontier{{from, false}};
  std::vector<std::string> seen_nonstrict;
  std::vector<std::string> seen_strict;
  const auto seen = [&](const std::string& n, const bool st) {
    const auto& v = st ? seen_strict : seen_nonstrict;
    return std::find(v.begin(), v.end(), n) != v.end();
  };
  const auto mark = [&](const std::string& n, const bool st) {
    (st ? seen_strict : seen_nonstrict).push_back(n);
  };
  mark(from, false);
  while (!frontier.empty()) {
    const Node cur = frontier.back();
    frontier.pop_back();
    if (cur.name == to && (!need_strict || cur.strict)) {
      return true;
    }
    for (const auto& e : ctx.edges) {
      if (e.from != cur.name) {
        continue;
      }
      const bool st = cur.strict || e.strict;
      if (seen(e.to, st)) {
        continue;
      }
      mark(e.to, st);
      frontier.push_back({e.to, st});
    }
  }
  return false;
}

Tri eval_bool(const Expr& e, const Ctx& ctx);

Tri eval_cmp(const BinOp op, const Expr& l, const Expr& r, const Ctx& ctx) {
  // Constant-fold arithmetic before comparing: `1.0 + 1.0 + 1.0 == 3.0`
  // reduces to `3.0 == 3.0` and closes without canonicalization.
  const FoldVal lc = fold_const(l);
  const FoldVal rc = fold_const(r);
  if (lc.ok && rc.ok) {
    return fold_cmp(op, lc.num(), rc.num());
  }
  const std::string lk = canon(l);
  const std::string rk = canon(r);
  if (lk == rk) {
    // a < a / a > a are false; a <= a / a >= a / a == a are true; a != a false.
    switch (op) {
      case BinOp::Lt:
      case BinOp::Gt:
      case BinOp::Ne:
        return Tri::F;
      case BinOp::Le:
      case BinOp::Ge:
      case BinOp::Eq:
        return Tri::T;
      default:
        return Tri::U;
    }
  }
  if (op == BinOp::Eq) {
    // a == b closes from an assumed equality or from a <= b and b <= a
    // (antisymmetry) — a single-direction path is only a <= b, not equality.
    if (ctx_has_eq(ctx, lk, rk) ||
        (order_reachable(ctx, lk, rk, false) && order_reachable(ctx, rk, lk, false))) {
      return Tri::T;
    }
  } else if (op == BinOp::Ne) {
    if (order_reachable(ctx, lk, rk, true) || order_reachable(ctx, rk, lk, true)) {
      return Tri::T;  // a strict order between distinct terms implies a != b
    }
  } else {
    const bool strict_goal = (op == BinOp::Lt || op == BinOp::Gt);
    const bool reversed = (op == BinOp::Gt || op == BinOp::Ge);
    const std::string from = reversed ? rk : lk;
    const std::string to = reversed ? lk : rk;
    if (order_reachable(ctx, from, to, strict_goal)) {
      return Tri::T;
    }
  }
  // Linear integer arithmetic: prove the goal by showing its negation plus
  // the assumed facts is infeasible (Fourier-Motzkin); disprove it by showing
  // the goal itself is infeasible with the facts (e.g. `a < b` rules out
  // `b < a`, so `not (b < a)` closes).
  switch (op) {
    case BinOp::Lt:
    case BinOp::Le:
    case BinOp::Gt:
    case BinOp::Ge: {
      std::vector<LinCon> cons = ctx.lia_cons;
      LinCon ng;
      // negated goal: for Lt/Gt, a >= b means b - a <= 0; for Le/Ge, a > b
      // means b - a <= -1.
      const bool strict_goal_neg = (op == BinOp::Le || op == BinOp::Ge);
      if (lincon_from_diff(op == BinOp::Lt || op == BinOp::Le ? r : l,
                           op == BinOp::Lt || op == BinOp::Le ? l : r,
                           strict_goal_neg ? -1 : 0, &ng)) {
        cons.push_back(ng);
        if (lia_infeasible(cons)) {
          return Tri::T;
        }
      }
      // disprove: the goal itself contradicts the assumed facts
      std::vector<LinCon> dcons = ctx.lia_cons;
      LinCon g;
      const bool strict_goal = (op == BinOp::Lt || op == BinOp::Gt);
      if (lincon_from_diff(l, r, strict_goal ? -1 : 0, &g)) {
        dcons.push_back(g);
        if (lia_infeasible(dcons)) {
          return Tri::F;
        }
      }
      return Tri::U;
    }
    case BinOp::Eq: {
      // a == b iff both a < b and a > b are impossible.
      std::vector<LinCon> c1 = ctx.lia_cons;
      std::vector<LinCon> c2 = ctx.lia_cons;
      LinCon g1;
      LinCon g2;
      if (lincon_from_diff(l, r, -1, &g1) &&
          lincon_from_diff(r, l, -1, &g2)) {
        c1.push_back(g1);
        c2.push_back(g2);
        if (lia_infeasible(c1) && lia_infeasible(c2)) {
          return Tri::T;
        }
      }
      return Tri::U;
    }
    case BinOp::Ne: {
      // a != b iff a == b is impossible.
      std::vector<LinCon> cons = ctx.lia_cons;
      LinCon g1;
      LinCon g2;
      if (lincon_from_diff(l, r, 0, &g1) &&
          lincon_from_diff(r, l, 0, &g2)) {
        cons.push_back(g1);
        cons.push_back(g2);
        if (lia_infeasible(cons)) {
          return Tri::T;
        }
      }
      return Tri::U;
    }
    default:
      return Tri::U;
  }
}

Tri eval_bool(const Expr& e, const Ctx& ctx) {
  switch (e.kind) {
    case Expr::Kind::Ident:
      if (e.ident == "true") {
        return Tri::T;
      }
      if (e.ident == "false") {
        return Tri::F;
      }
      return Tri::U;
    case Expr::Kind::IntLit:
      return e.int_value != 0 ? Tri::T : Tri::F;
    case Expr::Kind::FloatLit:
      return e.float_value != 0.0 ? Tri::T : Tri::F;
    case Expr::Kind::UnaryNot:
      return e.operand ? tri_not(eval_bool(*e.operand, ctx)) : Tri::U;
    case Expr::Kind::UnaryMinus:
      // `-x` as a condition is truthy iff the operand is non-zero.
      return e.operand ? eval_bool(*e.operand, ctx) : Tri::U;
    case Expr::Kind::BinOp:
      if (!e.lhs || !e.rhs) {
        return Tri::U;
      }
      switch (e.bin_op) {
        case BinOp::And: {
          const Tri l = eval_bool(*e.lhs, ctx);
          if (l == Tri::F) {
            return Tri::F;
          }
          const Tri r = eval_bool(*e.rhs, ctx);
          if (r == Tri::F) {
            return Tri::F;
          }
          if (l == Tri::T && r == Tri::T) {
            return Tri::T;
          }
          return Tri::U;
        }
        case BinOp::Or: {
          const Tri l = eval_bool(*e.lhs, ctx);
          if (l == Tri::T) {
            return Tri::T;
          }
          const Tri r = eval_bool(*e.rhs, ctx);
          if (r == Tri::T) {
            return Tri::T;
          }
          if (l == Tri::F && r == Tri::F) {
            return Tri::F;
          }
          return Tri::U;
        }
        case BinOp::Implies: {
          const Tri a = eval_bool(*e.lhs, ctx);
          if (a == Tri::F) {
            return Tri::T;
          }
          if (a == Tri::T) {
            return eval_bool(*e.rhs, ctx);
          }
          // assume the (possibly conjunctive) antecedent and re-check
          Ctx ext = ctx;
          ctx_add_conjuncts(ext, *e.lhs);
          return eval_bool(*e.rhs, ext);
        }
        case BinOp::Le:
        case BinOp::Lt:
        case BinOp::Ge:
        case BinOp::Gt:
        case BinOp::Eq:
        case BinOp::Ne:
          return eval_cmp(e.bin_op, *e.lhs, *e.rhs, ctx);
        default:
          return Tri::U;
      }
    default:
      return Tri::U;
  }
}

}  // namespace

NativeDischarge discharge_prop_natively(const Expr& prop) {
  switch (eval_bool(prop, Ctx{})) {
    case Tri::T:
      return NativeDischarge::Proved;
    case Tri::F:
      return NativeDischarge::Disproved;
    default:
      return NativeDischarge::Open;
  }
}

bool prove_prop_natively(const Expr& prop) {
  return discharge_prop_natively(prop) == NativeDischarge::Proved;
}

bool fold_numeric_equal(const Expr& a, const Expr& b) {
  const FoldVal l = fold_const(a);
  const FoldVal r = fold_const(b);
  if (!l.ok || !r.ok) {
    return false;
  }
  if (!l.is_float && !r.is_float) {
    return l.iv == r.iv;  // exact integer comparison
  }
  return l.num() == r.num();
}

TheoremDischargeResult discharge_theorems_natively(const Module& module) {
  TheoremDischargeResult out;
  for (const auto& thm : module.theorems) {
    if (thm.is_axiom) {
      ++out.axiom_count;
      continue;
    }
    if (!thm.proposition || discharge_prop_natively(*thm.proposition) != NativeDischarge::Proved) {
      ++out.open_count;
      out.open_names.push_back(thm.name);
      continue;
    }
    ++out.proved_count;
  }
  return out;
}

}  // namespace li
