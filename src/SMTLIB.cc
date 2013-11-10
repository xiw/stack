#include "config.h"
#include "SMTSolver.h"
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Allocator.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <sys/wait.h>
#include <err.h>
#include <stdarg.h>
#include <termios.h>
#include <unistd.h>

extern "C" int forkpty(int *amaster, char *name, struct termios *termp, struct winsize *winp);

using namespace llvm;

struct SMTExprImpl {
	unsigned width;
	const char *data;
};

class SMTContextImpl {
public:
	SMTExprImpl *bvtrue;
	SMTExprImpl *bvfalse;
	pid_t pid;
	FILE *fp;

	explicit SMTContextImpl(pid_t pid, int fd) : pid(pid) {
		fp = fdopen(fd, "r+");
		setbuf(fp, NULL);
		bvtrue = newexpr(1, "(_ bv1 1)");
		bvfalse = newexpr(1, "(_ bv0 1)");
	}

	~SMTContextImpl() {
		fclose(fp);
		int status;
		waitpid(pid, &status, 0);
	}

	void write(const char *fmt, ...) {
		va_list args;
		va_start(args, fmt);
		vfprintf(fp, fmt, args);
		va_end(args);
	}

	void readline(char *buf, size_t len) {
		if (!fgets(buf, len, fp))
			errx(1, "fgets");
	} 

	SMTExprImpl *newexpr(unsigned width, const char *data, size_t len) {
		char *dup = (char *)alloc.Allocate(len + 1, AlignOf<char>::Alignment);
		memcpy(dup, data, len);
		dup[len] = 0;
		SMTExprImpl *e_ = alloc.Allocate<SMTExprImpl>();
		e_->width = width;
		e_->data = static_cast<const char *>(dup);
		return e_;
	}

	SMTExprImpl *newexpr(unsigned width, const char *s) {
		return newexpr(width, s, strlen(s));
	}

	SMTExprImpl *newexpr(unsigned width, StringRef s) {
		return newexpr(width, s.data(), s.size());
	}

	SMTExprImpl *bv2bool(SMTExprImpl *e_) {
		std::string s = "(= " + std::string(e_->data) + " (_ bv1 1))";
		return newexpr(1, s);
	}
	
	SMTExprImpl *bool2bv(SMTExprImpl *e_) {
		std::string s = "(ite " + std::string(e_->data) + " (_ bv1 1) (_ bv0 1))";
		return newexpr(1, s);
	}

	SMTExprImpl *uniop(const std::string &op, unsigned width, SMTExprImpl *e_) {
		std::string s = "(" + op + " " + e_->data + ")";
		return newexpr(width, s);
	}
	
	SMTExprImpl *binop(const std::string &op, unsigned width, SMTExprImpl *lhs_, SMTExprImpl *rhs_) {
		std::string s = "(" + op + " " + lhs_->data + " " + rhs_->data + ")";
		return newexpr(width, s);
	}
	
	SMTExprImpl *triop(const std::string &op, unsigned width, SMTExprImpl *e_, SMTExprImpl *lhs_, SMTExprImpl *rhs_) {
		std::string s = "(" + op + " " + e_->data + " " + lhs_->data + " " + rhs_->data + ")";
		return newexpr(width, s);
	}

	SMTExprImpl *newint(const APInt &Val) {
		unsigned w = Val.getBitWidth();
		std::string s = "(_ bv" + Val.toString(10, false) + " " + utostr(w) + ")";
		return newexpr(w, s);
	}

private:
	BumpPtrAllocator alloc;
};

#define ctx ((SMTContextImpl *)ctx_)
#define e   ((SMTExprImpl *)e_)
#define lhs ((SMTExprImpl *)lhs_)
#define rhs ((SMTExprImpl *)rhs_)

SMTSolver::SMTSolver(bool modelgen) {
	const char *cmd = getenv("SMTLIB");
	if (!cmd)
		cmd = SMTLIB;
	int fd;
	pid_t pid = forkpty(&fd, NULL, NULL, NULL);
	if (pid < 0)
		err(1, "forkpty");
	if (pid == 0) {
		execl("/bin/sh", "sh", "-c", cmd, NULL);
		err(1, "execl");
	}
	// Disable echo.
	struct termios term = {0};
	if (tcgetattr(fd, &term))
		err(1, "tcgetattr");
	term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON);
	term.c_oflag &= ~(ONLCR);
	if (tcsetattr(fd, TCSANOW, &term))
		err(1, "tcsetattr");
	ctx_ = new SMTContextImpl(pid, fd);
	ctx->write("(set-option :print-success false)\n");
}

