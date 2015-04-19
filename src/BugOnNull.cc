#define DEBUG_TYPE "bugon-null"
#include "BugOn.h"
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>


using namespace llvm;

namespace {

struct BugOnNull : BugOnPass {
	static char ID;
	BugOnNull() : BugOnPass(ID) {
		//PassRegistry &Registry = *PassRegistry::getPassRegistry();
		//initializeDataLayoutPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		super::getAnalysisUsage(AU);
		//AU.addRequired<DataLayout>();
	}

	virtual bool runOnFunction(Function &);
	virtual bool runOnInstruction(Instruction *);

private:
	const DataLayout *DL;
	SmallPtrSet<Value *, 32> Visited;
};

} // anonymous namespace

bool BugOnNull::runOnFunction(Function &F) {
	DL = &F.getParent()->getDataLayout();
	return super::runOnFunction(F);
}

bool BugOnNull::runOnInstruction(Instruction *I) {
	if (isa<TerminatorInst>(I)) {
		Visited.clear();
		return false;
	}
	Value *Base = getNonvolatileBaseAddress(I, *DL);
	if (!Base)
		return false;
	if (!Visited.insert(Base).second)
		return false;
	return insert(createIsNull(Base, *DL), "null pointer dereference");
}

char BugOnNull::ID;

static RegisterPass<BugOnNull>
X("bugon-null", "Insert bugon calls for possible null pointer dereference");
