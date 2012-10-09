#include <llvm/DebugInfo.h>
#include <llvm/Pass.h>
#include <llvm/Instructions.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Module.h>
#include <llvm/Constants.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Analysis/CallGraph.h>

#include "Annotation.h"
#include "IntGlobal.h"

using namespace llvm;

// collect function pointer assignments in global initializers
void
CallGraphPass::processInitializers(Module *M, Constant *I, GlobalValue *V) {
	// structs
	if (ConstantStruct *CS = dyn_cast<ConstantStruct>(I)) {
		StructType *STy = CS->getType();
		if (!STy->hasName())
			return;
		for (unsigned i = 0; i != STy->getNumElements(); ++i) {
			Type *ETy = STy->getElementType(i);
			if (ETy->isStructTy() || ETy->isArrayTy()) {
				// nested array or struct
				processInitializers(M, CS->getOperand(i), NULL);
			} else if (isFunctionPointer(ETy)) {
				// found function pointers in struct fields
				if (Function *F = dyn_cast<Function>(CS->getOperand(i))) {
					std::string Id = getStructId(STy, M, i);
					Ctx->FuncPtrs[Id].insert(F);
				}
			}
		}
	} else if (ConstantArray *CA = dyn_cast<ConstantArray>(I)) {
		// array of structs
		if (CA->getType()->getElementType()->isStructTy())
			for (unsigned i = 0; i != CA->getNumOperands(); ++i)
				processInitializers(M, CA->getOperand(i), NULL);
	} else if (Function *F = dyn_cast<Function>(I)) {
		// global function pointer variables
		if (V) {
			std::string Id = getVarId(V);
			Ctx->FuncPtrs[Id].insert(F);
		}
	}
}

bool CallGraphPass::mergeFuncSet(FuncSet &S, const std::string &Id) {
	FuncPtrMap::iterator i = Ctx->FuncPtrs.find(Id);
	if (i != Ctx->FuncPtrs.end())
		return mergeFuncSet(S, i->second);
	return false;
}

bool CallGraphPass::mergeFuncSet(FuncSet &Dst, const FuncSet &Src) {
	bool Changed = false;
	for (FuncSet::const_iterator i = Src.begin(), e = Src.end(); i != e; ++i)
		Changed |= Dst.insert(*i);
	return Changed;
}


bool CallGraphPass::findFunctions(Value *V, FuncSet &S) {
	SmallPtrSet<Value *, 4> Visited;
	return findFunctions(V, S, Visited);
}

bool CallGraphPass::findFunctions(Value *V, FuncSet &S, 
                                  SmallPtrSet<Value *, 4> Visited) {
	if (!Visited.insert(V))
		return false;

	// real function, S = S + {F}
	if (Function *F = dyn_cast<Function>(V)) {
		if (!F->empty())
			return S.insert(F);

		// prefer the real definition to declarations
		FuncMap::iterator it = Ctx->Funcs.find(F->getName());
		if (it != Ctx->Funcs.end())
			return S.insert(it->second);
		else
			return S.insert(F);
	}

	// bitcast, ignore the cast
	if (BitCastInst *B = dyn_cast<BitCastInst>(V))
		return findFunctions(B->getOperand(0), S, Visited);
	
	// const bitcast, ignore the cast
	if (ConstantExpr *C = dyn_cast<ConstantExpr>(V)) {
		if (C->isCast())
			return findFunctions(C->getOperand(0), S, Visited);
	}
	
	// PHI node, recursively collect all incoming values
	if (PHINode *P = dyn_cast<PHINode>(V)) {
		bool Changed = false;
		for (unsigned i = 0; i != P->getNumIncomingValues(); ++i)
			Changed |= findFunctions(P->getIncomingValue(i), S, Visited);
		return Changed;
	}
	
	// select, recursively collect both paths
	if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
		bool Changed = false;
		Changed |= findFunctions(SI->getTrueValue(), S, Visited);
		Changed |= findFunctions(SI->getFalseValue(), S, Visited);
		return Changed;
	}
	
	// arguement, S = S + FuncPtrs[arg.ID]
	if (Argument *A = dyn_cast<Argument>(V))
		return mergeFuncSet(S, getArgId(A));
	
	// return value, S = S + FuncPtrs[ret.ID]
	if (CallInst *CI = dyn_cast<CallInst>(V)) {
		if (Function *CF = CI->getCalledFunction())
			return mergeFuncSet(S, getRetId(CF));

		// TODO: handle indirect calls
		return false;
	}
	
	// loads, S = S + FuncPtrs[struct.ID]
	if (LoadInst *L = dyn_cast<LoadInst>(V))
		return mergeFuncSet(S, getLoadStoreId(L));
	
	// ignore other constant (usually null), inline asm and inttoptr
	if (isa<Constant>(V) || isa<InlineAsm>(V) || isa<IntToPtrInst>(V))
		return false;
		
	V->dump();
	report_fatal_error("findFunctions: unhandled value type\n");
	return false;
}

