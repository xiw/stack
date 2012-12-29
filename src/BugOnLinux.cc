#define DEBUG_TYPE "bugon-linux"
#include "BugOn.h"
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/Module.h>
#include <llvm/ADT/DenseMap.h>

using namespace llvm;

namespace {

struct BugOnLinux : BugOnPass {
	static char ID;
	BugOnLinux() : BugOnPass(ID) {}

	virtual bool doInitialization(Module &);
	virtual bool runOnInstruction(Instruction *);

private:
	typedef bool (BugOnLinux::*handler_t)(CallInst *);
	DenseMap<Function *, handler_t> Handlers;

	bool visitFfz(CallInst *);
	bool visitDmaPoolCreate(CallInst *);
};

} // anonymous namespace

bool BugOnLinux::doInitialization(Module &M) {
#define HANDLER(method, name) \
	if (Function *F = M.getFunction(name)) \
		Handlers[F] = &BugOnLinux::method;

	// bitops
	HANDLER(visitFfz, "ffz");

	HANDLER(visitDmaPoolCreate, "dma_pool_create");

#undef HANDLER
	return false;
}

bool BugOnLinux::runOnInstruction(Instruction *I) {
	CallInst *CI = dyn_cast<CallInst>(I);
	if (!CI)
		return false;
	Function *F = CI->getCalledFunction();
	if (!F)
		return false;
	handler_t Handler = Handlers.lookup(F);
	if (!Handler)
		return false;
	return (this->*Handler)(CI);
}

// ffz(x): cttz(~x).
// x cannot be ~0.
bool BugOnLinux::visitFfz(CallInst *I) {
	if (I->getNumArgOperands() != 1)
		return false;
	IntegerType *T = dyn_cast<IntegerType>(I->getType());
	if (!T)
		return false;
	Value *R = I->getArgOperand(0);
	if (T != R->getType())
		return false;
	Value *NotR = Builder->CreateNot(R);
	Value *Pre = createIsZero(NotR);
	insert(Pre, I->getCalledFunction()->getName());
	Function *F = Intrinsic::getDeclaration(getModule(), Intrinsic::cttz, T);
	CallInst *NewInst = Builder->CreateCall2(F, NotR, Builder->getTrue());
	Value *Post = Builder->CreateICmpUGE(NewInst, ConstantInt::get(T, T->getBitWidth()));
	insert(Post, I->getCalledFunction()->getName());
	I->replaceAllUsesWith(NewInst);
	I->eraseFromParent();
	return true;
}

// dma_pool_create(name, dev, size, align, allocation): dev == null
bool BugOnLinux::visitDmaPoolCreate(CallInst *I) {
	if (I->getNumArgOperands() != 5)
		return false;
	Value *Dev = I->getArgOperand(1);
	if (!isa<PointerType>(Dev->getType()))
		return false;
	Value *V = createIsNull(Dev);
	return insert(V, I->getCalledFunction()->getName());
}

char BugOnLinux::ID;

static RegisterPass<BugOnLinux>
X("bugon-linux", "Insert bugon calls for Linux API");
