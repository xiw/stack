EXTRA_DIST = lit.cfg.in kint-cc1 kint-gcc kint-g++ diagdiff

LITFLAGS ?= -v
LITTESTS ?= $(builddir)
check-local: lit.cfg
	@$(top_srcdir)/lib/lit/lit.py $(LITFLAGS) $(LITTESTS)
