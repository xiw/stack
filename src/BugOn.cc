#include "BugOn.h"
#include <llvm/Module.h>
#include <llvm/Support/DebugLoc.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

#define KINT_BUGON "kint.bugon"

Function *getBugOn(Module *M) {
	return M->getFunction(KINT_BUGON);
}

Function *getOrInsertBugOn(Module *M) {
	LLVMContext &C = M->getContext();
	Type *VoidTy = Type::getVoidTy(C);
	Type *BoolTy = Type::getInt1Ty(C);
	FunctionType *T = FunctionType::get(VoidTy, BoolTy, false);
	Function *F = cast<Function>(M->getOrInsertFunction(KINT_BUGON, T));
	F->setDoesNotThrow();
	return F;
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
		Builder->SetInsertPoint(I);
		// Don't set debugging information for inserted instructions.
		Builder->SetCurrentDebugLocation(DebugLoc());
		Changed |= visit(I);
	}
	return Changed;
}

bool BugOnPass::insert(Value *V, StringRef Bug) {
	// Ignore bugon(false).
	if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
		if (CI->isZero())
			return false;
	}
	LLVMContext &C = V->getContext();
	if (!BugOn) {
		BugOn = getOrInsertBugOn(getModule());
		MD_bug = C.getMDKindID("bug");
	}
	const DebugLoc &DbgLoc = Builder->GetInsertPoint()->getDebugLoc();
	Instruction *I = Builder->CreateCall(BugOn, V);
	I->setDebugLoc(DbgLoc);
	if (!Bug.empty())
		I->setMetadata(MD_bug, MDNode::get(C, MDString::get(C, Bug)));
	return true;
}

Module *BugOnPass::getModule() {
	return Builder->GetInsertBlock()->getParent()->getParent();
}

Value *BugOnPass::createIsNull(Value *V) {
	// Ignore trivial non-null pointers (e.g., a stack pointer).
	if (V->isDereferenceablePointer())
		return Builder->getFalse();
	return Builder->CreateIsNull(V);
}
