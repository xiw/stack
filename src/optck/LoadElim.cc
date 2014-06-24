#define DEBUG_TYPE "load-elim"
#include <llvm/Pass.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Transforms/Utils/Local.h>

using namespace llvm;

namespace {

struct LoadElim : FunctionPass {
	static char ID;
	LoadElim() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeAliasAnalysisAnalysisGroup(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		AU.addRequired<AliasAnalysis>();
		AU.addRequired<MemoryDependenceAnalysis>();
	}

	virtual bool runOnFunction(Function &);

private:
	AliasAnalysis *AA;
	MemoryDependenceAnalysis *MDA;
	TargetLibraryInfo *TLI;

	bool merge(LoadInst *);
};

} // anonymous namespace

bool LoadElim::runOnFunction(Function &F) {
	AA = &getAnalysis<AliasAnalysis>();
	MDA = &getAnalysis<MemoryDependenceAnalysis>();
	TLI = getAnalysisIfAvailable<TargetLibraryInfo>();
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
		if (LoadInst *LI = dyn_cast<LoadInst>(I))
			Changed |= merge(LI);
	}
	return Changed;
}

// For now just merge loads in the same block.
bool LoadElim::merge(LoadInst *I) {
	if (I->isVolatile())
		return false;
	Instruction *Dep = MDA->getDependency(I).getInst();
	if (!Dep)
		return false;
	Value *P = NULL, *V = NULL;
	// Find a previous load/store.
	if (LoadInst *LI = dyn_cast<LoadInst>(Dep)) {
		P = LI->getPointerOperand();
		V = LI;
	} else if (StoreInst *SI = dyn_cast<StoreInst>(Dep)) {
		P = SI->getPointerOperand();
		V = SI->getValueOperand();
	}
	if (!P || !V)
		return false;
	// Must be the same type.
	if (V->getType() != I->getType())
		return false;
	// Must be the same address.
	if (!AA->isMustAlias(P, I->getPointerOperand()))
		return false;
	I->replaceAllUsesWith(V);
	RecursivelyDeleteTriviallyDeadInstructions(I, TLI);
	return true;
}

char LoadElim::ID;

static RegisterPass<LoadElim>
X(DEBUG_TYPE, "Eliminate redundant load instructions");
