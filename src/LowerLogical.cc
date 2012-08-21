// Consider the comparision.
//
//   x * 2 > y && x > UMAX / 2
//
// -simplifycfg may merge the two conditions using a bitwise OR.
//
//   %mul = %x, 2
//   %cmp0 = icmp ugt %mul, %y
//   %cmp1 = icmp ugt %x, ...
//   %or.cond = or %cmp0, %cmp1
//   br %or.cond, ...
//
// This leads to a false overflow error on x * 2, even the original code were
//
//   x > UMAX / 2 && x * 2 > y
//
// This pass performs the following transformations:
//
//   1) split a bitwise operation to several conditional branches;
//   2) carefully arrange the order of these suboperations.

#define DEBUG_TYPE "lower-logical"
#include <llvm/Function.h>
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Local.h>

using namespace llvm;

namespace {

struct LowerLogical : FunctionPass {
	static char ID;
	LowerLogical() : FunctionPass(ID) {}

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;
	SmallVector<std::pair<unsigned, Value *>, 4> Conds;

	void collectConditions(Instruction *);
	void rewriteAnd(TerminatorInst *);
	void rewriteOr(TerminatorInst *);
};

} // anonymous namespace

bool LowerLogical::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	bool Changed = false;
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
		BranchInst *BI = dyn_cast<BranchInst>(i->getTerminator());
		if (!BI || !BI->isConditional())
			continue;
		BinaryOperator *BO = dyn_cast<BinaryOperator>(BI->getCondition());
		if (!BO)
			continue;
		unsigned Opcode = BO->getOpcode();
		switch (Opcode) {
		default: continue;
		case Instruction::And:
		case Instruction::Or:
			break;
		}
		// Collect sub-conditions.
		collectConditions(BO);
		// Sort.
		std::sort(Conds.begin(), Conds.end());
		// Rewrite.
		Builder->SetInsertPoint(BI);
		if (Opcode == Instruction::And)
			rewriteAnd(BI);
		else
			rewriteOr(BI);
		RecursivelyDeleteTriviallyDeadInstructions(BO);
		Changed = true;
	}
	return Changed;
}

static unsigned rank(Value *V) {
	if (CmpInst *I = dyn_cast<CmpInst>(V))
		return rank(I->getOperand(0)) + rank(I->getOperand(1));
	if (CastInst *I = dyn_cast<CastInst>(V))
		return rank(I->getOperand(0));
	if (BinaryOperator *I = dyn_cast<BinaryOperator>(V)) {
		unsigned Val = rank(I->getOperand(0)) + rank(I->getOperand(1));
		unsigned Opcode = I->getOpcode();
		if (Opcode >= Instruction::Add && Opcode <= Instruction::FRem)
			++Val;
		return Val;
	}
	if (SelectInst *I = dyn_cast<SelectInst>(V))
		return rank(I->getTrueValue()) + rank(I->getFalseValue());
	return 0;
}

void LowerLogical::collectConditions(Instruction *I) {
	unsigned Opcode = I->getOpcode();
	for (unsigned i = 0, n = I->getNumOperands(); i != n; ++i) {
		Value *V = I->getOperand(i);
		BinaryOperator *BO = dyn_cast<BinaryOperator>(V);
		if (BO && BO->getOpcode() == Opcode)
			collectConditions(I);
		else
			Conds.push_back(std::make_pair(rank(V), V));
	}
}

void LowerLogical::rewriteAnd(TerminatorInst *TI) {
	BasicBlock *BB = TI->getParent();
	BasicBlock *TrueBB = TI->getSuccessor(0);
	BasicBlock *FalseBB = SplitEdge(BB, TI->getSuccessor(1), this);
	assert(!isa<PHINode>(FalseBB->front()));
	Function *F = BB->getParent();
	LLVMContext &VMCtx = Builder->getContext();
	for (unsigned i = 0, n = Conds.size(); i != n - 1; ++i) {
		Value *V = Conds[i].second;
		const Twine &Name = "bb." + V->getName() + ".true";
		BasicBlock *NewBB = BasicBlock::Create(VMCtx, Name, F, FalseBB);
		Builder->CreateCondBr(V, NewBB, FalseBB);
		Builder->SetInsertPoint(NewBB);
		Builder->SetCurrentDebugLocation(TI->getDebugLoc());
	}
	Builder->CreateCondBr(Conds.back().second, TrueBB, FalseBB);
	TI->eraseFromParent();
	// BB no long connects to TrueBB; update phi nodes in TrueBB.
	BasicBlock *NewPred = Builder->GetInsertBlock();
	for (BasicBlock::iterator i = TrueBB->begin(), e = TrueBB->end(); i != e; ++i) {
		PHINode *PN = dyn_cast<PHINode>(i);
		if (!PN)
			break;
		int Idx;
		while ((Idx = PN->getBasicBlockIndex(BB)) >= 0)
			PN->setIncomingBlock(Idx, NewPred);
	}
	Conds.clear();
}

void LowerLogical::rewriteOr(TerminatorInst *TI) {
	BasicBlock *BB = TI->getParent();
	BasicBlock *TrueBB = SplitEdge(BB, TI->getSuccessor(0), this);
	assert(!isa<PHINode>(TrueBB->front()));
	BasicBlock *FalseBB = TI->getSuccessor(1);
	Function *F = BB->getParent();
	LLVMContext &VMCtx = Builder->getContext();
	for (unsigned i = 0, n = Conds.size(); i != n - 1; ++i) {
		Value *V = Conds[i].second;
		const Twine &Name = "bb." + V->getName() + ".false";
		BasicBlock *NewBB = BasicBlock::Create(VMCtx, Name, F, TrueBB);
		Builder->CreateCondBr(V, TrueBB, NewBB);
		Builder->SetInsertPoint(NewBB);
		Builder->SetCurrentDebugLocation(TI->getDebugLoc());
	}
	Builder->CreateCondBr(Conds.back().second, TrueBB, FalseBB);
	TI->eraseFromParent();
	// BB no long connects to FalseBB; update phi nodes in FalseBB.
	BasicBlock *NewPred = Builder->GetInsertBlock();
	for (BasicBlock::iterator i = FalseBB->begin(), e = FalseBB->end(); i != e; ++i) {
		PHINode *PN = dyn_cast<PHINode>(i);
		if (!PN)
			break;
		int Idx;
		while ((Idx = PN->getBasicBlockIndex(BB)) >= 0)
			PN->setIncomingBlock(Idx, NewPred);
	}
	Conds.clear();
}

char LowerLogical::ID;

static RegisterPass<LowerLogical>
X("lower-logical", "Lower bitwise and/or into branches");
