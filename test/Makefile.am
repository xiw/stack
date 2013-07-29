EXTRA_DIST = lit.cfg.in cc1 diagdiff gcc g++

LITFLAGS ?= -v
LITTESTS ?= $(builddir)
check-local: lit.cfg
	@$(top_srcdir)/lib/lit/lit.py $(LITFLAGS) $(LITTESTS)
