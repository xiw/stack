// This pass simplifies expressions via "transposition" of formulae.
// Consider a comparison lhs < rhs.  The basic idea is to represent
// each side as a symbolic expression (e.g., p + x < p), and transform
// the comparison into lhs - rhs < 0 for simplification (e.g., x < 0).
// Emit an warning if the two forms of the same comparison are only
// equivalent under bug-free assertions.

#define DEBUG_TYPE "anti-algebra"
#include "AntiFunctionPass.h"
#include "Diagnostic.h"
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpander.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Transforms/Utils/Local.h>

using namespace llvm;

namespace {

struct AntiAlgebra : AntiFunctionPass {
	static char ID;
	AntiAlgebra() : AntiFunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AntiFunctionPass::getAnalysisUsage(AU);
		AU.addRequired<ScalarEvolution>();
		AU.setPreservesCFG();
	}

	virtual bool doInitialization(Module &) {
		TLI = getAnalysisIfAvailable<TargetLibraryInfo>();
		return false;
	}

	virtual bool runOnAntiFunction(Function &);

private:
	Diagnostic Diag;
	TargetLibraryInfo *TLI;
	ScalarEvolution *SE;

	bool visitICmpInst(ICmpInst *I);
	int checkEqv(ICmpInst *Old, ICmpInst *New);
};

} // anonymous namespace

bool AntiAlgebra::runOnAntiFunction(Function &F) {
	SE = &getAnalysis<ScalarEvolution>();
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
		if (!Diagnostic::hasSingleDebugLocation(I))
			continue;
		// For now we are only interested in comparisons.
		if (ICmpInst *ICI = dyn_cast<ICmpInst>(I))
			Changed |= visitICmpInst(ICI);
	}
	return Changed;
}

static unsigned getNumTerms(const SCEV *S) {
	if (const SCEVAddExpr *Add = dyn_cast<SCEVAddExpr>(S))
		return Add->getNumOperands();
	return 1;
}

bool AntiAlgebra::visitICmpInst(ICmpInst *I) {
	const SCEV *L = SE->getSCEV(I->getOperand(0));
	const SCEV *R = SE->getSCEV(I->getOperand(1));
	const SCEV *S = SE->getMinusSCEV(L, R);
	// Is S simpler than L and R?
	if (getNumTerms(S) >= getNumTerms(L) + getNumTerms(R))
		return false;
	LLVMContext &C = I->getContext();
	IntegerType *T = IntegerType::get(C, DL->getTypeSizeInBits(S->getType()));
	Value *V = SCEVExpander(*SE, "").expandCodeFor(S, T, I);
	Value *Z = Constant::getNullValue(T);
	// Transform (lhs op rhs) to ((lhs - rhs) op 0).
	ICmpInst *NewCmp = new ICmpInst(I, I->getSignedPredicate(), V, Z);
	NewCmp->setDebugLoc(I->getDebugLoc());
	int isEqv = 0;
	if (SMTFork() == 0)
		isEqv = checkEqv(I, NewCmp);
	SMTJoin(&isEqv);
	if (!isEqv) {
		RecursivelyDeleteTriviallyDeadInstructions(NewCmp, TLI);
		return false;
	}
	Diag.bug(DEBUG_TYPE);
	Diag << "model: |\n" << *I << "\n  -->" << *NewCmp << "\n";
	Diag.backtrace(I);
	I->replaceAllUsesWith(NewCmp);
	RecursivelyDeleteTriviallyDeadInstructions(I, TLI);
	return true;
}

int AntiAlgebra::checkEqv(ICmpInst *I0, ICmpInst *I1) {
	SMTSolver SMT(false);
	ValueGen VG(*DL, SMT);
	PathGen PG(VG, Backedges, *DT);
	int isEqv = 0;
	SMTExpr E0 = VG.get(I0);
	SMTExpr E1 = VG.get(I1);
	SMTExpr Q = SMT.ne(E0, E1);
	BasicBlock *BB = I0->getParent();
	SMTExpr R = PG.get(BB);
	SMT.assume(R);
	// E0 != E1 without bug-free assertions; must be reachable as well.
	if (SMT.query(Q) == SMT_SAT) {
		SMTExpr Delta = getDeltaForBlock(BB, VG);
		if (Delta) {
			// E0 == E1 with bug-free assertions.
			SMTStatus Status = queryWithDelta(Q, Delta, VG);
			SMT.decref(Delta);
			if (Status == SMT_UNSAT)
				isEqv = 1;
		}
	}
	SMT.decref(Q);
	return isEqv;
}

char AntiAlgebra::ID;

static RegisterPass<AntiAlgebra>
X("anti-algebra", "Anti Algebra Optimization");
