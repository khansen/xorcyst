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
  YYSYMBOL_DO = 34,                        /* DO  */
  YYSYMBOL_UNTIL = 35,                     /* UNTIL  */
  YYSYMBOL_ENDM = 36,                      /* ENDM  */
  YYSYMBOL_EXITM = 37,                     /* EXITM  */
  YYSYMBOL_ALIGN = 38,                     /* ALIGN  */
  YYSYMBOL_EQU = 39,                       /* EQU  */
  YYSYMBOL_UNDEF = 40,                     /* UNDEF  */
  YYSYMBOL_DEFINE = 41,                    /* DEFINE  */
  YYSYMBOL_END = 42,                       /* END  */
  YYSYMBOL_PUBLIC = 43,                    /* PUBLIC  */
  YYSYMBOL_EXTRN = 44,                     /* EXTRN  */
  YYSYMBOL_CHARMAP = 45,                   /* CHARMAP  */
  YYSYMBOL_STRUC = 46,                     /* STRUC  */
  YYSYMBOL_UNION = 47,                     /* UNION  */
  YYSYMBOL_ENDS = 48,                      /* ENDS  */
  YYSYMBOL_RECORD = 49,                    /* RECORD  */
  YYSYMBOL_ENUM = 50,                      /* ENUM  */
  YYSYMBOL_ENDE = 51,                      /* ENDE  */
  YYSYMBOL_PROC = 52,                      /* PROC  */
  YYSYMBOL_ENDP = 53,                      /* ENDP  */
  YYSYMBOL_SIZEOF = 54,                    /* SIZEOF  */
  YYSYMBOL_MASK = 55,                      /* MASK  */
  YYSYMBOL_TAG = 56,                       /* TAG  */
  YYSYMBOL_MESSAGE = 57,                   /* MESSAGE  */
  YYSYMBOL_WARNING = 58,                   /* WARNING  */
  YYSYMBOL_ERROR = 59,                     /* ERROR  */
  YYSYMBOL_ZEROPAGE = 60,                  /* ZEROPAGE  */
  YYSYMBOL_ORG = 61,                       /* ORG  */
  YYSYMBOL_62_n_ = 62,                     /* '\n'  */
  YYSYMBOL_63_ = 63,                       /* ':'  */
  YYSYMBOL_64_ = 64,                       /* '@'  */
  YYSYMBOL_SCOPE_OP = 65,                  /* SCOPE_OP  */
  YYSYMBOL_66_A_ = 66,                     /* 'A'  */
  YYSYMBOL_67_X_ = 67,                     /* 'X'  */
  YYSYMBOL_68_Y_ = 68,                     /* 'Y'  */
  YYSYMBOL_69_ = 69,                       /* '='  */
  YYSYMBOL_70_ = 70,                       /* '$'  */
  YYSYMBOL_71_ = 71,                       /* '{'  */
  YYSYMBOL_72_ = 72,                       /* '}'  */
  YYSYMBOL_73_ = 73,                       /* '['  */
  YYSYMBOL_74_ = 74,                       /* ']'  */
  YYSYMBOL_75_ = 75,                       /* ','  */
  YYSYMBOL_76_ = 76,                       /* '.'  */
  YYSYMBOL_77_ = 77,                       /* '#'  */
  YYSYMBOL_LO_OP = 78,                     /* LO_OP  */
  YYSYMBOL_HI_OP = 79,                     /* HI_OP  */
  YYSYMBOL_80_ = 80,                       /* '|'  */
  YYSYMBOL_81_ = 81,                       /* '^'  */
  YYSYMBOL_82_ = 82,                       /* '&'  */
  YYSYMBOL_EQ_OP = 83,                     /* EQ_OP  */
  YYSYMBOL_NE_OP = 84,                     /* NE_OP  */
  YYSYMBOL_LE_OP = 85,                     /* LE_OP  */
  YYSYMBOL_GE_OP = 86,                     /* GE_OP  */
  YYSYMBOL_87_ = 87,                       /* '>'  */
  YYSYMBOL_88_ = 88,                       /* '<'  */
  YYSYMBOL_SHL_OP = 89,                    /* SHL_OP  */
  YYSYMBOL_SHR_OP = 90,                    /* SHR_OP  */
  YYSYMBOL_91_ = 91,                       /* '+'  */
  YYSYMBOL_92_ = 92,                       /* '-'  */
  YYSYMBOL_UMINUS = 93,                    /* UMINUS  */
  YYSYMBOL_94_ = 94,                       /* '*'  */
  YYSYMBOL_95_ = 95,                       /* '/'  */
  YYSYMBOL_96_ = 96,                       /* '%'  */
  YYSYMBOL_97_ = 97,                       /* '!'  */
  YYSYMBOL_98_ = 98,                       /* '~'  */
  YYSYMBOL_99_ = 99,                       /* '('  */
  YYSYMBOL_100_ = 100,                     /* ')'  */
  YYSYMBOL_YYACCEPT = 101,                 /* $accept  */
  YYSYMBOL_assembly_unit = 102,            /* assembly_unit  */
  YYSYMBOL_end_opt = 103,                  /* end_opt  */
  YYSYMBOL_statement_list = 104,           /* statement_list  */
  YYSYMBOL_statement_list_opt = 105,       /* statement_list_opt  */
  YYSYMBOL_labelable_statement = 106,      /* labelable_statement  */
  YYSYMBOL_statement = 107,                /* statement  */
  YYSYMBOL_org_statement = 108,            /* org_statement  */
  YYSYMBOL_align_statement = 109,          /* align_statement  */
  YYSYMBOL_warning_statement = 110,        /* warning_statement  */
  YYSYMBOL_error_statement = 111,          /* error_statement  */
  YYSYMBOL_message_statement = 112,        /* message_statement  */
  YYSYMBOL_label_statement = 113,          /* label_statement  */
  YYSYMBOL_label_addr_part_opt = 114,      /* label_addr_part_opt  */
  YYSYMBOL_label_type_part_opt = 115,      /* label_type_part_opt  */
  YYSYMBOL_while_statement = 116,          /* while_statement  */
  YYSYMBOL_rept_statement = 117,           /* rept_statement  */
  YYSYMBOL_do_statement = 118,             /* do_statement  */
  YYSYMBOL_proc_statement = 119,           /* proc_statement  */
  YYSYMBOL_struc_decl_statement = 120,     /* struc_decl_statement  */
  YYSYMBOL_union_decl_statement = 121,     /* union_decl_statement  */
  YYSYMBOL_enum_decl_statement = 122,      /* enum_decl_statement  */
  YYSYMBOL_enum_item_list = 123,           /* enum_item_list  */
  YYSYMBOL_enum_item = 124,                /* enum_item  */
  YYSYMBOL_record_decl_statement = 125,    /* record_decl_statement  */
  YYSYMBOL_record_field_list = 126,        /* record_field_list  */
  YYSYMBOL_record_field = 127,             /* record_field  */
  YYSYMBOL_charmap_statement = 128,        /* charmap_statement  */
  YYSYMBOL_dataseg_statement = 129,        /* dataseg_statement  */
  YYSYMBOL_codeseg_statement = 130,        /* codeseg_statement  */
  YYSYMBOL_null_statement = 131,           /* null_statement  */
  YYSYMBOL_label_decl = 132,               /* label_decl  */
  YYSYMBOL_line_tail = 133,                /* line_tail  */
  YYSYMBOL_newline = 134,                  /* newline  */
  YYSYMBOL_instruction_statement = 135,    /* instruction_statement  */
  YYSYMBOL_instruction = 136,              /* instruction  */
  YYSYMBOL_expression = 137,               /* expression  */
  YYSYMBOL_indexed_identifier = 138,       /* indexed_identifier  */
  YYSYMBOL_extended_expression = 139,      /* extended_expression  */
  YYSYMBOL_sizeof_arg = 140,               /* sizeof_arg  */
  YYSYMBOL_expression_opt = 141,           /* expression_opt  */
  YYSYMBOL_scope_access = 142,             /* scope_access  */
  YYSYMBOL_struc_access = 143,             /* struc_access  */
  YYSYMBOL_struc_initializer = 144,        /* struc_initializer  */
  YYSYMBOL_field_initializer_list_opt = 145, /* field_initializer_list_opt  */
  YYSYMBOL_field_initializer_list = 146,   /* field_initializer_list  */
  YYSYMBOL_field_initializer = 147,        /* field_initializer  */
  YYSYMBOL_local_id = 148,                 /* local_id  */
  YYSYMBOL_arithmetic_expression = 149,    /* arithmetic_expression  */
  YYSYMBOL_comparison_expression = 150,    /* comparison_expression  */
  YYSYMBOL_label = 151,                    /* label  */
  YYSYMBOL_identifier = 152,               /* identifier  */
  YYSYMBOL_identifier_opt = 153,           /* identifier_opt  */
  YYSYMBOL_literal = 154,                  /* literal  */
  YYSYMBOL_if_statement = 155,             /* if_statement  */
  YYSYMBOL_elif_statement_list_opt = 156,  /* elif_statement_list_opt  */
  YYSYMBOL_elif_statement_list = 157,      /* elif_statement_list  */
  YYSYMBOL_elif_statement = 158,           /* elif_statement  */
  YYSYMBOL_else_part_opt = 159,            /* else_part_opt  */
  YYSYMBOL_ifdef_statement = 160,          /* ifdef_statement  */
  YYSYMBOL_ifndef_statement = 161,         /* ifndef_statement  */
  YYSYMBOL_data_statement = 162,           /* data_statement  */
  YYSYMBOL_named_data_statement = 163,     /* named_data_statement  */
  YYSYMBOL_unnamed_data_statement = 164,   /* unnamed_data_statement  */
  YYSYMBOL_datatype = 165,                 /* datatype  */
  YYSYMBOL_expression_list = 166,          /* expression_list  */
  YYSYMBOL_incsrc_statement = 167,         /* incsrc_statement  */
  YYSYMBOL_incbin_statement = 168,         /* incbin_statement  */
  YYSYMBOL_file_specifier = 169,           /* file_specifier  */
  YYSYMBOL_macro_decl_statement = 170,     /* macro_decl_statement  */
  YYSYMBOL_param_list_opt = 171,           /* param_list_opt  */
  YYSYMBOL_macro_statement = 172,          /* macro_statement  */
  YYSYMBOL_exitm_statement = 173,          /* exitm_statement  */
  YYSYMBOL_arg_list_opt = 174,             /* arg_list_opt  */
  YYSYMBOL_identifier_list = 175,          /* identifier_list  */
  YYSYMBOL_equ_statement = 176,            /* equ_statement  */
  YYSYMBOL_undef_statement = 177,          /* undef_statement  */
  YYSYMBOL_assign_statement = 178,         /* assign_statement  */
  YYSYMBOL_define_statement = 179,         /* define_statement  */
  YYSYMBOL_public_statement = 180,         /* public_statement  */
  YYSYMBOL_extrn_statement = 181,          /* extrn_statement  */
  YYSYMBOL_from_part_opt = 182,            /* from_part_opt  */
  YYSYMBOL_symbol_type = 183,              /* symbol_type  */
  YYSYMBOL_storage_statement = 184,        /* storage_statement  */
  YYSYMBOL_named_storage_statement = 185,  /* named_storage_statement  */
  YYSYMBOL_unnamed_storage_statement = 186, /* unnamed_storage_statement  */
  YYSYMBOL_storage = 187                   /* storage  */
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
#define YYFINAL  169
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1800

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  101
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  87
/* YYNRULES -- Number of rules.  */
#define YYNRULES  224
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  399

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   326


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
      62,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    97,     2,    77,    70,    96,    82,     2,
      99,   100,    94,    91,    75,    92,    76,    95,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    63,     2,
      88,    69,    87,     2,    64,    66,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    67,    68,
       2,    73,     2,    74,    81,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    71,    80,    72,    98,     2,     2,     2,
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
      55,    56,    57,    58,    59,    60,    61,    65,    78,    79,
      83,    84,    85,    86,    89,    90,    93
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   123,   123,   127,   128,   132,   133,   140,   141,   145,
     146,   150,   151,   152,   153,   154,   155,   156,   157,   158,
     159,   160,   161,   162,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   184,   185,   189,   193,   197,
     201,   205,   209,   213,   214,   218,   219,   223,   227,   231,
     235,   239,   243,   247,   251,   252,   256,   257,   261,   265,
     266,   270,   274,   278,   279,   282,   286,   290,   294,   295,
     299,   303,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,   333,   334,   338,   339,   340,
     344,   345,   349,   350,   351,   355,   356,   360,   364,   365,
     369,   373,   374,   378,   379,   383,   387,   391,   392,   393,
     394,   395,   396,   397,   398,   399,   400,   401,   402,   403,
     404,   405,   406,   410,   411,   412,   413,   414,   415,   419,
     420,   421,   422,   423,   424,   428,   432,   433,   437,   438,
     442,   446,   447,   451,   452,   456,   460,   461,   465,   469,
     473,   474,   478,   479,   480,   481,   482,   486,   487,   488,
     492,   493,   494,   495,   496,   497,   501,   502,   506,   510,
     514,   515,   519,   523,   524,   528,   532,   536,   537,   541,
     542,   546,   550,   554,   558,   559,   563,   567,   571,   572,
     576,   577,   578,   579,   583,   584,   588,   589,   590,   591,
     592,   596,   600,   601,   602
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
  "INCSRC", "INCBIN", "MACRO", "REPT", "WHILE", "DO", "UNTIL", "ENDM",
  "EXITM", "ALIGN", "EQU", "UNDEF", "DEFINE", "END", "PUBLIC", "EXTRN",
  "CHARMAP", "STRUC", "UNION", "ENDS", "RECORD", "ENUM", "ENDE", "PROC",
  "ENDP", "SIZEOF", "MASK", "TAG", "MESSAGE", "WARNING", "ERROR",
  "ZEROPAGE", "ORG", "'\\n'", "':'", "'@'", "SCOPE_OP", "'A'", "'X'",
  "'Y'", "'='", "'$'", "'{'", "'}'", "'['", "']'", "','", "'.'", "'#'",
  "LO_OP", "HI_OP", "'|'", "'^'", "'&'", "EQ_OP", "NE_OP", "LE_OP",
  "GE_OP", "'>'", "'<'", "SHL_OP", "SHR_OP", "'+'", "'-'", "UMINUS", "'*'",
  "'/'", "'%'", "'!'", "'~'", "'('", "')'", "$accept", "assembly_unit",
  "end_opt", "statement_list", "statement_list_opt", "labelable_statement",
  "statement", "org_statement", "align_statement", "warning_statement",
  "error_statement", "message_statement", "label_statement",
  "label_addr_part_opt", "label_type_part_opt", "while_statement",
  "rept_statement", "do_statement", "proc_statement",
  "struc_decl_statement", "union_decl_statement", "enum_decl_statement",
  "enum_item_list", "enum_item", "record_decl_statement",
  "record_field_list", "record_field", "charmap_statement",
  "dataseg_statement", "codeseg_statement", "null_statement", "label_decl",
  "line_tail", "newline", "instruction_statement", "instruction",
  "expression", "indexed_identifier", "extended_expression", "sizeof_arg",
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
  "macro_statement", "exitm_statement", "arg_list_opt", "identifier_list",
  "equ_statement", "undef_statement", "assign_statement",
  "define_statement", "public_statement", "extrn_statement",
  "from_part_opt", "symbol_type", "storage_statement",
  "named_storage_statement", "unnamed_storage_statement", "storage", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-302)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-9)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    1583,    -4,  -302,  -302,  -302,  -302,  -302,   243,     8,  -302,
    -302,  -302,  -302,  -302,  -302,  -302,   -31,    -4,   706,     8,
       8,     0,     0,     8,   706,   706,    -4,    -4,     8,     8,
       8,    11,     8,     0,     8,     8,     8,     8,     8,     8,
     706,   706,   706,    12,   706,  -302,  -302,     8,  -302,  -302,
      19,   800,  -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,
    -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,
    -302,  -302,  1648,  -302,  -302,  -302,    -4,  -302,   554,  -302,
    -302,  -302,  -302,    -4,    -4,   423,  -302,  -302,  -302,  -302,
    -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,
     706,  -302,  -302,  -302,  -302,  -302,  -302,    30,     8,  -302,
    -302,   706,   706,     8,   706,   706,  -302,   220,   706,   706,
     706,   683,   -46,  -302,  -302,  -302,  -302,  -302,     5,  -302,
     -36,    -4,  -302,  -302,   371,    -4,    -4,  -302,  -302,    -4,
      -4,     8,   371,   371,  1148,  -302,  -302,   595,    -4,   436,
       8,   101,    10,   -52,    -4,    -4,  -302,    -4,     8,    -4,
      -4,  -302,   371,   371,   371,     8,   101,   371,  -302,  -302,
      -4,  -302,  -302,  -302,  -302,   609,   609,   609,  1704,  -302,
    -302,  -302,   -34,    -4,  -302,  -302,  -302,   706,   -34,  1704,
      -4,  -302,    46,  -302,  -302,  -302,  -302,   -16,   642,  1704,
    -302,  1704,  1704,     1,  -302,  -302,  1662,    32,   706,   706,
     706,   706,   706,   706,   706,   706,   706,   706,   706,   706,
     706,   706,   706,   706,     8,     8,   706,   706,   706,    -7,
    -302,   974,  1061,  1061,  -302,  -302,    -4,   -21,  1235,  1235,
     887,    34,     8,   371,  -302,  -302,    -4,   101,  -302,  -302,
    -302,    51,  -302,  1322,  1322,    14,  -302,    18,     8,  1409,
    -302,  -302,  -302,   101,  -302,  -302,  -302,  -302,    -4,    -4,
    -302,    15,     9,  -302,   609,  -302,   661,  -302,   -26,    33,
      26,  -302,  -302,  -302,   197,   783,   870,   170,   170,   254,
     254,   254,   254,   118,   118,     1,     1,  -302,  -302,  -302,
     -46,  -302,   -64,  -302,  1645,  1683,  1704,    51,    -4,    78,
      83,    83,  1235,    75,    77,   706,  -302,  -302,  -302,  -302,
    -302,  -302,  -302,  -302,  -302,    50,    76,    80,     8,  -302,
     706,     2,  -302,    29,  -302,    70,  -302,  -302,  -302,  -302,
    -302,   609,  -302,  -302,  -302,    58,    57,  -302,  -302,  -302,
    -302,   706,    83,    78,  -302,    -4,   104,   108,   107,    -4,
      -4,   371,     8,    -4,    -4,    -4,  -302,  1704,    -4,  -302,
    -302,    -4,  -302,  -302,  -302,   371,   112,  -302,  1496,    -4,
      -4,    -4,  -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,
    -302,   974,    -4,  -302,  -302,  -302,  -302,  -302,  -302
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,   155,   153,   154,   149,   150,    82,     0,   180,
     181,   182,   183,   222,   223,   224,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   157,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    80,    79,     0,   151,   152,
       0,     0,     5,    10,    44,    43,    40,    41,    39,    38,
      36,    35,    37,    34,    30,    31,    32,    33,    29,    27,
      28,    45,     0,    76,    78,    24,     0,    77,   198,    11,
      12,    13,    25,     0,     0,   178,    17,    18,    14,    15,
      16,    19,    20,    21,    42,    22,    23,    26,   214,   215,
     116,    46,   158,   159,   126,   104,   105,     0,     0,    83,
      98,     0,     0,     0,     0,     0,   102,   103,     0,     0,
       0,    85,    91,    94,    95,    96,    99,   100,   107,    97,
      54,     0,    73,    75,     0,     0,     0,   190,   191,     0,
       0,   194,     0,     0,     0,   196,   199,     0,     0,     0,
       0,   199,     0,     0,     0,     0,   156,     0,     0,     0,
       0,   184,     0,     0,     0,     0,     0,     0,   185,     1,
       0,     2,     6,     9,    81,     0,     0,   122,   110,   186,
     111,   172,   197,     0,   216,   170,   171,     0,   177,   115,
       0,   114,     0,    92,   112,   113,   106,     0,     0,    84,
     139,   141,   140,   142,   138,   137,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    56,
      74,     0,     0,     0,   188,   189,     0,   193,     0,     0,
       0,     0,     0,     0,   202,   204,     0,     0,   174,   218,
     206,     0,    72,     0,     0,     0,    69,     0,     0,     0,
      51,    49,    50,     0,   173,   217,    47,     3,     0,     0,
     125,     0,   121,   123,     0,   195,     0,   221,     0,    90,
       0,   101,    86,    87,   133,   134,   132,   143,   144,   148,
     147,   145,   146,   135,   136,   127,   128,   129,   130,   131,
     119,   118,   107,   117,     0,     0,    53,     0,     0,   162,
     167,   167,     0,     0,     0,     0,   200,    48,   205,   176,
     220,   213,   212,   211,   210,   209,     0,     0,     0,    68,
       0,     0,    64,     0,    66,     0,   175,   219,   201,   203,
     120,     0,   187,   179,    93,     0,     0,   108,   109,    55,
      52,     0,   167,   161,   163,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    70,    71,     0,    65,
      67,     0,   124,    89,    88,     0,     0,   164,     0,     0,
       0,     0,    58,    57,    59,   208,   207,    61,    62,    63,
      60,     0,     0,   166,   168,   169,   192,   165,   160
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -302,  -302,  -302,   144,  -211,   -50,    73,  -302,  -302,  -302,
    -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,  -302,
    -302,  -302,  -302,  -185,  -302,  -302,  -178,  -302,  -302,  -302,
    -302,  -302,    -1,  -302,  -302,  -302,   369,   -69,  -137,   -40,
    -302,    52,   -59,  -302,  -302,  -302,  -173,  -302,  -302,  -302,
    -302,   164,  -302,  -302,  -302,  -302,  -302,  -183,  -301,  -302,
    -302,  -302,  -302,   -72,  -102,    86,  -302,  -302,    -9,  -302,
    -302,  -302,  -302,  -302,   -29,  -302,  -302,  -251,  -302,  -302,
    -302,  -302,  -134,  -302,  -302,   -41,  -302
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    50,   171,   240,   241,    52,    53,    54,    55,    56,
      57,    58,    59,   229,   308,    60,    61,    62,    63,    64,
      65,    66,   331,   332,    67,   255,   256,    68,    69,    70,
      71,    72,    73,    74,    75,    76,   178,   122,   179,   193,
     190,   123,   124,   180,   271,   272,   273,   125,   126,   127,
      77,   128,   157,   129,    79,   352,   353,   354,   356,    80,
      81,    82,    83,    84,    85,   182,    86,    87,   139,    88,
     236,    89,    90,   183,   147,    91,    92,    93,    94,    95,
      96,   363,   325,    97,    98,    99,   100
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     101,   172,   152,   153,   137,   195,   181,   334,     2,   226,
     357,   251,   246,   140,     2,   132,   133,     2,     2,   169,
     309,   310,   311,   242,   154,   144,   145,   313,   314,   131,
     224,    45,    46,   228,   191,   227,     2,   184,   268,   269,
     270,   274,   326,   327,     9,    10,    11,    12,   335,   225,
     191,   376,     2,   368,   242,   165,   307,     2,    45,    46,
       9,    10,    11,    12,   321,     9,    10,    11,    12,   315,
     225,   150,    45,    46,   344,   174,    45,    46,   226,   248,
     334,   330,   185,   186,   341,   242,    39,   340,   138,   328,
     195,    45,    46,   346,   264,   221,   222,   223,   176,   282,
     283,   358,    39,   322,   227,   351,    47,    39,   345,   355,
     249,   359,   237,   360,   362,     9,    10,    11,    12,    13,
      14,    15,    47,   371,   364,   265,   373,    47,   365,   192,
     230,   374,   379,   231,   232,   233,   380,   342,   234,   235,
     392,   238,   239,   381,    51,   173,   369,   244,   245,   324,
     366,   250,   278,   252,   253,   300,   254,    39,   258,   259,
     196,   260,   261,   262,    78,   301,   266,   393,   372,   267,
     377,   188,   130,   349,     0,   319,     0,    47,     0,     0,
     397,     0,   275,   135,   136,     0,     0,   141,     0,   277,
     172,   336,   146,   148,   149,   151,   146,     0,   155,   156,
     158,   159,   160,   161,   270,   324,   320,   166,     0,   219,
     220,   168,   221,   222,   223,    78,     0,     0,     0,     0,
       0,     0,   337,   102,   103,     0,     2,   104,   105,   106,
       0,     0,     0,     0,     0,   312,    78,     0,     0,     0,
       0,     0,   317,     0,     0,   318,   102,   103,     0,     2,
     104,   105,   106,     0,   329,   213,   214,   215,   216,   217,
     218,   219,   220,     0,   221,   222,   223,   338,   339,     0,
       0,   194,   197,     0,   107,   108,     0,   200,   209,   210,
     211,   212,   213,   214,   215,   216,   217,   218,   219,   220,
     110,   221,   222,   223,     0,     0,     0,   107,   108,     0,
       0,     0,     0,     0,     0,   146,     0,   350,    78,   109,
       0,     0,     0,   110,   247,     0,   111,   118,   119,   120,
     112,     0,   257,     0,   113,     0,     0,     0,     0,   263,
     114,   115,   370,     0,   116,   117,     0,     0,     0,     0,
     118,   119,   120,   217,   218,   219,   220,     0,   221,   222,
     223,     0,     0,     0,   378,     0,   194,     0,   382,   383,
     384,     0,   386,   387,   388,     0,     0,   389,     0,     0,
     390,     0,     0,     0,   391,     0,   121,     0,   394,   395,
     396,     0,     0,     0,     0,     0,     0,   134,   302,   303,
       0,   398,     0,   142,   143,    78,    78,    78,     0,     0,
       0,     0,    78,    78,    78,     0,   316,     0,     0,   162,
     163,   164,     0,   167,     0,   323,     0,    78,    78,     0,
       0,     0,   333,    78,     0,     0,   102,   103,     0,     2,
     104,   105,   106,    45,    46,     0,     0,     0,     0,   102,
     103,     0,     2,   104,   105,   106,     0,     0,     0,     0,
       0,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,     0,   221,   222,   223,     0,   189,
       0,   323,     0,     0,     0,     0,    78,   107,   108,     0,
     198,   199,     0,   201,   202,     0,   203,   204,   205,   206,
     107,   108,   257,   110,   177,   333,   187,     0,    45,    46,
       0,     0,     0,     0,   113,     0,   110,   177,     0,     0,
     114,   115,     0,     0,   116,   117,   243,   113,     0,     0,
     118,   119,   120,   114,   115,     0,   385,   116,   117,     0,
       0,     0,     0,   118,   119,   120,     0,     0,     0,     0,
       0,     0,    78,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    78,   276,   102,   103,     0,
       2,   104,   105,   106,     0,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,     0,     0,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   175,     0,   304,   305,   306,   102,   103,
       0,     2,   104,   105,   106,     0,     0,     0,   107,   108,
      39,     0,   102,   103,     0,     2,   104,   105,   106,     0,
       0,     0,     0,   176,   110,   177,     0,     0,     0,     0,
      47,     0,     0,     0,     0,   113,     0,     0,     0,     0,
       0,   114,   115,     0,     0,   116,   117,     0,     0,   107,
     108,   118,   119,   120,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   107,   108,   110,     0,     0,     0,     0,
     242,     0,     0,     0,     0,     0,   113,     0,     0,   110,
     177,     0,   114,   115,   361,     0,   116,   117,     0,     0,
     113,     0,   118,   119,   120,     0,   114,   115,     0,   367,
     116,   117,     0,     0,     0,     0,   118,   119,   120,   102,
     103,     0,     2,   104,   105,   106,   279,   280,     0,     0,
     375,     0,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,   343,   221,   222,   223,     0,
       0,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,     0,   221,   222,   223,   207,     0,
     107,   108,     0,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   110,   221,   222,   223,
       0,     0,     0,     0,     0,     0,     0,   113,     0,     0,
       0,     0,     0,   114,   115,     0,     0,   116,   117,     0,
      -4,     1,     0,   118,   119,   120,     2,     0,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,     0,     0,     0,    21,
      22,    23,    24,    25,    26,     0,     0,    27,    28,     0,
      29,    30,   170,    31,    32,    33,    34,    35,     0,    36,
      37,     0,    38,     0,     0,     0,    39,    40,    41,    42,
      43,    44,    45,    46,     0,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,    47,   221,   222,   223,
       0,     0,     0,     0,     0,     0,     0,     0,     1,     0,
       0,    48,    49,     2,     0,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    -7,    -7,    -7,    21,    22,    23,    24,
      25,    26,    -7,    -7,    27,    28,     0,    29,    30,     0,
      31,    32,    33,    34,    35,    -7,    36,    37,     0,    38,
      -7,     0,     0,    39,    40,    41,    42,    43,    44,    45,
      46,     0,     0,   211,   212,   213,   214,   215,   216,   217,
     218,   219,   220,    47,   221,   222,   223,     0,     0,     0,
       0,     0,     0,     0,     0,     1,     0,     0,    48,    49,
       2,     0,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      -8,    -8,    -8,    21,    22,    23,    24,    25,    26,     0,
       0,    27,    28,     0,    29,    30,     0,    31,    32,    33,
      34,    35,     0,    36,    37,     0,    38,     0,     0,     0,
      39,    40,    41,    42,    43,    44,    45,    46,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     1,     0,     0,    48,    49,     2,     0,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    -8,     0,    -8,
      21,    22,    23,    24,    25,    26,     0,     0,    27,    28,
       0,    29,    30,     0,    31,    32,    33,    34,    35,     0,
      36,    37,     0,    38,     0,     0,     0,    39,    40,    41,
      42,    43,    44,    45,    46,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     1,
       0,     0,    48,    49,     2,     0,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,     0,     0,     0,    21,    22,    23,
      24,    25,    26,    -8,     0,    27,    28,     0,    29,    30,
       0,    31,    32,    33,    34,    35,     0,    36,    37,     0,
      38,     0,     0,     0,    39,    40,    41,    42,    43,    44,
      45,    46,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     1,     0,     0,    48,
      49,     2,     0,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,     0,     0,     0,    21,    22,    23,    24,    25,    26,
       0,    -8,    27,    28,     0,    29,    30,     0,    31,    32,
      33,    34,    35,     0,    36,    37,     0,    38,     0,     0,
       0,    39,    40,    41,    42,    43,    44,    45,    46,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     1,     0,     0,    48,    49,     2,     0,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,     0,     0,
       0,    21,    22,    23,    24,    25,    26,     0,     0,    27,
      28,     0,    29,    30,     0,    31,    32,    33,    34,    35,
      -8,    36,    37,     0,    38,     0,     0,     0,    39,    40,
      41,    42,    43,    44,    45,    46,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       1,     0,     0,    48,    49,     2,     0,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,     0,     0,     0,    21,    22,
      23,    24,    25,    26,     0,     0,    27,    28,     0,    29,
      30,     0,    31,    32,    33,    34,    35,     0,    36,    37,
       0,    38,    -8,     0,     0,    39,    40,    41,    42,    43,
      44,    45,    46,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     1,     0,     0,
      48,    49,     2,     0,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,     0,    -8,    21,    22,    23,    24,    25,
      26,     0,     0,    27,    28,     0,    29,    30,     0,    31,
      32,    33,    34,    35,     0,    36,    37,     0,    38,     0,
       0,     0,    39,    40,    41,    42,    43,    44,    45,    46,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     1,     0,     0,    48,    49,     2,
       0,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,     0,
       0,     0,    21,    22,    23,    24,    25,    26,     0,     0,
      27,    28,     0,    29,    30,     0,    31,    32,    33,    34,
      35,     0,    36,    37,     0,    38,     0,     0,     0,    39,
      40,    41,    42,    43,    44,    45,    46,     0,     0,     1,
       0,     0,     0,     0,     2,     0,     0,     0,     0,    47,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    48,    49,     0,    21,    22,    23,
      24,    25,    26,     0,     0,    27,    28,     0,    29,    30,
       0,    31,    32,    33,    34,    35,     0,    36,    37,     0,
      38,     0,     0,     0,    39,    40,    41,    42,    43,    44,
      45,    46,     0,     0,     0,     0,     0,     0,     0,   347,
       0,     0,     0,     0,    47,   208,   209,   210,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   220,     0,   221,
     222,   223,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   218,   219,   220,     0,   221,   222,   223,     0,
       0,     0,   281,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,     0,   221,   222,   223,
       0,     0,     0,   348,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,     0,   221,   222,
     223
};

