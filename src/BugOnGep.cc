// Insert bug assertions on pointer arithmetic.

#define DEBUG_TYPE "bugon-gep"
#include "BugOn.h"
#include <llvm/DataLayout.h>
#include <llvm/Operator.h>
#include <llvm/Transforms/Utils/Local.h>

using namespace llvm;

namespace {

struct BugOnGep : BugOnPass {
	static char ID;
	BugOnGep() : BugOnPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		super::getAnalysisUsage(AU);
		AU.addRequired<DataLayout>();
	}
	virtual bool runOnFunction(Function &);

	virtual bool visit(Instruction *);

private:
	DataLayout *DL;

	bool insertIndexOverflow(GEPOperator *GEP);
	bool insertOffsetOverflow(GEPOperator *GEP);
};

} // anonymous namespace

bool BugOnGep::runOnFunction(Function &F) {
        DL = &getAnalysis<DataLayout>();
        return super::runOnFunction(F);
}

bool BugOnGep::visit(Instruction *I) {
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
		APInt Size(PtrBits, DL->getTypeAllocSize(IndexedTy));
		Value *Index = createSExtOrTrunc(*i, PtrIntTy);
		// Bug condition: index * size overflows.
		{
			APInt Hi = APInt::getSignedMaxValue(PtrBits).sdiv(Size);
			Value *V = Builder->CreateICmpSGT(Index, ConstantInt::get(VMCtx, Hi));
			Changed |= insert(V, "pointer overflow");
		}
		// Bug condition: index * size underflows.
		{
			APInt Lo = APInt::getSignedMinValue(PtrBits).sdiv(Size);
			Value *V = Builder->CreateICmpSLT(Index, ConstantInt::get(VMCtx, Lo));
			Changed |= insert(V, "pointer overflow");
		}
	}
#endif
	return Changed;
}

bool BugOnGep::insertOffsetOverflow(GEPOperator *GEP) {
	Value *P = GEP->getPointerOperand();
	Value *Offset = EmitGEPOffset(Builder, *DL, GEP);
	unsigned PtrBits = DL->getPointerSizeInBits(/*GEP->getPointerAddressSpace()*/);
	LLVMContext &VMCtx = GEP->getContext();
	// Extend to n + 1 bits to avoid overflowing ptr + offset.
	IntegerType *PtrIntExTy = Type::getIntNTy(VMCtx, PtrBits + 1);
	Value *End = Builder->CreateAdd(
		Builder->CreatePtrToInt(P, PtrIntExTy),
		createSExtOrTrunc(Offset, PtrIntExTy)
	);
	// Bug condition: ptr + offset > uintptr_max.
	{
		Value *PtrMax = ConstantInt::get(VMCtx, APInt::getMaxValue(PtrBits).zext(PtrBits + 1));
		Value *V = Builder->CreateICmpSGT(End, PtrMax);
		insert(V, "pointer overflow");
	}
	// Bug condition: ptr + offset < 0.
	{
		Value *V = Builder->CreateICmpSLT(End, Constant::getNullValue(PtrIntExTy));
		insert(V, "pointer overflow");
	}
	return true;
}

char BugOnGep::ID;

static RegisterPass<BugOnGep>
X("bugon-gep", "Insert bugon calls for pointer arithmetic");
