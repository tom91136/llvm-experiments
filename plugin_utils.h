#pragma once

#include <fstream>
#include <string>
#include <vector>

#include <cxxabi.h>

#ifdef __APPLE__
  #include <crt_externs.h>
#endif

#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

class tee_ostream : public llvm::raw_ostream {
private:
  raw_ostream &out;
  std::unique_ptr<llvm::raw_fd_ostream> file;

  uint64_t current_pos() const override { return 0; }

  void write_impl(const char *Ptr, size_t Size) override {
    out.write(Ptr, Size);
    if (file) file->write(Ptr, Size);
  }

public:
  tee_ostream(raw_ostream &out, const std::optional<std::string> &path) : raw_ostream(true), out(out) {
    std::error_code EC;
    if (path) {
      file = std::make_unique<llvm::raw_fd_ostream>(*path, EC, llvm::sys::fs::OF_Text);
      if (EC) {
        llvm::errs() << "Error: Unable to open file " << *path << ": " << EC.message() << "\n";
      }
    }
  }
};

// XXX Stolen from
// https://github.com/AdaptiveCpp/AdaptiveCpp/blob/061e2d6ffe1084021d99f22ac1f16e28c6dab899/include/hipSYCL/compiler/cbs/IRUtils.hpp#L183
template <class T> T *getValueOneLevel(llvm::Constant *V, unsigned idx = 0) {
  // opaque ptr
  if (auto *R = llvm::dyn_cast<T>(V)) return R;

  // typed ptr -> look through bitcast
  if (V->getNumOperands() == 0) return nullptr;
  return llvm::dyn_cast<T>(V->getOperand(idx));
}

// XXX Stolen from
// https://github.com/AdaptiveCpp/AdaptiveCpp/blob/061e2d6ffe1084021d99f22ac1f16e28c6dab899/include/hipSYCL/compiler/cbs/IRUtils.hpp#L195
template <class Handler> void findFunctionsWithStringAnnotationsWithArg(llvm::Module &M, Handler &&f) {
  for (auto &I : M.globals()) {
    if (I.getName() == "llvm.global.annotations") {
      auto *CA = llvm::dyn_cast<llvm::ConstantArray>(I.getOperand(0));
      for (auto *OI = CA->op_begin(); OI != CA->op_end(); ++OI) {
        if (auto *CS = llvm::dyn_cast<llvm::ConstantStruct>(OI->get()); CS && CS->getNumOperands() >= 2)
          if (auto *F = getValueOneLevel<llvm::Function>(CS->getOperand(0)))
            if (auto *AnnotationGL = getValueOneLevel<llvm::GlobalVariable>(CS->getOperand(1)))
              if (auto *Initializer = llvm::dyn_cast<llvm::ConstantDataArray>(AnnotationGL->getInitializer())) {
                llvm::StringRef Annotation = Initializer->getAsCString();
                f(F, Annotation, CS->getNumOperands() > 3 ? CS->getOperand(4) : nullptr);
              }
      }
    }
  }
}

// XXX Stolen from
// https://github.com/AdaptiveCpp/AdaptiveCpp/blob/061e2d6ffe1084021d99f22ac1f16e28c6dab899/include/hipSYCL/compiler/cbs/IRUtils.hpp#L215
template <class Handler> void findFunctionsWithStringAnnotations(llvm::Module &M, Handler &&f) {
  findFunctionsWithStringAnnotationsWithArg(M, [&f](llvm::Function *F, llvm::StringRef Annotation, llvm::Value *Arg) { f(F, Annotation); });
}

inline std::optional<std::string> demangleCXXName(const char *abiName) {
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

inline std::vector<std::string> getCmdLine() {
#ifdef __linux__
  std::ifstream cmdline("/proc/self/cmdline");
  if (!cmdline) {
    std::fprintf(stderr, "Failed to open /proc/self/cmdline\n");
    std::abort();
  }
  std::vector<std::string> args;
  std::string line;
  while (std::getline(cmdline, line, '\0'))
    args.push_back(line);
  return args;
#elif defined(__APPLE__)
  char ***argvp = _NSGetArgv();
  int *argcp = _NSGetArgc();

  std::vector<std::string> args;
  for (int i = 0; i < *argcp; ++i) {
    args.push_back(argvp[0][i]);
  }
  return args;
#else
  #error "getCmdLine unimplemented for OS"
#endif
}

inline std::optional<std::string> guessOutputName() {
  auto args = getCmdLine();
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "-o" && (i + 1) < args.size()) return args[i + 1];
  }
  return {};
}