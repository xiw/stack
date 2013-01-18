#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Frontend/MultiplexConsumer.h>
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

typedef std::set<SourceLocation> LocSet;

namespace {

// Add macro-expanded code with if or ?: conditions.
class ExtractMacroVisitor : public RecursiveASTVisitor<ExtractMacroVisitor> {
public:
	ExtractMacroVisitor(SourceManager &SM, LocSet &L) : SM(SM), Locs(L) {}

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

private:
	SourceManager &SM;
	LocSet &Locs;

	void addMacroLoc(SourceLocation Loc) {
		if (SM.isMacroBodyExpansion(Loc))
			Locs.insert(Loc);
	}
};

class ExtractMacroConsumer : public ASTConsumer {
public:
	ExtractMacroConsumer(LocSet &L) : Locs(L) {}

	virtual void HandleTranslationUnit(ASTContext &Ctx) {
		ExtractMacroVisitor Visitor(Ctx.getSourceManager(), Locs);
		Visitor.TraverseDecl(Ctx.getTranslationUnitDecl());
	}

private:
	LocSet &Locs;
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
			new ExtractMacroConsumer(Locs),
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
	LocSet Locs;
	raw_ostream *OS;

	void markMacroLocations(llvm::Module &);
};

} // anonymous namespace

void IntAction::markMacroLocations(llvm::Module &M) {
	// Filename => set<Line>.
	StringMap< DenseSet<unsigned> > MacroLines;
	SourceManager &SM = super::getCompilerInstance().getSourceManager();
	for (SourceLocation Loc : Locs) {
		if (Loc.isInvalid())
			continue;
		PresumedLoc PLoc = SM.getPresumedLoc(Loc);
		if (PLoc.isInvalid())
			continue;
		MacroLines[PLoc.getFilename()].insert(PLoc.getLine());
	}
	LLVMContext &C = M.getContext();
	unsigned MD_macro = C.getMDKindID("macro");
	MDNode *Dummy = MDNode::get(C, MDString::get(C, "dummy"));
	for (Function &F : M) {
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			Instruction *I = &*i;
			const DebugLoc &DbgLoc = I->getDebugLoc();
			if (DbgLoc.isUnknown())
				continue;
			StringRef Filename = DIScope(DbgLoc.getScope(C)).getFilename();
			unsigned Line = DbgLoc.getLine();
			if (MacroLines.lookup(Filename).count(Line))
				I->setMetadata(MD_macro, Dummy);
		}
	}
}

static FrontendPluginRegistry::Add<IntAction>
X("intfe", "Frontend rewriting");
