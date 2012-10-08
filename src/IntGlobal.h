#pragma once

#include <llvm/DebugInfo.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/ConstantRange.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

typedef std::vector< std::pair<llvm::Module *, llvm::StringRef> > ModuleList;
typedef llvm::SmallPtrSet<llvm::Function *, 8> FuncSet;
typedef std::map<llvm::StringRef, llvm::Function *> FuncMap;
typedef std::map<std::string, FuncSet> FuncPtrMap;
typedef llvm::DenseMap<llvm::CallInst *, FuncSet> CalleeMap;
typedef std::map<std::string, bool /* is source */> TaintSet;
typedef std::map<std::string, llvm::ConstantRange> RangeMap;

struct GlobalContext {
	// Map global function name to function defination
	FuncMap Funcs;

	// Map function pointers (IDs) to possible assignments
	FuncPtrMap FuncPtrs;
	
	// Map a callsite to all potential callees
	CalleeMap Callees;

	// Taints
	TaintSet Taints;

	// Ranges
	RangeMap IntRanges;
};

class IterativeModulePass {
protected:
	GlobalContext *Ctx;
	const char * ID;
public:
	IterativeModulePass(GlobalContext *Ctx_, const char *ID_)
		: Ctx(Ctx_), ID(ID_) { }
	
	// run on each module before iterative pass
	virtual bool doInitialization(llvm::Module *M)
		{ return true; }

	// run on each module after iterative pass
	virtual bool doFinalization(llvm::Module *M, llvm::StringRef)
		{ return true; }

	// iterative pass
	virtual bool doModulePass(llvm::Module *M)
		{ return false; }

	virtual void run(ModuleList &modules);
};

class CallGraphPass : public IterativeModulePass {
private:
	bool runOnFunction(llvm::Function *);
	void processInitializers(llvm::Module *, llvm::Constant *, llvm::GlobalValue *);
	bool mergeFuncSet(FuncSet &S, const std::string &Id);
	bool mergeFuncSet(FuncSet &Dst, const FuncSet &Src);
	bool findFunctions(llvm::Value *, FuncSet &);
	bool findFunctions(llvm::Value *, FuncSet &, 
	                   llvm::SmallPtrSet<llvm::Value *, 4>);


public:
	CallGraphPass(GlobalContext *Ctx_)
		: IterativeModulePass(Ctx_, "CallGraph") { }
	virtual bool doInitialization(llvm::Module *);
	virtual bool doFinalization(llvm::Module *, llvm::StringRef);
	virtual bool doModulePass(llvm::Module *);

	// debug
	void dumpFuncPtrs();
	void dumpCallees();
};

class TaintPass : public IterativeModulePass {
private:
	bool runOnFunction(llvm::Function *);
	bool isTaint(llvm::Value *V);
	bool checkTaintSource(llvm::Value *);
	bool markTaint(const std::string &Id, bool isSource);

	bool checkTaintSource(llvm::Instruction *I);
	bool checkTaintSource(llvm::Function *F);

	typedef llvm::SmallPtrSet<llvm::Value *, 16> ValueTaintSet;
	ValueTaintSet VTS;

public:
	TaintPass(GlobalContext *Ctx_)
		: IterativeModulePass(Ctx_, "Taint") { }
	virtual bool doModulePass(llvm::Module *);
	virtual bool doFinalization(llvm::Module *, llvm::StringRef);
	bool isTaintSource(const std::string &sID);

	// debug
	void dumpTaints();
};


class RangePass : public IterativeModulePass {

private:
	const unsigned MaxIterations;	
	
	bool safeUnion(llvm::ConstantRange &CR, const llvm::ConstantRange &R);
	bool unionRange(llvm::StringRef, const llvm::ConstantRange &, llvm::Value *);
	bool unionRange(llvm::BasicBlock *, llvm::Value *, const llvm::ConstantRange &);
	llvm::ConstantRange getRange(llvm::BasicBlock *, llvm::Value *);

	void collectInitializers(llvm::GlobalVariable *, llvm::Constant *);
	bool updateRangeFor(llvm::Function *);
	bool updateRangeFor(llvm::BasicBlock *);
	bool updateRangeFor(llvm::Instruction *);

	typedef std::map<llvm::Value *, llvm::ConstantRange> ValueRangeMap;
	typedef std::map<llvm::BasicBlock *, ValueRangeMap> FuncValueRangeMaps;
	FuncValueRangeMaps FuncVRMs;

	typedef std::set<std::string> ChangeSet;
	ChangeSet Changes;
	
	typedef std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *> Edge;
	typedef llvm::SmallVector<Edge, 16> EdgeList;
	EdgeList BackEdges;
	
	bool isBackEdge(const Edge &);
	
	llvm::ConstantRange visitBinaryOp(llvm::BinaryOperator *);
	llvm::ConstantRange visitCastInst(llvm::CastInst *);
	llvm::ConstantRange visitSelectInst(llvm::SelectInst *);
	llvm::ConstantRange visitPHINode(llvm::PHINode *);
	
	bool visitCallInst(llvm::CallInst *);
	bool visitReturnInst(llvm::ReturnInst *);
	bool visitStoreInst(llvm::StoreInst *);

	void visitBranchInst(llvm::BranchInst *, 
						 llvm::BasicBlock *, ValueRangeMap &);
	void visitTerminator(llvm::TerminatorInst *,
						 llvm::BasicBlock *, ValueRangeMap &);
	void visitSwitchInst(llvm::SwitchInst *, 
						 llvm::BasicBlock *, ValueRangeMap &);

public:
	RangePass(GlobalContext *Ctx_)
		: IterativeModulePass(Ctx_, "Range"), MaxIterations(5) { }
	
	virtual bool doInitialization(llvm::Module *);
	virtual bool doModulePass(llvm::Module *M);

	// debug
	void dumpRange();
};


void doWriteback(llvm::Module *M, llvm::StringRef name);
