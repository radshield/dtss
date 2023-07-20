#ifndef LLVM_TRANSFORMS_CONTROL_FLOW_DMR_H
#define LLVM_TRANSFORMS_CONTROL_FLOW_DMR_H

#include "llvm/IR/PassManager.h"

namespace ControlFlowDMR {
class ControlFlowDMRPass : public llvm::PassInfoMixin<ControlFlowDMRPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &AM);
};
} // namespace ControlFlowDMR

#endif
