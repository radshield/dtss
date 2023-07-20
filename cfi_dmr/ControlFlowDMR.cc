#include "ControlFlowDMR.h"

#include <stack>
#include <unordered_set>
#include <vector>

#include "llvm/ADT/SCCIterator.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace ControlFlowDMR {
llvm::PreservedAnalyses ControlFlowDMRPass::run(llvm::Function &F,
                                                llvm::FunctionAnalysisManager &AM) {
  std::unordered_set<llvm::Value *> important_values;
  std::unordered_set<llvm::Instruction *>terminator_values;
  std::stack<llvm::Value *> important_values_stack;

  // Go through all BBs and record each terminator value
  for (llvm::Function::iterator bb_it = F.begin(); bb_it != F.end(); ++bb_it) {
    llvm::BasicBlock *bb = &*bb_it;

    terminator_values.insert(bb->getTerminator());
  }

  // Get important values for each terminator value
  for (llvm::Instruction *terminator : terminator_values) {
    important_values.insert(terminator);

    // Add all terminator operands into "important operands list"
    if (auto u_br = dyn_cast<llvm::BranchInst>(terminator)) {
      if (u_br->isConditional())
        important_values.insert(u_br->getCondition());
    } else {
      for (int i = 0; i < terminator->getNumOperands(); i++)
        important_values.insert(terminator->getOperand(i));
    }
  }

  for (llvm::Value *value : important_values)
    important_values_stack.push(value);

  // Go through the uses of each important operand and insert all values into
  // the use-def tree
  while (!important_values_stack.empty()) {
    llvm::Value *important_value = important_values_stack.top();
    important_values_stack.pop();

    if (important_value == nullptr || important_values.contains(important_value))
      continue;
    else
      important_values.insert(important_value);

    // Handle instructions that may be called by previous instructions
    if (auto br_insn = dyn_cast<llvm::BranchInst>(important_value)) {
      // It's a branch instruction, so we add the condition to the list
      if (br_insn->isConditional())
        important_values_stack.push(br_insn->getCondition());
    } else if (auto alloc_insn = dyn_cast<llvm::AllocaInst>(important_value)) {
      // It's an alloc instruction, so we add the size to the list if it isn't
      // a static allocation
      if (!alloc_insn->isStaticAlloca())
        important_values_stack.push(alloc_insn->getArraySize());
    }  else if (auto insn = dyn_cast<llvm::Instruction>(important_value)) {
      // It's another instruction, so we just add the operands to the list
      for (int i = 0; i < insn->getNumOperands(); i++)
        important_values_stack.push(insn->getOperand(i));
    }
  }

  llvm::outs() << "\nimportant_values:\n";
  for (llvm::Value *important_value : important_values) {
    if (auto important_insn = dyn_cast<llvm::Instruction>(important_value))
      llvm::outs() << "  insn $" << important_insn->getName() << ": "
                   << important_value << "\n";
    else
      llvm::outs() << "  const $" << important_value->getValueID() << ": "
                   << important_value << "\n";
  }


  // Go through all BBs and remove non-important values
  for (llvm::Function::iterator bb_it = F.begin(); bb_it != F.end(); ++bb_it) {
    llvm::BasicBlock *bb = &*bb_it;
    std::vector<llvm::Instruction *> insn_list;

    for (llvm::BasicBlock::iterator insn_it = bb->begin(); insn_it != bb->end(); insn_it++) {
      llvm::Instruction *insn = &*insn_it;
      insn_list.push_back(insn);
    }

    for (auto insn : insn_list) {
      if (!important_values.contains(insn))
        insn->eraseFromParent();
    }
  }

  return llvm::PreservedAnalyses::all();
}
} // namespace ControlFlowDMR

llvm::PassPluginLibraryInfo getControlFlowDMRPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "DTSS", "0.1", [](llvm::PassBuilder &PB) {
        PB.registerVectorizerStartEPCallback(
            [](llvm::FunctionPassManager &PM, llvm::OptimizationLevel Level) {
              PM.addPass(ControlFlowDMR::ControlFlowDMRPass());
            });
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
