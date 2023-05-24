#include "DTSS.h"

#include <unordered_set>
#include <vector>

#include "llvm/ADT/SCCIterator.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace dtss {
llvm::PreservedAnalyses DTSSPass::run(llvm::Function &F,
                                      llvm::FunctionAnalysisManager &AM) {
  std::vector<std::unordered_set<llvm::BasicBlock *>> func_sccs;

  // Put all SCCs within this function into func_sccs
  for (llvm::scc_iterator<llvm::Function *> func_it = scc_begin(&F);
       func_it != scc_end(&F); ++func_it) {
    func_sccs.push_back({});
    // Obtain the BasicBlocks in this SCC
    const std::vector<llvm::BasicBlock *> &scc_bbs = *func_it;
    for (std::vector<llvm::BasicBlock *>::const_iterator bb_it = scc_bbs.begin();
         bb_it != scc_bbs.end(); ++bb_it) {
      func_sccs.back().insert(*bb_it);
    }
  }

  // TODO: Go through SCCs and find the BasicBlock with the right function call

  // TODO: Iterate through the predecessors and find all SCCs on the critical path

  return llvm::PreservedAnalyses::all();
}
} // namespace dtss

llvm::PassPluginLibraryInfo getDTSSPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "DTSS", "0.1", [](llvm::PassBuilder &PB) {
        PB.registerVectorizerStartEPCallback(
            [](llvm::FunctionPassManager &PM, llvm::OptimizationLevel Level) {
              PM.addPass(dtss::DTSSPass());
            });
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

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDTSSPluginInfo();
}
