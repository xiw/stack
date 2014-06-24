// This pass folds expressions into constants.  The basic idea is to
// warn against expression e that is 1) reachable and 2) non-constant,
// but 3) must be constant with bug-free assertions.

#define DEBUG_TYPE "anti-simplify"
#include "AntiFunctionPass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/Local.h>

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
	int foldConst(Instruction *);
};

} // anonymous namespace

#define FOLD_FAIL 2

static inline
const char *qstr(int ConstVal) {
	switch (ConstVal) {
	default:
		return "timeout";
	case 0: case 1:
		return "succ";
	case FOLD_FAIL:
		return "fail";
	}
}

bool AntiSimplify::runOnAntiFunction(Function &F) {
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
		if (!Diagnostic::hasSingleDebugLocation(I))
			continue;
		// For now we are only interested in bool expressions.
		if (!isa<ICmpInst>(I))
			continue;
		int ConstVal;
		if (SMTFork() == 0)
			ConstVal = foldConst(I);
		SMTJoin(&ConstVal);
		BENCHMARK(Diagnostic() << "query: " << qstr(ConstVal) << "\n");
		if (ConstVal != 0 && ConstVal != 1)
			continue;
		Diag.bug(DEBUG_TYPE);
		Diag << "model: |\n" << *I << "\n  -->  "
		     << (ConstVal ? "true" : "false") << "\n";
		Diag.backtrace(I);
		printMinimalAssertions();
		Type *T = I->getType();
		Constant *C = ConstantInt::get(T, ConstVal);
		I->replaceAllUsesWith(C);
		RecursivelyDeleteTriviallyDeadInstructions(I);
		Changed = true;
	}
	return Changed;
}

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
	int Status = queryWithDelta(E, Delta, VG);
	if (Status == SMT_UNSAT) {
		// I must be false with Delta.
		// Can I be true without Delta?
		if (SMT.query(E) == SMT_SAT)
			Result = 0;
	} else {
		// I can be false with Delta.
		// Let's try if it can be true.
		SMTExpr NE = SMT.bvnot(E);
		Status = queryWithDelta(NE, Delta, VG);
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
