EXTRA_DIST = lit.cfg.in intsat.in

check-local: lit.cfg intsat
	@`llvm-config --src-root`/utils/lit/lit.py $(LITFLAGS) $(builddir)
