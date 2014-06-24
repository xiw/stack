bin_SCRIPTS=\
  ncpu\
  optck\
  poptck

lib_LTLIBRARIES = liboptck.la

EXTRA_DIST = optck.in poptck ncpu

MOSTLYCLEANFILES=optck
optck: Makefile.am optck.in
	sed \
	  -e 's|[@]libdir[@]|$(libdir)|g'\
	  -e 's|[@]bindir[@]|$(bindir)|g'\
	  $(srcdir)/optck.in > $@
	chmod u+x $@

liboptck_la_CPPFLAGS = -I$(srcdir)/../
liboptck_la_CXXFLAGS = `llvm-config --cxxflags` -Werror -Wall -std=c++11 -fno-strict-aliasing
liboptck_la_LIBADD = ../sat/libsat.la
liboptck_la_LDFLAGS = -module

liboptck_la_SOURCES = AntiFunctionPass.cc AntiDCE.cc AntiAlgebra.cc AntiSimplify.cc
liboptck_la_SOURCES += InlineOnly.cc SimplifyDelete.cc IgnoreLoopInitial.cc LoadElim.cc
liboptck_la_SOURCES += AntiFunctionPass.h
