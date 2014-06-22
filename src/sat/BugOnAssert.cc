// This pass recognizes assertions and converts them into bugon
// conditions.  It also removes debugging information on "print"
// blocks (e.g., from WARN) to suppress warnings.
//
// Examples of programmer-written assertions:
// * BUG_ON()
// * assert()
//
// Examples of compiler-inserted assertions:
// * -ftrapv
// * -fsanitize=undefined (clang)

#define DEBUG_TYPE "bugon-assert"
#include "BugOn.h"
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/CFG.h>
#include <llvm/Support/CallSite.h>

using namespace llvm;

namespace {

struct BugOnAssert : BugOnPass {
	static char ID;
	BugOnAssert() : BugOnPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &) const {
		// Override.
	}

	virtual bool runOnFunction(Function &);
	virtual bool runOnInstruction(Instruction *) { return false; }

private:
	DataLayout *DL;

	bool simplify(Function &);
	bool markInfiniteLoops(Function &);
	bool isAssertBlock(BasicBlock *);
	bool visitAssertBlock(BasicBlock *);
	bool convert(BranchInst *I, BasicBlock *BB);
	bool isPrintBlock(BasicBlock *);
	bool visitPrintBlock(BasicBlock *);
};

} // anonymous namespace

bool BugOnAssert::runOnFunction(Function &F) {
	IRBuilder<> TheBuilder(F.getContext());
	Builder = &TheBuilder;
	DL = getAnalysisIfAvailable<DataLayout>();
	bool Changed = markInfiniteLoops(F);
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ) {
		BasicBlock *BB = i++;
		if (isAssertBlock(BB)) {
			Changed |= visitAssertBlock(BB);
			continue;
		}
		if (isPrintBlock(BB)) {
			Changed |= visitPrintBlock(BB);
			continue;
		}
	}
	return Changed;
}

static void propagateUnreachable(BasicBlock *BB) {
	// pred_iterator may become invalid as we remove predecessors.
	// Make a copy.
	SmallVector<BasicBlock *, 16> Preds;
	for (pred_iterator i = pred_begin(BB), e = pred_end(BB); i != e; ++i) {
		BasicBlock *Pred = *i;
		TerminatorInst *TI = Pred->getTerminator();
		// Unconditional jump to BB.
		if (TI->getNumSuccessors() == 1)
			Preds.push_back(Pred);
	}
	LLVMContext &C = BB->getContext();
	for (unsigned i = 0, n = Preds.size(); i != n; ++i) {
		BasicBlock *Pred = Preds[i];
		BB->removePredecessor(Pred);
		Pred->getTerminator()->eraseFromParent();
		new UnreachableInst(C, Pred);
	}
}

bool BugOnAssert::markInfiniteLoops(Function &F) {
	LLVMContext &C = F.getContext();
	bool Changed = false;
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
		BasicBlock *BB = i;
		TerminatorInst *TI = BB->getTerminator();
		// Jump to itself.
		if (TI->getNumSuccessors() != 1 || TI->getSuccessor(0) != BB)
			continue;
		BB->removePredecessor(BB);
		new UnreachableInst(C, BB);
		TI->eraseFromParent();
		propagateUnreachable(BB);
		Changed = true;
	}
	return Changed;
}

bool BugOnAssert::isAssertBlock(BasicBlock *BB) {
	TerminatorInst *TI = BB->getTerminator();
	// Cannot be return or anything other than unreachable/br.
	if (!isa<UnreachableInst>(TI) && !isa<BranchInst>(TI))
		return false;
	unsigned NumSucc = TI->getNumSuccessors();
	// A single unreachable?
	if (TI == &BB->front())
		return NumSucc == 0;
	// Check the last instruction before the terminator.
	// Cannot be invoke (which must be a terminator);
	CallInst *CI = dyn_cast<CallInst>(--BasicBlock::iterator(TI));
	// Block ends with non-call.
	if (!CI)
		return false;
	StringRef Name;
	if (Function *F = CI->getCalledFunction())
		Name = F->getName();
	if (NumSucc) {
		// -fsanitize=undefined.
		if (Name.startswith("__ubsan_handle_"))
			return true;
		return false;
	}
	// From now on BB must be unreachable.
	// Inline asm?  Simply return true for now (look for ud2?).
	if (CI->isInlineAsm())
		return true;
	// Then function pointer?
	if (Name.empty())
		return false;
	if (Name.find("assert") != StringRef::npos)
		return true;
	if (Name.find("panic") != StringRef::npos)
		return true;
	// -ftrapv.
	if (Name == "llvm.trap")
		return true;
	return false;
}

bool BugOnAssert::visitAssertBlock(BasicBlock *BB) {
	SmallVector<BranchInst *, 16> CondBrs;
	for (pred_iterator i = pred_begin(BB), e = pred_end(BB); i != e; ++i) {
		BasicBlock *Pred = *i;
		BranchInst *BI = dyn_cast<BranchInst>(Pred->getTerminator());
		if (!BI)
			continue;
		if (!BI->isConditional())
			continue;
		CondBrs.push_back(BI);
	}
	if (CondBrs.empty())
		return false;
	for (unsigned i = 0, n = CondBrs.size(); i != n; ++i) {
		BranchInst *BI = CondBrs[i];
		BasicBlock *Pred = BI->getParent();
		// Remove possible phi nodes.
		BB->removePredecessor(Pred);
		convert(BI, BB);
	}
	return true;
}

bool BugOnAssert::convert(BranchInst *I, BasicBlock *BB) {
	// Convert the branch condition into bugon.
	BasicBlock *TrueBB = I->getSuccessor(0);
	BasicBlock *FalseBB = I->getSuccessor(1);
	Value *Cond = I->getCondition();
	setInsertPoint(I);
	if (TrueBB == BB) {
		insert(Cond, "assert");
		Builder->CreateBr(FalseBB);
	} else {
		assert(FalseBB == BB);
		insert(Builder->CreateNot(Cond), "assert");
		Builder->CreateBr(TrueBB);
	}
	I->eraseFromParent();
	// Prevent anti-simplify from inspecting the condition.
	recursivelyClearDebugLoc(Cond);
	return true;
}

bool BugOnAssert::isPrintBlock(BasicBlock *BB) {
	bool hasPrint = false;
	for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ++i) {
		CallSite CS(i);
		if (!CS)
			continue;
		Function *F = CS.getCalledFunction();
		// A function pointer.
		if (!F)
			return false;
		StringRef Name = F->getName();
		if (Name.find("print") != StringRef::npos) {
			hasPrint = true;
			continue;
		}
		// Ignore constant functions.
		if (CS.onlyReadsMemory())
			continue;
		// Ignore calls to debugging intrinsics.
		if (isSafeToSpeculativelyExecute(i, DL))
			continue;
		// Unknown function.
		return false;
	}
	return hasPrint;
}

bool BugOnAssert::visitPrintBlock(BasicBlock *BB) {
	// Clear debugging information in this BB to suppress warnings.
	for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ++i)
		clearDebugLoc(i);
	for (pred_iterator i = pred_begin(BB), e = pred_end(BB); i != e; ++i) {
		BasicBlock *Pred = *i;
		BranchInst *BI = dyn_cast<BranchInst>(Pred->getTerminator());
		if (!BI || !BI->isConditional())
			continue;
		// Prevent anti-simplify from inspecting the condition.
		recursivelyClearDebugLoc(BI->getCondition());
	}
	return true;
}

char BugOnAssert::ID;

static RegisterPass<BugOnAssert>
X("bugon-assert", "Insert bugon calls from user assertions");
