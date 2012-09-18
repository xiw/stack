#include <llvm/Module.h>
#include <llvm/Type.h>
#include <llvm/Instructions.h>
#include <llvm/Metadata.h>
#include <llvm/Support/Path.h>
#include <string>

static inline bool isFunctionPointer(llvm::Type *Ty) {
	llvm::PointerType *PTy = llvm::dyn_cast<llvm::PointerType>(Ty);
	return PTy && PTy->getElementType()->isFunctionTy();
}

static inline std::string getScopeName(llvm::GlobalValue *GV) {
	if (llvm::GlobalValue::isExternalLinkage(GV->getLinkage()))
		return GV->getName();
	else {
		std::string prefix = llvm::sys::path::stem(
			GV->getParent()->getModuleIdentifier());
		return "local." + prefix + "." + GV->getName().str();
	}
}

static inline llvm::StringRef getLoadStoreId(llvm::Instruction *I) {
	if (llvm::MDNode *MD = I->getMetadata("id"))
		return llvm::dyn_cast<llvm::MDString>(MD->getOperand(0))->getString();
	return llvm::StringRef();
}

static inline std::string getStructId(llvm::Type *Ty, unsigned offset) {
	return Ty->getStructName().str() + "." + llvm::Twine(offset).str();
}

static inline std::string getVarId(llvm::GlobalValue *GV) {
	return "var." + getScopeName(GV);
}

static inline std::string getArgId(llvm::Argument *A) {
	return "arg." + getScopeName(A->getParent()) + "."
			+ llvm::Twine(A->getArgNo()).str();
}

static inline std::string getArgId(llvm::Function *F, unsigned no) {
	return "arg." + getScopeName(F) + "." + llvm::Twine(no).str();
}

static inline std::string getRetId(llvm::Function *F) {
	return "ret." + getScopeName(F);
}


