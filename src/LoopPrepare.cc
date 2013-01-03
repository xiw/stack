// This pass replaces some operations with SCEV-friendly alternatives.
// It should run before -loop-rotate.

#define DEBUG_TYPE "loop-prepare"
#include <llvm/Pass.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct LoopPrepare : FunctionPass {
	static char ID;
	LoopPrepare() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	bool visitShl(BinaryOperator *);
};

} // anonymous namespace

bool LoopPrepare::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		BinaryOperator *I = dyn_cast<BinaryOperator>(&*i++);
		if (!I || !I->getType()->isIntegerTy())
			continue;
		if (I->getOpcode() == Instruction::Shl) {
			Builder->SetInsertPoint(I);
			Changed |= visitShl(I);
		}
	}
	return Changed;
}

// (L << R) => (L * 2^R)
bool LoopPrepare::visitShl(BinaryOperator *I) {
	IntegerType *T = cast<IntegerType>(I->getType());
	Value *L = I->getOperand(0);
	Value *R = I->getOperand(1);
	Value *Power = Builder->CreateShl(ConstantInt::get(T, 1), R);
	bool hasNSW = I->hasNoSignedWrap();
	bool hasNUW = I->hasNoUnsignedWrap();
	Value *Mul = Builder->CreateMul(L, Power, "", hasNUW, hasNSW);
	I->replaceAllUsesWith(Mul);
	if (I->hasName())
		Mul->takeName(I);
	return true;
}

char LoopPrepare::ID;

static RegisterPass<LoopPrepare>
X("loop-prepare", "Optimize for loop-friendly operations");
