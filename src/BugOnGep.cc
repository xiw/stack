// Insert bug assertions on pointer arithmetic.

#define DEBUG_TYPE "bugon-gep"
#include "BugOn.h"
#include <llvm/Analysis/InstructionSimplify.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Operator.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Transforms/Utils/Local.h>

using namespace llvm;

namespace {

struct BugOnGep : BugOnPass {
	static char ID;
	BugOnGep() : BugOnPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		super::getAnalysisUsage(AU);
		AU.addRequired<DataLayout>();
		AU.addRequired<TargetLibraryInfo>();
	}
	virtual bool runOnFunction(Function &);

	virtual bool runOnInstruction(Instruction *);

private:
	DataLayout *DL;
	TargetLibraryInfo *TLI;

	bool insertIndexOverflow(GEPOperator *GEP);
	bool insertOffsetOverflow(GEPOperator *GEP);
	bool isAlwaysInBounds(Value *P, Value *Offset);
};

} // anonymous namespace

bool BugOnGep::runOnFunction(Function &F) {
	DL = &getAnalysis<DataLayout>();
	TLI = &getAnalysis<TargetLibraryInfo>();
	return super::runOnFunction(F);
}

bool BugOnGep::runOnInstruction(Instruction *I) {
	GEPOperator *GEP = dyn_cast<GEPOperator>(I);
	if (!GEP || !GEP->isInBounds())
		return false;
	// Ignore zero index.
	if (GEP->hasAllZeroIndices())
		return false;
	bool Changed = false;
	Changed |= insertIndexOverflow(GEP);
	Changed |= insertOffsetOverflow(GEP);
	return Changed;
}

bool BugOnGep::insertIndexOverflow(GEPOperator *GEP) {
	bool Changed = false;
#if 0
	LLVMContext &VMCtx = GEP->getContext();
	unsigned PtrBits = DL->getPointerSizeInBits(/*GEP->getPointerAddressSpace()*/);
	IntegerType *PtrIntTy = Type::getIntNTy(VMCtx, PtrBits);
	gep_type_iterator GTI = gep_type_begin(GEP);
        for (GEPOperator::op_iterator i = GEP->idx_begin(),
	     e = GEP->idx_end(); i != e; ++i, ++GTI) {
		if (isa<StructType>(*GTI))
			continue;
	        Type *IndexedTy = GTI.getIndexedType();
	        if (!IndexedTy->isSized())
			continue;
		Value *Size = ConstantInt::get(PtrIntTy, DL->getTypeAllocSize(IndexedTy));
		Value *Index = createSExtOrTrunc(*i, PtrIntTy);
		// Bug condition: size * index overflows.
		Value *V = createIsSMulWrap(Size, Index);
		Changed |= insert(V, "pointer overflow");
	}
#endif
	return Changed;
}

bool BugOnGep::insertOffsetOverflow(GEPOperator *GEP) {
	// Ignore struct offset, which is likely to be in range.
	for (gep_type_iterator GTI = gep_type_begin(GEP), E = gep_type_end(GEP); GTI != E; ++GTI) {
		if (isa<StructType>(*GTI))
			return false;
	}
	Value *P = GEP->getPointerOperand();
	Value *Offset = EmitGEPOffset(Builder, *DL, GEP);
	if (isAlwaysInBounds(P, Offset)) {
		RecursivelyDeleteTriviallyDeadInstructions(Offset, TLI);
		return false;
	}
	unsigned PtrBits = DL->getPointerSizeInBits(GEP->getPointerAddressSpace());
	LLVMContext &VMCtx = GEP->getContext();
	// Extend to n + 1 bits to avoid overflowing ptr + offset.
	IntegerType *PtrIntExTy = Type::getIntNTy(VMCtx, PtrBits + 1);
	Value *End = Builder->CreateAdd(
		Builder->CreatePtrToInt(P, PtrIntExTy),
		createSExtOrTrunc(Offset, PtrIntExTy)
	);
	bool Changed = false;
	// Bug condition: ptr + offset > uintptr_max.
	{
		Value *PtrMax = ConstantInt::get(VMCtx, APInt::getMaxValue(PtrBits).zext(PtrBits + 1));
		Value *V = Builder->CreateICmpSGT(End, PtrMax);
		Changed |= insert(V, "pointer overflow");
	}
	// Bug condition: ptr + offset < 0.
	{
		Value *V = Builder->CreateICmpSLT(End, Constant::getNullValue(PtrIntExTy));
		Changed |= insert(V, "pointer overflow");
	}
	if (!Changed)
		RecursivelyDeleteTriviallyDeadInstructions(End, TLI);
	return Changed;
}

// Ignore p + foo(p, ...) that is always in-bounds.
bool BugOnGep::isAlwaysInBounds(Value *P, Value *Offset) {
	// n + 0 => n.
	if (auto I = dyn_cast<Instruction>(Offset))
		Offset = SimplifyInstruction(I, DL, TLI);
	auto CI = dyn_cast<CallInst>(Offset);
	if (!CI)
		return false;
	auto Callee = CI->getCalledFunction();
	if (!Callee)
		return false;
	LibFunc::Func F;
	if (!TLI->getLibFunc(Callee->getName(), F) || !TLI->has(F))
		return false;
	Value *S;
	switch (F) {
	default:
		return false;
	case LibFunc::fwrite:
	case LibFunc::strcspn:
	case LibFunc::strlen:
	case LibFunc::strnlen:
	case LibFunc::strspn:
		S = CI->getArgOperand(0);
		break;
	}
	S = S->stripPointerCasts();
	if (S == P->stripPointerCasts())
		return true;
	return false;
}

char BugOnGep::ID;

static RegisterPass<BugOnGep>
X("bugon-gep", "Insert bugon calls for pointer arithmetic");
