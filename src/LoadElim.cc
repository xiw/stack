#define DEBUG_TYPE "load-elim"
#include <llvm/Pass.h>
#include <llvm/Analysis/AliasAnalysis.h>
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
	}

	virtual bool runOnFunction(Function &);

private:
	AliasAnalysis *AA;
	TargetLibraryInfo *TLI;

	bool merge(LoadInst *);
};

} // anonymous namespace

bool LoadElim::runOnFunction(Function &F) {
	AA = &getAnalysis<AliasAnalysis>();
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
	AliasAnalysis::Location Loc = AA->getLocation(I);
	Value *Addr = I->getPointerOperand();
	Type *T = I->getType();
	BasicBlock *BB = I->getParent();
	Instruction *Begin = BB->getFirstNonPHI();
	BasicBlock::iterator It(I);
	for (; &*It != Begin; ) {
		Instruction *Src = --It;
		// First check if current instruction modifies Loc.
		if (AA->getModRefInfo(Src, Loc) & AliasAnalysis::Mod)
			return false;
		LoadInst *LI = dyn_cast<LoadInst>(Src);
		if (!LI)
			continue;
		if (LI->getType() != T)
			continue;
		if (!AA->isMustAlias(Addr, LI->getPointerOperand()))
			continue;
		I->replaceAllUsesWith(LI);
		RecursivelyDeleteTriviallyDeadInstructions(I, TLI);
		return true;
	}
	return false;
}

char LoadElim::ID;

static RegisterPass<LoadElim>
X(DEBUG_TYPE, "Eliminate redundant load instructions");
