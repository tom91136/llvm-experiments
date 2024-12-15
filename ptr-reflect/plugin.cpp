#include <filesystem>
#include <unordered_set>

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "../plugin_utils.h"
#include "rt_reflect.hpp"

namespace {

bool runSplice(llvm::Module &M, const std::string &ResultFile) {
  // M.print(llvm::errs(), nullptr);
  tee_ostream out(llvm::nulls(), ResultFile);
  auto &C = M.getContext();
  auto RecordFn = M.getFunction("_rt_record");
  auto ReleaseFn = M.getFunction("_rt_release");
  if (!RecordFn) {
    llvm::errs() << "[RecordStackPass] _rt_record not found, giving up\n";
    return false;
  }
  if (!ReleaseFn) {
    llvm::errs() << "[RecordStackPass] _rt_release not found, giving up\n";
    return false;
  }

  out << "module:\n";
  out << "  name: " << M.getName() << "\n";
  out << "  functions: \n";

  std::unordered_set<llvm::Function *> ProtectedFunctions;
  findFunctionsWithStringAnnotations(M, [&](llvm::Function *F, llvm::StringRef Annotation) {
    if (F && Annotation == "__rt_protect") ProtectedFunctions.emplace(F);
  });

  for (llvm::Function &F : M) {
    if (F.isDeclaration()) continue;
    if (ProtectedFunctions.count(&F) > 0) continue;

    out << "  - " << demangleCXXName(F.getName().data()).value_or(F.getName().data()) << ":\n";
    out << "    calls: \n";

    llvm::DILocation *zeroDebugLoc{};
    if (auto SP = F.getSubprogram()) {
      zeroDebugLoc = llvm::DILocation::get(C, 0, 0, SP);
    }

    for (llvm::BasicBlock &BB : F) {
      std::vector<std::function<void(llvm::IRBuilder<> &)>> Functions;
      for (llvm::Instruction &I : BB) {
        if (auto *CB = llvm::dyn_cast<llvm::CallBase>(&I)) {
          auto F = CB->getCalledFunction();
          if (!F) continue;
          const auto name = F->getName().str();
          const auto log = [&]() {
            out << "    - instruction: '" << *CB << "'\n";
            out << "      name: " << name << "\n";
          };
          if (CB->getIntrinsicID() == llvm::Intrinsic::lifetime_start) {
            log();
            Functions.emplace_back([RecordFn, CB, zeroDebugLoc](llvm::IRBuilder<> &B) {
              B.SetInsertPoint(CB->getNextNode()); // after start
              auto alloc = B.getInt8(ptr_reflect::to_integral(ptr_reflect::_rt_Type::StackAlloc));
              auto Call = B.CreateCall(RecordFn, {CB->getArgOperand(1), CB->getArgOperand(0), alloc});
              if (zeroDebugLoc) Call->setDebugLoc(zeroDebugLoc);
            });
          }
          if (CB->getIntrinsicID() == llvm::Intrinsic::lifetime_end) {
            log();
            Functions.emplace_back([ReleaseFn, CB, zeroDebugLoc](llvm::IRBuilder<> &B) {
              B.SetInsertPoint(CB); // before end
              auto dealloc = B.getInt8(ptr_reflect::to_integral(ptr_reflect::_rt_Type::StackFree));
              auto Call = B.CreateCall(ReleaseFn, {CB->getArgOperand(1), dealloc});
              if (zeroDebugLoc) Call->setDebugLoc(zeroDebugLoc);
            });
          }
        }
      }
      llvm::IRBuilder<> B(&BB);
      for (auto f : Functions)
        f(B);
    }
  }
  out.flush();
  return true;
}

}; // namespace

class RecordStackPass : public llvm::PassInfoMixin<RecordStackPass> {
public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    std::string ModuleSuffix;
    ModuleSuffix += std::to_string(M.global_size()) + "g";
    ModuleSuffix += std::to_string(M.size()) + "f";
    ModuleSuffix += std::to_string(M.alias_size()) + "a";
    ModuleSuffix += std::to_string(M.ifunc_size()) + "i";
    std::filesystem::path OutputName = M.getSourceFileName();
    OutputName = guessOutputName().value_or(OutputName);
    auto Output = (OutputName.has_relative_path() ? OutputName.parent_path() : "./") /
                  (OutputName.filename().string() + "_" + ModuleSuffix + ".yaml");
    if (!runSplice(M, Output)) return llvm::PreservedAnalyses::all();
    return llvm::PreservedAnalyses::none();
  }
};

class ProtectRTPass : public llvm::PassInfoMixin<ProtectRTPass> {
public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {

    findFunctionsWithStringAnnotations(M, [&](llvm::Function *F, llvm::StringRef Annotation) {
      if (F && Annotation == "__rt_protect") F->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);
    });

    return llvm::PreservedAnalyses::none();
  }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ptrreflect", LLVM_VERSION_STRING, [](llvm::PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [&](llvm::ModulePassManager &MPM, llvm::OptimizationLevel Level) { MPM.addPass(ProtectRTPass()); });
            PB.registerOptimizerLastEPCallback(
                [&](llvm::ModulePassManager &MPM, llvm::OptimizationLevel Level) { MPM.addPass(RecordStackPass()); });
            PB.registerFullLinkTimeOptimizationLastEPCallback(
                [&](llvm::ModulePassManager &MPM, llvm::OptimizationLevel) { MPM.addPass(RecordStackPass()); });
          }};
}