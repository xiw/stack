#define DEBUG_TYPE "int-libcalls"
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct NamedParam {
	const char *Name;
	unsigned int Index;
};

struct IntLibcalls : ModulePass {
	static char ID;
	IntLibcalls() : ModulePass(ID) {}

	bool runOnModule(Module &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	void rewriteSize(Function *F);
	void rewriteSizeAt(CallInst *I, NamedParam *NPs);
};

} // anonymous namespace

static NamedParam LLVMSize[] = {
	{"llvm.memcpy.p0i8.p0i8.i32", 2},
	{"llvm.memcpy.p0i8.p0i8.i64", 2},
	{"llvm.memmove.p0i8.p0i8.i32", 2},
	{"llvm.memmove.p0i8.p0i8.i64", 2},
	{"llvm.memset.p0i8.i32", 2},
	{"llvm.memset.p0i8.i64", 2},
	{0, 0}
};

static NamedParam LinuxSize[] = {
	{"copy_from_user", 2},
	{"copy_in_user", 2},
	{"copy_to_user", 2},
	{"dma_free_coherent", 1},
	{"memcpy", 2},
	{"memcpy_fromiovec", 2},
	{"memcpy_fromiovecend", 2},
	{"memcpy_fromiovecend", 3},
	{"memcpy_toiovec", 2},
	{"memcpy_toiovecend", 2},
	{"memcpy_toiovecend", 3},	
	{"memmove", 2},
	{"memset", 2},
	{"pci_free_consistent", 1},
	{"sock_alloc_send_skb", 1},
	{"sock_alloc_send_pskb", 1},
	{"sock_alloc_send_pskb", 2},
	{0, 0}
};

void insertIntSat(Value *, Instruction *, StringRef);

bool IntLibcalls::runOnModule(Module &M) {
	BuilderTy TheBuilder(M.getContext());
	Builder = &TheBuilder;
	for (Module::iterator i = M.begin(), e = M.end(); i != e; ++i) {
		Function *F = i;
		if (F->empty())
			continue;
		rewriteSize(F);
	}
	return true;
}

void IntLibcalls::rewriteSize(Function *F) {
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		CallInst *I = dyn_cast<CallInst>(&*i);
		if (I && I->getCalledFunction()) {
			rewriteSizeAt(I, LLVMSize);
			rewriteSizeAt(I, LinuxSize);
		}
	}
}

void IntLibcalls::rewriteSizeAt(CallInst *I, NamedParam *NPs) {
	StringRef Name = I->getCalledFunction()->getName();
	for (NamedParam *NP = NPs; NP->Name; ++NP) {
		if (Name != NP->Name)
			continue;
		Value *Arg = I->getArgOperand(NP->Index);
		Type *T = Arg->getType();
		assert(T->isIntegerTy());
		Builder->SetInsertPoint(I);
		Value *V = Builder->CreateICmpSLT(Arg, Constant::getNullValue(T));
		insertIntSat(V, I, "size");
	}
}

char IntLibcalls::ID;

static RegisterPass<IntLibcalls>
X("int-libcalls", "Rewrite well-known library calls");
