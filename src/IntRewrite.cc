#define DEBUG_TYPE "int-rewrite"
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

namespace {

struct IntRewrite : ModulePass {
	static char ID;
	IntRewrite() : ModulePass(ID) { }

	virtual bool runOnModule(Module &M) {
		LLVMContext &VMCtx = M.getContext();
		Builder.reset(new IRBuilder<>(VMCtx));
		MD_int = VMCtx.getMDKindID("int");
		for (Module::iterator i = M.begin(), e = M.end(); i != e; ++i)
			rewrite(i);
		return true;
	}

private:
	OwningPtr<IRBuilder<> > Builder;
	unsigned MD_int;

	void rewrite(Function *);

	Value *insertOverflowCheck(Instruction *, Intrinsic::ID, Intrinsic::ID);
	Value *insertSDivCheck(Instruction *);
	Value *insertShiftCheck(Value *);

	void insertTrap(Value *V, Instruction *IP);
};

} // anonymous namespace

void IntRewrite::rewrite(Function *F) {
	SmallVector<std::pair<Value *, Instruction *>, 16> Checks;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		if (!isa<BinaryOperator>(I))
			continue;
		Builder->SetInsertPoint(I);
		Value *V = NULL;
		switch (I->getOpcode()) {
		default: continue;
		case Instruction::Add:
			V = insertOverflowCheck(I,
				Intrinsic::sadd_with_overflow,
				Intrinsic::uadd_with_overflow);
			break;
		case Instruction::Sub:
			V = insertOverflowCheck(I,
				Intrinsic::ssub_with_overflow,
				Intrinsic::usub_with_overflow);
			break;
		case Instruction::Mul:
			V = insertOverflowCheck(I,
				Intrinsic::smul_with_overflow,
				Intrinsic::umul_with_overflow);
			break;
		case Instruction::SDiv:
			V = insertSDivCheck(I);
			break;
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
			V = insertShiftCheck(I->getOperand(1));
			break;
		}
		Checks.push_back(std::make_pair(V, I));
	}
	// Since inserting trap will change the control flow, it's better
	// to do it after looping over all instructions.
	for (size_t i = 0, n = Checks.size(); i != n; ++i)
		insertTrap(Checks[i].first, Checks[i].second);
}

Value *IntRewrite::insertOverflowCheck(Instruction *I, Intrinsic::ID SID, Intrinsic::ID UID) {
	BinaryOperator *BO = cast<BinaryOperator>(I);
	Intrinsic::ID ID = BO->hasNoSignedWrap()? SID: UID;
	Module *M = I->getParent()->getParent()->getParent();
	Function *F = Intrinsic::getDeclaration(M, ID, BO->getType());
	CallInst *CI = Builder->CreateCall2(F, BO->getOperand(0), BO->getOperand(1));
	return Builder->CreateExtractValue(CI, 1);
}

Value *IntRewrite::insertSDivCheck(Instruction *I) {
	Value *L = I->getOperand(0), *R = I->getOperand(1);
	IntegerType *T = cast<IntegerType>(I->getType());
	unsigned n = T->getBitWidth();
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(n));
	Constant *MinusOne = ConstantInt::get(T, APInt::getAllOnesValue(n));
	return Builder->CreateAnd(
		Builder->CreateICmpEQ(L, SMin),
		Builder->CreateICmpEQ(R, MinusOne)
	);
}

Value *IntRewrite::insertShiftCheck(Value *V) {
	IntegerType *T = cast<IntegerType>(V->getType());
	Constant *C = ConstantInt::get(T, T->getBitWidth());
	return Builder->CreateICmpUGE(V, C);
}

void IntRewrite::insertTrap(Value *V, Instruction *IP) {
	BasicBlock *Pred = IP->getParent();
	BasicBlock *Succ = SplitBlock(Pred, IP, this);
	// Create a new BB containing a trap instruction.
	LLVMContext &VMCtx = V->getContext();
	Function *F = Pred->getParent();
	BasicBlock *BB = BasicBlock::Create(VMCtx, "", F, Succ);
	Builder->SetInsertPoint(BB);
	Builder->SetCurrentDebugLocation(IP->getDebugLoc());
	Module *M = F->getParent();
	Function *Trap = Intrinsic::getDeclaration(M, Intrinsic::debugtrap);
	CallInst *CI = Builder->CreateCall(Trap);
	// Embed operation name in metadata.
	std::string Anno = IP->getOpcodeName();
	switch (IP->getOpcode()) {
	case Instruction::Add:
	case Instruction::Sub:
	case Instruction::Mul:
		Anno = cast<BinaryOperator>(IP)->hasNoSignedWrap() ? "s" : "u" + Anno;
	}
	MDNode *MD = MDNode::get(VMCtx, MDString::get(VMCtx, Anno));
	CI->setMetadata(MD_int, MD);
	Builder->CreateBr(Succ);
	// Create a new conditional br in Pred.
	Pred->getTerminator()->eraseFromParent();
	BranchInst::Create(BB, Succ, V, Pred);
}

char IntRewrite::ID;

static RegisterPass<IntRewrite>
X("int-rewrite", "Insert llvm.debugtrap calls", false, false);
