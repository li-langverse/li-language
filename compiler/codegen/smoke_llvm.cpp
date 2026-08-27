#include "li/smoke_llvm.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>

namespace li {

bool smoke_llvm(std::string* error) {
  llvm::LLVMContext context;
  auto module = std::make_unique<llvm::Module>("smoke", context);

  llvm::FunctionType* fn_ty =
      llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {}, false);
  llvm::Function* main_fn =
      llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, "main",
                             module.get());
  llvm::BasicBlock* entry =
      llvm::BasicBlock::Create(context, "entry", main_fn);
  llvm::IRBuilder<> builder(entry);
  builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));

  std::string verify_err;
  llvm::raw_string_ostream verify_stream(verify_err);
  if (llvm::verifyModule(*module, &verify_stream)) {
    if (error) {
      *error = verify_err;
    }
    return false;
  }
  return true;
}

}  // namespace li
