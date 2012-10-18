#include <llvm/Constants.h>
#include <llvm/DataLayout.h>
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Support/CallSite.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/InstVisitor.h>

using namespace llvm;

namespace {

struct UBBugOn : FunctionPass, InstVisitor<UBBugOn, bool> {
	friend class InstVisitor;
	static char ID;
	UBBugOn() : FunctionPass(ID), BugOn(NULL) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;
	Function *BugOn;
	DataLayout *TD;

	bool insertBugOn(Value *);

	bool visitInstruction(Instruction &) { return false; }
	bool visit(Instruction &);
	bool visitOverflowingOperator(BinaryOperator &, Intrinsic::ID, Intrinsic::ID);
	bool visitAdd(BinaryOperator &I) {
		return visitOverflowingOperator(I,
			Intrinsic::sadd_with_overflow,
			Intrinsic::uadd_with_overflow);
	}
	bool visitSub(BinaryOperator &I) {
		return visitOverflowingOperator(I,
			Intrinsic::ssub_with_overflow,
			Intrinsic::usub_with_overflow);
	}
	bool visitMul(BinaryOperator &I) {
		return visitOverflowingOperator(I,
			Intrinsic::smul_with_overflow,
			Intrinsic::umul_with_overflow);
	}
	bool visitUDiv(BinaryOperator &);
	bool visitURem(BinaryOperator &);
	bool visitSDiv(BinaryOperator &);
	bool visitSRem(BinaryOperator &);
	bool visitShiftOperator(BinaryOperator &);
	bool visitShl(BinaryOperator &);
	bool visitShrOperator(BinaryOperator &);
	bool visitLShr(BinaryOperator &I) { return visitShrOperator(I); }
	bool visitAShr(BinaryOperator &I) { return visitShrOperator(I); }

	bool visitLoadInst(LoadInst &);
	bool visitStoreInst(StoreInst &);
};

} // anonymous namespace

bool UBBugOn::runOnFunction(Function &F) {
	IRBuilder<> TheBuilder(F.getContext());
	Builder = &TheBuilder;
	TD = getAnalysisIfAvailable<DataLayout>();
	// Dispatch through visitor.
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i)
		Changed |= visit(*i);
	return Changed;
}

static bool isDeadArg(Instruction &I, unsigned Idx) {
	CallSite CS(&I);
	if (!CS)
		return false;
	Function *F = CS.getCalledFunction();
	if (!F || F->empty() || Idx >= F->arg_size())
		return false;
	Function::arg_iterator A = F->arg_begin();
	for (unsigned i = 0; i < Idx; ++i)
		++A;
	return A->use_empty();
}

bool UBBugOn::visit(Instruction &I) {
	// It's okay to have undef in phi's operands.
	// TODO: how to catch those undefs?
	if (isa<PHINode>(I))
		return false;
	if (isa<InsertValueInst>(I) || isa<InsertElementInst>(I))
		return false;
	if (isa<SelectInst>(I))
		return false;
	if (isa<TerminatorInst>(I))
		return false;
	// Insert bugon calls after instruction I since some may use
	// I itself (e.g., shift exact).
	Builder->SetInsertPoint(++BasicBlock::iterator(&I));
	// If any operand is undef, this instruction must not be reachable.
	for (unsigned i = 0, n = I.getNumOperands(); i != n; ++i) {
		Value *V = I.getOperand(i);
		if (isa<UndefValue>(V)) {
			// Allow undef arguments created by -deadargelim,
			// which are basically unused in the function body.
			if (isDeadArg(I, i))
				continue;
			return insertBugOn(ConstantInt::getTrue(I.getContext()));
		}
	}
	// Skip vector type for now.
	if (isa<BinaryOperator>(I))
		if (I.getType()->isVectorTy())
			return false;
	// Go through visitor.
	return InstVisitor::visit(I);
}

bool UBBugOn::insertBugOn(Value *V) {
	// Ignore bugon(false).
	if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
		if (CI->isZero())
			return false;
	}
	if (!BugOn) {
		Module *M = Builder->GetInsertBlock()->getParent()->getParent();
		Type *VoidTy = Builder->getVoidTy();
		Type *BoolTy = Builder->getInt1Ty();
		FunctionType *T = FunctionType::get(VoidTy, BoolTy, false);
		Constant *F = M->getOrInsertFunction("c.bugon", T);
		BugOn = cast<Function>(F);
		BugOn->setDoesNotAccessMemory();
		BugOn->setDoesNotThrow();
	}
	Builder->CreateCall(BugOn, V);
	return true;
}

