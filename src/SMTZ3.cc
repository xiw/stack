#include "SMTSolver.h"
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <assert.h>
#include <z3.h>

struct SMTContextImpl {
	Z3_context c;
	Z3_ast bvfalse;
	Z3_ast bvtrue;
};

#define ctx (ctx_->c)
#define m   ((Z3_model)m_)
#define e   ((Z3_ast)e_)
#define lhs ((Z3_ast)lhs_)
#define rhs ((Z3_ast)rhs_)

static inline Z3_ast bv2bool_(SMTContextImpl *ctx_, Z3_ast e0) {
	return Z3_mk_eq(ctx, e0, ctx_->bvtrue);
}

static inline Z3_ast bool2bv_(SMTContextImpl *ctx_, Z3_ast e0) {
	return Z3_mk_ite(ctx, e0, ctx_->bvtrue, ctx_->bvfalse);
}

#define bv2bool(x) bv2bool_(ctx_, x)
#define bool2bv(x) bool2bv_(ctx_, x)

SMTSolver::SMTSolver() {
	ctx_ = new SMTContextImpl;
	Z3_config cfg = Z3_mk_config();
	// Enable model construction.
	Z3_set_param_value(cfg, "MODEL", "true");
	ctx_->c = Z3_mk_context(cfg);
	Z3_del_config(cfg);
	// Set up constants.
	Z3_sort sort = Z3_mk_bv_sort(ctx, 1);
	ctx_->bvfalse = Z3_mk_int(ctx, 0, sort);
	ctx_->bvtrue = Z3_mk_int(ctx, 1, sort);
}

SMTSolver::~SMTSolver() {
	Z3_del_context(ctx);
	delete ctx_;
}

SMTStatus SMTSolver::query(SMTExpr e_, SMTModel *m_) {
	Z3_assert_cnstr(ctx, bv2bool(e));
	Z3_lbool res = Z3_check_and_get_model(ctx, (Z3_model *)m_);
	switch (res) {
	default:         return SMT_UNDEF;
	case Z3_L_FALSE: return SMT_UNSAT;
	case Z3_L_TRUE:  return SMT_SAT;
	}
}

void SMTSolver::eval(SMTModel m_, SMTExpr e_, llvm::raw_ostream &OS) {
	Z3_ast v = 0;
	Z3_bool ret = Z3_eval(ctx, m, e, &v);
	assert(ret);
	assert(!v);
	OS << Z3_ast_to_string(ctx, v);
}

void SMTSolver::release(SMTModel m_) {
	Z3_del_model(ctx, m);
}

void SMTSolver::dump(SMTExpr e_) {
	print(e, llvm::dbgs());
	llvm::dbgs() << "\n";
}

void SMTSolver::print(SMTExpr e_, llvm::raw_ostream &OS) {
	OS << Z3_ast_to_string(ctx, Z3_simplify(ctx, e));
}

// Managed by Z3, no reference counting.
void SMTSolver::incref(SMTExpr) { }
void SMTSolver::decref(SMTExpr) { }

unsigned SMTSolver::bvwidth(SMTExpr e_) {
	return Z3_get_bv_sort_size(ctx, Z3_get_sort(ctx, e));
}

SMTExpr SMTSolver::bvfalse() {
	return ctx_->bvfalse;
}

SMTExpr SMTSolver::bvtrue() {
	return ctx_->bvtrue;
}

SMTExpr SMTSolver::bvconst(const llvm::APInt &Val) {
	unsigned width = Val.getBitWidth();
	Z3_sort t = Z3_mk_bv_sort(ctx, width);
	if (width <= 64)
		return Z3_mk_unsigned_int64(ctx, Val.getZExtValue(), t);
	llvm::SmallString<32> s;
	Val.toStringUnsigned(s);
	return Z3_mk_numeral(ctx, s.c_str(), t);
}

SMTExpr SMTSolver::bvvar(unsigned width, const char *name) {
	return Z3_mk_const(ctx, Z3_mk_string_symbol(ctx, name), Z3_mk_bv_sort(ctx, width));
}

SMTExpr SMTSolver::ite(SMTExpr e_, SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_ite(ctx, bv2bool(e), lhs, rhs);
}

SMTExpr SMTSolver::eq(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_eq(ctx, lhs, rhs));
}