SMTSolver::~SMTSolver() {
	delete ctx;
}

void SMTSolver::assume(SMTExpr e_) {
	ctx->write("(assert %s)\n", ctx->bv2bool(e)->data);
}

SMTStatus SMTSolver::query(SMTExpr e_, SMTModel *m_) {
	ctx->write("(push 1)\n");
	assume(e);
	ctx->write("(check-sat)\n");
	char buf[16];
	ctx->readline(buf, sizeof(buf));
	ctx->write("(pop 1)\n");
	StringRef status = StringRef(buf).rtrim();
	if (status == "unsat")
		return SMT_UNSAT;
	if (status == "sat")
		return SMT_SAT;
	dbgs() << "[SMTLIB] unknown response: " << status << "\n";
	return SMT_UNDEF;
}

void SMTSolver::eval(SMTModel, SMTExpr, APInt &) {
}

void SMTSolver::release(SMTModel) {}

void SMTSolver::dump(SMTExpr e_) {
	print(e, dbgs());
	dbgs() << "\n";
}

void SMTSolver::print(SMTExpr e_, raw_ostream &OS) {
	// TODO
}

// Managed by BumpPtrAllocator.
void SMTSolver::incref(SMTExpr e_) {}
void SMTSolver::decref(SMTExpr e_) {}

unsigned SMTSolver::bvwidth(SMTExpr e_) {
	return e->width;
}

SMTExpr SMTSolver::bvfalse() {
	return ctx->bvfalse;
}

SMTExpr SMTSolver::bvtrue() {
	return ctx->bvtrue;
}

SMTExpr SMTSolver::bvconst(const APInt &Val) {
	return ctx->newint(Val);
}

SMTExpr SMTSolver::bvvar(unsigned width, const char *name) {
	ctx->write("(declare-fun %s () (_ BitVec %u))\n", name, width);
	return ctx->newexpr(width, name);;
}

SMTExpr SMTSolver::ite(SMTExpr e_, SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->triop("ite", lhs->width, ctx->bv2bool(e), lhs, rhs);
}

SMTExpr SMTSolver::eq(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("=", 1, lhs, rhs));
}

SMTExpr SMTSolver::ne(SMTExpr lhs_, SMTExpr rhs_) {
	return bvnot(eq(lhs_, rhs_));
}

SMTExpr SMTSolver::bvslt(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("bvslt", 1, lhs, rhs));
}

SMTExpr SMTSolver::bvsle(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("bvsle", 1, lhs, rhs));
}

SMTExpr SMTSolver::bvsgt(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("bvsgt", 1, lhs, rhs));
}

SMTExpr SMTSolver::bvsge(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("bvsge", 1, lhs, rhs));
}

SMTExpr SMTSolver::bvult(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("bvult", 1, lhs, rhs));
}

SMTExpr SMTSolver::bvule(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("bvule", 1, lhs, rhs));
}

SMTExpr SMTSolver::bvugt(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("bvugt", 1, lhs, rhs));
}

SMTExpr SMTSolver::bvuge(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->bool2bv(ctx->binop("bvuge", 1, lhs, rhs));
}

SMTExpr SMTSolver::extract(unsigned high, unsigned low, SMTExpr e_) {
	std::string op = "(_ extract " + utostr(high) + " " + utostr(low) + ")";
	return ctx->uniop(op, high - low + 1, e);
}

SMTExpr SMTSolver::zero_extend(unsigned i, SMTExpr e_) {
	std::string op = "(_ zero_extend " + utostr(i) + ")";
	return ctx->uniop(op, e->width + i, e);
}

SMTExpr SMTSolver::sign_extend(unsigned i, SMTExpr e_) {
	std::string op = "(_ sign_extend " + utostr(i) + ")";
	return ctx->uniop(op, e->width + i, e);
}

