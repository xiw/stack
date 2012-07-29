// Build e-SSA form.
//
// This pass inserts pi nodes for comparisons.  A pi node is represented
// by a single-element phi node.
//
// This pass should run after -break-crit-edges.

#define DEBUG_TYPE "essa"
#include <llvm/BasicBlock.h>
#include <llvm/Function.h>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Pass.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/CFG.h>
#include <llvm/Support/Debug.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>

using namespace llvm;

namespace {

struct ESSA : FunctionPass {
	static char ID;
	ESSA() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	typedef DenseMap<Value *, SSAUpdater *> SSAMap;
	SSAMap SSAs;
	Function *F;

	bool runOnBranchInst(BranchInst *);
	bool runOnSwitchInst(SwitchInst *);
	bool update(Value *, TerminatorInst *);
	bool rewrite();
	void rewrite(Value *, SSAUpdater &);
	BasicBlock *getDefBB(Value *);
	SSAUpdater &getSSA(Value *);
};

} // anonymous namespace

bool ESSA::runOnFunction(Function &F) {
	bool Changed = false;
	this->F = &F;
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
		TerminatorInst *TI = i->getTerminator();
		switch (TI->getOpcode()) {
		default: break;
		case Instruction::Br:
			Changed |= runOnBranchInst(cast<BranchInst>(TI));
			break;
		case Instruction::Switch:
			Changed |= runOnSwitchInst(cast<SwitchInst>(TI));
			break;
		}
	}
	Changed |= rewrite();
	return Changed;
}

bool ESSA::runOnBranchInst(BranchInst *I) {
	bool Changed = false;
	if (I->isConditional()) {
		if (CmpInst *CI = dyn_cast<CmpInst>(I->getCondition())) {
			Changed |= update(CI->getOperand(0), I);
			Changed |= update(CI->getOperand(1), I);
		}
	}
	return Changed;
}

bool ESSA::runOnSwitchInst(SwitchInst *I) {
	return update(I->getCondition(), I);
}

bool ESSA::update(Value *V, TerminatorInst *I) {
	// Skip constant or if it is only used in the comparison.
	if (isa<Constant>(V) || V->hasOneUse())
		return false;
	BasicBlock *BB = I->getParent();
	// Verify every successor has only one predecessor.
	for (succ_iterator i = succ_begin(BB), e = succ_end(BB); i != e; ++i) {
		if (i->getSinglePredecessor() != BB)
			return false;
	}
	// Insert a pi node at the begining of each successor.
	SSAUpdater &SSA = getSSA(V);
	for (succ_iterator i = succ_begin(BB), e = succ_end(BB); i != e; ++i) {
		PHINode *PI = PHINode::Create(
			V->getType(),
			1,
			V->getName() + ".pi",
			i->begin()
		);
		PI->addIncoming(V, BB);
		SSA.AddAvailableValue(*i, PI);
	}
	return true;
}

bool ESSA::rewrite() {
	if (SSAs.empty())
		return false;
	for (SSAMap::iterator i = SSAs.begin(), e = SSAs.end(); i != e; ++i) {
		SSAUpdater *SSA = i->second;
		rewrite(i->first, *SSA);
		delete SSA;
	}
	SSAs.clear();
	return true;
}

void ESSA::rewrite(Value *V, SSAUpdater &SSA) {
	BasicBlock *DefBB = getDefBB(V);
	for (Value::use_iterator i = V->use_begin(), e = V->use_end(); i != e; ) {
		Use &U = i.getUse();
		++i;
		Instruction *I = dyn_cast<Instruction>(U.getUser());
		// Rewrite use of V in an instruction.
		if (!I)
			continue;
		// Skip rewriting any V's use in V's def BB,
		// where V should be used.
		if (DefBB == I->getParent())
			continue;
		// Skip pi nodes of V.
		if (PHINode *PHI = dyn_cast<PHINode>(I))
			if (PHI->getNumIncomingValues() == 1)
				continue;
		SSA.RewriteUseAfterInsertions(U);
	}
}

BasicBlock *ESSA::getDefBB(Value *V) {
	if (Instruction *I = dyn_cast<Instruction>(V))
		return I->getParent();
	return &F->getEntryBlock();
}

SSAUpdater &ESSA::getSSA(Value *V) {
	SSAUpdater *&SSA = SSAs[V];
	if (!SSA) {
		SSA = new SSAUpdater;
		SSA->Initialize(V->getType(), V->getName());
		SSA->AddAvailableValue(getDefBB(V), V);
	}
	return *SSA;
}

char ESSA::ID;

static RegisterPass<ESSA>
X("essa", "Extended SSA Form Pass");
