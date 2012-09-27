EXTRA_DIST = lit.cfg.in kcc kgcc kg++ kint.in kcmp.in kdiff

LITFLAGS ?= -v
check-local: lit.cfg kcc kgcc kg++ kint kcmp kdiff
	@`llvm-config --src-root`/utils/lit/lit.py $(LITFLAGS) $(builddir)
