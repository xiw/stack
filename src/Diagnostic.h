#pragma once

namespace llvm {
	class Instruction;
	class raw_ostream;
}

class Diagnostic {
public:
	Diagnostic();

	llvm::raw_ostream &os() { return OS; }

	void backtrace(llvm::Instruction *, const char *);

	template <typename T> Diagnostic &
	operator <<(const T &Val) {
		OS << Val;
		return *this;
	}

private:
	llvm::raw_ostream &OS;
};
