#include "BugOn.h"
#include "Diagnostic.h"
#include <llvm/ADT/STLExtras.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/DebugLoc.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

#define OPT_BUGON "opt.bugon"

static cl::opt<bool>
ShowTrueOpt("show-bugon-true",
            cl::desc("Show always true bug conditions"));

Function *getBugOn(const Module *M) {
	return M->getFunction(OPT_BUGON);
}

Function *getOrInsertBugOn(Module *M) {
	LLVMContext &C = M->getContext();
	Type *VoidTy = Type::getVoidTy(C);
	Type *BoolTy = Type::getInt1Ty(C);
	FunctionType *T = FunctionType::get(VoidTy, BoolTy, false);
	Function *F = cast<Function>(M->getOrInsertFunction(OPT_BUGON, T));
	F->setDoesNotThrow();
	return F;
}

StringRef BugOnInst::getAnnotation() const {
	MDNode *MD = getMetadata("bug");
	return cast<MDString>(MD->getOperand(0))->getString();
}

void BugOnPass::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesCFG();
}

bool BugOnPass::runOnFunction(Function &F) {
	IRBuilder<> TheBuilder(F.getContext());
	Builder = &TheBuilder;
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
		if (I->getDebugLoc().isUnknown())
			continue;
		setInsertPoint(I);
		Changed |= runOnInstruction(I);
	}
	return Changed;
}

bool BugOnPass::clearDebugLoc(Value *V) {
	Instruction *I = dyn_cast<Instruction>(V);
	if (!I || I->getDebugLoc().isUnknown())
		return false;
	I->setDebugLoc(DebugLoc());
	return true;
}

bool BugOnPass::recursivelyClearDebugLoc(Value *V) {
	Instruction *I = dyn_cast<Instruction>(V);
	if (!I || I->getDebugLoc().isUnknown())
		return false;
	I->setDebugLoc(DebugLoc());
	for (Instruction::op_iterator i = I->op_begin(), e = I->op_end(); i != e; ++i) {
		Value *U = *i;
		if (U->hasOneUse())
			recursivelyClearDebugLoc(U);
	}
	return true;
}

Value *BugOnPass::getUnderlyingObject(Value *V, DataLayout *DL) {
	return GetUnderlyingObject(V, DL, 1000);
}

Value *BugOnPass::getAddressOperand(Value *I, bool skipVolatile) {
#define IS_VOLATILE(x) (skipVolatile && (x)->isVolatile())
	if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
		if (!IS_VOLATILE(LI))
			return LI->getPointerOperand();
	} else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
		if (!IS_VOLATILE(SI))
			return SI->getPointerOperand();
	} else if (AtomicCmpXchgInst *AI = dyn_cast<AtomicCmpXchgInst>(I)) {
		if (!IS_VOLATILE(AI))
			return AI->getPointerOperand();
	} else if (AtomicRMWInst *AI = dyn_cast<AtomicRMWInst>(I)) {
		if (!IS_VOLATILE(AI))
			return AI->getPointerOperand();
	}
#undef IS_VOLATILE
	return NULL;
}

bool BugOnPass::insert(Value *V, StringRef Bug) {
	const DebugLoc &DbgLoc = Builder->GetInsertPoint()->getDebugLoc();
	return insert(V, Bug, DbgLoc);
}

bool BugOnPass::insert(Value *V, StringRef Bug, const DebugLoc &DbgLoc) {
	// Ignore bugon(false).
	if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
		if (CI->isZero())
			return false;
		if (ShowTrueOpt) {
			Instruction *I = Builder->GetInsertPoint();
			if (Diagnostic::hasSingleDebugLocation(I)) {
				Diagnostic Diag;
				Diag.bug(Pass::lookupPassInfo(getPassID())->getPassArgument());
				Diag << "model: |\n" << *I << "\n";
				Diag.backtrace(I);
			}
		}
	}
	LLVMContext &C = V->getContext();
	if (!BugOn) {
		BugOn = getOrInsertBugOn(getModule());
		MD_bug = C.getMDKindID("bug");
	}
	Instruction *I = Builder->CreateCall(BugOn, V);
	I->setDebugLoc(DbgLoc);
	if (!Bug.empty())
		I->setMetadata(MD_bug, MDNode::get(C, MDString::get(C, Bug)));
	return true;
}

