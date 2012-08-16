#define DEBUG_TYPE "int-rewrite"
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

namespace {

struct IntRewrite : FunctionPass {
	static char ID;
	IntRewrite() : FunctionPass(ID) { }

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool doInitialization(Module &M) {
		this->M = &M;
		this->MD_int = M.getContext().getMDKindID("int");
		return false;
	}

	virtual bool runOnFunction(Function &);

private:
	Module *M;
	unsigned MD_int;

	void insertTrap(Instruction *Check) {
		LLVMContext &VMCtx = M->getContext();
		BasicBlock *Pred = Check->getParent();
		Instruction *IP = ++BasicBlock::iterator(Check);
		Function *F = Pred->getParent();
		BasicBlock *Succ = SplitBlock(Pred, IP, this);
		// Create a new BB containing a trap instruction.
		BasicBlock *BB = BasicBlock::Create(VMCtx, "", F, Succ);
		Function *Trap = Intrinsic::getDeclaration(M, Intrinsic::debugtrap);
		Instruction *I = CallInst::Create(Trap, "", BB);
		I->setDebugLoc(Check->getDebugLoc());
		I->setMetadata(MD_int, Check->getMetadata(MD_int));
		BranchInst::Create(Succ, BB);
		// Create a new conditional br in Pred.
		Pred->getTerminator()->eraseFromParent();
		BranchInst::Create(BB, Succ, Check, Pred);
	}

	Instruction *insertOverflowCheck(Instruction *, Intrinsic::ID, Intrinsic::ID);
	
};

} // anonymous namespace

bool IntRewrite::runOnFunction(Function &F) {
	SmallVector<Instruction *, 16> Checks;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		Instruction *Check = NULL;
		switch (I->getOpcode()) {
		default: continue;
		case Instruction::Add:
			Check = insertOverflowCheck(I,
				Intrinsic::sadd_with_overflow,
				Intrinsic::uadd_with_overflow);
			break;
		case Instruction::Sub:
			Check = insertOverflowCheck(I,
				Intrinsic::ssub_with_overflow,
				Intrinsic::usub_with_overflow);
			break;
		case Instruction::Mul:
			Check = insertOverflowCheck(I,
				Intrinsic::smul_with_overflow,
				Intrinsic::umul_with_overflow);
			break;
		}
		Checks.push_back(Check);
	}
	for (size_t i = 0, n = Checks.size(); i != n; ++i)
		insertTrap(Checks[i]);
	return !Checks.empty();
}

Instruction *IntRewrite::insertOverflowCheck(Instruction *I, Intrinsic::ID SID, Intrinsic::ID UID) {
	BinaryOperator *BO = cast<BinaryOperator>(I);
	Intrinsic::ID ID = BO->hasNoSignedWrap()? SID: UID;
	Function *F = Intrinsic::getDeclaration(M, ID, BO->getType());
	Value *Args[] = {BO->getOperand(0), BO->getOperand(1)};
	CallInst *CI = CallInst::Create(F, Args, "", BO);
	ExtractValueInst *EVI = ExtractValueInst::Create(CI, 1, "", BO);
	StringRef Anno = F->getName().substr(5, 4);
	EVI->setDebugLoc(I->getDebugLoc());
	LLVMContext &VMCtx = M->getContext();
	MDNode *MD = MDNode::get(VMCtx, MDString::get(VMCtx, Anno));
	EVI->setMetadata(MD_int, MD);
	return EVI;
}

char IntRewrite::ID;

static RegisterPass<IntRewrite>
X("int-rewrite", "Insert llvm.debugtrap calls", false, false);
