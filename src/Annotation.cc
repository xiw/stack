#include <llvm/Module.h>
#include <llvm/Instructions.h>
#include <llvm/IntrinsicInst.h>
#include <llvm/Metadata.h>
#include <llvm/Pass.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/Debug.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Support/Path.h>


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


static inline bool isFunctionPointer(Type *Ty) {
	PointerType *PTy = dyn_cast<PointerType>(Ty);
	return PTy && PTy->getElementType()->isFunctionTy();
}

static inline bool needAnnotation(Value *V) {
	if (PointerType *PTy = dyn_cast<PointerType>(V->getType())) {
		Type *Ty = PTy->getElementType();
		return (Ty->isIntegerTy() || isFunctionPointer(Ty));
	}
	return false;
}

static inline std::string getScopeName(GlobalValue *GV) {
	if (GlobalValue::isExternalLinkage(GV->getLinkage()))
		return GV->getName();
	else {
		std::string prefix = sys::path::filename(
			GV->getParent()->getModuleIdentifier());
		return "local." + prefix + "." + GV->getName().str();
	}
}

std::string AnnotationPass::getAnnotation(Value *V) {
	std::string id;

	if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V))
		id = "var." + getScopeName(GV);
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
			if (Offset && isa<StructType>(Ty)) {
				id = Ty->getStructName().str() + "." +
					Twine(Offset->getLimitedValue()).str();
			}
		}
	}

	return id;
}

bool AnnotationPass::runOnFunction(Function &F) {
	bool Changed = false;
	LLVMContext &VMCtx = F.getContext();
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		std::string Anno;

		if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
			llvm::Value *V = LI->getPointerOperand();
			if (needAnnotation(V))
				Anno = getAnnotation(V);
		} else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
			llvm::Value *V = SI->getPointerOperand();
			if (needAnnotation(V))
				Anno = getAnnotation(V);
		}

		if (Anno.empty())
			continue;

		// Annotation should be in the form of key:value.
		MDNode *MD = MDNode::get(VMCtx, MDString::get(VMCtx, Anno));
		I->setMetadata("id", MD);
		Changed = true;
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


