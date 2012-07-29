// Bitwise range analysis.
//
// This pass implements the bidirectional range propagation algorithm
// described in
//
//   Bitwidth Analysis with Applications to Silicon Compilation
//   Mark Stephenson, Jonathan Babb, and Saman Amarasinghe
//   PLDI 2000
//
// This pass should run after -essa, which inserts range-refinement (aka pi)
// node.  It extensively makes uses of undefined behavior:
//
//   * / x                 => x != 0
//   INT_MIN /s  x         => x != -1
//   x /s -1               => x != INT_MIN
//
// TODO:
//   a[i]                  => i >= 0 && i <= ARRAY_SIZE(a)
//   c = a op_s b          => c >= INT_MIN && c <= INT_MAX
//   x << y or x >> y      => y <u BITS(x)
//   x << y, x is signed   => C11 may imply x >= 0, x << y >= 0

#define DEBUG_TYPE "bitwise"
#include <llvm/Pass.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/Support/CFG.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>
#include "CRange.h"

using namespace llvm;

static cl::opt<unsigned>
MaxIter("bitwise-max-iter", cl::Hidden, cl::init(100),
        cl::desc("The maximum number of iterations"));

namespace {

typedef SmallPtrSet<Value *, 64> ValueSet;

struct BitwiseAnalysis : FunctionPass {
	static char ID;
	BitwiseAnalysis() : FunctionPass(ID) {}

	virtual bool runOnFunction(Function &);
	virtual void releaseMemory();
	virtual void print(raw_ostream &, const Module *) const;

	CRange rangeof(Value *);
	bool shrink(Value *, const CRange &);

private:
	typedef DenseMap<Value *, CRange> RangeMap;
	RangeMap Ranges;
};

template <typename T>
struct PropagationVisitor : InstVisitor<T> {
public:
	ValueSet Todo;
protected:
	PropagationVisitor(BitwiseAnalysis &BA) : BA(BA) {}

	CRange rangeof(Value &V) { return rangeof(&V); }
	CRange rangeof(Value *V) { return BA.rangeof(V); }

	void shrink(Value &V, const CRange &CR) { shrink(&V, CR); }
	void shrink(Value *V, const CRange &CR) {
		if (BA.shrink(V, CR))
			Todo.insert(V);
	}

private:
	BitwiseAnalysis &BA;
};

struct ForwardVisitor : PropagationVisitor<ForwardVisitor> {
	explicit ForwardVisitor(BitwiseAnalysis &BA) : PropagationVisitor(BA) {}

	void visitPHINode(PHINode &);

	void visitAdd(BinaryOperator &I) {
		const CRange &L = rangeof(I.getOperand(0));
		const CRange &R = rangeof(I.getOperand(1));
		CRange CR = L.add(R);
		if (I.hasNoSignedWrap() && CR.isSignWrappedSet())
			CR = L.sadd(R);
		shrink(I, CR);
	}

	void visitSDiv(BinaryOperator &I) {
		Value *LHS = I.getOperand(0);
		Value *RHS = I.getOperand(1);
		unsigned N = I.getType()->getIntegerBitWidth();
		// RHS != 0.
		shrink(RHS, CRange(APInt::getNullValue(N)).inverse());
		// LHS == INT_MIN => RHS != -1.
		if (const APInt *Value = rangeof(LHS).getSingleElement())
			if (Value->isMinSignedValue())
				shrink(RHS, CRange(APInt::getAllOnesValue(N)).inverse());
		// RHS == -1 => LHS != INT_MIN.
		if (const APInt *Value = rangeof(RHS).getSingleElement())
			if (Value->isAllOnesValue())
				shrink(LHS, CRange(APInt::getSignedMinValue(N)).inverse());
		// Div.
		shrink(I, rangeof(LHS).sdiv(rangeof(RHS)));
	}

