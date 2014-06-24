EXTRA_DIST=\
  lit.cfg.in\
  cc1.in\
  diagdiff\
  stack-build.in\
  gcc arm-linux-gnueabi-gcc arm-linux-gnueabi-g++


bin_SCRIPTS=stack-build
pkglibexec_SCRIPTS=\
  gcc\
  cc1\
  arm-linux-gnueabi-gcc\
  arm-linux-gnueabi-g++

MOSTLYCLEANFILES=stack-build cc1
stack-build: Makefile.am stack-build.in
	sed \
	  -e 's|[@]pkglibexecdir[@]|$(pkglibexecdir)|g'\
	  $(srcdir)/stack-build.in > $@

cc1: Makefile.am cc1.in
	sed \
	  -e 's|[@]libdir[@]|$(libdir)|g'\
	  $(srcdir)/cc1.in > $@
	chmod u+x $@

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
LITTESTS ?= $(builddir)/opt
check-local: lit.cfg
	$(top_srcdir)/lib/lit/lit.py $(LITFLAGS) $(LITTESTS)
