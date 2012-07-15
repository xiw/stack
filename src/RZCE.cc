/// -rzce: redundant zero check elimination.
///
/// -rzce simplifies two usages:
/// 1) zero check after divison.
/// 2) zero check always leading to divison.
///
/// -rzce should be used after -lowerswitch, otherwise it will miss case 0
/// in switch statements.

#define DEBUG_TYPE "rzce"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"

using namespace llvm;

STATISTIC(RZCECmpAfterDivEliminated, "Number of zero check after division removed");
STATISTIC(RZCEBranchToDivEliminated, "Number of zero check always leading to division removed");

namespace {
	struct RZCE : FunctionPass {
		static char ID;
		RZCE() : FunctionPass(ID) {
			PassRegistry &Registery = *PassRegistry::getPassRegistry();
			initializeDominatorTreePass(Registery);
		}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<DominatorTree>();
			AU.addRequired<PostDominatorTree>();
			AU.setPreservesCFG();
		}
		virtual bool runOnFunction(Function &);
	};
} // anonymous namespace

static bool simplifyCmpAfterDiv(ICmpInst *Cmp, BinaryOperator *Div, DominatorTree &DT) {
	if (!DT.dominates(Div, Cmp))
		return false;
	bool Val = (Cmp->getPredicate() == CmpInst::ICMP_EQ) ? false : true;
	Constant *C = ConstantInt::get(Cmp->getType(), Val);
	Cmp->replaceAllUsesWith(C);
	Cmp->dump();
	Div->dump();
	++RZCECmpAfterDivEliminated;
	return true;
}

static bool simplifyBranchToDiv(BranchInst *Br, BinaryOperator *Div, PostDominatorTree &PDT) {
	ICmpInst *Cmp = cast<ICmpInst>(Br->getCondition());
	unsigned Idx = (Cmp->getPredicate() == CmpInst::ICMP_EQ) ? 0 : 1;
	BasicBlock *BB = Br->getSuccessor(Idx);
	if (!PDT.dominates(Div->getParent(), BB))
		return false;
	Br->setCondition(ConstantInt::get(Cmp->getType(), Idx));
	Br->dump();
	Cmp->dump();
	Div->dump();
	++RZCEBranchToDivEliminated;
	return true;
}

bool RZCE::runOnFunction(Function &F) {
	// Divs.
	SmallVector<Use *, 4> Divs;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		switch (I->getOpcode()) {
		default: break;
		case Instruction::SDiv:
		case Instruction::UDiv:
			// Extract the divisor.
			Divs.push_back(&I->getOperandUse(1));
			break;
		}
	}
	if (Divs.empty())
		return false;
	unsigned ndivs = Divs.size();
	bool Changed = false;
	DominatorTree &DT = getAnalysis<DominatorTree>();
	PostDominatorTree &PDT = getAnalysis<PostDominatorTree>();
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		ICmpInst *Cmp = dyn_cast<ICmpInst>(&*i++);
		if (!Cmp || !Cmp->isEquality())
			continue;
		Value *V = NULL;
		// Extract x in x == 0 or x != 0.
		for (unsigned i = 0; i != 2; ++i) {
			ConstantInt *C = dyn_cast<ConstantInt>(Cmp->getOperand(i));
			if (C && C->isZero()) {
				V = Cmp->getOperand(1 - i);
				break;
			}
		}
		if (!V)
			continue;
		for (unsigned k = 0; k != ndivs; ++k) {
			Use &Divisor = *Divs[k];
			if (Divisor != V)
				continue;
			BinaryOperator *Div = cast<BinaryOperator>(Divisor.getUser());
			if (simplifyCmpAfterDiv(Cmp, Div, DT)) {
				Changed = true;
				break;
			}
			// Check for conditional branches on the comparison.
			for (Value::use_iterator ui = Cmp->use_begin(), ue = Cmp->use_end(); ui != ue; ) {
				BranchInst *Br = dyn_cast<BranchInst>(*ui++);
				if (Br && Br->isConditional())
					Changed |= simplifyBranchToDiv(Br, Div, PDT);
			}
		}
	}
	return Changed;
}

char RZCE::ID;

static RegisterPass<RZCE>
X("rzce", "Redundant Zero Check Elimination");

Pass *createRedundantZeroCheckElimination() {
	return new RZCE;
}
