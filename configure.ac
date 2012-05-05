#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([ksat],[0.1],[int@pdos.csail.mit.edu])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Checks for programs.
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_LN_S
AC_PROG_MKDIR_P
AC_PROG_SED
AC_CHECK_PROG([CURL],[wget],[wget -O -],[curl])

LT_INIT([disable-static pic-only])
AC_PROG_LIBTOOL

# Checks for OS.
AM_CONDITIONAL(LINUX,false)
AM_CONDITIONAL(DARWIN,false)
case $build_os in
  darwin*)
	AM_CONDITIONAL(DARWIN,true)
	;;
  linux*)
	AM_CONDITIONAL(LINUX,true)
	;;
  *)
	AC_MSG_ERROR([platform not supported])
	;;
esac

# Checks for SMT.
AC_MSG_CHECKING([for SMT solver])
AC_ARG_ENABLE([z3],
	AS_HELP_STRING([--enable-z3],[Use Z3 as SMT solver]))
if test "x$enable_z3" = "xyes"; then
	SMT_LIB=libz3.a
	SMT_LIBADD="-lz3 -lgomp -lgmp"
	AC_DEFINE([HAVE_Z3],[1],[Use Z3 as SMT solver])
	AC_MSG_RESULT([Z3])
else
	SMT_LIB=libboolector.a
	SMT_LIBADD=-lboolector
	AC_DEFINE([HAVE_BOOLECTOR],[1],[Use Boolector as SMT solver])
	AC_MSG_RESULT([Boolector])
	AC_SEARCH_LIBS([zlibVersion],[z],,[AC_MSG_FAILURE([zlib not found])])
fi
AC_SUBST([SMT_LIB])
AC_SUBST([SMT_LIBADD])

# Checks for libraries.
if test "$SMT_LIB" = "libz3.a"; then
	AC_CHECK_LIB([gmp],[__gmpz_init],,
		[AC_MSG_ERROR([libgmp not found, required by Z3])])
fi

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([
	Makefile
	lib/Makefile
	src/Makefile
])
AC_OUTPUT
