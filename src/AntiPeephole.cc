#define DEBUG_TYPE "anti-peephole"
#include <llvm/DataLayout.h>
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include "Diagnostic.h"
#include "PathGen.h"
#include "ValueGen.h"

using namespace llvm;

namespace {

struct AntiPeephole : FunctionPass {
	static char ID;
	AntiPeephole() : FunctionPass(ID) {}
	virtual bool doInitialization(Module &);
	virtual bool runOnFunction(Function &);
	virtual bool doFinalization(Module &);
private:
	OwningPtr<FunctionPassManager> FPM;
	DataLayout *DL;
	Diagnostic Diag;
	SmallVector<PathGen::Edge, 32> BackEdges0, BackEdges1;
	ValueToValueMapTy VMap;

	void checkEqv(BasicBlock *BB1);
};

} // anonymous namespace

bool AntiPeephole::doInitialization(Module &M) {
	FPM.reset(new FunctionPassManager(&M));
	DL = new DataLayout(&M);
	TargetLibraryInfo *TLI = new TargetLibraryInfo(Triple(M.getTargetTriple()));
	// TODO: -fno-builtin
	// TLI->disableAllFunctions();
	Pass *P = createInstructionCombiningPass();
	FPM->add(DL);
	FPM->add(TLI);
	FPM->add(P);
	return FPM->doInitialization();
}

bool AntiPeephole::doFinalization(Module &) {
	return FPM->doFinalization();
}

static bool isRecomputable(Instruction *I) {
	Type *T = I->getType();
	if (!T->isIntegerTy() && !T->isPointerTy())
		return false;
	if (isa<BinaryOperator>(I))
		return true;
	switch (I->getOpcode()) {
	default: break;
	// Don't clone readonly calls for now; we need to compare its
	// parameters otherwise.
#if 0
	case Instruction::Call:
		return cast<CallInst>(I)->doesNotAccessMemory();
#endif
	case Instruction::ExtractElement:
	case Instruction::ExtractValue:
	case Instruction::GetElementPtr:
	case Instruction::Trunc:
	case Instruction::ZExt:
	case Instruction::SExt:
	case Instruction::PtrToInt:
	case Instruction::IntToPtr:
	case Instruction::BitCast:
	case Instruction::ICmp:
	case Instruction::PHI:
	case Instruction::Select:
		return true;
	}
	return false;
}

bool AntiPeephole::runOnFunction(Function &F) {
	// VMap stores values F => Clone.
	VMap.clear();
	// No need to clone arguments.
	for (Function::arg_iterator i = F.arg_begin(), e = F.arg_end(); i != e; ++i)
		VMap[i] = i;
	// Make a clone of F before running P.
	OwningPtr<Function> Clone(CloneFunction(&F, VMap, false));
	// Run P over F.
	if (!FPM->run(F))
		return false;
	// Check if P made any changes.
	BackEdges0.clear();
	FindFunctionBackedges(*Clone, BackEdges0);
	BackEdges1.clear();
	FindFunctionBackedges(F, BackEdges1);
	for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i)
		checkEqv(i);
	return true;
}

void AntiPeephole::checkEqv(BasicBlock *BB1) {
	BasicBlock *BB0 = cast<BasicBlock>(VMap.lookup(BB1));
	BranchInst *Br0 = dyn_cast<BranchInst>(BB0->getTerminator());
	if (!Br0 || Br0->isUnconditional())
		return;
	BranchInst *Br1 = dyn_cast<BranchInst>(BB1->getTerminator());
	if (!Br1 || Br1->isUnconditional())
		return;
	bool SuccSwapped = (Br0->getSuccessor(0) != VMap.lookup(Br1->getSuccessor(0)));

	SMTSolver SMT(true);
	ValueGen VG(*DL, SMT);

	// Establish equivalence via cloning.
	for (ValueToValueMapTy::iterator i = VMap.begin(), e = VMap.end(); i != e; ++i) {
		Instruction *I0 = dyn_cast<Instruction>(i->second);
		Instruction *I1 = const_cast<Instruction *>(dyn_cast<Instruction>(i->first));
		if (!I0 || !I1)
			continue;
		Type *T = I0->getType();
		assert(T == I1->getType());
		if (!ValueGen::isAnalyzable(T))
			continue;
		if (!isRecomputable(I0)) {
			SMTExpr E = SMT.eq(VG.get(I0), VG.get(I1));
			SMT.assume(E);
			SMT.decref(E);
		}
	}

	PathGen PG0(VG, BackEdges0), PG1(VG, BackEdges1);
	// First make sure the path conditions are the same.
	// Avoid repeating warnings from previous BBs.
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
	SMTExpr NewP1;
	if (SuccSwapped) {
		SMTExpr Tmp = SMT.bvnot(Cond1);
		NewP1 = SMT.bvand(Tmp, P1);
		SMT.decref(Tmp);
	} else {
		NewP1 = SMT.bvand(Cond1, P1);
	}

	// Then we check if the conditions change with the new branching.
	Query = SMT.ne(NewP0, NewP1);
	SMT.decref(NewP0);
	SMT.decref(NewP1);
	Status = SMT.query(Query);
	SMT.decref(Query);
	if (Status == SMT_UNSAT)
		return;

	// Output model.
	Diag.bug(DEBUG_TYPE);
	Diag << "model: |\n";
	Diag << "  <<<" << (isa<Instruction>(V0) ? "" : "  ") << *V0 << '\n';
	Diag << "  >>>" << (isa<Instruction>(V1) ? "" : "  ") << *V1 << '\n';
	Instruction *Loc = isa<Instruction>(V0) ? cast<Instruction>(V0) : Br0;
	Diag.backtrace(Loc);
}

char AntiPeephole::ID;

static RegisterPass<AntiPeephole>
X("anti-peephole", "Anti Peephole Optimization");
