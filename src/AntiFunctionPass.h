#pragma once

#include "BugOn.h"
#include "Diagnostic.h"
#include "PathGen.h"
#include "ValueGen.h"
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/IR/DataLayout.h>

#define BENCHMARK(e) if (BenchmarkFlag) { e; }

extern bool BenchmarkFlag;

class SMTSolver;

class AntiFunctionPass : public llvm::FunctionPass {
protected:
	llvm::DataLayout *DL;
	llvm::DominatorTree *DT;
	llvm::SmallVector<PathGen::Edge, 32> Backedges;
	Diagnostic Diag;

	explicit AntiFunctionPass(char &ID);
	~AntiFunctionPass();
	virtual bool runOnAntiFunction(llvm::Function &F) = 0;
	virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

	// Call if CFG has changed.
	void recalculate(llvm::Function &F);
	// Return bug-free assertion.
	SMTExpr getDeltaForBlock(llvm::BasicBlock *, ValueGen &);
	SMTStatus queryWithDelta(SMTExpr E, SMTExpr Delta, ValueGen &);
	void printMinimalAssertions();

private:
	llvm::Function *BugOn;
	llvm::PostDominatorTree *PDT;
	llvm::SmallVector<llvm::BasicBlock *, 8> InLoopBlocks;
	llvm::SmallVector<BugOnInst *, 8> Assertions;
	void *Buffer;

	virtual bool runOnFunction(llvm::Function &);
};
