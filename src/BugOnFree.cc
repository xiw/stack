// Insert bug assertions on use-after-free.

#define DEBUG_TYPE "bugon-free"
#include "BugOn.h"
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/MemoryBuiltins.h>
#include <llvm/Support/CallSite.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Target/TargetLibraryInfo.h>

using namespace llvm;

namespace {

struct BugOnFree : BugOnPass {
	static char ID;
	BugOnFree() : BugOnPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDataLayoutPass(Registry);
		initializeTargetLibraryInfoPass(Registry);
		initializeDominatorTreePass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		super::getAnalysisUsage(AU);
		AU.addRequired<DataLayout>();
		AU.addRequired<TargetLibraryInfo>();
		AU.addRequired<DominatorTree>();
	}

	virtual bool runOnFunction(Function &);
	virtual bool runOnInstruction(Instruction *);

private:
	DataLayout *DL;
	TargetLibraryInfo *TLI;
	DominatorTree *DT;
	SmallPtrSet<Use *, 4> FreePtrs; // Use is a <call, arg> pair.

	Use *extractFree(CallSite CS);
};

} // anonymous namespace

bool BugOnFree::runOnFunction(Function &F) {
	DT = &getAnalysis<DominatorTree>();
	TLI = &getAnalysis<TargetLibraryInfo>();
	DL = &getAnalysis<DataLayout>();
	// Collect free/realloc calls.
	FreePtrs.clear();
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction &I = *i;
		if (I.getDebugLoc().isUnknown())
			continue;
		CallSite CS(&I);
		if (!CS || !CS.getCalledFunction())
			continue;
		if (Use *U = extractFree(CS)) {
			FreePtrs.insert(U);
			continue;
		}
	}
	return super::runOnFunction(F);
}

bool BugOnFree::runOnInstruction(Instruction *I) {
	if (FreePtrs.empty())
		return false;

	Value *P = getNonvolatileBaseAddress(I, DL);
	if (!P)
		return false;

	bool Changed = false;
	// free(x): x == p (p must be nonnull).
	for (Use *U : FreePtrs) {
		Instruction *FreeCall = cast<Instruction>(U->getUser());
		if (!DT->dominates(FreeCall, I))
			continue;
		Value *X = U->get();
		Value *V = createPointerEQ(X, P);
		// x' = realloc(x, n): x == p && x' != null.
		if (FreeCall->getType()->isPointerTy())
			V = createAnd(createIsNotNull(FreeCall), V);
		StringRef Name = CallSite(FreeCall).getCalledFunction()->getName();
		Changed |= insert(V, Name, FreeCall->getDebugLoc());
	}
	return Changed;
}

Use *BugOnFree::extractFree(CallSite CS) {
#define P std::make_pair
	static std::pair<const char *, int> Frees[] = {
		P("kfree", 0),
		P("vfree", 0),
		P("__kfree_skb", 0),
	};
#undef P
	// Builtin free/delete/realloc.
	Instruction *I = CS.getInstruction();
	if (isFreeCall(I, TLI) || isReallocLikeFn(I, TLI))
		return CS.arg_begin();
	// Custom function.
	StringRef Name = CS.getCalledFunction()->getName();
	for (unsigned i = 0; i < sizeof(Frees) / sizeof(Frees[0]); i++) {
		if (Name == Frees[i].first)
			return CS.arg_begin() + Frees[i].second;
	}
	return NULL;

}

char BugOnFree::ID;

static RegisterPass<BugOnFree>
X("bugon-free", "Insert bugon calls for freed pointers");
