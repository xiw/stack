#include "Diagnostic.h"
#include "FileCache.h"
#include <llvm/Module.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Support/DebugLoc.h>
#include <llvm/Support/raw_ostream.h>
#include <stdlib.h>

using namespace llvm;

namespace {

class BugReporter : public DiagnosticImpl {
public:
	BugReporter(Module &M) : OS(llvm::errs()), M(M) {
		isDisplayed = OS.is_displayed();
	}
	virtual void emit(const llvm::DebugLoc &);
	virtual void emit(const llvm::Twine &);
	virtual llvm::raw_ostream &os() { return OS; }
private:
	raw_ostream &OS;
	Module &M;
	bool isDisplayed;
	FileCache Cache;
	StringRef Line;
};


class BugVerifier : public DiagnosticImpl {
public:
	BugVerifier(const char *Str) : Prefix(Str) {}
	virtual void emit(const llvm::DebugLoc &);
	virtual void emit(const llvm::Twine &);
	virtual llvm::raw_ostream &os() { return nulls(); }
private:
	std::string Prefix, Expected;
	unsigned Line, Col;
};

} // anonymous namespace

// Diagnostic

Diagnostic::Diagnostic(Module &M) {
	if (const char *Prefix = ::getenv("VERIFY_PREFIX")) {
		Diag.reset(new BugVerifier(Prefix));
		return;
	}
	Diag.reset(new BugReporter(M));
}

// BugReporter

void BugReporter::emit(const DebugLoc &DbgLoc) {
	if (isDisplayed)
		OS.changeColor(raw_ostream::CYAN);
	LLVMContext &VMCtx = M.getContext();
	Line = Cache.getLine(DbgLoc, VMCtx);
	MDNode *N = DbgLoc.getAsMDNode(VMCtx);
	DILocation Loc(N);
	for (;;) {
		OS << Loc.getDirectory() << Loc.getFilename() << ':';
		OS << Loc.getLineNumber() << ':';
		if (unsigned Col = Loc.getColumnNumber())
			OS << Col << ':';
		Loc = Loc.getOrigLocation();
		if (!Loc.Verify())
			break;
		OS << '\n';
	}
	if (isDisplayed)
		OS.changeColor(raw_ostream::MAGENTA);
	OS << " bug: ";
}

void BugReporter::emit(const llvm::Twine &Str) {
	if (isDisplayed)
		OS.changeColor(raw_ostream::CYAN);
	OS << Str << '\n';
	if (isDisplayed)
		OS.resetColor();
	if (!Line.empty())
		OS << Line << '\n';
}

// BugVerifier

void BugVerifier::emit(const DebugLoc &DbgLoc) {
	Line = DbgLoc.getLine();
	Col = DbgLoc.getCol();
	// TODO: read expected string
}

void BugVerifier::emit(const Twine &Str) {
	SmallString<256> Buf;
	StringRef Actual = Str.toNullTerminatedStringRef(Buf);
	if (Actual.startswith(Expected))
		return;
	// TODO: wrong
}
