AM_CXXFLAGS = `llvm-config --cxxflags` -Werror -Wall -Wno-unknown-pragmas

lib_LIBRARIES = libsmt.a
#lib_LTLIBRARIES = libkint.la

libsmt_a_CPPFLAGS = -I$(top_builddir)/lib
libsmt_a_SOURCES  = ValueGen.cc PathGen.cc
if BOOLECTOR
libsmt_a_SOURCES += SMTBoolector.cc
endif
if Z3
libsmt_a_SOURCES += SMTZ3.cc
endif

