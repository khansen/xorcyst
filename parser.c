/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "parser.y"

/*
 * $Id: parser.y,v 1.14 2007/11/11 22:35:51 khansen Exp $
 * $Log: parser.y,v $
 * Revision 1.14  2007/11/11 22:35:51  khansen
 * compile on mac
 *
 * Revision 1.13  2007/08/19 11:19:47  khansen
 * --case-insensitive option
 *
 * Revision 1.12  2007/08/12 18:59:00  khansen
 * ability to generate pure 6502 binary
 *
 * Revision 1.11  2007/08/11 01:25:50  khansen
 * includepaths support (-I option)
 *
 * Revision 1.10  2007/07/22 13:34:38  khansen
 * convert tabs to whitespaces
 *
 * Revision 1.9  2005/01/05 02:28:40  kenth
 * anonymous union parsing
 *
 * Revision 1.8  2004/12/29 21:45:26  kenth
 * xorcyst 1.4.2
 * static indexing
 *
 * Revision 1.7  2004/12/19 19:59:14  kenth
 * xorcyst 1.4.0
 *
 * Revision 1.6  2004/12/16 13:22:27  kenth
 * xorcyst 1.3.5
 *
 * Revision 1.5  2004/12/14 01:50:42  kenth
 * xorcyst 1.3.0
 *
 * Revision 1.4  2004/12/11 02:12:41  kenth
 * xorcyst 1.2.0
 *
 * Revision 1.3  2004/12/09 11:17:15  kenth
 * added: warning_statement, error_statement
 *
 * Revision 1.2  2004/12/06 05:06:21  kenth
 * xorcyst 1.1.0
 *
 * Revision 1.1  2004/06/30 07:57:03  kenth
 * Initial revision
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "symtab.h"
#include "loc.h"
//#define YYDEBUG 1
int yyparse(void);
void yyerror(const char *);   /* In lexer */
int yylex(void);    /* In lexer */
int yypushandrestart(const char *, int);   /* In lexer */
void __yy_memcpy(char *, char *, int);
extern char *yytext;    /* In lexer */
extern YYLTYPE yylloc;  /* In lexer */
char *scan_include(int); /* In lexer */
extern astnode *root_node;  /* Root of the generated parse tree */
void handle_incsrc(astnode *);  /* See below */
void handle_incbin(astnode *);  /* See below */

