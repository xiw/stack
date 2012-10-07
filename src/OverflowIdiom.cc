// This pass recognizes overflow checking idioms and rewrites them
// with overflow intrinsics.

#define DEBUG_TYPE "overflow-idiom"
#include <llvm/Constants.h>
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/IntrinsicInst.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/PatternMatch.h>
#include <llvm/Transforms/Utils/Local.h>

using namespace llvm;
using namespace llvm::PatternMatch;

namespace {

struct OverflowIdiom : FunctionPass {
	static char ID;
	OverflowIdiom() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	Value *createOverflowBit(Intrinsic::ID ID, Value *L, Value *R) {
		Module *M = Builder->GetInsertBlock()->getParent()->getParent();
		Function *F = Intrinsic::getDeclaration(M, ID, L->getType());
		CallInst *CI = Builder->CreateCall2(F, L, R);
		return Builder->CreateExtractValue(CI, 1);
	}

	Value *matchCmpWithSwappedAndInverse(CmpInst::Predicate Pred, Value *L, Value *R) {
		// Try match comparision and the swapped form.
		if (Value *V = matchCmpWithInverse(Pred, L, R))
			return V;
		Pred = CmpInst::getSwappedPredicate(Pred);
		if (Value *V = matchCmpWithInverse(Pred, R, L))
			return V;
		return NULL;
	}

	Value *matchCmpWithInverse(CmpInst::Predicate Pred, Value *L, Value *R) {
		// Try match comparision and the inverse form.
		if (Value *V = matchCmp(Pred, L, R))
			return V;
		Pred = CmpInst::getInversePredicate(Pred);
		if (Value *V = matchCmp(Pred, L, R))
			return Builder->CreateXor(V, 1);
		return NULL;
	}

	Value *matchCmp(CmpInst::Predicate, Value *, Value *);

	bool removeRedundantZeroCheck(BasicBlock *);
};

} // anonymous namespace

bool OverflowIdiom::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		ICmpInst *I = dyn_cast<ICmpInst>(&*i);
		++i;
		if (!I)
			continue;
		Value *L = I->getOperand(0), *R = I->getOperand(1);
		if (!L->getType()->isIntegerTy())
			continue;
		Builder->SetInsertPoint(I);
		Value *V = matchCmpWithSwappedAndInverse(I->getPredicate(), L, R);
		if (!V)
			continue;
		I->replaceAllUsesWith(V);
		if (I->hasName())
			V->takeName(I);
		RecursivelyDeleteTriviallyDeadInstructions(I);
		Changed = true;
	}
	return Changed;
}

Value *OverflowIdiom::matchCmp(CmpInst::Predicate Pred, Value *L, Value *R) {
	Value *X, *Y, *A, *B;
	ConstantInt *C;

	// x != (x * y) /u y => umul.overflow(x, y)
	// x != (y * x) /u y => umul.overflow(x, y)
	if (Pred == CmpInst::ICMP_NE
	    && match(L, m_Value(X))
	    && match(R, m_UDiv(m_Mul(m_Value(A), m_Value(B)), m_Value(Y)))
	    && ((A == X && B == Y) || (A == Y && B == X)))
		return createOverflowBit(Intrinsic::umul_with_overflow, X, Y);

	// x >u C /u y =>
	//    umul.overflow(x, y),                       if C == UMAX
	//    x >u C || y >u C || umul.overflow_k(x, y), if C == UMAX_k
	//    (zext x) * (zext y) > (zext N),            otherwise.
	if (Pred == CmpInst::ICMP_UGT
	    && match(L, m_Value(X))
	    && match(R, m_UDiv(m_ConstantInt(C), m_Value(Y)))) {
		const APInt &Val = C->getValue();
		if (Val.isMaxValue())
			return createOverflowBit(Intrinsic::umul_with_overflow, X, Y);
		if ((Val + 1).isPowerOf2()) {
			unsigned N = Val.countTrailingOnes();
			IntegerType *T = IntegerType::get(Builder->getContext(), N);
			return Builder->CreateOr(
				Builder->CreateOr(
					Builder->CreateICmpUGT(X, C),
					Builder->CreateICmpUGT(Y, C)
				),
				createOverflowBit(Intrinsic::umul_with_overflow,
					Builder->CreateTrunc(X, T),
					Builder->CreateTrunc(Y, T)
				)
			);
		}
		unsigned N = X->getType()->getIntegerBitWidth() * 2;
		IntegerType *T = IntegerType::get(Builder->getContext(), N);
		return Builder->CreateICmpUGT(
			Builder->CreateMul(
				Builder->CreateZExt(X, T),
				Builder->CreateZExt(Y, T)
			),
			Builder->CreateZExt(C, T)
		);
	}

	return NULL;
}

char OverflowIdiom::ID;

static RegisterPass<OverflowIdiom>
X("overflow-idiom", "Rewrite overflow checking idioms using intrinsics");
