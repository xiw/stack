#define DEBUG_TYPE "cmp-sat"
#include <llvm/DataLayout.h>
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include "Diagnostic.h"
#include "PathGen.h"
#include "ValueGen.h"

using namespace llvm;

namespace {

typedef const char *CmpStatus;
static CmpStatus CMP_FALSE = "comparison always false";
static CmpStatus CMP_TRUE = "comparison always true";

struct CmpSat : FunctionPass {
	static char ID;
	CmpSat() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDataLayoutPass(Registry);
		initializeDominatorTreePass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.addRequired<DataLayout>();
		AU.addRequired<DominatorTree>();
		AU.setPreservesAll();
	}

	virtual bool runOnFunction(Function &F) {
		DL = &getAnalysis<DataLayout>();
		DT = &getAnalysis<DominatorTree>();
		FindFunctionBackedges(F, Backedges);
		for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
			BranchInst *BI = dyn_cast<BranchInst>(i->getTerminator());
			if (!BI || !BI->isConditional())
				continue;
			check(BI);
		}
		Backedges.clear();
		return false;
	}

private:
	Diagnostic Diag;
	DataLayout *DL;
	DominatorTree *DT;
	SmallVector<PathGen::Edge, 32> Backedges;

	void check(BranchInst *);
};

} // anonymous namespace

void CmpSat::check(BranchInst *I) {
	BasicBlock *BB = I->getParent();
	Value *V = I->getCondition();
	SMTSolver SMT(false);
	ValueGen VG(*DL, SMT);
	PathGen PG(VG, Backedges, *DT);
	SMTExpr ValuePred = VG.get(V);
	SMTExpr PathPred = PG.get(BB);
	SMTExpr Query = SMT.bvand(ValuePred, PathPred);
	SMTStatus Status = SMT.query(Query);
	SMT.decref(Query);
	CmpStatus Reason = 0;
	if (Status == SMT_UNSAT) {
		Reason = CMP_FALSE;
	} else {
		SMTExpr NotValuePred = SMT.bvnot(ValuePred);
		Query = SMT.bvand(NotValuePred, PathPred);
		Status = SMT.query(Query);
		SMT.decref(Query);
		SMT.decref(NotValuePred);
		if (Status == SMT_UNSAT)
			Reason = CMP_TRUE;
	}
	if (!Reason)
		return;
	Diag.bug(Reason);
	Diag.backtrace(I);
}

char CmpSat::ID;

static RegisterPass<CmpSat>
X("cmp-sat", "Detecting bogus comparisons via satisfiability", false, true);
