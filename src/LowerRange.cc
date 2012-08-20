// Assume programmers annotate their code using the following macros.
//
//   #define __range(lo, hi)      __attribute__((annotate("range:"#lo":"#hi)))
//
//   #define __range_assert(cond) if (cond) __builtin_trap();
//
// Clang emits the annotate attribute as a call to llvm.ptr.annotation().
// This pass lowers the call to range metadata on load instructions, which
// KINT understands and uses to reduce false errors.
//
// __range applies to structure fields and global variables, such as:
//
//   struct A {
//     int x __range(100, 200);
//     ...
//   };
//
// __range_assert applies to function parameters and return values.
//
//   int foo_impl(int x)
//   {
//     __range_assert(x > 100 && x < 200);
//     ...
//   }
//
//   int foo(int x)
//   {
//     int r = foo_impl(x);
//     __range_assert(r >= 0);
//     return r;
//   }
//
// A caller of foo() then inlines the (r >= 0) assertion.

#define DEBUG_TYPE "lower-range"
#include <llvm/GlobalVariable.h>
#include <llvm/IntrinsicInst.h>
#include <llvm/MDBuilder.h>
#include <llvm/Pass.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct LowerRange : FunctionPass {
	static char ID;
	LowerRange() : FunctionPass(ID) {}

	bool runOnFunction(Function &);
};

} // anonymous namespace

static StringRef getCString(Value *V) {
	GlobalVariable *GV = dyn_cast<GlobalVariable>(V->stripPointerCasts());
	if (GV && GV->hasInitializer()) {
		ConstantDataSequential *CDS = dyn_cast<ConstantDataSequential>(GV->getInitializer());
		if (CDS && CDS->isCString())
			return CDS->getAsCString();
	}
	return StringRef();
}

static void rewrite(Value *V, StringRef LoStr, StringRef HiStr) {
	for (Instruction::use_iterator ui = V->use_begin(), ue = V->use_end(); ui != ue; ++ui) {
		if (CastInst *CI = dyn_cast<CastInst>(*ui))
			rewrite(CI, LoStr, HiStr);
		LoadInst *I = dyn_cast<LoadInst>(*ui);
		if (!I)
			continue;
		IntegerType *T = dyn_cast<IntegerType>(I->getType());
		if (!T)
			continue;
		unsigned n = T->getBitWidth();
		APInt Lo(n, 0), Hi(n, 0);
		LoStr.getAsInteger(0, Lo);
		HiStr.getAsInteger(0, Hi);
		if (Lo.getBitWidth() > n)
			Lo = Lo.trunc(n);
		if (Hi.getBitWidth() > n)
			Hi = Hi.trunc(n);
		MDNode *MD = MDBuilder(I->getContext()).createRange(Lo, Hi);
		I->setMetadata("range", MD);
	}
}

bool LowerRange::runOnFunction(Function &F) {
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		IntrinsicInst *I = dyn_cast<IntrinsicInst>(&*i++);
		if (!I || I->getIntrinsicID() != Intrinsic::ptr_annotation)
			continue;
		StringRef Anno = getCString(I->getOperand(1));
		if (!Anno.startswith("range:"))
			continue;
		StringRef LoStr, HiStr;
		tie(LoStr, HiStr) = Anno.substr(6).split(':');
		rewrite(I, LoStr, HiStr);
		Value *V = I->getOperand(0);
		I->replaceAllUsesWith(V);
		I->eraseFromParent();
		Changed = true;
	}
	return Changed;
}

char LowerRange::ID;

static RegisterPass<LowerRange>
X("lower-range", "Lower range annotations");
