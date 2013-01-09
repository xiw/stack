// Insert bug assertions on pointer aliasing.

#define DEBUG_TYPE "bugon-alias"
#include "BugOn.h"
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/InstIterator.h>
#include <algorithm>
#include <set>

using namespace llvm;

namespace {

struct BugOnAlias : BugOnPass {
	static char ID;
	BugOnAlias() : BugOnPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDominatorTreePass(Registry);
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
	std::set<Value*> Objects;

	void addObject(Value *);
	bool insertNoAlias(Value *, Value *);
	bool visitCallInst(CallInst *);
};

} // anonymous namespace

void BugOnAlias::addObject(Value *O) {
	if (!O->getType()->isPointerTy())
		return;
	O = GetUnderlyingObject(O, DL, 0);
	Objects.insert(O);
}

bool BugOnAlias::runOnFunction(Function &F) {
	DT = &getAnalysis<DominatorTree>();
	DL = &getAnalysis<DataLayout>();

	// Find all of the objects first.
	Objects.clear();
	for (Argument &a: F.getArgumentList())
		addObject(&a);
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
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
	if (!I->getCalledFunction() || !I->getType()->isPointerTy())
		return false;
	// Has noalias on return.
	if (!I->getCalledFunction()->doesNotAlias(0))
		return false;

	// Move insert point to after the noalias call.
	Instruction *OldIP = setInsertPointAfter(I);

	Value *NoaliasCond = createIsNotNull(I);
	// XXX hack: handle realloc.  Would be nice to have this table:
	// http://llvm.org/docs/doxygen/html/MemoryBuiltins_8cpp.html#af3fd8cba508798476dad01815e23711f
	if (I->getCalledFunction()->getName() == "realloc") {
		Value *Prev = I->getArgOperand(0);
		NoaliasCond = createAnd(NoaliasCond, createIsNotNull(Prev));
		NoaliasCond = createAnd(NoaliasCond,
			Builder->CreateICmpNE(I, Prev));
	}

	bool Changed = false;
	for (Value *V: Objects) {
		Instruction *VI = dyn_cast<Instruction>(V);
		if (VI != NULL && !DT->dominates(VI, I))
			continue;
		Value *E = createAnd(
			NoaliasCond,
			Builder->CreateICmpEQ(I,
				Builder->CreatePointerCast(V, I->getType()))
		);
		Changed |= insert(E, "noalias");
	}

	// Restore the insert point.
	setInsertPoint(OldIP);

	return Changed;
}

char BugOnAlias::ID;

static RegisterPass<BugOnAlias>
X("bugon-alias", "Insert bugon calls for pointer aliasing");
