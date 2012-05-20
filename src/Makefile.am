AM_CXXFLAGS = `llvm-config --cxxflags` -Werror -Wall -Wno-unknown-pragmas

noinst_LTLIBRARIES = libsat.la
lib_LTLIBRARIES    = libint.la libcmp.la

libsat_la_CPPFLAGS = -I$(top_builddir)/lib
libsat_la_SOURCES  = ValueGen.cc PathGen.cc Diagnostic.cc FileCache.cc
if BOOLECTOR
libsat_la_SOURCES += SMTBoolector.cc
endif
if Z3
libsat_la_SOURCES += SMTZ3.cc
endif
libsat_la_LIBADD   = $(SMT_LIBADD)
libsat_la_LDFLAGS  = -L`gcc -print-file-name=` -L$(top_builddir)/lib

libint_la_SOURCES = IntRewrite.cc IntSat.cc Overflow.cc OverflowSimplify.cc Annotation.cc
libint_la_LIBADD  = libsat.la
libint_la_LDFLAGS = -module

libcmp_la_SOURCES = CmpRange.cc CmpSat.cc
libcmp_la_LIBADD  = libsat.la
libcmp_la_LDFLAGS = -module
