// This pass attaches range metadata to phi nodes, especially those
// in loops, so as to add value constraints. 

#define DEBUG_TYPE "phi-range"
#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>

using namespace llvm;

namespace {

struct PHIRange : FunctionPass {
	static char ID;
	PHIRange() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeLoopInfoPass(Registry);
		initializeScalarEvolutionPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		AU.addRequired<LoopInfo>();
		AU.addRequired<ScalarEvolution>();
	}

	virtual bool runOnFunction(Function &);

private:
	LoopInfo *LI;
	ScalarEvolution *SE;

	bool visitPHINode(PHINode *);
};

} // anonymous namespace

bool PHIRange::runOnFunction(Function &F) {
	LI = &getAnalysis<LoopInfo>();
	SE = &getAnalysis<ScalarEvolution>();
	bool Changed = false;
	// PHI nodes must be at the start of each block.
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
		BasicBlock *BB = i;
		if (!LI->getLoopFor(BB))
			continue;
		BasicBlock::iterator PHIIter = BB->begin();
		for (;;) {
			PHINode *PHI = dyn_cast<PHINode>(PHIIter++);
			if (!PHI)
				break;
			Changed |= visitPHINode(PHI);
		}
	}
	return Changed;
}

bool PHIRange::visitPHINode(PHINode *I) {
	IntegerType *T = dyn_cast<IntegerType>(I->getType());
	// Ignore non-integer nodes.
	if (!T)
		return false;
	const SCEV *S = SE->getSCEV(I);
	ConstantRange Range = SE->getSignedRange(S);
	if (Range.isFullSet() || Range.isEmptySet())
		return false;
	Value *Vals[2] = {
		ConstantInt::get(T, Range.getLower()),
		ConstantInt::get(T, Range.getUpper())
	};
	LLVMContext &VMCtx = I->getContext();
	MDNode *MD = MDNode::get(VMCtx, Vals);
	I->setMetadata("intrange", MD);
	return true;
}

char PHIRange::ID;

static RegisterPass<PHIRange>
X("phi-range", "Add range metadata to phi nodes");
