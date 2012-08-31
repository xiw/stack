#define DEBUG_TYPE "int-rewrite"
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/GetElementPtrTypeIterator.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

namespace {

struct IntRewrite : FunctionPass {
	static char ID;
	IntRewrite() : FunctionPass(ID) { }

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	Value *insertOverflowCheck(Instruction *, Intrinsic::ID, Intrinsic::ID);
	Value *insertSDivCheck(Instruction *);
	Value *insertShiftCheck(Value *);
	Value *insertArrayCheck(Instruction *);
};

} // anonymous namespace

void insertIntSat(Value *V, Instruction *IP, const DebugLoc &DbgLoc, StringRef Anno) {
	Module *M = IP->getParent()->getParent()->getParent();
	LLVMContext &C = M->getContext();
	FunctionType *T = FunctionType::get(Type::getVoidTy(C), Type::getInt1Ty(C), false);
	Constant *F = M->getOrInsertFunction("int.sat", T);
	CallInst *I = CallInst::Create(F, V, "", IP);
	I->setDebugLoc(DbgLoc);
	// Embed operation name in metadata.
	MDNode *MD = MDNode::get(C, MDString::get(C, Anno));
	I->setMetadata("int", MD);
}

static std::string getOpcodeName(Instruction *I) {
	std::string Anno = I->getOpcodeName();
	switch (I->getOpcode()) {
	case Instruction::Add:
	case Instruction::Sub:
	case Instruction::Mul:
		Anno = (cast<BinaryOperator>(I)->hasNoSignedWrap() ? "s" : "u") + Anno;
		break;
	case Instruction::GetElementPtr:
		Anno = "array";
		break;
	}
	return Anno;
}

bool IntRewrite::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		if (!isa<BinaryOperator>(I) && !isa<GetElementPtrInst>(I))
			continue;
		Builder->SetInsertPoint(I);
		Value *V = NULL;
		switch (I->getOpcode()) {
		default: continue;
		case Instruction::Add:
			V = insertOverflowCheck(I,
				Intrinsic::sadd_with_overflow,
				Intrinsic::uadd_with_overflow);
			break;
		case Instruction::Sub:
			V = insertOverflowCheck(I,
				Intrinsic::ssub_with_overflow,
				Intrinsic::usub_with_overflow);
			break;
		case Instruction::Mul:
			V = insertOverflowCheck(I,
				Intrinsic::smul_with_overflow,
				Intrinsic::umul_with_overflow);
			break;
		case Instruction::SDiv:
			V = insertSDivCheck(I);
			break;
		// Div by zero is not rewitten here, but done in a separate
		// pass before -overflow-idiom, which runs before this pass
		// and rewrites divisions as multiplications.
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
			V = insertShiftCheck(I->getOperand(1));
			break;
		case Instruction::GetElementPtr:
			V = insertArrayCheck(I);
			break;
		}
		if (!V)
			continue;
		insertIntSat(V, I, I->getDebugLoc(), getOpcodeName(I));
		Changed = true;
	}
	return Changed;
}

Value *IntRewrite::insertOverflowCheck(Instruction *I, Intrinsic::ID SID, Intrinsic::ID UID) {
	BinaryOperator *BO = cast<BinaryOperator>(I);
	Intrinsic::ID ID = BO->hasNoSignedWrap()? SID: UID;
	Module *M = I->getParent()->getParent()->getParent();
	Function *F = Intrinsic::getDeclaration(M, ID, BO->getType());
	CallInst *CI = Builder->CreateCall2(F, BO->getOperand(0), BO->getOperand(1));
	return Builder->CreateExtractValue(CI, 1);
}

Value *IntRewrite::insertSDivCheck(Instruction *I) {
	Value *L = I->getOperand(0), *R = I->getOperand(1);
	IntegerType *T = cast<IntegerType>(I->getType());
	unsigned n = T->getBitWidth();
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(n));
	Constant *MinusOne = Constant::getAllOnesValue(T);
	return Builder->CreateAnd(
		Builder->CreateICmpEQ(L, SMin),
		Builder->CreateICmpEQ(R, MinusOne)
	);
}

Value *IntRewrite::insertShiftCheck(Value *V) {
	IntegerType *T = cast<IntegerType>(V->getType());
	Constant *C = ConstantInt::get(T, T->getBitWidth());
	return Builder->CreateICmpUGE(V, C);
}

Value *IntRewrite::insertArrayCheck(Instruction *I) {
	Value *V = NULL;
	gep_type_iterator i = gep_type_begin(I), e = gep_type_end(I);
	for (; i != e; ++i) {
		// For arr[idx], check idx >u n.  Here we don't use idx >= n
		// since it is unclear if the pointer will be dereferenced.
		ArrayType *T = dyn_cast<ArrayType>(*i);
		if (!T)
			continue;
		uint64_t n = T->getNumElements();
		Value *Idx = i.getOperand();
		Type *IdxTy = Idx->getType();
		Value *Check = Builder->CreateICmpUGT(Idx, ConstantInt::get(IdxTy, n));
		if (V)
			V = Builder->CreateOr(V, Check);
		else
			V = Check;
	}
	return V;
}

char IntRewrite::ID;

static RegisterPass<IntRewrite>
X("int-rewrite", "Insert int.sat calls", false, false);
