
#include <cstdlib>
#include <iterator>

#include "clang/AST/APNumericStorage.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Stmt.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

namespace {

template <typename F> class InlineMatchCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
  F f;
  void run(const clang::ast_matchers::MatchFinder::MatchResult &result) override { f(result); }

public:
  explicit InlineMatchCallback(F f) : f(f) {}
};

template <typename F, typename... M> void runMatch(clang::ASTContext &context, F callback, M... matcher) {
  using namespace clang::ast_matchers;
  InlineMatchCallback cb(callback);
  MatchFinder finder;
  (finder.addMatcher(matcher, &cb), ...);
  finder.matchAST(context);
}

static clang::DeclRefExpr *createDeclRef(clang::ASTContext &C, clang::VarDecl *lhs) {
  return clang::DeclRefExpr::Create(C, {}, {}, lhs, false, clang::SourceLocation{}, lhs->getType(), clang::ExprValueKind::VK_LValue);
};

static clang::FieldDecl *findCaptureWithName(clang::CXXRecordDecl *functorDecl, const std::string &name) {
  int64_t fieldIndex = -1;
  for (size_t idx = 0; idx < functorDecl->capture_size(); ++idx) {
    if (functorDecl->getCapture(idx)->getCapturedVar()->getName() == name) {
      fieldIndex = idx;
      break;
    }
  }

  if (fieldIndex == -1) return {};
  auto fieldIt = functorDecl->field_begin();
  std::advance(fieldIt, fieldIndex);
  return *fieldIt;
}

static void spliceGetImpl(clang::ASTContext &C, clang::FunctionDecl *fn, const std::string &name) {
  auto memberType = fn->getTemplateSpecializationArgs()->get(0).getAsType();
  auto functorType = fn->getTemplateSpecializationArgs()->get(1).getAsType();
  if (auto functorDecl = functorType->getAsCXXRecordDecl()) {
    if (!functorDecl->isLambda()) {
      llvm::errs() << "Functor is not a lambda\n";
      return;
    }
    auto field = findCaptureWithName(functorDecl, name);
    if (!field) {
      llvm::errs() << "Cannot find field " << name << " for reflection\n";
      return;
    }
    auto memberExpr = clang::MemberExpr::CreateImplicit( //
        C,
        /*Base*/ createDeclRef(C, fn->getParamDecl(0)),
        /*IsArrow*/ false,
        /*MemberDecl*/ field,
        /*T*/ memberType, // type of the MemberDecl
        /*VK*/ clang::ExprValueKind::VK_LValue,
        /*OK*/ clang::ExprObjectKind::OK_Ordinary);
    auto returnStmt = clang::ReturnStmt::Create(C, /*RL*/ clang::SourceLocation{}, /*E*/ memberExpr, /*NRVOCandidate*/ nullptr);
    fn->setBody(clang::CompoundStmt::Create(C, /*Stmts*/ {returnStmt}, /*FPFeatures*/ {}, /*LB*/ {}, /*RB*/ {}));
  } else {
    llvm::errs() << "Functor is not a synthetic CXXRecord\n";
  }
}

static void spliceSetImpl(clang::Sema &S, clang::ASTContext &C, clang::FunctionDecl *fn, const std::string &name, clang::Expr *value) {
  auto memberType = fn->getTemplateSpecializationArgs()->get(0).getAsType();
  auto functorType = fn->getTemplateSpecializationArgs()->get(1).getAsType();
  if (auto functorDecl = functorType->getAsCXXRecordDecl()) {
    if (!functorDecl->isLambda()) {
      llvm::errs() << "Functor is not a lambda\n";
      return;
    }
    auto field = findCaptureWithName(functorDecl, name);
    if (!field) {
      llvm::errs() << "Cannot find field " << name << " for reflection\n";
      return;
    }
    auto memberExpr = clang::MemberExpr::CreateImplicit( //
        C,
        /*Base*/ createDeclRef(C, fn->getParamDecl(0)),
        /*IsArrow*/ false,
        /*MemberDecl*/ field,
        /*T*/ memberType, // type of the MemberDecl
        /*VK*/ clang::ExprValueKind::VK_LValue,
        /*OK*/ clang::ExprObjectKind::OK_Ordinary);

    auto rhs = S.ImpCastExprToType(value, memberType, clang::CastKind::CK_ArrayToPointerDecay).get();
    auto assignment = clang::BinaryOperator::Create(C, memberExpr, rhs, clang::BinaryOperator::Opcode::BO_Assign, memberType,
                                                    clang::ExprValueKind::VK_LValue, clang::ExprObjectKind::OK_Ordinary, {}, {});
    fn->setBody(clang::CompoundStmt::Create(C, /*Stmts*/ {assignment}, /*FPFeatures*/ {}, /*LB*/ {}, /*RB*/ {}));
    
  } else {
    llvm::errs() << "Functor is not a synthetic CXXRecord\n";
  }
}

