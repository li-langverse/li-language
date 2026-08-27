#include "li/emit.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <functional>
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

bool is_cmp_binop(BinOp op) {
  return op == BinOp::Lt || op == BinOp::Le || op == BinOp::Gt || op == BinOp::Ge ||
         op == BinOp::Eq || op == BinOp::Ne;
}

llvm::Type* llvm_scalar(llvm::LLVMContext& ctx, bool is_float, bool is_i64, bool is_string = false) {
  if (is_string) {
    return i8_ptr(ctx);
  }
  if (is_i64) {
    return i64_ty(ctx);
  }
  return is_float ? llvm::Type::getDoubleTy(ctx) : i32_ty(ctx);
}

llvm::Type* llvm_array_type(llvm::LLVMContext& ctx, const MirParam& p) {
  llvm::Type* elem = llvm_scalar(ctx, p.is_float, p.is_i64, p.is_string);
  if (p.is_matrix && p.matrix_cols > 0) {
    llvm::Type* row_ty = llvm::ArrayType::get(elem, static_cast<unsigned>(p.matrix_cols));
    return llvm::ArrayType::get(row_ty, static_cast<unsigned>(p.fixed_array_elems));
  }
  return llvm::ArrayType::get(elem, static_cast<unsigned>(p.fixed_array_elems));
}

llvm::Type* llvm_type_for_mir_param(llvm::LLVMContext& ctx, const MirParam& p) {
  if (p.fixed_array_elems > 0) {
    return llvm_array_type(ctx, p);
  }
  if (p.is_var) {
    // `var` object leaf param: passed by reference; the callee loads and
    // stores through the incoming pointer so mutations propagate back.
    return llvm::PointerType::getUnqual(llvm_scalar(ctx, p.is_float, p.is_i64, p.is_string));
  }
  return llvm_scalar(ctx, p.is_float, p.is_i64, p.is_string);
}

llvm::Type* llvm_array_param_ptr(llvm::LLVMContext& ctx, const MirParam& p) {
  return llvm::PointerType::getUnqual(llvm_array_type(ctx, p));
}