SMTExpr SMTSolver::ne(SMTExpr lhs_, SMTExpr rhs_) {
	return bvnot(eq(lhs_, rhs_));
}

SMTExpr SMTSolver::bvslt(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_bvslt(ctx, lhs, rhs));
}

SMTExpr SMTSolver::bvsle(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_bvsle(ctx, lhs, rhs));
}

SMTExpr SMTSolver::bvsgt(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_bvsgt(ctx, lhs, rhs));
}

SMTExpr SMTSolver::bvsge(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_bvsge(ctx, lhs, rhs));
}

SMTExpr SMTSolver::bvult(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_bvult(ctx, lhs, rhs));
}

SMTExpr SMTSolver::bvule(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_bvule(ctx, lhs, rhs));
}

SMTExpr SMTSolver::bvugt(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_bvugt(ctx, lhs, rhs));
}

SMTExpr SMTSolver::bvuge(SMTExpr lhs_, SMTExpr rhs_) {
	return bool2bv(Z3_mk_bvuge(ctx, lhs, rhs));
}

SMTExpr SMTSolver::extract(unsigned high, unsigned low, SMTExpr e_) {
	return Z3_mk_extract(ctx, high, low, e);
}

SMTExpr SMTSolver::zero_extend(unsigned i, SMTExpr e_) {
	return Z3_mk_zero_ext(ctx, i, e);
}

SMTExpr SMTSolver::sign_extend(unsigned i, SMTExpr e_) {
	return Z3_mk_sign_ext(ctx, i, e);
}

SMTExpr SMTSolver::bvredand(SMTExpr e_) {
	return Z3_mk_bvredand(ctx, e);
}

SMTExpr SMTSolver::bvredor(SMTExpr e_) {
	return Z3_mk_bvredor(ctx, e);
}

SMTExpr SMTSolver::bvnot(SMTExpr e_) {
	return Z3_mk_bvnot(ctx, e);
}

SMTExpr SMTSolver::bvneg(SMTExpr e_) {
	return Z3_mk_bvneg(ctx, e);
}

SMTExpr SMTSolver::bvadd(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvadd(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsub(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvsub(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvmul(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvmul(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsdiv(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvsdiv(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvudiv(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvudiv(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvsrem(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvsrem(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvurem(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvurem(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvshl(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvshl(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvlshr(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvlshr(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvashr(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvashr(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvand(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvand(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvor(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvor(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvxor(SMTExpr lhs_, SMTExpr rhs_) {
	return Z3_mk_bvxor(ctx, lhs, rhs);
}

SMTExpr SMTSolver::bvneg_overflow(SMTExpr e_) {
	return bvnot(bool2bv(Z3_mk_bvneg_no_overflow(ctx, e)));
}

SMTExpr SMTSolver::bvsadd_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return bvor(
		bvnot(bool2bv(Z3_mk_bvadd_no_overflow(ctx, lhs, rhs, Z3_TRUE))),
		bvnot(bool2bv(Z3_mk_bvadd_no_underflow(ctx, lhs, rhs)))
	);
}

SMTExpr SMTSolver::bvuadd_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return bvnot(bool2bv(Z3_mk_bvadd_no_overflow(ctx, lhs, rhs, Z3_FALSE)));
}

SMTExpr SMTSolver::bvssub_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return bvor(
		bvnot(bool2bv(Z3_mk_bvsub_no_overflow(ctx, lhs, rhs))),
		bvnot(bool2bv(Z3_mk_bvsub_no_underflow(ctx, lhs, rhs, Z3_TRUE)))
	);
}

SMTExpr SMTSolver::bvusub_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return bvnot(bool2bv(Z3_mk_bvsub_no_underflow(ctx, lhs, rhs, Z3_FALSE)));
}

SMTExpr SMTSolver::bvsmul_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return bvor(
		bvnot(bool2bv(Z3_mk_bvmul_no_overflow(ctx, lhs, rhs, Z3_TRUE))),
		bvnot(bool2bv(Z3_mk_bvmul_no_underflow(ctx, lhs, rhs)))
	);
}

SMTExpr SMTSolver::bvumul_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return bvnot(bool2bv(Z3_mk_bvmul_no_overflow(ctx, lhs, rhs, Z3_FALSE)));
}

SMTExpr SMTSolver::bvsdiv_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return bvnot(bool2bv(Z3_mk_bvsdiv_no_overflow(ctx, lhs, rhs)));
}