#line 139 "parser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_INTEGER_LITERAL = 3,            /* INTEGER_LITERAL  */
  YYSYMBOL_STRING_LITERAL = 4,             /* STRING_LITERAL  */
  YYSYMBOL_FILE_PATH = 5,                  /* FILE_PATH  */
  YYSYMBOL_IDENTIFIER = 6,                 /* IDENTIFIER  */
  YYSYMBOL_LOCAL_ID = 7,                   /* LOCAL_ID  */
  YYSYMBOL_FORWARD_BRANCH = 8,             /* FORWARD_BRANCH  */
  YYSYMBOL_BACKWARD_BRANCH = 9,            /* BACKWARD_BRANCH  */
  YYSYMBOL_LABEL = 10,                     /* LABEL  */
  YYSYMBOL_LOCAL_LABEL = 11,               /* LOCAL_LABEL  */
  YYSYMBOL_MNEMONIC = 12,                  /* MNEMONIC  */
  YYSYMBOL__LABEL_ = 13,                   /* _LABEL_  */
  YYSYMBOL_BYTE = 14,                      /* BYTE  */
  YYSYMBOL_CHAR = 15,                      /* CHAR  */
  YYSYMBOL_WORD = 16,                      /* WORD  */
  YYSYMBOL_DWORD = 17,                     /* DWORD  */
  YYSYMBOL_DSB = 18,                       /* DSB  */
  YYSYMBOL_DSW = 19,                       /* DSW  */
  YYSYMBOL_DSD = 20,                       /* DSD  */
  YYSYMBOL_DATASEG = 21,                   /* DATASEG  */
  YYSYMBOL_CODESEG = 22,                   /* CODESEG  */
  YYSYMBOL_IF = 23,                        /* IF  */
  YYSYMBOL_IFDEF = 24,                     /* IFDEF  */
  YYSYMBOL_IFNDEF = 25,                    /* IFNDEF  */
  YYSYMBOL_ELSE = 26,                      /* ELSE  */
  YYSYMBOL_ELIF = 27,                      /* ELIF  */
  YYSYMBOL_ENDIF = 28,                     /* ENDIF  */
  YYSYMBOL_INCSRC = 29,                    /* INCSRC  */
  YYSYMBOL_INCBIN = 30,                    /* INCBIN  */
  YYSYMBOL_MACRO = 31,                     /* MACRO  */
  YYSYMBOL_REPT = 32,                      /* REPT  */
  YYSYMBOL_WHILE = 33,                     /* WHILE  */
  YYSYMBOL_ENDM = 34,                      /* ENDM  */
  YYSYMBOL_ALIGN = 35,                     /* ALIGN  */
  YYSYMBOL_EQU = 36,                       /* EQU  */
  YYSYMBOL_UNDEF = 37,                     /* UNDEF  */
  YYSYMBOL_DEFINE = 38,                    /* DEFINE  */
  YYSYMBOL_END = 39,                       /* END  */
  YYSYMBOL_PUBLIC = 40,                    /* PUBLIC  */
  YYSYMBOL_EXTRN = 41,                     /* EXTRN  */
  YYSYMBOL_CHARMAP = 42,                   /* CHARMAP  */
  YYSYMBOL_STRUC = 43,                     /* STRUC  */
  YYSYMBOL_UNION = 44,                     /* UNION  */
  YYSYMBOL_ENDS = 45,                      /* ENDS  */
  YYSYMBOL_RECORD = 46,                    /* RECORD  */
  YYSYMBOL_ENUM = 47,                      /* ENUM  */
  YYSYMBOL_ENDE = 48,                      /* ENDE  */
  YYSYMBOL_PROC = 49,                      /* PROC  */
  YYSYMBOL_ENDP = 50,                      /* ENDP  */
  YYSYMBOL_SIZEOF = 51,                    /* SIZEOF  */
  YYSYMBOL_MASK = 52,                      /* MASK  */
  YYSYMBOL_TAG = 53,                       /* TAG  */
  YYSYMBOL_MESSAGE = 54,                   /* MESSAGE  */
  YYSYMBOL_WARNING = 55,                   /* WARNING  */
  YYSYMBOL_ERROR = 56,                     /* ERROR  */
  YYSYMBOL_ZEROPAGE = 57,                  /* ZEROPAGE  */
  YYSYMBOL_ORG = 58,                       /* ORG  */
  YYSYMBOL_59_n_ = 59,                     /* '\n'  */
  YYSYMBOL_60_ = 60,                       /* ':'  */
  YYSYMBOL_61_ = 61,                       /* '@'  */
  YYSYMBOL_SCOPE_OP = 62,                  /* SCOPE_OP  */
  YYSYMBOL_63_A_ = 63,                     /* 'A'  */
  YYSYMBOL_64_X_ = 64,                     /* 'X'  */
  YYSYMBOL_65_Y_ = 65,                     /* 'Y'  */
  YYSYMBOL_66_ = 66,                       /* '='  */
  YYSYMBOL_67_ = 67,                       /* '$'  */
  YYSYMBOL_68_ = 68,                       /* '{'  */
  YYSYMBOL_69_ = 69,                       /* '}'  */
  YYSYMBOL_70_ = 70,                       /* '['  */
  YYSYMBOL_71_ = 71,                       /* ']'  */
  YYSYMBOL_72_ = 72,                       /* ','  */
  YYSYMBOL_73_ = 73,                       /* '.'  */
  YYSYMBOL_74_ = 74,                       /* '#'  */
  YYSYMBOL_LO_OP = 75,                     /* LO_OP  */
  YYSYMBOL_HI_OP = 76,                     /* HI_OP  */
  YYSYMBOL_77_ = 77,                       /* '|'  */
  YYSYMBOL_78_ = 78,                       /* '^'  */
  YYSYMBOL_79_ = 79,                       /* '&'  */
  YYSYMBOL_EQ_OP = 80,                     /* EQ_OP  */
  YYSYMBOL_NE_OP = 81,                     /* NE_OP  */
  YYSYMBOL_LE_OP = 82,                     /* LE_OP  */
  YYSYMBOL_GE_OP = 83,                     /* GE_OP  */
  YYSYMBOL_84_ = 84,                       /* '>'  */
  YYSYMBOL_85_ = 85,                       /* '<'  */
  YYSYMBOL_SHL_OP = 86,                    /* SHL_OP  */
  YYSYMBOL_SHR_OP = 87,                    /* SHR_OP  */
  YYSYMBOL_88_ = 88,                       /* '+'  */
  YYSYMBOL_89_ = 89,                       /* '-'  */
  YYSYMBOL_UMINUS = 90,                    /* UMINUS  */
  YYSYMBOL_91_ = 91,                       /* '*'  */
  YYSYMBOL_92_ = 92,                       /* '/'  */
  YYSYMBOL_93_ = 93,                       /* '%'  */
  YYSYMBOL_94_ = 94,                       /* '!'  */
  YYSYMBOL_95_ = 95,                       /* '~'  */
  YYSYMBOL_96_ = 96,                       /* '('  */
  YYSYMBOL_97_ = 97,                       /* ')'  */
  YYSYMBOL_YYACCEPT = 98,                  /* $accept  */
  YYSYMBOL_assembly_unit = 99,             /* assembly_unit  */
  YYSYMBOL_end_opt = 100,                  /* end_opt  */
  YYSYMBOL_statement_list = 101,           /* statement_list  */
  YYSYMBOL_statement_list_opt = 102,       /* statement_list_opt  */
  YYSYMBOL_labelable_statement = 103,      /* labelable_statement  */
  YYSYMBOL_statement = 104,                /* statement  */
  YYSYMBOL_org_statement = 105,            /* org_statement  */
  YYSYMBOL_align_statement = 106,          /* align_statement  */
  YYSYMBOL_warning_statement = 107,        /* warning_statement  */
  YYSYMBOL_error_statement = 108,          /* error_statement  */
  YYSYMBOL_message_statement = 109,        /* message_statement  */
  YYSYMBOL_label_statement = 110,          /* label_statement  */
  YYSYMBOL_label_addr_part_opt = 111,      /* label_addr_part_opt  */
  YYSYMBOL_label_type_part_opt = 112,      /* label_type_part_opt  */
  YYSYMBOL_while_statement = 113,          /* while_statement  */
  YYSYMBOL_rept_statement = 114,           /* rept_statement  */
  YYSYMBOL_proc_statement = 115,           /* proc_statement  */
  YYSYMBOL_struc_decl_statement = 116,     /* struc_decl_statement  */
  YYSYMBOL_union_decl_statement = 117,     /* union_decl_statement  */
  YYSYMBOL_enum_decl_statement = 118,      /* enum_decl_statement  */
  YYSYMBOL_enum_item_list = 119,           /* enum_item_list  */
  YYSYMBOL_enum_item = 120,                /* enum_item  */
  YYSYMBOL_record_decl_statement = 121,    /* record_decl_statement  */
  YYSYMBOL_record_field_list = 122,        /* record_field_list  */
  YYSYMBOL_record_field = 123,             /* record_field  */
  YYSYMBOL_charmap_statement = 124,        /* charmap_statement  */
  YYSYMBOL_dataseg_statement = 125,        /* dataseg_statement  */
  YYSYMBOL_codeseg_statement = 126,        /* codeseg_statement  */
  YYSYMBOL_null_statement = 127,           /* null_statement  */
  YYSYMBOL_label_decl = 128,               /* label_decl  */
  YYSYMBOL_line_tail = 129,                /* line_tail  */
  YYSYMBOL_newline = 130,                  /* newline  */
  YYSYMBOL_instruction_statement = 131,    /* instruction_statement  */
  YYSYMBOL_instruction = 132,              /* instruction  */
  YYSYMBOL_expression = 133,               /* expression  */
  YYSYMBOL_indexed_identifier = 134,       /* indexed_identifier  */
  YYSYMBOL_extended_expression = 135,      /* extended_expression  */
  YYSYMBOL_sizeof_arg = 136,               /* sizeof_arg  */
  YYSYMBOL_expression_opt = 137,           /* expression_opt  */
  YYSYMBOL_scope_access = 138,             /* scope_access  */
  YYSYMBOL_struc_access = 139,             /* struc_access  */
  YYSYMBOL_struc_initializer = 140,        /* struc_initializer  */
  YYSYMBOL_field_initializer_list_opt = 141, /* field_initializer_list_opt  */
  YYSYMBOL_field_initializer_list = 142,   /* field_initializer_list  */
  YYSYMBOL_field_initializer = 143,        /* field_initializer  */
  YYSYMBOL_local_id = 144,                 /* local_id  */
  YYSYMBOL_arithmetic_expression = 145,    /* arithmetic_expression  */
  YYSYMBOL_comparison_expression = 146,    /* comparison_expression  */
  YYSYMBOL_label = 147,                    /* label  */
  YYSYMBOL_identifier = 148,               /* identifier  */
  YYSYMBOL_identifier_opt = 149,           /* identifier_opt  */
  YYSYMBOL_literal = 150,                  /* literal  */
  YYSYMBOL_if_statement = 151,             /* if_statement  */
  YYSYMBOL_elif_statement_list_opt = 152,  /* elif_statement_list_opt  */
  YYSYMBOL_elif_statement_list = 153,      /* elif_statement_list  */
  YYSYMBOL_elif_statement = 154,           /* elif_statement  */
  YYSYMBOL_else_part_opt = 155,            /* else_part_opt  */
  YYSYMBOL_ifdef_statement = 156,          /* ifdef_statement  */
  YYSYMBOL_ifndef_statement = 157,         /* ifndef_statement  */
  YYSYMBOL_data_statement = 158,           /* data_statement  */
  YYSYMBOL_named_data_statement = 159,     /* named_data_statement  */
  YYSYMBOL_unnamed_data_statement = 160,   /* unnamed_data_statement  */
  YYSYMBOL_datatype = 161,                 /* datatype  */
  YYSYMBOL_expression_list = 162,          /* expression_list  */
  YYSYMBOL_incsrc_statement = 163,         /* incsrc_statement  */
  YYSYMBOL_incbin_statement = 164,         /* incbin_statement  */
  YYSYMBOL_file_specifier = 165,           /* file_specifier  */
  YYSYMBOL_macro_decl_statement = 166,     /* macro_decl_statement  */
  YYSYMBOL_param_list_opt = 167,           /* param_list_opt  */
  YYSYMBOL_macro_statement = 168,          /* macro_statement  */
  YYSYMBOL_arg_list_opt = 169,             /* arg_list_opt  */
  YYSYMBOL_identifier_list = 170,          /* identifier_list  */
  YYSYMBOL_equ_statement = 171,            /* equ_statement  */
  YYSYMBOL_undef_statement = 172,          /* undef_statement  */
  YYSYMBOL_assign_statement = 173,         /* assign_statement  */
  YYSYMBOL_define_statement = 174,         /* define_statement  */
  YYSYMBOL_public_statement = 175,         /* public_statement  */
  YYSYMBOL_extrn_statement = 176,          /* extrn_statement  */
  YYSYMBOL_from_part_opt = 177,            /* from_part_opt  */
  YYSYMBOL_symbol_type = 178,              /* symbol_type  */
  YYSYMBOL_storage_statement = 179,        /* storage_statement  */
  YYSYMBOL_named_storage_statement = 180,  /* named_storage_statement  */
  YYSYMBOL_unnamed_storage_statement = 181, /* unnamed_storage_statement  */
  YYSYMBOL_storage = 182                   /* storage  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  163
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1705

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  98
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  85
/* YYNRULES -- Number of rules.  */
#define YYNRULES  218
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  385

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   323


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      59,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    94,     2,    74,    67,    93,    79,     2,
      96,    97,    91,    88,    72,    89,    73,    92,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    60,     2,
      85,    66,    84,     2,    61,    63,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    64,    65,
       2,    70,     2,    71,    78,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    68,    77,    69,    95,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    62,    75,    76,    80,    81,    82,
      83,    86,    87,    90
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   123,   123,   127,   128,   132,   133,   140,   141,   145,
     146,   150,   151,   152,   153,   154,   155,   156,   157,   158,
     159,   160,   161,   162,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   187,   191,   195,   199,   203,
     207,   211,   212,   216,   217,   221,   225,   229,   233,   237,
     241,   245,   246,   250,   251,   255,   259,   260,   264,   268,
     272,   273,   276,   280,   284,   288,   289,   293,   297,   301,
     302,   303,   304,   305,   306,   307,   308,   309,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   331,   332,   333,   337,   338,   342,   343,
     347,   348,   352,   356,   357,   361,   365,   366,   370,   371,
     375,   379,   383,   384,   385,   386,   387,   388,   389,   390,
     391,   392,   393,   394,   395,   396,   397,   398,   402,   403,
     404,   405,   406,   407,   411,   412,   413,   414,   415,   416,
     420,   424,   425,   429,   430,   434,   438,   439,   443,   444,
     448,   452,   453,   457,   461,   465,   466,   470,   471,   472,
     473,   474,   478,   479,   480,   484,   485,   486,   487,   488,
     489,   493,   494,   498,   502,   506,   507,   511,   515,   516,
     520,   524,   525,   529,   530,   534,   538,   542,   546,   547,
     551,   555,   559,   560,   564,   565,   566,   567,   571,   572,
     576,   577,   578,   579,   580,   584,   588,   589,   590
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "INTEGER_LITERAL",
  "STRING_LITERAL", "FILE_PATH", "IDENTIFIER", "LOCAL_ID",
  "FORWARD_BRANCH", "BACKWARD_BRANCH", "LABEL", "LOCAL_LABEL", "MNEMONIC",
  "_LABEL_", "BYTE", "CHAR", "WORD", "DWORD", "DSB", "DSW", "DSD",
  "DATASEG", "CODESEG", "IF", "IFDEF", "IFNDEF", "ELSE", "ELIF", "ENDIF",
  "INCSRC", "INCBIN", "MACRO", "REPT", "WHILE", "ENDM", "ALIGN", "EQU",
  "UNDEF", "DEFINE", "END", "PUBLIC", "EXTRN", "CHARMAP", "STRUC", "UNION",
  "ENDS", "RECORD", "ENUM", "ENDE", "PROC", "ENDP", "SIZEOF", "MASK",
  "TAG", "MESSAGE", "WARNING", "ERROR", "ZEROPAGE", "ORG", "'\\n'", "':'",
  "'@'", "SCOPE_OP", "'A'", "'X'", "'Y'", "'='", "'$'", "'{'", "'}'",
  "'['", "']'", "','", "'.'", "'#'", "LO_OP", "HI_OP", "'|'", "'^'", "'&'",
  "EQ_OP", "NE_OP", "LE_OP", "GE_OP", "'>'", "'<'", "SHL_OP", "SHR_OP",
  "'+'", "'-'", "UMINUS", "'*'", "'/'", "'%'", "'!'", "'~'", "'('", "')'",
  "$accept", "assembly_unit", "end_opt", "statement_list",
  "statement_list_opt", "labelable_statement", "statement",
  "org_statement", "align_statement", "warning_statement",
  "error_statement", "message_statement", "label_statement",
  "label_addr_part_opt", "label_type_part_opt", "while_statement",
  "rept_statement", "proc_statement", "struc_decl_statement",
  "union_decl_statement", "enum_decl_statement", "enum_item_list",
  "enum_item", "record_decl_statement", "record_field_list",
  "record_field", "charmap_statement", "dataseg_statement",
  "codeseg_statement", "null_statement", "label_decl", "line_tail",
  "newline", "instruction_statement", "instruction", "expression",
  "indexed_identifier", "extended_expression", "sizeof_arg",
  "expression_opt", "scope_access", "struc_access", "struc_initializer",
  "field_initializer_list_opt", "field_initializer_list",
  "field_initializer", "local_id", "arithmetic_expression",
  "comparison_expression", "label", "identifier", "identifier_opt",
  "literal", "if_statement", "elif_statement_list_opt",
  "elif_statement_list", "elif_statement", "else_part_opt",
  "ifdef_statement", "ifndef_statement", "data_statement",
  "named_data_statement", "unnamed_data_statement", "datatype",
  "expression_list", "incsrc_statement", "incbin_statement",
  "file_specifier", "macro_decl_statement", "param_list_opt",
  "macro_statement", "arg_list_opt", "identifier_list", "equ_statement",
  "undef_statement", "assign_statement", "define_statement",
  "public_statement", "extrn_statement", "from_part_opt", "symbol_type",
  "storage_statement", "named_storage_statement",
  "unnamed_storage_statement", "storage", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-290)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-9)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    1457,   -33,  -290,  -290,  -290,  -290,  -290,   480,    14,  -290,
    -290,  -290,  -290,  -290,  -290,  -290,   -15,   -33,   694,    14,
      14,     3,     3,    14,   694,   694,    14,    14,    14,     4,
      14,     3,    14,    14,    14,    14,    14,    14,   694,   694,
     694,    29,   694,  -290,  -290,    14,  -290,  -290,    36,   785,
    -290,  -290,  -290,  -290,  -290,  -290,  -290,  -290,  -290,  -290,
    -290,  -290,  -290,  -290,  -290,  -290,  -290,  -290,  -290,  1519,
    -290,  -290,  -290,   -33,  -290,   440,  -290,  -290,  -290,  -290,
     -33,   -33,   587,  -290,  -290,  -290,  -290,  -290,  -290,  -290,
    -290,  -290,  -290,  -290,  -290,  -290,   694,  -290,  -290,  -290,
    -290,  -290,  -290,    17,    14,  -290,  -290,   694,   694,    14,
     694,   694,  -290,   171,   694,   694,   694,  1553,   -32,  -290,
    -290,  -290,  -290,  -290,    -2,  -290,    -4,   -33,  -290,  -290,
     525,   -33,   -33,  -290,  -290,   -33,   -33,    14,   525,   525,
    -290,   640,   -33,   574,    14,    38,     5,   -51,   -33,   -33,
    -290,   -33,    14,   -33,   -33,  -290,   525,   525,   525,    14,
      38,   525,  -290,  -290,   -33,  -290,  -290,  -290,  -290,   681,
     681,   681,  1612,  -290,  -290,  -290,     9,   -33,  -290,  -290,
    -290,   694,     9,  1612,   -33,  -290,  -290,  -290,  -290,     1,
     199,  1612,  -290,  1612,  1612,    76,  -290,  -290,  1570,    60,
     694,   694,   694,   694,   694,   694,   694,   694,   694,   694,
     694,   694,   694,   694,   694,   694,    14,    14,   694,   694,
     694,    23,  -290,   953,  1037,  1037,  -290,  -290,   -33,    15,
    1121,  1121,    14,   525,  -290,  -290,   -33,    38,  -290,  -290,
    -290,    33,  -290,  1205,  1205,    25,  -290,    32,    14,  1289,
    -290,  -290,  -290,    38,  -290,  -290,  -290,  -290,   -33,   -33,
    -290,     2,    40,  -290,   681,  -290,  1516,  -290,    46,    12,
    -290,  -290,  -290,   768,   852,   936,    16,    16,    28,    28,
      28,    28,   120,   120,    76,    76,  -290,  -290,  -290,   -32,
    -290,   -56,  -290,  1535,  1591,  1612,    33,   -33,   869,    68,
      84,    84,  1121,    88,    93,  -290,  -290,  -290,  -290,  -290,
    -290,  -290,  -290,  -290,    67,    87,    91,    14,  -290,   694,
      11,  -290,     7,  -290,    83,  -290,  -290,  -290,  -290,  -290,
     681,  -290,  -290,    74,    69,  -290,  -290,  -290,  -290,   694,
      84,    68,  -290,   -33,   115,   116,   112,   -33,   -33,    14,
     -33,   -33,   -33,  -290,  1612,   -33,  -290,  -290,   -33,  -290,
    -290,  -290,   525,   123,  -290,  1373,   -33,   -33,   -33,  -290,
    -290,  -290,  -290,  -290,  -290,  -290,  -290,   953,   -33,  -290,
    -290,  -290,  -290,  -290,  -290
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,   150,   148,   149,   144,   145,    79,     0,   175,
     176,   177,   178,   216,   217,   218,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   152,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    77,    76,     0,   146,   147,     0,     0,
       5,    10,    42,    41,    38,    39,    37,    36,    35,    34,
      33,    29,    30,    31,    32,    28,    26,    27,    43,     0,
      73,    75,    23,     0,    74,   192,    11,    12,    13,    24,
       0,     0,   173,    16,    17,    14,    15,    18,    19,    20,
      40,    21,    22,    25,   208,   209,   111,    44,   153,   154,
     121,   100,   101,     0,     0,    80,    94,     0,     0,     0,
       0,     0,    98,    99,     0,     0,     0,    82,    88,    90,
      91,    92,    95,    96,   103,    93,    52,     0,    70,    72,
       0,     0,     0,   185,   186,     0,     0,   189,     0,     0,
     193,     0,     0,     0,     0,   193,     0,     0,     0,     0,
     151,     0,     0,     0,     0,   179,     0,     0,     0,     0,
       0,     0,   180,     1,     0,     2,     6,     9,    78,     0,
       0,   117,   106,   181,   107,   167,   191,     0,   210,   165,
     166,     0,   172,   110,     0,    89,   108,   109,   102,     0,
       0,    81,   134,   136,   135,   137,   133,   132,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    71,     0,     0,     0,   183,   184,     0,   188,
       0,     0,     0,     0,   196,   198,     0,     0,   169,   212,
     200,     0,    69,     0,     0,     0,    66,     0,     0,     0,
      49,    47,    48,     0,   168,   211,    45,     3,     0,     0,
     120,     0,   116,   118,     0,   190,     0,   215,    87,     0,
      97,    83,    84,   128,   129,   127,   138,   139,   143,   142,
     140,   141,   130,   131,   122,   123,   124,   125,   126,   114,
     113,   103,   112,     0,     0,    51,     0,     0,     0,   157,
     162,   162,     0,     0,     0,   194,    46,   199,   171,   214,
     207,   206,   205,   204,   203,     0,     0,     0,    65,     0,
       0,    61,     0,    63,     0,   170,   213,   195,   197,   115,
       0,   182,   174,     0,     0,   104,   105,    53,    50,     0,
     162,   156,   158,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    67,    68,     0,    62,    64,     0,   119,
      86,    85,     0,     0,   159,     0,     0,     0,     0,    56,
      55,   202,   201,    58,    59,    60,    57,     0,     0,   161,
     163,   164,   187,   160,   155
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -290,  -290,  -290,   154,  -206,   -48,    89,  -290,  -290,  -290,
    -290,  -290,  -290,  -290,  -290,  -290,  -290,  -290,  -290,  -290,
    -290,  -290,  -159,  -290,  -290,  -155,  -290,  -290,  -290,  -290,
    -290,    -1,  -290,  -290,  -290,   222,   -52,  -141,  -290,  -290,
      77,   -44,  -290,  -290,  -290,  -143,  -290,  -290,  -290,  -290,
     165,  -290,  -290,  -290,  -290,  -290,  -151,  -289,  -290,  -290,
    -290,  -290,   -71,   -92,   114,  -290,  -290,    -9,  -290,  -290,
    -290,  -290,   -24,  -290,  -290,  -245,  -290,  -290,  -290,  -290,
     -93,  -290,  -290,   -67,  -290
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    48,   165,   298,   299,    50,    51,    52,    53,    54,
      55,    56,    57,   221,   297,    58,    59,    60,    61,    62,
      63,   320,   321,    64,   245,   246,    65,    66,    67,    68,
      69,    70,    71,    72,    73,   172,   118,   173,   185,   184,
     119,   120,   174,   261,   262,   263,   121,   122,   123,    74,
     124,   151,   125,    76,   340,   341,   342,   344,    77,    78,
      79,    80,    81,    82,   176,    83,    84,   135,    85,   228,
      86,   177,   141,    87,    88,    89,    90,    91,    92,   350,
     314,    93,    94,    95,    96
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      97,   166,   236,   323,   175,   146,   147,   133,   178,   241,
       2,   187,   345,   136,   218,   128,   129,     2,   300,   301,
       2,   232,   148,     2,   303,   304,    43,    44,   258,   259,
     260,     9,    10,    11,    12,     2,   163,   315,   316,     2,
     219,   216,   127,   324,    43,    44,   310,     9,    10,    11,
      12,   363,     9,    10,    11,    12,    13,    14,    15,   355,
     217,   144,   220,   217,    43,    44,    43,    44,   218,   159,
      37,   329,   168,   170,   238,   323,   334,   232,   239,   179,
     180,   264,   311,   296,    43,    44,    37,   232,   134,   254,
      45,    37,   319,   255,   219,   339,   346,   317,   205,   206,
     207,   208,   209,   210,   211,   212,    45,   213,   214,   215,
     343,    45,   330,   229,   209,   210,   211,   212,   333,   213,
     214,   215,   347,   331,   271,   272,   222,   348,   349,   223,
     224,   225,   351,   358,   226,   227,   352,   230,   231,   360,
     361,   234,   235,   366,   367,   240,   368,   242,   243,   313,
     244,   378,   248,   249,    49,   250,   251,   252,   167,   379,
     256,   356,   353,   257,   289,    75,   308,   213,   214,   215,
     309,   383,   290,   126,    98,    99,   265,     2,   100,   101,
     102,   188,   325,   267,   131,   132,   326,   359,   137,   260,
     364,   140,   142,   143,   145,   140,   182,   149,   150,   152,
     153,   154,   155,   337,   313,     0,   160,     0,   211,   212,
     162,   213,   214,   215,    75,     0,     0,     0,     0,     0,
       0,     0,   103,   104,     0,     0,     0,   302,     0,   117,
       0,     0,   306,     0,    75,   307,     0,     0,   106,     0,
     130,     0,     0,     0,   318,     0,   138,   139,     0,     0,
     166,     0,     0,     0,     0,     0,     0,   327,   328,     0,
     156,   157,   158,     0,   161,   114,   115,   116,   186,   189,
     268,   269,     0,     0,   192,     0,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,     0,
     213,   214,   215,     0,     0,     0,   338,     0,     0,     0,
       0,     0,   140,     0,     0,     0,     0,     0,     0,   237,
       0,     0,     0,     0,     0,     0,     0,   247,   183,     0,
       0,   357,     0,     0,   253,     0,     0,     0,     0,   190,
     191,     0,   193,   194,     0,   195,   196,   197,   198,     0,
       0,     0,   365,     0,     0,     0,   369,   370,     0,   372,
     373,   374,     0,     0,   375,     0,     0,   376,     0,     0,
       0,   377,     0,   233,     0,   380,   381,   382,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   384,     0,     0,
       0,   291,   292,     0,     0,     0,     0,     0,    75,    75,
      75,     0,     0,     0,     0,    75,    75,   305,     0,     0,
       0,     0,     0,   266,     0,     0,   312,     0,    75,    75,
       0,     0,     0,   322,    75,     0,     0,     0,     0,     0,
       0,     0,   273,   274,   275,   276,   277,   278,   279,   280,
     281,   282,   283,   284,   285,   286,   287,   288,     0,     0,
     293,   294,   295,    98,    99,     0,     2,   100,   101,   102,
       0,     0,     0,     0,     9,    10,    11,    12,    13,    14,
      15,   312,     0,    75,     0,     0,     0,    75,     0,     0,
       0,     0,     0,     0,     0,     0,   169,     0,     0,     0,
       0,     0,   247,    98,    99,   322,     2,   100,   101,   102,
       0,   103,   104,    37,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   170,   106,   171,     0,
       0,     0,     0,    45,   371,     0,     0,     0,   109,     0,
       0,     0,     0,     0,   110,   111,     0,     0,   112,   113,
      75,   103,   104,     0,   114,   115,   116,     0,     0,     0,
       0,   354,    75,   105,     0,     0,     0,   106,     0,     0,
     107,     0,     0,     0,   108,     0,     0,     0,   109,     0,
       0,   362,     0,     0,   110,   111,     0,     0,   112,   113,
       0,     0,     0,     0,   114,   115,   116,    98,    99,     0,
       2,   100,   101,   102,    43,    44,     0,     0,     0,     0,
      98,    99,     0,     2,   100,   101,   102,     0,     0,     0,
       0,     0,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,     0,   213,   214,   215,     0,
       0,     0,     0,     0,     0,   103,   104,     0,     0,     0,
       0,     0,     0,    43,    44,     0,     0,     0,   103,   104,
       0,   106,   171,    98,    99,     0,     2,   100,   101,   102,
       0,     0,   109,     0,   106,   171,     0,   181,   110,   111,
       0,     0,   112,   113,     0,   109,     0,     0,   114,   115,
     116,   110,   111,     0,     0,   112,   113,     0,     0,     0,
       0,   114,   115,   116,    98,    99,     0,     2,   100,   101,
     102,   103,   104,     0,     0,     0,     0,    98,    99,     0,
       2,   100,   101,   102,     0,     0,     0,   106,     0,     0,
       0,     0,   232,     0,     0,     0,     0,     0,   109,     0,
       0,     0,     0,     0,   110,   111,     0,     0,   112,   113,
       0,     0,   103,   104,   114,   115,   116,     0,     0,     0,
       0,     0,     0,     0,     0,   103,   104,     0,   106,   171,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   109,
       0,   106,     0,     0,     0,   110,   111,     0,     0,   112,
     113,     0,   109,     0,     0,   114,   115,   116,   110,   111,
       0,     0,   112,   113,     0,    -4,     1,     0,   114,   115,
     116,     2,     0,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,     0,     0,     0,    21,    22,    23,    24,    25,     0,
      26,     0,    27,    28,   164,    29,    30,    31,    32,    33,
       0,    34,    35,     0,    36,     0,     0,     0,    37,    38,
      39,    40,    41,    42,    43,    44,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,    45,   213,
     214,   215,     0,     0,     0,     0,     0,     0,     0,     0,
       1,     0,     0,    46,    47,     2,     0,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    -7,    -7,    -7,    21,    22,
      23,    24,    25,    -7,    26,     0,    27,    28,     0,    29,
      30,    31,    32,    33,    -7,    34,    35,     0,    36,    -7,
       0,     0,    37,    38,    39,    40,    41,    42,    43,    44,
       0,   202,   203,   204,   205,   206,   207,   208,   209,   210,
     211,   212,    45,   213,   214,   215,     0,     0,     0,     0,
       0,     0,     0,     0,     1,     0,     0,    46,    47,     2,
       0,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    -8,
      -8,    -8,    21,    22,    23,    24,    25,     0,    26,     0,
      27,    28,     0,    29,    30,    31,    32,    33,     0,    34,
      35,     0,    36,     0,     0,     0,    37,    38,    39,    40,
      41,    42,    43,    44,     0,     0,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,    45,   213,   214,   215,
       0,     0,     0,     0,     0,     0,     0,     0,     1,     0,
       0,    46,    47,     2,     0,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    -8,     0,    -8,    21,    22,    23,    24,
      25,     0,    26,     0,    27,    28,     0,    29,    30,    31,
      32,    33,     0,    34,    35,     0,    36,     0,     0,     0,
      37,    38,    39,    40,    41,    42,    43,    44,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     1,     0,     0,    46,    47,     2,     0,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,     0,     0,     0,
      21,    22,    23,    24,    25,    -8,    26,     0,    27,    28,
       0,    29,    30,    31,    32,    33,     0,    34,    35,     0,
      36,     0,     0,     0,    37,    38,    39,    40,    41,    42,
      43,    44,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     1,     0,     0,    46,
      47,     2,     0,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,     0,     0,     0,    21,    22,    23,    24,    25,     0,
      26,     0,    27,    28,     0,    29,    30,    31,    32,    33,
      -8,    34,    35,     0,    36,     0,     0,     0,    37,    38,
      39,    40,    41,    42,    43,    44,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       1,     0,     0,    46,    47,     2,     0,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,     0,     0,     0,    21,    22,
      23,    24,    25,     0,    26,     0,    27,    28,     0,    29,
      30,    31,    32,    33,     0,    34,    35,     0,    36,    -8,
       0,     0,    37,    38,    39,    40,    41,    42,    43,    44,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     1,     0,     0,    46,    47,     2,
       0,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,     0,
       0,    -8,    21,    22,    23,    24,    25,     0,    26,     0,
      27,    28,     0,    29,    30,    31,    32,    33,     0,    34,
      35,     0,    36,     0,     0,     0,    37,    38,    39,    40,
      41,    42,    43,    44,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     1,     0,
       0,    46,    47,     2,     0,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,     0,     0,     0,    21,    22,    23,    24,
      25,     0,    26,     0,    27,    28,     0,    29,    30,    31,
      32,    33,     0,    34,    35,     0,    36,     0,     0,     0,
      37,    38,    39,    40,    41,    42,    43,    44,     0,     0,
       1,     0,     0,     0,     0,     2,     0,     0,     0,     0,
      45,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    46,    47,     0,    21,    22,
      23,    24,    25,     0,    26,     0,    27,    28,     0,    29,
      30,    31,    32,    33,     0,    34,    35,     0,    36,     0,
       0,     0,    37,    38,    39,    40,    41,    42,    43,    44,
       0,     0,     0,     0,     0,     0,     0,   332,     0,     0,
       0,     0,    45,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   335,   213,   214,   215,
       0,     0,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   199,   213,   214,   215,     0,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,     0,   213,   214,   215,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   212,
       0,   213,   214,   215,     0,     0,     0,   270,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   209,   210,   211,
     212,     0,   213,   214,   215,     0,     0,     0,   336,   200,
     201,   202,   203,   204,   205,   206,   207,   208,   209,   210,
     211,   212,     0,   213,   214,   215
};

