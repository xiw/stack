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

} // anonymous nmaespace

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
	// Bug condition: p + elemsize * idx overflow.
	Value *Idx = *GEP->idx_begin();
	// Ignore zero/negative index.
	if (ConstantInt *C = dyn_cast<ConstantInt>(Idx)) {
		if (!C->getValue().isStrictlyPositive())
			return false;
	}
	Value *P = GEP->getPointerOperand();
	Type *ElemTy = cast<PointerType>(P->getType())->getElementType();
	unsigned PtrBits = DL->getPointerSizeInBits(/*GEP->getPointerAddressSpace()*/);
	LLVMContext &VMCtx = ElemTy->getContext();
	IntegerType *PtrIntTy = Type::getIntNTy(VMCtx, PtrBits);
	APInt ElemSize(PtrBits, DL->getTypeAllocSize(ElemTy));
	APInt AllocSize = APInt::getMaxValue(PtrBits);
	bool Changed = false;
	// Sign-extend index.
	Value *IdxExt = Builder->CreateSExtOrTrunc(Idx, PtrIntTy);
	Value *Offset;
	if (ElemSize.ugt(1)) {
		APInt Hi = APInt::getSignedMaxValue(PtrBits).sdiv(ElemSize);
		Value *V = Builder->CreateICmpSGT(IdxExt, ConstantInt::get(VMCtx, Hi));
		Changed |= insert(V, "pointer overflow");
		Offset = Builder->CreateMul(ConstantInt::get(VMCtx, ElemSize), IdxExt);
	} else {
		Offset = IdxExt;
	}
	// idx > 0 && sadd-overflow(ptr, idx * elemsize)
	Value *IsPos = Builder->CreateICmpSGT(Idx, Constant::getNullValue(Idx->getType()));
	Value *PtrMax = ConstantInt::get(VMCtx, APInt::getMaxValue(PtrBits));
	Value *V = Builder->CreateAnd(
		Builder->CreateICmpUGT(
			Builder->CreatePtrToInt(P, PtrIntTy),
			Builder->CreateSub(PtrMax, Offset)),
		IsPos // This will be folded if it is 1.
	);
	Changed |= insert(V, "pointer overflow");
	return Changed;
}

char BugOnGep::ID;

static RegisterPass<BugOnGep>
X("bugon-gep", "Insert bugon calls for pointer arithmetic");
