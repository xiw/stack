#include <llvm/IRBuilder.h>
#include <llvm/Intrinsics.h>
#include <llvm/Pass.h>

llvm::Function *getBugOn(llvm::Module *);
llvm::Function *getOrInsertBugOn(llvm::Module *);

struct BugOnPass : llvm::FunctionPass {
	BugOnPass(char &ID) : llvm::FunctionPass(ID), BugOn(NULL) {}

	virtual void getAnalysisUsage(llvm::AnalysisUsage &) const;
	virtual bool runOnFunction(llvm::Function &);

protected:
	typedef BugOnPass super;
	typedef llvm::IRBuilder<> BuilderTy;
	typedef llvm::Value Value;
	typedef llvm::Instruction Instruction;
	BuilderTy *Builder;

	virtual bool visit(Instruction *) = 0;

	bool insert(Value *, llvm::StringRef Bug);
	llvm::Module *getModule();
	Instruction *setInsertPoint(Instruction *);
	Instruction *setInsertPointAfter(Instruction *);

	Value *createIsNull(Value *);
	Value *createIsNotNull(Value *);
	Value *createIsZero(Value *);
	Value *createIsWrap(llvm::Intrinsic::ID, Value *, Value *);
	Value *createIsSAddWrap(Value *, Value *);
	Value *createIsUAddWrap(Value *, Value *);
	Value *createIsSSubWrap(Value *, Value *);
	Value *createIsUSubWrap(Value *, Value *);
	Value *createIsSMulWrap(Value *, Value *);
	Value *createIsUMulWrap(Value *, Value *);
	Value *createIsSDivWrap(Value *, Value *);
	Value *createAnd(Value *, Value *);
	Value *createSExtOrTrunc(Value *, llvm::IntegerType *);

private:
	llvm::Function *BugOn;
	unsigned int MD_bug;
};
