// This pass removes redundant NULL pointer checks Clang emits
// for C++ delete.

#define DEBUG_TYPE "simplify-delete"
#include "BugOn.h"
#include <llvm/Pass.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Local.h>

using namespace llvm;

namespace {

struct SimplifyDelete : FunctionPass {
	static char ID;
	SimplifyDelete() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDominatorTreePass(Registry);
	}

	void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.addRequired<DominatorTree>();
	}

	virtual bool runOnFunction(Function &);

private:
	bool removeUnreachable(Function &);
	bool visitDeleteBB(BasicBlock *);
};

} // anonymous namespace

bool SimplifyDelete::runOnFunction(Function &F) {
	bool Changed = false;
	Changed |= removeUnreachable(F);
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ) {
		BasicBlock *BB = i++;
		Changed |= visitDeleteBB(BB);
	        Changed |= ConstantFoldTerminator(BB, true);
	        Changed |= EliminateDuplicatePHINodes(BB);
		// Must be the last one.
		Changed |= MergeBlockIntoPredecessor(BB, this);
	}
	return Changed;
}

bool SimplifyDelete::removeUnreachable(Function &F) {
	DominatorTree &DT = getAnalysis<DominatorTree>();
	SmallVector<BasicBlock *, 4> UnreachableBB;
	for (BasicBlock &BB : F) {
		if (DT.isReachableFromEntry(&BB))
			continue;
		for (succ_iterator i = succ_begin(&BB), e = succ_end(&BB); i != e; ++i) {
			BasicBlock *Succ = *i;
			if (DT.isReachableFromEntry(Succ))
				Succ->removePredecessor(&BB);
		}
		BB.dropAllReferences();
		UnreachableBB.push_back(&BB);
	}
	if (UnreachableBB.empty())
		return false;
	for (BasicBlock *BB : UnreachableBB)
		BB->eraseFromParent();
	return true;
}

bool SimplifyDelete::visitDeleteBB(BasicBlock *BB) {
	// Clang emits BB with this special name for BB.
	// It works better with overloaded new/delete.
	StringRef Name = BB->getName();
	if (!Name.startswith("new.notnull")
	    && !Name.startswith("delete.notnull")
	    && !Name.startswith("cast.notnull"))
		return false;
	BasicBlock *Pred = BB->getSinglePredecessor();
	if (!Pred)
		return false;
	BranchInst *BI = dyn_cast<BranchInst>(Pred->getTerminator());
	if (!BI || !BI->isConditional())
		return false;
	// Remove debugging information to ignore the check.
	return BugOnPass::clearDebugLoc(BI->getCondition());
}

char SimplifyDelete::ID;

static RegisterPass<SimplifyDelete>
X("simplify-delete", "Remove redundant NULL pointer checks");
