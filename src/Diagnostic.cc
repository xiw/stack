#include "Diagnostic.h"
#include "FileCache.h"
#include <llvm/Module.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/DebugLoc.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <list>

using namespace llvm;

static cl::opt<std::string>
CheckPrefix("check-prefix",
            cl::desc("Specify a specific prefix to match"),
            cl::value_desc("prefix"));

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
	BugVerifier(const std::string &, Module &);
	~BugVerifier();
	virtual void emit(const DebugLoc &);
	virtual void emit(const Twine &);
	virtual llvm::raw_ostream &os() { return nulls(); }
private:
	struct ExpString {
		SmallString<64> Path;
		unsigned LineNo;
		StringRef Content;
		ExpString(const MDNode *MD, unsigned N, StringRef Str)
			: LineNo(N), Content(Str) { FileCache::getPath(Path, MD); }
		bool operator <(const ExpString &other) {
			if (Path != other.Path)
				return Path < other.Path;
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
	MDNode *CurScope;
	unsigned CurLine;

	void addExp(const MDNode *, unsigned LineNo, StringRef);
	static void report(raw_ostream &, ExpList &, const char *Category);
};

} // anonymous namespace

// Diagnostic

Diagnostic::Diagnostic(Module &M) {
	if (CheckPrefix.empty())
		Diag.reset(new BugReporter(M));
	else
		Diag.reset(new BugVerifier(CheckPrefix, M));
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
		SmallString<64> Path;
		FileCache::getPath(Path, Loc);
		OS << Path << ':' << Loc.getLineNumber() << ':';
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

BugVerifier::BugVerifier(const std::string &Str, Module &M) : M(M) {
	Prefix = "// " + Str + ":";
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
	CurScope = DbgLoc.getScope(M.getContext());
	CurLine = DbgLoc.getLine();
}

void BugVerifier::emit(const Twine &Str) {
	SmallString<256> Buf;
	StringRef Actual = Str.toStringRef(Buf);
	Acts.push_back(ExpString(CurScope, CurLine, Actual));
}

void BugVerifier::addExp(const MDNode *MD, unsigned LineNo, StringRef Str) {
	// Extract expected string.
	Str = Str.split(Prefix).second;
	while (!Str.empty()) {
		StringRef ExpStr;
		tie(ExpStr, Str) = Str.split("{{").second.split("}}");
		if (ExpStr.empty())
			continue;
		Exps.push_back(ExpString(MD, LineNo, ExpStr));
	}
}

void BugVerifier::report(raw_ostream &OS, ExpList &L, const char *Category) {
	OS << L.size() << ' ' << Category << '\n';
	ExpList::const_iterator i = L.begin(), e = L.end();
	for (; i != e; ++i) {
		OS << i->Path << ":";
		OS << i->LineNo << ": ";
		OS << i->Content << '\n';
	}
}
