EXTRA_DIST = lit.cfg.in kcc kgcc intsat.in cmpsat.in

LITFLAGS ?= -v
check-local: lit.cfg kcc kgcc intsat cmpsat
	@`llvm-config --src-root`/utils/lit/lit.py $(LITFLAGS) $(builddir)
