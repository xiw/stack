#define DEBUG_TYPE "overflow-simplify"
#include <llvm/Constants.h>
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
	bool simplify(bool Swapped, Value *, ConstantInt *, IntrinsicInst *);
	bool canonicalize(Value *, Value *, IntrinsicInst *);
};

} // anonymous namespace

bool OverflowSimplify::runOnFunction(Function &F) {
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		IntrinsicInst *I = dyn_cast<IntrinsicInst>(&*i);
		++i;
		if (!I || I->getNumArgOperands() != 2)
			continue;
		Value *LHS = I->getArgOperand(0), *RHS = I->getArgOperand(1);
		if (ConstantInt *C = dyn_cast<ConstantInt>(LHS))
			Changed |= simplify(true, RHS, C, I);
		else if (ConstantInt *C = dyn_cast<ConstantInt>(RHS))
			Changed |= simplify(false, LHS, C, I);
		else
			Changed |= canonicalize(LHS, RHS, I);
	}
	return Changed;
}

bool OverflowSimplify::simplify(bool Swapped, Value *V, ConstantInt *C, IntrinsicInst *I) {
	unsigned numBits = C->getBitWidth();
	LLVMContext &VMCtx = V->getContext();
	Value *Res, *Cmp;
	switch (I->getIntrinsicID()) {
	default: return false;
	// TODO: other overflow intrinsics.
	case Intrinsic::uadd_with_overflow:
		Res = BinaryOperator::Create(BinaryOperator::Add, V, C, "", I);
		Cmp = new ICmpInst(I, CmpInst::ICMP_UGT, V, ConstantInt::get(VMCtx,
			APInt::getMaxValue(numBits) - C->getValue()));
		break;
	case Intrinsic::usub_with_overflow:
		Res = BinaryOperator::Create(BinaryOperator::Sub, V, C, "", I);
		Cmp = new ICmpInst(I, (Swapped ? CmpInst::ICMP_UGT : CmpInst::ICMP_ULT), V, C);
		break;
	case Intrinsic::umul_with_overflow:
		Res = BinaryOperator::Create(BinaryOperator::Mul, V, C, "", I);
		Cmp = new ICmpInst(I, CmpInst::ICMP_UGT, V, ConstantInt::get(VMCtx,
			APInt::getMaxValue(numBits).udiv(C->getValue())));
		break;
	}
	Type *RetTy = StructType::get(V->getType(), Type::getInt1Ty(VMCtx), NULL);
	Value *RetV = UndefValue::get(RetTy);
	RetV = InsertValueInst::Create(RetV, Res, 0, "", I);
	Instruction *NewInst = InsertValueInst::Create(RetV, Cmp, 1, "", I);
	NewInst->setDebugLoc(I->getDebugLoc());
	I->replaceAllUsesWith(NewInst);
	I->eraseFromParent();
	return true;
}

bool OverflowSimplify::canonicalize(Value *L, Value *R, IntrinsicInst *I) {
	switch (I->getIntrinsicID()) {
	default: return false;
	case Intrinsic::sadd_with_overflow:
	case Intrinsic::uadd_with_overflow:
	case Intrinsic::smul_with_overflow:
	case Intrinsic::umul_with_overflow:
		break;
	}
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

char OverflowSimplify::ID;

static RegisterPass<OverflowSimplify>
X("overflow-simplify", "Canonicalize overflow intrinsics");
