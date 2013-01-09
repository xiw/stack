// Insert bug assertions on use-after-free.

#define DEBUG_TYPE "bugon-free"
#include "BugOn.h"
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/InstIterator.h>
#include <algorithm>

using namespace llvm;

namespace {

struct BugOnFree : BugOnPass {
	static char ID;
	BugOnFree() : BugOnPass(ID) {
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
	Function *CurrentF;

	bool insertNoFree(Value *P, CallInst *CI);
	bool insertNoRealloc(Value *P, CallInst *CI);
};

} // anonymous namespace

bool BugOnFree::runOnFunction(Function &F) {
	DT = &getAnalysis<DominatorTree>();
	DL = &getAnalysis<DataLayout>();
	CurrentF = &F;
	return super::runOnFunction(F);
}

bool BugOnFree::runOnInstruction(Instruction *I) {
	bool Changed = false;
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

	for (inst_iterator i = inst_begin(CurrentF), e = inst_end(CurrentF);
	     i != e; ) {
		Instruction *FI = &*i++;
		if (FI->getDebugLoc().isUnknown())
			continue;
		if (CallInst *CI = dyn_cast<CallInst>(FI)) {
			if (!DT->dominates(FI, I))
				continue;
			Changed |= insertNoFree(P, CI);
			Changed |= insertNoRealloc(P, CI);
		}
	}

	return Changed;
}

// free(f): f != NULL && f == p
bool BugOnFree::insertNoFree(Value *P, CallInst *CI) {
	#define P std::make_pair
	static std::pair<const char *, int> Frees[] = {
		P("free", 0),
		P("_ZdlPv", 0),		// operator delete(void*)
		P("_ZdaPv", 0),		// operator delete[](void*)
		P("kfree", 0),
		P("vfree", 0),
		P("__kfree_skb", 0),
	};
	#undef P

	Value *F = NULL;
	if (!CI->getCalledFunction())
		return false;
	StringRef Name = CI->getCalledFunction()->getName();
	for (unsigned i = 0; i < sizeof(Frees) / sizeof(Frees[0]); i++)
		if (Name == Frees[i].first)
			F = CI->getArgOperand(Frees[i].second);
	if (F == NULL)
		return false;

	P = GetUnderlyingObject(P, DL, 0);
	Value *V = createAnd(
		createIsNotNull(F),
		Builder->CreateICmpEQ(F, Builder->CreatePointerCast(P, F->getType()))
	);
	return insert(V, "nofree");
}

// r=realloc(f, n): f != NULL && r != NULL && f != r && f == p
bool BugOnFree::insertNoRealloc(Value *P, CallInst *CI) {
	#define P std::make_pair
	static std::pair<const char *, int> Reallocs[] = {
		P("realloc", 0),
	};
	#undef P

	Value *F = NULL;
	if (!CI->getCalledFunction())
		return false;
	StringRef Name = CI->getCalledFunction()->getName();
	for (unsigned i = 0; i < sizeof(Reallocs) / sizeof(Reallocs[0]); i++)
		if (Name == Reallocs[i].first)
			F = CI->getArgOperand(Reallocs[i].second);
	if (F == NULL)
		return false;

	P = GetUnderlyingObject(P, DL, 0);
	Value *V = createAnd(
		createAnd(
			createAnd(
				createIsNotNull(F),
				createIsNotNull(CI)
			),
			Builder->CreateICmpNE(F, CI)
		),
		Builder->CreateICmpEQ(F, Builder->CreatePointerCast(P, F->getType()))
	);
	return insert(V, "norealloc");
}

char BugOnFree::ID;

static RegisterPass<BugOnFree>
X("bugon-free", "Insert bugon calls for freed pointers");
