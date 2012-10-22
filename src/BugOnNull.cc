#define DEBUG_TYPE "bugon-null"
#include "BugOn.h"
#include <llvm/DataLayout.h>
#include <llvm/Analysis/ValueTracking.h>

using namespace llvm;

namespace {

struct BugOnNull : BugOnPass {
	static char ID;
	BugOnNull() : BugOnPass(ID) {}

	virtual bool runOnFunction(llvm::Function &);
	virtual bool visit(Instruction *);

private:
	DataLayout *DL;
};

} // anonymous nmaespace

bool BugOnNull::runOnFunction(llvm::Function &F) {
	DL = getAnalysisIfAvailable<DataLayout>();
	return super::runOnFunction(F);
}

bool BugOnNull::visit(Instruction *I) {
	Value *P = NULL;
	if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
		if (!LI->isVolatile())
			P = LI->getPointerOperand();
	} else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
		if (!SI->isVolatile())
			P = SI->getPointerOperand();
	}
	if (!P)
		return false;
	// Strip pointer offset to get the base pointer.
	Value *Base = GetUnderlyingObject(P, DL, 0);
	// Ignore trivial non-null cases (e.g., the base pointer is alloca).
	if (Base->isDereferenceablePointer())
		return false;
	return insert(Builder->CreateIsNull(Base), "null pointer dereference");
}

char BugOnNull::ID;

static RegisterPass<BugOnNull>
X("bugon-null", "Insert bugon calls for possible null pointer dereference");
