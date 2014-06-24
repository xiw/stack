#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Lex/Preprocessor.h>
#include <llvm/DebugInfo.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/InstIterator.h>
#include <set>

using namespace clang;
using namespace llvm;

// Filename, Line => Location.
typedef StringMap< DenseMap<unsigned, std::set<SourceLocation> > > MacroMap;

namespace {

// Add macro-expanded code with if or ?: conditions.
class ExtractMacroVisitor : public RecursiveASTVisitor<ExtractMacroVisitor> {
public:
	ExtractMacroVisitor(SourceManager &SM, MacroMap &MM) : SM(SM), MM(MM) {}

	bool VisitIfStmt(IfStmt *S) {
		addMacroLoc(S->getLocStart());
		return true;
	}

	bool VisitConditionalOperator(ConditionalOperator *E) {
		addMacroLoc(E->getLocStart());
		return true;
	}

	// GNU extension ?:, the middle operand omitted.
	bool VisitBinaryConditionalOperator(BinaryConditionalOperator *E) {
		addMacroLoc(E->getLocStart());
		return true;
	}

	// p && p->x.
	bool VisitBinaryOperator(BinaryOperator *E) {
		if (E->isLogicalOp() || E->isEqualityOp())
			addMacroLoc(E->getLocStart());
		return true;
	}

private:
	SourceManager &SM;
	MacroMap &MM;

	void addMacroLoc(SourceLocation Loc) {
		if (Loc.isInvalid())
			return;
		if (!Loc.isMacroID())
			return;
		PresumedLoc PLoc = SM.getPresumedLoc(Loc);
		if (PLoc.isInvalid())
			return;
		MM[PLoc.getFilename()][PLoc.getLine()].insert(Loc);
	}
};

class ExtractMacroConsumer : public ASTConsumer {
public:
	ExtractMacroConsumer(MacroMap &MM) : MM(MM) {}

	virtual void HandleTranslationUnit(ASTContext &Ctx) {
		ExtractMacroVisitor Visitor(Ctx.getSourceManager(), MM);
		Visitor.TraverseDecl(Ctx.getTranslationUnitDecl());
	}

private:
	MacroMap &MM;
};

// Use multiple inheritance to intercept code generation.
class IntAction : public PluginASTAction, EmitLLVMOnlyAction {
	typedef PluginASTAction super;
	typedef EmitLLVMOnlyAction Delegate;

public:
	virtual bool ParseArgs(const CompilerInstance &, const std::vector<std::string>&) {
		return true;
	}

	virtual bool usesPreprocessorOnly() const {
		return Delegate::usesPreprocessorOnly();
	}

	virtual TranslationUnitKind getTranslationUnitKind() {
		return Delegate::getTranslationUnitKind();
	}

	virtual bool hasPCHSupport() const {
		return Delegate::hasPCHSupport();
	}

	virtual bool hasASTFileSupport() const {
		return Delegate::hasASTFileSupport();
	}

	virtual bool hasIRSupport() const {
		return Delegate::hasIRSupport();
	}

	virtual bool hasCodeCompletionSupport() const {
		return Delegate::hasCodeCompletionSupport();
	}

protected:

	virtual ASTConsumer *CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
		OS = CI.createDefaultOutputFile(true, InFile, "bc");
		ASTConsumer *C[] = {
			new ExtractMacroConsumer(MM),
			Delegate::CreateASTConsumer(CI, InFile)
		};
		return new MultiplexConsumer(C);
	}

	virtual bool BeginInvocation(CompilerInstance &CI) {
		return Delegate::BeginInvocation(CI);
	}

	virtual bool BeginSourceFileAction(CompilerInstance &CI, StringRef Filename) {
		Delegate::setCurrentInput(super::getCurrentInput());
		Delegate::setCompilerInstance(&CI);
		return Delegate::BeginSourceFileAction(CI, Filename);
	}

	virtual void ExecuteAction() {
		Delegate::ExecuteAction();
	}

	virtual void EndSourceFileAction() {
		Delegate::EndSourceFileAction();
		OwningPtr<llvm::Module> M(Delegate::takeModule());
		if (!M)
			return;
		markMacroLocations(*M);
		WriteBitcodeToFile(M.get(), *OS);
	}

private:
	MacroMap MM;
	raw_ostream *OS;

	void markMacroLocations(llvm::Module &);
};

} // anonymous namespace

void IntAction::markMacroLocations(llvm::Module &M) {
	// Filename => set<Line>.
	LLVMContext &C = M.getContext();
	unsigned MD_macro = C.getMDKindID("macro");
	CompilerInstance &CI = super::getCompilerInstance();
	Preprocessor &PP = CI.getPreprocessor();
	for (Function &F : M) {
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			Instruction *I = &*i;
			const DebugLoc &DbgLoc = I->getDebugLoc();
			if (DbgLoc.isUnknown())
				continue;
			StringRef Filename = DIScope(DbgLoc.getScope(C)).getFilename();
			unsigned Line = DbgLoc.getLine();
			SmallVector<Value *, 4> MDElems;
			for (SourceLocation Loc : MM.lookup(Filename).lookup(Line)) {
				StringRef MacroName = PP.getImmediateMacroName(Loc);
				MDElems.push_back(MDString::get(C, MacroName));
			}
			if (MDElems.empty())
				continue;
			I->setMetadata(MD_macro, MDNode::get(C, MDElems));
		}
	}
}

static FrontendPluginRegistry::Add<IntAction>
X("intfe", "Frontend rewriting");
