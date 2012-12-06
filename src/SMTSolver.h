#pragma once

namespace llvm {
	class APInt;
	class raw_ostream;
} // namespace llvm

enum SMTStatus {
	SMT_TIMEOUT = -1,
	SMT_UNDEF,
	SMT_UNSAT,
	SMT_SAT,
};

typedef void *SMTContext;
typedef void *SMTExpr;
typedef void *SMTModel;

int SMTFork();
void SMTJoin(int *);

class SMTSolver {
public:
	SMTSolver(bool modelgen);
	~SMTSolver();

	void assume(SMTExpr);

	SMTStatus query(SMTExpr, SMTModel * = 0);
	void eval(SMTModel, SMTExpr, llvm::APInt &);
	void release(SMTModel);

	void dump(SMTExpr);
	void print(SMTExpr, llvm::raw_ostream &);

	void incref(SMTExpr);
	void decref(SMTExpr);

	unsigned bvwidth(SMTExpr);

	SMTExpr bvfalse();
	SMTExpr bvtrue();
	SMTExpr bvconst(const llvm::APInt &);
	SMTExpr bvvar(unsigned width, const char *name);

	// If-else-then.
	SMTExpr ite(SMTExpr, SMTExpr, SMTExpr);

	// Comparison.
	SMTExpr eq(SMTExpr, SMTExpr);
	SMTExpr ne(SMTExpr, SMTExpr);
	SMTExpr bvslt(SMTExpr, SMTExpr);
	SMTExpr bvsle(SMTExpr, SMTExpr);
	SMTExpr bvsgt(SMTExpr, SMTExpr);
	SMTExpr bvsge(SMTExpr, SMTExpr);
	SMTExpr bvult(SMTExpr, SMTExpr);
	SMTExpr bvule(SMTExpr, SMTExpr);
	SMTExpr bvugt(SMTExpr, SMTExpr);
	SMTExpr bvuge(SMTExpr, SMTExpr);

	SMTExpr extract(unsigned high, unsigned low, SMTExpr);
	SMTExpr zero_extend(unsigned i, SMTExpr);
	SMTExpr sign_extend(unsigned i, SMTExpr);

	SMTExpr bvredand(SMTExpr);
	SMTExpr bvredor(SMTExpr);
	SMTExpr bvnot(SMTExpr);
	SMTExpr bvneg(SMTExpr);

	// Arithmetic operations.
	SMTExpr bvadd(SMTExpr, SMTExpr);
	SMTExpr bvsub(SMTExpr, SMTExpr);
	SMTExpr bvmul(SMTExpr, SMTExpr);
	SMTExpr bvsdiv(SMTExpr, SMTExpr);
	SMTExpr bvudiv(SMTExpr, SMTExpr);
	SMTExpr bvsrem(SMTExpr, SMTExpr);
	SMTExpr bvurem(SMTExpr, SMTExpr);
	SMTExpr bvshl(SMTExpr, SMTExpr);
	SMTExpr bvlshr(SMTExpr, SMTExpr);
	SMTExpr bvashr(SMTExpr, SMTExpr);
	SMTExpr bvand(SMTExpr, SMTExpr);
	SMTExpr bvor(SMTExpr, SMTExpr);
	SMTExpr bvxor(SMTExpr, SMTExpr);

	// Overflow detection.
	SMTExpr bvneg_overflow(SMTExpr);
	SMTExpr bvsadd_overflow(SMTExpr, SMTExpr);
	SMTExpr bvuadd_overflow(SMTExpr, SMTExpr);
	SMTExpr bvssub_overflow(SMTExpr, SMTExpr);
	SMTExpr bvusub_overflow(SMTExpr, SMTExpr);
	SMTExpr bvsmul_overflow(SMTExpr, SMTExpr);
	SMTExpr bvumul_overflow(SMTExpr, SMTExpr);
	SMTExpr bvsdiv_overflow(SMTExpr, SMTExpr);

private:
	SMTContext ctx_;
};