class LambdaReflectConsumer : public clang::ASTConsumer {
  clang::CompilerInstance &CI;

public:
  LambdaReflectConsumer(clang::CompilerInstance &CI) : clang::ASTConsumer(), CI(CI) {}

  void HandleTranslationUnit(clang::ASTContext &C) override {
    using namespace clang::ast_matchers;
    runMatch(
        C,
        [&](const MatchFinder::MatchResult &result) {
          { // get
            auto callExpr = result.Nodes.getNodeAs<clang::CallExpr>("lambdaReflectGetCallExpr");
            auto getDecl = result.Nodes.getNodeAs<clang::FunctionDecl>("lambdaReflectGetFunctionDecl");
            if (callExpr && getDecl && getDecl->isTemplateInstantiation()) {

              if (callExpr->getNumArgs() != 2) {
                llvm::errs() << "Unexpected argument count for lambda_reflect::get\n";
                return;
              }

              auto fieldNameArg = callExpr->getArg(1)->IgnoreUnlessSpelledInSource();
              if (auto fieldName = llvm::dyn_cast<clang::StringLiteral>(fieldNameArg)) {
                spliceGetImpl(C, const_cast<clang::FunctionDecl *>(getDecl), fieldName->getString().str());
              } else {
                llvm::errs() << "lambda_reflect: Field name argument to `get` must be an immediate string literal\n";
                return;
              }
            }
          }
          { // set
            auto callExpr = result.Nodes.getNodeAs<clang::CallExpr>("lambdaReflectSetCallExpr");
            auto setDecl = result.Nodes.getNodeAs<clang::FunctionDecl>("lambdaReflectSetFunctionDecl");
            if (callExpr && setDecl && setDecl->isTemplateInstantiation()) {

              if (callExpr->getNumArgs() != 3) {
                llvm::errs() << "Unexpected argument count for lambda_reflect::set\n";
                return;
              }

              auto fieldNameArg = callExpr->getArg(1)->IgnoreUnlessSpelledInSource();
              auto valueArg = callExpr->getArg(2)->IgnoreUnlessSpelledInSource();
              if (auto fieldName = llvm::dyn_cast<clang::StringLiteral>(fieldNameArg)) {
                spliceSetImpl(CI.getSema(), C, const_cast<clang::FunctionDecl *>(setDecl), fieldName->getString().str(),
                              const_cast<clang::Expr *>(valueArg));
              } else {
                llvm::errs() << "lambda_reflect: Field name argument to `set` must be an immediate string literal\n";
                return;
              }
            }
          }
        },
        callExpr(callee(functionDecl(hasName("get"), hasDeclContext(namespaceDecl(hasName("lambda_reflect"))))
                            .bind("lambdaReflectGetFunctionDecl")))
            .bind("lambdaReflectGetCallExpr"),
        callExpr(callee(functionDecl(hasName("set"), hasDeclContext(namespaceDecl(hasName("lambda_reflect"))))
                            .bind("lambdaReflectSetFunctionDecl")))
            .bind("lambdaReflectSetCallExpr"));
  }
};

class LambdaReflectFrontendAction : public clang::PluginASTAction {

protected:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile) override {
    return std::make_unique<LambdaReflectConsumer>(CI);
  }

  bool ParseArgs(const clang::CompilerInstance &CI, const std::vector<std::string> &args) override { return true; }

  ActionType getActionType() override { return PluginASTAction::ActionType::CmdlineBeforeMainAction; }
};
} // namespace

[[maybe_unused]] static clang::FrontendPluginRegistry::Add<LambdaReflectFrontendAction> LambdaReflectClangPlugin("lambda-reflect", "");