Module *BugOnPass::getModule() {
	return Builder->GetInsertBlock()->getParent()->getParent();
}

Instruction *BugOnPass::setInsertPoint(Instruction *I) {
	Instruction *IP = Builder->GetInsertPoint();
	Builder->SetInsertPoint(I);
	// Don't set debugging information for inserted instructions.
	Builder->SetCurrentDebugLocation(DebugLoc());
	return IP;
}

Instruction *BugOnPass::setInsertPointAfter(Instruction *I) {
	assert(!isa<TerminatorInst>(I) && "Cannot insert after a terminator!");
	BasicBlock::iterator Iter(I);
	++Iter;
	return setInsertPoint(Iter);
}

Value *BugOnPass::createIsNull(Value *V) {
	// Ignore trivial non-null pointers (e.g., a stack pointer).
	if (V->isDereferenceablePointer())
		return Builder->getFalse();
	return Builder->CreateIsNull(V);
}

Value *BugOnPass::createIsZero(Value *V) {
	return Builder->CreateIsNull(V);
}

Value *BugOnPass::createIsNotNull(Value *V) {
	if (V->isDereferenceablePointer())
		return Builder->getTrue();
	return Builder->CreateIsNotNull(V);
}

Value *BugOnPass::createIsWrap(Intrinsic::ID ID, Value *L, Value *R) {
	Type *T = L->getType();
	assert(T == R->getType() && "Type mismatch!");
	Function *F = Intrinsic::getDeclaration(getModule(), ID, T);
	return Builder->CreateExtractValue(Builder->CreateCall2(F, L, R), 1);
}

Value *BugOnPass::createIsSAddWrap(Value *L, Value *R) {
	return createIsWrap(Intrinsic::sadd_with_overflow, L, R);
}

Value *BugOnPass::createIsUAddWrap(Value *L, Value *R) {
	return createIsWrap(Intrinsic::uadd_with_overflow, L, R);
}

Value *BugOnPass::createIsSSubWrap(Value *L, Value *R) {
	return createIsWrap(Intrinsic::ssub_with_overflow, L, R);
}

Value *BugOnPass::createIsUSubWrap(Value *L, Value *R) {
	return createIsWrap(Intrinsic::usub_with_overflow, L, R);
}

Value *BugOnPass::createIsSMulWrap(Value *L, Value *R) {
	return createIsWrap(Intrinsic::smul_with_overflow, L, R);
}

Value *BugOnPass::createIsUMulWrap(Value *L, Value *R) {
	return createIsWrap(Intrinsic::umul_with_overflow, L, R);
}

Value *BugOnPass::createIsSDivWrap(Value *L, Value *R) {
	Type *T = L->getType();
	assert(T == R->getType() && "Type mismatch!");
	unsigned n = T->getIntegerBitWidth();
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(n));
	Constant *MinusOne = Constant::getAllOnesValue(T);
	return createAnd(
		Builder->CreateICmpEQ(L, SMin),
		Builder->CreateICmpEQ(R, MinusOne)
	);
}

Value *BugOnPass::createAnd(Value *L, Value *R) {
	if (Constant *C = dyn_cast<Constant>(L)) {
		if (C->isAllOnesValue())
			return R;
		if (C->isNullValue())
			return L;
	}
	if (Constant *C = dyn_cast<Constant>(R)) {
		if (C->isAllOnesValue())
			return L;
		if (C->isNullValue())
			return R;
	}
	return Builder->CreateAnd(L, R);
}

Value *BugOnPass::createSExtOrTrunc(Value *V, IntegerType *T) {
	// New in 3.2.
	unsigned SrcWidth = cast<IntegerType>(V->getType())->getBitWidth();
	unsigned DstWidth = T->getBitWidth();
	if (SrcWidth < DstWidth)
		return Builder->CreateSExt(V, T);
	if (SrcWidth > DstWidth)
		return Builder->CreateTrunc(V, T);
	return V;
}

Value *BugOnPass::createPointerEQ(Value *V0, Value *V1) {
	if (V0 == V1)
		return Builder->getTrue();
	return Builder->CreateICmpEQ(V0, Builder->CreatePointerCast(V1, V0->getType()));
}