bool UBBugOn::visitOverflowingOperator(BinaryOperator &I, Intrinsic::ID SID, Intrinsic::ID UID) {
	bool hasNSW = I.hasNoSignedWrap();
	bool hasNUW = I.hasNoUnsignedWrap();
	if (!hasNSW && !hasNUW)
		return false;
	Module *M = I.getParent()->getParent()->getParent();
	Type *T = I.getType();
	Value *L = I.getOperand(0);
	Value *R = I.getOperand(1);
	bool Changed = false;
	if (hasNSW) {
		Function *F = Intrinsic::getDeclaration(M, SID, T);
		Value *V = Builder->CreateExtractValue(Builder->CreateCall2(F, L, R), 1);
		Changed |= insertBugOn(V);
	}
	if (hasNUW) {
		Function *F = Intrinsic::getDeclaration(M, UID, T);
		Value *V = Builder->CreateExtractValue(Builder->CreateCall2(F, L, R), 1);
		Changed |= insertBugOn(V);
	}
	return Changed;
}

bool UBBugOn::visitUDiv(BinaryOperator &I) {
	bool Changed = visitURem(I);
	// FIXME: use mul or urem?
	if (I.isExact()) {
		Value *L = I.getOperand(0);
		Value *R = I.getOperand(1);
		Value *V = Builder->CreateICmpNE(L, Builder->CreateMul(&I, R));
		Changed |= insertBugOn(V);
	}
	return Changed;
}

bool UBBugOn::visitURem(BinaryOperator &I) {
	Value *R = I.getOperand(1);
	return insertBugOn(Builder->CreateIsNull(R));
}

bool UBBugOn::visitSDiv(BinaryOperator &I) {
	bool Changed = visitSRem(I);
	if (I.isExact()) {
		Value *L = I.getOperand(0);
		Value *R = I.getOperand(1);
		Value *V = Builder->CreateICmpNE(L, Builder->CreateMul(&I, R));
		Changed |= insertBugOn(V);
	}
	return Changed;
}

bool UBBugOn::visitSRem(BinaryOperator &I) {
	// INT_MIN % -1.
	IntegerType *T = cast<IntegerType>(I.getType());
	Value *L = I.getOperand(0);
	Value *R = I.getOperand(1);
	unsigned n = T->getBitWidth();
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(n));
	Constant *MinusOne = Constant::getAllOnesValue(T);
	Value *V = Builder->CreateAnd(
		Builder->CreateICmpEQ(L, SMin),
		Builder->CreateICmpEQ(R, MinusOne));
	bool Changed = insertBugOn(V);
	// ... % 0.
	Changed |= insertBugOn(Builder->CreateIsNull(R));
	return Changed;
}

bool UBBugOn::visitShiftOperator(BinaryOperator &I) {
	IntegerType *T = cast<IntegerType>(I.getType());
	Value *R = I.getOperand(1);
	Constant *BitWidth = ConstantInt::get(T, T->getBitWidth());
	return insertBugOn(Builder->CreateICmpUGE(R, BitWidth));
}

bool UBBugOn::visitShl(BinaryOperator &I) {
	bool Changed = visitShiftOperator(I);
	bool hasNSW = I.hasNoSignedWrap();
	bool hasNUW = I.hasNoUnsignedWrap();
	if (!hasNSW && !hasNUW)
		return Changed;
	Value *L = I.getOperand(0);
	Value *R = I.getOperand(1);
	if (hasNSW) {
		Value *V = Builder->CreateICmpNE(L, Builder->CreateAShr(&I, R));
		Changed |= insertBugOn(V);
	}
	if (hasNUW) {
		Value *V = Builder->CreateICmpNE(L, Builder->CreateLShr(&I, R));
		Changed |= insertBugOn(V);
	}
	return Changed;
}

bool UBBugOn::visitShrOperator(BinaryOperator &I) {
	bool Changed = visitShiftOperator(I);
	if (I.isExact()) {
		Value *L = I.getOperand(0);
		Value *R = I.getOperand(1);
		Value *V = Builder->CreateICmpNE(L, Builder->CreateShl(&I, R));
		Changed |= insertBugOn(V);
	}
	return Changed;
}

bool UBBugOn::visitLoadInst(LoadInst &I) {
	if (I.isVolatile())
		return false;
	Value *V = GetUnderlyingObject(I.getPointerOperand(), TD, 0);
	if (V->isDereferenceablePointer())
		return false;
	return insertBugOn(Builder->CreateIsNull(V));
}

bool UBBugOn::visitStoreInst(StoreInst &I) {
	if (I.isVolatile())
		return false;
	Value *V = GetUnderlyingObject(I.getPointerOperand(), TD, 0);
	if (V->isDereferenceablePointer())
		return false;
	return insertBugOn(Builder->CreateIsNull(V));
}

char UBBugOn::ID;

static RegisterPass<UBBugOn>
X("ub-bugon", "Insert bugon calls for undefined behavior");
