EXTRA_DIST = lit.cfg.in kcc kgcc kint.in kcmp.in kdiff

LITFLAGS ?= -v
check-local: lit.cfg kcc kgcc kint kcmp kdiff
	@`llvm-config --src-root`/utils/lit/lit.py $(LITFLAGS) $(builddir)