static const yytype_int16 yycheck[] =
{
       1,    49,   143,   248,    75,    29,    30,     4,    75,    60,
       6,   103,   301,    22,    70,    16,    17,     6,   224,   225,
       6,    72,    31,     6,   230,   231,    59,    60,   169,   170,
     171,    14,    15,    16,    17,     6,     0,   243,   244,     6,
      96,    73,    57,   249,    59,    60,    13,    14,    15,    16,
      17,   340,    14,    15,    16,    17,    18,    19,    20,    48,
      62,    57,    66,    62,    59,    60,    59,    60,    70,    40,
      53,    69,    73,    66,   145,   320,    64,    72,   145,    80,
      81,    72,    49,    60,    59,    60,    53,    72,    85,   160,
      73,    53,    60,   160,    96,    27,   302,    72,    82,    83,
      84,    85,    86,    87,    88,    89,    73,    91,    92,    93,
      26,    73,    72,   137,    86,    87,    88,    89,    72,    91,
      92,    93,    34,   264,    64,    65,   127,    34,    61,   130,
     131,   132,    45,    50,   135,   136,    45,   138,   139,    65,
      71,   142,   143,    28,    28,   146,    34,   148,   149,   241,
     151,    28,   153,   154,     0,   156,   157,   158,    69,   365,
     161,   320,   317,   164,   216,     0,   237,    91,    92,    93,
     237,   377,   216,     8,     3,     4,   177,     6,     7,     8,
       9,   104,   253,   184,    19,    20,   253,   330,    23,   330,
     341,    26,    27,    28,    29,    30,    82,    32,    33,    34,
      35,    36,    37,   296,   296,    -1,    41,    -1,    88,    89,
      45,    91,    92,    93,    49,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    51,    52,    -1,    -1,    -1,   228,    -1,     7,
      -1,    -1,   233,    -1,    69,   236,    -1,    -1,    67,    -1,
      18,    -1,    -1,    -1,   245,    -1,    24,    25,    -1,    -1,
     298,    -1,    -1,    -1,    -1,    -1,    -1,   258,   259,    -1,
      38,    39,    40,    -1,    42,    94,    95,    96,   103,   104,
      71,    72,    -1,    -1,   109,    -1,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    -1,
      91,    92,    93,    -1,    -1,    -1,   297,    -1,    -1,    -1,
      -1,    -1,   137,    -1,    -1,    -1,    -1,    -1,    -1,   144,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   152,    96,    -1,
      -1,   322,    -1,    -1,   159,    -1,    -1,    -1,    -1,   107,
     108,    -1,   110,   111,    -1,   113,   114,   115,   116,    -1,
      -1,    -1,   343,    -1,    -1,    -1,   347,   348,    -1,   350,
     351,   352,    -1,    -1,   355,    -1,    -1,   358,    -1,    -1,
      -1,   362,    -1,   141,    -1,   366,   367,   368,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   378,    -1,    -1,
      -1,   216,   217,    -1,    -1,    -1,    -1,    -1,   223,   224,
     225,    -1,    -1,    -1,    -1,   230,   231,   232,    -1,    -1,
      -1,    -1,    -1,   181,    -1,    -1,   241,    -1,   243,   244,
      -1,    -1,    -1,   248,   249,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   214,   215,    -1,    -1,
     218,   219,   220,     3,     4,    -1,     6,     7,     8,     9,
      -1,    -1,    -1,    -1,    14,    15,    16,    17,    18,    19,
      20,   296,    -1,   298,    -1,    -1,    -1,   302,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    -1,    -1,    -1,
      -1,    -1,   317,     3,     4,   320,     6,     7,     8,     9,
      -1,    51,    52,    53,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    68,    -1,
      -1,    -1,    -1,    73,   349,    -1,    -1,    -1,    78,    -1,
      -1,    -1,    -1,    -1,    84,    85,    -1,    -1,    88,    89,
     365,    51,    52,    -1,    94,    95,    96,    -1,    -1,    -1,
      -1,   319,   377,    63,    -1,    -1,    -1,    67,    -1,    -1,
      70,    -1,    -1,    -1,    74,    -1,    -1,    -1,    78,    -1,
      -1,   339,    -1,    -1,    84,    85,    -1,    -1,    88,    89,
      -1,    -1,    -1,    -1,    94,    95,    96,     3,     4,    -1,
       6,     7,     8,     9,    59,    60,    -1,    -1,    -1,    -1,
       3,     4,    -1,     6,     7,     8,     9,    -1,    -1,    -1,
      -1,    -1,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    -1,    91,    92,    93,    -1,
      -1,    -1,    -1,    -1,    -1,    51,    52,    -1,    -1,    -1,
      -1,    -1,    -1,    59,    60,    -1,    -1,    -1,    51,    52,
      -1,    67,    68,     3,     4,    -1,     6,     7,     8,     9,
      -1,    -1,    78,    -1,    67,    68,    -1,    70,    84,    85,
      -1,    -1,    88,    89,    -1,    78,    -1,    -1,    94,    95,
      96,    84,    85,    -1,    -1,    88,    89,    -1,    -1,    -1,
      -1,    94,    95,    96,     3,     4,    -1,     6,     7,     8,
       9,    51,    52,    -1,    -1,    -1,    -1,     3,     4,    -1,
       6,     7,     8,     9,    -1,    -1,    -1,    67,    -1,    -1,
      -1,    -1,    72,    -1,    -1,    -1,    -1,    -1,    78,    -1,
      -1,    -1,    -1,    -1,    84,    85,    -1,    -1,    88,    89,
      -1,    -1,    51,    52,    94,    95,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    51,    52,    -1,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,
      -1,    67,    -1,    -1,    -1,    84,    85,    -1,    -1,    88,
      89,    -1,    78,    -1,    -1,    94,    95,    96,    84,    85,
      -1,    -1,    88,    89,    -1,     0,     1,    -1,    94,    95,
      96,     6,    -1,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    -1,    -1,    -1,    29,    30,    31,    32,    33,    -1,
      35,    -1,    37,    38,    39,    40,    41,    42,    43,    44,
      -1,    46,    47,    -1,    49,    -1,    -1,    -1,    53,    54,
      55,    56,    57,    58,    59,    60,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    73,    91,
      92,    93,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,    -1,    88,    89,     6,    -1,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    45,    46,    47,    -1,    49,    50,
      -1,    -1,    53,    54,    55,    56,    57,    58,    59,    60,
      -1,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    73,    91,    92,    93,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     1,    -1,    -1,    88,    89,     6,
      -1,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    -1,    35,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    46,
      47,    -1,    49,    -1,    -1,    -1,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    -1,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    73,    91,    92,    93,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,
      -1,    88,    89,     6,    -1,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    -1,    28,    29,    30,    31,    32,
      33,    -1,    35,    -1,    37,    38,    -1,    40,    41,    42,
      43,    44,    -1,    46,    47,    -1,    49,    -1,    -1,    -1,
      53,    54,    55,    56,    57,    58,    59,    60,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     1,    -1,    -1,    88,    89,     6,    -1,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    37,    38,
      -1,    40,    41,    42,    43,    44,    -1,    46,    47,    -1,
      49,    -1,    -1,    -1,    53,    54,    55,    56,    57,    58,
      59,    60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    73,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    88,
      89,     6,    -1,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    -1,    -1,    -1,    29,    30,    31,    32,    33,    -1,
      35,    -1,    37,    38,    -1,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,    49,    -1,    -1,    -1,    53,    54,
      55,    56,    57,    58,    59,    60,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,    -1,    88,    89,     6,    -1,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    -1,    35,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    46,    47,    -1,    49,    50,
      -1,    -1,    53,    54,    55,    56,    57,    58,    59,    60,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     1,    -1,    -1,    88,    89,     6,
      -1,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    -1,
      -1,    28,    29,    30,    31,    32,    33,    -1,    35,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    46,
      47,    -1,    49,    -1,    -1,    -1,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,
      -1,    88,    89,     6,    -1,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    -1,    35,    -1,    37,    38,    -1,    40,    41,    42,
      43,    44,    -1,    46,    47,    -1,    49,    -1,    -1,    -1,
      53,    54,    55,    56,    57,    58,    59,    60,    -1,    -1,
       1,    -1,    -1,    -1,    -1,     6,    -1,    -1,    -1,    -1,
      73,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    88,    89,    -1,    29,    30,
      31,    32,    33,    -1,    35,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    46,    47,    -1,    49,    -1,
      -1,    -1,    53,    54,    55,    56,    57,    58,    59,    60,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    71,    -1,    -1,
      -1,    -1,    73,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    71,    91,    92,    93,
      -1,    -1,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    72,    91,    92,    93,    -1,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    -1,    91,    92,    93,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      -1,    91,    92,    93,    -1,    -1,    -1,    97,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    -1,    91,    92,    93,    -1,    -1,    -1,    97,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    -1,    91,    92,    93
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     6,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    29,    30,    31,    32,    33,    35,    37,    38,    40,
      41,    42,    43,    44,    46,    47,    49,    53,    54,    55,
      56,    57,    58,    59,    60,    73,    88,    89,    99,   101,
     103,   104,   105,   106,   107,   108,   109,   110,   113,   114,
     115,   116,   117,   118,   121,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   147,   148,   151,   156,   157,   158,
     159,   160,   161,   163,   164,   166,   168,   171,   172,   173,
     174,   175,   176,   179,   180,   181,   182,   129,     3,     4,
       7,     8,     9,    51,    52,    63,    67,    70,    74,    78,
      84,    85,    88,    89,    94,    95,    96,   133,   134,   138,
     139,   144,   145,   146,   148,   150,   148,    57,   129,   129,
     133,   148,   148,     4,    85,   165,   165,   148,   133,   133,
     148,   170,   148,   148,    57,   148,   170,   170,   165,   148,
     148,   149,   148,   148,   148,   148,   133,   133,   133,    40,
     148,   133,   148,     0,    39,   100,   103,   104,   129,    36,
      66,    68,   133,   135,   140,   160,   162,   169,   181,   129,
     129,    70,   162,   133,   137,   136,   148,   161,   138,   148,
     133,   133,   148,   133,   133,   133,   133,   133,   133,    72,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    91,    92,    93,    73,    62,    70,    96,
      66,   111,   129,   129,   129,   129,   129,   129,   167,   170,
     129,   129,    72,   133,   129,   129,   135,   148,   160,   181,
     129,    60,   129,   129,   129,   122,   123,   148,   129,   129,
     129,   129,   129,   148,   160,   181,   129,   129,   135,   135,
     135,   141,   142,   143,    72,   129,   133,   129,    71,    72,
      97,    64,    65,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   133,   133,   133,   133,   133,   133,   133,   134,
     139,   148,   148,   133,   133,   133,    60,   112,   101,   102,
     102,   102,   129,   102,   102,   148,   129,   129,   160,   181,
      13,    49,   148,   161,   178,   102,   102,    72,   129,    60,
     119,   120,   148,   173,   102,   160,   181,   129,   129,    69,
      72,   135,    71,    72,    64,    71,    97,   178,   129,    27,
     152,   153,   154,    26,   155,   155,   102,    34,    34,    61,
     177,    45,    45,   123,   133,    48,   120,   129,    50,   143,
      65,    71,   133,   155,   154,   129,    28,    28,    34,   129,
     129,   148,   129,   129,   129,   129,   129,   129,    28,   102,
     129,   129,   129,   102,   129
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    98,    99,   100,   100,   101,   101,   102,   102,   103,
     103,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   105,   106,   107,   108,   109,
     110,   111,   111,   112,   112,   113,   114,   115,   116,   117,
     118,   119,   119,   120,   120,   121,   122,   122,   123,   124,
     125,   125,   126,   127,   128,   129,   129,   130,   131,   132,
     132,   132,   132,   132,   132,   132,   132,   132,   133,   133,
     133,   133,   133,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   133,   134,   134,   134,   135,   135,   136,   136,
     137,   137,   138,   139,   139,   140,   141,   141,   142,   142,
     143,   144,   145,   145,   145,   145,   145,   145,   145,   145,
     145,   145,   145,   145,   145,   145,   145,   145,   146,   146,
     146,   146,   146,   146,   147,   147,   147,   147,   147,   147,
     148,   149,   149,   150,   150,   151,   152,   152,   153,   153,
     154,   155,   155,   156,   157,   158,   158,   159,   159,   159,
     159,   159,   160,   160,   160,   161,   161,   161,   161,   161,
     161,   162,   162,   163,   164,   165,   165,   166,   167,   167,
     168,   169,   169,   170,   170,   171,   172,   173,   174,   174,
     175,   176,   177,   177,   178,   178,   178,   178,   179,   179,
     180,   180,   180,   180,   180,   181,   182,   182,   182
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     2,     0,     1,     2,     1,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     3,     4,     3,     3,     3,
       5,     2,     0,     2,     0,     6,     6,     6,     6,     6,
       6,     1,     2,     1,     2,     4,     1,     3,     3,     3,
       2,     3,     2,     1,     1,     1,     1,     1,     2,     1,
       2,     3,     2,     4,     4,     6,     6,     4,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     3,     1,     1,
       1,     1,     2,     1,     4,     4,     1,     1,     1,     1,
       1,     0,     3,     3,     3,     3,     1,     0,     1,     3,
       1,     1,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     2,     2,     2,     2,     2,     2,     3,     3,
       3,     3,     3,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     0,     1,     1,     8,     1,     0,     1,     2,
       4,     3,     0,     7,     7,     2,     2,     2,     3,     3,
       4,     4,     2,     1,     4,     1,     1,     1,     1,     2,
       2,     1,     3,     3,     3,     1,     1,     7,     1,     0,
       3,     1,     0,     1,     3,     4,     3,     4,     3,     4,
       3,     6,     2,     0,     1,     1,     1,     1,     1,     1,
       2,     3,     3,     4,     4,     3,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]));
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Location data for the lookahead symbol.  */
YYLTYPE yylloc
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* assembly_unit: statement_list end_opt  */
#line 123 "parser.y"
                           { root_node = astnode_create_list((yyvsp[-1].node)); }
