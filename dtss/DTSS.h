#ifndef LLVM_TRANSFORMS_DTSS_H
#define LLVM_TRANSFORMS_DTSS_H

#include "llvm/IR/PassManager.h"

namespace dtss {
class DTSSPass : public llvm::PassInfoMixin<DTSSPass> {
public:
  static char ID;
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &AM);
};
} // namespace dtss

#endif
