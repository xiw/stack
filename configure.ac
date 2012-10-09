#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([kint],[0.3],[int@pdos.csail.mit.edu])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Checks for programs.
AM_PROG_CC_C_O
AC_PROG_CXX
m4_ifdef([AM_PROG_AR],[AM_PROG_AR])
AC_PROG_LN_S
AC_PROG_MKDIR_P
AC_PROG_SED

LT_INIT([disable-static pic-only])
AC_PROG_LIBTOOL

AC_CONFIG_FILES([
	Makefile
	lib/Makefile
	src/Makefile
	test/Makefile
	test/lit.cfg
])
AC_CONFIG_LINKS([
	bin/kint-cc1:test/kint-cc1
	bin/kint-gcc:test/kint-gcc
	bin/kint-g++:test/kint-g++
	bin/kint-build:test/kint-build
	bin/ncpu:src/ncpu
	bin/intck:src/intck
	bin/cmpck:src/cmpck
	bin/pintck:src/pintck
])
AC_OUTPUT
