// This pass tries to move bugon calls out of loops.  It should run
// after other bugon passes.

#define DEBUG_TYPE "bugon-loop"
#include "BugOn.h"
#include <llvm/InstVisitor.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpander.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>

using namespace llvm;

namespace {
	
struct BugOnLoop : BugOnPass, InstVisitor<BugOnLoop, Value *> {
	static char ID;
	BugOnLoop() : BugOnPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeLoopInfoPass(Registry);
		initializeScalarEvolutionPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		super::getAnalysisUsage(AU);
		AU.addRequired<LoopInfo>();
		AU.addRequired<ScalarEvolution>();
	}

	virtual bool runOnFunction(Function &);

	virtual bool runOnInstruction(Instruction *);

	// InstVisitor.
	Value *visitInstruction(Instruction &);
	Value *visitBinaryOperator(BinaryOperator &);
	Value *visitICmpInst(ICmpInst &);
	Value *visitSelectInst(SelectInst &);
	Value *visitExtractValueInst(ExtractValueInst &);

private:
	LoopInfo *LI;
	ScalarEvolution *SE;
	const Loop *Scope;

	Value *computeValueAtScope(Value *V, const Loop *L) {
		Scope = L;
		return compute(V);
	}

	Value *compute(Value *V) {
		if (Instruction *I = dyn_cast<Instruction>(V))
			return visit(I);
		if (isa<Constant>(V))
			return V;
		return NULL;
	}
};

} // anonymous namespace

bool BugOnLoop::runOnFunction(Function &F) {
	LI = &getAnalysis<LoopInfo>();
        SE = &getAnalysis<ScalarEvolution>();
        return super::runOnFunction(F);
}

bool BugOnLoop::runOnInstruction(Instruction *I) {
	BugOnInst *BOI = dyn_cast<BugOnInst>(I);
	if (!BOI)
		return false;
	const Loop *L = LI->getLoopFor(I->getParent());
	if (!L)
		return false;
	BasicBlock *ExitBlock = L->getExitBlock();
	if (!ExitBlock)
		return false;
	Instruction *IP = setInsertPoint(ExitBlock->getFirstInsertionPt());
	// We could use SCEV's computeSCEVAtScope.  The problem is that SCEV
	// cannot represent many instructions (e.g., comparisons and overflow
	// intrinsics).  So just do it ourselves and only use SCEV for values
	// we cannot deal with (e.g., phi).
	Value *ExitValue = computeValueAtScope(BOI->getCondition(), L->getParentLoop());
	if (ExitValue)
		insert(ExitValue, BOI->getAnnotation(), BOI->getDebugLoc());
	setInsertPoint(IP);
	return true;
}

Value *BugOnLoop::visitInstruction(Instruction &I) {
	const SCEV *S = SE->getSCEVAtScope(&I, Scope);
	if (!SE->isLoopInvariant(S, Scope))
		return NULL;
	SCEVExpander Expander(*SE, "");
	return Expander.expandCodeFor(S, I.getType(), Builder->GetInsertPoint());
}

Value *BugOnLoop::visitBinaryOperator(BinaryOperator &I) {
	Value *L = compute(I.getOperand(0));
	if (!L)
		return NULL;
	Value *R = compute(I.getOperand(1));
	if (!R)
		return NULL;
	return Builder->CreateBinOp(I.getOpcode(), L, R);
}

Value *BugOnLoop::visitICmpInst(ICmpInst &I) {
	Value *L = compute(I.getOperand(0));
	if (!L)
		return NULL;
	Value *R = compute(I.getOperand(1));
	if (!R)
		return NULL;
	return Builder->CreateICmp(I.getPredicate(), L, R);
}

Value *BugOnLoop::visitSelectInst(SelectInst &I) {
	Value *Cond = compute(I.getCondition());
	if (!Cond)
		return NULL;
	Value *TrueVal = compute(I.getTrueValue());
	if (!TrueVal)
		return NULL;
	Value *FalseVal = compute(I.getFalseValue());
	if (!FalseVal)
		return NULL;
	return Builder->CreateSelect(Cond, TrueVal, FalseVal);
}

Value *BugOnLoop::visitExtractValueInst(ExtractValueInst &I) {
	if (I.getNumIndices() != 1)
		return NULL;
	IntrinsicInst *II = dyn_cast<IntrinsicInst>(I.getAggregateOperand());
	if (!II || II->getCalledFunction()->getName().find(".with.overflow.") == StringRef::npos)
		return NULL;
	Value *L = compute(II->getArgOperand(0));
	if (!L)
		return NULL;
	Value *R = compute(II->getArgOperand(1));
	if (!R)
		return NULL;
	switch (I.getIndices()[0]) {
	default: II->dump(); assert(0 && "Unknown overflow!");
	case 0:
		switch (II->getIntrinsicID()) {
		default: II->dump(); assert(0 && "Unknown overflow!");
		case Intrinsic::sadd_with_overflow:
		case Intrinsic::uadd_with_overflow:
			return Builder->CreateAdd(L, R);
		case Intrinsic::ssub_with_overflow:
		case Intrinsic::usub_with_overflow:
			return Builder->CreateSub(L, R);
		case Intrinsic::smul_with_overflow:
		case Intrinsic::umul_with_overflow:
			return Builder->CreateMul(L, R);
		}
	case 1:
		switch (II->getIntrinsicID()) {
		default: II->dump(); assert(0 && "Unknown overflow!");
		case Intrinsic::sadd_with_overflow:
			return createIsSAddWrap(L, R);
		case Intrinsic::uadd_with_overflow:
			return createIsUAddWrap(L, R);
		case Intrinsic::ssub_with_overflow:
			return createIsSSubWrap(L, R);
		case Intrinsic::usub_with_overflow:
			return createIsUSubWrap(L, R);
		case Intrinsic::smul_with_overflow:
			return createIsSMulWrap(L, R);
		case Intrinsic::umul_with_overflow:
			return createIsUMulWrap(L, R);
		}
	}
	assert(I.getIndices()[0] == 1 && "FIXME!");
}

char BugOnLoop::ID;

static RegisterPass<BugOnLoop>
X("bugon-loop", "Insert bugon calls for loop exiting values");
