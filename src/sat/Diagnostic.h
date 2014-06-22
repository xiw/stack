#pragma once

namespace llvm {
	class Instruction;
	class MDNode;
	class raw_ostream;
	class Twine;
} // namespace llvm


class Diagnostic {
public:
	typedef llvm::Instruction Instruction;

	// Return if I has a non-inlined debug location.
	static bool hasSingleDebugLocation(Instruction *I);

	Diagnostic();

	llvm::raw_ostream &os() { return OS; }

	void bug(Instruction *);
	void bug(const llvm::Twine &);

	void backtrace(Instruction *);
	void location(llvm::MDNode *);
	void status(int);

	template <typename T> Diagnostic &
	operator <<(const T &Val) {
		OS << Val;
		return *this;
	}

private:
	llvm::raw_ostream &OS;
};
