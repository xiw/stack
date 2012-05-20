#define DEBUG_TYPE "cmp-sat"
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include "Diagnostic.h"
#include "PathGen.h"
#include "SMTSolver.h"
#include "ValueGen.h"

using namespace llvm;

namespace {

typedef const char *CmpStatus;
static CmpStatus CMP_FALSE = "comparison always false";
static CmpStatus CMP_TRUE = "comparison always true";

struct CmpSat : FunctionPass {
	static char ID;
	CmpSat() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
	}

	virtual bool doInitialization(Module &M) {
		Diag.reset(new Diagnostic(M));
		TD.reset(new TargetData(&M));
		CurF = 0;
		return false;
	}

	virtual bool runOnFunction(Function &F) {
		inst_iterator i = inst_begin(F), e = inst_end(F);
		for (; i != e; ++i) {
			if (ICmpInst *ICI = dyn_cast<ICmpInst>(&*i)) {
				check(ICI);
			}
		}
		return false;
	}

private:
	OwningPtr<Diagnostic> Diag;
	OwningPtr<TargetData> TD;
	Function *CurF;
	SmallVector<PathGen::Edge, 32> BackEdges;

	void check(ICmpInst *);
};

} // anonymous namespace

void CmpSat::check(ICmpInst *I) {
	BasicBlock *BB = I->getParent();
	Function *F = BB->getParent();
	if (CurF != F) {
		CurF = F;
		BackEdges.clear();
		FindFunctionBackedges(*F, BackEdges);
	}
	SMTSolver SMT;
	ValueGen VG(*TD, SMT);
	PathGen PG(VG, BackEdges);
	SMTExpr ValuePred = VG.get(I);
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
	SMT.decref(ValuePred);
	SMT.decref(PathPred);
	if (Reason)
		*Diag << I->getDebugLoc() << Reason;
}

char CmpSat::ID;

static RegisterPass<CmpSat>
X("cmp-sat", "Detecting bogus comparisons via satisfiability", false, true);
