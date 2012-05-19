#define DEBUG_TYPE "cmp-sat"
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/Support/InstIterator.h>
#include "Diagnostic.h"

using namespace llvm;

namespace {

typedef const char *CmpStatus;
static CmpStatus CMP_FALSE = "comparison always false";
static CmpStatus CMP_TRUE = "comparison always true";

struct CmpSat : FunctionPass {
	static char ID;
	CmpSat() : FunctionPass(ID) {
		PassRegistry &Registry = *llvm::PassRegistry::getPassRegistry();
		initializeScalarEvolutionPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ScalarEvolution>();
	}

	virtual bool doInitialization(Module &M) {
		Diag.reset(new Diagnostic(M));
		return false;
	}

	virtual bool runOnFunction(Function &F) {
		SE = &getAnalysis<ScalarEvolution>();
		inst_iterator i = inst_begin(F), e = inst_end(F);
		for (; i != e; ++i) {
			if (ICmpInst *ICI = dyn_cast<ICmpInst>(&*i)) {
				checkSat(ICI);
			}
		}
		return false;
	}

private:
	OwningPtr<Diagnostic> Diag;
	ScalarEvolution *SE;

	void checkSat(ICmpInst *);
	CmpStatus solveSat(ICmpInst *);
};

} // anonymous namespace

void CmpSat::checkSat(ICmpInst *I) {
	if (!SE->isSCEVable(I->getOperand(0)->getType()))
		return;
	const SCEV *L = SE->getSCEV(I->getOperand(0));
	const SCEV *R = SE->getSCEV(I->getOperand(1));
	// Ignore constant comparison.
	if (isa<SCEVConstant>(L) && isa<SCEVConstant>(R))
		return;
	// Ignore loop variables.
	if (isa<llvm::SCEVAddRecExpr>(L) || isa<llvm::SCEVAddRecExpr>(R))
		return;
	const char *Reason;
	if (SE->isKnownPredicate(I->getPredicate(), L, R))
		Reason = CMP_TRUE;
	else if (SE->isKnownPredicate(I->getInversePredicate(), L, R))
		Reason = CMP_FALSE;
	else
		Reason = solveSat(I);
	if (!Reason)
		return;
	*Diag << I->getDebugLoc() << Reason;
}

CmpStatus CmpSat::solveSat(ICmpInst *I) {
	return 0;
}

char CmpSat::ID;

static RegisterPass<CmpSat>
X("cmp-sat", "Detecting bogus comparisons", false, true);