bool CallGraphPass::runOnFunction(Function *F) {
	bool Changed = false;

	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
			// stores to function pointers
			Value *V = SI->getValueOperand();
			if (isFunctionPointer(V->getType())) {
				StringRef Id = getLoadStoreId(SI);
				if (!Id.empty())
					Changed |= findFunctions(V, Ctx->FuncPtrs[Id]);
			}
		} else if (ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
			// function returns
			if (isFunctionPointer(F->getReturnType())) {
				Value *V = RI->getReturnValue();
				std::string Id = getRetId(F);
				Changed |= findFunctions(V, Ctx->FuncPtrs[Id]);
			}
		} else if (CallInst *CI = dyn_cast<CallInst>(I)) {
			// ignore inline asm or intrinsic calls
			if (CI->isInlineAsm() || (CI->getCalledFunction()
					&& CI->getCalledFunction()->isIntrinsic()))
				continue;

			// might be an indirect call, find all possible callees
			FuncSet FS;
			if (!findFunctions(CI->getCalledValue(), FS))
				continue;

			// looking for function pointer arguments
			for (unsigned no = 0; no != CI->getNumArgOperands(); ++no) {
				Value *V = CI->getArgOperand(no);
				if (!isFunctionPointer(V->getType()))
					continue;

				// find all possible assignments to the argument
				FuncSet VS;
				if (!findFunctions(V, VS))
					continue;

				// update argument FP-set for possible callees
				for (FuncSet::iterator k = FS.begin(), ke = FS.end();
				        k != ke; ++k) {
					llvm::Function *CF = *k;
					std::string Id = getArgId(CF, no);
					Changed |= mergeFuncSet(Ctx->FuncPtrs[Id], VS);
				}
			}
		}
	}
	return Changed;
}

bool CallGraphPass::doInitialization(Module *M) {
	// collect function pointer assignments in global initializers
	Module::global_iterator i, e;
	for (i = M->global_begin(), e = M->global_end(); i != e; ++i) {
		if (i->hasInitializer())
			processInitializers(M, i->getInitializer(), &*i);
	}

	// collect global function definitions
	for (Module::iterator f = M->begin(), fe = M->end(); f != fe; ++f) {
		if (f->hasExternalLinkage() && !f->empty())
			Ctx->Funcs[f->getName()] = &*f;
	}

	return true;
}

bool CallGraphPass::doFinalization(Module *M) {
	// update callee mapping
	for (Module::iterator f = M->begin(), fe = M->end(); f != fe; ++f) {
		Function *F = &*f;
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			// map callsite to possible callees
			if (CallInst *CI = dyn_cast<CallInst>(&*i)) {
				FuncSet &FS = Ctx->Callees[CI];
				findFunctions(CI->getCalledValue(), FS);
			}
		}
	}
	return false;
}

bool CallGraphPass::doModulePass(Module *M) {
	bool Changed = true, ret = false;
	while (Changed) {
		Changed = false;
		for (Module::iterator i = M->begin(), e = M->end(); i != e; ++i)
			Changed |= runOnFunction(&*i);
		ret |= Changed;
	}
	return ret;
}

// debug
void CallGraphPass::dumpFuncPtrs() {
	raw_ostream &OS = dbgs();
	for (FuncPtrMap::iterator i = Ctx->FuncPtrs.begin(), 
		 e = Ctx->FuncPtrs.end(); i != e; ++i) {
		OS << i->first << "\n";
		FuncSet &v = i->second;
		for (FuncSet::iterator j = v.begin(), ej = v.end();
			 j != ej; ++j) {
			OS << "  " << ((*j)->hasInternalLinkage() ? "f" : "F")
				<< " " << (*j)->getName() << "\n";
		}
	}
}

void CallGraphPass::dumpCallees() {
	raw_ostream &OS = dbgs();
	for (CalleeMap::iterator i = Ctx->Callees.begin(), 
		 e = Ctx->Callees.end(); i != e; ++i) {
		 
		CallInst *CI = i->first;
		FuncSet &v = i->second;
		if (CI->isInlineAsm() || CI->getCalledFunction() || v.empty())
		 	continue;

		CI->dump();
		for (FuncSet::iterator j = v.begin(), ej = v.end();
			 j != ej; ++j) {
			OS << "         " << ((*j)->hasInternalLinkage() ? "f" : "F")
				<< " " << (*j)->getName() << "\n";
		}
	}
}

