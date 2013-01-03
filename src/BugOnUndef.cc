// Insert bug assertions on undef (e.g., uninitialized) value.

#define DEBUG_TYPE "bugon-undef"
#include "BugOn.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/CallSite.h>

using namespace llvm;

namespace {

struct BugOnUndef : BugOnPass {
	static char ID;
	BugOnUndef() : BugOnPass(ID) {}

	virtual bool runOnInstruction(Instruction *);
};

} // anonymous namespace

static bool isDeadArg(Instruction *I, unsigned Idx) {
	CallSite CS(I);
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

static bool isDeadRet(Function *F) {
	for (Function::use_iterator i = F->use_begin(), e = F->use_end(); i != e; ++i) {
		CallSite CS(*i);
		if (!CS)
			return false;
		if (!CS->use_empty())
			return false;
	}
	return true;
}

bool BugOnUndef::runOnInstruction(Instruction *I) {
	// It's okay to have undef in phi's operands.
	// TODO: catch conditional undefs.
	if (isa<PHINode>(I) || isa<SelectInst>(I))
		return false;
	if (isa<InsertValueInst>(I) || isa<InsertElementInst>(I))
		return false;
	// Allow ret undef is the return value is never used.
	if (isa<ReturnInst>(I)) {
		if (isDeadRet(I->getParent()->getParent()))
			return false;
	}
	// If any operand is undef, this instruction must not be reachable.
	for (unsigned i = 0, n = I->getNumOperands(); i != n; ++i) {
		Value *V = I->getOperand(i);
		if (isa<UndefValue>(V)) {
			// Allow undef arguments created by -deadargelim,
			// which are basically unused in the function body.
			if (isDeadArg(I, i))
				continue;
			return insert(Builder->getTrue(), "undef");
		}
	}
	return false;
}

char BugOnUndef::ID;

static RegisterPass<BugOnUndef>
X("bugon-undef", "Insert bugon calls for undef values");
