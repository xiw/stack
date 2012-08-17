#define DEBUG_TYPE "overflow-simplify"
#include <llvm/Constants.h>
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/IntrinsicInst.h>
#include <llvm/Pass.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

namespace {

struct OverflowSimplify : FunctionPass {
	static char ID;
	OverflowSimplify() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	void replace(IntrinsicInst *I, Value *Res, Value *Cmp);

	bool simplifySAdd(IntrinsicInst *);
	bool simplifyUAdd(IntrinsicInst *);
	bool simplifySSub(IntrinsicInst *);
	bool simplifyUSub(IntrinsicInst *);
	bool simplifySMul(IntrinsicInst *);
	bool simplifyUMul(IntrinsicInst *);
};

} // anonymous namespace

bool OverflowSimplify::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		IntrinsicInst *I = dyn_cast<IntrinsicInst>(&*i);
		++i;
		if (!I || I->getNumArgOperands() != 2)
			continue;
		Builder->SetInsertPoint(I);
		switch (I->getIntrinsicID()) {
		default: break;
		case Intrinsic::sadd_with_overflow:
			Changed |= simplifySAdd(I);
			break;
		case Intrinsic::uadd_with_overflow:
			Changed |= simplifyUAdd(I);
			break;
		case Intrinsic::ssub_with_overflow:
			Changed |= simplifySSub(I);
			break;
		case Intrinsic::usub_with_overflow:
			Changed |= simplifyUSub(I);
			break;
		case Intrinsic::smul_with_overflow:
			Changed |= simplifySMul(I);
			break;
		case Intrinsic::umul_with_overflow:
			Changed |= simplifyUMul(I);
			break;
		}
	}
	return Changed;
}

static bool canonicalize(IntrinsicInst *I, Value **X, ConstantInt **C) {
	Value *L = I->getArgOperand(0), *R = I->getArgOperand(1);
	// We need a "stable" order of commutative operations.
	// Order based on their names.  If a variable doesn't have a name,
	// use its slot index.
	if (ConstantInt *CI = dyn_cast<ConstantInt>(R)) {
		*X = L;
		*C = CI;
		return false;
	}
	if (ConstantInt *CI = dyn_cast<ConstantInt>(L)) {
		*X = R;
		*C = CI;
		return false;
	}
	*X = NULL;
	*C = NULL;
	SmallString<16> LS, RS;
	{
		raw_svector_ostream OS(LS);
		WriteAsOperand(OS, L, false);
	}
	{
		raw_svector_ostream OS(RS);
		WriteAsOperand(OS, R, false);
	}
	if (LS <= RS)
		return false;
	I->setArgOperand(0, R);
	I->setArgOperand(1, L);
	return true;
}

void OverflowSimplify::replace(IntrinsicInst *I, Value *Res, Value *Cmp) {
	StructType *T = StructType::get(Res->getType(), Cmp->getType(), NULL);
	Value *V = UndefValue::get(T);
	V = Builder->CreateInsertValue(V, Res, 0);
	V = Builder->CreateInsertValue(V, Cmp, 1);
	I->replaceAllUsesWith(V);
	if (I->hasName())
		V->takeName(I);
	I->eraseFromParent();
}

bool OverflowSimplify::simplifySAdd(IntrinsicInst *I) {
	Value *X;
	ConstantInt *C;
	bool Changed = canonicalize(I, &X, &C);
	if (!C)
		return Changed;
	unsigned N = C->getBitWidth();
	Value *Res = Builder->CreateAdd(X, C);
	// sadd.overflow(X, C) =>
	//   X < SMIN - C, if C < 0
	//   X > SMAX - C, otherwise.
	Value *Cmp;
	if (C->isNegative())
		Cmp = Builder->CreateICmpSLT(X, Builder->getInt(
			APInt::getSignedMinValue(N) - C->getValue()));
	else
		Cmp = Builder->CreateICmpSGT(X, Builder->getInt(
			APInt::getSignedMaxValue(N) - C->getValue()));
	replace(I, Res, Cmp);
	return true;
}

bool OverflowSimplify::simplifyUAdd(IntrinsicInst *I) {
	Value *X;
	ConstantInt *C;
	bool Changed = canonicalize(I, &X, &C);
	if (!C)
		return Changed;
	// uadd.overflow(X, C) => X > UMAX - C.
	Value *Res = Builder->CreateAdd(X, C);
	Value *Cmp = Builder->CreateICmpUGT(X, Builder->getInt(
		APInt::getMaxValue(C->getBitWidth()) - C->getValue()));
	replace(I, Res, Cmp);
	return true;
}

