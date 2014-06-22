// This pass ignores loop initial conditions for anti-simplify.
// It should run after -loop-rotate.

#define DEBUG_TYPE "ignore-loop-initial"
#include <llvm/Pass.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;

namespace {

struct IgnoreLoopInitial : FunctionPass {
	static char ID;
	IgnoreLoopInitial() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);
};

} // anonymous namespace

bool IgnoreLoopInitial::runOnFunction(Function &F) {
	bool Changed = false;
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
		BasicBlock *Preheader = i;
		if (Preheader->getName().find(".lr.ph") == StringRef::npos)
			continue;
		// Conditional jump to preheader?
		BasicBlock *Initial = Preheader->getSinglePredecessor();
		if (!Initial)
			continue;
		BranchInst *BI = dyn_cast<BranchInst>(Initial->getTerminator());
		if (!BI || !BI->isConditional())
			continue;
		Value *Cond = BI->getCondition();
		Instruction *I = dyn_cast<Instruction>(Cond);
		if (!I)
			continue;
		I->setDebugLoc(DebugLoc());
		Changed = true;
	}
	return Changed;
}

char IgnoreLoopInitial::ID;

static RegisterPass<IgnoreLoopInitial>
X(DEBUG_TYPE, "Ignore loop initial conditions");
