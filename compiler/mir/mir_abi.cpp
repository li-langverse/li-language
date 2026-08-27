#include "li/mir_abi.hpp"

#include <sstream>

namespace li {

namespace {

const ProcDecl* find_proc_by_name(const Module& module, const std::string& name) {
  for (const auto& proc : module.procs) {
    if (proc.name == name) {
      return &proc;
    }
  }
  return nullptr;
}

bool type_is_pointer_width_ret(const std::string& name) {
  return name == "ptr" || name == "int64" || name == "i64" || name == "long" || name == "str" ||
         name == "string" || name == "bytes" || name == "StringView";
}

bool extern_should_return_i8_ptr(const ProcDecl& proc) {
  return proc.is_extern && proc.ret_type && type_is_pointer_width_ret(proc.ret_type->name) &&
         proc.ret_type->name != "unit";
}

const MirFn* find_mir_fn(const MirModule& mir, const std::string& name) {
  for (const auto& fn : mir.functions) {
    if (fn.name == name) {
      return &fn;
    }
  }
  return nullptr;
}

bool call_stores_pointer_width(const MirInsn& ins) {
  return ins.is_i64 || ins.ret_is_i64;
}

}  // namespace

bool verify_mir_extern_abi(const Module& module, const MirModule& mir, std::string* error) {
  for (const auto& proc : module.procs) {
    if (!extern_should_return_i8_ptr(proc)) {
      continue;
    }
    const MirFn* mfn = find_mir_fn(mir, proc.name);
    if (!mfn) {
      continue;
    }
    if (!mfn->returns_i64) {
      if (error) {
        std::ostringstream os;
        os << "E0360: extern `" << proc.name
           << "` returns pointer-width type `" << proc.ret_type->name
           << "` but MIR declares i32 return (would truncate at link time)";
        *error = os.str();
      }
      return false;
    }
  }

  for (const auto& fn : mir.functions) {
    if (fn.is_extern) {
      continue;
    }
    for (const auto& ins : fn.body) {
      if (ins.op != MirOp::CallExtern || ins.ident.empty()) {
        continue;
      }
      const ProcDecl* callee = find_proc_by_name(module, ins.callee);
      if (!callee || !extern_should_return_i8_ptr(*callee)) {
        continue;
      }
      if (!call_stores_pointer_width(ins)) {
        if (error) {
          std::ostringstream os;
          os << "E0360: call `" << ins.callee
             << "` returns `" << callee->ret_type->name
             << "` but result is stored as i32 (missing is_i64 on CallExtern)";
          *error = os.str();
        }
        return false;
      }
    }
    for (const auto& ins : fn.body) {
      if (ins.op != MirOp::CallProc || ins.ident.empty()) {
        continue;
      }
      const ProcDecl* callee = find_proc_by_name(module, ins.callee);
      if (!callee || !callee->ret_type || callee->ret_type->name == "unit") {
        continue;
      }
      if (!type_is_pointer_width_ret(callee->ret_type->name)) {
        continue;
      }
      if (!ins.ret_is_i64) {
        if (error) {
          std::ostringstream os;
          os << "E0360: call `" << ins.callee
             << "` returns `" << callee->ret_type->name
             << "` but CallProc missing ret_is_i64";
          *error = os.str();
        }
        return false;
      }
    }
  }
  return true;
}

}  // namespace li
