#include "SMTSolver.h"
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
extern "C" {
#include <boolector/boolector.h>
}

using namespace llvm;

#define ctx ((Btor *)ctx_)
#define m   ((Btor *)m_)
#define e   ((BtorNode *)e_)
#define lhs ((BtorNode *)lhs_)
#define rhs ((BtorNode *)rhs_)

// Boolector 1.5 is much slower due to the new SAT backend.
// Use the workaround to disable preprocessing for performance.
namespace {
	struct SMTWorkaround {
		SMTWorkaround() { ::setenv("LGLPLAIN", "1", 1); }
	};
}

static SMTWorkaround X;

SMTSolver::SMTSolver(bool modelgen) {
	ctx_ = boolector_new();
	if (modelgen)
		boolector_enable_model_gen(ctx);
	boolector_enable_inc_usage(ctx);
}

SMTSolver::~SMTSolver() {
	assert(boolector_get_refs(ctx) == 0);
	boolector_delete(ctx);
}

void SMTSolver::assume(SMTExpr e_) {
	boolector_assert(ctx, e);
}

SMTStatus SMTSolver::query(SMTExpr e_, SMTModel *m_) {
	boolector_assume(ctx, e);
	switch (boolector_sat(ctx)) {
	default:              return SMT_UNDEF;
	case BOOLECTOR_UNSAT: return SMT_UNSAT;
	case BOOLECTOR_SAT:   break;
	}
	if (m_)
		*m_ = ctx_;
	return SMT_SAT;
}

void SMTSolver::eval(SMTModel m_, SMTExpr e_, APInt &v) {
	char *s = boolector_bv_assignment(ctx, e);
	std::string str(s);
	boolector_free_bv_assignment(ctx, s);
	std::replace(str.begin(), str.end(), 'x', '0');
	v = APInt(bvwidth(e), str.c_str(), 2);
}

void SMTSolver::release(SMTModel m_) {}

void SMTSolver::dump(SMTExpr e_) {
	print(e, dbgs());
	dbgs() << "\n";
}

void SMTSolver::print(SMTExpr e_, raw_ostream &OS) {
	FILE *fp = tmpfile();
	assert(fp && "tmpfile");
	boolector_dump_btor(ctx, fp, e);
	long n = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *s = (char *)malloc(n + 1);
	n = fread(s, 1, n, fp);
	assert(n > 0);
	s[n] = 0;
	OS << s;
	free(s);
	fclose(fp);
}

void SMTSolver::incref(SMTExpr e_) {
	boolector_copy(ctx, e);
}

void SMTSolver::decref(SMTExpr e_) {
	boolector_release(ctx, e);
}

unsigned SMTSolver::bvwidth(SMTExpr e_) {
	return boolector_get_width(ctx, e);
}

SMTExpr SMTSolver::bvfalse() {
	return boolector_false(ctx);
}

SMTExpr SMTSolver::bvtrue() {
	return boolector_true(ctx);
}

SMTExpr SMTSolver::bvconst(const APInt &Val) {
	unsigned intbits = sizeof(unsigned) * CHAR_BIT;
	unsigned width = Val.getBitWidth();
	if (width <= intbits)
		return boolector_unsigned_int(ctx, Val.getZExtValue(), width);
	SmallString<32> Str, FullStr;
	Val.toStringUnsigned(Str, 2);
	assert(Str.size() <= width);
	FullStr.assign(width - Str.size(), '0');
	FullStr += Str;
	return boolector_const(ctx, FullStr.c_str());
}

SMTExpr SMTSolver::bvvar(unsigned width, const char *name) {
	return boolector_var(ctx, width, name);
}

SMTExpr SMTSolver::ite(SMTExpr e_, SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_cond(ctx, e, lhs, rhs);
}

SMTExpr SMTSolver::eq(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_eq(ctx, lhs, rhs);
}

