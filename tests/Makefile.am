EXTRA_DIST = lit.cfg.in kcc kgcc intsat.in

check-local: lit.cfg kcc kgcc intsat
	@`llvm-config --src-root`/utils/lit/lit.py $(LITFLAGS) $(builddir)