static const yytype_int16 yycheck[] =
{
       1,    51,    31,    32,     4,   107,    78,   258,     6,    73,
     311,    63,   149,    22,     6,    16,    17,     6,     6,     0,
     231,   232,   233,    75,    33,    26,    27,   238,   239,    60,
      76,    62,    63,    69,     4,    99,     6,    78,   175,   176,
     177,    75,   253,   254,    14,    15,    16,    17,   259,    65,
       4,   352,     6,    51,    75,    43,    63,     6,    62,    63,
      14,    15,    16,    17,    13,    14,    15,    16,    17,    35,
      65,    60,    62,    63,   100,    76,    62,    63,    73,   151,
     331,    63,    83,    84,    75,    75,    56,    72,    88,    75,
     192,    62,    63,    67,   166,    94,    95,    96,    69,    67,
      68,   312,    56,    52,    99,    27,    76,    56,    75,    26,
     151,    36,   141,    36,    64,    14,    15,    16,    17,    18,
      19,    20,    76,    53,    48,   166,    68,    76,    48,    99,
     131,    74,    28,   134,   135,   136,    28,   274,   139,   140,
      28,   142,   143,    36,     0,    72,   331,   148,   149,   251,
     328,   152,   192,   154,   155,   224,   157,    56,   159,   160,
     108,   162,   163,   164,     0,   224,   167,   378,   341,   170,
     353,    85,     8,   307,    -1,   247,    -1,    76,    -1,    -1,
     391,    -1,   183,    19,    20,    -1,    -1,    23,    -1,   190,
     240,   263,    28,    29,    30,    31,    32,    -1,    34,    35,
      36,    37,    38,    39,   341,   307,   247,    43,    -1,    91,
      92,    47,    94,    95,    96,    51,    -1,    -1,    -1,    -1,
      -1,    -1,   263,     3,     4,    -1,     6,     7,     8,     9,
      -1,    -1,    -1,    -1,    -1,   236,    72,    -1,    -1,    -1,
      -1,    -1,   243,    -1,    -1,   246,     3,     4,    -1,     6,
       7,     8,     9,    -1,   255,    85,    86,    87,    88,    89,
      90,    91,    92,    -1,    94,    95,    96,   268,   269,    -1,
      -1,   107,   108,    -1,    54,    55,    -1,   113,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      70,    94,    95,    96,    -1,    -1,    -1,    54,    55,    -1,
      -1,    -1,    -1,    -1,    -1,   141,    -1,   308,   144,    66,
      -1,    -1,    -1,    70,   150,    -1,    73,    97,    98,    99,
      77,    -1,   158,    -1,    81,    -1,    -1,    -1,    -1,   165,
      87,    88,   333,    -1,    91,    92,    -1,    -1,    -1,    -1,
      97,    98,    99,    89,    90,    91,    92,    -1,    94,    95,
      96,    -1,    -1,    -1,   355,    -1,   192,    -1,   359,   360,
     361,    -1,   363,   364,   365,    -1,    -1,   368,    -1,    -1,
     371,    -1,    -1,    -1,   375,    -1,     7,    -1,   379,   380,
     381,    -1,    -1,    -1,    -1,    -1,    -1,    18,   224,   225,
      -1,   392,    -1,    24,    25,   231,   232,   233,    -1,    -1,
      -1,    -1,   238,   239,   240,    -1,   242,    -1,    -1,    40,
      41,    42,    -1,    44,    -1,   251,    -1,   253,   254,    -1,
      -1,    -1,   258,   259,    -1,    -1,     3,     4,    -1,     6,
       7,     8,     9,    62,    63,    -1,    -1,    -1,    -1,     3,
       4,    -1,     6,     7,     8,     9,    -1,    -1,    -1,    -1,
      -1,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    -1,    94,    95,    96,    -1,   100,
      -1,   307,    -1,    -1,    -1,    -1,   312,    54,    55,    -1,
     111,   112,    -1,   114,   115,    -1,   117,   118,   119,   120,
      54,    55,   328,    70,    71,   331,    73,    -1,    62,    63,
      -1,    -1,    -1,    -1,    81,    -1,    70,    71,    -1,    -1,
      87,    88,    -1,    -1,    91,    92,   147,    81,    -1,    -1,
      97,    98,    99,    87,    88,    -1,   362,    91,    92,    -1,
      -1,    -1,    -1,    97,    98,    99,    -1,    -1,    -1,    -1,
      -1,    -1,   378,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   391,   187,     3,     4,    -1,
       6,     7,     8,     9,    -1,    -1,    -1,    -1,    14,    15,
      16,    17,    18,    19,    20,    -1,    -1,   208,   209,   210,
     211,   212,   213,   214,   215,   216,   217,   218,   219,   220,
     221,   222,   223,    39,    -1,   226,   227,   228,     3,     4,
      -1,     6,     7,     8,     9,    -1,    -1,    -1,    54,    55,
      56,    -1,     3,     4,    -1,     6,     7,     8,     9,    -1,
      -1,    -1,    -1,    69,    70,    71,    -1,    -1,    -1,    -1,
      76,    -1,    -1,    -1,    -1,    81,    -1,    -1,    -1,    -1,
      -1,    87,    88,    -1,    -1,    91,    92,    -1,    -1,    54,
      55,    97,    98,    99,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    54,    55,    70,    -1,    -1,    -1,    -1,
      75,    -1,    -1,    -1,    -1,    -1,    81,    -1,    -1,    70,
      71,    -1,    87,    88,   315,    -1,    91,    92,    -1,    -1,
      81,    -1,    97,    98,    99,    -1,    87,    88,    -1,   330,
      91,    92,    -1,    -1,    -1,    -1,    97,    98,    99,     3,
       4,    -1,     6,     7,     8,     9,    74,    75,    -1,    -1,
     351,    -1,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    74,    94,    95,    96,    -1,
      -1,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    -1,    94,    95,    96,    75,    -1,
      54,    55,    -1,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    70,    94,    95,    96,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    81,    -1,    -1,
      -1,    -1,    -1,    87,    88,    -1,    -1,    91,    92,    -1,
       0,     1,    -1,    97,    98,    99,     6,    -1,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    -1,    -1,    37,    38,    -1,
      40,    41,    42,    43,    44,    45,    46,    47,    -1,    49,
      50,    -1,    52,    -1,    -1,    -1,    56,    57,    58,    59,
      60,    61,    62,    63,    -1,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    76,    94,    95,    96,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,
      -1,    91,    92,     6,    -1,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    -1,    40,    41,    -1,
      43,    44,    45,    46,    47,    48,    49,    50,    -1,    52,
      53,    -1,    -1,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    76,    94,    95,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    91,    92,
       6,    -1,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
      -1,    37,    38,    -1,    40,    41,    -1,    43,    44,    45,
      46,    47,    -1,    49,    50,    -1,    52,    -1,    -1,    -1,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     1,    -1,    -1,    91,    92,     6,    -1,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    -1,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,    37,    38,
      -1,    40,    41,    -1,    43,    44,    45,    46,    47,    -1,
      49,    50,    -1,    52,    -1,    -1,    -1,    56,    57,    58,
      59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    91,    92,     6,    -1,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    37,    38,    -1,    40,    41,
      -1,    43,    44,    45,    46,    47,    -1,    49,    50,    -1,
      52,    -1,    -1,    -1,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    91,
      92,     6,    -1,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    -1,    -1,    -1,    29,    30,    31,    32,    33,    34,
      -1,    36,    37,    38,    -1,    40,    41,    -1,    43,    44,
      45,    46,    47,    -1,    49,    50,    -1,    52,    -1,    -1,
      -1,    56,    57,    58,    59,    60,    61,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     1,    -1,    -1,    91,    92,     6,    -1,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    -1,    -1,    37,
      38,    -1,    40,    41,    -1,    43,    44,    45,    46,    47,
      48,    49,    50,    -1,    52,    -1,    -1,    -1,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,    -1,    91,    92,     6,    -1,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    -1,    -1,    37,    38,    -1,    40,
      41,    -1,    43,    44,    45,    46,    47,    -1,    49,    50,
      -1,    52,    53,    -1,    -1,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,
      91,    92,     6,    -1,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    -1,    -1,    28,    29,    30,    31,    32,    33,
      34,    -1,    -1,    37,    38,    -1,    40,    41,    -1,    43,
      44,    45,    46,    47,    -1,    49,    50,    -1,    52,    -1,
      -1,    -1,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     1,    -1,    -1,    91,    92,     6,
      -1,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    -1,    -1,
      37,    38,    -1,    40,    41,    -1,    43,    44,    45,    46,
      47,    -1,    49,    50,    -1,    52,    -1,    -1,    -1,    56,
      57,    58,    59,    60,    61,    62,    63,    -1,    -1,     1,
      -1,    -1,    -1,    -1,     6,    -1,    -1,    -1,    -1,    76,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    91,    92,    -1,    29,    30,    31,
      32,    33,    34,    -1,    -1,    37,    38,    -1,    40,    41,
      -1,    43,    44,    45,    46,    47,    -1,    49,    50,    -1,
      52,    -1,    -1,    -1,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,
      -1,    -1,    -1,    -1,    76,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    -1,    94,
      95,    96,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    -1,    94,    95,    96,    -1,
      -1,    -1,   100,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    -1,    94,    95,    96,
      -1,    -1,    -1,   100,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    -1,    94,    95,
      96
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     6,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    29,    30,    31,    32,    33,    34,    37,    38,    40,
      41,    43,    44,    45,    46,    47,    49,    50,    52,    56,
      57,    58,    59,    60,    61,    62,    63,    76,    91,    92,
     102,   104,   106,   107,   108,   109,   110,   111,   112,   113,
     116,   117,   118,   119,   120,   121,   122,   125,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   151,   152,   155,
     160,   161,   162,   163,   164,   165,   167,   168,   170,   172,
     173,   176,   177,   178,   179,   180,   181,   184,   185,   186,
     187,   133,     3,     4,     7,     8,     9,    54,    55,    66,
      70,    73,    77,    81,    87,    88,    91,    92,    97,    98,
      99,   137,   138,   142,   143,   148,   149,   150,   152,   154,
     152,    60,   133,   133,   137,   152,   152,     4,    88,   169,
     169,   152,   137,   137,   133,   133,   152,   175,   152,   152,
      60,   152,   175,   175,   169,   152,   152,   153,   152,   152,
     152,   152,   137,   137,   137,    43,   152,   137,   152,     0,
      42,   103,   106,   107,   133,    39,    69,    71,   137,   139,
     144,   164,   166,   174,   186,   133,   133,    73,   166,   137,
     141,     4,    99,   140,   152,   165,   142,   152,   137,   137,
     152,   137,   137,   137,   137,   137,   137,    75,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    94,    95,    96,    76,    65,    73,    99,    69,   114,
     133,   133,   133,   133,   133,   133,   171,   175,   133,   133,
     104,   105,    75,   137,   133,   133,   139,   152,   164,   186,
     133,    63,   133,   133,   133,   126,   127,   152,   133,   133,
     133,   133,   133,   152,   164,   186,   133,   133,   139,   139,
     139,   145,   146,   147,    75,   133,   137,   133,   140,    74,
      75,   100,    67,    68,   137,   137,   137,   137,   137,   137,
     137,   137,   137,   137,   137,   137,   137,   137,   137,   137,
     138,   143,   152,   152,   137,   137,   137,    63,   115,   105,
     105,   105,   133,   105,   105,    35,   152,   133,   133,   164,
     186,    13,    52,   152,   165,   183,   105,   105,    75,   133,
      63,   123,   124,   152,   178,   105,   164,   186,   133,   133,
      72,    75,   139,    74,   100,    75,    67,    74,   100,   183,
     133,    27,   156,   157,   158,    26,   159,   159,   105,    36,
      36,   137,    64,   182,    48,    48,   127,   137,    51,   124,
     133,    53,   147,    68,    74,   137,   159,   158,   133,    28,
      28,    36,   133,   133,   133,   152,   133,   133,   133,   133,
     133,   133,    28,   105,   133,   133,   133,   105,   133
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,   101,   102,   103,   103,   104,   104,   105,   105,   106,
     106,   107,   107,   107,   107,   107,   107,   107,   107,   107,
     107,   107,   107,   107,   107,   107,   107,   107,   107,   107,
     107,   107,   107,   107,   107,   107,   107,   107,   107,   107,
     107,   107,   107,   107,   107,   107,   107,   108,   109,   110,
     111,   112,   113,   114,   114,   115,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   123,   124,   124,   125,   126,
     126,   127,   128,   129,   129,   130,   131,   132,   133,   133,
     134,   135,   136,   136,   136,   136,   136,   136,   136,   136,
     136,   137,   137,   137,   137,   137,   137,   137,   137,   137,
     137,   137,   137,   137,   137,   137,   137,   138,   138,   138,
     139,   139,   140,   140,   140,   141,   141,   142,   143,   143,
     144,   145,   145,   146,   146,   147,   148,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   150,   150,   150,   150,   150,   150,   151,
     151,   151,   151,   151,   151,   152,   153,   153,   154,   154,
     155,   156,   156,   157,   157,   158,   159,   159,   160,   161,
     162,   162,   163,   163,   163,   163,   163,   164,   164,   164,
     165,   165,   165,   165,   165,   165,   166,   166,   167,   168,
     169,   169,   170,   171,   171,   172,   173,   174,   174,   175,
     175,   176,   177,   178,   179,   179,   180,   181,   182,   182,
     183,   183,   183,   183,   184,   184,   185,   185,   185,   185,
     185,   186,   187,   187,   187
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     2,     0,     1,     2,     1,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     3,     4,     3,
       3,     3,     5,     2,     0,     2,     0,     6,     6,     6,
       6,     6,     6,     6,     1,     2,     1,     2,     4,     1,
       3,     3,     3,     2,     3,     2,     1,     1,     1,     1,
       1,     2,     1,     2,     3,     2,     4,     4,     6,     6,
       4,     1,     2,     4,     1,     1,     1,     1,     1,     1,
       1,     3,     1,     1,     1,     1,     2,     1,     4,     4,
       1,     1,     1,     1,     1,     1,     0,     3,     3,     3,
       3,     1,     0,     1,     3,     1,     1,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     2,     2,     2,
       2,     2,     2,     3,     3,     3,     3,     3,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     0,     1,     1,
       8,     1,     0,     1,     2,     4,     3,     0,     7,     7,
       2,     2,     2,     3,     3,     4,     4,     2,     1,     4,
       1,     1,     1,     1,     2,     2,     1,     3,     3,     3,
       1,     1,     7,     1,     0,     3,     2,     1,     0,     1,
       3,     4,     3,     4,     3,     4,     3,     6,     2,     0,
       1,     1,     1,     1,     1,     1,     2,     3,     3,     4,
       4,     3,     1,     1,     1
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
#line 2290 "parser.c"
    break;

  case 3: /* end_opt: END line_tail  */
#line 127 "parser.y"
                  { ; }
#line 2296 "parser.c"
    break;

  case 5: /* statement_list: labelable_statement  */
#line 132 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2302 "parser.c"
    break;

  case 6: /* statement_list: statement_list labelable_statement  */
#line 133 "parser.y"
                                         {
         if ((yyvsp[-1].node) != NULL) { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
         else { (yyval.node) = (yyvsp[0].node); }
        }
#line 2311 "parser.c"
    break;

  case 7: /* statement_list_opt: statement_list  */
#line 140 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2317 "parser.c"
    break;

  case 8: /* statement_list_opt: %empty  */
#line 141 "parser.y"
      { (yyval.node) = NULL; }
#line 2323 "parser.c"
    break;

  case 9: /* labelable_statement: label_decl statement  */
#line 145 "parser.y"
                         { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2329 "parser.c"
    break;

  case 10: /* labelable_statement: statement  */
#line 146 "parser.y"
                { (yyval.node) = (yyvsp[0].node); }
#line 2335 "parser.c"
    break;

  case 11: /* statement: if_statement  */
#line 150 "parser.y"
                 { (yyval.node) = (yyvsp[0].node); }
#line 2341 "parser.c"
    break;

  case 12: /* statement: ifdef_statement  */
#line 151 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2347 "parser.c"
    break;

  case 13: /* statement: ifndef_statement  */
#line 152 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2353 "parser.c"
    break;

  case 14: /* statement: macro_decl_statement  */
#line 153 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2359 "parser.c"
    break;

  case 15: /* statement: macro_statement  */
#line 154 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2365 "parser.c"
    break;

  case 16: /* statement: exitm_statement  */
#line 155 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2371 "parser.c"
    break;

  case 17: /* statement: incsrc_statement  */
#line 156 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2377 "parser.c"
    break;

  case 18: /* statement: incbin_statement  */
#line 157 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2383 "parser.c"
    break;

  case 19: /* statement: equ_statement  */
#line 158 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2389 "parser.c"
    break;

  case 20: /* statement: undef_statement  */
#line 159 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2395 "parser.c"
    break;

  case 21: /* statement: assign_statement  */
#line 160 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2401 "parser.c"
    break;

  case 22: /* statement: public_statement  */
#line 161 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2407 "parser.c"
    break;

  case 23: /* statement: extrn_statement  */
#line 162 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2413 "parser.c"
    break;

  case 24: /* statement: instruction_statement  */
#line 163 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2419 "parser.c"
    break;

  case 25: /* statement: data_statement  */
#line 164 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2425 "parser.c"
    break;

  case 26: /* statement: storage_statement  */
#line 165 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2431 "parser.c"
    break;

  case 27: /* statement: dataseg_statement  */
#line 166 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2437 "parser.c"
    break;

  case 28: /* statement: codeseg_statement  */
#line 167 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2443 "parser.c"
    break;

  case 29: /* statement: charmap_statement  */
#line 168 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2449 "parser.c"
    break;

  case 30: /* statement: struc_decl_statement  */
#line 169 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2455 "parser.c"
    break;

  case 31: /* statement: union_decl_statement  */
#line 170 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2461 "parser.c"
    break;

  case 32: /* statement: enum_decl_statement  */
#line 171 "parser.y"
                          { (yyval.node) = (yyvsp[0].node); }
#line 2467 "parser.c"
    break;

  case 33: /* statement: record_decl_statement  */
#line 172 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2473 "parser.c"
    break;

  case 34: /* statement: proc_statement  */
#line 173 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2479 "parser.c"
    break;

  case 35: /* statement: rept_statement  */
#line 174 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2485 "parser.c"
    break;

  case 36: /* statement: while_statement  */
#line 175 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2491 "parser.c"
    break;

  case 37: /* statement: do_statement  */
#line 176 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2497 "parser.c"
    break;

  case 38: /* statement: label_statement  */
#line 177 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2503 "parser.c"
    break;

  case 39: /* statement: message_statement  */
#line 178 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2509 "parser.c"
    break;

  case 40: /* statement: warning_statement  */
#line 179 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2515 "parser.c"
    break;

  case 41: /* statement: error_statement  */
#line 180 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2521 "parser.c"
    break;

  case 42: /* statement: define_statement  */
#line 181 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2527 "parser.c"
    break;

  case 43: /* statement: align_statement  */
#line 182 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2533 "parser.c"
    break;

  case 44: /* statement: org_statement  */
#line 183 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2539 "parser.c"
    break;

  case 45: /* statement: null_statement  */
#line 184 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2545 "parser.c"
    break;

  case 46: /* statement: error line_tail  */
#line 185 "parser.y"
                      { (yyval.node) = NULL; }
#line 2551 "parser.c"
    break;

  case 47: /* org_statement: ORG expression line_tail  */
#line 189 "parser.y"
                             { (yyval.node) = astnode_create_org((yyvsp[-1].node), (yyloc)); }
#line 2557 "parser.c"
    break;

  case 48: /* align_statement: ALIGN identifier_list expression line_tail  */
#line 193 "parser.y"
                                               { (yyval.node) = astnode_create_align((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 2563 "parser.c"
    break;

  case 49: /* warning_statement: WARNING expression line_tail  */
#line 197 "parser.y"
                                 { (yyval.node) = astnode_create_warning((yyvsp[-1].node), (yyloc)); }
#line 2569 "parser.c"
    break;

  case 50: /* error_statement: ERROR expression line_tail  */
#line 201 "parser.y"
                               { (yyval.node) = astnode_create_error((yyvsp[-1].node), (yyloc)); }
#line 2575 "parser.c"
    break;

  case 51: /* message_statement: MESSAGE expression line_tail  */
#line 205 "parser.y"
                                 { (yyval.node) = astnode_create_message((yyvsp[-1].node), (yyloc)); }
#line 2581 "parser.c"
    break;

  case 52: /* label_statement: _LABEL_ identifier label_addr_part_opt label_type_part_opt line_tail  */
#line 209 "parser.y"
                                                                         { (yyval.node) = astnode_create_label((yyvsp[-3].node)->label, (yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); astnode_finalize((yyvsp[-3].node)); }
#line 2587 "parser.c"
    break;

  case 53: /* label_addr_part_opt: '=' expression  */
#line 213 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2593 "parser.c"
    break;

  case 54: /* label_addr_part_opt: %empty  */
#line 214 "parser.y"
      { (yyval.node) = NULL; }
#line 2599 "parser.c"
    break;

  case 55: /* label_type_part_opt: ':' symbol_type  */
#line 218 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2605 "parser.c"
    break;

  case 56: /* label_type_part_opt: %empty  */
#line 219 "parser.y"
      { (yyval.node) = NULL; }
#line 2611 "parser.c"
    break;

  case 57: /* while_statement: WHILE expression line_tail statement_list_opt ENDM line_tail  */
#line 223 "parser.y"
                                                                 { (yyval.node) = astnode_create_while((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2617 "parser.c"
    break;

  case 58: /* rept_statement: REPT expression line_tail statement_list_opt ENDM line_tail  */
#line 227 "parser.y"
                                                                { (yyval.node) = astnode_create_rept((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2623 "parser.c"
    break;

  case 59: /* do_statement: DO line_tail statement_list_opt UNTIL expression line_tail  */
#line 231 "parser.y"
                                                               { (yyval.node) = astnode_create_do((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 2629 "parser.c"
    break;

  case 60: /* proc_statement: PROC identifier line_tail statement_list_opt ENDP line_tail  */
#line 235 "parser.y"
                                                                { (yyval.node) = astnode_create_proc((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2635 "parser.c"
    break;

  case 61: /* struc_decl_statement: STRUC identifier line_tail statement_list_opt ENDS line_tail  */
#line 239 "parser.y"
                                                                 { (yyval.node) = astnode_create_struc_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2641 "parser.c"
    break;

  case 62: /* union_decl_statement: UNION identifier_opt line_tail statement_list_opt ENDS line_tail  */
#line 243 "parser.y"
                                                                     { (yyval.node) = astnode_create_union_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2647 "parser.c"
    break;

  case 63: /* enum_decl_statement: ENUM identifier line_tail enum_item_list ENDE line_tail  */
#line 247 "parser.y"
                                                            { (yyval.node) = astnode_create_enum_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2653 "parser.c"
    break;

  case 64: /* enum_item_list: enum_item  */
#line 251 "parser.y"
              { (yyval.node) = (yyvsp[0].node); }
#line 2659 "parser.c"
    break;

  case 65: /* enum_item_list: enum_item_list enum_item  */
#line 252 "parser.y"
                               { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2665 "parser.c"
    break;

  case 66: /* enum_item: assign_statement  */
#line 256 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2671 "parser.c"
    break;

  case 67: /* enum_item: identifier line_tail  */
#line 257 "parser.y"
                           { (yyval.node) = (yyvsp[-1].node); }
#line 2677 "parser.c"
    break;

  case 68: /* record_decl_statement: RECORD identifier record_field_list line_tail  */
#line 261 "parser.y"
                                                  { (yyval.node) = astnode_create_record_decl((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 2683 "parser.c"
    break;

  case 69: /* record_field_list: record_field  */
#line 265 "parser.y"
                 { (yyval.node) = (yyvsp[0].node); }
#line 2689 "parser.c"
    break;

  case 70: /* record_field_list: record_field_list ',' record_field  */
#line 266 "parser.y"
                                         { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2695 "parser.c"
    break;

  case 71: /* record_field: identifier ':' expression  */
#line 270 "parser.y"
                              { (yyval.node) = astnode_create_bitfield_decl((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2701 "parser.c"
    break;

  case 72: /* charmap_statement: CHARMAP file_specifier line_tail  */
#line 274 "parser.y"
                                     { (yyval.node) = astnode_create_charmap((yyvsp[-1].node), (yyloc)); }
#line 2707 "parser.c"
    break;

  case 73: /* dataseg_statement: DATASEG line_tail  */
#line 278 "parser.y"
                      { (yyval.node) = astnode_create_dataseg(0, (yyloc)); }
#line 2713 "parser.c"
    break;

  case 74: /* dataseg_statement: DATASEG ZEROPAGE line_tail  */
#line 279 "parser.y"
                                 { (yyval.node) = astnode_create_dataseg(ZEROPAGE_FLAG, (yyloc)); }
#line 2719 "parser.c"
    break;

  case 75: /* codeseg_statement: CODESEG line_tail  */
#line 282 "parser.y"
                      { (yyval.node) = astnode_create_codeseg((yyloc)); }
#line 2725 "parser.c"
    break;

  case 76: /* null_statement: line_tail  */
#line 286 "parser.y"
              { (yyval.node) = NULL; }
#line 2731 "parser.c"
    break;

  case 77: /* label_decl: label  */
#line 290 "parser.y"
          { (yyval.node) = (yyvsp[0].node); }
#line 2737 "parser.c"
    break;

  case 78: /* line_tail: newline  */
#line 294 "parser.y"
            { ; }
#line 2743 "parser.c"
    break;

  case 79: /* line_tail: ':'  */
#line 295 "parser.y"
          { ; }
#line 2749 "parser.c"
    break;

  case 80: /* newline: '\n'  */
#line 299 "parser.y"
         { ; }
#line 2755 "parser.c"
    break;

  case 81: /* instruction_statement: instruction line_tail  */
#line 303 "parser.y"
                          { (yyval.node) = (yyvsp[-1].node); }
#line 2761 "parser.c"
    break;

  case 82: /* instruction: MNEMONIC  */
#line 307 "parser.y"
             { (yyval.node) = astnode_create_instruction((yyvsp[0].mnemonic), IMPLIED_MODE, NULL, (yyloc)); }
#line 2767 "parser.c"
    break;

  case 83: /* instruction: MNEMONIC 'A'  */
#line 308 "parser.y"
                   { (yyval.node) = astnode_create_instruction((yyvsp[-1].mnemonic), ACCUMULATOR_MODE, NULL, (yyloc)); }
#line 2773 "parser.c"
    break;

  case 84: /* instruction: MNEMONIC '#' expression  */
#line 309 "parser.y"
                              { (yyval.node) = astnode_create_instruction((yyvsp[-2].mnemonic), IMMEDIATE_MODE, (yyvsp[0].node), (yyloc)); }
#line 2779 "parser.c"
    break;

  case 85: /* instruction: MNEMONIC expression  */
#line 310 "parser.y"
                          { (yyval.node) = astnode_create_instruction((yyvsp[-1].mnemonic), ABSOLUTE_MODE, (yyvsp[0].node), (yyloc)); }
#line 2785 "parser.c"
    break;

  case 86: /* instruction: MNEMONIC expression ',' 'X'  */
#line 311 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), ABSOLUTE_X_MODE, (yyvsp[-2].node), (yyloc)); }
#line 2791 "parser.c"
    break;

  case 87: /* instruction: MNEMONIC expression ',' 'Y'  */
#line 312 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), ABSOLUTE_Y_MODE, (yyvsp[-2].node), (yyloc)); }
#line 2797 "parser.c"
    break;

  case 88: /* instruction: MNEMONIC '[' expression ',' 'X' ']'  */
#line 313 "parser.y"
                                          { (yyval.node) = astnode_create_instruction((yyvsp[-5].mnemonic), PREINDEXED_INDIRECT_MODE, (yyvsp[-3].node), (yyloc)); }
#line 2803 "parser.c"
    break;

  case 89: /* instruction: MNEMONIC '[' expression ']' ',' 'Y'  */
#line 314 "parser.y"
                                          { (yyval.node) = astnode_create_instruction((yyvsp[-5].mnemonic), POSTINDEXED_INDIRECT_MODE, (yyvsp[-3].node), (yyloc)); }
#line 2809 "parser.c"
    break;

  case 90: /* instruction: MNEMONIC '[' expression ']'  */
#line 315 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), INDIRECT_MODE, (yyvsp[-1].node), (yyloc)); }
#line 2815 "parser.c"
    break;

  case 91: /* expression: indexed_identifier  */
#line 319 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2821 "parser.c"
    break;

  case 92: /* expression: SIZEOF sizeof_arg  */
#line 320 "parser.y"
                        { (yyval.node) = astnode_create_sizeof((yyvsp[0].node), (yyloc)); }
#line 2827 "parser.c"
    break;

  case 93: /* expression: SIZEOF '(' sizeof_arg ')'  */
#line 321 "parser.y"
                                { (yyval.node) = astnode_create_sizeof((yyvsp[-1].node), (yyloc)); }
#line 2833 "parser.c"
    break;

  case 94: /* expression: scope_access  */
#line 322 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2839 "parser.c"
    break;

  case 95: /* expression: struc_access  */
#line 323 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2845 "parser.c"
    break;

  case 96: /* expression: local_id  */
#line 324 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2851 "parser.c"
    break;

  case 97: /* expression: literal  */
#line 325 "parser.y"
              { (yyval.node) = (yyvsp[0].node); }
#line 2857 "parser.c"
    break;

  case 98: /* expression: '$'  */
#line 326 "parser.y"
          { (yyval.node) = astnode_create_pc((yyloc)); }
#line 2863 "parser.c"
    break;

  case 99: /* expression: arithmetic_expression  */
#line 327 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2869 "parser.c"
    break;

  case 100: /* expression: comparison_expression  */
#line 328 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2875 "parser.c"
    break;

  case 101: /* expression: '(' expression ')'  */
#line 329 "parser.y"
                         { (yyval.node) = (yyvsp[-1].node); }
#line 2881 "parser.c"
    break;

  case 102: /* expression: '+'  */
#line 330 "parser.y"
          { (yyval.node) = astnode_create_forward_branch("+", (yyloc)); }
#line 2887 "parser.c"
    break;

  case 103: /* expression: '-'  */
#line 331 "parser.y"
          { (yyval.node) = astnode_create_backward_branch("-", (yyloc)); }
#line 2893 "parser.c"
    break;

  case 104: /* expression: FORWARD_BRANCH  */
#line 332 "parser.y"
                     { (yyval.node) = astnode_create_forward_branch((yyvsp[0].ident), (yyloc)); }
#line 2899 "parser.c"
    break;

  case 105: /* expression: BACKWARD_BRANCH  */
#line 333 "parser.y"
                      { (yyval.node) = astnode_create_backward_branch((yyvsp[0].ident), (yyloc)); }
#line 2905 "parser.c"
    break;

  case 106: /* expression: MASK scope_access  */
#line 334 "parser.y"
                        { (yyval.node) = astnode_create_mask((yyvsp[0].node), (yyloc)); }
#line 2911 "parser.c"
    break;

  case 107: /* indexed_identifier: identifier  */
#line 338 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2917 "parser.c"
    break;

  case 108: /* indexed_identifier: identifier '[' expression ']'  */
#line 339 "parser.y"
                                    { (yyval.node) = astnode_create_index((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 2923 "parser.c"
    break;

  case 109: /* indexed_identifier: identifier '(' expression ')'  */
#line 340 "parser.y"
                                    { (yyval.node) = astnode_create_index((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 2929 "parser.c"
    break;

  case 110: /* extended_expression: expression  */
#line 344 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2935 "parser.c"
    break;

  case 111: /* extended_expression: struc_initializer  */
#line 345 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2941 "parser.c"
    break;

  case 112: /* sizeof_arg: identifier  */
#line 349 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2947 "parser.c"
    break;

  case 113: /* sizeof_arg: datatype  */
#line 350 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2953 "parser.c"
    break;

  case 114: /* sizeof_arg: STRING_LITERAL  */
#line 351 "parser.y"
                     { (yyval.node) = astnode_create_string((yyvsp[0].string), (yyloc)); }
#line 2959 "parser.c"
    break;

  case 115: /* expression_opt: expression  */
#line 355 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 2965 "parser.c"
    break;

  case 116: /* expression_opt: %empty  */
#line 356 "parser.y"
      { (yyval.node) = NULL; }
#line 2971 "parser.c"
    break;

  case 117: /* scope_access: identifier SCOPE_OP identifier  */
#line 360 "parser.y"
                                   { (yyval.node) = astnode_create_scope((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2977 "parser.c"
    break;

  case 118: /* struc_access: indexed_identifier '.' struc_access  */
#line 364 "parser.y"
                                        { (yyval.node) = astnode_create_dot((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2983 "parser.c"
    break;

  case 119: /* struc_access: indexed_identifier '.' indexed_identifier  */
#line 365 "parser.y"
                                                { (yyval.node) = astnode_create_dot((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2989 "parser.c"
    break;

  case 120: /* struc_initializer: '{' field_initializer_list_opt '}'  */
#line 369 "parser.y"
                                       { (yyval.node) = astnode_create_struc((yyvsp[-1].node), (yyloc)); }
#line 2995 "parser.c"
    break;

  case 121: /* field_initializer_list_opt: field_initializer_list  */
#line 373 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 3001 "parser.c"
    break;

  case 122: /* field_initializer_list_opt: %empty  */
#line 374 "parser.y"
      { (yyval.node) = NULL; }
#line 3007 "parser.c"
    break;

  case 123: /* field_initializer_list: field_initializer  */
#line 378 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 3013 "parser.c"
    break;

  case 124: /* field_initializer_list: field_initializer_list ',' field_initializer  */
#line 379 "parser.y"
                                                   { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3019 "parser.c"
    break;

  case 125: /* field_initializer: extended_expression  */
#line 383 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3025 "parser.c"
    break;

  case 126: /* local_id: LOCAL_ID  */
#line 387 "parser.y"
             { (yyval.node) = astnode_create_local_id((yyvsp[0].ident), (yyloc)); }
#line 3031 "parser.c"
    break;

  case 127: /* arithmetic_expression: expression '+' expression  */
#line 391 "parser.y"
                              { (yyval.node) = astnode_create_arithmetic(PLUS_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3037 "parser.c"
    break;

  case 128: /* arithmetic_expression: expression '-' expression  */
#line 392 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MINUS_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3043 "parser.c"
    break;

  case 129: /* arithmetic_expression: expression '*' expression  */
#line 393 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MUL_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3049 "parser.c"
    break;

  case 130: /* arithmetic_expression: expression '/' expression  */
#line 394 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(DIV_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3055 "parser.c"
    break;

  case 131: /* arithmetic_expression: expression '%' expression  */
#line 395 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MOD_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3061 "parser.c"
    break;

  case 132: /* arithmetic_expression: expression '&' expression  */
#line 396 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(AND_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3067 "parser.c"
    break;

  case 133: /* arithmetic_expression: expression '|' expression  */
#line 397 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(OR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3073 "parser.c"
    break;

  case 134: /* arithmetic_expression: expression '^' expression  */
#line 398 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(XOR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3079 "parser.c"
    break;

  case 135: /* arithmetic_expression: expression SHL_OP expression  */
#line 399 "parser.y"
                                   { (yyval.node) = astnode_create_arithmetic(SHL_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3085 "parser.c"
    break;

  case 136: /* arithmetic_expression: expression SHR_OP expression  */
#line 400 "parser.y"
                                   { (yyval.node) = astnode_create_arithmetic(SHR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3091 "parser.c"
    break;

  case 137: /* arithmetic_expression: '~' expression  */
#line 401 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(NEG_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3097 "parser.c"
    break;

  case 138: /* arithmetic_expression: '!' expression  */
#line 402 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(NOT_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3103 "parser.c"
    break;

  case 139: /* arithmetic_expression: '^' identifier  */
#line 403 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(BANK_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3109 "parser.c"
    break;

  case 140: /* arithmetic_expression: '<' expression  */
#line 404 "parser.y"
                                 { (yyval.node) = astnode_create_arithmetic(LO_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3115 "parser.c"
    break;

  case 141: /* arithmetic_expression: '>' expression  */
#line 405 "parser.y"
                                 { (yyval.node) = astnode_create_arithmetic(HI_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3121 "parser.c"
    break;

  case 142: /* arithmetic_expression: '-' expression  */
#line 406 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(UMINUS_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3127 "parser.c"
    break;

  case 143: /* comparison_expression: expression EQ_OP expression  */
#line 410 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(EQ_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3133 "parser.c"
    break;

  case 144: /* comparison_expression: expression NE_OP expression  */
#line 411 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(NE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3139 "parser.c"
    break;

  case 145: /* comparison_expression: expression '>' expression  */
#line 412 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(GT_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3145 "parser.c"
    break;

  case 146: /* comparison_expression: expression '<' expression  */
#line 413 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(LT_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3151 "parser.c"
    break;

  case 147: /* comparison_expression: expression GE_OP expression  */
#line 414 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(GE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3157 "parser.c"
    break;

  case 148: /* comparison_expression: expression LE_OP expression  */
#line 415 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(LE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3163 "parser.c"
    break;

  case 149: /* label: LABEL  */
#line 419 "parser.y"
          { (yyval.node) = astnode_create_label((yyvsp[0].label), NULL, NULL, (yyloc)); }
#line 3169 "parser.c"
    break;

  case 150: /* label: LOCAL_LABEL  */
#line 420 "parser.y"
                  { (yyval.node) = astnode_create_local_label((yyvsp[0].label), (yyloc)); }
#line 3175 "parser.c"
    break;

  case 151: /* label: '+'  */
#line 421 "parser.y"
          { (yyval.node) = astnode_create_forward_branch_decl("+", (yyloc)); }
#line 3181 "parser.c"
    break;

  case 152: /* label: '-'  */
#line 422 "parser.y"
          { (yyval.node) = astnode_create_backward_branch_decl("-", (yyloc)); }
#line 3187 "parser.c"
    break;

  case 153: /* label: FORWARD_BRANCH  */
#line 423 "parser.y"
                     { (yyval.node) = astnode_create_forward_branch_decl((yyvsp[0].ident), (yyloc)); }
#line 3193 "parser.c"
    break;

  case 154: /* label: BACKWARD_BRANCH  */
#line 424 "parser.y"
                      { (yyval.node) = astnode_create_backward_branch_decl((yyvsp[0].ident), (yyloc)); }
#line 3199 "parser.c"
    break;

  case 155: /* identifier: IDENTIFIER  */
#line 428 "parser.y"
               { (yyval.node) = astnode_create_identifier((yyvsp[0].ident), (yyloc)); }
#line 3205 "parser.c"
    break;

  case 156: /* identifier_opt: identifier  */
#line 432 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3211 "parser.c"
    break;

  case 157: /* identifier_opt: %empty  */
#line 433 "parser.y"
      { (yyval.node) = astnode_create_null((yyloc)); }
#line 3217 "parser.c"
    break;

  case 158: /* literal: INTEGER_LITERAL  */
#line 437 "parser.y"
                    { (yyval.node) = astnode_create_integer((yyvsp[0].integer), (yyloc)); }
#line 3223 "parser.c"
    break;

  case 159: /* literal: STRING_LITERAL  */
#line 438 "parser.y"
                     { (yyval.node) = astnode_create_string((yyvsp[0].string), (yyloc)); }
#line 3229 "parser.c"
    break;

  case 160: /* if_statement: IF expression line_tail statement_list_opt elif_statement_list_opt else_part_opt ENDIF line_tail  */
#line 442 "parser.y"
                                                                                                     { (yyval.node) = astnode_create_if((yyvsp[-6].node), (yyvsp[-4].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3235 "parser.c"
    break;

  case 161: /* elif_statement_list_opt: elif_statement_list  */
#line 446 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3241 "parser.c"
    break;

  case 162: /* elif_statement_list_opt: %empty  */
#line 447 "parser.y"
      { (yyval.node) = NULL; }
#line 3247 "parser.c"
    break;

  case 163: /* elif_statement_list: elif_statement  */
#line 451 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 3253 "parser.c"
    break;

  case 164: /* elif_statement_list: elif_statement_list elif_statement  */
#line 452 "parser.y"
                                         { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3259 "parser.c"
    break;

  case 165: /* elif_statement: ELIF expression line_tail statement_list_opt  */
#line 456 "parser.y"
                                                 { (yyval.node) = astnode_create_case((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3265 "parser.c"
    break;

  case 166: /* else_part_opt: ELSE line_tail statement_list_opt  */
#line 460 "parser.y"
                                      { (yyval.node) = (yyvsp[0].node); }
#line 3271 "parser.c"
    break;

  case 167: /* else_part_opt: %empty  */
#line 461 "parser.y"
      { (yyval.node) = NULL; }
#line 3277 "parser.c"
    break;

  case 168: /* ifdef_statement: IFDEF identifier line_tail statement_list_opt else_part_opt ENDIF line_tail  */
#line 465 "parser.y"
                                                                                { (yyval.node) = astnode_create_ifdef((yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3283 "parser.c"
    break;

  case 169: /* ifndef_statement: IFNDEF identifier line_tail statement_list_opt else_part_opt ENDIF line_tail  */
#line 469 "parser.y"
                                                                                 { (yyval.node) = astnode_create_ifndef((yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3289 "parser.c"
    break;

  case 170: /* data_statement: named_data_statement line_tail  */
#line 473 "parser.y"
                                   { (yyval.node) = (yyvsp[-1].node); }
#line 3295 "parser.c"
    break;

  case 171: /* data_statement: unnamed_data_statement line_tail  */
#line 474 "parser.y"
                                       { (yyval.node) = (yyvsp[-1].node); }
#line 3301 "parser.c"
    break;

  case 172: /* named_data_statement: identifier unnamed_data_statement  */
#line 478 "parser.y"
                                      { (yyval.node) = astnode_create_var_decl(0, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3307 "parser.c"
    break;

  case 173: /* named_data_statement: ZEROPAGE identifier unnamed_data_statement  */
#line 479 "parser.y"
                                                 { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3313 "parser.c"
    break;

  case 174: /* named_data_statement: PUBLIC identifier unnamed_data_statement  */
#line 480 "parser.y"
                                               { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3319 "parser.c"
    break;

  case 175: /* named_data_statement: ZEROPAGE PUBLIC identifier unnamed_data_statement  */
#line 481 "parser.y"
                                                        { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG | PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3325 "parser.c"
    break;

  case 176: /* named_data_statement: PUBLIC ZEROPAGE identifier unnamed_data_statement  */
#line 482 "parser.y"
                                                        { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG | ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3331 "parser.c"
    break;

  case 177: /* unnamed_data_statement: datatype expression_list  */
#line 486 "parser.y"
                             { (yyval.node) = astnode_create_data((yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3337 "parser.c"
    break;

  case 178: /* unnamed_data_statement: datatype  */
#line 487 "parser.y"
               { (yyval.node) = astnode_create_storage((yyvsp[0].node), NULL, (yyloc)); }
#line 3343 "parser.c"
    break;

  case 179: /* unnamed_data_statement: datatype '[' expression ']'  */
#line 488 "parser.y"
                                  { (yyval.node) = astnode_create_storage((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3349 "parser.c"
    break;

  case 180: /* datatype: BYTE  */
#line 492 "parser.y"
         { (yyval.node) = astnode_create_datatype(BYTE_DATATYPE, NULL, (yyloc)); }
#line 3355 "parser.c"
    break;

  case 181: /* datatype: CHAR  */
#line 493 "parser.y"
           { (yyval.node) = astnode_create_datatype(CHAR_DATATYPE, NULL, (yyloc)); }
#line 3361 "parser.c"
    break;

  case 182: /* datatype: WORD  */
#line 494 "parser.y"
           { (yyval.node) = astnode_create_datatype(WORD_DATATYPE, NULL, (yyloc)); }
#line 3367 "parser.c"
    break;

  case 183: /* datatype: DWORD  */
#line 495 "parser.y"
            { (yyval.node) = astnode_create_datatype(DWORD_DATATYPE, NULL, (yyloc)); }
#line 3373 "parser.c"
    break;

  case 184: /* datatype: TAG identifier  */
#line 496 "parser.y"
                     { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3379 "parser.c"
    break;

  case 185: /* datatype: '.' identifier  */
#line 497 "parser.y"
                     { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3385 "parser.c"
    break;

  case 186: /* expression_list: extended_expression  */
#line 501 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3391 "parser.c"
    break;

  case 187: /* expression_list: expression_list ',' extended_expression  */
#line 502 "parser.y"
                                              { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3397 "parser.c"
    break;

  case 188: /* incsrc_statement: INCSRC file_specifier line_tail  */
#line 506 "parser.y"
                                    { (yyval.node) = astnode_create_incsrc((yyvsp[-1].node), (yyloc)); handle_incsrc((yyval.node)); }
#line 3403 "parser.c"
    break;

  case 189: /* incbin_statement: INCBIN file_specifier line_tail  */
#line 510 "parser.y"
                                    { (yyval.node) = astnode_create_incbin((yyvsp[-1].node), (yyloc)); handle_incbin((yyval.node)); }
#line 3409 "parser.c"
    break;

  case 190: /* file_specifier: STRING_LITERAL  */
#line 514 "parser.y"
                   { (yyval.node) = astnode_create_string((yyvsp[0].string), (yyloc)); }
#line 3415 "parser.c"
    break;

  case 191: /* file_specifier: '<'  */
#line 515 "parser.y"
          { (yyval.node) = astnode_create_file_path(scan_include('>'), (yyloc)); }
#line 3421 "parser.c"
    break;

  case 192: /* macro_decl_statement: MACRO identifier param_list_opt line_tail statement_list_opt ENDM line_tail  */
#line 519 "parser.y"
                                                                                { (yyval.node) = astnode_create_macro_decl((yyvsp[-5].node), (yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 3427 "parser.c"
    break;

  case 193: /* param_list_opt: identifier_list  */
#line 523 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 3433 "parser.c"
    break;

  case 194: /* param_list_opt: %empty  */
#line 524 "parser.y"
      { (yyval.node) = NULL; }
#line 3439 "parser.c"
    break;

  case 195: /* macro_statement: identifier arg_list_opt line_tail  */
#line 528 "parser.y"
                                      { (yyval.node) = astnode_create_macro((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3445 "parser.c"
    break;

  case 196: /* exitm_statement: EXITM line_tail  */
#line 532 "parser.y"
                    { (yyval.node) = astnode_create(EXITM_NODE, (yyloc)); }
#line 3451 "parser.c"
    break;

  case 197: /* arg_list_opt: expression_list  */
#line 536 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 3457 "parser.c"
    break;

  case 198: /* arg_list_opt: %empty  */
#line 537 "parser.y"
      { (yyval.node) = NULL; }
#line 3463 "parser.c"
    break;

  case 199: /* identifier_list: identifier  */
#line 541 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3469 "parser.c"
    break;

  case 200: /* identifier_list: identifier_list ',' identifier  */
#line 542 "parser.y"
                                     { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3475 "parser.c"
    break;

  case 201: /* equ_statement: identifier EQU extended_expression line_tail  */
#line 546 "parser.y"
                                                 { (yyval.node) = astnode_create_equ((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3481 "parser.c"
    break;

  case 202: /* undef_statement: UNDEF identifier line_tail  */
#line 550 "parser.y"
                               { (yyval.node) = astnode_create_undef((yyvsp[-1].node), (yyloc)); }
#line 3487 "parser.c"
    break;

  case 203: /* assign_statement: identifier '=' extended_expression line_tail  */
#line 554 "parser.y"
                                                 { (yyval.node) = astnode_create_assign((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3493 "parser.c"
    break;

  case 204: /* define_statement: DEFINE identifier line_tail  */
#line 558 "parser.y"
                                { (yyval.node) = astnode_create_equ((yyvsp[-1].node), astnode_create_integer(0, (yyloc)), (yyloc)); }
#line 3499 "parser.c"
    break;

  case 205: /* define_statement: DEFINE identifier extended_expression line_tail  */
#line 559 "parser.y"
                                                      { (yyval.node) = astnode_create_equ((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3505 "parser.c"
    break;

  case 206: /* public_statement: PUBLIC identifier_list line_tail  */
#line 563 "parser.y"
                                     { (yyval.node) = astnode_create_public((yyvsp[-1].node), (yyloc)); }
#line 3511 "parser.c"
    break;

  case 207: /* extrn_statement: EXTRN identifier_list ':' symbol_type from_part_opt line_tail  */
#line 567 "parser.y"
                                                                  { (yyval.node) = astnode_create_extrn((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3517 "parser.c"
    break;

  case 208: /* from_part_opt: '@' identifier  */
#line 571 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 3523 "parser.c"
    break;

  case 209: /* from_part_opt: %empty  */
#line 572 "parser.y"
      { (yyval.node) = NULL; }
#line 3529 "parser.c"
    break;

  case 210: /* symbol_type: datatype  */
#line 576 "parser.y"
             { (yyval.node) = (yyvsp[0].node); }
#line 3535 "parser.c"
    break;

  case 211: /* symbol_type: identifier  */
#line 577 "parser.y"
                 { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3541 "parser.c"
    break;

  case 212: /* symbol_type: PROC  */
#line 578 "parser.y"
           { (yyval.node) = astnode_create_integer(PROC_SYMBOL, (yyloc)); }
#line 3547 "parser.c"
    break;

  case 213: /* symbol_type: _LABEL_  */
#line 579 "parser.y"
              { (yyval.node) = astnode_create_integer(LABEL_SYMBOL, (yyloc)); }
#line 3553 "parser.c"
    break;

  case 214: /* storage_statement: named_storage_statement  */
#line 583 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 3559 "parser.c"
    break;

  case 215: /* storage_statement: unnamed_storage_statement  */
#line 584 "parser.y"
                                { (yyval.node) = (yyvsp[0].node); }
#line 3565 "parser.c"
    break;

  case 216: /* named_storage_statement: identifier unnamed_storage_statement  */
#line 588 "parser.y"
                                         { (yyval.node) = astnode_create_var_decl(0, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3571 "parser.c"
    break;

  case 217: /* named_storage_statement: ZEROPAGE identifier unnamed_storage_statement  */
#line 589 "parser.y"
                                                    { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3577 "parser.c"
    break;

  case 218: /* named_storage_statement: PUBLIC identifier unnamed_storage_statement  */
#line 590 "parser.y"
                                                  { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3583 "parser.c"
    break;

  case 219: /* named_storage_statement: ZEROPAGE PUBLIC identifier unnamed_storage_statement  */
#line 591 "parser.y"
                                                           { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG | PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3589 "parser.c"
    break;

  case 220: /* named_storage_statement: PUBLIC ZEROPAGE identifier unnamed_storage_statement  */
#line 592 "parser.y"
                                                           { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG | ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3595 "parser.c"
    break;

  case 221: /* unnamed_storage_statement: storage expression_opt line_tail  */
#line 596 "parser.y"
                                     { (yyval.node) = astnode_create_storage((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3601 "parser.c"
    break;

  case 222: /* storage: DSB  */
#line 600 "parser.y"
        { (yyval.node) = astnode_create_datatype(BYTE_DATATYPE, NULL, (yyloc)); }
#line 3607 "parser.c"
    break;

  case 223: /* storage: DSW  */
#line 601 "parser.y"
          { (yyval.node) = astnode_create_datatype(WORD_DATATYPE, NULL, (yyloc)); }
#line 3613 "parser.c"
    break;

  case 224: /* storage: DSD  */
#line 602 "parser.y"
          { (yyval.node) = astnode_create_datatype(DWORD_DATATYPE, NULL, (yyloc)); }
#line 3619 "parser.c"
    break;


#line 3623 "parser.c"

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

#line 604 "parser.y"

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
