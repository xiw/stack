EXTRA_DIST = lit.cfg.in kcc kgcc kg++ kdiff

LITFLAGS ?= -v
check-local: lit.cfg kcc kgcc kg++ kdiff
	@`llvm-config --src-root`/utils/lit/lit.py $(LITFLAGS) $(builddir)
