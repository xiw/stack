AM_CXXFLAGS = `llvm-config --cxxflags` -Werror -Wall -Wno-unknown-pragmas

noinst_LTLIBRARIES = libsat.la
lib_LTLIBRARIES    = libint.la libcmp.la
bin_PROGRAMS       = intglobal

all-local: libint.la libcmp.la
	@ln -f $(builddir)/.libs/libint.so $(top_builddir)/lib/libint.so
	@ln -f $(builddir)/.libs/libcmp.so $(top_builddir)/lib/libcmp.so

libsat_la_CPPFLAGS = -I$(top_builddir)/lib
libsat_la_SOURCES  = ValueGen.cc PathGen.cc Diagnostic.cc
if BOOLECTOR
libsat_la_SOURCES += SMTBoolector.cc
endif
if Z3
libsat_la_SOURCES += SMTZ3.cc
endif
libsat_la_LIBADD   = $(SMT_LIBADD)
libsat_la_LDFLAGS  = -L`gcc -print-file-name=` -L$(top_builddir)/lib

libint_la_SOURCES = IntRewrite.cc IntLibcalls.cc IntSat.cc \
	OverflowIdiom.cc OverflowSimplify.cc \
	LoadElim.cc Annotation.cc
libint_la_LIBADD  = libsat.la
libint_la_LDFLAGS = -module

libcmp_la_SOURCES = CmpTautology.cc CmpOverflow.cc
libcmp_la_LIBADD  = libsat.la
libcmp_la_LDFLAGS = -module


intglobal_LDFLAGS = `llvm-config --ldflags` -lLLVM-`llvm-config --version`
intglobal_SOURCES = IntGlobal.cc CallGraph.cc
