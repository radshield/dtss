#ifndef LLVM_TRANSFORMS_RAD_RATING_H
#define LLVM_TRANSFORMS_RAD_RATING_H

#include "llvm/IR/PassManager.h"

namespace dtss {
class RadRatingPass : public llvm::PassInfoMixin<RadRatingPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &AM);
};
} // namespace dtss

#endif
