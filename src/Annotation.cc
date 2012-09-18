#include <llvm/Module.h>
#include <llvm/Instructions.h>
#include <llvm/Metadata.h>
#include <llvm/Constants.h>
#include <llvm/Pass.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/Debug.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Transforms/Utils/Local.h>

#include "Annotation.h"

using namespace llvm;

namespace {

class AnnotationPass : public FunctionPass {
protected:
	std::string getAnnotation(Value *V);
public:
	static char ID;
	AnnotationPass() : FunctionPass(ID) { }

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}
	virtual bool runOnFunction(Function &);
	virtual bool doInitialization(Module &);
};

}


static inline bool needAnnotation(Value *V) {
	if (PointerType *PTy = dyn_cast<PointerType>(V->getType())) {
		Type *Ty = PTy->getElementType();
		return (Ty->isIntegerTy() || isFunctionPointer(Ty));
	}
	return false;
}

std::string AnnotationPass::getAnnotation(Value *V) {
	std::string id;

	if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V))
		id = getVarId(GV);
	else {
		User::op_iterator is, ie; // GEP indices
		Type *PTy = NULL;         // Pointer type
		if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
			// GEP instruction
			is = GEP->idx_begin();
			ie = GEP->idx_end() - 1;
			PTy = GEP->getPointerOperandType();
		} else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V)) {
			// constant GEP expression
			if (CE->getOpcode() == Instruction::GetElementPtr) {
				is = CE->op_begin() + 1;
				ie = CE->op_end() - 1;
				PTy = CE->getOperand(0)->getType();
			}
		}
		// id is in the form of struct.[name].[offset]
		if (PTy) {
			SmallVector<Value *, 4> Idx(is, ie);
			Type *Ty = GetElementPtrInst::getIndexedType(PTy, Idx);
			ConstantInt *Offset = dyn_cast<ConstantInt>(ie->get());
			if (Offset && isa<StructType>(Ty))
				id = getStructId(Ty, Offset->getLimitedValue());
		}
	}

	return id;
}

bool AnnotationPass::runOnFunction(Function &F) {
	bool Changed = false;
	LLVMContext &VMCtx = F.getContext();
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;

		if (isa<LoadInst>(I) || isa<StoreInst>(I)) {
			// annotate load/stores
			std::string Anno;
			if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
				llvm::Value *V = LI->getPointerOperand();
				if (needAnnotation(V))
					Anno = getAnnotation(V);
			} else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
				Value *V = SI->getPointerOperand();
				if (needAnnotation(V))
					Anno = getAnnotation(V);
			}

			if (Anno.empty())
				continue;

			MDNode *MD = MDNode::get(VMCtx, MDString::get(VMCtx, Anno));
			I->setMetadata("id", MD);
			Changed = true;

		} else if (CallInst *CI = dyn_cast<CallInst>(I)) {
			// annotate taints
			Function *CF = CI->getCalledFunction();
			if (!CF)
				continue;

			Value *V = NULL;
			bool Replace;
			if (CF->getName().startswith("__kint_taint_u")) {
				// 1st arg is the tainted value
				V = CI->getArgOperand(0);
				Replace = true;
			} else if (CF->getName() == "__kint_taint_any") {
				// 2nd arg is the tainted value
				V = CI->getArgOperand(1);
				Replace = false;
			}

			// skip non-instruction taints (args, etc.)
			Instruction *I = dyn_cast_or_null<Instruction>(V);
			if (!I)
				continue;
			MDNode *MD = MDNode::get(VMCtx, MDString::get(VMCtx, CF->getName()));
			I->setMetadata("taint", MD);

			// erase __kint_taint_* calls
			if (Replace) {
				assert(CI->getType() == V->getType());
				CI->replaceAllUsesWith(V);
			}
			CI->eraseFromParent();
			Changed = true;
		}
	}
	return Changed;
}

bool AnnotationPass::doInitialization(Module &M)
{
	return true;
}


char AnnotationPass::ID;

static RegisterPass<AnnotationPass>
X("anno", "add annotation for load/stores");


