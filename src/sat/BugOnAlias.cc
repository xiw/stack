// Insert bug assertions on pointer aliasing.

#define DEBUG_TYPE "bugon-alias"
#include "BugOn.h"
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct BugOnAlias : BugOnPass {
	static char ID;
	BugOnAlias() : BugOnPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDominatorTreePass(Registry);
		initializeDataLayoutPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		super::getAnalysisUsage(AU);
		AU.addRequired<DominatorTree>();
		AU.addRequired<DataLayout>();
	}
	virtual bool runOnFunction(Function &);

	virtual bool runOnInstruction(Instruction *);

private:
	DominatorTree *DT;
	DataLayout *DL;
	SmallPtrSet<Value *, 32> Objects;

	void addObject(Value *);
	bool insertNoAlias(Value *, Value *);
	bool visitCallInst(CallInst *);
};

} // anonymous namespace

void BugOnAlias::addObject(Value *O) {
	if (!O->getType()->isPointerTy())
		return;
	O = getUnderlyingObject(O, DL);
	Objects.insert(O);
}

bool BugOnAlias::runOnFunction(Function &F) {
	DT = &getAnalysis<DominatorTree>();
	DL = &getAnalysis<DataLayout>();

	// Find all of the objects first.
	Objects.clear();
	for (Argument &A : F.getArgumentList())
		addObject(&A);
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		if (I->getDebugLoc().isUnknown())
			continue;
		addObject(I);
	}

	return super::runOnFunction(F);
}

bool BugOnAlias::runOnInstruction(Instruction *I) {
	bool Changed = false;
	if (CallInst *CI = dyn_cast<CallInst>(I))
		Changed |= visitCallInst(CI);
	return Changed;
}

// A function with the malloc or noalias attribute (e.g., malloc)
// returns a pointer that does not alias any other pointer if the
// returned pointer is non-null.
//
// Given p = malloc(...) and any pointer q, a bug condition right
// after the malloc call is:
//   p != NULL && p == q.
bool BugOnAlias::visitCallInst(CallInst *I) {
	if (!I->getType()->isPointerTy())
		return false;
	// Has noalias on return.
	Function *F = I->getCalledFunction();
	if (!F || !F->doesNotAlias(0))
		return false;
	// Move insert point to after the noalias call.
	Instruction *OldIP = setInsertPointAfter(I);
	Value *notNull = createIsNotNull(I);
	for (Value *O : Objects) {
		if (Instruction *OI = dyn_cast<Instruction>(O)) {
			// OI needs to properly dominate I.
			if (OI == I || !DT->dominates(OI, I))
				continue;
		}
		Value *E = createAnd(notNull, createPointerEQ(I, O));
		insert(E, "noalias");
	}
	// Restore the insert point.
	setInsertPoint(OldIP);
	return true;
}

char BugOnAlias::ID;

static RegisterPass<BugOnAlias>
X("bugon-alias", "Insert bugon calls for pointer aliasing");
