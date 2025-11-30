dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.71])
AC_INIT([xorcyst],[1.6.1])

AC_CONFIG_SRCDIR([astnode.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign silent-rules])
AM_SILENT_RULES([yes])

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

dnl Optional parser/lexer tools for maintainers
dnl If not found, we just won't regenerate parser.c / scanner.c
AC_CHECK_PROGS([YACC], [bison byacc yacc], [:])
AC_CHECK_PROGS([LEX],  [flex lex],          [:])
AC_SUBST([LEXLIB], [])

if test "x$YACC" = "x:"; then
  AC_MSG_WARN([No yacc/bison/byacc found; parser.c will not be regenerated from parser.y])
fi

if test "x$LEX" = "x:"; then
  AC_MSG_WARN([No lex/flex found; scanner.c will not be regenerated from scanner.l])
fi

dnl Checks for libraries.

dnl Checks for header files.
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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
