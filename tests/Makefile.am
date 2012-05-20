EXTRA_DIST = lit.cfg.in kcc kgcc kint.in kcmp.in

LITFLAGS ?= -v
check-local: lit.cfg kcc kgcc kint kcmp
	@`llvm-config --src-root`/utils/lit/lit.py $(LITFLAGS) $(builddir)
