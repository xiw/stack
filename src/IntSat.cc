#define DEBUG_TYPE "int-sat"
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/Target/TargetData.h>
#include "PathGen.h"
#include "SMTSolver.h"
#include "ValueGen.h"

using namespace llvm;

namespace {

struct IntSat : ModulePass {
	static char ID;
	IntSat() : ModulePass(ID) { }

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<TargetData>();
	}

	virtual bool runOnModule(Module &);

private:
	TargetData *TD;

	void check(CallInst *);
};

} // anonymous namespace

bool IntSat::runOnModule(Module &M) {
	Function *F = M.getFunction("int.sat");
	if (F) {
		TD = &getAnalysis<TargetData>();
		Function::use_iterator i = F->use_begin(), e = F->use_end();
		for (; i != e; ++i) {
			CallInst *CI = dyn_cast<CallInst>(*i);
			if (CI && CI->getCalledFunction() == F)
				check(CI);
		}
	}
	return false;
}

void IntSat::check(CallInst *I) {
	SMTSolver SMT;
	ValueGen VG(*TD, SMT);
	PathGen PG(VG);
	Value *V = I->getArgOperand(0);
	SMTExpr ValuePred = VG.get(V);
	SMTExpr PathPred = PG.get(I->getParent());
	SMT.dump(ValuePred);
	SMT.dump(PathPred);
#if 0
	SMTExpr Query = SMT.bvand(ValuePred, PathPred);
	SMTModel Model = 0;
	SMTStatus Status = SMT.query(Query, &Model);
	SMT.decref(Query);
	switch (Status) {
	default:
	case SMT_UNSAT:
	case SMT_SAT:
	case SMT_TIMEOUT:
		break;
	}
#endif
}

char IntSat::ID;

static RegisterPass<IntSat>
X("int-sat", "Check int.sat calls for satisfiability", false, true);
