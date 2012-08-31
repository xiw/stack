#define DEBUG_TYPE "int-sat"
#include "Diagnostic.h"
#include "PathGen.h"
#include "SMTSolver.h"
#include "ValueGen.h"
#include <llvm/BasicBlock.h>
#include <llvm/Instructions.h>
#include <llvm/Function.h>
#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <err.h>
#include <signal.h>
#include <unistd.h>

using namespace llvm;

static cl::opt<unsigned>
SolverTimeout("solver-timeout",
              cl::desc("Specify a timeout for SMT solver"),
              cl::value_desc("seconds"));

static cl::opt<bool>
NoModelOutput("no-model",
              cl::desc("Do not output model when SAT"),
              cl::init(false));


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
	OwningPtr<TargetData> TD;
	OwningPtr<Diagnostic> Diag;
	Function *Trap;
	unsigned MD_int;

	SmallVector<PathGen::Edge, 32> BackEdges;
	SmallPtrSet<Value *, 32> ReportedBugs;

	void check(CallInst *);
};

} // anonymous namespace

bool IntSat::doInitialization(Module &M) {
	TD.reset(new TargetData(&M));
	Diag.reset(new Diagnostic(M));
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
	StringRef Reason = cast<MDString>(MD->getOperand(0))->getString();
	assert(I->getNumArgOperands() >= 1);
	Value *Cond = I->getArgOperand(0);
	assert(Cond->getType()->isIntegerTy(1));
	if (isa<ConstantInt>(Cond)) {
		// TODO.
		return;
	}
	if (ReportedBugs.count(Cond))
		return;

	int fds[2] = {-1, -1};
	if (SolverTimeout) {
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
			err(1, "socketpair");
		int pid = fork();
		if (pid < 0)
			err(1, "fork");
		// Parent process.
		if (pid) {
			close(fds[1]);
			struct timeval tv = {SolverTimeout, 0};
			fd_set fdset;
			FD_ZERO(&fdset);
			FD_SET(fds[0], &fdset);
			select(fds[0] + 1, &fdset, NULL, NULL, &tv);
			if (FD_ISSET(fds[0], &fdset)) {
				SMTStatus dummy;
				// Child done.
				if (read(fds[0], &dummy, sizeof(dummy)) < 0)
					err(1, "read (parent)");
				// Ack.
				if (write(fds[0], &dummy, sizeof(dummy)) < 0)
					err(1, "write (parent)");
			} else {
				// Child timeout.
				kill(pid, SIGKILL);
				*Diag << DbgLoc << "timeout - " + Reason;
			}
			int stat_loc;
			waitpid(pid, &stat_loc, 0);
			return;
		}
		// Child process fall through.
		close(fds[0]);
	}

	BasicBlock *BB = I->getParent();

	SMTSolver SMT;
	ValueGen VG(*TD, SMT);
	PathGen PG(VG, BackEdges);
	SMTExpr Query = SMT.bvand(VG.get(Cond), PG.get(BB));
	SMTModel Model = 0;
	SMTStatus Status = SMT.query(Query, &Model);
	SMT.decref(Query);

	if (SolverTimeout) {
		SMTStatus dummy;
		// Notify parent.
		if (write(fds[1], &Status, sizeof(Status)) < 0)
			err(1, "write (child)");
		// Acked by parent.
		if (read(fds[1], &dummy, sizeof(dummy)) < 0)
			err(1, "read (child)");
	}

	switch (Status) {
	default: break;
	case SMT_UNSAT:
		if (SolverTimeout)
			_exit(0);
		return;
	}
	// Save to suppress furture warnings.
	ReportedBugs.insert(Cond);
	// Output location and operator.
	*Diag << DbgLoc << Reason;
	// Output model.
	if (Model && !NoModelOutput) {
		raw_ostream &OS = Diag->os();
		for (ValueGen::iterator i = VG.begin(), e = VG.end(); i != e; ++i) {
			Value *KeyV = i->first;
			if (isa<Constant>(KeyV))
				continue;
			WriteAsOperand(OS, KeyV, false, Trap->getParent());
			OS << ":\t";
			SMT.eval(Model, i->second, OS);
			OS << '\n';
		}
	}
	if (SolverTimeout)
		_exit(0);
}

char IntSat::ID;

static RegisterPass<IntSat>
X("int-sat", "Check int.sat for satisfiability", false, true);
