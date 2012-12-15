#include "AntiFunctionPass.h"
#include "BugOn.h"
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/ADT/SCCIterator.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Support/CFG.h>
#include <llvm/Support/Debug.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <algorithm>
#include <cxxabi.h>

using namespace llvm;

AntiFunctionPass::AntiFunctionPass(char &ID) : FunctionPass(ID) {
	PassRegistry &Registry = *PassRegistry::getPassRegistry();
	initializeDataLayoutPass(Registry);
	initializeDominatorTreePass(Registry);
	initializePostDominatorTreePass(Registry);
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

SMTExpr AntiFunctionPass::getDeltaForBlock(BasicBlock *BB, ValueGen &VG) {
	SmallVector<SMTExpr, 32> Dom;
	bool BBInLoop = (std::find(InLoopBlocks.begin(), InLoopBlocks.end(), BB) != InLoopBlocks.end());
	Function *F = BB->getParent();
	for (Function::iterator i = F->begin(), e = F->end(); i != e; ++i) {
		BasicBlock *Blk = i;
		// Collect blocks that (post)dominate BB: if BB is reachable,
		// these blocks must also be reachable, and we need to check
		// their bug assertions.
		bool dom = DT->dominates(Blk, BB);
		// Skip inspecting postdominators if BB is in a loop.
		bool postdom = BBInLoop ? false : PDT->dominates(Blk, BB);
		if (!dom && !postdom)
			continue;
		for (BasicBlock::iterator i = Blk->begin(), e = Blk->end(); i != e; ++i) {
			CallInst *CI = dyn_cast<CallInst>(i);
			if (!CI || CI->getCalledFunction() != BugOn)
				continue;
			Value *V = CI->getArgOperand(0);
			SMTExpr E = VG.get(V);
			Dom.push_back(E);
		}
	}
	if (Dom.empty())
		return NULL;
	SMTSolver &SMT = VG.SMT;
	SMTExpr U = SMT.bvfalse();
	// Compute R and U.
	for (unsigned i = 0, n = Dom.size(); i != n; ++i) {
		SMTExpr Tmp = SMT.bvor(U, Dom[i]);
		SMT.decref(U);
		U = Tmp;
	}
	SMTExpr NotU = SMT.bvnot(U);
	SMT.decref(U);
	return NotU;
}
