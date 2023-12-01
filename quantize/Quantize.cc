#include "Quantize.h"

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

clang::QualType minimizedFloatType;

bool dtss::QuantizePass::VisitVarDecl(clang::VarDecl *TargetVarDecl) {
  if (TargetVarDecl->getType()->isFloatingType()) {
    TargetVarDecl->setType(minimizedFloatType);
  }
  return true;
}

dtss::QuantizePassASTConsumer::QuantizePassASTConsumer(clang::ASTContext *Ctx) : Visitor(Ctx) {}
void dtss::QuantizePassASTConsumer::HandleTranslationUnit(clang::ASTContext &Ctx) {
  clang::ASTContext::GetBuiltinTypeError err;
  minimizedFloatType = Ctx.GetBuiltinType(clang::BuiltinType::Float16, err);

  if (err != clang::ASTContext::GE_None)
    return;

  Visitor.TraverseDecl(Ctx.getTranslationUnitDecl());
}

static clang::FrontendPluginRegistry::Add<QuantizeAction>
    X("quantize", "The dtss QuantizePass plugin");
