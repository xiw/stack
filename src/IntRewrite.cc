#define DEBUG_TYPE "int-rewrite"
#include <llvm/DerivedTypes.h>
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct IntRewrite : FunctionPass {
	static char ID;
	IntRewrite() : FunctionPass(ID) { }

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}

	virtual bool doInitialization(Module &M) {
		this->IntSat = 0;
		this->M = &M;
		this->MD_int = M.getContext().getMDKindID("int");
		return false;
	}

	virtual bool runOnFunction(Function &);

private:
	Function *IntSat;
	Module *M;
	unsigned MD_int;

	Function *getSat() {
		if (!IntSat) {
			LLVMContext &VMCtx = M->getContext();
			Type *VoidTy = Type::getVoidTy(VMCtx);
			Type *BoolTy = Type::getInt1Ty(VMCtx);
			Constant *C = M->getOrInsertFunction("int.sat", VoidTy, BoolTy, NULL);
			IntSat = cast<Function>(C);
			IntSat->setDoesNotThrow();
		}
		return IntSat;
	}

	void insertSat(Value *V, Instruction *IP, StringRef Anno) {
		Instruction *I = CallInst::Create(getSat(), V, "", IP);
		I->setDebugLoc(IP->getDebugLoc());
		LLVMContext &VMCtx = M->getContext();
		MDNode *MD = MDNode::get(VMCtx, MDString::get(VMCtx, Anno));
		I->setMetadata(MD_int, MD);
	}

	bool insertOverflowSat(Instruction *, Intrinsic::ID, Intrinsic::ID);
	
};

} // anonymous namespace

bool IntRewrite::runOnFunction(Function &F) {
	inst_iterator i = inst_begin(F), e = inst_end(F);
	bool Changed = false;
	for (; i != e; ++i) {
		Instruction *I = &*i;
		switch (I->getOpcode()) {
		default: break;
		case Instruction::Add:
			Changed = insertOverflowSat(I,
				Intrinsic::sadd_with_overflow,
				Intrinsic::uadd_with_overflow);
			break;
		case Instruction::Sub:
			Changed = insertOverflowSat(I,
				Intrinsic::ssub_with_overflow,
				Intrinsic::usub_with_overflow);
			break;
		case Instruction::Mul:
			Changed = insertOverflowSat(I,
				Intrinsic::smul_with_overflow,
				Intrinsic::umul_with_overflow);
			break;
		}
	}
	return Changed;
}

bool IntRewrite::insertOverflowSat(Instruction *I, Intrinsic::ID SID, Intrinsic::ID UID) {
	BinaryOperator *BO = cast<BinaryOperator>(I);
	Intrinsic::ID ID = BO->hasNoSignedWrap()? SID: UID;
	Function *F = Intrinsic::getDeclaration(M, ID, BO->getType());
	Value *Args[] = {BO->getOperand(0), BO->getOperand(1)};
	CallInst *CI = CallInst::Create(F, Args, "", BO);
	ExtractValueInst *EVI = ExtractValueInst::Create(CI, 1, "", BO);
	insertSat(EVI, BO, F->getName().substr(5, 4));
	return true;
}

char IntRewrite::ID;

static RegisterPass<IntRewrite>
X("int-rewrite", "Insert int.sat calls", false, false);
