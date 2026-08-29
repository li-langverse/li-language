#include "li/mir.hpp"

#include <algorithm>
#include <unordered_set>

namespace li {

namespace {

int temp_counter = 0;
std::vector<std::string> while_head_labels;
std::vector<std::string> while_exit_labels;
const ProcDecl* g_cur_proc = nullptr;

std::string fresh_temp() { return "__t" + std::to_string(temp_counter++); }
std::string fresh_label(const std::string& prefix) {
  return prefix + std::to_string(temp_counter++);
}

bool is_float_type_name(const std::string& n) {
  return n == "float" || n == "f64" || n == "float64";
}

bool is_string_type_name(const std::string& n) {
  return n == "str" || n == "string";
}

bool is_i64_type_name(const std::string& n) {
  return n == "ptr" || n == "int64" || n == "i64" || n == "long";
}

bool is_int_type_name(const std::string& n) {
  return n == "int" || n == "bool" || n == "unit";
}

void push_label(std::vector<MirInsn>& out, const std::string& name) {
  MirInsn ins;
  ins.op = MirOp::Label;
  ins.label = name;
  out.push_back(std::move(ins));
}

void push_jump(std::vector<MirInsn>& out, const std::string& name) {
  MirInsn ins;
  ins.op = MirOp::Jump;
  ins.label = name;
  out.push_back(std::move(ins));
}

void push_branch_if_zero(std::vector<MirInsn>& out, const std::string& ident,
                         const std::string& label) {
  MirInsn ins;
  ins.op = MirOp::BranchIfZero;
  ins.ident = ident;
  ins.label = label;
  out.push_back(std::move(ins));
}

bool tail_is_terminator(const std::vector<MirInsn>& out) {
  return !out.empty() && (out.back().op == MirOp::ReturnVoid ||
                          out.back().op == MirOp::ReturnInt ||
                          out.back().op == MirOp::ReturnFloat ||
                          out.back().op == MirOp::ReturnIdent ||
                          out.back().op == MirOp::Jump ||
                          out.back().op == MirOp::BranchIfZero);
}

// Emit a jump only if the current block hasn't already terminated (e.g. a body
// that ended in `return`). Without this, a dead Jump after a Ret would put two
// terminators in one basic block.
void push_jump_if_open(std::vector<MirInsn>& out, const std::string& label) {
  if (!tail_is_terminator(out)) {
    push_jump(out, label);
  }
}

bool is_arith_binop(BinOp op) {
  return op == BinOp::Add || op == BinOp::Sub || op == BinOp::Mul || op == BinOp::Div ||
         op == BinOp::Mod || op == BinOp::FloorDiv || op == BinOp::Pow;
}

bool is_float_expr(const Expr& e, const std::unordered_set<std::string>& float_names,
                   const std::unordered_set<std::string>& float_arrays) {
  switch (e.kind) {
    case Expr::Kind::FloatLit:
      return true;
    case Expr::Kind::Ident:
      return float_names.count(e.ident) > 0;
    case Expr::Kind::Index:
      // x[i] over a float array param/local is a float element.
      if (e.base && e.base->kind == Expr::Kind::Ident) {
        return float_arrays.count(e.base->ident) > 0;
      }
      return false;
    case Expr::Kind::BinOp:
      if (!is_arith_binop(e.bin_op)) {
        return false;
      }
      return is_float_expr(*e.lhs, float_names, float_arrays) ||
             is_float_expr(*e.rhs, float_names, float_arrays);
    case Expr::Kind::UnaryMinus:
      if (e.operand) {
        return is_float_expr(*e.operand, float_names, float_arrays);
      }
      return false;
    default:
      return false;
  }
}

const ProcDecl* find_proc(const Module& module, const std::string& name) {
  const auto it = std::find_if(module.procs.begin(), module.procs.end(),
                               [&](const ProcDecl& p) { return p.name == name; });
  return it != module.procs.end() ? &*it : nullptr;
}

void seed_float_params(const MirFn& fn, std::unordered_set<std::string>& float_names,
                       std::unordered_set<std::string>& float_arrays) {
  for (const auto& p : fn.params) {
    if (p.is_float) {
      float_names.insert(p.name);
    }
    if (p.is_float && p.is_array) {
      float_arrays.insert(p.name);
    }
  }
}

std::string lower_expr_to(const Expr& e, const Module& module, std::vector<MirInsn>& out,
                          std::unordered_set<std::string>& float_names,
                          std::unordered_set<std::string>& float_arrays) {
  switch (e.kind) {
    case Expr::Kind::IntLit: {
      const std::string dest = fresh_temp();
      MirInsn ins;
      ins.op = MirOp::StoreInt;
      ins.ident = dest;
      ins.rhs_is_literal = true;
      ins.rhs_int = e.int_value;
      out.push_back(std::move(ins));
      return dest;
    }
    case Expr::Kind::FloatLit: {
      const std::string dest = fresh_temp();
      MirInsn ins;
      ins.op = MirOp::StoreFloat;
      ins.ident = dest;
      ins.rhs_is_literal = true;
      ins.float_value = e.float_value;
      out.push_back(std::move(ins));
      float_names.insert(dest);
      return dest;
    }
    case Expr::Kind::Ident:
      return e.ident;
    case Expr::Kind::UnaryMinus: {
      // Walker mir_lower_expr k==11: `-x` = `0 - x`. Int operands fold the
      // literal zero into the BinOpInt (lhs_is_literal=1, lhs_int=0); float
      // operands materialize a StoreFloat(0.0) zero temp first. Dest counter
      // is allocated before the operand.
      const std::string dest = fresh_temp();
      if (!e.operand) {
        MirInsn ins;
        ins.op = MirOp::BinOpInt;
        ins.ident = dest;
        ins.bin_op = BinOp::Sub;
        ins.lhs_is_literal = true;
        ins.lhs_int = 0;
        ins.rhs_is_literal = false;
        ins.rhs_ident = dest;
        out.push_back(std::move(ins));
        return dest;
      }
      const bool flt = is_float_expr(*e.operand, float_names, float_arrays);
      MirInsn ins;
      ins.ident = dest;
      ins.bin_op = BinOp::Sub;
      if (flt) {
        const std::string zero = fresh_temp();
        MirInsn z;
        z.op = MirOp::StoreFloat;
        z.ident = zero;
        z.float_value = 0.0;
        out.push_back(std::move(z));
        float_names.insert(zero);
        ins.op = MirOp::BinOpFloat;
        ins.lhs_ident = zero;
        ins.lhs_is_literal = false;
        ins.rhs_ident = lower_expr_to(*e.operand, module, out, float_names, float_arrays);
        ins.rhs_is_literal = false;
        float_names.insert(dest);
      } else {
        ins.op = MirOp::BinOpInt;
        ins.lhs_is_literal = true;
        ins.lhs_int = 0;
        ins.rhs_ident = lower_expr_to(*e.operand, module, out, float_names, float_arrays);
        ins.rhs_is_literal = false;
      }
      out.push_back(std::move(ins));
      return dest;
    }
    case Expr::Kind::BinOp: {
      // `x @ y` over float-array params lowers to ArrayDotF64 (walker INS 14)
      // with the declared array length in int_value.
      if (e.bin_op == BinOp::MatMul && e.lhs && e.rhs &&
          e.lhs->kind == Expr::Kind::Ident && e.rhs->kind == Expr::Kind::Ident) {
        std::int64_t len = 0;
        if (g_cur_proc) {
          for (const auto& p : g_cur_proc->params) {
            if (p.name == e.lhs->ident && p.type.kind == TypeKind::Array) {
              len = p.type.array_size;
              break;
            }
          }
        }
        MirInsn ins;
        ins.op = MirOp::ArrayDotF64;
        ins.int_value = len;
        ins.lhs_ident = e.lhs->ident;
        ins.rhs_ident = e.rhs->ident;
        const std::string dest = fresh_temp();
        ins.ident = dest;
        out.push_back(std::move(ins));
        float_names.insert(dest);
        return dest;
      }
      // Walker contract (bootstrap/lic/main.li mir_lower_expr k==7): the dest
      // counter is allocated BEFORE the operands, and int literals fold into
      // lhs_is_literal/rhs_is_literal instead of materializing a StoreInt temp.
      // Float literals do NOT fold — they materialize as temps (walker lowers
      // both float operands through mir_lower_expr).
      const std::string dest = fresh_temp();
      MirInsn ins;
      const bool flt = is_float_expr(e, float_names, float_arrays);
      ins.op = flt ? MirOp::BinOpFloat : MirOp::BinOpInt;
      ins.ident = dest;
      ins.bin_op = e.bin_op;
      const auto lower_operand = [&](const Expr& opnd) -> std::string {
        // Float binops never fold literals (walker lowers both operands); an
        // int literal in a float context materializes as a StoreFloat temp,
        // which is exactly the walker's `-x` float zero-temp dance.
        if (flt && opnd.kind == Expr::Kind::IntLit) {
          const std::string d = fresh_temp();
          MirInsn s;
          s.op = MirOp::StoreFloat;
          s.ident = d;
          s.float_value = static_cast<double>(opnd.int_value);
          out.push_back(std::move(s));
          float_names.insert(d);
          return d;
        }
        return lower_expr_to(opnd, module, out, float_names, float_arrays);
      };
      if (!flt && e.lhs->kind == Expr::Kind::IntLit) {
        ins.lhs_is_literal = true;
        ins.lhs_int = e.lhs->int_value;
      } else {
        // Walker sets f[29]=0 for non-literal operands (mir_fdef defaults it 0).
        ins.lhs_is_literal = false;
        ins.lhs_ident = lower_operand(*e.lhs);
      }
      if (!flt && e.rhs->kind == Expr::Kind::IntLit) {
        ins.rhs_is_literal = true;
        ins.rhs_int = e.rhs->int_value;
      } else {
        // Walker sets f[26]=0 explicitly (struct default here is true).
        ins.rhs_is_literal = false;
        ins.rhs_ident = lower_operand(*e.rhs);
      }
      out.push_back(std::move(ins));
      if (flt) {
        float_names.insert(dest);
      }
      return dest;
    }
    case Expr::Kind::Call: {
      const ProcDecl* callee = find_proc(module, e.ident);
      if (callee && !callee->is_extern) {
        MirInsn ins;
        ins.op = MirOp::CallProc;
        ins.callee = e.ident;
        for (const auto& arg : e.args) {
          MirArg ma;
          if (arg->kind == Expr::Kind::IntLit) {
            ma.is_literal = true;
            ma.int_value = arg->int_value;
          } else if (arg->kind == Expr::Kind::StringLit) {
            ma.is_string = true;
            ma.str_value = arg->str_value;
          } else if (arg->kind == Expr::Kind::Ident) {
            ma.ident = arg->ident;
            // Array args pass by address (walker ARG is_array_ident).
            if (float_arrays.count(arg->ident) > 0) {
              ma.is_array_ident = true;
            }
          } else {
            ma.ident = lower_expr_to(*arg, module, out, float_names, float_arrays);
          }
          ins.args.push_back(std::move(ma));
        }
        const std::string dest = fresh_temp();
        ins.ident = dest;
        if (callee->ret_type && is_float_type_name(callee->ret_type->name)) {
          ins.ret_is_float = true;
          float_names.insert(dest);
        }
        out.push_back(std::move(ins));
        return dest;
      }
      MirInsn ins;
      ins.op = MirOp::CallExtern;
      ins.callee = e.ident;
      for (const auto& arg : e.args) {
        MirArg ma;
        if (arg->kind == Expr::Kind::StringLit) {
          ma.is_string = true;
          ma.str_value = arg->str_value;
        } else if (arg->kind == Expr::Kind::IntLit) {
          ma.is_literal = true;
          ma.int_value = arg->int_value;
        } else if (arg->kind == Expr::Kind::Ident) {
          ma.ident = arg->ident;
        } else {
          ma.ident = lower_expr_to(*arg, module, out, float_names, float_arrays);
        }
        ins.args.push_back(std::move(ma));
      }
      if (callee && callee->is_extern && callee->ret_type &&
          callee->ret_type->name != "unit") {
        const std::string dest = fresh_temp();
        ins.ident = dest;
        if (callee->ret_type->name == "ptr" || callee->ret_type->name == "int64" ||
            callee->ret_type->name == "i64") {
          ins.is_i64 = true;
        } else if (is_float_type_name(callee->ret_type->name)) {
          ins.ret_is_float = true;
          float_names.insert(dest);
        }
        out.push_back(std::move(ins));
        return dest;
      }
      out.push_back(std::move(ins));
      return fresh_temp();
    }
    case Expr::Kind::Index: {
      if (e.base && e.base->kind == Expr::Kind::Ident && e.index) {
        MirInsn load;
        load.op = MirOp::ArrayLoadInt;
        load.ident = e.base->ident;
        if (e.index->kind == Expr::Kind::IntLit) {
          load.index_is_literal = true;
          load.int_value = e.index->int_value;
        } else if (e.index->kind == Expr::Kind::Ident) {
          load.index_is_literal = false;
          load.index_ident = e.index->ident;
        }
        const std::string dest = fresh_temp();
        load.lhs_ident = dest;
        out.push_back(std::move(load));
        return dest;
      }
      return fresh_temp();
    }
    default:
      return fresh_temp();
  }
}

void lower_echo_arg(const Expr& arg, std::vector<MirInsn>& out) {
  MirInsn ins;
  if (arg.kind == Expr::Kind::IntLit) {
    ins.op = MirOp::EchoInt;
    ins.int_value = arg.int_value;
  } else if (arg.kind == Expr::Kind::Ident) {
    ins.op = MirOp::EchoInt;
    ins.ident = arg.ident;
  } else if (arg.kind == Expr::Kind::StringLit) {
    ins.op = MirOp::EchoString;
    ins.str_value = arg.str_value;
  }
  out.push_back(std::move(ins));
}

void lower_return_expr(const Expr& e, bool returns_float, const Module& module,
                       std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                       std::unordered_set<std::string>& float_arrays) {
  MirInsn ins;
  if (e.kind == Expr::Kind::IntLit) {
    ins.op = MirOp::ReturnInt;
    ins.int_value = e.int_value;
  } else if (e.kind == Expr::Kind::FloatLit) {
    ins.op = MirOp::ReturnFloat;
    ins.float_value = e.float_value;
  } else if (e.kind == Expr::Kind::Ident) {
    ins.op = MirOp::ReturnIdent;
    ins.ident = e.ident;
    ins.ret_is_float = returns_float || float_names.count(e.ident) > 0;
  } else if (e.kind == Expr::Kind::Call || e.kind == Expr::Kind::BinOp ||
             e.kind == Expr::Kind::Index || e.kind == Expr::Kind::UnaryMinus) {
    const std::string tmp = lower_expr_to(e, module, out, float_names, float_arrays);
    ins.op = MirOp::ReturnIdent;
    ins.ident = tmp;
    ins.ret_is_float = returns_float || is_float_expr(e, float_names, float_arrays);
  } else {
    ins.op = MirOp::ReturnVoid;
  }
  out.push_back(std::move(ins));
}

void lower_stmts(const std::vector<Stmt>& stmts, const Module& module, bool returns_float,
                 std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                 std::unordered_set<std::string>& float_arrays);

void lower_stmt(const Stmt& stmt, const Module& module, bool returns_float,
                std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                std::unordered_set<std::string>& float_arrays) {
  switch (stmt.kind) {
    case Stmt::Kind::Return:
      if (!stmt.expr) {
        MirInsn ins;
        ins.op = MirOp::ReturnVoid;
        out.push_back(std::move(ins));
      } else {
        lower_return_expr(*stmt.expr, returns_float, module, out, float_names, float_arrays);
      }
      break;
    case Stmt::Kind::VarDecl: {
      if (stmt.var_type.kind == TypeKind::Array && stmt.var_type.elem) {
        MirInsn ins;
        ins.op = MirOp::ArrayAlloc;
        ins.ident = stmt.var_name;
        ins.int_value = stmt.var_type.array_size;
        // Walker sets the array_is_float flag on ArrayAlloc for float arrays
        // and uses it to pick ArrayStoreFloat / BinOpFloat on elements.
        if (is_float_type_name(stmt.var_type.elem->name)) {
          ins.array_is_float = true;
          float_arrays.insert(stmt.var_name);
        }
        out.push_back(std::move(ins));
      } else if (is_i64_type_name(stmt.var_type.name)) {
        MirInsn ins;
        ins.op = MirOp::LocalAllocI64;
        ins.ident = stmt.var_name;
        out.push_back(std::move(ins));
        if (stmt.init) {
          MirInsn store;
          store.op = MirOp::StoreI64;
          store.ident = stmt.var_name;
          if (stmt.init->kind == Expr::Kind::IntLit) {
            store.rhs_is_literal = true;
            store.rhs_int = stmt.init->int_value;
          } else if (stmt.init->kind == Expr::Kind::Ident) {
            store.rhs_is_literal = false;
            store.rhs_ident = stmt.init->ident;
          } else {
            const std::string tmp = lower_expr_to(*stmt.init, module, out, float_names, float_arrays);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
        }
      } else if (is_float_type_name(stmt.var_type.name)) {
        MirInsn ins;
        ins.op = MirOp::LocalAllocFloat;
        ins.ident = stmt.var_name;
        out.push_back(std::move(ins));
        float_names.insert(stmt.var_name);
        if (stmt.init) {
          MirInsn store;
          store.op = MirOp::StoreFloat;
          store.ident = stmt.var_name;
          if (stmt.init->kind == Expr::Kind::FloatLit) {
            store.rhs_is_literal = true;
            store.float_value = stmt.init->float_value;
          } else if (stmt.init->kind == Expr::Kind::Ident) {
            store.rhs_is_literal = false;
            store.rhs_ident = stmt.init->ident;
          } else {
            const std::string tmp = lower_expr_to(*stmt.init, module, out, float_names, float_arrays);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
        }
      } else {
        MirInsn ins;
        ins.op = MirOp::LocalAllocInt;
        ins.ident = stmt.var_name;
        out.push_back(std::move(ins));
        if (stmt.init) {
          MirInsn store;
          store.op = MirOp::StoreInt;
          store.ident = stmt.var_name;
          if (stmt.init->kind == Expr::Kind::IntLit) {
            store.rhs_is_literal = true;
            store.rhs_int = stmt.init->int_value;
          } else if (stmt.init->kind == Expr::Kind::Ident) {
            store.rhs_is_literal = false;
            store.rhs_ident = stmt.init->ident;
          } else {
            const std::string tmp = lower_expr_to(*stmt.init, module, out, float_names, float_arrays);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
        }
      }
      break;
    }
    case Stmt::Kind::Assign:
      if (stmt.init && stmt.init->kind == Expr::Kind::Index && stmt.init->base &&
          stmt.init->base->kind == Expr::Kind::Ident && stmt.expr) {
        const bool arr_flt = float_arrays.count(stmt.init->base->ident) > 0;
        MirInsn ins;
        // Walker emits ArrayStoreFloat (INS 12) for float arrays, folding a
        // float literal into int_value with rhs_is_literal=1 (no temp).
        ins.op = arr_flt ? MirOp::ArrayStoreFloat : MirOp::ArrayStoreInt;
        ins.ident = stmt.init->base->ident;
        if (stmt.init->index && stmt.init->index->kind == Expr::Kind::IntLit) {
          ins.index_is_literal = true;
          ins.int_value = stmt.init->index->int_value;
        } else if (stmt.init->index && stmt.init->index->kind == Expr::Kind::Ident) {
          ins.index_is_literal = false;
          ins.index_ident = stmt.init->index->ident;
        }
        if (arr_flt && stmt.expr->kind == Expr::Kind::FloatLit) {
          // Walker stores the literal in the float field (INS 12 0 <f>).
          ins.rhs_is_literal = true;
          ins.float_value = stmt.expr->float_value;
        } else if (stmt.expr->kind == Expr::Kind::IntLit && !arr_flt) {
          ins.rhs_is_literal = true;
          ins.rhs_int = stmt.expr->int_value;
        } else if (stmt.expr->kind == Expr::Kind::Ident) {
          ins.rhs_is_literal = false;
          ins.rhs_ident = stmt.expr->ident;
        } else {
          ins.rhs_ident = lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
          ins.rhs_is_literal = false;
        }
        out.push_back(std::move(ins));
      } else if (stmt.init && stmt.init->kind == Expr::Kind::Ident && stmt.expr) {
        const bool flt = float_names.count(stmt.init->ident) > 0;
        MirInsn ins;
        ins.op = flt ? MirOp::StoreFloat : MirOp::StoreInt;
        ins.ident = stmt.init->ident;
        if (stmt.expr->kind == Expr::Kind::IntLit && !flt) {
          ins.rhs_is_literal = true;
          ins.rhs_int = stmt.expr->int_value;
        } else if (stmt.expr->kind == Expr::Kind::FloatLit && flt) {
          ins.rhs_is_literal = true;
          ins.float_value = stmt.expr->float_value;
        } else if (stmt.expr->kind == Expr::Kind::Ident) {
          ins.rhs_is_literal = false;
          ins.rhs_ident = stmt.expr->ident;
        } else {
          ins.rhs_ident = lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
          ins.rhs_is_literal = false;
        }
        out.push_back(std::move(ins));
      }
      break;
    case Stmt::Kind::If: {
      if (!stmt.cond) {
        break;
      }
      const std::string cond_tmp = lower_expr_to(*stmt.cond, module, out, float_names, float_arrays);
      const std::string else_label = fresh_label("else_");
      const std::string merge_label = fresh_label("merge_");
      push_branch_if_zero(out, cond_tmp, else_label);
      lower_stmts(stmt.then_body, module, returns_float, out, float_names, float_arrays);
      if (stmt.else_body) {
        push_jump_if_open(out, merge_label);
        push_label(out, else_label);
        lower_stmts(*stmt.else_body, module, returns_float, out, float_names, float_arrays);
        push_label(out, merge_label);
      } else {
        push_label(out, else_label);
      }
      break;
    }
    case Stmt::Kind::Continue: {
      if (!while_head_labels.empty()) {
        push_jump(out, while_head_labels.back());
      }
      break;
    }
    case Stmt::Kind::Break: {
      if (!while_exit_labels.empty()) {
        push_jump(out, while_exit_labels.back());
      }
      break;
    }
    case Stmt::Kind::While: {
      if (!stmt.cond) {
        break;
      }
      const std::string head_label = fresh_label("while_head_");
      const std::string exit_label = fresh_label("while_exit_");
      while_head_labels.push_back(head_label);
      while_exit_labels.push_back(exit_label);
      push_label(out, head_label);
      const std::string cond_tmp = lower_expr_to(*stmt.cond, module, out, float_names, float_arrays);
      push_branch_if_zero(out, cond_tmp, exit_label);
      lower_stmts(stmt.while_body, module, returns_float, out, float_names, float_arrays);
      push_jump_if_open(out, head_label);
      push_label(out, exit_label);
      while_head_labels.pop_back();
      while_exit_labels.pop_back();
      break;
    }
    case Stmt::Kind::Expr:
      if (stmt.expr && stmt.expr->kind == Expr::Kind::Call) {
        // The walker treats print() exactly like echo(): EchoInt for int
        // args, EchoString for string literals (mir_stmt print dispatch).
        if ((stmt.expr->ident == "echo" || stmt.expr->ident == "print") &&
            !stmt.expr->args.empty()) {
          lower_echo_arg(*stmt.expr->args[0], out);
        } else if (stmt.expr->ident == "print_int" && !stmt.expr->args.empty()) {
          // print_int() is a prelude builtin — lower to CallExtern li_rt_print_int
          MirInsn ins;
          ins.op = MirOp::CallExtern;
          ins.callee = "li_rt_print_int";
          MirArg ma;
          if (stmt.expr->args[0]->kind == Expr::Kind::IntLit) {
            ma.is_literal = true;
            ma.int_value = stmt.expr->args[0]->int_value;
          } else if (stmt.expr->args[0]->kind == Expr::Kind::Ident) {
            ma.ident = stmt.expr->args[0]->ident;
          } else {
            ma.ident = lower_expr_to(*stmt.expr->args[0], module, out, float_names, float_arrays);
          }
          ins.args.push_back(std::move(ma));
          out.push_back(std::move(ins));
        } else {
          (void)lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
        }
      }
      break;
    default:
      break;
  }
}

void lower_stmts(const std::vector<Stmt>& stmts, const Module& module, bool returns_float,
                 std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                 std::unordered_set<std::string>& float_arrays) {
  for (const auto& stmt : stmts) {
    lower_stmt(stmt, module, returns_float, out, float_names, float_arrays);
    if (!out.empty() && (out.back().op == MirOp::ReturnVoid || out.back().op == MirOp::ReturnInt ||
                         out.back().op == MirOp::ReturnFloat || out.back().op == MirOp::ReturnIdent)) {
      return;
    }
  }
}

bool insn_terminates(MirOp op) {
  return op == MirOp::ReturnVoid || op == MirOp::ReturnInt || op == MirOp::ReturnFloat ||
         op == MirOp::ReturnIdent;
}

void append_implicit_return(std::vector<MirInsn>& body) {
  if (body.empty() || !insn_terminates(body.back().op)) {
    MirInsn ins;
    ins.op = MirOp::ReturnVoid;
    body.push_back(std::move(ins));
  }
}

}  // namespace

MirModule lower_to_mir(const Module& module) {
  temp_counter = 0;
  MirModule mir;
  for (const auto& proc : module.procs) {
    MirFn fn;
    fn.name = proc.name;
    fn.is_extern = proc.is_extern;
    if (proc.ret_type) {
      fn.returns_float = is_float_type_name(proc.ret_type->name);
      fn.returns_void = proc.ret_type->name == "unit";
      fn.returns_i64 = is_i64_type_name(proc.ret_type->name);
    } else if (proc.is_extern) {
      fn.returns_void = true;
    }
    for (const auto& p : proc.params) {
      MirParam mp;
      mp.name = p.name;
      mp.is_float = is_float_type_name(p.type.name);
      mp.is_string = is_string_type_name(p.type.name);
      if (p.type.kind == TypeKind::Array && p.type.elem) {
        // Walker PARAM layout (mir_param_line): array[N, float] -> is_float=1,
        // fixed_array_elems=N, is_i64=0; array[N, ptr|str|i64] elements are
        // pointer-width (is_i64=1).
        mp.is_array = true;
        mp.array_size = p.type.array_size;
        mp.fixed_array_elems = p.type.array_size;
        mp.is_float = is_float_type_name(p.type.elem->name);
        mp.is_i64 = is_i64_type_name(p.type.elem->name) ||
                    is_string_type_name(p.type.elem->name);
      } else {
        mp.is_i64 = is_i64_type_name(p.type.name);
      }
      fn.params.push_back(std::move(mp));
    }
    if (!proc.is_extern) {
      g_cur_proc = &proc;
      std::unordered_set<std::string> float_names;
      std::unordered_set<std::string> float_arrays;
      seed_float_params(fn, float_names, float_arrays);
      lower_stmts(proc.body, module, fn.returns_float, fn.body, float_names, float_arrays);
      append_implicit_return(fn.body);
      g_cur_proc = nullptr;
    }
    if (!proc.is_extern && fn.body.empty()) {
      MirInsn ins;
      ins.op = MirOp::ReturnVoid;
      fn.body.push_back(std::move(ins));
    }
    mir.functions.push_back(std::move(fn));
  }
  return mir;
}

}  // namespace li
