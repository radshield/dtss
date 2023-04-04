#include "DTSS.h"

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace dtss {
llvm::PreservedAnalyses DTSSPass::run(llvm::Function &F,
                                      llvm::FunctionAnalysisManager &AM) {
  return llvm::PreservedAnalyses::all();
}

llvm::PassPluginLibraryInfo getDTSSPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DTSS", LLVM_VERSION_STRING,
          [](llvm::PassBuilder &PB) {
            PB.registerVectorizerStartEPCallback(
                [](llvm::FunctionPassManager &PM,
                   llvm::OptimizationLevel Level) { PM.addPass(DTSSPass()); });
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &PM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "dtss") {
                    PM.addPass(dtss::DTSSPass());
                    return true;
                  }
                  return false;
                });
          }};
}
} // namespace dtss

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return dtss::getDTSSPluginInfo();
}
