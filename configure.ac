#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([stack],[0.3],[int@pdos.csail.mit.edu])
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
	bin/stackcc/gcc:test/gcc
	bin/stackcc/g++:test/g++
	bin/stackcc/cc:test/gcc
	bin/stackcc/c++:test/g++
	bin/stackcc/cc1:test/cc1
	bin/stackcc/clang:test/clang
	bin/stackcc/clang++:test/clang++
	bin/stackcc/arm-linux-gnueabi-gcc:test/arm-linux-gnueabi-gcc
	bin/stackcc/arm-linux-gnueabi-g++:test/arm-linux-gnueabi-g++
	bin/stack-build:test/stack-build
	bin/ncpu:src/ncpu
	bin/stack:src/stack
	bin/pstack:src/pstack
	bin/optck:src/stack
])
AC_OUTPUT
