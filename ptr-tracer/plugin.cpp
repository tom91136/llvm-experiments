#include "llvm/Analysis/LazyCallGraph.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/AbstractCallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include <cxxabi.h>
#include <filesystem>
#include <unordered_set>

#include "../plugin_utils.h"

#ifdef __APPLE__
  #include <crt_externs.h>
#endif

using namespace llvm;

namespace {

template <typename Ret, typename Arg, typename... Rest> Arg arg0_helper(Ret (*)(Arg, Rest...));
template <typename Ret, typename F, typename Arg, typename... Rest> Arg arg0_helper(Ret (F::*)(Arg, Rest...));
template <typename Ret, typename F, typename Arg, typename... Rest> Arg arg0_helper(Ret (F::*)(Arg, Rest...) const);
template <typename F> decltype(arg0_helper(&F::operator())) arg0_helper(F);
template <typename T> using arg0_t = decltype(arg0_helper(std::declval<T>()));
template <typename T, typename Node, typename... Fs> std::optional<T> visitDyn(Node n, Fs... fs) {
  std::optional<T> result{};
  [[maybe_unused]] auto _ = {[&]() {
    if (!result) {
      if (auto x = llvm::dyn_cast<std::remove_pointer_t<arg0_t<Fs>>>(n)) {
        result = T(fs(x));
      }
    }
    return 0;
  }()...};
  return result;
}
template <typename Node, typename... Fs> void visitDyn0(Node n, Fs... fs) {
  [[maybe_unused]] auto _ = ([&]() -> bool {
    if (auto x = llvm::dyn_cast<std::remove_pointer_t<arg0_t<Fs>>>(n)) {
      fs(x);
      return true;
    }
    return false;
  }() || ...);
}

class tee_ostream : public raw_ostream {
private:
  raw_ostream &out;
  std::unique_ptr<raw_fd_ostream> file;

  uint64_t current_pos() const override { return 0; }

