dnl Process this file with autoconf to produce a configure script.
AC_INIT([xorcyst], [1.5.2])

AC_CONFIG_SRCDIR([astnode.c])
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE

AC_TYPE_SIZE_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_C_CONST

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_YACC
AM_PROG_LEX
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(stddef.h)

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.
AC_FUNC_VPRINTF
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([getcwd])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS(strtol)
AC_CHECK_FUNCS(strchr)
AC_CHECK_FUNCS(strrchr)

AC_OUTPUT(Makefile)
