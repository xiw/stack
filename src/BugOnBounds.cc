#define DEBUG_TYPE "bugon-bounds"
#include "BugOn.h"
#include <llvm/Analysis/MemoryBuiltins.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Target/TargetLibraryInfo.h>

using namespace llvm;

namespace {

struct BugOnBounds : BugOnPass {
	static char ID;
	BugOnBounds() : BugOnPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDataLayoutPass(Registry);
		initializeTargetLibraryInfoPass(Registry);
	}

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
	ObjectSizeOffsetEvaluator *ObjSizeEval;
};

} // anonymous namespace

bool BugOnBounds::runOnFunction(llvm::Function &F) {
	DL = &getAnalysis<DataLayout>();
	TLI = &getAnalysis<TargetLibraryInfo>();
	ObjectSizeOffsetEvaluator TheObjSizeEval(DL, TLI, F.getContext());
	ObjSizeEval = &TheObjSizeEval;
	return super::runOnFunction(F);
}

bool BugOnBounds::runOnInstruction(Instruction *I) {
	Value *Ptr = getNonvolatileAddressOperand(I);
	if (!Ptr)
		return false;
	SizeOffsetEvalType SizeOffset = ObjSizeEval->compute(Ptr);
	if (!ObjSizeEval->bothKnown(SizeOffset))
		return false;
	Value *Size = SizeOffset.first;
	Value *Offset = SizeOffset.second;
	Type *T = Offset->getType();
	assert(T == Size->getType());
	Type *ElemTy = cast<PointerType>(Ptr->getType())->getElementType();
	Value *StoreSize = ConstantInt::get(T, DL->getTypeStoreSize(ElemTy));
	// Bug condition: Offset < 0.
	{
		Value *V = Builder->CreateICmpSLT(Offset, Constant::getNullValue(T));
		insert(V, "buffer overflow");
	}
	// Bug condition: Offset > Size.
	{
		Value *V = Builder->CreateICmpUGT(Offset, Size);
		insert(V, "buffer overflow");
	}
	// Bug condition: Size - Offset < StoreSize.
	{
		Value *V = Builder->CreateICmpULT(Builder->CreateSub(Size, Offset), StoreSize);
		insert(V, "buffer overflow");
	}
	return true;
}

char BugOnBounds::ID;

static RegisterPass<BugOnBounds>
X("bugon-bounds", "Insert bugon calls for bounds checking");