llvm::Type* llvm_struct_from_layout(llvm::LLVMContext& ctx, const std::vector<MirParam>& layout) {
  std::vector<llvm::Type*> elems;
  elems.reserve(layout.size());
  for (const auto& p : layout) {
    elems.push_back(llvm_type_for_mir_param(ctx, p));
  }
  if (elems.empty()) {
    return llvm::Type::getVoidTy(ctx);
  }
  return llvm::StructType::get(ctx, elems);
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

// CallInst::setFastMathFlags after CreateCall does not remove the inherited
// `fast` flag on aggregate (struct/array) returns, which clang rejects
// ("fast-math-flags specified for call without floating-point scalar or
// vector return type"). Clear the builder flags before the call instead.
llvm::CallInst* create_user_call(llvm::IRBuilder<>& builder, llvm::Function* callee,
                                 llvm::ArrayRef<llvm::Value*> args) {
  llvm::Type* ret_ty = callee->getReturnType();
  const bool strip_fmf = ret_ty->isStructTy() || ret_ty->isArrayTy();
  llvm::FastMathFlags saved;
  if (strip_fmf) {
    saved = builder.getFastMathFlags();
    builder.setFastMathFlags(llvm::FastMathFlags());
  }
  llvm::CallInst* call = builder.CreateCall(callee, args);
  if (strip_fmf) {
    builder.setFastMathFlags(saved);
  }
  return call;
}

struct ArraySlot {
  llvm::Value* alloca = nullptr;  // base pointer (alloca, or a `var` by-ref arg)
  llvm::Type* slot_ty = nullptr;  // pointee array type (opaque-pointer era)
  std::int64_t size = 0;
  std::int64_t cols = 0;
  bool is_float = false;
  bool is_matrix = false;
  bool is_i64 = false;
};

struct EmitCtx {
  llvm::LLVMContext& context;
  llvm::Module* module;
  llvm::Function* func;
  llvm::IRBuilder<>* builder;
  llvm::Type* ret_ty = nullptr;
  bool returns_float = false;
  bool returns_i64 = false;
  bool returns_object = false;
  bool fp_numerically_stable = false;
  int runtime_threads = 0;
  bool enable_array_simd = true;
  std::vector<bool> array_simd_scope_stack;
  std::map<std::string, llvm::AllocaInst*> int_locals;
  std::map<std::string, llvm::AllocaInst*> float_locals;
  std::map<std::string, llvm::AllocaInst*> i64_locals;
  std::map<std::string, llvm::AllocaInst*> ptr_locals;
  std::map<std::string, ArraySlot> arrays;
  std::map<std::string, llvm::AllocaInst*> simd_f64x4_locals;
  /** `var` object leaf params: slot name -> incoming by-ref pointer.
   *  Loads and stores on these names go straight through the pointer so
   *  callee mutations propagate back to the caller's slots. */
  std::map<std::string, llvm::Value*> var_ref_params;
  /** Element (pointee) scalar type of each var-ref slot, by slot name. */
  std::map<std::string, llvm::Type*> var_ref_elem_ty;
  std::unordered_map<std::string, llvm::BasicBlock*> labels;
  int str_counter = 0;

  llvm::Type* vec4_f64() const {
    return llvm::VectorType::get(llvm::Type::getDoubleTy(context),
                                 llvm::ElementCount::getFixed(4));
  }

  // Locals are alloca'd lazily at first use and cached by name, so a name
  // declared in two sibling branches (e.g. `var i` under `if a:` and under
  // `if b:`) would otherwise get one alloca in the first branch — an
  // "Instruction does not dominate all uses!" verifier failure. Hoisting
  // every alloca to the entry block makes it dominate the whole function.
  llvm::AllocaInst* entry_alloca(llvm::Type* ty, const std::string& name) {
    llvm::IRBuilder<> eb(&func->getEntryBlock(), func->getEntryBlock().begin());
    return eb.CreateAlloca(ty, nullptr, name);
  }

  bool array_simd_enabled() const {
    if (!array_simd_scope_stack.empty()) {
      return array_simd_scope_stack.back();
    }
    return enable_array_simd;
  }

  llvm::AllocaInst* ensure_simd_f64x4(const std::string& name) {
    auto it = simd_f64x4_locals.find(name);
    if (it != simd_f64x4_locals.end()) {
      return it->second;
    }
    llvm::AllocaInst* slot = entry_alloca(vec4_f64(), name);
    simd_f64x4_locals[name] = slot;
    return slot;
  }

  llvm::Value* load_simd_f64x4(const std::string& name) {
    return builder->CreateLoad(vec4_f64(), ensure_simd_f64x4(name));
  }

  llvm::Value* horiz_sum_f64x4(llvm::Value* vec) {
    llvm::Type* f64 = llvm::Type::getDoubleTy(context);
    llvm::Value* sum = llvm::ConstantFP::get(f64, 0.0);
    for (unsigned lane = 0; lane < 4; ++lane) {
      llvm::Value* elt = builder->CreateExtractElement(
          vec, llvm::ConstantInt::get(i32_ty(context), lane));
      sum = builder->CreateFAdd(sum, elt);
    }
    return sum;
  }

  llvm::Value* gather_array_f64x4(llvm::Value* base, llvm::Type* arr_ty, unsigned start) {
    llvm::Type* f64 = llvm::Type::getDoubleTy(context);
    llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
    llvm::Value* vec = llvm::UndefValue::get(vec4_f64());
    for (unsigned lane = 0; lane < 4; ++lane) {
      llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), start + lane);
      llvm::Value* gep_idx[] = {zero, idx};
      llvm::Value* ptr = builder->CreateInBoundsGEP(arr_ty, base, gep_idx);
      llvm::Value* scalar = builder->CreateLoad(f64, ptr);
      vec = builder->CreateInsertElement(
          vec, scalar, llvm::ConstantInt::get(i32_ty(context), lane));
    }
    return vec;
  }

  llvm::Value* matmul_gep2d(llvm::AllocaInst* mat, llvm::Value* row, llvm::Value* col) {
    llvm::Value* zero = llvm::ConstantInt::get(i32_ty(context), 0);
    llvm::Value* idx[] = {zero, row, col};
    return builder->CreateInBoundsGEP(mat->getAllocatedType(), mat, idx);
  }

  void emit_idx_for(llvm::AllocaInst* iv, llvm::Value* limit,
                    const std::function<void(llvm::Value*)>& body) {
    llvm::Type* i32t = i32_ty(context);
    llvm::BasicBlock* head = llvm::BasicBlock::Create(context, "for_head", func);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(context, "for_body", func);
    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(context, "for_exit", func);
    builder->CreateStore(llvm::ConstantInt::get(i32t, 0), iv);
    builder->CreateBr(head);
    builder->SetInsertPoint(head);
    llvm::Value* i = builder->CreateLoad(i32t, iv);
    llvm::Value* cond = builder->CreateICmpULT(i, limit);
    builder->CreateCondBr(cond, body_bb, exit_bb);
    builder->SetInsertPoint(body_bb);
    body(i);
    llvm::Value* next = builder->CreateAdd(i, llvm::ConstantInt::get(i32t, 1));
    builder->CreateStore(next, iv);
    builder->CreateBr(head);
    builder->SetInsertPoint(exit_bb);
  }

  void emit_matmul2d_ijk_loops(llvm::AllocaInst* c_mat, llvm::AllocaInst* a_mat,
                               llvm::AllocaInst* b_mat, unsigned m, unsigned k,
                               unsigned n) {
    llvm::Type* f64 = llvm::Type::getDoubleTy(context);
    llvm::Type* i32t = i32_ty(context);
    llvm::Value* lim_m = llvm::ConstantInt::get(i32t, m);
    llvm::Value* lim_k = llvm::ConstantInt::get(i32t, k);
    llvm::Value* lim_n = llvm::ConstantInt::get(i32t, n);
    llvm::Value* zf = llvm::ConstantFP::get(f64, 0.0);
    llvm::AllocaInst* i_s = builder->CreateAlloca(i32t, nullptr, "mm_i");
    llvm::AllocaInst* j_s = builder->CreateAlloca(i32t, nullptr, "mm_j");
    llvm::AllocaInst* t_s = builder->CreateAlloca(i32t, nullptr, "mm_t");

    emit_idx_for(i_s, lim_m, [&](llvm::Value* i) {
      emit_idx_for(j_s, lim_n, [&](llvm::Value* j) {
        builder->CreateStore(zf, matmul_gep2d(c_mat, i, j));
      });
    });

    llvm::Function* fma_fn = nullptr;
    if (!fp_numerically_stable) {
      fma_fn = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::fmuladd, {f64});
    }

    emit_idx_for(i_s, lim_m, [&](llvm::Value* i) {
      emit_idx_for(t_s, lim_k, [&](llvm::Value* t) {
        llvm::Value* aik = builder->CreateLoad(f64, matmul_gep2d(a_mat, i, t));
        emit_idx_for(j_s, lim_n, [&](llvm::Value* j) {
          llvm::Value* cp = matmul_gep2d(c_mat, i, j);
          llvm::Value* cv = builder->CreateLoad(f64, cp);
          llvm::Value* bv = builder->CreateLoad(f64, matmul_gep2d(b_mat, t, j));
          if (fma_fn != nullptr) {
            builder->CreateStore(builder->CreateCall(fma_fn, {aik, bv, cv}), cp);
          } else {
            builder->CreateStore(builder->CreateFAdd(cv, builder->CreateFMul(aik, bv)), cp);
          }
        });
      });
    });
  }

  void emit_matmul2d_ijk_unrolled(llvm::AllocaInst* c_mat, llvm::AllocaInst* a_mat,
                                  llvm::AllocaInst* b_mat, unsigned m, unsigned k,
                                  unsigned n) {
    llvm::Type* f64 = llvm::Type::getDoubleTy(context);
    llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
    llvm::Value* zf = llvm::ConstantFP::get(f64, 0.0);
    for (unsigned i = 0; i < m; ++i) {
      llvm::Value* ri = llvm::ConstantInt::get(i32_ty(context), i);
      for (unsigned j = 0; j < n; ++j) {
        llvm::Value* rj = llvm::ConstantInt::get(i32_ty(context), j);
        builder->CreateStore(zf, matmul_gep2d(c_mat, ri, rj));
      }
    }
    llvm::Function* fma_fn = nullptr;
    if (!fp_numerically_stable) {
      fma_fn = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::fmuladd, {f64});
    }
    for (unsigned i = 0; i < m; ++i) {
      llvm::Value* ri = llvm::ConstantInt::get(i32_ty(context), i);
      for (unsigned t = 0; t < k; ++t) {
        llvm::Value* rt = llvm::ConstantInt::get(i32_ty(context), t);
        llvm::Value* av = builder->CreateLoad(f64, matmul_gep2d(a_mat, ri, rt));
        for (unsigned j = 0; j < n; ++j) {
          llvm::Value* rj = llvm::ConstantInt::get(i32_ty(context), j);
          llvm::Value* cp = matmul_gep2d(c_mat, ri, rj);
          llvm::Value* cv = builder->CreateLoad(f64, cp);
          llvm::Value* bv = builder->CreateLoad(f64, matmul_gep2d(b_mat, rt, rj));
          if (fma_fn != nullptr) {
            builder->CreateStore(builder->CreateCall(fma_fn, {av, bv, cv}), cp);
          } else {
            builder->CreateStore(builder->CreateFAdd(cv, builder->CreateFMul(av, bv)), cp);
          }
        }
      }
    }
  }

  void scatter_array_f64x4(llvm::Value* base, llvm::Type* arr_ty, unsigned start, llvm::Value* vec) {
    llvm::Type* f64 = llvm::Type::getDoubleTy(context);
    llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
    for (unsigned lane = 0; lane < 4; ++lane) {
      llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), start + lane);
      llvm::Value* gep_idx[] = {zero, idx};
      llvm::Value* ptr = builder->CreateInBoundsGEP(arr_ty, base, gep_idx);
      llvm::Value* scalar = builder->CreateExtractElement(
          vec, llvm::ConstantInt::get(i32_ty(context), lane));
      builder->CreateStore(scalar, ptr);
    }
  }


  llvm::AllocaInst* ensure_int_local(const std::string& name) {
    auto it = int_locals.find(name);
    if (it != int_locals.end()) {
      return it->second;
    }
    llvm::AllocaInst* slot = entry_alloca(i32_ty(context), name);
    int_locals[name] = slot;
    return slot;
  }

  llvm::AllocaInst* ensure_float_local(const std::string& name) {
    auto it = float_locals.find(name);
    if (it != float_locals.end()) {
      return it->second;
    }
    llvm::AllocaInst* slot = entry_alloca(llvm::Type::getDoubleTy(context), name);
    float_locals[name] = slot;
    return slot;
  }

  llvm::Value* load_as_f64(const std::string& name) {
    if (auto it = var_ref_params.find(name); it != var_ref_params.end()) {
      return load_ref_as_f64(name, it->second);
    }
    if (auto it = float_locals.find(name); it != float_locals.end()) {
      return builder->CreateLoad(llvm::Type::getDoubleTy(context), it->second);
    }
    if (auto it64 = i64_locals.find(name); it64 != i64_locals.end()) {
      return builder->CreateSIToFP(builder->CreateLoad(i64_ty(context), it64->second),
                                   llvm::Type::getDoubleTy(context));
    }
    return builder->CreateSIToFP(builder->CreateLoad(i32_ty(context), ensure_int_local(name)),
                                 llvm::Type::getDoubleTy(context));
  }

  llvm::AllocaInst* ensure_ptr_local(const std::string& name) {
    auto it = ptr_locals.find(name);
    if (it != ptr_locals.end()) {
      return it->second;
    }
    llvm::AllocaInst* slot = entry_alloca(i8_ptr(context), name);
    ptr_locals[name] = slot;
    return slot;
  }

  llvm::Value* load_ptr(const std::string& name) {
    if (auto it = var_ref_params.find(name); it != var_ref_params.end()) {
      return builder->CreateLoad(i8_ptr(context), it->second);
    }
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
    }    llvm::AllocaInst* slot = entry_alloca(i64_ty(context), name);
    i64_locals[name] = slot;
    return slot;
  }

  // Address of a slot by name. For `var` object refs this is the incoming
  // pointer itself; for locals it is the alloca. Used by CallProc to pass
  // by-ref args and by stores.
  llvm::Value* slot_ptr(const std::string& name) {
    if (auto it = var_ref_params.find(name); it != var_ref_params.end()) {
      return it->second;
    }
    if (auto it = arrays.find(name); it != arrays.end()) {
      return it->second.alloca;
    }
    if (auto it = float_locals.find(name); it != float_locals.end()) {
      return it->second;
    }
    if (auto it = i64_locals.find(name); it != i64_locals.end()) {
      return it->second;
    }
    if (auto it = ptr_locals.find(name); it != ptr_locals.end()) {
      return it->second;
    }
    return ensure_int_local(name);
  }

  // Loads a var-ref slot as a double (float context).
  llvm::Value* load_ref_as_f64(const std::string& name, llvm::Value* ptr) {
    llvm::Type* el = var_ref_elem_ty[name];
    if (el->isDoubleTy()) {
      return builder->CreateLoad(llvm::Type::getDoubleTy(context), ptr);
    }
    if (el->isIntegerTy(64)) {
      return builder->CreateSIToFP(builder->CreateLoad(i64_ty(context), ptr),
                                   llvm::Type::getDoubleTy(context));
    }
    return builder->CreateSIToFP(builder->CreateLoad(i32_ty(context), ptr),
                                 llvm::Type::getDoubleTy(context));
  }

  llvm::Value* load_float(const std::string& name) {
    if (auto it = var_ref_params.find(name); it != var_ref_params.end()) {
      return load_ref_as_f64(name, it->second);
    }
    return builder->CreateLoad(llvm::Type::getDoubleTy(context), ensure_float_local(name));
  }

  llvm::Value* load_int(const std::string& name) {
    if (auto it = var_ref_params.find(name); it != var_ref_params.end()) {
      llvm::Type* el = var_ref_elem_ty[name];
      if (el->isDoubleTy()) {
        return builder->CreateFPToSI(
            builder->CreateLoad(llvm::Type::getDoubleTy(context), it->second), i32_ty(context));
      }
      if (el->isIntegerTy(64)) {
        return builder->CreateTrunc(
            builder->CreateLoad(i64_ty(context), it->second), i32_ty(context));
      }
      return builder->CreateLoad(i32_ty(context), it->second);
    }
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
    if (auto it = var_ref_params.find(name); it != var_ref_params.end()) {
      llvm::Type* el = var_ref_elem_ty[name];
      if (el->isIntegerTy(64)) {
        return builder->CreateLoad(i64_ty(context), it->second);
      }
      if (el->isDoubleTy()) {
        return builder->CreateFPToSI(
            builder->CreateLoad(llvm::Type::getDoubleTy(context), it->second), i64_ty(context));
      }
      return builder->CreateSExt(
          builder->CreateLoad(i32_ty(context), it->second), i64_ty(context));
    }
    if (auto it = i64_locals.find(name); it != i64_locals.end()) {
      return builder->CreateLoad(i64_ty(context), it->second);
    }
    return builder->CreateSExt(load_int(name), i64_ty(context));
  }

  // Store `val` into the slot named `name`, going through the by-ref pointer
  // when the slot is a `var` object leaf param (converting to the element
  // type as needed).
  void store_to_slot(const std::string& name, llvm::Value* val) {
    if (auto it = var_ref_params.find(name); it != var_ref_params.end()) {
      llvm::Type* el = var_ref_elem_ty[name];
      llvm::Value* v = val;
      if (el->isDoubleTy() && v->getType()->isIntegerTy(32)) {
        v = builder->CreateSIToFP(v, el);
      } else if (el->isDoubleTy() && v->getType()->isIntegerTy(64)) {
        v = builder->CreateSIToFP(v, el);
      } else if (el->isIntegerTy(64) && v->getType()->isIntegerTy(32)) {
        v = builder->CreateSExt(v, el);
      } else if (el->isIntegerTy(64) && v->getType()->isPointerTy()) {
        v = builder->CreatePtrToInt(v, el);
      } else if (el->isPointerTy() && v->getType()->isIntegerTy(64)) {
        v = builder->CreateIntToPtr(v, el);
      } else if (el->isIntegerTy(32) && v->getType()->isIntegerTy(64)) {
        v = builder->CreateTrunc(v, el);
      }
      builder->CreateStore(v, it->second);
      return;
    }
    if (auto it = arrays.find(name); it != arrays.end()) {
      // Callers store into array slots via element stores; a whole-array store
      // only occurs for the copy paths which target ArrayAlloc slots.
      builder->CreateStore(val, it->second.alloca);
      return;
    }
    builder->CreateStore(val, ensure_int_local(name));
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

  llvm::Function* rt_fn_i32_i32(const char* name) {
    if (llvm::Function* f = module->getFunction(name)) {
      return f;
    }
    llvm::FunctionType* ft =
        llvm::FunctionType::get(i32_ty(context), {i32_ty(context), i32_ty(context)}, false);
    return llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, module);
  }

  llvm::Value* call_rt_i32_i32(const char* name, llvm::Value* a, llvm::Value* b) {
    return builder->CreateCall(rt_fn_i32_i32(name), {a, b});
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
      case BinOp::Mod:
        return builder->CreateSRem(lhs, rhs);
      case BinOp::FloorDiv:
        return call_rt_i32_i32("li_rt_floor_div_i32", lhs, rhs);
      case BinOp::Pow:
        return call_rt_i32_i32("li_rt_pow_i32", lhs, rhs);
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
      case BinOp::Pow: {
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Function* pow_fn =
            llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::pow, {f64});
        return builder->CreateCall(pow_fn, {lhs, rhs});
      }
      case BinOp::Lt:
        return builder->CreateZExt(builder->CreateFCmpOLT(lhs, rhs), i32_ty(context));
      case BinOp::Le:
        return builder->CreateZExt(builder->CreateFCmpOLE(lhs, rhs), i32_ty(context));
      case BinOp::Gt:
        return builder->CreateZExt(builder->CreateFCmpOGT(lhs, rhs), i32_ty(context));
      case BinOp::Ge:
        return builder->CreateZExt(builder->CreateFCmpOGE(lhs, rhs), i32_ty(context));
      case BinOp::Eq:
        return builder->CreateZExt(builder->CreateFCmpOEQ(lhs, rhs), i32_ty(context));
      case BinOp::Ne:
        return builder->CreateZExt(builder->CreateFCmpONE(lhs, rhs), i32_ty(context));
      default:
        return lhs;
    }
  }

  llvm::Value* mir_arg_value(const MirArg& arg, bool ptr_param = false) {
    if (arg.is_string) {
      llvm::GlobalVariable* gv = emit_string_global(module, arg.str_value, str_counter);
      return string_ptr(*builder, gv);
    }
    if (arg.is_float_literal) {
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), arg.float_value);
    }
    if (arg.is_literal) {
      return int32_val(*builder, context, arg.int_value);
    }
    if (arg.is_array_ident) {
      if (auto a_it = arrays.find(arg.ident); a_it != arrays.end()) {
        return a_it->second.alloca;
      }
    }
    if (auto a_it = arrays.find(arg.ident); a_it != arrays.end()) {
      llvm::Type* arr_ty = a_it->second.slot_ty;
      return builder->CreateLoad(arr_ty, a_it->second.alloca);
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
        // The preceding block may already terminate (e.g. an `if` branch that
        // ends in `return`); an unconditional jump there is dead code.
        if (builder->GetInsertBlock() != nullptr &&
            builder->GetInsertBlock()->getTerminator() == nullptr) {
          builder->CreateBr(block_for(ins.label));
        }
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
        } else if (returns_object && ret_ty->isStructTy()) {
          builder->CreateRet(llvm::ConstantAggregateZero::get(ret_ty));
        } else if (ret_ty->isPointerTy()) {
          builder->CreateRet(llvm::ConstantPointerNull::get(
              llvm::dyn_cast<llvm::PointerType>(ret_ty)));
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
        } else if (ins.ret_is_i64 || returns_i64 || i64_locals.count(ins.ident) > 0) {
          llvm::Value* wide = load_i64(ins.ident);
          if (ret_ty->isPointerTy()) {
            builder->CreateRet(builder->CreateIntToPtr(wide, ret_ty));
          } else {
            builder->CreateRet(wide);
          }
        } else {
          builder->CreateRet(load_int(ins.ident));
        }
        return false;
      case MirOp::ReturnObject: {
        auto* st = llvm::dyn_cast<llvm::StructType>(ret_ty);
        if (!st || ins.object_layout.empty()) {
          builder->CreateRetVoid();
          return false;
        }
        llvm::Value* agg = llvm::UndefValue::get(st);
        for (unsigned i = 0; i < ins.object_layout.size(); ++i) {
          const MirParam& leaf = ins.object_layout[i];
          const std::string full = ins.ident + "_" + leaf.name;
          llvm::Value* v = nullptr;
          if (leaf.fixed_array_elems > 0) {
            auto a_it = arrays.find(full);
            if (a_it != arrays.end()) {
              llvm::Type* arr_ty = a_it->second.slot_ty;
              v = builder->CreateLoad(arr_ty, a_it->second.alloca);
            }
          } else {
            v = leaf.is_float ? load_float(full)
                            : (leaf.is_i64 || leaf.is_string ? load_i64(full) : load_int(full));
          }
          if (!v) {
            continue;
          }
          llvm::Type* want = st->getElementType(i);
          if (v->getType() != want) {
            if (want->isIntegerTy(32) && v->getType()->isIntegerTy(64)) {
              v = builder->CreateTrunc(v, want);
            } else if (want->isIntegerTy(64) && v->getType()->isIntegerTy(32)) {
              v = builder->CreateSExt(v, want);
            }
          }
          agg = builder->CreateInsertValue(agg, v, i);
        }
        builder->CreateRet(agg);
        return false;
      }
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
        if (var_ref_params.find(ins.ident) != var_ref_params.end()) {
          store_to_slot(ins.ident, val);
        } else {
          builder->CreateStore(val, ensure_int_local(ins.ident));
        }
        return true;
      }
      case MirOp::StoreI64: {
        llvm::Value* val = nullptr;
        if (ins.rhs_is_string) {
          llvm::GlobalVariable* gv = emit_string_global(module, ins.str_value, str_counter);
          val = builder->CreatePtrToInt(string_ptr(*builder, gv), i64_ty(context));
        } else {
          val = ins.rhs_is_literal
                    ? llvm::ConstantInt::get(i64_ty(context), ins.rhs_int)
                    : load_i64(ins.rhs_ident);
        }
        if (var_ref_params.find(ins.ident) != var_ref_params.end()) {
          store_to_slot(ins.ident, val);
        } else {
          builder->CreateStore(val, ensure_i64_local(ins.ident));
        }
        return true;
      }
      case MirOp::StoreFloat: {
        llvm::Value* val = ins.rhs_is_literal
                               ? llvm::ConstantFP::get(llvm::Type::getDoubleTy(context),
                                                       ins.float_value)
                               : load_float(ins.rhs_ident);
        if (var_ref_params.find(ins.ident) != var_ref_params.end()) {
          store_to_slot(ins.ident, val);
        } else {
          builder->CreateStore(val, ensure_float_local(ins.ident));
        }
        return true;
      }
      case MirOp::BinOpInt: {
        llvm::Value* lhs = ins.lhs_is_literal ? int32_val(*builder, context, ins.lhs_int)
                                              : load_int(ins.lhs_ident);
        llvm::Value* rhs = ins.rhs_is_literal ? int32_val(*builder, context, ins.rhs_int)
                                              : load_int(ins.rhs_ident);
        llvm::Value* result = emit_binop(ins.bin_op, lhs, rhs);
        builder->CreateStore(result, ensure_int_local(ins.ident));
        return true;
      }
      case MirOp::FmaFloatF64: {
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Value* factor = load_float(ins.lhs_ident);
        llvm::Value* acc = load_float(ins.rhs_ident);
        llvm::Value* addend = llvm::ConstantFP::get(f64, ins.float_value);
        llvm::Function* fma_fn =
            llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::fmuladd, {f64});
        llvm::Value* result = builder->CreateCall(fma_fn, {factor, acc, addend});
        builder->CreateStore(result, ensure_float_local(ins.ident));
        return true;
      }
      case MirOp::HornerFmaUnroll: {
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Function* fma_fn =
            llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::fmuladd, {f64});
        llvm::Value* x = load_float(ins.lhs_ident);
        llvm::Value* one = llvm::ConstantFP::get(f64, ins.float_value);
        llvm::Value* acc = load_float(ins.ident);
        const int steps = static_cast<int>(ins.int_value > 0 ? ins.int_value : 1);
        for (int i = 0; i < steps; ++i) {
          acc = builder->CreateCall(fma_fn, {x, acc, one});
        }
        builder->CreateStore(acc, ensure_float_local(ins.ident));
        return true;
      }
      case MirOp::HornerStepPow4: {
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Function* fma_fn =
            llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::fmuladd, {f64});
        const double x = ins.float_value;
        const double x2 = x * x;
        const double x3 = x2 * x;
        const double x4 = x3 * x;
        const double tail = 1.0 + x + x2 + x3;
        llvm::Value* x4v = llvm::ConstantFP::get(f64, x4);
        llvm::Value* tailv = llvm::ConstantFP::get(f64, tail);
        llvm::Value* acc = load_float(ins.ident);
        const int steps = static_cast<int>(ins.int_value > 0 ? ins.int_value : 1);
        for (int i = 0; i < steps; ++i) {
          acc = builder->CreateCall(fma_fn, {x4v, acc, tailv});
        }
        builder->CreateStore(acc, ensure_float_local(ins.ident));
        return true;
      }
      case MirOp::BinOpFloat: {
        llvm::Value* lhs = load_as_f64(ins.lhs_ident);
        llvm::Value* rhs = load_as_f64(ins.rhs_ident);
        llvm::Value* result = emit_fbinop(ins.bin_op, lhs, rhs);
        if (is_cmp_binop(ins.bin_op)) {
          builder->CreateStore(result, ensure_int_local(ins.ident));
        } else {
          builder->CreateStore(result, ensure_float_local(ins.ident));
        }
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
        llvm::CallInst* call = create_user_call(*builder, callee, args);
        if (!ins.ident.empty()) {
          if (ins.is_i64) {
            llvm::Value* wide = call;
            if (wide->getType()->isPointerTy()) {
              wide = builder->CreatePtrToInt(wide, i64_ty(context));
            } else if (wide->getType()->isIntegerTy(32)) {
              wide = builder->CreateSExt(wide, i64_ty(context));
            }
            builder->CreateStore(wide, ensure_i64_local(ins.ident));
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
        for (std::size_t ai = 0; ai < ins.args.size(); ++ai) {
          llvm::Type* want =
              ai < callee->arg_size() ? callee->getArg(ai)->getType() : nullptr;
          const bool i8_ptr_param = want && want == i8_ptr(context);
          llvm::Value* val;
          if (ins.args[ai].is_var_ref && !ins.args[ai].is_array_ident) {
            // `var` object leaf param: pass the address of the slot so the
            // callee's writes land in the caller's slots directly.
            val = slot_ptr(ins.args[ai].ident);
          } else {
            val = mir_arg_value(ins.args[ai], i8_ptr_param);
          }
          if (i8_ptr_param && val->getType() == i64_ty(context)) {
            val = builder->CreateIntToPtr(val, i8_ptr(context));
          }
          if (want != nullptr && val->getType() != want) {
            if (ins.args[ai].is_var_ref && want->isPointerTy()) {
              val = builder->CreatePointerCast(val, want);
            } else if (ins.args[ai].is_array_ident && want->isPointerTy()) {
              val = builder->CreatePointerCast(val, want);
            } else if (want->isIntegerTy(64) && val->getType()->isPointerTy()) {
              // pointer-width user-fn param (str/bytes/ptr): the ABI passes
              // pointers as i64, so string-literal globals must be narrowed.
              val = builder->CreatePtrToInt(val, want);
            } else if (want->isPointerTy() && val->getType()->isIntegerTy(64)) {
              val = builder->CreateIntToPtr(val, want);
            } else if (want->isDoubleTy() && val->getType()->isIntegerTy(32)) {
              val = builder->CreateSIToFP(val, want);
            } else if (want->isIntegerTy(32) && val->getType()->isDoubleTy()) {
              val = builder->CreateFPTrunc(val, llvm::Type::getFloatTy(context));
              val = builder->CreateBitCast(val, want);
            } else if (want->isIntegerTy(32) && val->getType()->isFloatTy()) {
              val = builder->CreateBitCast(val, want);
            }
          }
          args.push_back(val);
        }
        llvm::CallInst* call = create_user_call(*builder, callee, args);
        if (callee->getReturnType()->isVoidTy()) {
          return true;
        }
        if (!ins.object_layout.empty() && callee->getReturnType()->isStructTy()) {
          llvm::Value* agg = call;
          auto* st = llvm::cast<llvm::StructType>(callee->getReturnType());
          for (unsigned i = 0; i < ins.object_layout.size(); ++i) {
            const MirParam& leaf = ins.object_layout[i];
            const std::string full = ins.ident + "_" + leaf.name;
            llvm::Value* elt = builder->CreateExtractValue(agg, i);
            llvm::Type* want = st->getElementType(i);
            if (leaf.fixed_array_elems > 0) {
              auto a_it = arrays.find(full);
              if (a_it != arrays.end() && elt->getType() == a_it->second.slot_ty) {
                builder->CreateStore(elt, a_it->second.alloca);
              }
              continue;
            }
            if (elt->getType() != want) {
              if (want->isIntegerTy(32) && elt->getType()->isIntegerTy(64)) {
                elt = builder->CreateTrunc(elt, want);
              } else if (want->isIntegerTy(64) && elt->getType()->isIntegerTy(32)) {
                elt = builder->CreateSExt(elt, want);
              }
            }
            if (leaf.is_float) {
              builder->CreateStore(elt, ensure_float_local(full));
            } else if (leaf.is_i64 || leaf.is_string) {
              builder->CreateStore(elt, ensure_i64_local(full));
            } else {
              builder->CreateStore(elt, ensure_int_local(full));
            }
          }
          return true;
        }
        if (!ins.ident.empty()) {
          if (ins.ret_is_float) {
            builder->CreateStore(call, ensure_float_local(ins.ident));
          } else if (ins.ret_is_i64) {
            llvm::Value* wide = call;
            if (wide->getType()->isPointerTy()) {
              wide = builder->CreatePtrToInt(wide, i64_ty(context));
            } else if (wide->getType()->isIntegerTy(32)) {
              wide = builder->CreateSExt(wide, i64_ty(context));
            }
            builder->CreateStore(wide, ensure_i64_local(ins.ident));
          } else {
            builder->CreateStore(call, ensure_int_local(ins.ident));
          }
        }
        return true;
      }
      case MirOp::LocalAllocSimdF64:
        (void)ensure_simd_f64x4(ins.ident);
        return true;
      case MirOp::SimdSplatF64: {
        llvm::Value* scalar = ins.rhs_is_literal
                                  ? llvm::ConstantFP::get(llvm::Type::getDoubleTy(context),
                                                          ins.float_value)
                                  : load_float(ins.rhs_ident);
        llvm::Value* vec = builder->CreateVectorSplat(4, scalar);
        builder->CreateStore(vec, ensure_simd_f64x4(ins.ident));
        return true;
      }
      case MirOp::SimdMulF64:
      case MirOp::SimdAddF64: {
        llvm::Value* lhs = load_simd_f64x4(ins.lhs_ident);
        llvm::Value* rhs = load_simd_f64x4(ins.rhs_ident);
        llvm::Value* result = ins.op == MirOp::SimdMulF64 ? builder->CreateFMul(lhs, rhs)
                                                          : builder->CreateFAdd(lhs, rhs);
        builder->CreateStore(result, ensure_simd_f64x4(ins.ident));
        return true;
      }
      case MirOp::SimdHorizSumF64: {
        llvm::Value* vec = load_simd_f64x4(ins.lhs_ident);
        llvm::Value* sum = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 0.0);
        for (unsigned i = 0; i < 4; ++i) {
          llvm::Value* elt = builder->CreateExtractElement(vec, llvm::ConstantInt::get(i32_ty(context), i));
          sum = builder->CreateFAdd(sum, elt);
        }
        builder->CreateStore(sum, ensure_float_local(ins.ident));
        return true;
      }
      case MirOp::SimdCopyF64: {
        llvm::Value* vec = load_simd_f64x4(ins.rhs_ident);
        builder->CreateStore(vec, ensure_simd_f64x4(ins.ident));
        return true;
      }
      case MirOp::OmpParallelFor: {
        llvm::Function* par_fn = module->getFunction(ins.callee);
        if (!par_fn) {
          return true;
        }
        llvm::FunctionType* iter_ty =
            llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i64_ty(context)}, false);
        llvm::FunctionType* par_ty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context),
            {i64_ty(context), i64_ty(context), iter_ty->getPointerTo(), i32_ty(context)},
            false);
        llvm::FunctionCallee par_rt =
            module->getOrInsertFunction("li_parallel_for_i64", par_ty);
        builder->CreateCall(
            par_rt,
            {llvm::ConstantInt::get(i64_ty(context), ins.int_value),
             llvm::ConstantInt::get(i64_ty(context), ins.rhs_int), par_fn,
             llvm::ConstantInt::get(i32_ty(context), runtime_threads)});
        return true;
      }
      case MirOp::ArrayAlloc: {
        llvm::AllocaInst* slot = nullptr;
        if (ins.array_is_matrix) {
          llvm::Type* f64 = llvm::Type::getDoubleTy(context);
          llvm::ArrayType* row_ty =
              llvm::ArrayType::get(f64, static_cast<unsigned>(ins.rhs_int));
          llvm::ArrayType* mat_ty =
              llvm::ArrayType::get(row_ty, static_cast<unsigned>(ins.int_value));
          slot = entry_alloca(mat_ty, ins.ident);
          arrays[ins.ident] = ArraySlot{slot, mat_ty, ins.int_value, ins.rhs_int, true, true};
        } else {
          llvm::Type* elem_ty = llvm::Type::getDoubleTy(context);
          if (ins.array_is_float) {
            elem_ty = llvm::Type::getDoubleTy(context);
          } else if (ins.array_is_i64) {
            elem_ty = i64_ty(context);
          } else {
            elem_ty = i32_ty(context);
          }
          llvm::ArrayType* arr_ty =
              llvm::ArrayType::get(elem_ty, static_cast<unsigned>(ins.int_value));
          slot = entry_alloca(arr_ty, ins.ident);
          arrays[ins.ident] = ArraySlot{slot, arr_ty, ins.int_value, 0, ins.array_is_float, false,
                                        ins.array_is_i64};
        }
        return true;
      }
      case MirOp::ArrayStoreInt:
      case MirOp::ArrayStoreFloat: {
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
            it->second.slot_ty, it->second.alloca, gep_indices);
        if (it->second.is_float || ins.op == MirOp::ArrayStoreFloat) {
          llvm::Value* val =
              ins.rhs_is_literal
                  ? llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), ins.float_value)
                  : load_float(ins.rhs_ident);
          builder->CreateStore(val, ptr);
        } else if (it->second.is_i64) {
          llvm::Value* val = ins.rhs_is_literal
                                 ? llvm::ConstantInt::get(i64_ty(context), ins.rhs_int)
                                 : load_i64(ins.rhs_ident);
          builder->CreateStore(val, ptr);
        } else {
          llvm::Value* val = ins.rhs_is_literal ? int32_val(*builder, context, ins.rhs_int)
                                                : load_int(ins.rhs_ident);
          builder->CreateStore(val, ptr);
        }
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
            it->second.slot_ty, it->second.alloca, gep_indices);
        if (it->second.is_float) {
          llvm::Value* loaded = builder->CreateLoad(llvm::Type::getDoubleTy(context), ptr);
          if (!ins.lhs_ident.empty()) {
            builder->CreateStore(loaded, ensure_float_local(ins.lhs_ident));
          }
        } else if (it->second.is_i64) {
          llvm::Value* loaded = builder->CreateLoad(i64_ty(context), ptr);
          if (!ins.lhs_ident.empty()) {
            builder->CreateStore(loaded, ensure_i64_local(ins.lhs_ident));
          }
        } else {
          llvm::Value* loaded = builder->CreateLoad(i32_ty(context), ptr);
          if (!ins.lhs_ident.empty()) {
            builder->CreateStore(loaded, ensure_int_local(ins.lhs_ident));
          }
        }
        return true;
      }
      case MirOp::ArrayLoadFloat:
        return true;
      case MirOp::ArraySumF64: {
        auto a_it = arrays.find(ins.lhs_ident);
        if (a_it == arrays.end()) {
          return true;
        }
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Value* sum = llvm::ConstantFP::get(f64, 0.0);
        llvm::Value* c_comp = llvm::ConstantFP::get(f64, 0.0);
        const auto n = static_cast<unsigned>(ins.int_value);
        for (unsigned i = 0; i < n; ++i) {
          llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), i);
          llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
          llvm::Value* gep_idx[] = {zero, idx};
          llvm::Value* ap = builder->CreateInBoundsGEP(
              a_it->second.slot_ty, a_it->second.alloca, gep_idx);
          llvm::Value* y = builder->CreateLoad(f64, ap);
          if (fp_numerically_stable) {
            llvm::Value* y_adj = builder->CreateFSub(y, c_comp);
            llvm::Value* t = builder->CreateFAdd(sum, y_adj);
            c_comp = builder->CreateFSub(builder->CreateFSub(t, sum), y_adj);
            sum = t;
          } else {
            sum = builder->CreateFAdd(sum, y);
          }
        }
        builder->CreateStore(sum, ensure_float_local(ins.ident));
        return true;
      }
      case MirOp::ArraySumI64: {
        auto a_it = arrays.find(ins.lhs_ident);
        if (a_it == arrays.end()) {
          return true;
        }
        llvm::Value* acc = llvm::ConstantInt::get(i32_ty(context), 0);
        const auto n = static_cast<unsigned>(ins.int_value);
        for (unsigned i = 0; i < n; ++i) {
          llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), i);
          llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
          llvm::Value* gep_idx[] = {zero, idx};
          llvm::Value* ap = builder->CreateInBoundsGEP(
              a_it->second.slot_ty, a_it->second.alloca, gep_idx);
          llvm::Value* av = builder->CreateLoad(i32_ty(context), ap);
          acc = builder->CreateAdd(acc, av);
        }
        builder->CreateStore(acc, ensure_int_local(ins.ident));
        return true;
      }
      case MirOp::ArrayBinOpF64: {
        auto d_it = arrays.find(ins.ident);
        auto a_it = arrays.find(ins.lhs_ident);
        auto b_it = arrays.find(ins.rhs_ident);
        if (d_it == arrays.end() || a_it == arrays.end() || b_it == arrays.end()) {
          return true;
        }
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        const auto n = static_cast<unsigned>(ins.int_value);
        const bool simd_ok = array_simd_enabled() && ins.bin_op != BinOp::Pow &&
                             !ins.array_broadcast_lhs_len1 && !ins.array_broadcast_rhs_len1;
        const unsigned simd_end = simd_ok ? (n / 4) * 4 : 0;
        for (unsigned i = 0; i < simd_end; i += 4) {
          llvm::Value* av = gather_array_f64x4(a_it->second.alloca, a_it->second.slot_ty, i);
          llvm::Value* bv = gather_array_f64x4(b_it->second.alloca, b_it->second.slot_ty, i);
          llvm::Value* rv = emit_fbinop(ins.bin_op, av, bv);
          scatter_array_f64x4(d_it->second.alloca, d_it->second.slot_ty, i, rv);
        }
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        llvm::Value* b_broadcast = nullptr;
        if (ins.array_broadcast_rhs_len1) {
          llvm::Value* gep0[] = {zero, zero};
          llvm::Value* bp0 = builder->CreateInBoundsGEP(
              b_it->second.slot_ty, b_it->second.alloca, gep0);
          b_broadcast = builder->CreateLoad(f64, bp0);
        }
        llvm::Value* a_broadcast = nullptr;
        if (ins.array_broadcast_lhs_len1) {
          llvm::Value* gep0[] = {zero, zero};
          llvm::Value* ap0 = builder->CreateInBoundsGEP(
              a_it->second.slot_ty, a_it->second.alloca, gep0);
          a_broadcast = builder->CreateLoad(f64, ap0);
        }
        for (unsigned i = simd_end; i < n; ++i) {
          llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), i);
          llvm::Value* gep_idx[] = {zero, idx};
          llvm::Value* ap = builder->CreateInBoundsGEP(
              a_it->second.slot_ty, a_it->second.alloca, gep_idx);
          llvm::Value* bp = builder->CreateInBoundsGEP(
              b_it->second.slot_ty, b_it->second.alloca, gep_idx);
          llvm::Value* dp = builder->CreateInBoundsGEP(
              d_it->second.slot_ty, d_it->second.alloca, gep_idx);
          llvm::Value* av =
              ins.array_broadcast_lhs_len1 ? a_broadcast : builder->CreateLoad(f64, ap);
          llvm::Value* bv =
              ins.array_broadcast_rhs_len1 ? b_broadcast : builder->CreateLoad(f64, bp);
          llvm::Value* rv = emit_fbinop(ins.bin_op, av, bv);
          builder->CreateStore(rv, dp);
        }
        return true;
      }
      case MirOp::ArrayBinOpI64: {
        auto d_it = arrays.find(ins.ident);
        auto a_it = arrays.find(ins.lhs_ident);
        auto b_it = arrays.find(ins.rhs_ident);
        if (d_it == arrays.end() || a_it == arrays.end() || b_it == arrays.end()) {
          return true;
        }
        const auto n = static_cast<unsigned>(ins.int_value);
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        llvm::Value* b_broadcast = nullptr;
        if (ins.array_broadcast_rhs_len1) {
          llvm::Value* gep0[] = {zero, zero};
          llvm::Value* bp0 = builder->CreateInBoundsGEP(
              b_it->second.slot_ty, b_it->second.alloca, gep0);
          b_broadcast = builder->CreateLoad(i32_ty(context), bp0);
        }
        llvm::Value* a_broadcast = nullptr;
        if (ins.array_broadcast_lhs_len1) {
          llvm::Value* gep0[] = {zero, zero};
          llvm::Value* ap0 = builder->CreateInBoundsGEP(
              a_it->second.slot_ty, a_it->second.alloca, gep0);
          a_broadcast = builder->CreateLoad(i32_ty(context), ap0);
        }
        for (unsigned i = 0; i < n; ++i) {
          llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), i);
          llvm::Value* gep_idx[] = {zero, idx};
          llvm::Value* ap = builder->CreateInBoundsGEP(
              a_it->second.slot_ty, a_it->second.alloca, gep_idx);
          llvm::Value* bp = builder->CreateInBoundsGEP(
              b_it->second.slot_ty, b_it->second.alloca, gep_idx);
          llvm::Value* dp = builder->CreateInBoundsGEP(
              d_it->second.slot_ty, d_it->second.alloca, gep_idx);
          llvm::Value* av = ins.array_broadcast_lhs_len1
                                ? a_broadcast
                                : builder->CreateLoad(i32_ty(context), ap);
          llvm::Value* bv = ins.array_broadcast_rhs_len1
                                ? b_broadcast
                                : builder->CreateLoad(i32_ty(context), bp);
          llvm::Value* rv = emit_binop(ins.bin_op, av, bv);
          builder->CreateStore(rv, dp);
        }
        return true;
      }
      case MirOp::ArrayScaleF64: {
        auto d_it = arrays.find(ins.ident);
        auto a_it = arrays.find(ins.lhs_ident);
        if (d_it == arrays.end() || a_it == arrays.end()) {
          return true;
        }
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Value* scale_v = ins.rhs_is_literal
                                 ? llvm::ConstantFP::get(f64, ins.float_value)
                                 : load_float(ins.rhs_ident);
        const auto n = static_cast<unsigned>(ins.int_value);
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        for (unsigned i = 0; i < n; ++i) {
          llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), i);
          llvm::Value* gep_idx[] = {zero, idx};
          llvm::Value* ap = builder->CreateInBoundsGEP(
              a_it->second.slot_ty, a_it->second.alloca, gep_idx);
          llvm::Value* dp = builder->CreateInBoundsGEP(
              d_it->second.slot_ty, d_it->second.alloca, gep_idx);
          llvm::Value* av = builder->CreateLoad(f64, ap);
          llvm::Value* rv = builder->CreateFMul(av, scale_v);
          builder->CreateStore(rv, dp);
        }
        return true;
      }
      case MirOp::ArrayAxpyF64: {
        auto x_it = arrays.find(ins.lhs_ident);
        auto y_it = arrays.find(ins.rhs_ident);
        if (x_it == arrays.end() || y_it == arrays.end()) {
          return true;
        }
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Value* scale_v = ins.rhs_is_literal
                                 ? llvm::ConstantFP::get(f64, ins.float_value)
                                 : load_float(ins.ident);
        const auto n = static_cast<unsigned>(ins.int_value);
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        for (unsigned i = 0; i < n; ++i) {
          llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), i);
          llvm::Value* gep_idx[] = {zero, idx};
          llvm::Value* xp = builder->CreateInBoundsGEP(
              x_it->second.slot_ty, x_it->second.alloca, gep_idx);
          llvm::Value* yp = builder->CreateInBoundsGEP(
              y_it->second.slot_ty, y_it->second.alloca, gep_idx);
          llvm::Value* xv = builder->CreateLoad(f64, xp);
          llvm::Value* yv = builder->CreateLoad(f64, yp);
          llvm::Value* rv = builder->CreateFAdd(builder->CreateFMul(xv, scale_v), yv);
          builder->CreateStore(rv, yp);
        }
        return true;
      }
      case MirOp::ArrayLoad2DF64: {
        auto it = arrays.find(ins.ident);
        if (it == arrays.end() || !it->second.is_matrix) {
          return true;
        }
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Value* row = ins.index_is_literal
                               ? int32_val(*builder, context, ins.int_value)
                               : load_int(ins.index_ident);
        llvm::Value* col = ins.rhs_is_literal
                               ? int32_val(*builder, context, ins.rhs_int)
                               : load_int(ins.rhs_ident);
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        llvm::Value* gep_idx[] = {zero, row, col};
        llvm::Value* ptr = builder->CreateInBoundsGEP(
            it->second.slot_ty, it->second.alloca, gep_idx);
        llvm::Value* loaded = builder->CreateLoad(f64, ptr);
        if (!ins.lhs_ident.empty()) {
          builder->CreateStore(loaded, ensure_float_local(ins.lhs_ident));
        }
        return true;
      }
      case MirOp::ArrayStore2DF64: {
        auto it = arrays.find(ins.ident);
        if (it == arrays.end() || !it->second.is_matrix) {
          return true;
        }
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Value* row = ins.index_is_literal
                               ? int32_val(*builder, context, ins.int_value)
                               : load_int(ins.index_ident);
        llvm::Value* col = ins.rhs_is_literal
                               ? int32_val(*builder, context, ins.rhs_int)
                               : load_int(ins.rhs_ident);
        llvm::Value* val =
            ins.lhs_is_literal
                ? llvm::ConstantFP::get(f64, ins.float_value)
                : load_float(ins.lhs_ident);
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        llvm::Value* gep_idx[] = {zero, row, col};
        llvm::Value* ptr = builder->CreateInBoundsGEP(
            it->second.slot_ty, it->second.alloca, gep_idx);
        builder->CreateStore(val, ptr);
        return true;
      }
      case MirOp::ArrayMatMul2DF64: {
        auto c_it = arrays.find(ins.ident);
        auto a_it = arrays.find(ins.lhs_ident);
        auto b_it = arrays.find(ins.rhs_ident);
        if (c_it == arrays.end() || a_it == arrays.end() || b_it == arrays.end()) {
          return true;
        }
        const unsigned m = static_cast<unsigned>(ins.int_value);
        const unsigned k = static_cast<unsigned>(ins.rhs_int);
        const unsigned n = static_cast<unsigned>(ins.lhs_int);
        constexpr unsigned kUnrollMax = 24;
        const bool use_loops = m > kUnrollMax || k > kUnrollMax || n > kUnrollMax ||
                               static_cast<std::uint64_t>(m) * k * n > 4096;
        if (use_loops) {
          // Matmul is float-matrix only and never by-ref, so the slots are
          // real allocas (safe to narrow from the generic Value base).
          emit_matmul2d_ijk_loops(static_cast<llvm::AllocaInst*>(c_it->second.alloca),
                                  static_cast<llvm::AllocaInst*>(a_it->second.alloca),
                                  static_cast<llvm::AllocaInst*>(b_it->second.alloca), m, k, n);
        } else {
          emit_matmul2d_ijk_unrolled(static_cast<llvm::AllocaInst*>(c_it->second.alloca),
                                     static_cast<llvm::AllocaInst*>(a_it->second.alloca),
                                     static_cast<llvm::AllocaInst*>(b_it->second.alloca), m, k, n);
        }
        return true;
      }
      case MirOp::ArrayDotF64: {
        auto a_it = arrays.find(ins.lhs_ident);
        auto b_it = arrays.find(ins.rhs_ident);
        if (a_it == arrays.end() || b_it == arrays.end()) {
          return true;
        }
        llvm::Type* f64 = llvm::Type::getDoubleTy(context);
        llvm::Value* acc = llvm::ConstantFP::get(f64, 0.0);
        const auto n = static_cast<unsigned>(ins.int_value);
        const unsigned simd_end = array_simd_enabled() ? (n / 4) * 4 : 0;
        if (simd_end > 0) {
          llvm::Value* v_acc =
              builder->CreateVectorSplat(4, llvm::ConstantFP::get(f64, 0.0));
          for (unsigned i = 0; i < simd_end; i += 4) {
            llvm::Value* av = gather_array_f64x4(a_it->second.alloca, a_it->second.slot_ty, i);
            llvm::Value* bv = gather_array_f64x4(b_it->second.alloca, b_it->second.slot_ty, i);
            v_acc = builder->CreateFAdd(v_acc, builder->CreateFMul(av, bv));
          }
          acc = horiz_sum_f64x4(v_acc);
        }
        llvm::Value* zero = llvm::ConstantInt::get(builder->getInt32Ty(), 0);
        for (unsigned i = simd_end; i < n; ++i) {
          llvm::Value* idx = llvm::ConstantInt::get(i32_ty(context), i);
          llvm::Value* gep_idx[] = {zero, idx};
          llvm::Value* ap = builder->CreateInBoundsGEP(
              a_it->second.slot_ty, a_it->second.alloca, gep_idx);
          llvm::Value* bp = builder->CreateInBoundsGEP(
              b_it->second.slot_ty, b_it->second.alloca, gep_idx);
          llvm::Value* av = builder->CreateLoad(f64, ap);
          llvm::Value* bv = builder->CreateLoad(f64, bp);
          acc = builder->CreateFAdd(acc, builder->CreateFMul(av, bv));
        }
        builder->CreateStore(acc, ensure_float_local(ins.ident));
        return true;
      }
      case MirOp::LoadIntToIdent:
        return true;
      case MirOp::AsyncAwait: {
        llvm::Function* await_fn = module->getFunction("li_async_await_i32");
        llvm::Value* pending = load_int(ins.lhs_ident);
        llvm::Value* result = builder->CreateCall(await_fn, {pending});
        builder->CreateStore(result, ensure_int_local(ins.ident));
        return true;
      }
      case MirOp::AsyncFrameEnter: {
        llvm::Function* enter = module->getFunction("li_async_frame_enter");
        builder->CreateCall(enter, {});
        return true;
      }
      case MirOp::AsyncFrameLeave: {
        llvm::Function* leave = module->getFunction("li_async_frame_leave");
        builder->CreateCall(leave, {});
        return true;
      }
      case MirOp::ArraySimdScope:
        if (ins.int_value != 0) {
          array_simd_scope_stack.push_back(true);
        } else if (!array_simd_scope_stack.empty()) {
          array_simd_scope_stack.pop_back();
        }
        return true;
    }
    return true;
  }
};

}  // namespace

