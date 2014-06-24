#include "Diagnostic.h"
#include "SMTSolver.h"
#include <llvm/DebugInfo.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

bool Diagnostic::hasSingleDebugLocation(Instruction *I) {
	const DebugLoc &DbgLoc = I->getDebugLoc();
	// Skip inserted instructions without debugging information.
	if (DbgLoc.isUnknown())
		return false;
	// Skip inlined instructions.
	if (DbgLoc.getInlinedAt(I->getContext()))
		return false;
	// Macro-expanded code.
	if (I->getMetadata("macro"))
		return false;
	return true;
}

Diagnostic::Diagnostic() : OS(errs()) {}

void Diagnostic::backtrace(Instruction *I) {
	MDNode *MD = I->getDebugLoc().getAsMDNode(I->getContext());
	if (!MD)
		return;
	OS << "stack: \n";
	DILocation Loc(MD);
	for (;;) {
		this->location(Loc);
		Loc = Loc.getOrigLocation();
		if (!Loc.Verify())
			break;
	}
}

void Diagnostic::location(MDNode *MD) {
	DILocation Loc(MD);
	SmallString<64> Path;
	StringRef Filename = Loc.getFilename();
	if (sys::path::is_absolute(Filename))
		Path.append(Filename.begin(), Filename.end());
	else
		sys::path::append(Path, Loc.getDirectory(), Filename);
	OS << "  - " << Path
	   << ':' << Loc.getLineNumber()
	   << ':' << Loc.getColumnNumber() << "\n";
}

void Diagnostic::bug(Instruction *I) {
	MDNode *MD = I->getMetadata("bug");
	if (!MD)
		return;
	this->bug(cast<MDString>(MD->getOperand(0))->getString());
}

void Diagnostic::bug(const Twine &Str) {
	OS << "---\n" << "bug: " << Str << "\n";
}

void Diagnostic::status(int Status) {
	const char *Str;
	switch (Status) {
	case SMT_UNDEF:   Str = "undef";   break;
	case SMT_UNSAT:   Str = "unsat";   break;
	case SMT_SAT:     Str = "sat";     break;
	default:          Str = "timeout"; break;
	}
	OS << "status: " << Str << "\n";
}
