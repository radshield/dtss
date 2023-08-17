#include "ControlFlowDMR.h"

#include <stack>
#include <unordered_set>
#include <vector>

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace ControlFlowDMR {
llvm::PreservedAnalyses ControlFlowDMRPass::run(llvm::Function &F,
                                                llvm::FunctionAnalysisManager &AM) {
  std::unordered_set<llvm::Instruction *> important_insns;
  std::unordered_set<llvm::Instruction *> terminator_insns;
  std::stack<llvm::Instruction *> important_insns_stack;

  // Do MemorySSA analysis for future use-def chains
  auto &MSSA = AM.getResult<llvm::MemorySSAAnalysis>(F).getMSSA();
  auto *walker = MSSA.getWalker();

  // Go through all BBs and record each terminator value
  for (auto &bb : F)
    terminator_insns.insert(bb.getTerminator());

  // Get important values for each terminator value
  for (llvm::Instruction *terminator : terminator_insns) {
    important_insns.insert(terminator);

    // Add all terminator users into "important operands list"
    for (llvm::Use &u : terminator->operands()) {
      llvm::Value *v = u.get();

      if (auto v_insn = dyn_cast<llvm::Instruction>(v))
        important_insns_stack.push(v_insn);
    }
  }

  for (llvm::Instruction *insn : important_insns) {
    llvm::outs() << insn;
    important_insns_stack.push(insn);
  }

  // Go through the uses of each important operand and insert all values into
  // the use-def tree
  while (!important_insns_stack.empty()) {
    llvm::Instruction *important_insn = important_insns_stack.top();
    important_insns_stack.pop();

    if (important_insn == nullptr || important_insns.contains(important_insn))
      continue;
    else
      important_insns.insert(important_insn);

    // If this instruction might read or write memory, traverse the MSSA
    auto *important_access = walker->getClobberingMemoryAccess(important_insn);

    llvm::outs() << "a\n";
    if (auto v_insn = dyn_cast<llvm::Instruction>(important_access)) {
      llvm::outs() << "b\n";
      important_insns_stack.push(v_insn);
    }

    // Traverse use-def tree and push in other important values
    for (llvm::Use &u : important_insn->uses()) {
      llvm::Value *v = u.get();

      if (auto v_insn = dyn_cast<llvm::Instruction>(v))
        important_insns_stack.push(v_insn);
    }
  }

  // Go through all BBs and remove non-important values
  for (auto &bb : F) {
    std::vector<llvm::Instruction *> insn_list;

    for (auto &insn: bb)
      insn_list.push_back(&insn);

    for (auto insn: insn_list)
      if (!important_insns.contains(insn))
        insn->eraseFromParent();
  }

  return llvm::PreservedAnalyses::all();
}
} // namespace ControlFlowDMR

llvm::PassPluginLibraryInfo getControlFlowDMRPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "DTSS", "0.1", [](llvm::PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
            [](llvm::StringRef Name, llvm::FunctionPassManager &PM,
               llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
              if (Name == "cfi_dmr") {
                PM.addPass(ControlFlowDMR::ControlFlowDMRPass());
                return true;
              }
              return false;
            });

      }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getControlFlowDMRPassPluginInfo();
}