#line 2259 "parser.c"
    break;

  case 3: /* end_opt: END line_tail  */
#line 127 "parser.y"
                  { ; }
#line 2265 "parser.c"
    break;

  case 5: /* statement_list: labelable_statement  */
#line 132 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2271 "parser.c"
    break;

  case 6: /* statement_list: statement_list labelable_statement  */
#line 133 "parser.y"
                                         {
         if ((yyvsp[-1].node) != NULL) { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
         else { (yyval.node) = (yyvsp[0].node); }
        }
#line 2280 "parser.c"
    break;

  case 7: /* statement_list_opt: statement_list  */
#line 140 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2286 "parser.c"
    break;

  case 8: /* statement_list_opt: %empty  */
#line 141 "parser.y"
      { (yyval.node) = NULL; }
#line 2292 "parser.c"
    break;

  case 9: /* labelable_statement: label_decl statement  */
#line 145 "parser.y"
                         { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2298 "parser.c"
    break;

  case 10: /* labelable_statement: statement  */
#line 146 "parser.y"
                { (yyval.node) = (yyvsp[0].node); }
#line 2304 "parser.c"
    break;

  case 11: /* statement: if_statement  */
#line 150 "parser.y"
                 { (yyval.node) = (yyvsp[0].node); }
#line 2310 "parser.c"
    break;

  case 12: /* statement: ifdef_statement  */
#line 151 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2316 "parser.c"
    break;

  case 13: /* statement: ifndef_statement  */
#line 152 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2322 "parser.c"
    break;

  case 14: /* statement: macro_decl_statement  */
#line 153 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2328 "parser.c"
    break;

  case 15: /* statement: macro_statement  */
#line 154 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2334 "parser.c"
    break;

  case 16: /* statement: incsrc_statement  */
#line 155 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2340 "parser.c"
    break;

  case 17: /* statement: incbin_statement  */
#line 156 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2346 "parser.c"
    break;

  case 18: /* statement: equ_statement  */
#line 157 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2352 "parser.c"
    break;

  case 19: /* statement: undef_statement  */
#line 158 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2358 "parser.c"
    break;

  case 20: /* statement: assign_statement  */
#line 159 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2364 "parser.c"
    break;

  case 21: /* statement: public_statement  */
#line 160 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2370 "parser.c"
    break;

  case 22: /* statement: extrn_statement  */
#line 161 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2376 "parser.c"
    break;

  case 23: /* statement: instruction_statement  */
#line 162 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2382 "parser.c"
    break;

  case 24: /* statement: data_statement  */
#line 163 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2388 "parser.c"
    break;

  case 25: /* statement: storage_statement  */
#line 164 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2394 "parser.c"
    break;

  case 26: /* statement: dataseg_statement  */
#line 165 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2400 "parser.c"
    break;

  case 27: /* statement: codeseg_statement  */
#line 166 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2406 "parser.c"
    break;

  case 28: /* statement: charmap_statement  */
#line 167 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2412 "parser.c"
    break;

  case 29: /* statement: struc_decl_statement  */
#line 168 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2418 "parser.c"
    break;

  case 30: /* statement: union_decl_statement  */
#line 169 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2424 "parser.c"
    break;

  case 31: /* statement: enum_decl_statement  */
#line 170 "parser.y"
                          { (yyval.node) = (yyvsp[0].node); }
#line 2430 "parser.c"
    break;

  case 32: /* statement: record_decl_statement  */
#line 171 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2436 "parser.c"
    break;

  case 33: /* statement: proc_statement  */
#line 172 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2442 "parser.c"
    break;

  case 34: /* statement: rept_statement  */
#line 173 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2448 "parser.c"
    break;

  case 35: /* statement: while_statement  */
#line 174 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2454 "parser.c"
    break;

  case 36: /* statement: label_statement  */
#line 175 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2460 "parser.c"
    break;

  case 37: /* statement: message_statement  */
#line 176 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2466 "parser.c"
    break;

  case 38: /* statement: warning_statement  */
#line 177 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2472 "parser.c"
    break;

  case 39: /* statement: error_statement  */
#line 178 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2478 "parser.c"
    break;

  case 40: /* statement: define_statement  */
#line 179 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2484 "parser.c"
    break;

  case 41: /* statement: align_statement  */
#line 180 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2490 "parser.c"
    break;

  case 42: /* statement: org_statement  */
#line 181 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2496 "parser.c"
    break;

  case 43: /* statement: null_statement  */
#line 182 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2502 "parser.c"
    break;

  case 44: /* statement: error line_tail  */
#line 183 "parser.y"
                      { (yyval.node) = NULL; }
#line 2508 "parser.c"
    break;

  case 45: /* org_statement: ORG expression line_tail  */
#line 187 "parser.y"
                             { (yyval.node) = astnode_create_org((yyvsp[-1].node), (yyloc)); }
#line 2514 "parser.c"
    break;

  case 46: /* align_statement: ALIGN identifier_list expression line_tail  */
#line 191 "parser.y"
                                               { (yyval.node) = astnode_create_align((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 2520 "parser.c"
    break;

  case 47: /* warning_statement: WARNING expression line_tail  */
#line 195 "parser.y"
                                 { (yyval.node) = astnode_create_warning((yyvsp[-1].node), (yyloc)); }
#line 2526 "parser.c"
    break;

  case 48: /* error_statement: ERROR expression line_tail  */
#line 199 "parser.y"
                               { (yyval.node) = astnode_create_error((yyvsp[-1].node), (yyloc)); }
#line 2532 "parser.c"
    break;

  case 49: /* message_statement: MESSAGE expression line_tail  */
#line 203 "parser.y"
                                 { (yyval.node) = astnode_create_message((yyvsp[-1].node), (yyloc)); }
#line 2538 "parser.c"
    break;

  case 50: /* label_statement: _LABEL_ identifier label_addr_part_opt label_type_part_opt line_tail  */
#line 207 "parser.y"
                                                                         { (yyval.node) = astnode_create_label((yyvsp[-3].node)->label, (yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); astnode_finalize((yyvsp[-3].node)); }
#line 2544 "parser.c"
    break;

  case 51: /* label_addr_part_opt: '=' expression  */
#line 211 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2550 "parser.c"
    break;

  case 52: /* label_addr_part_opt: %empty  */
#line 212 "parser.y"
      { (yyval.node) = NULL; }
#line 2556 "parser.c"
    break;

  case 53: /* label_type_part_opt: ':' symbol_type  */
#line 216 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2562 "parser.c"
    break;

  case 54: /* label_type_part_opt: %empty  */
#line 217 "parser.y"
      { (yyval.node) = NULL; }
#line 2568 "parser.c"
    break;

  case 55: /* while_statement: WHILE expression line_tail statement_list_opt ENDM line_tail  */
#line 221 "parser.y"
                                                                 { (yyval.node) = astnode_create_while((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2574 "parser.c"
    break;

  case 56: /* rept_statement: REPT expression line_tail statement_list_opt ENDM line_tail  */
#line 225 "parser.y"
                                                                { (yyval.node) = astnode_create_rept((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2580 "parser.c"
    break;

  case 57: /* proc_statement: PROC identifier line_tail statement_list_opt ENDP line_tail  */
#line 229 "parser.y"
                                                                { (yyval.node) = astnode_create_proc((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2586 "parser.c"
    break;

  case 58: /* struc_decl_statement: STRUC identifier line_tail statement_list_opt ENDS line_tail  */
#line 233 "parser.y"
                                                                 { (yyval.node) = astnode_create_struc_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2592 "parser.c"
    break;

  case 59: /* union_decl_statement: UNION identifier_opt line_tail statement_list_opt ENDS line_tail  */
#line 237 "parser.y"
                                                                     { (yyval.node) = astnode_create_union_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2598 "parser.c"
    break;

  case 60: /* enum_decl_statement: ENUM identifier line_tail enum_item_list ENDE line_tail  */
#line 241 "parser.y"
                                                            { (yyval.node) = astnode_create_enum_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2604 "parser.c"
    break;

  case 61: /* enum_item_list: enum_item  */
#line 245 "parser.y"
              { (yyval.node) = (yyvsp[0].node); }
#line 2610 "parser.c"
    break;

  case 62: /* enum_item_list: enum_item_list enum_item  */
#line 246 "parser.y"
                               { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2616 "parser.c"
    break;

  case 63: /* enum_item: assign_statement  */
#line 250 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2622 "parser.c"
    break;

  case 64: /* enum_item: identifier line_tail  */
#line 251 "parser.y"
                           { (yyval.node) = (yyvsp[-1].node); }
#line 2628 "parser.c"
    break;

  case 65: /* record_decl_statement: RECORD identifier record_field_list line_tail  */
#line 255 "parser.y"
                                                  { (yyval.node) = astnode_create_record_decl((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 2634 "parser.c"
    break;

  case 66: /* record_field_list: record_field  */
#line 259 "parser.y"
                 { (yyval.node) = (yyvsp[0].node); }
#line 2640 "parser.c"
    break;

  case 67: /* record_field_list: record_field_list ',' record_field  */
#line 260 "parser.y"
                                         { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2646 "parser.c"
    break;

  case 68: /* record_field: identifier ':' expression  */
#line 264 "parser.y"
                              { (yyval.node) = astnode_create_bitfield_decl((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2652 "parser.c"
    break;

  case 69: /* charmap_statement: CHARMAP file_specifier line_tail  */
#line 268 "parser.y"
                                     { (yyval.node) = astnode_create_charmap((yyvsp[-1].node), (yyloc)); }
#line 2658 "parser.c"
    break;

  case 70: /* dataseg_statement: DATASEG line_tail  */
#line 272 "parser.y"
                      { (yyval.node) = astnode_create_dataseg(0, (yyloc)); }
#line 2664 "parser.c"
    break;

  case 71: /* dataseg_statement: DATASEG ZEROPAGE line_tail  */
#line 273 "parser.y"
                                 { (yyval.node) = astnode_create_dataseg(ZEROPAGE_FLAG, (yyloc)); }
#line 2670 "parser.c"
    break;

  case 72: /* codeseg_statement: CODESEG line_tail  */
#line 276 "parser.y"
                      { (yyval.node) = astnode_create_codeseg((yyloc)); }
#line 2676 "parser.c"
    break;

  case 73: /* null_statement: line_tail  */
#line 280 "parser.y"
              { (yyval.node) = NULL; }
#line 2682 "parser.c"
    break;

  case 74: /* label_decl: label  */
#line 284 "parser.y"
          { (yyval.node) = (yyvsp[0].node); }
#line 2688 "parser.c"
    break;

  case 75: /* line_tail: newline  */
#line 288 "parser.y"
            { ; }
#line 2694 "parser.c"
    break;

  case 76: /* line_tail: ':'  */
#line 289 "parser.y"
          { ; }
#line 2700 "parser.c"
    break;

  case 77: /* newline: '\n'  */
#line 293 "parser.y"
         { ; }
#line 2706 "parser.c"
    break;

  case 78: /* instruction_statement: instruction line_tail  */
#line 297 "parser.y"
                          { (yyval.node) = (yyvsp[-1].node); }
#line 2712 "parser.c"
    break;

  case 79: /* instruction: MNEMONIC  */
#line 301 "parser.y"
             { (yyval.node) = astnode_create_instruction((yyvsp[0].mnemonic), IMPLIED_MODE, NULL, (yyloc)); }
#line 2718 "parser.c"
    break;

  case 80: /* instruction: MNEMONIC 'A'  */
#line 302 "parser.y"
                   { (yyval.node) = astnode_create_instruction((yyvsp[-1].mnemonic), ACCUMULATOR_MODE, NULL, (yyloc)); }
#line 2724 "parser.c"
    break;

  case 81: /* instruction: MNEMONIC '#' expression  */
#line 303 "parser.y"
                              { (yyval.node) = astnode_create_instruction((yyvsp[-2].mnemonic), IMMEDIATE_MODE, (yyvsp[0].node), (yyloc)); }
#line 2730 "parser.c"
    break;

  case 82: /* instruction: MNEMONIC expression  */
#line 304 "parser.y"
                          { (yyval.node) = astnode_create_instruction((yyvsp[-1].mnemonic), ABSOLUTE_MODE, (yyvsp[0].node), (yyloc)); }
#line 2736 "parser.c"
    break;

  case 83: /* instruction: MNEMONIC expression ',' 'X'  */
#line 305 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), ABSOLUTE_X_MODE, (yyvsp[-2].node), (yyloc)); }
#line 2742 "parser.c"
    break;

  case 84: /* instruction: MNEMONIC expression ',' 'Y'  */
#line 306 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), ABSOLUTE_Y_MODE, (yyvsp[-2].node), (yyloc)); }
#line 2748 "parser.c"
    break;

  case 85: /* instruction: MNEMONIC '[' expression ',' 'X' ']'  */
#line 307 "parser.y"
                                          { (yyval.node) = astnode_create_instruction((yyvsp[-5].mnemonic), PREINDEXED_INDIRECT_MODE, (yyvsp[-3].node), (yyloc)); }
#line 2754 "parser.c"
    break;

  case 86: /* instruction: MNEMONIC '[' expression ']' ',' 'Y'  */
#line 308 "parser.y"
                                          { (yyval.node) = astnode_create_instruction((yyvsp[-5].mnemonic), POSTINDEXED_INDIRECT_MODE, (yyvsp[-3].node), (yyloc)); }
#line 2760 "parser.c"
    break;

  case 87: /* instruction: MNEMONIC '[' expression ']'  */
#line 309 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), INDIRECT_MODE, (yyvsp[-1].node), (yyloc)); }
#line 2766 "parser.c"
    break;

  case 88: /* expression: indexed_identifier  */
#line 313 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2772 "parser.c"
    break;

  case 89: /* expression: SIZEOF sizeof_arg  */
#line 314 "parser.y"
                        { (yyval.node) = astnode_create_sizeof((yyvsp[0].node), (yyloc)); }
#line 2778 "parser.c"
    break;

  case 90: /* expression: scope_access  */
#line 315 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2784 "parser.c"
    break;

  case 91: /* expression: struc_access  */
#line 316 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2790 "parser.c"
    break;

  case 92: /* expression: local_id  */
#line 317 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2796 "parser.c"
    break;

  case 93: /* expression: literal  */
#line 318 "parser.y"
              { (yyval.node) = (yyvsp[0].node); }
#line 2802 "parser.c"
    break;

  case 94: /* expression: '$'  */
#line 319 "parser.y"
          { (yyval.node) = astnode_create_pc((yyloc)); }
#line 2808 "parser.c"
    break;

  case 95: /* expression: arithmetic_expression  */
#line 320 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2814 "parser.c"
    break;

  case 96: /* expression: comparison_expression  */
#line 321 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2820 "parser.c"
    break;

  case 97: /* expression: '(' expression ')'  */
#line 322 "parser.y"
                         { (yyval.node) = (yyvsp[-1].node); }
#line 2826 "parser.c"
    break;

  case 98: /* expression: '+'  */
#line 323 "parser.y"
          { (yyval.node) = astnode_create_forward_branch("+", (yyloc)); }
#line 2832 "parser.c"
    break;

  case 99: /* expression: '-'  */
#line 324 "parser.y"
          { (yyval.node) = astnode_create_backward_branch("-", (yyloc)); }
#line 2838 "parser.c"
    break;

  case 100: /* expression: FORWARD_BRANCH  */
#line 325 "parser.y"
                     { (yyval.node) = astnode_create_forward_branch((yyvsp[0].ident), (yyloc)); }
#line 2844 "parser.c"
    break;

  case 101: /* expression: BACKWARD_BRANCH  */
#line 326 "parser.y"
                      { (yyval.node) = astnode_create_backward_branch((yyvsp[0].ident), (yyloc)); }
#line 2850 "parser.c"
    break;

  case 102: /* expression: MASK scope_access  */
#line 327 "parser.y"
                        { (yyval.node) = astnode_create_mask((yyvsp[0].node), (yyloc)); }
#line 2856 "parser.c"
    break;

  case 103: /* indexed_identifier: identifier  */
#line 331 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2862 "parser.c"
    break;

  case 104: /* indexed_identifier: identifier '[' expression ']'  */
#line 332 "parser.y"
                                    { (yyval.node) = astnode_create_index((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 2868 "parser.c"
    break;

  case 105: /* indexed_identifier: identifier '(' expression ')'  */
#line 333 "parser.y"
                                    { (yyval.node) = astnode_create_index((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 2874 "parser.c"
    break;

  case 106: /* extended_expression: expression  */
#line 337 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2880 "parser.c"
    break;

  case 107: /* extended_expression: struc_initializer  */
#line 338 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2886 "parser.c"
    break;

  case 108: /* sizeof_arg: identifier  */
#line 342 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2892 "parser.c"
    break;

  case 109: /* sizeof_arg: datatype  */
#line 343 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2898 "parser.c"
    break;

  case 110: /* expression_opt: expression  */
#line 347 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2904 "parser.c"
    break;

  case 111: /* expression_opt: %empty  */
#line 348 "parser.y"
      { (yyval.node) = NULL; }
#line 2910 "parser.c"
    break;

  case 112: /* scope_access: identifier SCOPE_OP identifier  */
#line 352 "parser.y"
                                   { (yyval.node) = astnode_create_scope((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2916 "parser.c"
    break;

  case 113: /* struc_access: indexed_identifier '.' struc_access  */
#line 356 "parser.y"
                                        { (yyval.node) = astnode_create_dot((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2922 "parser.c"
    break;

  case 114: /* struc_access: indexed_identifier '.' indexed_identifier  */
#line 357 "parser.y"
                                                { (yyval.node) = astnode_create_dot((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2928 "parser.c"
    break;

  case 115: /* struc_initializer: '{' field_initializer_list_opt '}'  */
#line 361 "parser.y"
                                       { (yyval.node) = astnode_create_struc((yyvsp[-1].node), (yyloc)); }
#line 2934 "parser.c"
    break;

  case 116: /* field_initializer_list_opt: field_initializer_list  */
#line 365 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2940 "parser.c"
    break;

  case 117: /* field_initializer_list_opt: %empty  */
#line 366 "parser.y"
      { (yyval.node) = NULL; }
#line 2946 "parser.c"
    break;

  case 118: /* field_initializer_list: field_initializer  */
#line 370 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2952 "parser.c"
    break;

  case 119: /* field_initializer_list: field_initializer_list ',' field_initializer  */
#line 371 "parser.y"
                                                   { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2958 "parser.c"
    break;

  case 120: /* field_initializer: extended_expression  */
#line 375 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2964 "parser.c"
    break;

  case 121: /* local_id: LOCAL_ID  */
#line 379 "parser.y"
             { (yyval.node) = astnode_create_local_id((yyvsp[0].ident), (yyloc)); }
#line 2970 "parser.c"
    break;

  case 122: /* arithmetic_expression: expression '+' expression  */
#line 383 "parser.y"
                              { (yyval.node) = astnode_create_arithmetic(PLUS_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2976 "parser.c"
    break;

  case 123: /* arithmetic_expression: expression '-' expression  */
#line 384 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MINUS_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2982 "parser.c"
    break;

  case 124: /* arithmetic_expression: expression '*' expression  */
#line 385 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MUL_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2988 "parser.c"
    break;

  case 125: /* arithmetic_expression: expression '/' expression  */
#line 386 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(DIV_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2994 "parser.c"
    break;

  case 126: /* arithmetic_expression: expression '%' expression  */
#line 387 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MOD_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3000 "parser.c"
    break;

  case 127: /* arithmetic_expression: expression '&' expression  */
#line 388 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(AND_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3006 "parser.c"
    break;

  case 128: /* arithmetic_expression: expression '|' expression  */
#line 389 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(OR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3012 "parser.c"
    break;

  case 129: /* arithmetic_expression: expression '^' expression  */
#line 390 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(XOR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3018 "parser.c"
    break;

  case 130: /* arithmetic_expression: expression SHL_OP expression  */
#line 391 "parser.y"
                                   { (yyval.node) = astnode_create_arithmetic(SHL_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3024 "parser.c"
    break;

  case 131: /* arithmetic_expression: expression SHR_OP expression  */
#line 392 "parser.y"
                                   { (yyval.node) = astnode_create_arithmetic(SHR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3030 "parser.c"
    break;

  case 132: /* arithmetic_expression: '~' expression  */
#line 393 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(NEG_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3036 "parser.c"
    break;

  case 133: /* arithmetic_expression: '!' expression  */
#line 394 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(NOT_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3042 "parser.c"
    break;

  case 134: /* arithmetic_expression: '^' identifier  */
#line 395 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(BANK_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3048 "parser.c"
    break;

  case 135: /* arithmetic_expression: '<' expression  */
#line 396 "parser.y"
                                 { (yyval.node) = astnode_create_arithmetic(LO_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3054 "parser.c"
    break;

  case 136: /* arithmetic_expression: '>' expression  */
#line 397 "parser.y"
                                 { (yyval.node) = astnode_create_arithmetic(HI_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3060 "parser.c"
    break;

  case 137: /* arithmetic_expression: '-' expression  */
#line 398 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(UMINUS_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3066 "parser.c"
    break;

  case 138: /* comparison_expression: expression EQ_OP expression  */
#line 402 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(EQ_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3072 "parser.c"
    break;

  case 139: /* comparison_expression: expression NE_OP expression  */
#line 403 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(NE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3078 "parser.c"
    break;

  case 140: /* comparison_expression: expression '>' expression  */
#line 404 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(GT_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3084 "parser.c"
    break;

  case 141: /* comparison_expression: expression '<' expression  */
#line 405 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(LT_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3090 "parser.c"
    break;

  case 142: /* comparison_expression: expression GE_OP expression  */
#line 406 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(GE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3096 "parser.c"
    break;

  case 143: /* comparison_expression: expression LE_OP expression  */
#line 407 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(LE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3102 "parser.c"
    break;

  case 144: /* label: LABEL  */
#line 411 "parser.y"
          { (yyval.node) = astnode_create_label((yyvsp[0].label), NULL, NULL, (yyloc)); }
#line 3108 "parser.c"
    break;

  case 145: /* label: LOCAL_LABEL  */
#line 412 "parser.y"
                  { (yyval.node) = astnode_create_local_label((yyvsp[0].label), (yyloc)); }
#line 3114 "parser.c"
    break;

  case 146: /* label: '+'  */
#line 413 "parser.y"
          { (yyval.node) = astnode_create_forward_branch_decl("+", (yyloc)); }
#line 3120 "parser.c"
    break;

  case 147: /* label: '-'  */
#line 414 "parser.y"
          { (yyval.node) = astnode_create_backward_branch_decl("-", (yyloc)); }
#line 3126 "parser.c"
    break;

  case 148: /* label: FORWARD_BRANCH  */
#line 415 "parser.y"
                     { (yyval.node) = astnode_create_forward_branch_decl((yyvsp[0].ident), (yyloc)); }
#line 3132 "parser.c"
    break;

  case 149: /* label: BACKWARD_BRANCH  */
#line 416 "parser.y"
                      { (yyval.node) = astnode_create_backward_branch_decl((yyvsp[0].ident), (yyloc)); }
#line 3138 "parser.c"
    break;

  case 150: /* identifier: IDENTIFIER  */
#line 420 "parser.y"
               { (yyval.node) = astnode_create_identifier((yyvsp[0].ident), (yyloc)); }
#line 3144 "parser.c"
    break;

  case 151: /* identifier_opt: identifier  */
#line 424 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3150 "parser.c"
    break;

  case 152: /* identifier_opt: %empty  */
#line 425 "parser.y"
      { (yyval.node) = astnode_create_null((yyloc)); }
#line 3156 "parser.c"
    break;

  case 153: /* literal: INTEGER_LITERAL  */
#line 429 "parser.y"
                    { (yyval.node) = astnode_create_integer((yyvsp[0].integer), (yyloc)); }
#line 3162 "parser.c"
    break;

  case 154: /* literal: STRING_LITERAL  */
#line 430 "parser.y"
                     { (yyval.node) = astnode_create_string((yyvsp[0].string), (yyloc)); }
#line 3168 "parser.c"
    break;

  case 155: /* if_statement: IF expression line_tail statement_list_opt elif_statement_list_opt else_part_opt ENDIF line_tail  */
#line 434 "parser.y"
                                                                                                     { (yyval.node) = astnode_create_if((yyvsp[-6].node), (yyvsp[-4].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3174 "parser.c"
    break;

  case 156: /* elif_statement_list_opt: elif_statement_list  */
#line 438 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3180 "parser.c"
    break;

  case 157: /* elif_statement_list_opt: %empty  */
#line 439 "parser.y"
      { (yyval.node) = NULL; }
#line 3186 "parser.c"
    break;

  case 158: /* elif_statement_list: elif_statement  */
#line 443 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 3192 "parser.c"
    break;

  case 159: /* elif_statement_list: elif_statement_list elif_statement  */
#line 444 "parser.y"
                                         { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3198 "parser.c"
    break;

  case 160: /* elif_statement: ELIF expression line_tail statement_list_opt  */
#line 448 "parser.y"
                                                 { (yyval.node) = astnode_create_case((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3204 "parser.c"
    break;

  case 161: /* else_part_opt: ELSE line_tail statement_list_opt  */
#line 452 "parser.y"
                                      { (yyval.node) = (yyvsp[0].node); }
#line 3210 "parser.c"
    break;

  case 162: /* else_part_opt: %empty  */
#line 453 "parser.y"
      { (yyval.node) = NULL; }
#line 3216 "parser.c"
    break;

  case 163: /* ifdef_statement: IFDEF identifier line_tail statement_list_opt else_part_opt ENDIF line_tail  */
#line 457 "parser.y"
                                                                                { (yyval.node) = astnode_create_ifdef((yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3222 "parser.c"
    break;

  case 164: /* ifndef_statement: IFNDEF identifier line_tail statement_list_opt else_part_opt ENDIF line_tail  */
#line 461 "parser.y"
                                                                                 { (yyval.node) = astnode_create_ifndef((yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3228 "parser.c"
    break;

  case 165: /* data_statement: named_data_statement line_tail  */
#line 465 "parser.y"
                                   { (yyval.node) = (yyvsp[-1].node); }
#line 3234 "parser.c"
    break;

  case 166: /* data_statement: unnamed_data_statement line_tail  */
#line 466 "parser.y"
                                       { (yyval.node) = (yyvsp[-1].node); }
#line 3240 "parser.c"
    break;

  case 167: /* named_data_statement: identifier unnamed_data_statement  */
#line 470 "parser.y"
                                      { (yyval.node) = astnode_create_var_decl(0, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3246 "parser.c"
    break;

  case 168: /* named_data_statement: ZEROPAGE identifier unnamed_data_statement  */
#line 471 "parser.y"
                                                 { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3252 "parser.c"
    break;

  case 169: /* named_data_statement: PUBLIC identifier unnamed_data_statement  */
#line 472 "parser.y"
                                               { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3258 "parser.c"
    break;

  case 170: /* named_data_statement: ZEROPAGE PUBLIC identifier unnamed_data_statement  */
#line 473 "parser.y"
                                                        { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG | PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3264 "parser.c"
    break;

  case 171: /* named_data_statement: PUBLIC ZEROPAGE identifier unnamed_data_statement  */
#line 474 "parser.y"
                                                        { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG | ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3270 "parser.c"
    break;

  case 172: /* unnamed_data_statement: datatype expression_list  */
#line 478 "parser.y"
                             { (yyval.node) = astnode_create_data((yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3276 "parser.c"
    break;

  case 173: /* unnamed_data_statement: datatype  */
#line 479 "parser.y"
               { (yyval.node) = astnode_create_storage((yyvsp[0].node), NULL, (yyloc)); }
#line 3282 "parser.c"
    break;

  case 174: /* unnamed_data_statement: datatype '[' expression ']'  */
#line 480 "parser.y"
                                  { (yyval.node) = astnode_create_storage((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3288 "parser.c"
    break;

  case 175: /* datatype: BYTE  */
#line 484 "parser.y"
         { (yyval.node) = astnode_create_datatype(BYTE_DATATYPE, NULL, (yyloc)); }
#line 3294 "parser.c"
    break;

  case 176: /* datatype: CHAR  */
#line 485 "parser.y"
           { (yyval.node) = astnode_create_datatype(CHAR_DATATYPE, NULL, (yyloc)); }
#line 3300 "parser.c"
    break;

  case 177: /* datatype: WORD  */
#line 486 "parser.y"
           { (yyval.node) = astnode_create_datatype(WORD_DATATYPE, NULL, (yyloc)); }
#line 3306 "parser.c"
    break;

  case 178: /* datatype: DWORD  */
#line 487 "parser.y"
            { (yyval.node) = astnode_create_datatype(DWORD_DATATYPE, NULL, (yyloc)); }
#line 3312 "parser.c"
    break;

  case 179: /* datatype: TAG identifier  */
#line 488 "parser.y"
                     { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3318 "parser.c"
    break;

  case 180: /* datatype: '.' identifier  */
#line 489 "parser.y"
                     { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3324 "parser.c"
    break;

  case 181: /* expression_list: extended_expression  */
#line 493 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3330 "parser.c"
    break;

  case 182: /* expression_list: expression_list ',' extended_expression  */
#line 494 "parser.y"
                                              { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3336 "parser.c"
    break;

  case 183: /* incsrc_statement: INCSRC file_specifier line_tail  */
#line 498 "parser.y"
                                    { (yyval.node) = astnode_create_incsrc((yyvsp[-1].node), (yyloc)); handle_incsrc((yyval.node)); }
#line 3342 "parser.c"
    break;

  case 184: /* incbin_statement: INCBIN file_specifier line_tail  */
#line 502 "parser.y"
                                    { (yyval.node) = astnode_create_incbin((yyvsp[-1].node), (yyloc)); handle_incbin((yyval.node)); }
#line 3348 "parser.c"
    break;

  case 185: /* file_specifier: STRING_LITERAL  */
#line 506 "parser.y"
                   { (yyval.node) = astnode_create_string((yyvsp[0].string), (yyloc)); }
#line 3354 "parser.c"
    break;

  case 186: /* file_specifier: '<'  */
#line 507 "parser.y"
          { (yyval.node) = astnode_create_file_path(scan_include('>'), (yyloc)); }
#line 3360 "parser.c"
    break;

  case 187: /* macro_decl_statement: MACRO identifier param_list_opt line_tail statement_list_opt ENDM line_tail  */
#line 511 "parser.y"
                                                                                { (yyval.node) = astnode_create_macro_decl((yyvsp[-5].node), (yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 3366 "parser.c"
    break;

  case 188: /* param_list_opt: identifier_list  */
#line 515 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 3372 "parser.c"
    break;

  case 189: /* param_list_opt: %empty  */
#line 516 "parser.y"
      { (yyval.node) = NULL; }
#line 3378 "parser.c"
    break;

  case 190: /* macro_statement: identifier arg_list_opt line_tail  */
#line 520 "parser.y"
                                      { (yyval.node) = astnode_create_macro((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3384 "parser.c"
    break;

  case 191: /* arg_list_opt: expression_list  */
#line 524 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 3390 "parser.c"
    break;

  case 192: /* arg_list_opt: %empty  */
#line 525 "parser.y"
      { (yyval.node) = NULL; }
#line 3396 "parser.c"
    break;

  case 193: /* identifier_list: identifier  */
#line 529 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3402 "parser.c"
    break;

  case 194: /* identifier_list: identifier_list ',' identifier  */
#line 530 "parser.y"
                                     { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3408 "parser.c"
    break;

  case 195: /* equ_statement: identifier EQU extended_expression line_tail  */
#line 534 "parser.y"
                                                 { (yyval.node) = astnode_create_equ((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3414 "parser.c"
    break;

  case 196: /* undef_statement: UNDEF identifier line_tail  */
#line 538 "parser.y"
                               { (yyval.node) = astnode_create_undef((yyvsp[-1].node), (yyloc)); }
#line 3420 "parser.c"
    break;

  case 197: /* assign_statement: identifier '=' extended_expression line_tail  */
#line 542 "parser.y"
                                                 { (yyval.node) = astnode_create_assign((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3426 "parser.c"
    break;

  case 198: /* define_statement: DEFINE identifier line_tail  */
#line 546 "parser.y"
                                { (yyval.node) = astnode_create_equ((yyvsp[-1].node), astnode_create_integer(0, (yyloc)), (yyloc)); }
#line 3432 "parser.c"
    break;

  case 199: /* define_statement: DEFINE identifier extended_expression line_tail  */
#line 547 "parser.y"
                                                      { (yyval.node) = astnode_create_equ((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3438 "parser.c"
    break;

  case 200: /* public_statement: PUBLIC identifier_list line_tail  */
#line 551 "parser.y"
                                     { (yyval.node) = astnode_create_public((yyvsp[-1].node), (yyloc)); }
#line 3444 "parser.c"
    break;

  case 201: /* extrn_statement: EXTRN identifier_list ':' symbol_type from_part_opt line_tail  */
#line 555 "parser.y"
                                                                  { (yyval.node) = astnode_create_extrn((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3450 "parser.c"
    break;

  case 202: /* from_part_opt: '@' identifier  */
#line 559 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 3456 "parser.c"
    break;

  case 203: /* from_part_opt: %empty  */
#line 560 "parser.y"
      { (yyval.node) = NULL; }
#line 3462 "parser.c"
    break;

  case 204: /* symbol_type: datatype  */
#line 564 "parser.y"
             { (yyval.node) = (yyvsp[0].node); }
#line 3468 "parser.c"
    break;

  case 205: /* symbol_type: identifier  */
#line 565 "parser.y"
                 { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3474 "parser.c"
    break;

  case 206: /* symbol_type: PROC  */
#line 566 "parser.y"
           { (yyval.node) = astnode_create_integer(PROC_SYMBOL, (yyloc)); }
#line 3480 "parser.c"
    break;

  case 207: /* symbol_type: _LABEL_  */
#line 567 "parser.y"
              { (yyval.node) = astnode_create_integer(LABEL_SYMBOL, (yyloc)); }
#line 3486 "parser.c"
    break;

  case 208: /* storage_statement: named_storage_statement  */
#line 571 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 3492 "parser.c"
    break;

  case 209: /* storage_statement: unnamed_storage_statement  */
#line 572 "parser.y"
                                { (yyval.node) = (yyvsp[0].node); }
#line 3498 "parser.c"
    break;

  case 210: /* named_storage_statement: identifier unnamed_storage_statement  */
#line 576 "parser.y"
                                         { (yyval.node) = astnode_create_var_decl(0, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3504 "parser.c"
    break;

  case 211: /* named_storage_statement: ZEROPAGE identifier unnamed_storage_statement  */
#line 577 "parser.y"
                                                    { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3510 "parser.c"
    break;

  case 212: /* named_storage_statement: PUBLIC identifier unnamed_storage_statement  */
#line 578 "parser.y"
                                                  { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3516 "parser.c"
    break;

  case 213: /* named_storage_statement: ZEROPAGE PUBLIC identifier unnamed_storage_statement  */
#line 579 "parser.y"
                                                           { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG | PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3522 "parser.c"
    break;

  case 214: /* named_storage_statement: PUBLIC ZEROPAGE identifier unnamed_storage_statement  */
#line 580 "parser.y"
                                                           { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG | ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3528 "parser.c"
    break;

  case 215: /* unnamed_storage_statement: storage expression_opt line_tail  */
#line 584 "parser.y"
                                     { (yyval.node) = astnode_create_storage((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3534 "parser.c"
    break;

  case 216: /* storage: DSB  */
#line 588 "parser.y"
        { (yyval.node) = astnode_create_datatype(BYTE_DATATYPE, NULL, (yyloc)); }
#line 3540 "parser.c"
    break;

  case 217: /* storage: DSW  */
#line 589 "parser.y"
          { (yyval.node) = astnode_create_datatype(WORD_DATATYPE, NULL, (yyloc)); }
#line 3546 "parser.c"
    break;

  case 218: /* storage: DSD  */
#line 590 "parser.y"
          { (yyval.node) = astnode_create_datatype(DWORD_DATATYPE, NULL, (yyloc)); }
#line 3552 "parser.c"
    break;


#line 3556 "parser.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 592 "parser.y"

/**
 * Takes care of switching to a new scanner input stream when a "incsrc" statement
 * has been encountered.
 * The current stream is pushed on a stack, and will be popped when EOF is reached
 * in the new stream.
 * @param n A node of type INCSRC_NODE
 */
void handle_incsrc(astnode *n)
{
    char errs[512];
    /* Get the node which describes the file to include */
    astnode *file = astnode_get_child(n, 0);
    int quoted_form = (astnode_get_type(file) == STRING_NODE) ? 1 : 0;
    switch (yypushandrestart(file->string, quoted_form)) {
        case 0:
        /* Success */
        break;
        case 1:
        /* Failed to open file */
        snprintf(errs, sizeof(errs), "could not open '%s' for reading", file->string);
        yyerror(errs);
        break;
        case 2:
        /* Stack overflow */
        yyerror("Maximum include nesting level reached");
        break;
    }
}

// TODO: This shouldn't be done here but rather in astproc module.

FILE *open_included_file(const char *, int, char **);

/**
 * Takes care of including the binary contents of the file specified by a parsed
 * "incbin" statement.
 * Calls yyerror() if the file can't be included for some reason.
 * @param n A node of type INCBIN_NODE
 */
void handle_incbin(astnode *n)
{
    FILE *fp;
    unsigned char *data;
    int size;
    long file_size;
    char errs[512];
    /* Get the node which describes the file to include */
    astnode *file = astnode_get_child(n, 0);
    const char *filename = file->string;
    int quoted_form = (astnode_get_type(file) == STRING_NODE) ? 1 : 0;
    /* Try to open it */
    fp = open_included_file(filename, quoted_form, NULL);
    if (fp) {
        /* Get filesize */
        if (fseek(fp, 0, SEEK_END) != 0) {
            snprintf(errs, sizeof(errs), "could not read '%s'", file->string);
            yyerror(errs);
        } else {
            file_size = ftell(fp);
            if (file_size < 0) {
                snprintf(errs, sizeof(errs), "could not read '%s'", file->string);
                yyerror(errs);
            } else if (file_size > INT_MAX) {
                snprintf(errs, sizeof(errs), "file '%s' is too large to include", file->string);
                yyerror(errs);
            } else {
                size = (int)file_size;
                rewind(fp);
                if (size > 0) {
                    size_t bytes_read;
                    /* Allocate buffer to hold file contents */
                    data = (unsigned char *)malloc((size_t)size);
                    if (data == NULL) {
                        yyerror("out of memory while processing INCBIN");
                    } else {
                        /* Read file contents into buffer */
                        bytes_read = fread(data, 1, (size_t)size, fp);
                        if (bytes_read != (size_t)size) {
                            free(data);
                            snprintf(errs, sizeof(errs), "could not read '%s'", file->string);
                            yyerror(errs);
                        } else {
                            /* Insert binary node */
                            astnode_add_sibling(n, astnode_create_binary(data, size, n->loc) );
                        }
                    }
                }
            }
        }
        /* Close file */
        fclose(fp);
    } else {
        /* Couldn't open file */
        snprintf(errs, sizeof(errs), "could not open '%s' for reading", file->string);
        yyerror(errs);
    }
}
