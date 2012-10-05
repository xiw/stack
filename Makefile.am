SUBDIRS = lib src test

EXTRA_DIST = COPYING INSTALL

intck_bin_distdir = intck-$(PACKAGE_VERSION)-bin

intck-bin:
	rm -rf $(intck_bin_distdir)
	$(MKDIR_P) $(intck_bin_distdir)/bin
	cp bin/* $(intck_bin_distdir)/bin/
	$(MKDIR_P) $(intck_bin_distdir)/lib
	cp lib/*.so $(intck_bin_distdir)/lib/
	cp `which opt` $(intck_bin_distdir)/bin/
	cp `llvm-config --libdir`/libLLVM-`llvm-config --version`* $(intck_bin_distdir)/lib/
	tar chvjf $(intck_bin_distdir).tar.bz2 $(intck_bin_distdir)
	rm -rf $(intck_bin_distdir)
