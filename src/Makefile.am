AM_CXXFLAGS = `llvm-config --cxxflags` -Werror -Wall

noinst_LTLIBRARIES = libsat.la
lib_LTLIBRARIES    = libintck.la libcmpck.la libantiopt.la
bin_PROGRAMS       = intglobal
EXTRA_DIST         = intck cmpck

all-local: libintck.la libcmpck.la
	@ln -f $(builddir)/.libs/libintck.so $(top_builddir)/lib/libintck.so
	@ln -f $(builddir)/.libs/libcmpck.so $(top_builddir)/lib/libcmpck.so
	@ln -f $(builddir)/.libs/libantiopt.so $(top_builddir)/lib/libantiopt.so

libsat_la_CPPFLAGS = -I$(top_builddir)/lib
libsat_la_SOURCES  = ValueGen.cc PathGen.cc Diagnostic.cc
if Z3
libsat_la_SOURCES += SMTZ3.cc
else
libsat_la_SOURCES += SMTBoolector.cc
endif
libsat_la_LIBADD   = $(SMT_LIBADD)
libsat_la_LDFLAGS  = -L`gcc -print-file-name=` -L$(top_builddir)/lib

libintck_la_SOURCES = IntRewrite.cc IntLibcalls.cc IntSat.cc \
	OverflowIdiom.cc OverflowSimplify.cc \
	LoadElim.cc Annotation.cc
libintck_la_LIBADD  = libsat.la
libintck_la_LDFLAGS = -module

libcmpck_la_SOURCES = CmpTautology.cc CmpOverflow.cc
libcmpck_la_LIBADD  = libsat.la
libcmpck_la_LDFLAGS = -module

libantiopt_la_SOURCES = UBBugOn.cc AntiDCE.cc
libantiopt_la_LIBADD  = libsat.la
libantiopt_la_LDFLAGS = -module

intglobal_LDFLAGS = `llvm-config --ldflags` -lLLVM-`llvm-config --version`
intglobal_SOURCES = IntGlobal.cc CallGraph.cc
