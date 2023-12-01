#ifndef CLANG_TRANSFORMS_QUANTIZE_H
#define CLANG_TRANSFORMS_QUANTIZE_H

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"

namespace dtss {
class QuantizePass : public clang::RecursiveASTVisitor<QuantizePass> {
public:
  explicit QuantizePass(clang::ASTContext *Context) : Context(Context) {}
  bool VisitVarDecl(clang::VarDecl *TargetVarDecl);
private:
  clang::ASTContext *Context;
};

class QuantizePassASTConsumer : public clang::ASTConsumer {
public:
  explicit QuantizePassASTConsumer(clang::ASTContext *Ctx);
  void HandleTranslationUnit(clang::ASTContext &Ctx) override;
private:
  QuantizePass Visitor;
};
}

class QuantizeAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &Compiler,
                    llvm::StringRef InFile) override {
    return std::unique_ptr<clang::ASTConsumer>(
        std::make_unique<dtss::QuantizePassASTConsumer>(&Compiler.getASTContext()));
  }
  bool ParseArgs(const clang::CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

#endif
