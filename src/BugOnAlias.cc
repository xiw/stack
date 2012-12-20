// Insert bug assertions on pointer aliasing.

#define DEBUG_TYPE "bugon-alias"
#include "BugOn.h"
#include <llvm/Function.h>
#include <llvm/Instructions.h>
#include <llvm/Analysis/Dominators.h>
#include <algorithm>

using namespace llvm;

namespace {

struct BugOnAlias : BugOnPass {
	static char ID;
	BugOnAlias() : BugOnPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		super::getAnalysisUsage(AU);
		AU.addRequired<DominatorTree>();
	}
	virtual bool runOnFunction(Function &);

	virtual bool visit(Instruction *);

private:
	DominatorTree *DT;

	bool insertNoAlias(Value *, Value *);
	bool visitICmpInst(ICmpInst *);
};

} // anonymous namespace

bool BugOnAlias::runOnFunction(Function &F) {
        DT = &getAnalysis<DominatorTree>();
        return super::runOnFunction(F);
}

bool BugOnAlias::visit(Instruction *I) {
	bool Changed = false;
	if (ICmpInst *ICI = dyn_cast<ICmpInst>(I))
		Changed |= visitICmpInst(ICI);
	return Changed;
}

// A function with the malloc or noalias attribute (e.g., malloc)
// returns a pointer that does not alias any other pointer if the
// returned pointer is non-null.
//
// Given p = malloc(...) and any pointer q, a bug condition right
// after the malloc call is:
//   p != NULL && p == q.
bool BugOnAlias::visitICmpInst(ICmpInst *I) {
	if (!I->isEquality())
		return false;
	Value *L = I->getOperand(0)->stripPointerCasts();
	Value *R = I->getOperand(1)->stripPointerCasts();
	if (!L->getType()->isPointerTy())
		return false;
	bool Changed = false;
	Changed |= insertNoAlias(L, R);
	Changed |= insertNoAlias(R, L);
	return Changed;

}

// Insert L != NULL && L == R right after L's def.
bool BugOnAlias::insertNoAlias(Value *L, Value *R) {
	// Ignore invoke for now.
	CallInst *CI = dyn_cast<CallInst>(L);
	if (!CI)
		return false;
	Function *F = CI->getCalledFunction();
	if (!F)
		return false;
	// Has noalias on return.
	if (!F->doesNotAlias(0))
		return false;
	// To insert L == R, R's def has to domminate L.
	if (Instruction *RI = dyn_cast<Instruction>(R)) {
		if (!DT->dominates(RI, CI))
			return false;
	}
	// Move insert point to the noalias call L.
	Instruction *IP = setInsertPointAfter(CI);
	Value *V = createAnd(
		createIsNotNull(L),
		Builder->CreateICmpEQ(L, R)
	);
	bool Changed = insert(V, "noalias");
	// Restore the insert point.
	setInsertPoint(IP);
	return Changed;
}

char BugOnAlias::ID;

static RegisterPass<BugOnAlias>
X("bugon-alias", "Insert bugon calls for pointer aliasing");
