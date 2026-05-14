#include "li/emit.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace li {

namespace {

llvm::Type* i8_ptr(llvm::LLVMContext& ctx) {
  return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx));
}

llvm::Type* i32_ty(llvm::LLVMContext& ctx) {
  return llvm::Type::getInt32Ty(ctx);
}

llvm::Type* i64_ty(llvm::LLVMContext& ctx) {
  return llvm::Type::getInt64Ty(ctx);
}

llvm::Type* llvm_scalar(llvm::LLVMContext& ctx, bool is_float, bool is_i64) {
  if (is_i64) {
    return i64_ty(ctx);
  }
  return is_float ? llvm::Type::getDoubleTy(ctx) : i32_ty(ctx);
}

llvm::Value* int32_val(llvm::IRBuilder<>& builder, llvm::LLVMContext& ctx, std::int64_t v) {
  return llvm::ConstantInt::get(i32_ty(ctx), v);
}

llvm::GlobalVariable* emit_string_global(llvm::Module* module, const std::string& text,
                                         int& counter) {
  llvm::LLVMContext& ctx = module->getContext();
  std::string name = ".str." + std::to_string(counter++);
  const std::size_t len = text.size() + 1;
  llvm::ArrayType* arr_ty = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), len);
  llvm::Constant* init = llvm::ConstantDataArray::getString(ctx, text, true);
  auto* gv = new llvm::GlobalVariable(*module, arr_ty, true,
                                      llvm::GlobalValue::PrivateLinkage, init, name);
  return gv;
}

llvm::Value* string_ptr(llvm::IRBuilder<>& builder, llvm::GlobalVariable* gv) {
  llvm::Value* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
  llvm::Value* indices[] = {zero, zero};
  return builder.CreateInBoundsGEP(gv->getValueType(), gv, indices);
}

struct ArraySlot {
  llvm::AllocaInst* alloca = nullptr;
  std::int64_t size = 0;
};

struct EmitCtx {
  llvm::LLVMContext& context;
  llvm::Module* module;
  llvm::Function* func;
  llvm::IRBuilder<>* builder;
  llvm::Type* ret_ty = nullptr;
  bool returns_float = false;
  std::map<std::string, llvm::AllocaInst*> int_locals;
  std::map<std::string, llvm::AllocaInst*> float_locals;
  std::map<std::string, llvm::AllocaInst*> i64_locals;
  std::map<std::string, llvm::AllocaInst*> ptr_locals;
  std::map<std::string, ArraySlot> arrays;
  std::unordered_map<std::string, llvm::BasicBlock*> labels;
  int str_counter = 0;

  llvm::AllocaInst* ensure_int_local(const std::string& name) {
    auto it = int_locals.find(name);
    if (it != int_locals.end()) {
      return it->second;
    }
    llvm::AllocaInst* slot =
        builder->CreateAlloca(i32_ty(context), nullptr, name);
    int_locals[name] = slot;
    return slot;
  }

  llvm::AllocaInst* ensure_float_local(const std::string& name) {
    auto it = float_locals.find(name);
    if (it != float_locals.end()) {
      return it->second;
    }
    llvm::AllocaInst* slot =
        builder->CreateAlloca(llvm::Type::getDoubleTy(context), nullptr, name);
    float_locals[name] = slot;
    return slot;
  }

  llvm::Value* load_float(const std::string& name) {
    return builder->CreateLoad(llvm::Type::getDoubleTy(context), ensure_float_local(name));
  }

  llvm::AllocaInst* ensure_ptr_local(const std::string& name) {
    auto it = ptr_locals.find(name);
    if (it != ptr_locals.end()) {
      return it->second;
    }
    llvm::AllocaInst* slot =
        builder->CreateAlloca(i8_ptr(context), nullptr, name);
    ptr_locals[name] = slot;
    return slot;
  }

