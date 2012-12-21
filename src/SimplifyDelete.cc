// This pass removes redundant NULL pointer checks Clang emits
// for C++ delete.

#define DEBUG_TYPE "simplify-delete"
#include <llvm/Constants.h>
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Analysis/MemoryBuiltins.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Target/TargetLibraryInfo.h>

using namespace llvm;

namespace {

struct SimplifyDelete : FunctionPass {
	static char ID;
	SimplifyDelete() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		AU.addRequired<TargetLibraryInfo>();
	}

	virtual bool runOnFunction(Function &);

private:
	bool visitFreeCall(CallInst *);
};

} // anonymous namespace

bool SimplifyDelete::runOnFunction(Function &F) {
	TargetLibraryInfo *TLI = &getAnalysis<TargetLibraryInfo>();
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		if (CallInst *I = isFreeCall(&*i, TLI))
			Changed |= visitFreeCall(I);
	}
	return Changed;
}

bool SimplifyDelete::visitFreeCall(CallInst *I) {
	// TODO: we could check if anything going on after the call.
	Value *P = I->getArgOperand(0)->stripPointerCasts();
	BasicBlock *BB = I->getParent();
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
	switch (ICI->getPredicate()) {
	default: return false;
	case CmpInst::ICMP_EQ:
		if (BI->getSuccessor(1) != BB)
			return false;
		break;
	case CmpInst::ICMP_NE:
		if (BI->getSuccessor(0) != BB)
			return false;
		break;
	}
	Value *L = ICI->getOperand(0)->stripPointerCasts();
	Value *R = ICI->getOperand(1)->stripPointerCasts();
	if (!((L == P && isa<ConstantPointerNull>(R))
	   || (R == P && isa<ConstantPointerNull>(L))))
		return false;
	// Remove debugging information to ignore the check.
	ICI->setDebugLoc(DebugLoc());
	return true;
}

char SimplifyDelete::ID;

static RegisterPass<SimplifyDelete>
X("simplify-delete", "Remove redundant NULL pointer checks");