  void write_impl(const char *Ptr, size_t Size) override {
    out.write(Ptr, Size);
    if (file) file->write(Ptr, Size);
  }

public:
  tee_ostream(raw_ostream &out, const std::optional<std::string> &path) : raw_ostream(true), out(out) {
    std::error_code EC;
    if (path) {
      file = std::make_unique<raw_fd_ostream>(*path, EC, sys::fs::OF_Text);
      if (EC) {
        errs() << "Error: Unable to open file " << *path << ": " << EC.message() << "\n";
      }
    }
  }
};

size_t traceValueOrigin(llvm::Value *V, std::vector<llvm::Value *> &Results, std::unordered_set<Value *> &Visited, size_t depth = 0) {
  auto traceSingle0 = [&](auto V) -> size_t { return traceValueOrigin(V, Results, Visited, depth + 1); };
  auto traceSingle = [&](auto V) -> void { depth = std::max(depth, traceValueOrigin(V, Results, Visited, depth + 1)); };
  auto traceFn = [&](CallBase *CB) -> size_t {
    auto F = CB->getCalledFunction();
    if (!F) return traceSingle0(CB);
    if (F->isDeclaration()) return traceSingle0(F);
    for (auto &BB : *F) {
      for (auto &Inst : BB) {
        if (auto R = dyn_cast<llvm::ReturnInst>(&Inst)) {
          if (auto RV = R->getReturnValue()) traceSingle(RV);
        }
      }
    }
    return depth;
  };

  auto Root = getUnderlyingObject(V, 0);

  if (auto [_, inserted] = Visited.emplace(Root); !inserted) {
    // Results.emplace_back(Root);
    return depth; // give up, recursive chain?
  }

  // if (depth > 100) {
  //   Results.emplace_back(Root);
  //   return depth; // give up, recursive chain?
  // }

  // Results.emplace_back(Root);

  auto handled = visitDyn<size_t>(
      Root,                                      //
      [&](CallInst *C) { return traceFn(C); },   // Trace all returns
      [&](InvokeInst *I) { return traceFn(I); }, // Trace all returns
      [&](Argument *A) {
        auto F = A->getParent();
        for (auto &U : F->uses())
          if (auto ACS = AbstractCallSite(&U)) traceSingle(ACS.getCallArgOperand(A->getArgNo()));
        return depth;
      },
      [&](LoadInst *L) { return traceSingle0(L->getPointerOperand()); },
      [&](GetElementPtrInst *GEP) { return traceSingle0(GEP->getPointerOperand()); }, // +Offset
      [&](ExtractValueInst *EV) { return traceSingle0(EV->getAggregateOperand()); },  // FIXME may be incorrect
      [&](InsertValueInst *IV) { return traceSingle0(IV->getAggregateOperand()); },   // FIXME may be incorrect
      [&](IntToPtrInst *PTI) { return traceSingle0(PTI->getOperand(0)); },
      [&](PHINode *PHI) { // Union of offsets
        for (auto &U : PHI->incoming_values())
          traceSingle(U.get());
        return depth;
      },
      [&](SelectInst *S) { // Union of offsets
        traceSingle(S->getTrueValue());
        traceSingle(S->getFalseValue());
        return depth;
      });
  if (handled) return *handled;
  else {
    Results.emplace_back(Root);
    return depth;
  }
}

const static std::unordered_map<std::string, std::string> AllocFunctions{
    {"aligned_alloc", "aligned_alloc"},
    {"calloc", "calloc"},
    {"free", "free"},
    {"malloc", "malloc"},
    {"memalign", "aligned_alloc"},
    {"posix_memalign", "posix_aligned_alloc"},
    {"realloc", "realloc"},
    {"reallocarray", "realloc_array"},
    {"_ZdaPv", "operator_delete"},
    {"_ZdaPvm", "operator_delete_sized"},
    {"_ZdaPvSt11align_val_t", "operator_delete_aligned"},
    {"_ZdaPvmSt11align_val_t", "operator_delete_aligned_sized"},
    {"_ZdlPv", "operator_delete"},
    {"_ZdlPvm", "operator_delete_sized"},
    {"_ZdlPvSt11align_val_t", "operator_delete_aligned"},
    {"_ZdlPvmSt11align_val_t", "operator_delete_aligned_sized"},
    {"_Znam", "operator_new"},
    {"_ZnamRKSt9nothrow_t", "operator_new_nothrow"},
    {"_ZnamSt11align_val_t", "operator_new_aligned"},
    {"_ZnamSt11align_val_tRKSt9nothrow_t", "operator_new_aligned_nothrow"},

    {"_Znwm", "operator_new"},
    {"_ZnwmRKSt9nothrow_t", "operator_new_nothrow"},
    {"_ZnwmSt11align_val_t", "operator_new_aligned"},
    {"_ZnwmSt11align_val_tRKSt9nothrow_t", "operator_new_aligned_nothrow"},
    {"__builtin_calloc", "calloc"},
    {"__builtin_free", "free"},
    {"__builtin_malloc", "malloc"},
    {"__builtin_operator_delete", "operator_delete"},
    {"__builtin_operator_new", "operator_new"},
    {"__builtin_realloc", "realloc"},
    {"__libc_calloc", "calloc"},
    {"__libc_free", "free"},
    {"__libc_malloc", "malloc"},
    {"__libc_memalign", "aligned_alloc"},
    {"__libc_realloc", "realloc"}};

static std::optional<std::string> demangleCXXName(const char *abiName) {
  int failed;
  char *ret = abi::__cxa_demangle(abiName, nullptr /* output buffer */, nullptr /* length */, &failed);
  if (failed) {
    // 0: The demangling operation succeeded.
    // -1: A memory allocation failure occurred.
    // -2: mangled_name is not a valid name under the C++ ABI mangling rules.
    // -3: One of the arguments is invalid.
    return {};
  } else {
    return ret;
  }
}

enum class Kind { Stack, Heap, StdCall, LandingPad, Global, Constant, Null };

bool isAllocating(Value *v) {
  auto handleFn = [&](Function *F) {
    auto Name = F->getName();
    if (AllocFunctions.find(Name.str()) != AllocFunctions.end()) return true;
    if (auto CXXName = demangleCXXName(Name.data())) {
      return CXXName->rfind("std::", 0) == 0;
    }
    return false;
  };
  auto handled = visitDyn<bool>(
      v,                                           //
      [&](PoisonValue *) { return true; },         //
      [&](ConstantPointerNull *) { return true; }, //
      [&](GlobalVariable *) { return true; },      //
      [&](ConstantDataArray *) { return true; },   //
      [&](AllocaInst *) { return true; },          //
      [&](LandingPadInst *) { return true; },      //
      [&](Function *) { return true; },            //
      [&](CallInst *Call) {
        if (auto F = Call->getCalledFunction()) {
          return handleFn(F);
        }
        return false;
      },
      [&](llvm::InvokeInst *Invoke) {
        if (auto F = Invoke->getCalledFunction()) {
          return handleFn(F);
        }
        return false;
      });
  return handled ? *handled : false;
}

bool runPtrTracer(Module &M, const std::string &ResultFile) {

  // M.print(llvm::errs(), nullptr);

  tee_ostream out(nulls(), ResultFile);

  out << "module:\n";
  out << "  name: " << M.getName() << "\n";
  out << "  functions: \n";
  for (Function &F : M) {
    out << "  - " << F.getName() << ":\n";
    out << "    calls: \n";

    auto report = [&](CallBase *CI) {
      out << "    - instruction: '" << *CI << "'\n";
      out << "      args: \n";

      for (size_t i = 0; i < CI->arg_size(); i++) {
        auto V = CI->getArgOperandUse(i).get();
        if (!V->getType()->isPointerTy()) continue;
        std::vector<Value *> Results;
        std::unordered_set<Value *> Visited;
        auto depth = traceValueOrigin(V, Results, Visited);

        size_t nonAllocOrigins = std::count_if(Results.begin(), Results.end(), [](auto &v) { return !isAllocating(v); });

        if (auto RF = dyn_cast<Function>(V)) {
          out << "      - arg: '" << RF->getName() << "'\n";
        } else {
          out << "      - arg: '" << *V << "'\n";
        }

        out << "        maxDepth: " << depth << "\n";
        out << "        indirections: " << Visited.size() << "\n";
        out << "        nonAllocOrigins: " << nonAllocOrigins << "\n";
        out << "        origins:\n";
        for (auto R : Results) {
          if (auto RF = dyn_cast<Function>(R)) {
            out << "        - '" << RF->getName() << "'\n";
          } else if (auto I = dyn_cast<Instruction>(R)) {
            out << "        - '" << (*R) << " (in " << I->getFunction()->getName() << ")'\n";
          } else {
            out << "        - '" << (*R) << "'\n";
          }
        }
      }
    };

    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (auto *CB = dyn_cast<CallBase>(&I)) {
          report(CB);
        }
      }
    }
  }
  out.flush();

  return false;
}

