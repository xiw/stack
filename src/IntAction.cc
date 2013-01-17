#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <llvm/DebugInfo.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/InstIterator.h>

using namespace clang;
using namespace llvm;

namespace {

class ExtractMacros : public PPCallbacks {
public:
	ExtractMacros(std::vector<SourceRange> &R) : Ranges(R) {}

	virtual void MacroExpands(const Token &, const MacroInfo *, SourceRange Range) {
		Ranges.push_back(Range);
	}

private:
	std::vector<SourceRange> &Ranges;
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
		return Delegate::CreateASTConsumer(CI, InFile);
	}

	virtual bool BeginInvocation(CompilerInstance &CI) {
		return Delegate::BeginInvocation(CI);
	}

	virtual bool BeginSourceFileAction(CompilerInstance &CI, StringRef Filename) {
		Preprocessor &PP = CI.getPreprocessor();
		PP.addPPCallbacks(new ExtractMacros(Ranges));
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
		clearMacroLocations(*M);
		WriteBitcodeToFile(M.get(), *OS);
	}

private:
	std::vector<SourceRange> Ranges;
	raw_ostream *OS;

	void clearMacroLocations(llvm::Module &);
};

} // anonymous namespace

void IntAction::clearMacroLocations(llvm::Module &M) {
	// Filename => set<Line>.
	StringMap< DenseSet<unsigned> > MacroLines;
	SourceManager &SM = super::getCompilerInstance().getSourceManager();
	for (SourceRange &R : Ranges) {
		if (R.isInvalid())
			continue;
		// Clang only emits the start location.
		SourceLocation Loc = R.getBegin();
		PresumedLoc PLoc = SM.getPresumedLoc(Loc);
		if (PLoc.isInvalid())
			continue;
		MacroLines[PLoc.getFilename()].insert(PLoc.getLine());
	}
	// Remove !dbg if it is expanded from a macro.
	LLVMContext &C = M.getContext();
	for (Function &F : M) {
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			Instruction *I = &*i;
			const DebugLoc &DbgLoc = I->getDebugLoc();
			if (DbgLoc.isUnknown())
				continue;
			StringRef Filename = DIScope(DbgLoc.getScope(C)).getFilename();
			unsigned Line = DbgLoc.getLine();
			if (MacroLines.lookup(Filename).count(Line))
				I->setDebugLoc(DebugLoc());
		}
	}
}

static FrontendPluginRegistry::Add<IntAction>
X("intfe", "Frontend rewriting");
