EXTRA_DIST = lit.cfg.in kint-cc1 kint-gcc kint-g++ diagdiff

LITFLAGS ?= -v
LITTESTS ?= $(builddir)
check-local: lit.cfg
	@`llvm-config --src-root`/utils/lit/lit.py $(LITFLAGS) $(LITTESTS)
