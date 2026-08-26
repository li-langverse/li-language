#include "li/num_stable.hpp"

#include <unordered_map>
#include <vector>

namespace li {

namespace {

enum class DefKind { Unknown, FloatLit, BinOp, CallExtern };

struct DefInfo {
  DefKind kind = DefKind::Unknown;
  BinOp op = BinOp::Add;
  std::string lhs;
  std::string rhs;
  bool is_square = false;
  double float_lit = 0.0;
  std::string callee;
  std::string call_arg;
};

void push_binop_float(std::vector<MirInsn>& out, const std::string& dest, BinOp op,
                      const std::string& lhs, const std::string& rhs) {
  MirInsn ins;
  ins.op = MirOp::BinOpFloat;
  ins.ident = dest;
  ins.lhs_ident = lhs;
  ins.rhs_ident = rhs;
  ins.bin_op = op;
  out.push_back(std::move(ins));
}

void push_call_extern(std::vector<MirInsn>& out, const std::string& dest,
                      const std::string& callee, const std::string& arg_ident) {
  MirInsn ins;
  ins.op = MirOp::CallExtern;
  ins.ident = dest;
  ins.callee = callee;
  ins.ret_is_float = true;
  MirArg a;
  a.ident = arg_ident;
  ins.args.push_back(a);
  out.push_back(std::move(ins));
}

void record_def(const MirInsn& ins, std::unordered_map<std::string, DefInfo>& defs) {
  if (ins.ident.empty()) {
    return;
  }
  switch (ins.op) {
    case MirOp::StoreFloat:
      if (ins.rhs_is_literal) {
        DefInfo d;
        d.kind = DefKind::FloatLit;
        d.float_lit = ins.float_value;
        defs[ins.ident] = d;
      } else if (!ins.rhs_ident.empty()) {
        auto it = defs.find(ins.rhs_ident);
        if (it != defs.end()) {
          defs[ins.ident] = it->second;
        } else {
          defs.erase(ins.ident);
        }
      }
      break;
    case MirOp::BinOpFloat: {
      DefInfo d;
      d.kind = DefKind::BinOp;
      d.op = ins.bin_op;
      d.lhs = ins.lhs_ident;
      d.rhs = ins.rhs_ident;
      d.is_square = ins.bin_op == BinOp::Mul && ins.lhs_ident == ins.rhs_ident;
      defs[ins.ident] = d;
      break;
    }
    case MirOp::CallExtern: {
      DefInfo d;
      d.kind = DefKind::CallExtern;
      d.callee = ins.callee;
      if (!ins.args.empty()) {
        d.call_arg = ins.args[0].ident;
      }
      defs[ins.ident] = d;
      break;
    }
    default:
      break;
  }
}

void apply_to_function(MirFn& fn) {
  std::unordered_map<std::string, DefInfo> defs;
  std::vector<MirInsn> out;
  out.reserve(fn.body.size() + 16);

  int fresh = 0;
  auto next_temp = [&]() { return "__ns" + std::to_string(fresh++); };

  for (const MirInsn& ins : fn.body) {
    if (ins.op == MirOp::BinOpFloat && ins.bin_op == BinOp::Sub) {
      const auto l_it = defs.find(ins.lhs_ident);
      const auto r_it = defs.find(ins.rhs_ident);
      if (l_it != defs.end() && r_it != defs.end() && l_it->second.kind == DefKind::BinOp &&
          r_it->second.kind == DefKind::BinOp && l_it->second.op == BinOp::Mul &&
          r_it->second.op == BinOp::Mul && l_it->second.is_square && r_it->second.is_square) {
        const std::string& x = l_it->second.lhs;
        const std::string& y = r_it->second.lhs;
        const std::string d = next_temp();
        const std::string s = next_temp();
        push_binop_float(out, d, BinOp::Sub, x, y);
        push_binop_float(out, s, BinOp::Add, x, y);
        push_binop_float(out, ins.ident, BinOp::Mul, d, s);
        DefInfo result;
        result.kind = DefKind::BinOp;
        result.op = BinOp::Mul;
        result.lhs = d;
        result.rhs = s;
        defs[ins.ident] = result;
        continue;
      }
      if (l_it != defs.end() && l_it->second.kind == DefKind::BinOp &&
          l_it->second.op == BinOp::Add) {
        const std::string& a = l_it->second.lhs;
        const std::string& b = l_it->second.rhs;
        const std::string& c = ins.rhs_ident;
        const std::string t = next_temp();
        push_binop_float(out, t, BinOp::Sub, c, b);
        push_binop_float(out, ins.ident, BinOp::Sub, a, t);
        DefInfo result;
        result.kind = DefKind::BinOp;
        result.op = BinOp::Sub;
        result.lhs = a;
        result.rhs = t;
        defs[ins.ident] = result;
        continue;
      }
    }

    if (ins.op == MirOp::CallExtern && ins.callee == "li_rt_sqrt" && ins.args.size() == 1 &&
        !ins.args[0].is_literal) {
      const auto a_it = defs.find(ins.args[0].ident);
      if (a_it != defs.end() && a_it->second.kind == DefKind::BinOp &&
          a_it->second.op == BinOp::Add) {
        const auto l_sq = defs.find(a_it->second.lhs);
        const auto r_sq = defs.find(a_it->second.rhs);
        if (l_sq != defs.end() && r_sq != defs.end() && l_sq->second.kind == DefKind::BinOp &&
            r_sq->second.kind == DefKind::BinOp && l_sq->second.op == BinOp::Mul &&
            r_sq->second.op == BinOp::Mul && l_sq->second.is_square && r_sq->second.is_square) {
          MirInsn hyp;
          hyp.op = MirOp::CallExtern;
          hyp.ident = ins.ident;
          hyp.callee = "li_rt_hypot";
          hyp.ret_is_float = true;
          MirArg ax;
          ax.ident = l_sq->second.lhs;
          MirArg ay;
          ay.ident = r_sq->second.lhs;
          hyp.args = {ax, ay};
          out.push_back(std::move(hyp));
          DefInfo d;
          d.kind = DefKind::CallExtern;
          d.callee = "li_rt_hypot";
          defs[ins.ident] = d;
          continue;
        }
      }
    }

    if (ins.op == MirOp::BinOpFloat && ins.bin_op == BinOp::Sub) {
      const auto r_it = defs.find(ins.rhs_ident);
      if (r_it != defs.end() && r_it->second.kind == DefKind::FloatLit &&
          r_it->second.float_lit == 1.0) {
        const auto l_it = defs.find(ins.lhs_ident);
        if (l_it != defs.end() && l_it->second.kind == DefKind::CallExtern &&
            l_it->second.callee == "li_rt_exp") {
          push_call_extern(out, ins.ident, "li_rt_expm1", l_it->second.call_arg);
          DefInfo d;
          d.kind = DefKind::CallExtern;
          d.callee = "li_rt_expm1";
          defs[ins.ident] = d;
          continue;
        }
      }
    }

    if (ins.op == MirOp::CallExtern && ins.callee == "li_rt_log" && ins.args.size() == 1 &&
        !ins.args[0].is_literal) {
      const auto a_it = defs.find(ins.args[0].ident);
      if (a_it != defs.end() && a_it->second.kind == DefKind::BinOp &&
          a_it->second.op == BinOp::Add) {
        const auto l_it = defs.find(a_it->second.lhs);
        if (l_it != defs.end() && l_it->second.kind == DefKind::FloatLit &&
            l_it->second.float_lit == 1.0) {
          MirInsn log1p;
          log1p.op = MirOp::CallExtern;
          log1p.ident = ins.ident;
          log1p.callee = "li_rt_log1p";
          log1p.ret_is_float = true;
          MirArg ax;
          ax.ident = a_it->second.rhs;
          log1p.args = {ax};
          out.push_back(std::move(log1p));
          DefInfo d;
          d.kind = DefKind::CallExtern;
          d.callee = "li_rt_log1p";
          defs[ins.ident] = d;
          continue;
        }
      }
    }

    out.push_back(ins);
    record_def(ins, defs);
  }

  fn.body = std::move(out);
}

}  // namespace

void apply_numerical_stability(MirModule& mir) {
  if (!mir.fp_numerically_stable) {
    return;
  }
  for (auto& fn : mir.functions) {
    apply_to_function(fn);
  }
}

}  // namespace li
