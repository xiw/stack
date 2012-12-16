// This pass folds expressions into constants.  The basic idea is to
// warn against expression e that is 1) reachable and 2) non-constant,
// but 3) must be constant with bug-free assertions.

#define DEBUG_TYPE "anti-simplify"
#include "AntiFunctionPass.h"
#include "Diagnostic.h"
#include <llvm/Constants.h>
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Support/Debug.h>
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
		// Skip inserted instructions.
		if (I->getDebugLoc().isUnknown())
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
		Diag.backtrace(I);
		Constant *C = ConstantInt::get(T, ConstVal);
		I->replaceAllUsesWith(C);
		Changed = true;
	}
	return Changed;
}

int AntiSimplify::foldConst(Instruction *I) {
	SMTSolver SMT(true);
	ValueGen VG(*DL, SMT);
	SMTExpr E = VG.get(I);
	BasicBlock *BB = I->getParent();
	// Compute path condition.
	PathGen PG(VG, Backedges, *DT);
	SMTExpr R = PG.get(BB);
	SMTModel Model = NULL;
	// Ignore dead path.
	if (SMT.query(R, &Model) != SMT_SAT)
		return 2;
	if (!Model)
		return 2;
	// Get one possible constant value for I.
	APInt Val;
	SMT.eval(Model, E, Val);
	SMT.release(Model);
	// See if it is possible to make I != C.
	{
		SMTExpr C = SMT.bvconst(Val);
		SMTExpr NC = SMT.ne(E, C);
		SMTExpr RNC = SMT.bvand(R, NC);
		SMT.assume(RNC);
		SMT.decref(C);
		SMT.decref(NC);
		SMT.decref(RNC);
	}
	// Make sure I doesn't have to be C; otherwise I is trivially constant.
	int Status = SMT.query(R);
	if (Status != SMT_SAT)
		return 2;
	// Collect bug-free assertions.
	SMTExpr Delta = getDeltaForBlock(BB, VG);
	if (!Delta)
		return 2;
	SMT.assume(Delta);
	SMT.decref(Delta);
	// Test if I != C doesn't hold with bug-free assertions.
	Status = SMT.query(R);
	if (Status != SMT_UNSAT)
		return 2;
	return !!Val;
}

char AntiSimplify::ID;

static RegisterPass<AntiSimplify>
X("anti-simplify", "Anti Code Simplification");