SMTExpr SMTSolver::bvredand(SMTExpr e_) {
	SMTExprImpl *umax = ctx->newint(APInt::getAllOnesValue(e->width));
	return ctx->binop("bvcomp", 1, e, umax);
}

SMTExpr SMTSolver::bvredor(SMTExpr e_) {
	SMTExprImpl *zero = ctx->newint(APInt::getNullValue(e->width));
	return bvnot(ctx->binop("bvcomp", 1, e, zero));
}

SMTExpr SMTSolver::bvnot(SMTExpr e_) {
	return ctx->uniop("bvnot", e->width, e);
}

SMTExpr SMTSolver::bvneg(SMTExpr e_) {
	return ctx->uniop("bvneg", e->width, e);
}

SMTExpr SMTSolver::bvadd(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvadd", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvsub(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvsub", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvmul(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvmul", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvsdiv(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvsdiv", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvudiv(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvudiv", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvsrem(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvsrem", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvurem(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvurem", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvshl(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvshl", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvlshr(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvlshr", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvashr(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvashr", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvand(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvand", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvor(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvor", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvxor(SMTExpr lhs_, SMTExpr rhs_) {
	return ctx->binop("bvxor", lhs->width, lhs, rhs);
}

SMTExpr SMTSolver::bvneg_overflow(SMTExpr e_) {
	unsigned w = e->width;
	SMTExprImpl *smin = ctx->newint(APInt::getSignedMinValue(w));
	return eq(e, smin);
}

SMTExpr SMTSolver::bvsadd_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	unsigned w = lhs->width;
	// Overflow:
	// * lhs and rhs are of the same sign, and
	// * sum has a different sign.
	SMTExpr lhs_sign = extract(w - 1, w - 1, lhs);
	SMTExpr rhs_sign = extract(w - 1, w - 1, lhs);
	SMTExpr sum_sign = extract(w - 1, w - 1, bvadd(lhs, rhs));
	return bvand(eq(lhs_sign, rhs_sign), ne(lhs_sign, sum_sign));
}

SMTExpr SMTSolver::bvuadd_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	unsigned w = lhs->width;
	return extract(w, w, bvadd(zero_extend(1, lhs), zero_extend(1, rhs)));
}

SMTExpr SMTSolver::bvssub_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	unsigned w = lhs->width;
	// Overflow:
	// * lhs and rhs are of the opposite sign, and
	// * diff has a different sign from lhs.
	SMTExpr lhs_sign = extract(w - 1, w - 1, lhs);
	SMTExpr rhs_sign = extract(w - 1, w - 1, lhs);
	SMTExpr diff_sign = extract(w - 1, w - 1, bvsub(lhs, rhs));
	return bvand(ne(lhs_sign, rhs_sign), ne(lhs_sign, diff_sign));
}

SMTExpr SMTSolver::bvusub_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	return bvult(lhs, rhs);
}

SMTExpr SMTSolver::bvsmul_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	unsigned w = lhs->width;
	SMTExpr lhs_sign = extract(w - 1, w - 1, lhs);
	SMTExpr rhs_sign = extract(w - 1, w - 1, lhs);
	// sign_bits:
	// * 000..0 if lhs and rhs are of the same sign;
	// * 111..1 if lhs and rhs are of the opposite sign.
	SMTExpr sign_bits = sign_extend(w, ne(lhs_sign, rhs_sign));
	SMTExpr prod = bvmul(sign_extend(w, lhs), sign_extend(w, rhs));
	SMTExpr high_bits = extract(w + w - 1, w - 1, prod);
	return ne(high_bits, sign_bits);
}

SMTExpr SMTSolver::bvumul_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	unsigned w = lhs->width;
	SMTExpr prod = bvmul(zero_extend(w, lhs), zero_extend(w, rhs));
	return bvredor(extract(w + w - 1, w, prod));
}

SMTExpr SMTSolver::bvsdiv_overflow(SMTExpr lhs_, SMTExpr rhs_) {
	unsigned w = lhs->width;
	SMTExprImpl *minusone = ctx->newint(APInt::getAllOnesValue(w));
	return bvand(bvneg_overflow(lhs), eq(rhs, minusone));
}
