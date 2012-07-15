#define DEBUG_TYPE "undef-sat"
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include "Diagnostic.h"
#include "PathGen.h"
#include "SMTSolver.h"
#include "ValueGen.h"

using namespace llvm;

namespace {

struct UndefSat : ModulePass {
	static char ID;
	UndefSat() : ModulePass(ID) { }
	virtual bool runOnModule(Module &);

private:
	OwningPtr<Diagnostic> Diag;
};

struct UndefPassManager : PassManager {
	UndefPassManager(Module *M, Diagnostic &Diag) : M(M), Diag(Diag) { }
	virtual void add(Pass *);
private:
	Module *M;
	Diagnostic &Diag;
};

struct UndefWrapper: FunctionPass {
	static char ID;
	UndefWrapper(Module *, Pass *, Diagnostic &);
	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		P->getAnalysisUsage(AU);
	}
	virtual const char *getPassName() const {
		return Name.c_str();
	}
	virtual bool doInitialization(Module &) {
		return FPM.doInitialization();
	}
	virtual bool runOnFunction(Function &);
	virtual bool doFinalization(Module &) {
		return FPM.doFinalization();
	}
private:
	llvm::Module *M;
	FunctionPassManager FPM;
	Pass *P;
	std::string Name;
	SmallVector<PathGen::Edge, 32> BackEdges0, BackEdges1;
	TargetData *TD;
	Diagnostic &Diag;

	void checkEqv(BasicBlock *BB0, BasicBlock *BB1);
	void write(raw_ostream &OS, Value *V);
};

} // anonymous namespace

bool UndefSat::runOnModule(Module &M) {
	Diag.reset(new Diagnostic(M));
	UndefPassManager MPM(&M, *Diag);
	MPM.add(new TargetData(&M));

	unsigned Threshold = 275;
	PassManagerBuilder PMBuilder;
	PMBuilder.OptLevel = 3;
	PMBuilder.Inliner = createFunctionInliningPass(Threshold);
	PMBuilder.LibraryInfo = new TargetLibraryInfo(Triple(M.getTargetTriple()));
	PMBuilder.populateModulePassManager(MPM);

	return MPM.run(M);
}

void UndefPassManager::add(Pass *P) {
	StringRef ID = Pass::lookupPassInfo(P->getPassID())->getPassArgument();
	bool Wrap = StringSwitch<bool>(ID)
		.Case("instcombine", true)
		.Default(false);
	if (Wrap) {
		PassManager::add(new UndefWrapper(M, P, Diag));
	} else {
		PassManager::add(P);
	}
}

UndefWrapper::UndefWrapper(Module *M, Pass *P, Diagnostic &Diag)
		: FunctionPass(ID), M(M), FPM(M), P(P), Diag(Diag) {
	assert(P->getPassKind() == PT_Function && "FunctionPass only!");
	// Add target passes.
	TD = new TargetData(M);
	FPM.add(TD);
	FPM.add(new TargetLibraryInfo(Triple(M->getTargetTriple())));
	// ADd alias passes.
	FPM.add(createTypeBasedAliasAnalysisPass());
	FPM.add(createBasicAliasAnalysisPass());
	FPM.add(P);
	Name = std::string("Undef (") + P->getPassName() + ")";
}

bool UndefWrapper::runOnFunction(Function &F) {
	// Make a clone before pass.
	// VMap stores instructions F => Clone.
	ValueToValueMapTy VMap;
	for (Function::arg_iterator i = F.arg_begin(), e = F.arg_end(); i != e; ++i)
		VMap[i] = i;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		switch (I->getOpcode()) {
		default: break;
		case Instruction::Call:
		case Instruction::Load:
			VMap[I] = I;
			break;
		}
	}
	OwningPtr<Function> Clone(CloneFunction(&F, VMap, false));
	// Run the pass, which updates VMap.
	if (!FPM.run(F))
		return false;
	// Diff if the pass made any changes.
	BackEdges0.clear();
	FindFunctionBackedges(F, BackEdges0);
	BackEdges1.clear();
	FindFunctionBackedges(F, BackEdges1);
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i)
		checkEqv(cast<BasicBlock>(VMap.lookup(i)), i);
	return true;
}

void UndefWrapper::checkEqv(BasicBlock *BB0, BasicBlock *BB1) {
	BranchInst *Br0 = dyn_cast<BranchInst>(BB0->getTerminator());
	if (!Br0 || Br0->isUnconditional())
		return;
	BranchInst *Br1 = dyn_cast<BranchInst>(BB1->getTerminator());
	if (!Br1 || Br1->isUnconditional())
		return;

	SMTSolver SMT;
	ValueGen VG(*TD, SMT);
	PathGen PG0(VG, BackEdges0), PG1(VG, BackEdges1);

	// First make sure the path conditions are the same.
	SMTExpr P0 = PG0.get(BB0), P1 = PG1.get(BB1);
	SMTExpr Query = SMT.ne(P0, P1);
	SMTStatus Status = SMT.query(Query);
	SMT.decref(Query);
	if (Status != SMT_UNSAT)
		return;

	Value *V0 = Br0->getCondition();
	SMTExpr Cond0 = VG.get(V0);
	SMTExpr NewP0 = SMT.bvand(Cond0, P0);

	Value *V1 = Br1->getCondition();
	SMTExpr Cond1 = VG.get(V1);
	SMTExpr NewP1 = SMT.bvand(Cond1, P1);

	// Then we check if the conditions change with the new branching.
	Query = SMT.ne(NewP0, NewP1);
	Status = SMT.query(Query);
	SMT.decref(Query);
	SMT.decref(NewP0);
	SMT.decref(NewP1);

	if (Status == SMT_UNSAT)
		return;
	const DebugLoc &DbgLoc = isa<Instruction>(V0)
		? cast<Instruction>(V0)->getDebugLoc()
		: Br0->getDebugLoc();
	Diag << DbgLoc << "undefined behavior";
	// Output model.
	raw_ostream &OS = Diag.os();
	OS << "<<<<<<< ";
	write(OS, V0);
	OS << '\n';
	OS << ">>>>>>> ";
	write(OS, V1);
	OS << '\n';
}

void UndefWrapper::write(raw_ostream &OS, Value *V) {
	if (Instruction *I = dyn_cast<Instruction>(V)) {
		std::string Str;
		{
			raw_string_ostream OS(Str);
			OS << *I;
		}
		OS << StringRef(Str).ltrim();
	} else {
		WriteAsOperand(OS, V, false, M);
	}
}

char UndefSat::ID;
char UndefWrapper::ID;

static RegisterPass<UndefSat>
X("undef-sat", "Detect undefined behavior");
