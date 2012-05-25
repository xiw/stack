#define DEBUG_TYPE "int-sat"
#include <llvm/BasicBlock.h>
#include <llvm/Instructions.h>
#include <llvm/Function.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include "Diagnostic.h"
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
	Function *CurF;
	SmallVector<PathGen::Edge, 32> BackEdges;
	OwningPtr<Diagnostic> Diag;
	unsigned MD_int;

	void check(CallInst *);
};

} // anonymous namespace

bool IntSat::runOnModule(Module &M) {
	Function *IntSat = M.getFunction("int.sat");
	if (IntSat) {
		TD = &getAnalysis<TargetData>();
		CurF = 0;
		BackEdges.clear();
		Diag.reset(new Diagnostic(M));
		MD_int = M.getContext().getMDKindID("int");
		Function::use_iterator i = IntSat->use_begin(), e = IntSat->use_end();
		for (; i != e; ++i) {
			CallInst *CI = dyn_cast<CallInst>(*i);
			if (CI && CI->getCalledFunction() == IntSat)
				check(CI);
		}
	}
	return false;
}

void IntSat::check(CallInst *I) {
	Value *V = I->getArgOperand(0);
	if (isa<Constant>(V))
		return;
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
	SMTExpr ValuePred = VG.get(V);
	SMTExpr PathPred = PG.get(BB);
	SMTExpr Query = SMT.bvand(ValuePred, PathPred);
	SMTModel Model = 0;
	SMTStatus Status = SMT.query(Query, &Model);
	SMT.decref(Query);
	switch (Status) {
	default: break;
	case SMT_UNSAT:
		return;
	}
	// Output location and operator.
	StringRef Reason = cast<MDString>(
		I->getMetadata(MD_int)->getOperand(0)
	)->getString();
	*Diag << I->getDebugLoc() << Reason;
	// Output model.
	if (Model) {
		raw_ostream &OS = Diag->os();
		for (ValueGen::iterator i = VG.begin(), e = VG.end(); i != e; ++i) {
			Value *KeyV = i->first;
			if (isa<Constant>(KeyV))
				continue;
			WriteAsOperand(OS, KeyV, false, F->getParent());
			OS << ":\t";
			SMT.eval(Model, i->second, OS);
			OS << '\n';
		}
	}
}

char IntSat::ID;

static RegisterPass<IntSat>
X("int-sat", "Check int.sat calls for satisfiability", false, true);