SMTExpr SMTSolver::ne(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_ne(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvslt(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_slt(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsle(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_slte(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsgt(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_sgt(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsge(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_sgte(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvult(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_ult(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvule(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_ulte(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvugt(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_ugt(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvuge(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_ugte(ctx, lhs, rhs);
}

SMTExpr SMTSolver::extract(unsigned high, unsigned low, SMTExpr e_) {
	return boolector_slice(ctx, e, high, low);
}

SMTExpr SMTSolver::zero_extend(unsigned i, SMTExpr e_) {
	return boolector_uext(ctx, e, i);
}

SMTExpr SMTSolver::sign_extend(unsigned i, SMTExpr e_) {
	return boolector_sext(ctx, e, i);
}

SMTExpr SMTSolver::bvredand(SMTExpr e_) {
	return boolector_redand(ctx, e);
}

SMTExpr SMTSolver::bvredor(SMTExpr e_) {
	return boolector_redor(ctx, e);
}

SMTExpr SMTSolver::bvnot(SMTExpr e_) {
	return boolector_not(ctx, e);
}

SMTExpr SMTSolver::bvneg(SMTExpr e_) {
	return boolector_neg(ctx, e);
}

SMTExpr SMTSolver::bvadd(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_add(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsub(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_sub(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvmul(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_mul(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsdiv(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_sdiv(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvudiv(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_udiv(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsrem(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_srem(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvurem(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_urem(ctx, lhs, rhs);
}

// Shift operations use log2n bits for shifting amount. 
template <BtorNode *(*F)(Btor *, BtorNode *,  BtorNode *)>
static inline BtorNode *shift(Btor *btor,  BtorNode *e0,  BtorNode *e1) {
	unsigned n = boolector_get_width(btor, e1);
	// Round up to nearest power of 2.
	unsigned amount = (sizeof(n) * CHAR_BIT - __builtin_clz(n - 1));
	unsigned power2 = 1U << amount;
	// Extend e0 to power2 bits.
	if (power2 != n)
		e0 = boolector_uext(btor, e0, power2 - n);
	BtorNode *log2w = boolector_slice(btor, e1, amount - 1, 0);
	BtorNode *result = F(btor, e0, log2w);
	boolector_release(btor, log2w);
	if (power2 != n) {
		boolector_release(btor, e0);
		// Truncate result back to n bits.
		BtorNode *tmp = boolector_slice(btor, result, n - 1, 0);
		boolector_release(btor, result);
		result = tmp;
	}
	return result;
}

SMTExpr SMTSolver::bvshl(SMTExpr lhs_, SMTExpr rhs_) {
	// Return 0 if rhs >= n.
	unsigned n = boolector_get_width(ctx, rhs);
	BtorNode *width = boolector_unsigned_int(ctx, n, n);
	BtorNode *cond = boolector_ugte(ctx, rhs, width);
	BtorNode *zero = boolector_zero(ctx, n);
	BtorNode *tmp = shift<boolector_sll>(ctx, lhs, rhs);
	BtorNode *result = boolector_cond(ctx, cond, zero, tmp);
	boolector_release(ctx, width);
	boolector_release(ctx, cond);
	boolector_release(ctx, zero);
	boolector_release(ctx, tmp);
	return result;
}

SMTExpr SMTSolver::bvlshr(SMTExpr lhs_, SMTExpr rhs_) {
	// Return 0 if rhs >= n.
	unsigned n = boolector_get_width(ctx, rhs);
	BtorNode *width = boolector_unsigned_int(ctx, n, n);
	BtorNode *cond = boolector_ugte(ctx, rhs, width);
	BtorNode *zero = boolector_zero(ctx, n);
	BtorNode *tmp = shift<boolector_srl>(ctx, lhs, rhs);
	BtorNode *result = boolector_cond(ctx, cond, zero, tmp);
	boolector_release(ctx, width);
	boolector_release(ctx, cond);
	boolector_release(ctx, zero);
	boolector_release(ctx, tmp);
	return result;
}

SMTExpr SMTSolver::bvashr(SMTExpr lhs_, SMTExpr rhs_) {
	// If rhs is too large, the result is either zero or all-one,
	// the same as limiting rhs to n - 1.
	unsigned n = boolector_get_width(ctx, rhs);
	BtorNode *maxw = boolector_unsigned_int(ctx, n - 1, n);
	BtorNode *cond = boolector_ugt(ctx, rhs, maxw);
	BtorNode *rhs_max = boolector_cond(ctx, cond, maxw, rhs);
	BtorNode *result = shift<boolector_sra>(ctx, lhs, rhs_max);
	boolector_release(ctx, maxw);
	boolector_release(ctx, cond);
	boolector_release(ctx, rhs_max);
	return result;
}

SMTExpr SMTSolver::bvand(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_and(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvor(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_or(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvxor(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_xor(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvneg_overflow(SMTExpr e_) {
	SMTExpr zero = boolector_zero(ctx, bvwidth(e));
	SMTExpr tmp = bvssub_overflow(zero, e);
	decref(zero);
	return tmp;
}

SMTExpr SMTSolver::bvsadd_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_saddo(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvuadd_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_uaddo(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvssub_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_ssubo(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvusub_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_usubo(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsmul_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_smulo(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvumul_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_umulo(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsdiv_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return boolector_sdivo(ctx, lhs, rhs);
}
