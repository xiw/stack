#define DEBUG_TYPE "load-elim"
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct LoadElim : FunctionPass {
	static char ID;
	LoadElim() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeScalarEvolutionPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		AU.addRequired<ScalarEvolution>();
	}

	virtual bool runOnFunction(Function &);

private:
	ScalarEvolution *SE;

	bool hoist(LoadInst *);
};

} // anonymous namespace

bool LoadElim::runOnFunction(Function &F) {
	SE = &getAnalysis<ScalarEvolution>();
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		LoadInst *I = dyn_cast<LoadInst>(&*i);
		if (I && !I->isVolatile())
			Changed |= hoist(I);
	}
	return Changed;
}

static std::pair<Value *, const SCEVConstant *>
extractPointerBaseAndOffset(const SCEV *S) {
	Value *V = NULL;
	const SCEVConstant *Offset = NULL;
	// p + 0.
	if (const SCEVUnknown *Unknown = dyn_cast<SCEVUnknown>(S))
		return std::make_pair(Unknown->getValue(), Offset);
	// p + offset.
	if (const SCEVAddExpr *Add = dyn_cast<SCEVAddExpr>(S)) {
		if (Add->getNumOperands() == 2) {
			const SCEVConstant *L = dyn_cast<SCEVConstant>(Add->getOperand(0));
			const SCEVUnknown *R = dyn_cast<SCEVUnknown>(Add->getOperand(1));
			if (L && R)
				return std::make_pair(R->getValue(), L);
		}
	}
	return std::make_pair(V, Offset);
}

bool LoadElim::hoist(LoadInst *I) {
	if (I->use_empty())
		return false;
	const SCEV *S = SE->getSCEV(I->getPointerOperand());
	Value *BaseV;
	const SCEVConstant *Offset;
	tie(BaseV, Offset) = extractPointerBaseAndOffset(S);
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
	IRBuilder<> Builder(IP);
	Value *AddrV = BaseV;
	// Offset is based on the type of char *.
	if (Offset) {
		PointerType *PT = cast<PointerType>(BaseV->getType());
		Type *Int8Ty = Type::getInt8PtrTy(PT->getContext(), PT->getAddressSpace());
		AddrV = Builder.CreatePointerCast(AddrV, Int8Ty);
		AddrV = Builder.CreateGEP(AddrV, Offset->getValue());
	}
	AddrV = Builder.CreatePointerCast(AddrV, I->getPointerOperand()->getType());
	SSAUpdater SSA;
	Type *T = I->getType();
	SSA.Initialize(T, I->getPointerOperand()->getName());
	// Insert load.
	LoadInst *LoadV = Builder.CreateLoad(AddrV, true);
	// Update all loads with the new inserted one.
	for (Function::iterator bi = F->begin(), be = F->end(); bi != be; ++bi) {
		BasicBlock *BB = bi;
		Value *V = NULL;
		for (BasicBlock::iterator i = BB->begin(), e = BB->end(); i != e; ++i) {
			Instruction *I = i;
			if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
				Value *StoreV = SI->getValueOperand();
				if (StoreV->getType() != T)
					continue;
				if (S != SE->getSCEV(SI->getPointerOperand()))
					continue;
				V = StoreV;
				continue;
			}
			if (!V) {
				if (I == LoadV)
					V = LoadV;
				continue;
			}
			// Rewrite loads in the same bb of an earlier store.
			if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
				if (LI->isVolatile())
					continue;
				if (LI->getType() != T)
					continue;
				if (S != SE->getSCEV(LI->getPointerOperand()))
					continue;
				I->replaceAllUsesWith(V);
				continue;
			}
		}
		if (V)
			SSA.AddAvailableValue(BB, V);
	}
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		LoadInst *LI = dyn_cast<LoadInst>(&*i);
		if (!LI || LI->isVolatile() || LI->use_empty())
			continue;
		if (LI->getType() != T)
			continue;
		if (S != SE->getSCEV(LI->getPointerOperand()))
			continue;
		for (Value::use_iterator ui = LI->use_begin(), ue = LI->use_end(); ui != ue; ) {
			Use &U = ui.getUse();
			ui++;
			SSA.RewriteUse(U);
		}
	}

	return true;
}

char LoadElim::ID;

static RegisterPass<LoadElim>
X("load-elim", "Load instruction elimination");
