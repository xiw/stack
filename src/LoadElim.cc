#define DEBUG_TYPE "load-elim"
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpander.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct LoadElim : FunctionPass {
	static char ID;
	LoadElim() : FunctionPass(ID) {
		PassRegistry &Registry = *llvm::PassRegistry::getPassRegistry();
		//initializeAliasAnalysisAnalysisGroup(Registry);
		//initializeDominatorTreePass(Registry);
		initializeScalarEvolutionPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		//AU.addRequired<DominatorTree>();
		AU.addRequired<ScalarEvolution>();
	}

	virtual bool runOnFunction(Function &);

private:
	//DominatorTree *DT;
	ScalarEvolution *SE;

	bool hoist(const SCEV *, LoadInst *);
};

} // anonymous namespace

bool LoadElim::runOnFunction(Function &F) {
	//DT = &getAnalysis<DominatorTree>();
	SE = &getAnalysis<ScalarEvolution>();
	// Collect load addresses.
	typedef DenseMap<const SCEV *, LoadInst *> AddrMapTy;
	AddrMapTy AddrMap;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		LoadInst *I = llvm::dyn_cast<llvm::LoadInst>(&*i);
		if (I && !I->isVolatile()) {
			const SCEV *S = SE->getSCEV(I->getPointerOperand());
			AddrMap.insert(std::make_pair(S, I));
		}
	}
	bool Changed = false;
	for (AddrMapTy::iterator i = AddrMap.begin(), e = AddrMap.end(); i != e; ++i)
		Changed |= hoist(i->first, i->second);
	return Changed;
}

static Value *baseOf(const SCEV *S) {
	// p + 0.
	if (const SCEVUnknown *Unknown = dyn_cast<SCEVUnknown>(S))
		return Unknown->getValue();
	// p + offset.
	if (const SCEVAddExpr *Add = dyn_cast<SCEVAddExpr>(S)) {
		if (Add->getNumOperands() == 2) {
			const SCEV *L = Add->getOperand(0);
			const SCEV *R = Add->getOperand(1);
			if (isa<SCEVConstant>(L) && isa<SCEVUnknown>(R))
				return cast<SCEVUnknown>(R)->getValue();
		}
	}
	return NULL;
}

bool LoadElim::hoist(const SCEV *S, LoadInst *I) {
	Value *BaseV = baseOf(S);
	if (!BaseV)
		return false;
	Function *F = I->getParent()->getParent();
	// Insert a load to the earliest point:
	// 1) right after Base's definition, if Base is an instruction;
	// 2) in the entry block, if Base is an argument or a global variable.
	Instruction *IP;
	if (Instruction *BaseI = dyn_cast<Instruction>(BaseV))
		IP = ++BasicBlock::iterator(BaseI);
	else
		IP = F->getEntryBlock().begin();
	// Skip phi nodes, if any.
	if (isa<PHINode>(IP))
		IP = IP->getParent()->getFirstInsertionPt();
	// We may reuse the address if it is a simple GEP.  Otherwise
	// expand the SCEV expression instead.
	Value *AddrV = NULL;
	if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(I->getPointerOperand())) {
		if (GEP->hasAllConstantIndices()) {
			if (GEP == IP)
				IP = ++BasicBlock::iterator(IP);
			else
				GEP->moveBefore(IP);
			AddrV = GEP;
		}
	}
	if (!AddrV) {
		SCEVExpander Expander(*SE, "");
		AddrV = Expander.expandCodeFor(S, I->getPointerOperand()->getType(), IP);
	}
	SSAUpdater SSA;
	SSA.Initialize(I->getType(), I->getName());
	// Insert load.
	SSA.AddAvailableValue(IP->getParent(),
	                      new LoadInst(AddrV, I->getName(), true, IP));
	// Update all loads with the new inserted one.
	for (Function::iterator bi = F->begin(), be = F->end(); bi != be; ++bi) {
		BasicBlock *BB = bi;
		Value *V = NULL;
		for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ) {
			Instruction *I = i++;
			if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
				if (S == SE->getSCEV(SI->getPointerOperand()))
					V = SI->getValueOperand();
				continue;
			}
			if (!V)
				continue;
			// Rewrite loads in the same bb of an earlier store.
			if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
				if (!LI->isVolatile() && S == SE->getSCEV(LI->getPointerOperand())) {
					I->replaceAllUsesWith(V);
					I->eraseFromParent();
				}
			}
		}
		if (V)
			SSA.AddAvailableValue(BB, V);
	}
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		LoadInst *LI = dyn_cast<LoadInst>(&*i++);
		if (!LI || LI->isVolatile() || S != SE->getSCEV(LI->getPointerOperand()))
			continue;
		for (Value::use_iterator ui = LI->use_begin(), ue = LI->use_end(); ui != ue; ) {
			Use &U = ui.getUse();
			ui++;
			SSA.RewriteUseAfterInsertions(U);
		}
		assert(LI->use_empty());
		LI->eraseFromParent();
	}
	return true;
}

char LoadElim::ID;

static RegisterPass<LoadElim>
X("load-elim", "Load instruction elimination");
