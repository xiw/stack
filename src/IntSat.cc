#define DEBUG_TYPE "int-sat"
#include "Diagnostic.h"
#include "PathGen.h"
#include "SMTSolver.h"
#include "ValueGen.h"
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
#include <sys/time.h>
#include <sys/wait.h>
#include <err.h>

using namespace llvm;

static cl::opt<unsigned>
SMTTimeoutOpt("smt-timeout",
              cl::desc("Specify a timeout for SMT solver"),
              cl::value_desc("milliseconds"));

static cl::opt<bool>
SMTModelOpt("smt-model", cl::desc("Output SMT model"));

namespace {

struct IntSat : FunctionPass {
	static char ID;
	IntSat() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
	}

	virtual bool doInitialization(Module &);

	virtual bool runOnFunction(Function &);

private:
	Diagnostic Diag;
	OwningPtr<TargetData> TD;
	Function *Trap;
	unsigned MD_int;

	SmallVector<PathGen::Edge, 32> BackEdges;
	SmallPtrSet<Value *, 32> ReportedBugs;

	void check(CallInst *);
	SMTStatus query(Value *, BasicBlock *);
};

} // anonymous namespace

bool IntSat::doInitialization(Module &M) {
	TD.reset(new TargetData(&M));
	Trap = M.getFunction("int.sat");
	MD_int = M.getContext().getMDKindID("int");
	return false;
}

bool IntSat::runOnFunction(Function &F) {
	if (!Trap)
		return false;
	BackEdges.clear();
	FindFunctionBackedges(F, BackEdges);
	ReportedBugs.clear();
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		CallInst *CI = dyn_cast<CallInst>(&*i);
		if (CI && CI->getCalledFunction() == Trap)
			check(CI);
	}
	return false;
}

void IntSat::check(CallInst *I) {
	const DebugLoc &DbgLoc = I->getDebugLoc();
	if (DbgLoc.isUnknown())
		return;
	MDNode *MD = I->getMetadata(MD_int);
	if (!MD)
		return;
	assert(I->getNumArgOperands() >= 1);
	Value *V = I->getArgOperand(0);
	assert(V->getType()->isIntegerTy(1));
	if (isa<ConstantInt>(V))
		return;
	if (ReportedBugs.count(V))
		return;

	Diag << "---\n";
	BasicBlock *BB = I->getParent();
	SMTStatus SMTRes;
	if (!SMTTimeoutOpt) {
		SMTRes = query(V, BB);
	} else {
		int pid = fork();
		if (pid < 0)
			err(1, "fork");
		// Child process.
		if (pid == 0) {
			struct itimerval itv = {{0, 0}, {SMTTimeoutOpt / 1000, SMTTimeoutOpt % 1000 * 1000}};
			setitimer(ITIMER_VIRTUAL, &itv, NULL);
			_exit(query(V, BB));
		}
		// Parent process.
		int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			SMTRes = (SMTStatus)WEXITSTATUS(status);
		else
			SMTRes = SMT_TIMEOUT;
	}

	// Save to suppress furture warnings.
	if (SMTRes == SMT_SAT)
		ReportedBugs.insert(V);

	// Output location and operator.
	const char *SMTStr;
	switch (SMTRes) {
	default:          SMTStr = "undef";   break;
	case SMT_UNSAT:   SMTStr = "unsat";   break;
	case SMT_SAT:     SMTStr = "sat";     break;
	case SMT_TIMEOUT: SMTStr = "timeout"; break;
	}
	Diag << "status: " << SMTStr << "\n";
	StringRef Anno = cast<MDString>(MD->getOperand(0))->getString();
	Diag << "opcode: " << Anno << "\n";
	Diag << "stack: \n";
	Diag.backtrace(I, "  - ");
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