struct PtrTracer : PassInfoMixin<PtrTracer> {
  std::string prefix;
  explicit PtrTracer(const std::string &prefix) : prefix(prefix) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    // LazyCallGraph &LCG = MAM.getResult<LazyCallGraphAnalysis>(M);

    std::string ModuleSuffix;
    ModuleSuffix += std::to_string(M.global_size()) + "g";
    ModuleSuffix += std::to_string(M.size()) + "f";
    ModuleSuffix += std::to_string(M.alias_size()) + "a";
    ModuleSuffix += std::to_string(M.ifunc_size()) + "i";

    std::filesystem::path OutputName = guessOutputName().value_or(M.getSourceFileName());
    auto Output = (OutputName.has_relative_path() ? OutputName.parent_path() : "./") /
                  (prefix + OutputName.filename().string() + "_" + ModuleSuffix + ".yaml");

    if (!runPtrTracer(M, Output)) return PreservedAnalyses::all();
    return PreservedAnalyses::none();
  }
};

} // namespace

llvm::PassPluginLibraryInfo getPtrTracerPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "PtrTracer", LLVM_VERSION_STRING, [](PassBuilder &PB) {
            // PB.registerFullLinkTimeOptimizationEarlyEPCallback(
            //     [](llvm::ModulePassManager &MPM, OptimizationLevel Level) { MPM.addPass(PtrTracer("early_")); });
            PB.registerFullLinkTimeOptimizationLastEPCallback(
                [](llvm::ModulePassManager &MPM, OptimizationLevel) { MPM.addPass(PtrTracer("last_")); });
            // PB.registerPipelineParsingCallback(
            //     [](StringRef Name, llvm::FunctionPassManager &PM, ArrayRef<llvm::PassBuilder::PipelineElement>) {
            //       if (Name == "ptrtracer") {
            //         PM.addPass(Bye());
            //         return true;
            //       }
            //       return false;
            //     });
          }};
}

#ifndef LLVM_BYE_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() { return getPtrTracerPluginInfo(); }
#endif