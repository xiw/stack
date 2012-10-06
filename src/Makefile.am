AM_CXXFLAGS = `llvm-config --cxxflags` -Werror -Wall

noinst_LTLIBRARIES = libsat.la
lib_LTLIBRARIES    = libintck.la libcmpck.la libantiopt.la
bin_PROGRAMS       = intglobal
EXTRA_DIST         = intck cmpck antiopt

all-local: libintck.la libcmpck.la
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libintck.so
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libcmpck.so
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libantiopt.so

libsat_la_CPPFLAGS = -I$(top_builddir)/lib
libsat_la_SOURCES  = ValueGen.cc PathGen.cc Diagnostic.cc SMTSolver.cc
libsat_la_SOURCES += SMTBoolector.cc
libsat_la_LIBADD   = -lboolector -llgl
libsat_la_LDFLAGS  = -L$(top_builddir)/lib

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