bool emit_llvm_ir(const MirModule& mir, const std::string& out_path, int runtime_threads,
                  std::string* error) {
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
  module->getOrInsertFunction("li_rt_sqrt",
                              llvm::FunctionType::get(llvm::Type::getDoubleTy(context),
                                                      {llvm::Type::getDoubleTy(context)}, false));
  llvm::Type* f64 = llvm::Type::getDoubleTy(context);
  module->getOrInsertFunction(
      "li_rt_hypot", llvm::FunctionType::get(f64, {f64, f64}, false));
  module->getOrInsertFunction("li_rt_expm1", llvm::FunctionType::get(f64, {f64}, false));
  module->getOrInsertFunction("li_rt_log1p", llvm::FunctionType::get(f64, {f64}, false));
  module->getOrInsertFunction(
      "li_rt_volatile_sink_f64",
      llvm::FunctionType::get(llvm::Type::getVoidTy(context), {f64}, false));
  module->getOrInsertFunction("li_rt_print_f64",
                              llvm::FunctionType::get(llvm::Type::getVoidTy(context), {f64},
                                                      false));
  module->getOrInsertFunction(
      "li_parallel_for_i64",
      llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                              {i64_ty(context), i64_ty(context),
                               llvm::PointerType::getUnqual(llvm::FunctionType::get(
                                   llvm::Type::getVoidTy(context), {i64_ty(context)}, false)),
                               i32_ty(context)},
                              false));
  module->getOrInsertFunction("li_async_frame_enter",
                              llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false));
  module->getOrInsertFunction("li_async_frame_leave",
                              llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false));
  module->getOrInsertFunction("li_async_await_i32",
                              llvm::FunctionType::get(i32_ty(context), {i32_ty(context)}, false));
  module->getOrInsertFunction("li_async_poll",
                              llvm::FunctionType::get(i32_ty(context), {i32_ty(context)}, false));

  module->getOrInsertFunction("bytes_len",
                              llvm::FunctionType::get(i32_ty(context), {i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "bytes_slice",
      llvm::FunctionType::get(i8_ptr(context), {i8_ptr(context), i32_ty(context), i32_ty(context)},
                              false));
  module->getOrInsertFunction(
      "li_rt_str_byte_at",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context), i32_ty(context)}, false));
  module->getOrInsertFunction(
      "li_rt_str_prefix_is_get",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "li_rt_http_parse_request_len_tag",
      llvm::FunctionType::get(i32_ty(context),
                              {i8_ptr(context), i32_ty(context), i32_ty(context)},
                              false));
  module->getOrInsertFunction(
      "li_rt_str_eq",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context), i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "li_rt_studio_profile_from_name",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "li_rt_studio_parse_toml_profile_line",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context)}, false));
  module->getOrInsertFunction("li_rt_lig_device_kind",
                              llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction("li_rt_lig_backend_available",
                              llvm::FunctionType::get(i32_ty(context), {i32_ty(context)}, false));
  module->getOrInsertFunction("li_rt_lig_backend_select_auto",
                              llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction("li_rt_lig_capability_json",
                              llvm::FunctionType::get(i8_ptr(context), {}, false));
  module->getOrInsertFunction("li_rt_lig_parse_toml_backend_line",
                              llvm::FunctionType::get(i32_ty(context), {i8_ptr(context)}, false));
  module->getOrInsertFunction("li_rt_lig_present_surface_ok",
                              llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_world_format_version",
      llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_world_serialize_slot",
      llvm::FunctionType::get(i8_ptr(context),
                              {i32_ty(context), i32_ty(context), i32_ty(context)},
                              false));
  module->getOrInsertFunction(
      "li_rt_world_parse_line",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "li_rt_world_parsed_name_slot",
      llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_world_parsed_tick",
      llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_world_parsed_entity_count",
      llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_world_snapshot_eq_fields",
      llvm::FunctionType::get(i32_ty(context),
                              {i32_ty(context), i32_ty(context), i32_ty(context), i32_ty(context),
                               i32_ty(context), i32_ty(context)},
                              false));
  module->getOrInsertFunction(
      "li_rt_world_roundtrip_fields",
      llvm::FunctionType::get(i32_ty(context),
                              {i32_ty(context), i32_ty(context), i32_ty(context)},
                              false));
  module->getOrInsertFunction(
      "li_rt_studio_timeline_playing", llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_studio_timeline_toggle_play", llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_studio_timeline_tick_frame", llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_studio_timeline_playhead_pct",
      llvm::FunctionType::get(llvm::Type::getDoubleTy(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_studio_timeline_reset_mock", llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_studio_viewport_error_kind", llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction("li_rt_studio_viewport_error_set_mock",
                              llvm::FunctionType::get(i32_ty(context), {i32_ty(context)}, false));
  module->getOrInsertFunction(
      "li_rt_studio_viewport_error_retry", llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_studio_mcp_tool_from_name",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "li_rt_studio_mcp_tool_name",
      llvm::FunctionType::get(i8_ptr(context), {i32_ty(context)}, false));
  module->getOrInsertFunction("li_rt_lig_device_kind",
                              llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction("li_rt_lig_backend_available",
                              llvm::FunctionType::get(i32_ty(context), {i32_ty(context)}, false));
  module->getOrInsertFunction("li_rt_lig_backend_select_auto",
                              llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction("li_rt_lig_capability_json",
                              llvm::FunctionType::get(i8_ptr(context), {}, false));
  module->getOrInsertFunction("li_rt_lig_parse_toml_backend_line",
                              llvm::FunctionType::get(i32_ty(context), {i8_ptr(context)}, false));
  module->getOrInsertFunction("li_rt_lig_present_surface_ok",
                              llvm::FunctionType::get(i32_ty(context), {}, false));
  module->getOrInsertFunction(
      "li_rt_path_exact",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context), i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "li_rt_path_prefix",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context), i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "li_rt_match_route_fixture",
      llvm::FunctionType::get(i32_ty(context), {i8_ptr(context), i8_ptr(context)}, false));
  module->getOrInsertFunction("tcp_listen",
                              llvm::FunctionType::get(i32_ty(context), {i32_ty(context)}, false));
  module->getOrInsertFunction("tcp_accept",
                              llvm::FunctionType::get(i32_ty(context), {i32_ty(context)}, false));
  module->getOrInsertFunction(
      "tcp_send",
      llvm::FunctionType::get(i32_ty(context), {i32_ty(context), i8_ptr(context)}, false));
  module->getOrInsertFunction(
      "tcp_recv",
      llvm::FunctionType::get(i8_ptr(context), {i32_ty(context), i32_ty(context)}, false));
  module->getOrInsertFunction("tcp_close",
                              llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                                      {i32_ty(context)}, false));

  llvm::Function* user_main = nullptr;
  bool user_main_argv_wrapper = false;

  struct UserFnEmit {
    const MirFn* mir_fn = nullptr;
    llvm::Function* llvm_fn = nullptr;
    llvm::Type* ret_ty = nullptr;
    bool is_par_fn = false;
  };
  std::vector<UserFnEmit> user_fns;

  // Pass 1: declare every MIR function before any body references callees.
  for (const auto& fn : mir.functions) {
    if (fn.is_extern) {
      llvm::Type* ret_ty = fn.returns_void ? llvm::Type::getVoidTy(context)
                                           : (fn.returns_i64 ? i8_ptr(context)
                                                             : llvm_scalar(context, fn.returns_float, false));
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

    const bool is_par_fn = fn.name.rfind("__li_par_", 0) == 0;
    llvm::Type* ret_ty = nullptr;
    if (fn.returns_void) {
      ret_ty = llvm::Type::getVoidTy(context);
    } else if (fn.returns_object && !fn.return_object_layout.empty()) {
      ret_ty = llvm_struct_from_layout(context, fn.return_object_layout);
    } else if (fn.returns_i64) {
      ret_ty = i8_ptr(context);
    } else {
      ret_ty = llvm_scalar(context, fn.returns_float, false);
    }
    std::vector<llvm::Type*> param_tys;
    for (const auto& p : fn.params) {
      if (is_par_fn) {
        param_tys.push_back(i64_ty(context));
      } else if (p.is_simd_f64) {
        param_tys.push_back(llvm::VectorType::get(llvm::Type::getDoubleTy(context),
                                                  llvm::ElementCount::getFixed(4)));
      } else if (p.fixed_array_elems > 0) {
        param_tys.push_back(llvm_array_param_ptr(context, p));
      } else {
        param_tys.push_back(llvm_type_for_mir_param(context, p));
      }
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
    user_fns.push_back(UserFnEmit{&fn, func, ret_ty, is_par_fn});
  }

  // Pass 2: emit bodies (all callees already declared).
  for (const UserFnEmit& ufe : user_fns) {
    const MirFn& fn = *ufe.mir_fn;
    llvm::Function* func = ufe.llvm_fn;
    llvm::Type* ret_ty = ufe.ret_ty;
    const bool is_par_fn = ufe.is_par_fn;

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    llvm::IRBuilder<> builder(entry);
    llvm::FastMathFlags saved_fmf = builder.getFastMathFlags();
    if (mir.fp_numerically_stable) {
      builder.setFastMathFlags(llvm::FastMathFlags());
    } else {
      llvm::FastMathFlags fmf;
      fmf.setFast();
      fmf.setAllowContract(true);
      fmf.setAllowReassoc(true);
      builder.setFastMathFlags(fmf);
    }

    EmitCtx ctx{context,
                module.get(),
                func,
                &builder,
                ret_ty,
                fn.returns_float,
                fn.returns_i64,
                fn.returns_object,
                mir.fp_numerically_stable,
                runtime_threads,
                !fn.no_vectorize,
                {},
                {},
                {},
                {},
                {},
                {}};

    unsigned idx = 0;
    for (auto& arg : func->args()) {
      if (idx < fn.params.size()) {
        arg.setName(fn.params[idx].name);
        const auto& mp = fn.params[idx];
        if (mp.fixed_array_elems > 0) {
          llvm::Type* arr_ty = llvm_array_type(context, mp);
          if (mp.is_var) {
            // `var array` params are by-reference: callee writes propagate to
            // the caller's array. The ABI already passes a pointer; use it
            // directly as the slot base instead of copying in.
            ctx.arrays[mp.name] = ArraySlot{&arg, arr_ty, mp.fixed_array_elems, mp.matrix_cols,
                                            mp.is_float, mp.is_matrix, mp.is_i64};
          } else {
          llvm::AllocaInst* ap = builder.CreateAlloca(arr_ty, nullptr, mp.name);
          ctx.arrays[mp.name] = ArraySlot{ap, arr_ty, mp.fixed_array_elems, mp.matrix_cols,
                                          mp.is_float, mp.is_matrix, mp.is_i64};
          llvm::Type* elem = llvm_scalar(context, mp.is_float, mp.is_i64, mp.is_string);
          llvm::Value* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
          if (mp.is_matrix && mp.matrix_cols > 0) {
            const unsigned m = static_cast<unsigned>(mp.fixed_array_elems);
            const unsigned n = static_cast<unsigned>(mp.matrix_cols);
            for (unsigned i = 0; i < m; ++i) {
              llvm::Value* ri = llvm::ConstantInt::get(i32_ty(context), i);
              for (unsigned j = 0; j < n; ++j) {
                llvm::Value* rj = llvm::ConstantInt::get(i32_ty(context), j);
                llvm::Value* gep_idx[] = {zero, ri, rj};
                llvm::Value* from_p = builder.CreateInBoundsGEP(arr_ty, &arg, gep_idx);
                llvm::Value* to_p = builder.CreateInBoundsGEP(arr_ty, ap, gep_idx);
                llvm::Value* v = builder.CreateLoad(elem, from_p);
                builder.CreateStore(v, to_p);
              }
            }
          } else {
            const auto len = static_cast<unsigned>(mp.fixed_array_elems);
            for (unsigned i = 0; i < len; ++i) {
              llvm::Value* ai = llvm::ConstantInt::get(i32_ty(context), i);
              llvm::Value* gep_idx[] = {zero, ai};
              llvm::Value* from_p = builder.CreateInBoundsGEP(arr_ty, &arg, gep_idx);
              llvm::Value* to_p = builder.CreateInBoundsGEP(arr_ty, ap, gep_idx);
              llvm::Value* v = builder.CreateLoad(elem, from_p);
              builder.CreateStore(v, to_p);
            }
          }
          }
        } else if (mp.is_var) {
          // `var` object leaf param: keep the incoming pointer; loads and
          // stores on this slot go through it so mutations propagate back.
          ctx.var_ref_params[mp.name] = &arg;
          ctx.var_ref_elem_ty[mp.name] =
              llvm_scalar(context, mp.is_float, mp.is_i64, mp.is_string);
        } else if (mp.is_string) {
          builder.CreateStore(&arg, ctx.ensure_ptr_local(mp.name));
        } else if (is_par_fn || mp.is_i64) {
          builder.CreateStore(&arg, ctx.ensure_i64_local(mp.name));
        } else if (mp.is_float) {
          builder.CreateStore(&arg, ctx.ensure_float_local(mp.name));
        } else {
          builder.CreateStore(&arg, ctx.ensure_int_local(mp.name));
        }
      }
      idx++;
    }

    for (const auto& ins : fn.body) {
      ctx.emit_insn(ins);
    }
    builder.setFastMathFlags(saved_fmf);
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
