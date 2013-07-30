#include "AntiFunctionPass.h"
#include <llvm/ADT/SCCIterator.h>
#include <llvm/Analysis/CFG.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CFG.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>
#include <sys/mman.h>
#include <algorithm>
#include <cxxabi.h>
#include <stdlib.h>

using namespace llvm;

static cl::opt<bool>
IgnorePostOpt("ignore-bugon-post",
              cl::desc("Ignore bugon conditions on post-dominators"));

static cl::opt<bool>
MinBugOnOpt("min-bugon",
            cl::desc("Compute minimal bugon set"), cl::init(true));

static const size_t BUFFER_SIZE = 4096;

bool BenchmarkFlag;

namespace {
	struct BenchmarkInit {
		BenchmarkInit() { BenchmarkFlag = !!::getenv("BENCHMARK"); }
	};
}

static BenchmarkInit X;

AntiFunctionPass::AntiFunctionPass(char &ID) : FunctionPass(ID), Buffer(NULL) {
	PassRegistry &Registry = *PassRegistry::getPassRegistry();
	initializeDataLayoutPass(Registry);
	initializeDominatorTreePass(Registry);
	initializePostDominatorTreePass(Registry);
	if (MinBugOnOpt)
		Buffer = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
}

AntiFunctionPass::~AntiFunctionPass() {
	if (Buffer)
		munmap(Buffer, BUFFER_SIZE);
}

void AntiFunctionPass::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<DataLayout>();
	AU.addRequired<DominatorTree>();
	AU.addRequired<PostDominatorTree>();
}

#ifndef NDEBUG
static std::string demangle(Function &F) {
	std::string Name = F.getName();
	char *s = abi::__cxa_demangle(Name.c_str(), NULL, NULL, NULL);
	if (s) {
		Name = s;
		free(s);
	}
	return Name;
}
#endif

static void calculateBackedges(Function &F, SmallVectorImpl<PathGen::Edge> &Backedges, SmallVectorImpl<BasicBlock *> &InLoopBlocks) {
	FindFunctionBackedges(F, Backedges);
	if (Backedges.empty())
		return;
	// No need to calculate InLoopBlocks if post-dominators are ignored.
	if (IgnorePostOpt)
		return;
	for (scc_iterator<Function *> i = scc_begin(&F), e = scc_end(&F); i != e; ++i) {
		if (i.hasLoop())
			InLoopBlocks.append((*i).begin(), (*i).end());
	}
}

bool AntiFunctionPass::runOnFunction(Function &F) {
	BugOn = getBugOn(F.getParent());
	if (!BugOn)
		return false;
	DEBUG(dbgs() << "Analyzing " << demangle(F) << "\n");
	assert(BugOn->arg_size() == 1);
	assert(BugOn->arg_begin()->getType()->isIntegerTy(1));
	DT = &getAnalysis<DominatorTree>();
	PDT = &getAnalysis<PostDominatorTree>();
	DL = &getAnalysis<DataLayout>();
	calculateBackedges(F, Backedges, InLoopBlocks);
	bool Changed = runOnAntiFunction(F);
	Backedges.clear();
	InLoopBlocks.clear();
	return Changed;
}

void AntiFunctionPass::recalculate(Function &F) {
	DT->DT->recalculate(F);
	PDT->DT->recalculate(F);
	Backedges.clear();
	InLoopBlocks.clear();
	calculateBackedges(F, Backedges, InLoopBlocks);
}

static SMTExpr computeDelta(ValueGen &VG, SmallVectorImpl<BugOnInst *> &Assertions) {
	SMTSolver &SMT = VG.SMT;
	SMTExpr U = SMT.bvfalse();
	// Compute R and U.
	for (BugOnInst *I : Assertions) {
		// ->queryWithAssertions() may mask some assertions.
		if (!I)
			continue;
		Value *V = I->getCondition();
		SMTExpr E = VG.get(V);
		SMTExpr Tmp = SMT.bvor(U, E);
		SMT.decref(U);
		U = Tmp;
	}
	SMTExpr NotU = SMT.bvnot(U);
	SMT.decref(U);
	return NotU;
}

SMTExpr AntiFunctionPass::getDeltaForBlock(BasicBlock *BB, ValueGen &VG) {
	Assertions.clear();
	// Ignore post-dominators if the option is set or BB is in a loop.
	bool IgnorePostdom = IgnorePostOpt
		|| (std::find(InLoopBlocks.begin(), InLoopBlocks.end(), BB) != InLoopBlocks.end());
	Function *F = BB->getParent();
	for (Function::iterator i = F->begin(), e = F->end(); i != e; ++i) {
		BasicBlock *Blk = i;
		// Collect blocks that (post)dominate BB: if BB is reachable,
		// these blocks must also be reachable, and we need to check
		// their bug assertions.
		if (!DT->dominates(Blk, BB)) {
			// Skip inspecting postdominators if BB is in a loop.
			if (IgnorePostdom)
				continue;
			if (!PDT->dominates(Blk, BB))
				continue;
		}
		for (BasicBlock::iterator i = Blk->begin(), e = Blk->end(); i != e; ++i) {
			if (BugOnInst *BOI = dyn_cast<BugOnInst>(i))
				Assertions.push_back(BOI);
		}
	}
	if (Assertions.empty())
		return NULL;
	return computeDelta(VG, Assertions);
}

SMTStatus AntiFunctionPass::queryWithDelta(SMTExpr E, SMTExpr Delta, ValueGen &VG) {
	SMTSolver &SMT = VG.SMT;
	{
		SMTExpr Q = SMT.bvand(E, Delta);
		SMTStatus Status = SMT.query(Q);
		SMT.decref(Q);
		if (!Buffer || Status != SMT_UNSAT)
			return Status;
	}
	unsigned n = Assertions.size();
	// Compute the minimal bugon set.
	for (BugOnInst *&I : Assertions) {
		if (n <= 1)
			break;
		BugOnInst *Tmp = I;
		// Mask out this bugon and see if still unsat.
		I = NULL;
		SMTExpr MinDelta = computeDelta(VG, Assertions);
		SMTExpr Q = SMT.bvand(E, MinDelta);
		SMT.decref(MinDelta);
		SMTStatus Status = SMT.query(Q);
		SMT.decref(Q);
		// Keep this assertions.
		if (Status != SMT_UNSAT)
			I = Tmp;
		else
			--n;
	}
	// Output the unsat core.
	BugOnInst **p = (BugOnInst **)Buffer;
	for (BugOnInst *I : Assertions) {
		if (!I)
			continue;
		*p++ = I;
	}
	*p = NULL;
	return SMT_UNSAT;
}

void AntiFunctionPass::printMinimalAssertions() {
	if (!Buffer)
		return;
	int Count = 0;
	for (BugOnInst **p = (BugOnInst **)Buffer; *p; ++p)
		Count++;
	Diag << "ncore: " << Count << "\n";
	Diag << "core: \n";
	LLVMContext &C = BugOn->getContext();
	for (BugOnInst **p = (BugOnInst **)Buffer; *p; ++p) {
		BugOnInst *I = *p;
		MDNode *MD = I->getDebugLoc().getAsMDNode(C);
		Diag.location(MD);
		Diag << "    - " << I->getAnnotation() << "\n";
	}
}