  llvm::Value* load_ptr(const std::string& name) {
    if (auto it = ptr_locals.find(name); it != ptr_locals.end()) {
      return builder->CreateLoad(i8_ptr(context), it->second);
    }
    if (auto it64 = i64_locals.find(name); it64 != i64_locals.end()) {
      return builder->CreateIntToPtr(
          builder->CreateLoad(i64_ty(context), it64->second), i8_ptr(context));
    }
    return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr(context)));
  }

  llvm::AllocaInst* ensure_i64_local(const std::string& name) {
    auto it = i64_locals.find(name);
    if (it != i64_locals.end()) {
      return it->second;
    }
    llvm::AllocaInst* slot =
        builder->CreateAlloca(i64_ty(context), nullptr, name);
    i64_locals[name] = slot;
    return slot;
  }

  llvm::Value* load_int(const std::string& name) {
    if (auto it = float_locals.find(name); it != float_locals.end()) {
      return builder->CreateFPToSI(
          builder->CreateLoad(llvm::Type::getDoubleTy(context), it->second), i32_ty(context));
    }
    if (auto it64 = i64_locals.find(name); it64 != i64_locals.end()) {
      return builder->CreateTrunc(
          builder->CreateLoad(i64_ty(context), it64->second), i32_ty(context));
    }
    return builder->CreateLoad(i32_ty(context), ensure_int_local(name));
  }

  llvm::Value* load_i64(const std::string& name) {
    if (auto it = i64_locals.find(name); it != i64_locals.end()) {
      return builder->CreateLoad(i64_ty(context), it->second);
    }
    return builder->CreateSExt(load_int(name), i64_ty(context));
  }

  llvm::BasicBlock* block_for(const std::string& label) {
    auto it = labels.find(label);
    if (it != labels.end()) {
      return it->second;
    }
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, label, func);
    labels[label] = bb;
    return bb;
  }

  llvm::Value* emit_binop(BinOp op, llvm::Value* lhs, llvm::Value* rhs) {
    switch (op) {
      case BinOp::Add:
        return builder->CreateAdd(lhs, rhs);
      case BinOp::Sub:
        return builder->CreateSub(lhs, rhs);
      case BinOp::Mul:
        return builder->CreateMul(lhs, rhs);
      case BinOp::Div:
        return builder->CreateSDiv(lhs, rhs);
      case BinOp::Lt:
        return builder->CreateZExt(
            builder->CreateICmpSLT(lhs, rhs), i32_ty(context));
      case BinOp::Le:
        return builder->CreateZExt(
            builder->CreateICmpSLE(lhs, rhs), i32_ty(context));
      case BinOp::Gt:
        return builder->CreateZExt(
            builder->CreateICmpSGT(lhs, rhs), i32_ty(context));
      case BinOp::Ge:
        return builder->CreateZExt(
            builder->CreateICmpSGE(lhs, rhs), i32_ty(context));
      case BinOp::Eq:
        return builder->CreateZExt(
            builder->CreateICmpEQ(lhs, rhs), i32_ty(context));
      case BinOp::Ne:
        return builder->CreateZExt(
            builder->CreateICmpNE(lhs, rhs), i32_ty(context));
      case BinOp::And:
        return builder->CreateAnd(lhs, rhs);
      case BinOp::Or:
        return builder->CreateOr(lhs, rhs);
    }
    return lhs;
  }

  llvm::Value* emit_fbinop(BinOp op, llvm::Value* lhs, llvm::Value* rhs) {
    switch (op) {
      case BinOp::Add:
        return builder->CreateFAdd(lhs, rhs);
      case BinOp::Sub:
        return builder->CreateFSub(lhs, rhs);
      case BinOp::Mul:
        return builder->CreateFMul(lhs, rhs);
      case BinOp::Div:
        return builder->CreateFDiv(lhs, rhs);
      default:
        return lhs;
    }
  }

  llvm::Value* mir_arg_value(const MirArg& arg, bool ptr_param = false) {
    if (arg.is_string) {
      llvm::GlobalVariable* gv = emit_string_global(module, arg.str_value, str_counter);
      return string_ptr(*builder, gv);
    }
    if (arg.is_literal) {
      return int32_val(*builder, context, arg.int_value);
    }
    if (float_locals.find(arg.ident) != float_locals.end()) {
      return load_float(arg.ident);
    }
    if (ptr_param || ptr_locals.find(arg.ident) != ptr_locals.end()) {
      return load_ptr(arg.ident);
    }
    if (i64_locals.find(arg.ident) != i64_locals.end()) {
      return load_i64(arg.ident);
    }
    return load_int(arg.ident);
  }

  bool emit_insn(const MirInsn& ins) {
    switch (ins.op) {
      case MirOp::Label: {
        llvm::BasicBlock* dest = block_for(ins.label);
        if (builder->GetInsertBlock() != nullptr &&
            builder->GetInsertBlock()->getTerminator() == nullptr) {
          builder->CreateBr(dest);
        }
        builder->SetInsertPoint(dest);
        return true;
      }
      case MirOp::Jump:
        builder->CreateBr(block_for(ins.label));
        return false;
      case MirOp::BranchIfZero: {
        llvm::Value* cond = load_int(ins.ident);
        llvm::Value* zero = int32_val(*builder, context, 0);
        llvm::Value* is_zero = builder->CreateICmpEQ(cond, zero);
        llvm::BasicBlock* dest = block_for(ins.label);
        llvm::BasicBlock* fall =
            llvm::BasicBlock::Create(context, "br_fall", func);
        builder->CreateCondBr(is_zero, dest, fall);
        builder->SetInsertPoint(fall);
        return true;
      }
      case MirOp::ReturnVoid:
        if (ret_ty->isVoidTy()) {
          builder->CreateRetVoid();
        } else {
          builder->CreateRet(returns_float ? llvm::ConstantFP::get(ret_ty, 0.0)
                                           : llvm::ConstantInt::get(ret_ty, 0));
        }
        return false;
      case MirOp::ReturnInt:
        builder->CreateRet(int32_val(*builder, context, ins.int_value));
        return false;
      case MirOp::ReturnFloat:
        builder->CreateRet(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context),
                                                    ins.float_value));
        return false;
      case MirOp::ReturnIdent:
        if (ins.ret_is_float || returns_float || float_locals.count(ins.ident) > 0) {
          builder->CreateRet(load_float(ins.ident));
        } else {
          builder->CreateRet(load_int(ins.ident));
        }
        return false;
      case MirOp::LocalAllocInt:
        (void)ensure_int_local(ins.ident);
        return true;
      case MirOp::LocalAllocFloat:
        (void)ensure_float_local(ins.ident);
        return true;
      case MirOp::LocalAllocI64:
        (void)ensure_i64_local(ins.ident);
        return true;
      case MirOp::StoreInt: {
        llvm::Value* val = ins.rhs_is_literal ? int32_val(*builder, context, ins.rhs_int)
                                              : load_int(ins.rhs_ident);
        builder->CreateStore(val, ensure_int_local(ins.ident));
        return true;
      }
      case MirOp::StoreI64: {
        llvm::Value* val = ins.rhs_is_literal
                               ? llvm::ConstantInt::get(i64_ty(context), ins.rhs_int)
                               : load_i64(ins.rhs_ident);
        builder->CreateStore(val, ensure_i64_local(ins.ident));
        return true;
      }
      case MirOp::StoreFloat: {
        llvm::Value* val = ins.rhs_is_literal
                               ? llvm::ConstantFP::get(llvm::Type::getDoubleTy(context),
                                                       ins.float_value)
                               : load_float(ins.rhs_ident);
        builder->CreateStore(val, ensure_float_local(ins.ident));
        return true;
      }
      case MirOp::BinOpInt: {
        llvm::Value* lhs = load_int(ins.lhs_ident);
        llvm::Value* rhs = load_int(ins.rhs_ident);
        llvm::Value* result = emit_binop(ins.bin_op, lhs, rhs);
        builder->CreateStore(result, ensure_int_local(ins.ident));
        return true;
      }
      case MirOp::BinOpFloat: {
        llvm::Value* lhs = load_float(ins.lhs_ident);
        llvm::Value* rhs = load_float(ins.rhs_ident);
        llvm::Value* result = emit_fbinop(ins.bin_op, lhs, rhs);
        builder->CreateStore(result, ensure_float_local(ins.ident));
        return true;
      }
      case MirOp::EchoInt: {
        llvm::Function* rt_fn = module->getFunction("li_rt_print_int");
        llvm::Value* val = ins.ident.empty() ? int32_val(*builder, context, ins.int_value)
                                             : load_int(ins.ident);
        builder->CreateCall(rt_fn, {val});
        return true;
      }
      case MirOp::EchoString: {
        llvm::Function* rt_fn = module->getFunction("li_rt_print_str");
        llvm::GlobalVariable* gv = emit_string_global(module, ins.str_value, str_counter);
        builder->CreateCall(rt_fn, {string_ptr(*builder, gv)});
        return true;
      }
      case MirOp::CallExtern: {
        llvm::Function* callee = module->getFunction(ins.callee);
        if (!callee) {
          return true;
        }
        std::vector<llvm::Value*> args;
        for (std::size_t ai = 0; ai < ins.args.size(); ++ai) {
          const bool ptr_param =
              ai < callee->arg_size() &&
              callee->getArg(ai)->getType() == i8_ptr(context);
          llvm::Value* val = mir_arg_value(ins.args[ai], ptr_param);
          if (ptr_param && val->getType() == i64_ty(context)) {
            val = builder->CreateIntToPtr(val, i8_ptr(context));
          }
          args.push_back(val);
        }
        llvm::CallInst* call = builder->CreateCall(callee, args);
        if (!ins.ident.empty()) {
          if (ins.is_i64) {
            builder->CreateStore(call, ensure_ptr_local(ins.ident));
          } else if (ins.ret_is_float) {
            builder->CreateStore(call, ensure_float_local(ins.ident));
          } else {
            builder->CreateStore(call, ensure_int_local(ins.ident));
          }
        }
        return true;
      }
      case MirOp::CallProc: {
        llvm::Function* callee = module->getFunction(ins.callee);
        if (!callee) {
          return true;
        }
        std::vector<llvm::Value*> args;
        for (const auto& arg : ins.args) {
          args.push_back(mir_arg_value(arg, false));
        }
        llvm::CallInst* call = builder->CreateCall(callee, args);
        if (!ins.ident.empty()) {
          if (ins.ret_is_float) {
            builder->CreateStore(call, ensure_float_local(ins.ident));
          } else {
            builder->CreateStore(call, ensure_int_local(ins.ident));
          }
        }
        return true;
      }
      case MirOp::ArrayAlloc: {
        llvm::ArrayType* arr_ty =
            llvm::ArrayType::get(i32_ty(context), static_cast<unsigned>(ins.int_value));
        llvm::AllocaInst* slot = builder->CreateAlloca(arr_ty, nullptr, ins.ident);
        arrays[ins.ident] = ArraySlot{slot, ins.int_value};
        return true;
      }
      case MirOp::ArrayStoreInt: {
        auto it = arrays.find(ins.ident);
        if (it == arrays.end()) {
          return true;
        }
        llvm::Value* idx = ins.index_is_literal
                               ? int32_val(*builder, context, ins.int_value)
                               : load_int(ins.index_ident);
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        llvm::Value* gep_indices[] = {zero, idx};
        llvm::Value* ptr = builder->CreateInBoundsGEP(
            it->second.alloca->getAllocatedType(), it->second.alloca, gep_indices);
        llvm::Value* val = ins.rhs_is_literal ? int32_val(*builder, context, ins.rhs_int)
                                              : load_int(ins.rhs_ident);
        builder->CreateStore(val, ptr);
        return true;
      }
      case MirOp::ArrayLoadInt: {
        auto it = arrays.find(ins.ident);
        if (it == arrays.end()) {
          return true;
        }
        llvm::Value* idx = ins.index_is_literal
                               ? int32_val(*builder, context, ins.int_value)
                               : load_int(ins.index_ident);
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        llvm::Value* gep_indices[] = {zero, idx};
        llvm::Value* ptr = builder->CreateInBoundsGEP(
            it->second.alloca->getAllocatedType(), it->second.alloca, gep_indices);
        llvm::Value* loaded = builder->CreateLoad(i32_ty(context), ptr);
        if (!ins.lhs_ident.empty()) {
          builder->CreateStore(loaded, ensure_int_local(ins.lhs_ident));
        }
        return true;
      }
      case MirOp::LoadIntToIdent:
        return true;
    }
    return true;
  }
};

}  // namespace

