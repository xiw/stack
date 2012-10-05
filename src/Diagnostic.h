#pragma once

namespace llvm {
	class Instruction;
	class raw_ostream;
	class Twine;
} // namespace llvm


class Diagnostic {
public:
	Diagnostic();

	llvm::raw_ostream &os() { return OS; }

	void bug(const llvm::Twine &);

	void backtrace(llvm::Instruction *);
	void status(unsigned);

	template <typename T> Diagnostic &
	operator <<(const T &Val) {
		OS << Val;
		return *this;
	}

private:
	llvm::raw_ostream &OS;
};
