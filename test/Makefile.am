EXTRA_DIST = lit.cfg.in cc1 diagdiff gcc g++ stack-build.in

bin_SCRIPTS=stack-build
pkglibexec_SCRIPTS=\
  gcc\
  cc1\
  arm-linux-gnueabi-gcc\
  arm-linux-gnueabi-g++

stack-build: Makefile.am stack-build.in
	sed \
	  -e 's|[@]pkglibexecdir[@]|$(pkglibexecdir)|g'\
	  $(srcdir)/stack-build.in > $@

install-exec-hook:
	$(LN_S) $(DESTDIR)$(pkglibexecdir)/gcc$(EXEEXT)\
	  $(DESTDIR)$(pkglibexecdir)/cc$(EXEEXT)
	$(LN_S) $(DESTDIR)$(pkglibexecdir)/gcc$(EXEEXT)\
	  $(DESTDIR)$(pkglibexecdir)/g++$(EXEEXT)
	$(LN_S) $(DESTDIR)$(pkglibexecdir)/gcc$(EXEEXT)\
	  $(DESTDIR)$(pkglibexecdir)/clang$(EXEEXT)
	$(LN_S) $(DESTDIR)$(pkglibexecdir)/gcc$(EXEEXT)\
	  $(DESTDIR)$(pkglibexecdir)/clang++$(EXEEXT)

uninstall-hook:
	- (cd $(DESTDIR)$(pkglibexecdir) && rm -f cc$(EXEEXT))
	- (cd $(DESTDIR)$(pkglibexecdir) && rm -f g++$(EXEEXT))
	- (cd $(DESTDIR)$(pkglibexecdir) && rm -f clang$(EXEEXT))
	- (cd $(DESTDIR)$(pkglibexecdir) && rm -f clang++$(EXEEXT))

LITFLAGS ?= -v
LITTESTS ?= $(builddir)
check-local: lit.cfg
	@$(top_srcdir)/lib/lit/lit.py $(LITFLAGS) $(LITTESTS)