	void visitICmpInst(ICmpInst &I) {
		Value *LHS = I.getOperand(0);
		Value *RHS = I.getOperand(1);
		// Skip pointers.
		if (!LHS->getType()->isIntegerTy())
			return;
		assert(I.getType()->isIntegerTy(1));
		// LHS and (pred RHS) share nothing => comparison is false.
		// NB: LHS in (pred RHS) does not imply the comparison is true.
		CRange CR0 = CRange::makeICmpRegion(I.getPredicate(), rangeof(RHS));
		if (rangeof(LHS).intersectWith(CR0).isEmptySet())
			shrink(I, CRange(APInt::getNullValue(1)));
		// LHS and (!pred RHS) share nothing => comparison is true.
		// NB: (!pred RHS) is not the same as !(pred RHS).
		//     (!< [1,2]) => (>= [1,2]) => [1, max]
		//     !(< [1,2]) => ![min, 2) => [2, max]
		CRange CR1 = CRange::makeICmpRegion(I.getInversePredicate(), rangeof(RHS));
		if (rangeof(LHS).intersectWith(CR1).isEmptySet())
			shrink(I, CRange(APInt::getAllOnesValue(1)));
	}
};

struct BackwardVisitor : PropagationVisitor<BackwardVisitor> {
	explicit BackwardVisitor(BitwiseAnalysis &BA) : PropagationVisitor(BA) {}

	void visitPHINode(PHINode &);
};

} // anonymous namespace

bool BitwiseAnalysis::runOnFunction(Function &F) {
	ForwardVisitor FV(*this);
	BackwardVisitor BV(*this);
	unsigned Iter = 0;

	// Iterate.
	for (;;) {
		// Perform forward propagation on each instruction.
		FV.Todo.clear();
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			Instruction *I = &*i;
			if (!I->getType()->isIntegerTy())
				continue;
			FV.visit(I);
		}
		// Merge previous backward todo list.
		FV.Todo.insert(BV.Todo.begin(), BV.Todo.end());
		// Perform backward propagation.
		BV.Todo.clear();
		for (ValueSet::iterator i = FV.Todo.begin(), e = FV.Todo.end(); i != e; ++i)
			if (Instruction *I = dyn_cast<Instruction>(*i))
				BV.visit(I);
		// Reach fixed point.
		if (FV.Todo.empty() && BV.Todo.empty())
			break;
		// Reach max iteration.
		if (++Iter == MaxIter)
			break;
	}
	return false;
}

void BitwiseAnalysis::releaseMemory() {
	Ranges.clear();
}

void BitwiseAnalysis::print(raw_ostream &OS, const Module *) const {
	for (RangeMap::const_iterator i = Ranges.begin(), e = Ranges.end(); i != e; ++i) {
		const CRange &CR = i->second;
		OS << *i->first << "\n  -->  ";
		if (const APInt *Value = CR.getSingleElement()) {
			if (Value->getBitWidth() == 1)
				OS << (Value->getBoolValue() ? "true" : "false");
			else
				OS << *Value;
		} else if (CR.isEmptySet()) {
			OS << "undef";
		} else {
			OS << CR;
		}
		OS << "\n";
	}
}

CRange BitwiseAnalysis::rangeof(Value *V) {
	unsigned N = V->getType()->getIntegerBitWidth();
	if (ConstantInt *CI = dyn_cast<ConstantInt>(V))
		return CRange(CI->getValue());
	RangeMap::iterator i = Ranges.find(V);
	if (i != Ranges.end())
		return i->second;
	// Range metadata on load.
	if (Instruction *I = dyn_cast<Instruction>(V)) {
		if (MDNode *MD = I->getMetadata(LLVMContext::MD_range)) {
			unsigned NumOps = MD->getNumOperands();
			assert(NumOps && NumOps % 2 == 0);
			CRange CR = CRange::makeEmptySet(N);
			for (unsigned i = 0; i != NumOps; i += 2) {
				ConstantInt *Lower = cast<ConstantInt>(MD->getOperand(i));
				ConstantInt *Upper = cast<ConstantInt>(MD->getOperand(i + 1));
				CR = CR.unionWith(CRange(Lower->getValue(), Upper->getValue()));
			}
			return CR;
		}
	}
	return CRange::makeFullSet(N);
}

