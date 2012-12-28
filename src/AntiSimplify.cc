// This pass folds expressions into constants.  The basic idea is to
// warn against expression e that is 1) reachable and 2) non-constant,
// but 3) must be constant with bug-free assertions.

#define DEBUG_TYPE "anti-simplify"
#include "AntiFunctionPass.h"
#include "Diagnostic.h"
#include <llvm/Constants.h>
#include <llvm/Instructions.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct AntiSimplify: AntiFunctionPass {
	static char ID;
	AntiSimplify() : AntiFunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AntiFunctionPass::getAnalysisUsage(AU);
		AU.setPreservesCFG();
	}

	virtual bool runOnAntiFunction(Function &);

private:
	Diagnostic Diag;

	int foldConst(Instruction *);
};

} // anonymous namespace

bool AntiSimplify::runOnAntiFunction(Function &F) {
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
		if (!hasSingleDebugLocation(I))
			continue;
		Type *T = I->getType();
		// For now we are only interested in bool expressions.
		if (!T->isIntegerTy(1))
			continue;
		int ConstVal;
		if (SMTFork() == 0)
			ConstVal = foldConst(I);
		SMTJoin(&ConstVal);
		if (ConstVal != 0 && ConstVal != 1)
			continue;
		Diag.bug(DEBUG_TYPE);
		Diag << "model: |\n" << *I << "\n  -->  "
		     << (ConstVal ? "true" : "false") << "\n";
		Diag.backtrace(I);
		Constant *C = ConstantInt::get(T, ConstVal);
		I->replaceAllUsesWith(C);
		Changed = true;
	}
	return Changed;
}

#define FOLD_FAIL 2

int AntiSimplify::foldConst(Instruction *I) {
	int Result = FOLD_FAIL;
	SMTSolver SMT(false);
	ValueGen VG(*DL, SMT);
	BasicBlock *BB = I->getParent();
	SMTExpr Delta = getDeltaForBlock(BB, VG);
	if (!Delta)
		return Result;
	// Compute path condition.
	PathGen PG(VG, Backedges, *DT);
	{
		SMTExpr R = PG.get(BB);
		SMT.assume(R);
	}
	SMTExpr E = VG.get(I);
	int Status;
	{
		SMTExpr Q = SMT.bvand(E, Delta);
		Status = SMT.query(Q);
		SMT.decref(Q);
	}
	if (Status == SMT_UNSAT) {
		// I must be false with Delta.
		// Can I be true without Delta?
		if (SMT.query(E) == SMT_SAT)
			Result = 0;
	} else {
		// I can be false with Delta.
		// Let's try if it can be true.
		SMTExpr NE = SMT.bvnot(E);
		SMTExpr Q = SMT.bvand(NE, Delta);
		Status = SMT.query(Q);
		SMT.decref(Q);
		if (Status == SMT_UNSAT) {
			// I must be true with Delta.
			// Can I be false with Delta?
			if (SMT.query(NE) == SMT_SAT)
				Result = 1;
		}
		SMT.decref(NE);
	}
	SMT.decref(Delta);
	return Result;
}

char AntiSimplify::ID;

static RegisterPass<AntiSimplify>
X("anti-simplify", "Anti Code Simplification");
