#include "SMTSolver.h"
#include <sonolar/sonolar.h>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <assert.h>

using namespace llvm;

#define ctx ((sonolar_t)ctx_)
#define m   ((sonolar_t *)m_)
#define e   ((sonolar_term_t *)e_)
#define lhs ((sonolar_term_t *)lhs_)
#define rhs ((sonolar_term_t *)rhs_)

SMTSolver::SMTSolver(bool /*modelgen*/) {
	ctx_ = sonolar_create();
	if (sonolar_set_sat_solver(ctx, SONOLAR_SAT_SOLVER_MINISAT))
		assert(0 && "sonolar_set_sat_solver");
}

SMTSolver::~SMTSolver() {
	sonolar_destroy(ctx);
}

void SMTSolver::assume(SMTExpr e_) {
	sonolar_assert_formula(ctx, e);
}

SMTStatus SMTSolver::query(SMTExpr e_, SMTModel *m_) {
	if (sonolar_assume_formula(ctx, e))
		assert(0 && "sonolar_assume_formula");
	switch (sonolar_solve(ctx)) {
	default:                         return SMT_UNDEF;
	case SONOLAR_SOLVE_RESULT_UNSAT: return SMT_UNSAT;
	case SONOLAR_SOLVE_RESULT_SAT:   break;
	}
	if (m_)
		*m_ = ctx;
	return SMT_SAT;
}

void SMTSolver::eval(SMTModel m_, SMTExpr e_, APInt &) {
	assert(0 && "NOT SUPPORTED");
}

void SMTSolver::release(SMTModel m_) {}

void SMTSolver::dump(SMTExpr e_) {
	print(e, dbgs());
	dbgs() << "\n";
}

void SMTSolver::print(SMTExpr e_, raw_ostream &OS) {
	OS << "NOT SUPPORTED";
}

void SMTSolver::incref(SMTExpr e_) {
	sonolar_add_reference(ctx, e);
}

void SMTSolver::decref(SMTExpr e_) {
	sonolar_remove_reference(ctx, e);
}

unsigned SMTSolver::bvwidth(SMTExpr e_) {
	size_t width;
	if (sonolar_get_bitwidth(ctx, e, &width))
		assert(0 && "sonolar_get_bitwidth");
	return (unsigned)width;
}

SMTExpr SMTSolver::bvfalse() {
	return sonolar_make_constant_false(ctx);
}

SMTExpr SMTSolver::bvtrue() {
	return sonolar_make_constant_true(ctx);
}

SMTExpr SMTSolver::bvconst(const APInt &Val) {
	const void *data = Val.getRawData();
	unsigned width = Val.getBitWidth();
	return sonolar_make_constant_bytes(ctx, data, width, SONOLAR_BYTE_ORDER_NATIVE);
}

SMTExpr SMTSolver::bvvar(unsigned width, const char *name) {
	return sonolar_make_variable(ctx, width, name);
}

SMTExpr SMTSolver::ite(SMTExpr e_, SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_ite(ctx, e, lhs, rhs);
}

SMTExpr SMTSolver::eq(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_equal(ctx, lhs, rhs);
}

SMTExpr SMTSolver::ne(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_distinct(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvslt(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_slt(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsle(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_sle(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsgt(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_sgt(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsge(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_sge(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvult(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_ult(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvule(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_ule(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvugt(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_ugt(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvuge(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_uge(ctx, lhs, rhs);
}

SMTExpr SMTSolver::extract(unsigned high, unsigned low, SMTExpr e_) {
	return sonolar_make_bv_extract(ctx, e, high, low);
}

SMTExpr SMTSolver::zero_extend(unsigned i, SMTExpr e_) {
	return sonolar_make_bv_zero_extend(ctx, e, i);
}

SMTExpr SMTSolver::sign_extend(unsigned i, SMTExpr e_) {
	return sonolar_make_bv_sign_extend(ctx, e, i);
}

SMTExpr SMTSolver::bvredand(SMTExpr e_) {
	SMTExpr neg = bvnot(e);
	SMTExpr tmp = sonolar_make_is_zero(ctx, neg);
	decref(neg);
	return tmp;
}

SMTExpr SMTSolver::bvredor(SMTExpr e_) {
	SMTExpr z = sonolar_make_is_zero(ctx, e);
	SMTExpr nz = sonolar_make_not(ctx, z);
	decref(z);
	return nz;
}

SMTExpr SMTSolver::bvnot(SMTExpr e_) {
	return sonolar_make_bv_not(ctx, e);
}

SMTExpr SMTSolver::bvneg(SMTExpr e_) {
	return sonolar_make_bv_neg(ctx, e);
}

SMTExpr SMTSolver::bvadd(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_add(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsub(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_sub(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvmul(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_mul(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsdiv(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_sdiv(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvudiv(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_udiv(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsrem(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_srem(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvurem(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_urem(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvshl(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_shl(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvlshr(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_lshr(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvashr(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_ashr(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvand(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_and(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvor(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_or(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvxor(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_xor(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvneg_overflow(SMTExpr e_) {
	SMTExpr zero = sonolar_make_constant_0_bits(ctx, bvwidth(e));
	SMTExpr tmp = bvssub_overflow(zero, e);
	decref(zero);
	return tmp;
}

SMTExpr SMTSolver::bvsadd_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_sadd_ovfl(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvuadd_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_uadd_ovfl(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvssub_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_ssub_ovfl(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvusub_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_usub_ovfl(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsmul_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_smul_ovfl(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvumul_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_umul_ovfl(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsdiv_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return sonolar_make_bv_sdiv_ovfl(ctx, lhs, rhs);
}
