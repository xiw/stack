AM_CXXFLAGS = `llvm-config --cxxflags` -Werror -Wall

noinst_LTLIBRARIES = libsat.la
lib_LTLIBRARIES    = libintck.la libcmpck.la libantiopt.la
bin_PROGRAMS       = intglobal
EXTRA_DIST         = intck cmpck antiopt llvm/DataLayout.h llvm/DebugInfo.h llvm/IRBuilder.h

all-local: libintck.la libcmpck.la
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libintck.so
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libcmpck.so
	@cd $(top_builddir)/bin && $(LN_S) -f ../src/intglobal
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libantiopt.so

libsat_la_CPPFLAGS = -I$(top_builddir)/lib
libsat_la_SOURCES  = ValueGen.cc PathGen.cc Diagnostic.cc SMTSolver.cc IdealShift.cc
libsat_la_SOURCES += BugOn.cc BugOnInt.cc BugOnNull.cc BugOnLibc.cc
libsat_la_SOURCES += ValueGen.h PathGen.h Diagnostic.h SMTSolver.h BugOn.h
libsat_la_SOURCES += SMTBoolector.cc
libsat_la_LIBADD   = -lboolector -llgl
libsat_la_LDFLAGS  = -L$(top_builddir)/lib

libintck_la_SOURCES = IntRewrite.cc IntLibcalls.cc IntSat.cc \
	OverflowIdiom.cc OverflowSimplify.cc \
	LoadElim.cc
libintck_la_LIBADD  = libsat.la
libintck_la_LDFLAGS = -module

libcmpck_la_SOURCES = CmpTautology.cc CmpOverflow.cc CmpSat.cc
libcmpck_la_LIBADD  = libsat.la
libcmpck_la_LDFLAGS = -module

libantiopt_la_SOURCES = AntiDCE.cc
libantiopt_la_LIBADD  = libsat.la
libantiopt_la_LDFLAGS = -module

intglobal_LDFLAGS = `llvm-config --ldflags` -lLLVM-`llvm-config --version`
intglobal_SOURCES = IntGlobal.cc Annotation.cc CallGraph.cc Taint.cc Range.cc \
	IntGlobal.h Annotation.h CRange.h
