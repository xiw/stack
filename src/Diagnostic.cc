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
#include <algorithm>
#include <cstdlib>
#include <list>

using namespace llvm;

namespace {

class BugReporter : public DiagnosticImpl {
public:
	BugReporter(Module &M) : OS(llvm::errs()), M(M) {
		isDisplayed = OS.is_displayed();
	}
	virtual void emit(const DebugLoc &);
	virtual void emit(const Twine &);
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
	BugVerifier(const char *, Module &);
	~BugVerifier();
	virtual void emit(const DebugLoc &);
	virtual void emit(const Twine &);
	virtual llvm::raw_ostream &os() { return nulls(); }
private:
	struct ExpString {
		const MDNode *Unit;
		unsigned LineNo;
		StringRef Content;
		ExpString(const MDNode *MD, unsigned N, StringRef Str)
			: Unit(MD), LineNo(N), Content(Str) {}
		bool operator <(const ExpString &other) {
			if (Unit != other.Unit)
				return Unit < other.Unit;
			if (LineNo != other.LineNo)
				return LineNo < other.LineNo;
			return Content < other.Content;
		}
	};
	typedef std::list<ExpString> ExpList;

	std::string Prefix;
	Module &M;
	FileCache Cache;
	ExpList Exps, Acts;
	MDNode *CurUnit;
	unsigned CurLine;

	void addExp(const MDNode *, unsigned LineNo, StringRef);
	static void report(raw_ostream &, ExpList &, const char *Category);
	static MDNode *getCompileUnit(MDNode *);
};

} // anonymous namespace

// Diagnostic

Diagnostic::Diagnostic(Module &M) {
	if (const char *Prefix = std::getenv("VERIFY_PREFIX")) {
		Diag.reset(new BugVerifier(Prefix, M));
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

void BugReporter::emit(const Twine &Str) {
	if (isDisplayed)
		OS.changeColor(raw_ostream::CYAN);
	OS << Str << '\n';
	if (isDisplayed)
		OS.resetColor();
	if (!Line.empty())
		OS << Line << '\n';
}

// BugVerifier

BugVerifier::BugVerifier(const char *Str, Module &M) : M(M) {
	Prefix = std::string("// ") + Str;
	DebugInfoFinder DIF;
	DIF.processModule(M);
	DebugInfoFinder::iterator i = DIF.compile_unit_begin(), e = DIF.compile_unit_end();
	for (; i != e; ++i) {
		const MDNode *MD = *i;
		StringRef First, Second = Cache.getFile(MD);
		for (unsigned LineNo = 1, EffectiveLineNo = 1; !Second.empty(); ) {
			tie(First, Second) = Second.split('\n');
			addExp(MD, EffectiveLineNo, First);
			++LineNo;
			if (!First.endswith("\\"))
				EffectiveLineNo = LineNo;
		}
	}
}

BugVerifier::~BugVerifier() {
	Exps.sort();
	Acts.sort();
	ExpList FN, FP;
	std::set_difference(Exps.begin(), Exps.end(), Acts.begin(), Acts.end(),
		std::back_inserter(FN));
	std::set_difference(Acts.begin(), Acts.end(), Exps.begin(), Exps.end(),
		std::back_inserter(FP));
	raw_ostream &OS = errs();
	if (!FN.empty())
		report(OS, FN, "missing");
	if (!FP.empty())
		report(OS, FP, "superfluous");
	if (!FN.empty() || !FP.empty()) {
		OS << "test failed\n";
		exit(1);
	}
}

void BugVerifier::emit(const DebugLoc &DbgLoc) {
	CurUnit = getCompileUnit(DbgLoc.getScope(M.getContext()));
	CurLine = DbgLoc.getLine();
}

void BugVerifier::emit(const Twine &Str) {
	SmallString<256> Buf;
	StringRef Actual = Str.toStringRef(Buf);
	Acts.push_back(ExpString(CurUnit, CurLine, Actual));
}

void BugVerifier::addExp(const MDNode *MD, unsigned LineNo, StringRef Str) {
	// Extract expected string.
	Str = Str.split(Prefix).second;
	for (;;) {
		StringRef ExpStr;
		tie(ExpStr, Str) = Str.split("{{").second.split("}}");
		if (Str.empty())
			break;
		Exps.push_back(ExpString(MD, LineNo, ExpStr));
	}
}

void BugVerifier::report(raw_ostream &OS, ExpList &L, const char *Category) {
	OS << L.size() << ' ' << Category << '\n';
	ExpList::const_iterator i = L.begin(), e = L.end();
	for (; i != e; ++i) {
		OS << DICompileUnit(i->Unit).getFilename() << ":";
		OS << i->LineNo << ": ";
		OS << i->Content << '\n';
	}
}

MDNode *BugVerifier::getCompileUnit(MDNode *MD) {
	DIScope N(MD);
	if (N.isCompileUnit())
		return MD;
	if (N.isFile())
		return DIFile(MD).getCompileUnit();
	if (N.isType())
		return DIType(MD).getCompileUnit();
	if (N.isSubprogram())
		return DISubprogram(MD).getCompileUnit();
	if (N.isGlobalVariable())
		return DIGlobalVariable(MD).getCompileUnit();
	if (N.isVariable())
		return DIVariable(MD).getCompileUnit();
	if (N.isNameSpace())
		return DINameSpace(MD).getCompileUnit();
	assert(0);
}
