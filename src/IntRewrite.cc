#define DEBUG_TYPE "int-rewrite"
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/GetElementPtrTypeIterator.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

static cl::opt<bool>
WrapOpt("fwrapv", cl::desc("Use two's complement for signed integers"));

namespace {

struct IntRewrite : FunctionPass {
	static char ID;
	IntRewrite() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDominatorTreePass(Registry);
		initializeLoopInfoPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.addRequired<DominatorTree>();
		AU.addRequired<LoopInfo>();
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	DominatorTree *DT;
	LoopInfo *LI;
	TargetData *TD;

	bool insertOverflowCheck(Instruction *, Intrinsic::ID, Intrinsic::ID);
	bool insertDivCheck(Instruction *);
	bool insertShiftCheck(Instruction *);
	bool insertArrayCheck(Instruction *);

	bool isObservable(Value *);
};

} // anonymous namespace

static void insertIntSat(Value *V, Instruction *IP, StringRef Bug, const DebugLoc &DbgLoc) {
	Module *M = IP->getParent()->getParent()->getParent();
	LLVMContext &C = M->getContext();
	FunctionType *T = FunctionType::get(Type::getVoidTy(C), Type::getInt1Ty(C), false);
	Function *F = cast<Function>(M->getOrInsertFunction("int.sat", T));
	F->setDoesNotThrow();
	CallInst *I = CallInst::Create(F, V, "", IP);
	I->setDebugLoc(DbgLoc);
	// Embed operation name in metadata.
	MDNode *MD = MDNode::get(C, MDString::get(C, Bug));
	I->setMetadata("bug", MD);
}

void insertIntSat(Value *V, Instruction *I, StringRef Bug) {
	insertIntSat(V, I, Bug, I->getDebugLoc());
}

static void insertIntSat(Value *V, Instruction *I) {
	insertIntSat(V, I, I->getOpcodeName());
}

bool IntRewrite::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	DT = &getAnalysis<DominatorTree>();
	LI = &getAnalysis<LoopInfo>();
	TD = getAnalysisIfAvailable<TargetData>();
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
		case Instruction::UDiv:
			Changed |= insertDivCheck(I);
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

bool IntRewrite::isObservable(Value *V) {
	Instruction *I = dyn_cast<Instruction>(V);
	if (!I)
		return false;
	BasicBlock *BB = I->getParent();
	switch (I->getOpcode()) {
	default: break;
	case Instruction::Br:
	case Instruction::IndirectBr:
	case Instruction::Switch:
		if (Loop *L = LI->getLoopFor(BB)) {
			if (L->isLoopExiting(BB))
				return true;
		}
		return false;
	}
	// Default: observable if unsafe to speculately execute.
	return !isSafeToSpeculativelyExecute(I, TD);
}

bool IntRewrite::insertOverflowCheck(Instruction *I, Intrinsic::ID SID, Intrinsic::ID UID) {
	// Skip pointer subtraction, where LLVM converts both operands into
	// integers first.
	Value *L = I->getOperand(0), *R = I->getOperand(1);
	if (isa<PtrToIntInst>(L) || isa<PtrToIntInst>(R))
		return false;

	bool hasNSW = cast<BinaryOperator>(I)->hasNoSignedWrap();
	Intrinsic::ID ID = hasNSW ? SID : UID;
	Module *M = I->getParent()->getParent()->getParent();
	Function *F = Intrinsic::getDeclaration(M, ID, I->getType());
	CallInst *CI = Builder->CreateCall2(F, L, R);
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

	// Defer the check.
	SmallPtrSet<Value *, 16> Visited;
	SmallVector<Value *, 16> Worklist;
	typedef SmallPtrSet<BasicBlock *, 16> ObSet;
	ObSet ObPoints;
	BasicBlock *BB = I->getParent();

	Worklist.push_back(I);
	Visited.insert(I);
	while (!Worklist.empty()) {
		Value *E = Worklist.back();
		Worklist.pop_back();
		for (Value::use_iterator i = E->use_begin(), e = E->use_end(); i != e; ++i) {
			User *U = *i;
			// Observable point.
			if (isObservable(U)) {
				// U must be an instruction for now.
				BasicBlock *ObBB = cast<Instruction>(U)->getParent();
				// If the instruction's own BB is an observation
				// point, a check will be performed there, so
				// there is no need for other checks.
				//
				// If ObBB is not dominated by BB (e.g., due to
				// loops), fall back.
				if (ObBB == BB || !DT->dominates(BB, ObBB)) {
					insertIntSat(V, I, Anno);
					return true;
				}
				ObPoints.insert(ObBB);
				continue;
			}
			// Add to worklist if new.
			if (U->use_empty())
				continue;
			if (Visited.insert(U))
				Worklist.push_back(U);
		}
	}

	const DebugLoc &DbgLoc = I->getDebugLoc();
	for (ObSet::iterator i = ObPoints.begin(), e = ObPoints.end(); i != e; ++i) {
		BasicBlock *ObBB = *i;
		insertIntSat(V, ObBB->getTerminator(), Anno, DbgLoc);
	}
	return true;
}

bool IntRewrite::insertDivCheck(Instruction *I) {
	Value *R = I->getOperand(1);
	// R == 0.
	Value *V = Builder->CreateIsNull(R);
	// L == INT_MIN && R == -1.
	if (I->getOpcode() == Instruction::SDiv) {
		Value *L = I->getOperand(0);
		IntegerType *T = cast<IntegerType>(I->getType());
		unsigned n = T->getBitWidth();
		Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(n));
		Constant *MinusOne = Constant::getAllOnesValue(T);
		V = Builder->CreateOr(V, Builder->CreateAnd(
			Builder->CreateICmpEQ(L, SMin),
			Builder->CreateICmpEQ(R, MinusOne)));
	}
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
		assert(IdxTy->isIntegerTy());
		// a[0] or a[1] are weird idioms used at the end of a struct.
		// Use the maximum signed value instead for the upper bound.
		if (n <= 1)
			n = INT_MAX;
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
