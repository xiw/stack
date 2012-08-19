#define DEBUG_TYPE "overflow-combine"
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/IntrinsicInst.h>
#include <llvm/Pass.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct OverflowCombine : FunctionPass {
	static char ID;
	OverflowCombine() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		AU.addRequired<DominatorTree>();
	}

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;
	DominatorTree *DT;

	bool rewrite(unsigned Opcode, IntrinsicInst *II);
	bool rewriteRange(BasicBlock::iterator I, BasicBlock::iterator E, unsigned Opcode, IntrinsicInst *II);
};

} // anonymous namespace

bool OverflowCombine::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	DT = &getAnalysis<DominatorTree>();
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		IntrinsicInst *II = dyn_cast<IntrinsicInst>(&*i);
		if (!II || II->getNumArgOperands() != 2)
			continue;
		switch (II->getIntrinsicID()) {
		default: break;
		case Intrinsic::sadd_with_overflow:
		case Intrinsic::uadd_with_overflow:
			Changed |= rewrite(Instruction::Add, II);
			break;
		case Intrinsic::ssub_with_overflow:
		case Intrinsic::usub_with_overflow:
			Changed |= rewrite(Instruction::Sub, II);
			break;
		case Intrinsic::smul_with_overflow:
		case Intrinsic::umul_with_overflow:
			Changed |= rewrite(Instruction::Mul, II);
			break;
		}
	}
	return Changed;
}

bool OverflowCombine::rewrite(unsigned Opcode, IntrinsicInst *II) {
	bool Changed = false;
	// Check current BB.
	BasicBlock *BB = II->getParent();
	Changed |= rewriteRange(++BasicBlock::iterator(II), BB->end(), Opcode, II);
	// Check other BBs dominated by current BB.
	Function *F = BB->getParent();
	for (Function::iterator i = F->begin(), e = F->end(); i != e; ++i) {
		if (DT->properlyDominates(BB, i))
			Changed |= rewriteRange(i->begin(), i->end(), Opcode, II);
	}
	return Changed;
}

bool OverflowCombine::rewriteRange(BasicBlock::iterator I, BasicBlock::iterator E, unsigned Opcode, IntrinsicInst *II) {
	bool Changed = false;
	Value *L = II->getArgOperand(0), *R = II->getArgOperand(1);
	for (; I != E; ++I) {
		if (I->getOpcode() != Opcode)
			continue;
		Value *V0 = I->getOperand(0), *V1 = I->getOperand(1);
		if (!(V0 == L && V1 == R) && !(I->isCommutative() && V0 == R && V1 == L))
			continue;
		Builder->SetInsertPoint(I);
		Value *V = Builder->CreateExtractValue(II, 0);
		I->replaceAllUsesWith(V);
		if (I->hasName())
			V->takeName(I);
		Changed = true;
	}
	return Changed;
}


char OverflowCombine::ID;

static RegisterPass<OverflowCombine>
X("overflow-combine", "Combine overflow intrinsics and binary operators");
