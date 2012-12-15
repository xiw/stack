#pragma once

#include "PathGen.h"
#include "ValueGen.h"
#include <llvm/DataLayout.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>

namespace llvm {
	class BasicBlock;
	class CallInst;
	class DominatorTree;
	class PostDominatorTree;
} // namespace llvm

class SMTSolver;

class AntiFunctionPass : public llvm::FunctionPass {
protected:
	llvm::DataLayout *DL;
	llvm::DominatorTree *DT;
	llvm::SmallVector<PathGen::Edge, 32> Backedges;

	explicit AntiFunctionPass(char &ID);
	virtual bool runOnAntiFunction(llvm::Function &F) = 0;
	virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

	// Call if CFG has changed.
	void recalculate(llvm::Function &F);
	// Return bug-free assertion.
	SMTExpr getDeltaForBlock(llvm::BasicBlock *, ValueGen &);

private:
	llvm::Function *BugOn;
	llvm::PostDominatorTree *PDT;
	llvm::SmallVector<llvm::BasicBlock *, 8> InLoopBlocks;

	virtual bool runOnFunction(llvm::Function &);
};
