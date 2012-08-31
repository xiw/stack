#define DEBUG_TYPE "int-rewrite"
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/GetElementPtrTypeIterator.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

static cl::opt<bool>
WrapOpt("fwrapv", cl::desc("Use two's complement for signed integers"));

namespace {

struct IntRewrite : FunctionPass {
	static char ID;
	IntRewrite() : FunctionPass(ID) { }

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	bool insertOverflowCheck(Instruction *, Intrinsic::ID, Intrinsic::ID);
	bool insertSDivCheck(Instruction *);
	bool insertShiftCheck(Instruction *);
	bool insertArrayCheck(Instruction *);
};

} // anonymous namespace

static void insertIntSat(Value *V, Instruction *IP, StringRef Anno, const DebugLoc &DbgLoc) {
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

void insertIntSat(Value *V, Instruction *I, StringRef Anno) {
	insertIntSat(V, I, Anno, I->getDebugLoc());
}

void insertIntSat(Value *V, Instruction *I) {
	insertIntSat(V, I, I->getOpcodeName());
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
		switch (I->getOpcode()) {
		default: continue;
		case Instruction::Add:
			Changed |= insertOverflowCheck(I,
				Intrinsic::sadd_with_overflow,
				Intrinsic::uadd_with_overflow);
			break;
		case Instruction::Sub:
			Changed |= insertOverflowCheck(I,
				Intrinsic::ssub_with_overflow,
				Intrinsic::usub_with_overflow);
			break;
		case Instruction::Mul:
			Changed |= insertOverflowCheck(I,
				Intrinsic::smul_with_overflow,
				Intrinsic::umul_with_overflow);
			break;
		case Instruction::SDiv:
			Changed |= insertSDivCheck(I);
			break;
		// Div by zero is not rewitten here, but done in a separate
		// pass before -overflow-idiom, which runs before this pass
		// and rewrites divisions as multiplications.
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
			Changed |= insertShiftCheck(I);
			break;
		case Instruction::GetElementPtr:
			Changed |= insertArrayCheck(I);
			break;
		}
	}
	return Changed;
}

bool IntRewrite::insertOverflowCheck(Instruction *I, Intrinsic::ID SID, Intrinsic::ID UID) {
	bool hasNSW = cast<BinaryOperator>(I)->hasNoSignedWrap();
	Intrinsic::ID ID = hasNSW ? SID : UID;
	Module *M = I->getParent()->getParent()->getParent();
	Function *F = Intrinsic::getDeclaration(M, ID, I->getType());
	CallInst *CI = Builder->CreateCall2(F, I->getOperand(0), I->getOperand(1));
	Value *V = Builder->CreateExtractValue(CI, 1);
	// llvm.[s|u][add|sub|mul].with.overflow.*
	StringRef Anno = F->getName().substr(5, 4);
	if (hasNSW) {
		// Insert the check eagerly for signed integer overflow,
		// if -fwrapv is not given.
		if (!WrapOpt) {
			insertIntSat(V, I, Anno);
			return true;
		}
		// Clear NSW flag given -fwrapv.
		cast<BinaryOperator>(I)->setHasNoSignedWrap(false);
	}
	// TODO: Defer the check.
	insertIntSat(V, I, Anno);
	return true;
}

bool IntRewrite::insertSDivCheck(Instruction *I) {
	Value *L = I->getOperand(0), *R = I->getOperand(1);
	IntegerType *T = cast<IntegerType>(I->getType());
	unsigned n = T->getBitWidth();
	Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(n));
	Constant *MinusOne = Constant::getAllOnesValue(T);
	Value *V = Builder->CreateAnd(
		Builder->CreateICmpEQ(L, SMin),
		Builder->CreateICmpEQ(R, MinusOne)
	);
	insertIntSat(V, I);
	return true;
}

bool IntRewrite::insertShiftCheck(Instruction *I) {
	Value *Amount = I->getOperand(1);
	IntegerType *T = cast<IntegerType>(Amount->getType());
	Constant *C = ConstantInt::get(T, T->getBitWidth());
	Value *V = Builder->CreateICmpUGE(Amount, C);
	insertIntSat(V, I);
	return true;
}

bool IntRewrite::insertArrayCheck(Instruction *I) {
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
	if (!V)
		return false;
	insertIntSat(V, I, "array");
	return true;
}

char IntRewrite::ID;

static RegisterPass<IntRewrite>
X("int-rewrite", "Insert int.sat calls", false, false);