bool OverflowSimplify::simplifySSub(IntrinsicInst *I) {
	Value *L = I->getArgOperand(0), *R = I->getArgOperand(1);
	Value *Res, *Cmp;
	unsigned N = L->getType()->getIntegerBitWidth();
	if (ConstantInt *C = dyn_cast<ConstantInt>(R)) {
		Res = Builder->CreateSub(L, R);
		// ssub.overflow(L, C) =>
		//    L > SMAX + C, if C < 0
		//    L < SMIN + C, otherwise.
		if (C->isNegative())
			Cmp = Builder->CreateICmpSGT(L, Builder->getInt(
				APInt::getSignedMaxValue(N) + C->getValue()));
		else
			Cmp = Builder->CreateICmpSLT(L, Builder->getInt(
				APInt::getSignedMinValue(N) + C->getValue()));
	} else if (ConstantInt *C = dyn_cast<ConstantInt>(L)) {
		Res = Builder->CreateSub(L, R);
		// ssub.overflow(C, R) =>
		//    R > C - SMIN, if C < 0
		//    R < C - SMAX, otherwise.
		if (C->isNegative())
			Cmp = Builder->CreateICmpSGT(L, Builder->getInt(
				C->getValue() - APInt::getSignedMinValue(N)));
		else
			Cmp = Builder->CreateICmpSLT(L, Builder->getInt(
				C->getValue() - APInt::getSignedMaxValue(N)));
	} else {
		return false;
	}
	replace(I, Res, Cmp);
	return true;
}

bool OverflowSimplify::simplifyUSub(IntrinsicInst *I) {
	// usub.overflow(L, R) => L < R.
	Value *L = I->getArgOperand(0), *R = I->getArgOperand(1);
	Value *Res = Builder->CreateSub(L, R);
	Value *Cmp;
	if (isa<Constant>(L))
		Cmp = Builder->CreateICmpUGT(R, L);
	else
		Cmp = Builder->CreateICmpULT(L, R);
	replace(I, Res, Cmp);
	return true;
}

bool OverflowSimplify::simplifySMul(IntrinsicInst *I) {
	Value *X;
	ConstantInt *C;
	bool Changed = canonicalize(I, &X, &C);
	if (!C)
		return Changed;
	Value *Res = Builder->CreateMul(X, C);
	Value *Cmp;
	// smul.overflow(X, C) =>
	//    false,                        if C == 0
	//    X < SMAX / C || X > SMIN / C, if C < 0
	//    X > SMAX / C || X < SMIN / C, otherwise
	if (C->isZero()) {
		Cmp = Builder->getFalse();
	} else {
		unsigned N = C->getBitWidth();
		Value *Max, *Min;
		Max = Builder->getInt(APInt::getSignedMaxValue(N).sdiv(C->getValue()));
		Min = Builder->getInt(APInt::getSignedMinValue(N).sdiv(C->getValue()));
		if (C->isNegative())
			Cmp = Builder->CreateOr(
				Builder->CreateICmpSLT(X, Max),
				Builder->CreateICmpSGT(X, Min)
			);
		else
			Cmp = Builder->CreateOr(
				Builder->CreateICmpSGT(X, Max),
				Builder->CreateICmpSLT(X, Min)
			);
	}
	replace(I, Res, Cmp);
	return true;
}

bool OverflowSimplify::simplifyUMul(IntrinsicInst *I) {
	Value *X;
	ConstantInt *C;
	bool Changed = canonicalize(I, &X, &C);
	if (!C)
		return Changed;
	// umul.overflow(X, C) =>
	//    false,        if C == 0
	//    X > UMAX / C, otherwise.
	Value *Res = Builder->CreateMul(X, C);
	Value *Cmp;
	if (C->isZero())
		Cmp = Builder->getFalse();
	else
		Cmp = Builder->CreateICmpUGT(X, Builder->getInt(
			APInt::getMaxValue(C->getBitWidth()).udiv(C->getValue())));
	replace(I, Res, Cmp);
	return true;
}

char OverflowSimplify::ID;

static RegisterPass<OverflowSimplify>
X("overflow-simplify", "Canonicalize overflow intrinsics");
