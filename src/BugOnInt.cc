#define DEBUG_TYPE "bugon-int"
#include "BugOn.h"
#include <llvm/Function.h>
#include <llvm/Intrinsics.h>

using namespace llvm;

namespace {

struct BugOnInt : BugOnPass {
	static char ID;
	BugOnInt() : BugOnPass(ID) {}
	virtual bool visit(Instruction *);
private:
	bool visitOverflowingOperator(Intrinsic::ID, IntegerType *, Value *L, Value *R, const char *Bug);
	bool visitShiftOperator(IntegerType *, Value *R, const char *Bug);
	bool visitSignedDivisionOperator(IntegerType *, Value *L, Value *R, const char *Bug);
};

} // anonymous nmaespace

bool BugOnInt::visit(Instruction *I) {
	BinaryOperator *BO = dyn_cast<BinaryOperator>(I);
	if (!BO)
		return false;
	IntegerType *T = dyn_cast<IntegerType>(I->getType());
	if (!T)
		return false;
	Value *L = I->getOperand(0);
	Value *R = I->getOperand(1);
	bool Changed = false;
	switch (BO->getOpcode()) {
	default: break;
	case Instruction::Add:
		if (BO->hasNoSignedWrap())
			Changed |= visitOverflowingOperator(Intrinsic::sadd_with_overflow,
					T, L, R, "signed addition overflow");
		if (BO->hasNoUnsignedWrap())
			Changed |= visitOverflowingOperator(Intrinsic::uadd_with_overflow,
					T, L, R, "unsigned addition overflow");
		break;
	case Instruction::Sub:
		if (BO->hasNoSignedWrap())
			Changed |= visitOverflowingOperator(Intrinsic::ssub_with_overflow,
					T, L, R, "signed subtraction overflow");
		if (BO->hasNoUnsignedWrap())
			Changed |= visitOverflowingOperator(Intrinsic::usub_with_overflow,
					T, L, R, "unsigned subtraction overflow");
		break;
	case Instruction::Mul:
		if (BO->hasNoSignedWrap())
			Changed |= visitOverflowingOperator(Intrinsic::smul_with_overflow,
					T, L, R, "signed multiplication overflow");
		if (BO->hasNoUnsignedWrap())
			Changed |= visitOverflowingOperator(Intrinsic::umul_with_overflow,
					T, L, R, "unsigned multiplication overflow");
		break;
	case Instruction::SDiv:
	case Instruction::SRem:
		Changed |= visitSignedDivisionOperator(T, L, R, "signed division overflow");
		// Fall through.
	case Instruction::UDiv:
	case Instruction::URem:
		Changed |= insert(Builder->CreateIsNull(R), "division by zero");
		break;
	case Instruction::Shl:
		Changed |= visitShiftOperator(T, R, "shift left overflow");
		if (BO->hasNoSignedWrap()) {
			Value *V = Builder->CreateICmpNE(L, Builder->CreateAShr(Builder->CreateShl(L, R), R));
			Changed |= insert(V, "signed shift left overflow");
		}
		if (BO->hasNoUnsignedWrap()) {
			Value *V = Builder->CreateICmpNE(L, Builder->CreateLShr(Builder->CreateShl(L, R), R));
			Changed |= insert(V, "unsigned shift left overflow");
		}
		break;
	case Instruction::LShr:
		Changed |= visitShiftOperator(T, R, "logical shift right overflow");
		break;
	case Instruction::AShr:
		Changed |= visitShiftOperator(T, R, "arithmetic shift right overflow");
		break;
	}
	return Changed;
}

bool BugOnInt::visitOverflowingOperator(Intrinsic::ID ID, IntegerType *T, Value *L, Value *R, const char *Bug) {
	Function *F = Intrinsic::getDeclaration(getModule(), ID, T);
	Value *V = Builder->CreateExtractValue(Builder->CreateCall2(F, L, R), 1);
	return insert(V, Bug);
}

bool BugOnInt::visitShiftOperator(IntegerType *T, Value *R, const char *Bug) {
	Constant *BitWidth = ConstantInt::get(T, T->getBitWidth());
	Value *V = Builder->CreateICmpUGE(R, BitWidth);
	return insert(V, Bug);
}

bool BugOnInt::visitSignedDivisionOperator(IntegerType *T, Value *L, Value *R, const char *Bug) {
	unsigned n = T->getBitWidth();
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(n));
	Constant *MinusOne = Constant::getAllOnesValue(T);
	Value *V = Builder->CreateAnd(
		Builder->CreateICmpEQ(L, SMin),
		Builder->CreateICmpEQ(R, MinusOne));
	return insert(V, Bug);
}

char BugOnInt::ID;

static RegisterPass<BugOnInt>
X("bugon-int", "Insert bugon calls for integer operations");
