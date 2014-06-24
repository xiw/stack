#define DEBUG_TYPE "elim-assert"
#include <set>
#include <llvm/Pass.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/InstIterator.h>
#include "Diagnostic.h"

using namespace llvm;

namespace {

struct ElimAssert : FunctionPass {
	static char ID;
	ElimAssert() : FunctionPass(ID) {}

	virtual bool runOnFunction(Function &);

private:
	bool safeBB(BasicBlock *BB);
	bool dropBB(Function &F, BasicBlock *BB);
};

} // anonymous namespace

static std::set<StringRef> AssertFailures = {
	"__assert_fail",	// Linux assert()
	"panic",		// Linux kernel BUG_ON()
};

static std::set<StringRef> SafeFunctions = {
	"printf",
	"printk",		// Linux kernel
};

bool ElimAssert::runOnFunction(Function &F) {
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
		if (I->getDebugLoc().isUnknown())
			continue;
		CallInst *CI = dyn_cast<CallInst>(I);
		if (!CI || !CI->getCalledFunction())
			continue;
		if (AssertFailures.find(CI->getCalledFunction()->getName()) ==
		    AssertFailures.end())
			continue;
		BasicBlock *BB = CI->getParent();

		if (!safeBB(BB))
			continue;

		Changed |= dropBB(F, BB);
	}
	return Changed;
}

// Return true if this basic block has no calls to functions that
// might prevent an eventual assertion failure.
bool ElimAssert::safeBB(BasicBlock *BB) {
	for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ) {
		Instruction *I = &*i++;
		CallInst *CI = dyn_cast<CallInst>(I);
		if (!CI)
			continue;
		if (!CI->getCalledFunction())
			return false;
		StringRef N = CI->getCalledFunction()->getName();
		if (AssertFailures.find(N) == AssertFailures.end() &&
		    SafeFunctions.find(N) == SafeFunctions.end()) {
			// Diagnostic() << "ElimAssert: unsafe call to " << N << "\n";
			return false;
		}
	}
	return true;
}

bool ElimAssert::dropBB(Function &F, BasicBlock *BB) {
	IRBuilder<> TheBuilder(F.getContext());

	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
		BranchInst *BI = dyn_cast<BranchInst>(I);
		if (!BI)
			continue;
		if (BI->isUnconditional())
			continue;
		if (BI->getNumSuccessors() != 2)
			continue;

		if (BI->getSuccessor(0) == BB) {
			BI->setCondition(TheBuilder.getFalse());
			Changed = true;
		}

		if (BI->getSuccessor(1) == BB) {
			BI->setCondition(TheBuilder.getTrue());
			Changed = true;
		}
	}
	return Changed;
}

char ElimAssert::ID;

static RegisterPass<ElimAssert>
X("elim-assert", "Eliminate asserts");
