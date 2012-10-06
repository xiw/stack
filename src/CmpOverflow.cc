#define DEBUG_TYPE "cmp-overflow"
#include <llvm/Pass.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/Support/CallSite.h>
#include <llvm/Support/InstIterator.h>
#include <algorithm>
#include "Diagnostic.h"

using namespace llvm;

namespace {

typedef const char *CmpStatus;
static CmpStatus CMP_AXB_0 = "UMAX / b => (UMAX - a) / b";
static CmpStatus CMP_AXB_1 = "UMAX / b - a => (UMAX - a) / b";
static CmpStatus CMP_XY_X   = "x * y cmp x";
static CmpStatus CMP_XY_EXT = "zext(x * y) cmp z";

struct CmpOverflow : FunctionPass {
	static char ID;
	CmpOverflow() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeScalarEvolutionPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ScalarEvolution>();
	}

	virtual bool runOnFunction(Function &F) {
		SE = &getAnalysis<ScalarEvolution>();
		inst_iterator i, e = inst_end(F);
		for (i = inst_begin(F); i != e; ++i) {
			CallSite CS(&*i);
			if (CS)
				collect(CS);
		}
		for (i = inst_begin(F); i != e; ++i) {
			if (ICmpInst *ICI = dyn_cast<ICmpInst>(&*i))
				check(ICI);
		}
		AxBs.clear();
		return false;
	}

private:
	Diagnostic Diag;
	ScalarEvolution *SE;
	// a + x * b
	typedef std::pair<ConstantInt *, ConstantInt *> AxBTy;
	DenseMap<const SCEV *, AxBTy> AxBs;

	void collect(CallSite);
	void check(ICmpInst *);
	CmpStatus checkAxB(const SCEV *, const SCEV *);
	CmpStatus checkXY(const SCEV *, const SCEV *);
};

} // anonymous namespace

void CmpOverflow::collect(CallSite CS) {
	CallSite::arg_iterator i = CS.arg_begin(), e = CS.arg_end();
	for (; i != e; ++i) {
		Value *V = *i;
		if (!SE->isSCEVable(V->getType()))
			continue;
		const SCEV *S = SE->getSCEV(V);
		// A + X * B
		if (const SCEVAddExpr *Add = dyn_cast<SCEVAddExpr>(S)) {
			const SCEVConstant *A, *B;
			if (Add->getNumOperands() != 2)
				continue;
			A = dyn_cast<SCEVConstant>(Add->getOperand(0));
			if (!A)
				continue;
			const SCEVMulExpr *Mul;
			Mul = dyn_cast<SCEVMulExpr>(Add->getOperand(1));
			if (!Mul || Mul->getNumOperands() != 2)
				continue;
			B = dyn_cast<SCEVConstant>(Mul->getOperand(0));
			if (!B)
				continue;
			const SCEV *X = Mul->getOperand(1);
			AxBs[X] = std::make_pair(A->getValue(), B->getValue());
			continue;
		}
	}
}

void CmpOverflow::check(ICmpInst *I) {
	const SCEV *L = SE->getSCEV(I->getOperand(0));
	const SCEV *R = SE->getSCEV(I->getOperand(1));
	CmpStatus Status = NULL;
	if (!Status) Status = checkAxB(L, R);
	if (!Status) Status = checkXY(L, R);
	if (!Status) return;
	Diag.bug("bad overflow check");
	Diag << "mode: |\n  " << Status << "\n";
	Diag.backtrace(I);
}

CmpStatus CmpOverflow::checkAxB(const SCEV *L, const SCEV *R) {
	Type *T = L->getType();
	if (!T->isIntegerTy())
		return NULL;
	if (isa<SCEVConstant>(L))
		std::swap(L, R);
	if (!isa<SCEVConstant>(R))
		return NULL;
	AxBTy AxB = AxBs.lookup(L);
	if (!AxB.first || !AxB.second)
		return NULL;
	unsigned n = T->getIntegerBitWidth();
	APInt Max = APInt::getMaxValue(n);
	const APInt &A = AxB.first->getValue();
	const APInt &B = AxB.second->getValue();
	const APInt &C = cast<SCEVConstant>(R)->getValue()->getValue();
	if (C == (Max - A).udiv(B))
		return NULL;
	APInt C0 = Max.udiv(B);
	if (C == C0)
		return CMP_AXB_0;
	APInt C1 = C0 - A;
	if (C == C1)
		return CMP_AXB_1;
	return NULL;
}

static CmpStatus checkMul(const SCEV *MS, const SCEV *S) {
	if (const SCEVZeroExtendExpr *ZExt = dyn_cast<SCEVZeroExtendExpr>(MS)) {
		if (isa<SCEVMulExpr>(ZExt->getOperand()))
			return CMP_XY_EXT;
	}
	const SCEVMulExpr *Mul = dyn_cast<SCEVMulExpr>(MS);
	if (!Mul)
		return NULL;
	if (Mul->getNumOperands() != 2)
		return NULL;
	if (S == Mul->getOperand(0) || S == Mul->getOperand(1))
		return CMP_XY_X;
	
	return NULL;
}

CmpStatus CmpOverflow::checkXY(const SCEV *L, const SCEV *R) {
	CmpStatus Status = NULL;
	if (!Status) Status = checkMul(L, R);
	if (!Status) Status = checkMul(R, L);
	return NULL;
}

char CmpOverflow::ID;

static RegisterPass<CmpOverflow>
X("cmp-overflow", "Detecting broken overflow checks", false, true);
