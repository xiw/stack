// Insert bug assertions on pointer arithmetic.

#define DEBUG_TYPE "bugon-gep"
#include "BugOn.h"
#include <llvm/DataLayout.h>
#include <llvm/Operator.h>

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
	// For now we only deal with gep with one index (e.g., array).
	if (GEP->getNumIndices() > 1)
		return false;
	// Ignore zero index.
	if (GEP->hasAllZeroIndices())
		return false;
	Value *P = GEP->getPointerOperand();
	Type *ElemTy = cast<PointerType>(P->getType())->getElementType();
	unsigned PtrBits = DL->getPointerSizeInBits(/*GEP->getPointerAddressSpace()*/);
	LLVMContext &VMCtx = ElemTy->getContext();
	IntegerType *PtrIntTy = Type::getIntNTy(VMCtx, PtrBits);
	APInt ElemSize(PtrBits, DL->getTypeAllocSize(ElemTy));
	bool Changed = false;
	// Sign-extend index.
	Value *Idx = createSExtOrTrunc(*GEP->idx_begin(), PtrIntTy);
	Value *Offset;
	if (ElemSize.ugt(1)) {
		// idx * elemsize overflows.
		{
			APInt Hi = APInt::getSignedMaxValue(PtrBits).sdiv(ElemSize);
			Value *V = Builder->CreateICmpSGT(Idx, ConstantInt::get(VMCtx, Hi));
			Changed |= insert(V, "pointer overflow");
		}
		// idx * elemsize underflows.
		{
			APInt Lo = APInt::getSignedMinValue(PtrBits).sdiv(ElemSize);
			Value *V = Builder->CreateICmpSLT(Idx, ConstantInt::get(VMCtx, Lo));
			Changed |= insert(V, "pointer overflow");
		}
		Offset = Builder->CreateMul(ConstantInt::get(VMCtx, ElemSize), Idx);
	} else {
		Offset = Idx;
	}
	// Extend pointers to n + 1 bits to avoid overflow.
	IntegerType *PtrIntExTy = Type::getIntNTy(VMCtx, PtrBits + 1);
	// end = ptr + idx * elemsize.
	Value *End = Builder->CreateAdd(
		Builder->CreatePtrToInt(P, PtrIntExTy),
		createSExtOrTrunc(Offset, PtrIntExTy)
	);
	// Bug condition: end > uintptr_max.
	{
		Value *V = Builder->CreateICmpSGT(End, Constant::getAllOnesValue(PtrIntExTy));
		Changed |= insert(V, "pointer overflow");
	}
	// Bug condition: end < 0.
	{
		Value *V = Builder->CreateICmpSLT(End, Constant::getNullValue(PtrIntExTy));
		Changed |= insert(V, "pointer overflow");
	}
	// TODO: more constraints if we can extract the allocation size.
	return Changed;
}

char BugOnGep::ID;

static RegisterPass<BugOnGep>
X("bugon-gep", "Insert bugon calls for pointer arithmetic");
