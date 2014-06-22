noinst_LTLIBRARIES = libsat.la

libsat_la_CXXFLAGS = `llvm-config --cxxflags` -Werror -Wall -std=c++11 -fno-strict-aliasing

libsat_la_CPPFLAGS = -I$(top_builddir)/lib
libsat_la_SOURCES  = ValueGen.cc PathGen.cc Diagnostic.cc SMTSolver.cc
libsat_la_SOURCES += PHIRange.cc LoopPrepare.cc ElimAssert.cc
libsat_la_SOURCES += BugOn.cc BugOnInt.cc BugOnNull.cc BugOnGep.cc
libsat_la_SOURCES += BugOnAlias.cc BugOnFree.cc BugOnBounds.cc BugOnUndef.cc
libsat_la_SOURCES += BugOnLoop.cc BugOnAssert.cc
libsat_la_SOURCES += BugOnLibc.cc BugOnLinux.cc
libsat_la_SOURCES += ValueGen.h PathGen.h Diagnostic.h SMTSolver.h BugOn.h
libsat_la_SOURCES += GlobalTimeout.cc

if HAVE_SMTLIB
libsat_la_SOURCES += SMTLIB.cc
libsat_la_LIBADD   = -lutil
endif
if HAVE_BOOLECTOR
libsat_la_SOURCES += SMTBoolector.cc
libsat_la_LIBADD   = -lboolector -llgl
endif
#libsat_la_SOURCES += SMTSonolar.cc
#libsat_la_LIBADD   = -lsonolar
#libsat_la_SOURCES += SMTZ3.cc
#libsat_la_LIBADD   = -lz3 -lgomp

libsat_la_LDFLAGS  = -L$(top_builddir)/lib
