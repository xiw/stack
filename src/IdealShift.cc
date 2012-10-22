// This pass changes the semantics of shifting instructions, by inserting
// select instructions.  If the shifting amount is oversized, the result
// is set to either 0 (for shl/lshr) or the sign (ashr).

#define DEBUG_TYPE "ideal-shift"
#include <llvm/Constants.h>
#include <llvm/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct IdealShift : FunctionPass {
	static char ID;
	IdealShift() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
	}

	virtual bool runOnFunction(Function &);
};

} // anonymous namespace

bool IdealShift::runOnFunction(Function &F) {
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ) {
		Instruction *I = &*i++;
		unsigned Opcode = I->getOpcode();
		switch (Opcode) {
		default: continue;
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
			break;
		}
		IntegerType *T = dyn_cast<IntegerType>(I->getType());
		if (!T)
			continue;
		Value *L = I->getOperand(0);
		Value *R = I->getOperand(1);
		if (isa<Constant>(R))
			continue;
		// Set the insert point to be after I since the result is used.
		Instruction *IP = &*i;
		unsigned n = T->getBitWidth();
		ConstantInt *BitWidth = ConstantInt::get(T, n);
		ICmpInst *Cond = new ICmpInst(IP, CmpInst::ICMP_UGE, R, BitWidth);
		Value *IdealVal;
		// The ideal value is zero for logical shifts,
		// and the sign for arithmetic shifts.
		if (Opcode == Instruction::AShr)
			IdealVal = BinaryOperator::CreateAShr(L, ConstantInt::get(T, n - 1), "", IP);
		else
			IdealVal = Constant::getNullValue(T);
		// The new select instruction will use I, but we need to replace
		// all other uses with this select.  So use a fake false value
		// first, and then set it to I after RAUW.
		SelectInst *SI = SelectInst::Create(Cond, IdealVal, IdealVal, "", IP);
		SI->setDebugLoc(I->getDebugLoc());
		I->replaceAllUsesWith(SI);
		SI->setOperand(2, I);
		Changed = true;
	}
	return Changed;
}

char IdealShift::ID;

static RegisterPass<IdealShift>
X("ideal-shift", "Rewrite shift instructions for ideal results for oversized shifting amount");
