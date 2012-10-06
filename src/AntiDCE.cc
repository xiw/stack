// This pass kills unreachable (dead) code by exploiting undefined behavior.
// The basic idea is that given a reachable statement s, if it always blows up
// another reachable statement t (i.e., triggering t's undefined behavior),
// then s is actually "dead" in terms of undefined behavior.

#define DEBUG_TYPE "anti-dce"
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Support/Debug.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <cxxabi.h>
#include "Diagnostic.h"
#include "PathGen.h"
#include "SMTSolver.h"
#include "ValueGen.h"

using namespace llvm;

namespace {

struct AntiDCE: FunctionPass {
	static char ID;
	AntiDCE() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDominatorTreePass(Registry);
		initializePostDominatorTreePass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.addRequired<DominatorTree>();
		AU.addRequired<PostDominatorTree>();
		AU.addRequired<TargetData>();
		AU.addPreserved<DominatorTree>();
		AU.addPreserved<PostDominatorTree>();
	}

	virtual bool runOnFunction(Function &);

private:
	Diagnostic Diag;
	Function *BugOn;
	DominatorTree *DT;
	PostDominatorTree *PDT;
	TargetData *TD;
	SmallVector<PathGen::Edge, 32> Backedges;

	int shouldKeepCode(BasicBlock *BB);
	void markAsDead(BasicBlock *BB);
};

} // anonymous namespace

static std::string demangle(Function &F) {
	std::string Name = F.getName();
	char *s = abi::__cxa_demangle(Name.c_str(), NULL, NULL, NULL);
	if (s) {
		Name = s;
		free(s);
	}
	return Name;
}

bool AntiDCE::runOnFunction(Function &F) {
	BugOn = F.getParent()->getFunction("c.bugon");
	if (!BugOn)
		return false;
	DEBUG(dbgs() << "Analyzing " << demangle(F) << "\n");
	assert(BugOn->arg_size() == 1);
	assert(BugOn->arg_begin()->getType()->isIntegerTy(1));
	DT = &getAnalysis<DominatorTree>();
	PDT = &getAnalysis<PostDominatorTree>();
	TD = &getAnalysis<TargetData>();
	FindFunctionBackedges(F, Backedges);
	bool Changed = false;
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
		BasicBlock *BB = i;
		int Keep;
		if (SMTFork() == 0)
			Keep = shouldKeepCode(BB);
		SMTJoin(&Keep);
		if (Keep)
			continue;
		Changed = true;
		markAsDead(BB);
		// Update DT & PDT if any optimization performed.
		DT->DT->recalculate(F);
		PDT->DT->recalculate(F);
		Backedges.clear();
		FindFunctionBackedges(F, Backedges);
	}
	Backedges.clear();
	return Changed;
}

int AntiDCE::shouldKeepCode(BasicBlock *BB) {
	SMTSolver SMT(false);
	ValueGen VG(*TD, SMT);
	Function *F = BB->getParent();
	// Compute path condition.
	PathGen PG(VG, Backedges, *DT);
	SMTExpr R = PG.get(BB);
	// Ignore dead path.
	if (SMT.query(R) == SMT_UNSAT)
		return 1;
	// Collect undefined behavior assertions.
	SmallVector<SMTExpr, 16> UBs;
	for (Function::iterator bi = F->begin(), be = F->end(); bi != be; ++bi) {
		BasicBlock *Blk = bi;
		// Collect blocks that (post)dominate BB: if BB is reachable,
		// these blocks must also be reachable, and we need to check
		// their undefined behavior assertions.
		if (!DT->dominates(Blk, BB) && !PDT->dominates(Blk, BB))
			continue;
		for (BasicBlock::iterator i = Blk->begin(), e = Blk->end(); i != e; ++i) {
			CallInst *CI = dyn_cast<CallInst>(i);
			if (!CI || CI->getCalledFunction() != BugOn)
				continue;
			Value *V = CI->getArgOperand(0);
			SMTExpr E = VG.get(V);
			UBs.push_back(E);
		}
	}
	if (UBs.empty())
		return 1;
	SMTExpr U = SMT.bvfalse();
	// Compute R and U.
	for (unsigned i = 0, n = UBs.size(); i != n; ++i) {
		SMTExpr Tmp = SMT.bvor(U, UBs[i]);
		SMT.decref(U);
		U = Tmp;
	}
	SMTExpr NotU = SMT.bvnot(U);
	SMT.decref(U);
	SMTExpr Q = SMT.bvand(R, NotU);
	SMT.decref(NotU);
	SMTStatus Status = SMT.query(Q);
	SMT.decref(Q);
	if (Status == SMT_UNSAT)
		return 0;
	return 1;
}

void AntiDCE::markAsDead(BasicBlock *BB) {
	// Prove BB is dead; output warning message.
	Diag.bug(DEBUG_TYPE);
	Diag << "model: |\n  " << BB->getName() << ":\n";
	for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ++i)
		Diag << *i << '\n';
	for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ++i) {
		if (!i->getDebugLoc().isUnknown()) {
			Diag.backtrace(i);
			break;
		}
	}
	// Remove BB from successors.
	std::vector<BasicBlock *> Succs(succ_begin(BB), succ_end(BB));
	for (unsigned i = 0, e = Succs.size(); i != e; ++i)
		Succs[i]->removePredecessor(BB);
	// Empty BB.
	for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ) {
		Instruction *I = i++;
		Type *T = I->getType();
		if (!I->use_empty())
			I->replaceAllUsesWith(UndefValue::get(T));
		I->eraseFromParent();
	}
	// Mark it as unreachable.
	new UnreachableInst(BB->getContext(), BB);
}

char AntiDCE::ID;

static RegisterPass<AntiDCE>
X("anti-dce", "Anti Dead Code Elimination");
