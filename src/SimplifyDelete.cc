// This pass removes redundant NULL pointer checks Clang emits
// for C++ delete.

#define DEBUG_TYPE "simplify-delete"
#include <llvm/BasicBlock.h>
#include <llvm/Function.h>
#include <llvm/Instructions.h>
#include <llvm/Pass.h>

using namespace llvm;

namespace {

struct SimplifyDelete : FunctionPass {
	static char ID;
	SimplifyDelete() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	bool visitDeleteBB(BasicBlock *);
};

} // anonymous namespace

bool SimplifyDelete::runOnFunction(Function &F) {
	bool Changed = false;
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i)
		Changed |= visitDeleteBB(i);
	return Changed;
}

bool SimplifyDelete::visitDeleteBB(BasicBlock *BB) {
	// Clang emits BB with this special name for BB.
	// It works better with overloaded delete.
	if (!BB->getName().startswith("delete.notnull"))
		return false;
	BasicBlock *Pred = BB->getSinglePredecessor();
	if (!Pred)
		return false;
	BranchInst *BI = dyn_cast<BranchInst>(Pred->getTerminator());
	if (!BI || !BI->isConditional())
		return false;
	ICmpInst *ICI = dyn_cast<ICmpInst>(BI->getCondition());
	if (!ICI || !ICI->isEquality())
		return false;
	if (ICI->getDebugLoc().isUnknown())
		return false;
	// Remove debugging information to ignore the check.
	ICI->setDebugLoc(DebugLoc());
	return true;
}

char SimplifyDelete::ID;

static RegisterPass<SimplifyDelete>
X("simplify-delete", "Remove redundant NULL pointer checks");
