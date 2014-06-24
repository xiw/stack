#define DEBUG_TYPE "bugon-libc"
#include "BugOn.h"
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

using namespace llvm;

namespace {

struct BugOnLibc : BugOnPass {
	static char ID;
	BugOnLibc() : BugOnPass(ID) {}

	virtual bool doInitialization(Module &);
	virtual bool runOnInstruction(Instruction *);

private:
	typedef bool (BugOnLibc::*handler_t)(CallInst *);
	DenseMap<Function *, handler_t> FunctionMap;
	DenseMap<unsigned int, handler_t> IntrinsicMap;

	bool visitAbs(CallInst *);
	bool visitCtz(CallInst *);
	bool visitDiv(CallInst *);
	bool visitMemcpy(CallInst *);
};

} // anonymous namespace

bool BugOnLibc::doInitialization(Module &M) {
#define HANDLER(method, name) \
	if (Function *F = M.getFunction(name)) \
		FunctionMap[F] = &BugOnLibc::method;

	HANDLER(visitAbs, "abs");
	HANDLER(visitAbs, "labs");
	HANDLER(visitAbs, "llabs");
	HANDLER(visitAbs, "imaxabs");

	HANDLER(visitDiv, "div");
	HANDLER(visitDiv, "ldiv");
	HANDLER(visitDiv, "lldiv");
	HANDLER(visitDiv, "imaxdiv");

	HANDLER(visitMemcpy, "memcpy");
	HANDLER(visitMemcpy, "__memcpy");	// Linux kernel internal

#undef HANDLER

#define HANDLER(method, id) \
	IntrinsicMap[id] = &BugOnLibc::method;

	HANDLER(visitMemcpy, Intrinsic::memcpy);
	HANDLER(visitCtz, Intrinsic::ctlz);
	HANDLER(visitCtz, Intrinsic::cttz);

#undef HANDLER
	return false;
}

bool BugOnLibc::runOnInstruction(Instruction *I) {
	CallInst *CI = dyn_cast<CallInst>(I);
	if (!CI || isa<BugOnInst>(CI))
		return false;
	Function *F = CI->getCalledFunction();
	if (!F)
		return false;
	handler_t Handler = NULL;
	if (F->isIntrinsic()) {
		Handler = IntrinsicMap.lookup(F->getIntrinsicID());
	} else {
		Handler = FunctionMap.lookup(F);
	}
	if (!Handler)
		return false;
	return (this->*Handler)(CI);
}

// abs(x): x == INT_MIN
bool BugOnLibc::visitAbs(CallInst *I) {
	if (I->getNumArgOperands() != 1)
		return false;
	Value *R = I->getArgOperand(0);
	IntegerType *T = dyn_cast<IntegerType>(R->getType());
	if (!T || I->getType() != T)
		return false;
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(T->getBitWidth()));
	Value *V = Builder->CreateICmpEQ(R, SMin);
	insert(V, I->getCalledFunction()->getName());
	Value *IsNeg = Builder->CreateICmpSLT(R, ConstantInt::get(T, 0));
	Value *Abs = Builder->CreateSelect(IsNeg, Builder->CreateNeg(R), R);
	I->replaceAllUsesWith(Abs);
	return true;
}

bool BugOnLibc::visitCtz(CallInst *I) {
	if (I->getNumArgOperands() != 2)
		return false;
	Value *X = I->getArgOperand(0);
	// Skip vectors for now.
	if (!X->getType()->isIntegerTy())
		return false;
	// Test if zero is undef.
	ConstantInt *IsZeroUndef = dyn_cast<ConstantInt>(I->getArgOperand(1));
	if (!IsZeroUndef || IsZeroUndef->isZero())
		return false;
	Value *V = createIsZero(X);
	StringRef Name = I->getCalledFunction()->getName();
	if (Name.startswith("llvm."))
		Name = Name.substr(5);
	Name = Name.split('.').first;
	return insert(V, Name);
}

// div(numer, denom): denom == 0 || (numer == INT_MIN && denom == -1)
bool BugOnLibc::visitDiv(CallInst *I) {
	if (I->getNumArgOperands() != 2)
		return false;
	Value *L = I->getArgOperand(0);
	Value *R = I->getArgOperand(1);
	IntegerType *T = dyn_cast<IntegerType>(I->getType());
	if (!T)
		return false;
	if (T != L->getType() || T != R->getType())
		return false;
	StringRef Name = I->getCalledFunction()->getName();
	insert(createIsZero(R), Name);
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(T->getBitWidth()));
	Constant *MinusOne = Constant::getAllOnesValue(T);
	Value *V = createAnd(
		Builder->CreateICmpEQ(L, SMin),
		Builder->CreateICmpEQ(R, MinusOne)
	);
	insert(V, Name);
	Value *SDiv = Builder->CreateSDiv(L, R);
	I->replaceAllUsesWith(SDiv);
	return true;
}

// memcpy(a, b, n): a-b<n || b-a<n
bool BugOnLibc::visitMemcpy(CallInst *I) {
	if (I->getNumArgOperands() < 3)
		return false;
	Value *A = I->getArgOperand(0);
	Value *B = I->getArgOperand(1);
	Value *N = I->getArgOperand(2);
	Value *AN = Builder->CreatePointerCast(A, N->getType());
	Value *BN = Builder->CreatePointerCast(B, N->getType());
	Value *AminusB = Builder->CreateSub(AN, BN);
	Value *BminusA = Builder->CreateSub(BN, AN);

	/*
	 * Allow memcpy(a, a, n), for two reasons:
	 * - in practice, memcpy implementations don't get this wrong, and
	 * - Clang generates memcpy instrinsic calls for self-assignment.
	 */
	insert(createAnd(
			Builder->CreateICmpULT(AminusB, N),
			Builder->CreateICmpNE(AN, BN)
	       ), "memcpy");
	insert(createAnd(
			Builder->CreateICmpULT(BminusA, N),
			Builder->CreateICmpNE(AN, BN)
	       ), "memcpy");
	return true;
}

char BugOnLibc::ID;

static RegisterPass<BugOnLibc>
X("bugon-libc", "Insert bugon calls for libc functions");
