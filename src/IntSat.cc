#define DEBUG_TYPE "int-sat"
#include "Diagnostic.h"
#include "PathGen.h"
#include "SMTSolver.h"
#include "ValueGen.h"
#include <llvm/Constants.h>
#include <llvm/BasicBlock.h>
#include <llvm/Instructions.h>
#include <llvm/Function.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

static cl::opt<bool>
SMTModelOpt("smt-model", cl::desc("Output SMT model"));

namespace {

// Better to make this as a module pass rather than a function pass.
// Otherwise, put `M.getFunction("int.sat")' in doInitialization() and
// it will return NULL, since it's scheduled to run before -int-rewrite.
struct IntSat : ModulePass {
	static char ID;
	IntSat() : ModulePass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
	}

	virtual bool runOnModule(Module &);

private:
	Diagnostic Diag;
	Function *Trap;
	OwningPtr<TargetData> TD;
	unsigned MD_bug;

	SmallVector<PathGen::Edge, 32> BackEdges;
	SmallPtrSet<Value *, 32> ReportedBugs;

	void runOnFunction(Function &);
	void check(CallInst *);
	SMTStatus query(Value *, BasicBlock *);
};

} // anonymous namespace

bool IntSat::runOnModule(Module &M) {
	Trap = M.getFunction("int.sat");
	if (!Trap)
		return false;
	TD.reset(new TargetData(&M));
	MD_bug = M.getContext().getMDKindID("bug");
	for (Module::iterator i = M.begin(), e = M.end(); i != e; ++i) {
		Function &F = *i;
		if (F.empty())
			continue;
		runOnFunction(F);
	}
	return false;
}

void IntSat::runOnFunction(Function &F) {
	BackEdges.clear();
	FindFunctionBackedges(F, BackEdges);
	ReportedBugs.clear();
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		CallInst *CI = dyn_cast<CallInst>(&*i);
		if (CI && CI->getCalledFunction() == Trap)
			check(CI);
	}
}

void IntSat::check(CallInst *I) {
	assert(I->getNumArgOperands() >= 1);
	Value *V = I->getArgOperand(0);
	assert(V->getType()->isIntegerTy(1));
	if (isa<ConstantInt>(V))
		return;
	if (ReportedBugs.count(V))
		return;

	const DebugLoc &DbgLoc = I->getDebugLoc();
	if (DbgLoc.isUnknown())
		return;
	MDNode *MD = I->getMetadata(MD_bug);
	if (!MD)
		return;
	Diag.bug(cast<MDString>(MD->getOperand(0))->getString());

	int SMTRes;
	if (SMTFork() == 0) {
		BasicBlock *BB = I->getParent();
		SMTRes = query(V, BB);
	}
	SMTJoin(&SMTRes);

	// Save to suppress furture warnings.
	if (SMTRes == SMT_SAT)
		ReportedBugs.insert(V);

	// Output location.
	Diag.status(SMTRes);
	Diag.backtrace(I);
}

SMTStatus IntSat::query(Value *V, BasicBlock *BB) {
	SMTSolver SMT(SMTModelOpt);
	ValueGen VG(*TD, SMT);
	PathGen PG(VG, BackEdges);
	SMTExpr Query = SMT.bvand(VG.get(V), PG.get(BB));
	SMTModel Model = NULL;
	SMTStatus Res = SMT.query(Query, &Model);
	SMT.decref(Query);
	// Output model.
	if (SMTModelOpt && Model) {
		Diag << "model: |\n";
		raw_ostream &OS = Diag.os();
		for (ValueGen::iterator i = VG.begin(), e = VG.end(); i != e; ++i) {
			Value *KeyV = i->first;
			if (isa<Constant>(KeyV))
				continue;
			OS << "  ";
			WriteAsOperand(OS, KeyV, false, Trap->getParent());
			OS << ": ";
			SMT.eval(Model, i->second, OS);
			OS << '\n';
		}
	}
	if (Model)
		SMT.release(Model);
	return Res;
}

char IntSat::ID;

static RegisterPass<IntSat>
X("int-sat", "Check int.sat for satisfiability", false, true);
