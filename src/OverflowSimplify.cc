#define DEBUG_TYPE "overflow-simplify"
#include <llvm/IntrinsicInst.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

namespace {

struct OverflowSimplify : FunctionPass {
	static char ID;
	OverflowSimplify() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	Instruction *simplify(Intrinsic::ID, Constant *, Value *);
	bool canonicalize(Intrinsic::ID, Value *, Value *, IntrinsicInst *);
};

} // anonymous namespace

bool OverflowSimplify::runOnFunction(Function &F) {
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		IntrinsicInst *I = dyn_cast<IntrinsicInst>(&*i);
		++i;
		if (!I)
			continue;
		Intrinsic::ID ID = I->getIntrinsicID();
		Value *LHS = I->getArgOperand(0), *RHS = I->getArgOperand(1);
		Instruction *NewInst;
		if (Constant *C = dyn_cast<Constant>(LHS))
			NewInst = simplify(ID, C, RHS);
		else if (Constant *C = dyn_cast<Constant>(RHS))
			NewInst = simplify(ID, C, LHS);
		else {
			Changed |= canonicalize(ID, LHS, RHS, I);
			continue;
		}
		if (!NewInst)
			continue;
		NewInst->setDebugLoc(I->getDebugLoc());
		ReplaceInstWithInst(I, NewInst);
		Changed = true;
	}
	return Changed;
}

Instruction *OverflowSimplify::simplify(Intrinsic::ID ID, Constant *C, Value *V) {
	// TODO: fold into a simple cmp.
	return 0;
}

bool OverflowSimplify::canonicalize(Intrinsic::ID ID, Value *L, Value *R, IntrinsicInst *I) {
	switch (ID) {
	default: return false;
	case Intrinsic::sadd_with_overflow:
	case Intrinsic::uadd_with_overflow:
	case Intrinsic::smul_with_overflow:
	case Intrinsic::umul_with_overflow:
		break;
	}
	SmallString<16> LS, RS;
	{
		raw_svector_ostream OS(LS);
		WriteAsOperand(OS, L, false);
	}
	{
		raw_svector_ostream OS(RS);
		WriteAsOperand(OS, R, false);
	}
	if (LS <= RS)
		return false;
	I->setArgOperand(0, R);
	I->setArgOperand(1, L);
	return true;
}

char OverflowSimplify::ID;

static RegisterPass<OverflowSimplify>
X("overflow-simplify", "Canonicalize overflow intrinsics");
