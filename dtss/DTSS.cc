#include "DTSS.h"

#include <stack>
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
  llvm::BasicBlock *terminal_bb = nullptr;
  std::unordered_set<llvm::BasicBlock *> bbs_on_path;
  std::vector<std::unordered_set<llvm::BasicBlock *>> func_sccs;
  std::unordered_set<std::unordered_set<llvm::BasicBlock *> *> visited_sccs;
  std::unordered_set<llvm::Value *> important_values;

  llvm::outs() << "Function: " << F.getName() << "";

  // Put all SCCs within this function into func_sccs
  for (auto func_it = scc_begin(&F); func_it != scc_end(&F); ++func_it) {
    func_sccs.push_back({});
    // Obtain the BasicBlocks in this SCC
    std::vector<llvm::BasicBlock *> scc_bbs = *func_it;
    for (auto bb_it = scc_bbs.begin(); bb_it != scc_bbs.end(); ++bb_it) {
      func_sccs.back().insert(*bb_it);
      bbs_on_path.insert(*bb_it);
    }
  }

#ifdef DEBUG
  llvm::outs() << "Function SCCs:\n";
  for (std::unordered_set<llvm::BasicBlock *> scc : func_sccs) {
    llvm::outs() << "  SCC " << &scc << ":\n";
    for (llvm::BasicBlock *bb : scc) {
      llvm::outs() << "    " << bb << "\n";
    }
  }
#endif

  // Go through BBs and find the BasicBlock with the right function call
  for (llvm::Function::iterator bb_it = F.begin(); bb_it != F.end(); ++bb_it) {
    if (terminal_bb != nullptr)
      break;

    llvm::BasicBlock *bb = &*bb_it;

    for (llvm::BasicBlock::iterator insn_it = bb->begin(); insn_it != bb->end();
         ++insn_it) {
      llvm::Instruction *insn = &*insn_it;
      if (auto *call_insn = dyn_cast<llvm::CallInst>(insn)) {
        if (call_insn->getCalledFunction() != nullptr && call_insn->getCalledFunction()->getName().contains(
                "raiseSuccessFlag")) {
          terminal_bb = bb;
          break;
        }
      }
    }
  }

  // Make sure we don't start dereferencing nullptrs
  if (terminal_bb == nullptr) {
    llvm::outs() << ": Can't find raiseSuccessFlag function!\n";
    return llvm::PreservedAnalyses::all();
  }

  // Add success SCC to the cricital path
  for (auto scc = func_sccs.begin(); scc != func_sccs.end(); scc++) {
    if (scc->find(terminal_bb) != scc->end()) {
      if (visited_sccs.find(&*scc) == visited_sccs.end())
        visited_sccs.insert(&*scc);
      break;
    }
  }

  // Iterate through the predecessors and find all SCCs on the critical path
  for (llvm::BasicBlock *pred_bb : llvm::predecessors(terminal_bb)) {
    for (auto scc = func_sccs.begin(); scc != func_sccs.end(); scc++) {
      // Make sure the target SCC contains the predecessor BB
      if (scc->count(pred_bb)) {
        // Only add if the SCC isn't already listed
        if (visited_sccs.count(&*scc) == 0)
          visited_sccs.insert(&*scc);
        break;
      }
    }
  }

  // Output all SCCs on the critical path
  for (std::unordered_set<llvm::BasicBlock *> *bb_set : visited_sccs) {
    llvm::outs() << "\n  SCC with size " << bb_set->size() << ":\n";
    for (llvm::BasicBlock *bb : *bb_set) {
      llvm::outs() << "    block " << bb;

      llvm::outs() << ": " << bb->getTerminator()->getOpcodeName() << '\n';

      // Add all terminator operands into "important operands list"
      // TODO: do we not include terminators that lead to other blocks in the SCC?
      for (int i = 0; i < bb->getTerminator()->getNumOperands(); i++) {
        llvm::outs() << "      $" << bb->getTerminator()->getOperand(i)->getValueID() << '\n';
        if (!important_values.contains(bb->getTerminator()->getOperand(i)))
          important_values.insert(bb->getTerminator()->getOperand(i));
      }
    }
  }

  // Go through the uses of each important operand and insert all values into the use-def tree
  std::stack<llvm::Value *> important_values_stack;
  for (llvm::Value *important_value : important_values)
    important_values_stack.push(important_value);

  while (!important_values_stack.empty()) {
    llvm::Value *important_value = important_values_stack.top();
    important_values_stack.pop();

    for (auto u = important_value->user_begin(); u != important_value->user_end(); u++) {
      if (important_values.contains(important_value)) {
        continue;
      } else if (llvm::Instruction *u_insn = dyn_cast<llvm::Instruction>(*u)) {
        // It's an instruction, so we add the operands to the list
        llvm::outs() << "important value insn\n";
        for (int i = 0; i < u_insn->getNumOperands(); i++)
          important_values_stack.push(u_insn->getOperand(i));
      } else if (!important_values.contains(*u)) {
        // It's another value, so we add the value to the list
        important_values.insert(*u);
        important_values_stack.push(*u);
      }
    }
  }

  llvm::outs() << "\nimportant_values:\n";
  for (llvm::Value *important_value : important_values) {
    llvm::outs() << "  $" << important_value->getValueID() << ": " << important_value << "\n";
  }

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
