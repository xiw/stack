#include "Diagnostic.h"
#include "SMTSolver.h"
#include "NomadicHeaders.h"
#include <llvm/Instruction.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

Diagnostic::Diagnostic() : OS(errs()) {}

static void getPath(SmallVectorImpl<char> &Path, const MDNode *MD) {
	StringRef Filename = DIScope(MD).getFilename();
	if (sys::path::is_absolute(Filename))
		Path.append(Filename.begin(), Filename.end());
	else
		sys::path::append(Path, DIScope(MD).getDirectory(), Filename);
}

void Diagnostic::backtrace(Instruction *I) {
	const char *Prefix = " - ";
	MDNode *MD = I->getDebugLoc().getAsMDNode(I->getContext());
	if (!MD)
		return;
	OS << "stack: \n";
	DILocation Loc(MD);
	for (;;) {
		SmallString<64> Path;
		getPath(Path, Loc.getScope());
		OS << Prefix << Path
		   << ':' << Loc.getLineNumber()
		   << ':' << Loc.getColumnNumber() << '\n';
		Loc = Loc.getOrigLocation();
		if (!Loc.Verify())
			break;
	}
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
