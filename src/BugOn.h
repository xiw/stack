#include <llvm/Function.h>
#include <llvm/IRBuilder.h>
#include <llvm/Intrinsics.h>
#include <llvm/Pass.h>

llvm::Function *getBugOn(const llvm::Module *);
llvm::Function *getOrInsertBugOn(llvm::Module *);

class BugOnInst : public llvm::CallInst {
	typedef llvm::CallInst CallInst;
	typedef llvm::Function Function;
	typedef llvm::StringRef StringRef;
	typedef llvm::Value Value;
public:
	Value *getCondition() const { return getArgOperand(0); }
	StringRef getAnnotation() const;

	// For LLVM casts.
	static inline bool classof(const CallInst *I) {
		if (const Function *F = I->getCalledFunction())
			return getBugOn(F->getParent()) == F;
		return false;
	}
	static inline bool classof(const Value *V) {
		return llvm::isa<CallInst>(V) && classof(llvm::cast<CallInst>(V));
	}
};

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

	virtual bool runOnInstruction(Instruction *) = 0;

	bool insert(Value *, llvm::StringRef Bug);
	bool insert(Value *, llvm::StringRef Bug, const llvm::DebugLoc &);
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