bool emit_llvm_ir(const MirModule& mir, const std::string& out_path, std::string* error) {
  llvm::LLVMContext context;
  auto module = std::make_unique<llvm::Module>("li", context);

  module->getOrInsertFunction("li_rt_print_int",
                              llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                                      {i32_ty(context)}, false));
  module->getOrInsertFunction("li_rt_print_str",
                              llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                                      {i8_ptr(context)}, false));
  module->getOrInsertFunction("li_bounds_fail",
                              llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_set_args",
      llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                              {i32_ty(context), llvm::PointerType::getUnqual(i8_ptr(context))},
                              false));
  module->getOrInsertFunction("li_rt_argc",
                              llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction("li_rt_argv",
                              llvm::FunctionType::get(i8_ptr(context), {i32_ty(context)}, false));

  llvm::Function* user_main = nullptr;
  bool user_main_argv_wrapper = false;

  for (const auto& fn : mir.functions) {
    if (fn.is_extern) {
      llvm::Type* ret_ty = fn.returns_void ? llvm::Type::getVoidTy(context)
                                           : llvm_scalar(context, fn.returns_float, false);
      std::vector<llvm::Type*> param_tys;
      for (const auto& p : fn.params) {
        if (p.is_string || p.is_i64) {
          param_tys.push_back(i8_ptr(context));
        } else {
          param_tys.push_back(llvm_scalar(context, p.is_float, false));
        }
      }
      llvm::FunctionType* fn_ty = llvm::FunctionType::get(ret_ty, param_tys, false);
      llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, fn.name, module.get());
      continue;
    }

    llvm::Type* ret_ty = fn.returns_void ? llvm::Type::getVoidTy(context)
                                         : llvm_scalar(context, fn.returns_float, false);
    std::vector<llvm::Type*> param_tys;
    for (const auto& p : fn.params) {
      param_tys.push_back(llvm_scalar(context, p.is_float, p.is_i64));
    }
    llvm::FunctionType* fn_ty = llvm::FunctionType::get(ret_ty, param_tys, false);
    const bool argv_main = fn.name == "main" && fn.params.empty();
    const std::string llvm_name = argv_main ? "li_user_main" : fn.name;
    llvm::Function* func =
        llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, llvm_name, module.get());
    if (fn.name == "main") {
      user_main = func;
      user_main_argv_wrapper = argv_main;
    }

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    llvm::IRBuilder<> builder(entry);

    EmitCtx ctx{context, module.get(), func, &builder, ret_ty, fn.returns_float,
                {}, {}, {}, {}, {}};

    unsigned idx = 0;
    for (auto& arg : func->args()) {
      if (idx < fn.params.size()) {
        arg.setName(fn.params[idx].name);
        if (fn.params[idx].is_i64) {
          builder.CreateStore(&arg, ctx.ensure_i64_local(fn.params[idx].name));
        } else if (fn.params[idx].is_float) {
          builder.CreateStore(&arg, ctx.ensure_float_local(fn.params[idx].name));
        } else {
          builder.CreateStore(&arg, ctx.ensure_int_local(fn.params[idx].name));
        }
      }
      idx++;
    }

    for (const auto& ins : fn.body) {
      ctx.emit_insn(ins);
    }
  }

  if (user_main && user_main_argv_wrapper) {
    llvm::Type* argv_ty = llvm::PointerType::getUnqual(i8_ptr(context));
    llvm::FunctionType* main_ty =
        llvm::FunctionType::get(i32_ty(context), {i32_ty(context), argv_ty}, false);
    llvm::Function* main_fn =
        llvm::Function::Create(main_ty, llvm::Function::ExternalLinkage, "main", module.get());
    llvm::BasicBlock* main_entry = llvm::BasicBlock::Create(context, "entry", main_fn);
    llvm::IRBuilder<> main_builder(main_entry);
    llvm::Function* set_args = module->getFunction("li_rt_set_args");
    main_builder.CreateCall(set_args, {main_fn->getArg(0), main_fn->getArg(1)});
    llvm::CallInst* user_ret = main_builder.CreateCall(user_main, {});
    main_builder.CreateRet(user_ret);
  } else if (!user_main) {
    llvm::FunctionType* main_ty = llvm::FunctionType::get(i32_ty(context), {}, false);
    llvm::Function* main_fn =
        llvm::Function::Create(main_ty, llvm::Function::ExternalLinkage, "main", module.get());
    llvm::BasicBlock* main_entry = llvm::BasicBlock::Create(context, "entry", main_fn);
    llvm::IRBuilder<> main_builder(main_entry);
    main_builder.CreateRet(llvm::ConstantInt::get(i32_ty(context), 0));
  }

  std::string verify_err;
  llvm::raw_string_ostream verify_stream(verify_err);
  if (llvm::verifyModule(*module, &verify_stream)) {
    if (error) {
      *error = verify_err;
    }
    return false;
  }

  std::error_code ec;
  llvm::raw_fd_ostream out(out_path, ec, llvm::sys::fs::OF_Text);
  if (ec) {
    if (error) {
      *error = ec.message();
    }
    return false;
  }
  module->print(out, nullptr);
  return true;
}

}  // namespace li
