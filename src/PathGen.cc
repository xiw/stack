#include "PathGen.h"
#include "ValueGen.h"
#include <llvm/Constants.h>
#include <llvm/Instructions.h>
#include <llvm/Support/CFG.h>

using namespace llvm;

#define SMT VG.SMT

PathGen::PathGen(ValueGen &VG, const EdgeVec &BE)
	: VG(VG), BackEdges(BE) {}

PathGen::~PathGen() {
	for (iterator i = Cache.begin(), e = Cache.end(); i != e; ++i)
		SMT.decref(i->second);
}

SMTExpr PathGen::get(BasicBlock *BB) {
	SMTExpr G = Cache.lookup(BB);
	if (G)
		return G;
	// Entry block has true guard.
	if (BB == &BB->getParent()->getEntryBlock()) {
		G = SMT.bvtrue();
		Cache[BB] = G;
		return G;
	}
	// Initialize to false.
	G = SMT.bvfalse();
	// The guard is the disjunction of predecessors' guards.
	pred_iterator i = pred_begin(BB), e = pred_end(BB);
	for (; i != e; ++i) {
		BasicBlock *Pred = *i;
		// Skip back edges.
		if (std::find(BackEdges.begin(), BackEdges.end(),
				Edge(Pred, BB)) != BackEdges.end())
			continue;
		SMTExpr Term = getTermGuard(Pred->getTerminator(), BB);
		SMTExpr PN = getPHIGuard(BB, Pred);
		SMTExpr TermWithPN = SMT.bvand(Term, PN);
		SMT.decref(Term);
		SMT.decref(PN);
		SMTExpr Br = SMT.bvand(TermWithPN, get(Pred));
		SMT.decref(TermWithPN);
		SMTExpr Tmp = SMT.bvor(G, Br);
		SMT.decref(G);
		SMT.decref(Br);
		G = Tmp;
	}
	Cache[BB] = G;
	return G;
}

SMTExpr PathGen::getPHIGuard(BasicBlock *BB, BasicBlock *Pred) {
	SMTExpr E = SMT.bvtrue();
	BasicBlock::iterator i = BB->begin(), e = BB->end();
	for (; i != e; ++i) {
		PHINode *I = dyn_cast<PHINode>(i);
		if (!I)
			break;
		Value *V = I->getIncomingValueForBlock(Pred);
		// Skip undef.
		if (isa<UndefValue>(V))
			continue;
		// Skip non-integral types.
		if (!ValueGen::isAnalyzable(V))
			continue;
		// Generate I == V.
		SMTExpr PN = SMT.eq(VG.get(I), VG.get(V));
		SMTExpr Tmp = SMT.bvand(E, PN);
		SMT.decref(E);
		SMT.decref(PN);
		E = Tmp;
	}
	return E;
}

SMTExpr PathGen::getTermGuard(TerminatorInst *I, BasicBlock *BB) {
	switch (I->getOpcode()) {
	default: I->dump(); llvm_unreachable("Unknown terminator!");
	case Instruction::Br:
		return getTermGuard(cast<BranchInst>(I), BB);
	case Instruction::Switch:
		return getTermGuard(cast<SwitchInst>(I), BB);
	case Instruction::IndirectBr:
	case Instruction::Invoke:
		return SMT.bvtrue();
	}
}

SMTExpr PathGen::getTermGuard(BranchInst *I, BasicBlock *BB) {
	if (I->isUnconditional())
		return SMT.bvtrue();
	// Conditional branch.
	Value *V = I->getCondition();
	SMTExpr E = VG.get(V);
	SMT.incref(E);
	// True or false branch.
	if (I->getSuccessor(0) != BB) {
		assert(I->getSuccessor(1) == BB);
		SMTExpr Tmp = SMT.bvnot(E);
		SMT.decref(E);
		E = Tmp;
	}
	return E;
}

SMTExpr PathGen::getTermGuard(SwitchInst *I, BasicBlock *BB) {
	Value *V = I->getCondition();
	SMTExpr L = VG.get(V);
	SwitchInst::CaseIt i = I->case_begin(), e = I->case_end();
	if (I->getDefaultDest() != BB) {
		// Find all x = C_i for BB.
		SMTExpr E = SMT.bvfalse();
		for (; i != e; ++i) {
			if (i.getCaseSuccessor() == BB) {
				ConstantInt *CI = i.getCaseValue();
				SMTExpr Cond = SMT.eq(L, VG.get(CI));
				SMTExpr Tmp = SMT.bvor(E, Cond);
				SMT.decref(Cond);
				SMT.decref(E);
				E = Tmp;
			}
		}
		return E;
	}
	// Compute guard for the default case.
	// i starts from 1; 0 is reserved for the default.
	SMTExpr E = SMT.bvfalse();
	for (; i != e; ++i) {
		ConstantInt *CI = i.getCaseValue();
		SMTExpr Cond = SMT.eq(L, VG.get(CI));
		SMTExpr Tmp = SMT.bvor(E, Cond);
		SMT.decref(Cond);
		SMT.decref(E);
		E = Tmp;
	}
	SMTExpr NotE = SMT.bvnot(E);
	SMT.decref(E);
	return NotE;
}
