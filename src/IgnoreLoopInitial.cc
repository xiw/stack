// This pass ignores loop initial conditions for anti-simplify.
// It should run after -loop-rotate.

#define DEBUG_TYPE "ignore-loop-initial"
#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;

namespace {

struct IgnoreLoopInitial : FunctionPass {
	static char ID;
	IgnoreLoopInitial() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeLoopInfoPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		AU.addRequired<LoopInfo>();
	}

	virtual bool runOnFunction(Function &);
};

} // anonymous namespace

bool IgnoreLoopInitial::runOnFunction(Function &F) {
	LoopInfo &LI = getAnalysis<LoopInfo>();
	bool Changed = false;
	for (LoopInfo::iterator i = LI.begin(), e = LI.end(); i != e; ++i) {
		Loop *L = *i;
		BasicBlock *Preheader = L->getLoopPreheader();
		if (!Preheader)
			continue;
		// Conditional jump to preheader?
		BasicBlock *Initial = Preheader->getSinglePredecessor();
		if (!Initial)
			continue;
		BranchInst *BI = dyn_cast<BranchInst>(Initial->getTerminator());
		if (!BI || !BI->isConditional())
			continue;
		// TODO: Another branch to exit?
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