bool BitwiseAnalysis::shrink(Value *V, const CRange &CR) {
	if (isa<ConstantInt>(V) || CR.isFullSet())
		return false;
	RangeMap::iterator i;
	bool Inserted;
	tie(i, Inserted) = Ranges.insert(std::make_pair(V, CR));
	if (Inserted)
		return true;
	const CRange &OldCR = i->second;
	const CRange &NewCR = OldCR.intersectWith(CR);
	if (NewCR == OldCR)
		return false;
	i->second = NewCR;
	return true;
}

void ForwardVisitor::visitPHINode(PHINode &I) {
	Type *T = I.getType();
	if (!T->isIntegerTy())
		return;
	unsigned NumIncoming = I.getNumIncomingValues();
	// Merge subranges for phi.
	if (NumIncoming > 1) {
		// Start with empty set.
		CRange CR = CRange::makeEmptySet(T->getIntegerBitWidth());
		for (unsigned i = 0; i != NumIncoming; ++i)
			CR = CR.unionWith(rangeof(I.getIncomingValue(i)));
		shrink(I, CR);
		return;
	}
	// Pi node: compute refinement range.
	Value *V = I.getIncomingValue(0);
	BasicBlock *IncomingBB = I.getIncomingBlock(0);
	BasicBlock *BB = I.getParent();
	assert(BB->getSinglePredecessor() == IncomingBB);
	TerminatorInst *TI = IncomingBB->getTerminator();
	if (SwitchInst *SI = dyn_cast<SwitchInst>(TI)) {
		if (ConstantInt *CI = SI->findCaseDest(BB))
			shrink(I, ConstantRange(CI->getValue()));
		return;
	}
	if (BranchInst *BI = dyn_cast<BranchInst>(TI)) {
		if (BI->isUnconditional())
			return;
		ICmpInst *ICI = dyn_cast<ICmpInst>(BI->getCondition());
		if (!ICI)
			return;
		Value *LHS = ICI->getOperand(0);
		Value *RHS = ICI->getOperand(1);
		Value *Other;
		CmpInst::Predicate Predicate;
		if (V == LHS) {
			Predicate = ICI->getPredicate();
			Other = RHS;
		} else if (V == RHS) {
			Predicate = ICI->getSwappedPredicate();
			Other = LHS;
		} else {
			return;
		}
		// Inverse if on the false branch.
		if (BB == BI->getSuccessor(1))
			Predicate = CmpInst::getInversePredicate(Predicate);
		ConstantRange PiCR = ConstantRange::makeICmpRegion(
			Predicate, rangeof(Other));
		shrink(I, rangeof(V).intersectWith(PiCR));
		return;
	}
}

void BackwardVisitor::visitPHINode(PHINode &I) {
	Type *T = I.getType();
	if (!T->isIntegerTy())
		return;
	unsigned NumIncoming = I.getNumIncomingValues();
	// Shrink each subrange.
	if (NumIncoming > 1) {
		CRange CR = rangeof(I);
		for (unsigned i = 0; i != NumIncoming; ++i)
			shrink(I.getIncomingValue(i), CR);
		return;
	}
	// Shrink V's range from the union of pi nodes.
	Value *V = I.getIncomingValue(0);
	BasicBlock *IncomingBB = I.getIncomingBlock(0);
	assert(IncomingBB);
	CRange CR = CRange::makeEmptySet(T->getIntegerBitWidth());
	for (succ_iterator i = succ_begin(IncomingBB), e = succ_end(IncomingBB); i != e; ++i) {
		BasicBlock *BB = *i;
		assert(IncomingBB == BB->getSinglePredecessor());
		// Locate each pi node.
		PHINode *PI = NULL;
		for (BasicBlock::iterator bi = BB->begin(), be = BB->end(); bi != be; ++bi) {
			PI = cast<PHINode>(bi);
			if (PI->getNumIncomingValues() == 1 && PI->getOperand(0) == V)
				break;
		}
		CR = CR.unionWith(rangeof(PI));
	}
	shrink(V, CR);
}

char BitwiseAnalysis::ID;

static RegisterPass<BitwiseAnalysis>
X("bitwise", "Bitwise range analysis");
