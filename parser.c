/* A Bison parser, made by GNU Bison 3.7.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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
#define YYBISON 30704

/* Bison version string.  */
#define YYBISON_VERSION "3.7.4"

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
#include "symtab.h"
#include "loc.h"
#include "xasm.h"
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

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    INTEGER_LITERAL = 258,         /* INTEGER_LITERAL  */
    STRING_LITERAL = 259,          /* STRING_LITERAL  */
    FILE_PATH = 260,               /* FILE_PATH  */
    IDENTIFIER = 261,              /* IDENTIFIER  */
    LOCAL_ID = 262,                /* LOCAL_ID  */
    FORWARD_BRANCH = 263,          /* FORWARD_BRANCH  */
    BACKWARD_BRANCH = 264,         /* BACKWARD_BRANCH  */
    LABEL = 265,                   /* LABEL  */
    LOCAL_LABEL = 266,             /* LOCAL_LABEL  */
    MNEMONIC = 267,                /* MNEMONIC  */
    _LABEL_ = 268,                 /* _LABEL_  */
    BYTE = 269,                    /* BYTE  */
    CHAR = 270,                    /* CHAR  */
    WORD = 271,                    /* WORD  */
    DWORD = 272,                   /* DWORD  */
    DSB = 273,                     /* DSB  */
    DSW = 274,                     /* DSW  */
    DSD = 275,                     /* DSD  */
    DATASEG = 276,                 /* DATASEG  */
    CODESEG = 277,                 /* CODESEG  */
    IF = 278,                      /* IF  */
    IFDEF = 279,                   /* IFDEF  */
    IFNDEF = 280,                  /* IFNDEF  */
    ELSE = 281,                    /* ELSE  */
    ELIF = 282,                    /* ELIF  */
    ENDIF = 283,                   /* ENDIF  */
    INCSRC = 284,                  /* INCSRC  */
    INCBIN = 285,                  /* INCBIN  */
    MACRO = 286,                   /* MACRO  */
    REPT = 287,                    /* REPT  */
    WHILE = 288,                   /* WHILE  */
    ENDM = 289,                    /* ENDM  */
    ALIGN = 290,                   /* ALIGN  */
    EQU = 291,                     /* EQU  */
    DEFINE = 292,                  /* DEFINE  */
    END = 293,                     /* END  */
    PUBLIC = 294,                  /* PUBLIC  */
    EXTRN = 295,                   /* EXTRN  */
    CHARMAP = 296,                 /* CHARMAP  */
    STRUC = 297,                   /* STRUC  */
    UNION = 298,                   /* UNION  */
    ENDS = 299,                    /* ENDS  */
    RECORD = 300,                  /* RECORD  */
    ENUM = 301,                    /* ENUM  */
    ENDE = 302,                    /* ENDE  */
    PROC = 303,                    /* PROC  */
    ENDP = 304,                    /* ENDP  */
    SIZEOF = 305,                  /* SIZEOF  */
    MASK = 306,                    /* MASK  */
    TAG = 307,                     /* TAG  */
    MESSAGE = 308,                 /* MESSAGE  */
    WARNING = 309,                 /* WARNING  */
    ERROR = 310,                   /* ERROR  */
    ZEROPAGE = 311,                /* ZEROPAGE  */
    ORG = 312,                     /* ORG  */
    SCOPE_OP = 313,                /* SCOPE_OP  */
    WIDE_MODIFIER = 314,           /* WIDE_MODIFIER  */
    LO_OP = 315,                   /* LO_OP  */
    HI_OP = 316,                   /* HI_OP  */
    EQ_OP = 317,                   /* EQ_OP  */
    NE_OP = 318,                   /* NE_OP  */
    LE_OP = 319,                   /* LE_OP  */
    GE_OP = 320,                   /* GE_OP  */
    SHL_OP = 321,                  /* SHL_OP  */
    SHR_OP = 322,                  /* SHR_OP  */
    UMINUS = 323                   /* UMINUS  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define INTEGER_LITERAL 258
#define STRING_LITERAL 259
#define FILE_PATH 260
#define IDENTIFIER 261
#define LOCAL_ID 262
#define FORWARD_BRANCH 263
#define BACKWARD_BRANCH 264
#define LABEL 265
#define LOCAL_LABEL 266
#define MNEMONIC 267
#define _LABEL_ 268
#define BYTE 269
#define CHAR 270
#define WORD 271
#define DWORD 272
#define DSB 273
#define DSW 274
#define DSD 275
#define DATASEG 276
#define CODESEG 277
#define IF 278
#define IFDEF 279
#define IFNDEF 280
#define ELSE 281
#define ELIF 282
#define ENDIF 283
#define INCSRC 284
#define INCBIN 285
#define MACRO 286
#define REPT 287
#define WHILE 288
#define ENDM 289
#define ALIGN 290
#define EQU 291
#define DEFINE 292
#define END 293
#define PUBLIC 294
#define EXTRN 295
#define CHARMAP 296
#define STRUC 297
#define UNION 298
#define ENDS 299
#define RECORD 300
#define ENUM 301
#define ENDE 302
#define PROC 303
#define ENDP 304
#define SIZEOF 305
#define MASK 306
#define TAG 307
#define MESSAGE 308
#define WARNING 309
#define ERROR 310
#define ZEROPAGE 311
#define ORG 312
#define SCOPE_OP 313
#define WIDE_MODIFIER 314
#define LO_OP 315
#define HI_OP 316
#define EQ_OP 317
#define NE_OP 318
#define LE_OP 319
#define GE_OP 320
#define SHL_OP 321
#define SHR_OP 322
#define UMINUS 323

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 69 "parser.y"

    long integer;
    int mnemonic;
    const char *string;
    const char *label;
    const char *ident;
    astnode *node;

#line 337 "parser.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


extern YYSTYPE yylval;
extern YYLTYPE yylloc;
int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
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
  YYSYMBOL_DEFINE = 37,                    /* DEFINE  */
  YYSYMBOL_END = 38,                       /* END  */
  YYSYMBOL_PUBLIC = 39,                    /* PUBLIC  */
  YYSYMBOL_EXTRN = 40,                     /* EXTRN  */
  YYSYMBOL_CHARMAP = 41,                   /* CHARMAP  */
  YYSYMBOL_STRUC = 42,                     /* STRUC  */
  YYSYMBOL_UNION = 43,                     /* UNION  */
  YYSYMBOL_ENDS = 44,                      /* ENDS  */
  YYSYMBOL_RECORD = 45,                    /* RECORD  */
  YYSYMBOL_ENUM = 46,                      /* ENUM  */
  YYSYMBOL_ENDE = 47,                      /* ENDE  */
  YYSYMBOL_PROC = 48,                      /* PROC  */
  YYSYMBOL_ENDP = 49,                      /* ENDP  */
  YYSYMBOL_SIZEOF = 50,                    /* SIZEOF  */
  YYSYMBOL_MASK = 51,                      /* MASK  */
  YYSYMBOL_TAG = 52,                       /* TAG  */
  YYSYMBOL_MESSAGE = 53,                   /* MESSAGE  */
  YYSYMBOL_WARNING = 54,                   /* WARNING  */
  YYSYMBOL_ERROR = 55,                     /* ERROR  */
  YYSYMBOL_ZEROPAGE = 56,                  /* ZEROPAGE  */
  YYSYMBOL_ORG = 57,                       /* ORG  */
  YYSYMBOL_58_n_ = 58,                     /* '\n'  */
  YYSYMBOL_59_ = 59,                       /* ':'  */
  YYSYMBOL_60_ = 60,                       /* '@'  */
  YYSYMBOL_SCOPE_OP = 61,                  /* SCOPE_OP  */
  YYSYMBOL_62_A_ = 62,                     /* 'A'  */
  YYSYMBOL_63_X_ = 63,                     /* 'X'  */
  YYSYMBOL_64_Y_ = 64,                     /* 'Y'  */
  YYSYMBOL_65_ = 65,                       /* '='  */
  YYSYMBOL_66_ = 66,                       /* '$'  */
  YYSYMBOL_67_ = 67,                       /* '{'  */
  YYSYMBOL_68_ = 68,                       /* '}'  */
  YYSYMBOL_69_ = 69,                       /* '['  */
  YYSYMBOL_70_ = 70,                       /* ']'  */
  YYSYMBOL_71_ = 71,                       /* ','  */
  YYSYMBOL_72_ = 72,                       /* '.'  */
  YYSYMBOL_WIDE_MODIFIER = 73,             /* WIDE_MODIFIER  */
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
  YYSYMBOL_assign_statement = 172,         /* assign_statement  */
  YYSYMBOL_define_statement = 173,         /* define_statement  */
  YYSYMBOL_public_statement = 174,         /* public_statement  */
  YYSYMBOL_extrn_statement = 175,          /* extrn_statement  */
  YYSYMBOL_from_part_opt = 176,            /* from_part_opt  */
  YYSYMBOL_symbol_type = 177,              /* symbol_type  */
  YYSYMBOL_storage_statement = 178,        /* storage_statement  */
  YYSYMBOL_named_storage_statement = 179,  /* named_storage_statement  */
  YYSYMBOL_unnamed_storage_statement = 180, /* unnamed_storage_statement  */
  YYSYMBOL_storage = 181                   /* storage  */
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
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
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
#define YYFINAL  161
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1742

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  98
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  84
/* YYNRULES -- Number of rules.  */
#define YYNRULES  219
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  386

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
      58,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    94,     2,    74,    66,    93,    79,     2,
      96,    97,    91,    88,    71,    89,    72,    92,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    59,     2,
      85,    65,    84,     2,    60,    62,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    63,    64,
       2,    69,     2,    70,    78,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    67,    77,    68,    95,     2,     2,     2,
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
      55,    56,    57,    61,    73,    75,    76,    80,    81,    82,
      83,    86,    87,    90
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   124,   124,   128,   129,   133,   134,   141,   142,   146,
     147,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   187,   191,   195,   199,   203,   207,
     211,   212,   216,   217,   221,   225,   229,   233,   237,   241,
     245,   246,   250,   251,   255,   259,   260,   264,   268,   272,
     273,   276,   280,   284,   288,   289,   293,   297,   301,   302,
     303,   304,   305,   306,   307,   308,   309,   310,   311,   312,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   334,   335,   336,   340,   341,
     345,   346,   350,   351,   355,   359,   360,   364,   368,   369,
     373,   374,   378,   382,   386,   387,   388,   389,   390,   391,
     392,   393,   394,   395,   396,   397,   398,   399,   400,   401,
     405,   406,   407,   408,   409,   410,   414,   415,   416,   417,
     418,   419,   423,   427,   428,   432,   433,   437,   441,   442,
     446,   447,   451,   455,   456,   460,   464,   468,   469,   473,
     474,   475,   476,   477,   481,   482,   483,   487,   488,   489,
     490,   491,   492,   496,   497,   501,   505,   509,   510,   514,
     518,   519,   523,   527,   528,   532,   533,   537,   541,   545,
     546,   550,   554,   558,   559,   563,   564,   565,   566,   570,
     571,   575,   576,   577,   578,   579,   583,   587,   588,   589
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
  "DEFINE", "END", "PUBLIC", "EXTRN", "CHARMAP", "STRUC", "UNION", "ENDS",
  "RECORD", "ENUM", "ENDE", "PROC", "ENDP", "SIZEOF", "MASK", "TAG",
  "MESSAGE", "WARNING", "ERROR", "ZEROPAGE", "ORG", "'\\n'", "':'", "'@'",
  "SCOPE_OP", "'A'", "'X'", "'Y'", "'='", "'$'", "'{'", "'}'", "'['",
  "']'", "','", "'.'", "WIDE_MODIFIER", "'#'", "LO_OP", "HI_OP", "'|'",
  "'^'", "'&'", "EQ_OP", "NE_OP", "LE_OP", "GE_OP", "'>'", "'<'", "SHL_OP",
  "SHR_OP", "'+'", "'-'", "UMINUS", "'*'", "'/'", "'%'", "'!'", "'~'",
  "'('", "')'", "$accept", "assembly_unit", "end_opt", "statement_list",
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
  "assign_statement", "define_statement", "public_statement",
  "extrn_statement", "from_part_opt", "symbol_type", "storage_statement",
  "named_storage_statement", "unnamed_storage_statement", "storage", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,    10,    58,
      64,   313,    65,    88,    89,    61,    36,   123,   125,    91,
      93,    44,    46,   314,    35,   315,   316,   124,    94,    38,
     317,   318,   319,   320,    62,    60,   321,   322,    43,    45,
     323,    42,    47,    37,    33,   126,    40,    41
};
#endif

#define YYPACT_NINF (-272)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-9)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    1466,   -46,  -272,  -272,  -272,  -272,  -272,   508,    11,  -272,
    -272,  -272,  -272,  -272,  -272,  -272,   111,   -46,   703,    11,
      11,     3,     3,    11,   703,   703,    11,    11,     5,    11,
       3,    11,    11,    11,    11,    11,    11,   703,   703,   703,
       4,   703,  -272,  -272,    11,  -272,  -272,    46,   794,  -272,
    -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,
    -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,  1527,  -272,
    -272,  -272,   -46,  -272,   477,  -272,  -272,  -272,  -272,   -46,
     -46,   605,  -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,
    -272,  -272,  -272,  -272,   703,  -272,  -272,  -272,  -272,  -272,
    -272,     8,    11,  -272,  -272,   703,   703,   703,    11,   703,
     703,  -272,   231,   703,   703,   703,  1588,   -42,  -272,  -272,
    -272,  -272,  -272,   -24,  -272,   -30,   -46,  -272,  -272,  1529,
     -46,   -46,  -272,  -272,   -46,   -46,    11,  1529,  1529,  -272,
     654,   581,    11,    39,     6,   -40,   -46,   -46,  -272,   -46,
      11,   -46,   -46,  -272,  1529,  1529,  1529,    11,    39,  1529,
    -272,  -272,   -46,  -272,  -272,  -272,  -272,   678,   678,   678,
     328,  -272,  -272,  -272,   -19,   -46,  -272,  -272,  -272,   703,
     -19,   328,   -46,  -272,  -272,  -272,  -272,     1,   129,  1607,
     328,  -272,   328,   328,    80,  -272,  -272,  1624,   -22,   703,
     703,   703,   703,   703,   703,   703,   703,   703,   703,   703,
     703,   703,   703,   703,   703,    11,    11,   703,   703,   703,
       7,  -272,   962,  1046,  1046,  -272,  -272,   -46,    -8,  1130,
    1130,    11,  1529,  -272,   -46,    39,  -272,  -272,  -272,    34,
    -272,  1214,  1214,    25,  -272,     9,    11,  1298,  -272,  -272,
    -272,    39,  -272,  -272,  -272,  -272,   -46,   -46,  -272,    17,
       2,  -272,   678,  -272,  1549,  -272,    19,    13,    68,  -272,
    -272,  -272,   537,   187,   379,    30,    30,   197,   197,   197,
     197,    16,    16,    80,    80,  -272,  -272,  -272,   -42,  -272,
     -60,  -272,  1569,  1645,   328,    34,   -46,   878,    65,    69,
      69,  1130,    63,    67,  -272,  -272,  -272,  -272,  -272,  -272,
    -272,  -272,  -272,    43,    55,    76,    11,  -272,   703,    22,
    -272,    35,  -272,    75,  -272,  -272,  -272,  -272,  -272,   678,
    -272,  -272,    62,    71,  -272,  -272,  -272,  -272,  -272,  -272,
     703,    69,    65,  -272,   -46,   107,   110,   105,   -46,   -46,
      11,   -46,   -46,   -46,  -272,   328,   -46,  -272,  -272,   -46,
    -272,  -272,  -272,  1529,   116,  -272,  1382,   -46,   -46,   -46,
    -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,   962,   -46,
    -272,  -272,  -272,  -272,  -272,  -272
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,   152,   150,   151,   146,   147,    78,     0,   177,
     178,   179,   180,   217,   218,   219,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   154,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    76,    75,     0,   148,   149,     0,     0,     5,
      10,    41,    40,    37,    38,    36,    35,    34,    33,    32,
      28,    29,    30,    31,    27,    25,    26,    42,     0,    72,
      74,    22,     0,    73,   194,    11,    12,    13,    23,     0,
       0,   175,    16,    17,    14,    15,    18,    19,    39,    20,
      21,    24,   209,   210,   113,    43,   155,   156,   123,   102,
     103,     0,     0,    79,    96,     0,     0,     0,     0,     0,
       0,   100,   101,     0,     0,     0,    81,    90,    92,    93,
      94,    97,    98,   105,    95,    51,     0,    69,    71,     0,
       0,     0,   187,   188,     0,     0,   191,     0,     0,   195,
       0,     0,     0,   195,     0,     0,     0,     0,   153,     0,
       0,     0,     0,   181,     0,     0,     0,     0,     0,     0,
     182,     1,     0,     2,     6,     9,    77,     0,     0,   119,
     108,   183,   109,   169,   193,     0,   211,   167,   168,     0,
     174,   112,     0,    91,   110,   111,   104,     0,     0,    84,
      80,   136,   138,   137,   139,   135,   134,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    70,     0,     0,     0,   185,   186,     0,   190,     0,
       0,     0,     0,   199,     0,     0,   171,   213,   201,     0,
      68,     0,     0,     0,    65,     0,     0,     0,    48,    46,
      47,     0,   170,   212,    44,     3,     0,     0,   122,     0,
     118,   120,     0,   192,     0,   216,    89,     0,     0,    99,
      82,    83,   130,   131,   129,   140,   141,   145,   144,   142,
     143,   132,   133,   124,   125,   126,   127,   128,   116,   115,
     105,   114,     0,     0,    50,     0,     0,     0,   159,   164,
     164,     0,     0,     0,   196,    45,   200,   173,   215,   208,
     207,   206,   205,   204,     0,     0,     0,    64,     0,     0,
      60,     0,    62,     0,   172,   214,   197,   198,   117,     0,
     184,   176,     0,     0,    85,    86,   106,   107,    52,    49,
       0,   164,   158,   160,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    66,    67,     0,    61,    63,     0,
     121,    88,    87,     0,     0,   161,     0,     0,     0,     0,
      55,    54,   203,   202,    57,    58,    59,    56,     0,     0,
     163,   165,   166,   189,   162,   157
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -272,  -272,  -272,   147,  -203,   -47,    81,  -272,  -272,  -272,
    -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,  -272,
    -272,  -272,  -167,  -272,  -272,  -160,  -272,  -272,  -272,  -272,
    -272,    -1,  -272,  -272,  -272,   223,   -55,  -135,  -272,  -272,
      57,   -53,  -272,  -272,  -272,  -165,  -272,  -272,  -272,  -272,
     157,  -272,  -272,  -272,  -272,  -272,  -174,  -271,  -272,  -272,
    -272,  -272,   -69,   -97,    97,  -272,  -272,    45,  -272,  -272,
    -272,  -272,   -26,  -272,  -238,  -272,  -272,  -272,  -272,  -108,
    -272,  -272,   -56,  -272
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    47,   163,   297,   298,    49,    50,    51,    52,    53,
      54,    55,    56,   220,   296,    57,    58,    59,    60,    61,
      62,   319,   320,    63,   243,   244,    64,    65,    66,    67,
      68,    69,    70,    71,    72,   170,   117,   171,   183,   182,
     118,   119,   172,   259,   260,   261,   120,   121,   122,    73,
     123,   149,   124,    75,   341,   342,   343,   345,    76,    77,
      78,    79,    80,    81,   174,    82,    83,   134,    84,   227,
      85,   175,   140,    86,    87,    88,    89,    90,   351,   313,
      91,    92,    93,    94
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      95,   164,   144,   145,   185,   173,   234,   132,   322,   217,
       2,     2,    42,    43,     2,   127,   128,     2,   176,   239,
     299,   300,     9,    10,    11,    12,   302,   303,     2,   346,
     215,   231,   256,   257,   258,   219,   218,   216,   314,   315,
       2,   270,   271,   157,   323,   217,   161,   309,     9,    10,
      11,    12,   262,     9,    10,    11,    12,    13,    14,    15,
      36,   142,   216,   231,    42,    43,   295,   135,   318,   356,
     364,   166,   218,   329,   236,   146,   333,   231,   177,   178,
      44,   322,   310,    42,    43,   328,    36,   237,   133,   252,
     332,    36,   340,    42,    43,   344,   316,   348,   347,   352,
     168,   349,   253,   350,   210,   211,    44,   212,   213,   214,
     228,    44,   204,   205,   206,   207,   208,   209,   210,   211,
     353,   212,   213,   214,   359,   221,   361,   330,   222,   223,
     224,   334,   335,   225,   226,   367,   229,   230,   368,   369,
     233,   362,   312,   238,   379,   240,   241,    48,   242,   165,
     246,   247,   357,   248,   249,   250,   354,    74,   254,   186,
     288,   255,   289,   380,   360,   125,   307,   126,   365,    42,
      43,   212,   213,   214,   263,   384,   130,   131,   180,   308,
     136,   265,   324,   139,   141,   143,   139,   338,   147,   148,
     150,   151,   152,   153,   258,   325,     0,   158,   312,   266,
     267,   160,     0,     0,     0,    74,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,     0,
     212,   213,   214,     0,     0,    74,   301,     0,     0,     0,
     116,   305,     0,   306,    96,    97,     0,     2,    98,    99,
     100,   129,   317,     0,     0,     0,     0,   137,   138,     0,
     164,     0,     0,     0,     0,   326,   327,     0,   184,   187,
     154,   155,   156,     0,   159,   191,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,     0,   212,   213,
     214,   101,   102,   208,   209,   210,   211,     0,   212,   213,
     214,     0,     0,   139,     0,   339,     0,   104,     0,   235,
       0,     0,     0,     0,     0,     0,     0,   245,     0,     0,
       0,     0,     0,     0,   251,     0,     0,   181,     0,     0,
     358,     0,     0,     0,     0,   113,   114,   115,   188,   189,
     190,     0,   192,   193,     0,   194,   195,   196,   197,     0,
       0,     0,     0,   366,     0,     0,     0,   370,   371,     0,
     373,   374,   375,     0,     0,   376,     0,     0,   377,     0,
       0,     0,   378,   232,     0,     0,   381,   382,   383,     0,
       0,     0,   290,   291,     0,     0,     0,     0,   385,    74,
      74,    74,     0,     0,     0,     0,    74,    74,   304,     0,
       0,     0,     0,     0,     0,     0,   311,     0,    74,    74,
       0,     0,   264,   321,    74,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,     0,   212,
     213,   214,   272,   273,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   284,   285,   286,   287,     0,     0,
     292,   293,   294,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   311,     0,    74,     0,     0,     0,    74,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,     0,
     212,   213,   214,   245,     0,     0,   321,     0,     0,     0,
      96,    97,     0,     2,    98,    99,   100,     0,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   372,     0,     0,
       0,    96,    97,   167,     2,    98,    99,   100,     0,     0,
       0,     0,     0,    74,     0,     0,     0,   101,   102,    36,
       0,     0,     0,     0,     0,    74,     0,     0,     0,     0,
       0,   355,   168,   104,   169,     0,     0,     0,     0,    44,
       0,     0,     0,     0,     0,   108,     0,     0,   101,   102,
       0,   109,   110,   363,     0,   111,   112,     0,     0,     0,
     103,   113,   114,   115,   104,     0,     0,   105,     0,     0,
       0,   106,   107,     0,    96,    97,   108,     2,    98,    99,
     100,     0,   109,   110,     0,     0,   111,   112,     0,     0,
       0,     0,   113,   114,   115,     0,     0,     0,    96,    97,
       0,     2,    98,    99,   100,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,     0,   212,   213,
     214,   101,   102,     0,     0,     0,     0,     0,     0,    42,
      43,     0,     0,     0,     0,     0,     0,   104,   169,     0,
       0,     0,     0,     0,     0,   101,   102,    96,    97,   108,
       2,    98,    99,   100,     0,   109,   110,     0,     0,   111,
     112,   104,   169,     0,   179,   113,   114,   115,     0,     0,
       0,    96,    97,   108,     2,    98,    99,   100,     0,   109,
     110,     0,     0,   111,   112,     0,     0,     0,     0,   113,
     114,   115,     0,     0,   101,   102,    96,    97,     0,     2,
      98,    99,   100,     0,     0,     0,     0,     0,     0,     0,
     104,     0,     0,     0,     0,   231,     0,     0,   101,   102,
       0,     0,   108,     0,     0,     0,     0,     0,   109,   110,
       0,     0,   111,   112,   104,   169,     0,     0,   113,   114,
     115,     0,     0,   101,   102,     0,   108,     0,     0,     0,
       0,     0,   109,   110,     0,     0,   111,   112,     0,   104,
       0,     0,   113,   114,   115,     0,     0,     0,     0,     0,
       0,   108,     0,     0,     0,     0,     0,   109,   110,     0,
       0,   111,   112,     0,    -4,     1,     0,   113,   114,   115,
       2,     0,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,     0,     0,    21,    22,    23,    24,    25,     0,    26,
       0,    27,   162,    28,    29,    30,    31,    32,     0,    33,
      34,     0,    35,     0,     0,     0,    36,    37,    38,    39,
      40,    41,    42,    43,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    44,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     1,
       0,     0,    45,    46,     2,     0,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    -7,    -7,    -7,    21,    22,    23,
      24,    25,    -7,    26,     0,    27,     0,    28,    29,    30,
      31,    32,    -7,    33,    34,     0,    35,    -7,     0,     0,
      36,    37,    38,    39,    40,    41,    42,    43,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      44,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     1,     0,     0,    45,    46,     2,     0,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    -8,    -8,
      -8,    21,    22,    23,    24,    25,     0,    26,     0,    27,
       0,    28,    29,    30,    31,    32,     0,    33,    34,     0,
      35,     0,     0,     0,    36,    37,    38,    39,    40,    41,
      42,    43,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    44,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     1,     0,     0,
      45,    46,     2,     0,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    -8,     0,    -8,    21,    22,    23,    24,    25,
       0,    26,     0,    27,     0,    28,    29,    30,    31,    32,
       0,    33,    34,     0,    35,     0,     0,     0,    36,    37,
      38,    39,    40,    41,    42,    43,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    44,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     1,     0,     0,    45,    46,     2,     0,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,     0,     0,     0,    21,
      22,    23,    24,    25,    -8,    26,     0,    27,     0,    28,
      29,    30,    31,    32,     0,    33,    34,     0,    35,     0,
       0,     0,    36,    37,    38,    39,    40,    41,    42,    43,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    44,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     1,     0,     0,    45,    46,
       2,     0,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,     0,     0,    21,    22,    23,    24,    25,     0,    26,
       0,    27,     0,    28,    29,    30,    31,    32,    -8,    33,
      34,     0,    35,     0,     0,     0,    36,    37,    38,    39,
      40,    41,    42,    43,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    44,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     1,
       0,     0,    45,    46,     2,     0,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,     0,     0,     0,    21,    22,    23,
      24,    25,     0,    26,     0,    27,     0,    28,    29,    30,
      31,    32,     0,    33,    34,     0,    35,    -8,     0,     0,
      36,    37,    38,    39,    40,    41,    42,    43,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      44,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     1,     0,     0,    45,    46,     2,     0,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,     0,     0,
      -8,    21,    22,    23,    24,    25,     0,    26,     0,    27,
       0,    28,    29,    30,    31,    32,     0,    33,    34,     0,
      35,     0,     0,     0,    36,    37,    38,    39,    40,    41,
      42,    43,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    44,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     1,     0,     0,
      45,    46,     2,     0,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,     0,     0,    21,    22,    23,    24,    25,
       0,    26,     0,    27,     0,    28,    29,    30,    31,    32,
       0,    33,    34,     0,    35,     0,     0,     0,    36,    37,
      38,    39,    40,    41,    42,    43,     0,     0,     1,     0,
       0,     0,     0,     2,     0,     0,     0,     0,    44,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,     0,    45,    46,    21,    22,    23,    24,
      25,     0,    26,     0,    27,     0,    28,    29,    30,    31,
      32,     0,    33,    34,     0,    35,     0,     0,     0,    36,
      37,    38,    39,    40,    41,    42,    43,    42,    43,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    44,
       0,     0,     0,     0,     0,     0,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   331,
     212,   213,   214,     0,     0,     0,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   336,
     212,   213,   214,     0,     0,     0,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   198,
     212,   213,   214,     0,     0,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   268,   212,
     213,   214,     0,     0,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,     0,   212,   213,
     214,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,     0,   212,   213,   214,     0,     0,
       0,   269,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,     0,   212,   213,   214,     0,
       0,     0,   337
};

static const yytype_int16 yycheck[] =
{
       1,    48,    28,    29,   101,    74,   141,     4,   246,    69,
       6,     6,    58,    59,     6,    16,    17,     6,    74,    59,
     223,   224,    14,    15,    16,    17,   229,   230,     6,   300,
      72,    71,   167,   168,   169,    65,    96,    61,   241,   242,
       6,    63,    64,    39,   247,    69,     0,    13,    14,    15,
      16,    17,    71,    14,    15,    16,    17,    18,    19,    20,
      52,    56,    61,    71,    58,    59,    59,    22,    59,    47,
     341,    72,    96,    71,   143,    30,    63,    71,    79,    80,
      72,   319,    48,    58,    59,    68,    52,   143,    85,   158,
      71,    52,    27,    58,    59,    26,    71,    34,   301,    44,
      65,    34,   158,    60,    88,    89,    72,    91,    92,    93,
     136,    72,    82,    83,    84,    85,    86,    87,    88,    89,
      44,    91,    92,    93,    49,   126,    64,   262,   129,   130,
     131,    63,    64,   134,   135,    28,   137,   138,    28,    34,
     141,    70,   239,   144,    28,   146,   147,     0,   149,    68,
     151,   152,   319,   154,   155,   156,   316,     0,   159,   102,
     215,   162,   215,   366,   329,     8,   235,    56,   342,    58,
      59,    91,    92,    93,   175,   378,    19,    20,    81,   235,
      23,   182,   251,    26,    27,    28,    29,   295,    31,    32,
      33,    34,    35,    36,   329,   251,    -1,    40,   295,    70,
      71,    44,    -1,    -1,    -1,    48,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    -1,
      91,    92,    93,    -1,    -1,    68,   227,    -1,    -1,    -1,
       7,   232,    -1,   234,     3,     4,    -1,     6,     7,     8,
       9,    18,   243,    -1,    -1,    -1,    -1,    24,    25,    -1,
     297,    -1,    -1,    -1,    -1,   256,   257,    -1,   101,   102,
      37,    38,    39,    -1,    41,   108,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    -1,    91,    92,
      93,    50,    51,    86,    87,    88,    89,    -1,    91,    92,
      93,    -1,    -1,   136,    -1,   296,    -1,    66,    -1,   142,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   150,    -1,    -1,
      -1,    -1,    -1,    -1,   157,    -1,    -1,    94,    -1,    -1,
     321,    -1,    -1,    -1,    -1,    94,    95,    96,   105,   106,
     107,    -1,   109,   110,    -1,   112,   113,   114,   115,    -1,
      -1,    -1,    -1,   344,    -1,    -1,    -1,   348,   349,    -1,
     351,   352,   353,    -1,    -1,   356,    -1,    -1,   359,    -1,
      -1,    -1,   363,   140,    -1,    -1,   367,   368,   369,    -1,
      -1,    -1,   215,   216,    -1,    -1,    -1,    -1,   379,   222,
     223,   224,    -1,    -1,    -1,    -1,   229,   230,   231,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   239,    -1,   241,   242,
      -1,    -1,   179,   246,   247,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    -1,    91,
      92,    93,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,    -1,    -1,
     217,   218,   219,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   295,    -1,   297,    -1,    -1,    -1,   301,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    -1,
      91,    92,    93,   316,    -1,    -1,   319,    -1,    -1,    -1,
       3,     4,    -1,     6,     7,     8,     9,    -1,    -1,    -1,
      -1,    14,    15,    16,    17,    18,    19,    20,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   350,    -1,    -1,
      -1,     3,     4,    36,     6,     7,     8,     9,    -1,    -1,
      -1,    -1,    -1,   366,    -1,    -1,    -1,    50,    51,    52,
      -1,    -1,    -1,    -1,    -1,   378,    -1,    -1,    -1,    -1,
      -1,   318,    65,    66,    67,    -1,    -1,    -1,    -1,    72,
      -1,    -1,    -1,    -1,    -1,    78,    -1,    -1,    50,    51,
      -1,    84,    85,   340,    -1,    88,    89,    -1,    -1,    -1,
      62,    94,    95,    96,    66,    -1,    -1,    69,    -1,    -1,
      -1,    73,    74,    -1,     3,     4,    78,     6,     7,     8,
       9,    -1,    84,    85,    -1,    -1,    88,    89,    -1,    -1,
      -1,    -1,    94,    95,    96,    -1,    -1,    -1,     3,     4,
      -1,     6,     7,     8,     9,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    -1,    91,    92,
      93,    50,    51,    -1,    -1,    -1,    -1,    -1,    -1,    58,
      59,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    -1,    50,    51,     3,     4,    78,
       6,     7,     8,     9,    -1,    84,    85,    -1,    -1,    88,
      89,    66,    67,    -1,    69,    94,    95,    96,    -1,    -1,
      -1,     3,     4,    78,     6,     7,     8,     9,    -1,    84,
      85,    -1,    -1,    88,    89,    -1,    -1,    -1,    -1,    94,
      95,    96,    -1,    -1,    50,    51,     3,     4,    -1,     6,
       7,     8,     9,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      66,    -1,    -1,    -1,    -1,    71,    -1,    -1,    50,    51,
      -1,    -1,    78,    -1,    -1,    -1,    -1,    -1,    84,    85,
      -1,    -1,    88,    89,    66,    67,    -1,    -1,    94,    95,
      96,    -1,    -1,    50,    51,    -1,    78,    -1,    -1,    -1,
      -1,    -1,    84,    85,    -1,    -1,    88,    89,    -1,    66,
      -1,    -1,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,
      -1,    78,    -1,    -1,    -1,    -1,    -1,    84,    85,    -1,
      -1,    88,    89,    -1,     0,     1,    -1,    94,    95,    96,
       6,    -1,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    -1,    35,
      -1,    37,    38,    39,    40,    41,    42,    43,    -1,    45,
      46,    -1,    48,    -1,    -1,    -1,    52,    53,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    88,    89,     6,    -1,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    -1,    37,    -1,    39,    40,    41,
      42,    43,    44,    45,    46,    -1,    48,    49,    -1,    -1,
      52,    53,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      72,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     1,    -1,    -1,    88,    89,     6,    -1,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    -1,    35,    -1,    37,
      -1,    39,    40,    41,    42,    43,    -1,    45,    46,    -1,
      48,    -1,    -1,    -1,    52,    53,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,
      88,    89,     6,    -1,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    -1,    28,    29,    30,    31,    32,    33,
      -1,    35,    -1,    37,    -1,    39,    40,    41,    42,    43,
      -1,    45,    46,    -1,    48,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    58,    59,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     1,    -1,    -1,    88,    89,     6,    -1,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    37,    -1,    39,
      40,    41,    42,    43,    -1,    45,    46,    -1,    48,    -1,
      -1,    -1,    52,    53,    54,    55,    56,    57,    58,    59,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    72,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    88,    89,
       6,    -1,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    -1,    35,
      -1,    37,    -1,    39,    40,    41,    42,    43,    44,    45,
      46,    -1,    48,    -1,    -1,    -1,    52,    53,    54,    55,
      56,    57,    58,    59,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    88,    89,     6,    -1,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    -1,    35,    -1,    37,    -1,    39,    40,    41,
      42,    43,    -1,    45,    46,    -1,    48,    49,    -1,    -1,
      52,    53,    54,    55,    56,    57,    58,    59,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      72,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     1,    -1,    -1,    88,    89,     6,    -1,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    -1,    -1,
      28,    29,    30,    31,    32,    33,    -1,    35,    -1,    37,
      -1,    39,    40,    41,    42,    43,    -1,    45,    46,    -1,
      48,    -1,    -1,    -1,    52,    53,    54,    55,    56,    57,
      58,    59,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,
      88,    89,     6,    -1,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      -1,    35,    -1,    37,    -1,    39,    40,    41,    42,    43,
      -1,    45,    46,    -1,    48,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    58,    59,    -1,    -1,     1,    -1,
      -1,    -1,    -1,     6,    -1,    -1,    -1,    -1,    72,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    -1,    88,    89,    29,    30,    31,    32,
      33,    -1,    35,    -1,    37,    -1,    39,    40,    41,    42,
      43,    -1,    45,    46,    -1,    48,    -1,    -1,    -1,    52,
      53,    54,    55,    56,    57,    58,    59,    58,    59,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    72,
      -1,    -1,    -1,    -1,    -1,    -1,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    70,
      91,    92,    93,    -1,    -1,    -1,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    70,
      91,    92,    93,    -1,    -1,    -1,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    71,
      91,    92,    93,    -1,    -1,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    71,    91,
      92,    93,    -1,    -1,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    -1,    91,    92,
      93,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    -1,    91,    92,    93,    -1,    -1,
      -1,    97,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    -1,    91,    92,    93,    -1,
      -1,    -1,    97
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     6,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    29,    30,    31,    32,    33,    35,    37,    39,    40,
      41,    42,    43,    45,    46,    48,    52,    53,    54,    55,
      56,    57,    58,    59,    72,    88,    89,    99,   101,   103,
     104,   105,   106,   107,   108,   109,   110,   113,   114,   115,
     116,   117,   118,   121,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   147,   148,   151,   156,   157,   158,   159,
     160,   161,   163,   164,   166,   168,   171,   172,   173,   174,
     175,   178,   179,   180,   181,   129,     3,     4,     7,     8,
       9,    50,    51,    62,    66,    69,    73,    74,    78,    84,
      85,    88,    89,    94,    95,    96,   133,   134,   138,   139,
     144,   145,   146,   148,   150,   148,    56,   129,   129,   133,
     148,   148,     4,    85,   165,   165,   148,   133,   133,   148,
     170,   148,    56,   148,   170,   170,   165,   148,   148,   149,
     148,   148,   148,   148,   133,   133,   133,    39,   148,   133,
     148,     0,    38,   100,   103,   104,   129,    36,    65,    67,
     133,   135,   140,   160,   162,   169,   180,   129,   129,    69,
     162,   133,   137,   136,   148,   161,   138,   148,   133,   133,
     133,   148,   133,   133,   133,   133,   133,   133,    71,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    91,    92,    93,    72,    61,    69,    96,    65,
     111,   129,   129,   129,   129,   129,   129,   167,   170,   129,
     129,    71,   133,   129,   135,   148,   160,   180,   129,    59,
     129,   129,   129,   122,   123,   148,   129,   129,   129,   129,
     129,   148,   160,   180,   129,   129,   135,   135,   135,   141,
     142,   143,    71,   129,   133,   129,    70,    71,    71,    97,
      63,    64,   133,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   133,   133,   133,   133,   133,   133,   134,   139,
     148,   148,   133,   133,   133,    59,   112,   101,   102,   102,
     102,   129,   102,   102,   148,   129,   129,   160,   180,    13,
      48,   148,   161,   177,   102,   102,    71,   129,    59,   119,
     120,   148,   172,   102,   160,   180,   129,   129,    68,    71,
     135,    70,    71,    63,    63,    64,    70,    97,   177,   129,
      27,   152,   153,   154,    26,   155,   155,   102,    34,    34,
      60,   176,    44,    44,   123,   133,    47,   120,   129,    49,
     143,    64,    70,   133,   155,   154,   129,    28,    28,    34,
     129,   129,   148,   129,   129,   129,   129,   129,   129,    28,
     102,   129,   129,   129,   102,   129
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    98,    99,   100,   100,   101,   101,   102,   102,   103,
     103,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   105,   106,   107,   108,   109,   110,
     111,   111,   112,   112,   113,   114,   115,   116,   117,   118,
     119,   119,   120,   120,   121,   122,   122,   123,   124,   125,
     125,   126,   127,   128,   129,   129,   130,   131,   132,   132,
     132,   132,   132,   132,   132,   132,   132,   132,   132,   132,
     133,   133,   133,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   133,   133,   133,   134,   134,   134,   135,   135,
     136,   136,   137,   137,   138,   139,   139,   140,   141,   141,
     142,   142,   143,   144,   145,   145,   145,   145,   145,   145,
     145,   145,   145,   145,   145,   145,   145,   145,   145,   145,
     146,   146,   146,   146,   146,   146,   147,   147,   147,   147,
     147,   147,   148,   149,   149,   150,   150,   151,   152,   152,
     153,   153,   154,   155,   155,   156,   157,   158,   158,   159,
     159,   159,   159,   159,   160,   160,   160,   161,   161,   161,
     161,   161,   161,   162,   162,   163,   164,   165,   165,   166,
     167,   167,   168,   169,   169,   170,   170,   171,   172,   173,
     173,   174,   175,   176,   176,   177,   177,   177,   177,   178,
     178,   179,   179,   179,   179,   179,   180,   181,   181,   181
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     2,     0,     1,     2,     1,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     3,     4,     3,     3,     3,     5,
       2,     0,     2,     0,     6,     6,     6,     6,     6,     6,
       1,     2,     1,     2,     4,     1,     3,     3,     3,     2,
       3,     2,     1,     1,     1,     1,     1,     2,     1,     2,
       3,     2,     4,     4,     3,     5,     5,     6,     6,     4,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     3,
       1,     1,     1,     1,     2,     1,     4,     4,     1,     1,
       1,     1,     1,     0,     3,     3,     3,     3,     1,     0,
       1,     3,     1,     1,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     2,     2,     2,     2,     2,     2,
       3,     3,     3,     3,     3,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     0,     1,     1,     8,     1,     0,
       1,     2,     4,     3,     0,     7,     7,     2,     2,     2,
       3,     3,     4,     4,     2,     1,     4,     1,     1,     1,
       1,     2,     2,     1,     3,     3,     3,     1,     1,     7,
       1,     0,     3,     1,     0,     1,     3,     4,     4,     3,
       4,     3,     6,     2,     0,     1,     1,     1,     1,     1,
       1,     2,     3,     3,     4,     4,     3,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


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


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YY_LOCATION_PRINT
#  if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

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

#   define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

#  else
#   define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#  endif
# endif /* !defined YY_LOCATION_PRINT */


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
  YYUSE (yyoutput);
  YYUSE (yylocationp);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yykind);
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

  YY_LOCATION_PRINT (yyo, *yylocationp);
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
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yykind);
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
    goto yyexhaustedlab;
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
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
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
#line 124 "parser.y"
                           { root_node = astnode_create_list((yyvsp[-1].node)); }
#line 2453 "parser.c"
    break;

  case 3: /* end_opt: END line_tail  */
#line 128 "parser.y"
                  { ; }
#line 2459 "parser.c"
    break;

  case 5: /* statement_list: labelable_statement  */
#line 133 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2465 "parser.c"
    break;

  case 6: /* statement_list: statement_list labelable_statement  */
#line 134 "parser.y"
                                         {
         if ((yyvsp[-1].node) != NULL) { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
         else { (yyval.node) = (yyvsp[0].node); }
        }
#line 2474 "parser.c"
    break;

  case 7: /* statement_list_opt: statement_list  */
#line 141 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2480 "parser.c"
    break;

  case 8: /* statement_list_opt: %empty  */
#line 142 "parser.y"
      { (yyval.node) = NULL; }
#line 2486 "parser.c"
    break;

  case 9: /* labelable_statement: label_decl statement  */
#line 146 "parser.y"
                         { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2492 "parser.c"
    break;

  case 10: /* labelable_statement: statement  */
#line 147 "parser.y"
                { (yyval.node) = (yyvsp[0].node); }
#line 2498 "parser.c"
    break;

  case 11: /* statement: if_statement  */
#line 151 "parser.y"
                 { (yyval.node) = (yyvsp[0].node); }
#line 2504 "parser.c"
    break;

  case 12: /* statement: ifdef_statement  */
#line 152 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2510 "parser.c"
    break;

  case 13: /* statement: ifndef_statement  */
#line 153 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2516 "parser.c"
    break;

  case 14: /* statement: macro_decl_statement  */
#line 154 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2522 "parser.c"
    break;

  case 15: /* statement: macro_statement  */
#line 155 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2528 "parser.c"
    break;

  case 16: /* statement: incsrc_statement  */
#line 156 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2534 "parser.c"
    break;

  case 17: /* statement: incbin_statement  */
#line 157 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2540 "parser.c"
    break;

  case 18: /* statement: equ_statement  */
#line 158 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2546 "parser.c"
    break;

  case 19: /* statement: assign_statement  */
#line 159 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2552 "parser.c"
    break;

  case 20: /* statement: public_statement  */
#line 160 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2558 "parser.c"
    break;

  case 21: /* statement: extrn_statement  */
#line 161 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2564 "parser.c"
    break;

  case 22: /* statement: instruction_statement  */
#line 162 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2570 "parser.c"
    break;

  case 23: /* statement: data_statement  */
#line 163 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2576 "parser.c"
    break;

  case 24: /* statement: storage_statement  */
#line 164 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2582 "parser.c"
    break;

  case 25: /* statement: dataseg_statement  */
#line 165 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2588 "parser.c"
    break;

  case 26: /* statement: codeseg_statement  */
#line 166 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2594 "parser.c"
    break;

  case 27: /* statement: charmap_statement  */
#line 167 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2600 "parser.c"
    break;

  case 28: /* statement: struc_decl_statement  */
#line 168 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2606 "parser.c"
    break;

  case 29: /* statement: union_decl_statement  */
#line 169 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 2612 "parser.c"
    break;

  case 30: /* statement: enum_decl_statement  */
#line 170 "parser.y"
                          { (yyval.node) = (yyvsp[0].node); }
#line 2618 "parser.c"
    break;

  case 31: /* statement: record_decl_statement  */
#line 171 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 2624 "parser.c"
    break;

  case 32: /* statement: proc_statement  */
#line 172 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2630 "parser.c"
    break;

  case 33: /* statement: rept_statement  */
#line 173 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2636 "parser.c"
    break;

  case 34: /* statement: while_statement  */
#line 174 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2642 "parser.c"
    break;

  case 35: /* statement: label_statement  */
#line 175 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2648 "parser.c"
    break;

  case 36: /* statement: message_statement  */
#line 176 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2654 "parser.c"
    break;

  case 37: /* statement: warning_statement  */
#line 177 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 2660 "parser.c"
    break;

  case 38: /* statement: error_statement  */
#line 178 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2666 "parser.c"
    break;

  case 39: /* statement: define_statement  */
#line 179 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2672 "parser.c"
    break;

  case 40: /* statement: align_statement  */
#line 180 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2678 "parser.c"
    break;

  case 41: /* statement: org_statement  */
#line 181 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2684 "parser.c"
    break;

  case 42: /* statement: null_statement  */
#line 182 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2690 "parser.c"
    break;

  case 43: /* statement: error line_tail  */
#line 183 "parser.y"
                      { (yyval.node) = NULL; }
#line 2696 "parser.c"
    break;

  case 44: /* org_statement: ORG expression line_tail  */
#line 187 "parser.y"
                             { (yyval.node) = astnode_create_org((yyvsp[-1].node), (yyloc)); }
#line 2702 "parser.c"
    break;

  case 45: /* align_statement: ALIGN identifier_list expression line_tail  */
#line 191 "parser.y"
                                               { (yyval.node) = astnode_create_align((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 2708 "parser.c"
    break;

  case 46: /* warning_statement: WARNING expression line_tail  */
#line 195 "parser.y"
                                 { (yyval.node) = astnode_create_warning((yyvsp[-1].node), (yyloc)); }
#line 2714 "parser.c"
    break;

  case 47: /* error_statement: ERROR expression line_tail  */
#line 199 "parser.y"
                               { (yyval.node) = astnode_create_error((yyvsp[-1].node), (yyloc)); }
#line 2720 "parser.c"
    break;

  case 48: /* message_statement: MESSAGE expression line_tail  */
#line 203 "parser.y"
                                 { (yyval.node) = astnode_create_message((yyvsp[-1].node), (yyloc)); }
#line 2726 "parser.c"
    break;

  case 49: /* label_statement: _LABEL_ identifier label_addr_part_opt label_type_part_opt line_tail  */
#line 207 "parser.y"
                                                                         { (yyval.node) = astnode_create_label((yyvsp[-3].node)->label, (yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); astnode_finalize((yyvsp[-3].node)); }
#line 2732 "parser.c"
    break;

  case 50: /* label_addr_part_opt: '=' expression  */
#line 211 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2738 "parser.c"
    break;

  case 51: /* label_addr_part_opt: %empty  */
#line 212 "parser.y"
      { (yyval.node) = NULL; }
#line 2744 "parser.c"
    break;

  case 52: /* label_type_part_opt: ':' symbol_type  */
#line 216 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2750 "parser.c"
    break;

  case 53: /* label_type_part_opt: %empty  */
#line 217 "parser.y"
      { (yyval.node) = NULL; }
#line 2756 "parser.c"
    break;

  case 54: /* while_statement: WHILE expression line_tail statement_list_opt ENDM line_tail  */
#line 221 "parser.y"
                                                                 { (yyval.node) = astnode_create_while((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2762 "parser.c"
    break;

  case 55: /* rept_statement: REPT expression line_tail statement_list_opt ENDM line_tail  */
#line 225 "parser.y"
                                                                { (yyval.node) = astnode_create_rept((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2768 "parser.c"
    break;

  case 56: /* proc_statement: PROC identifier line_tail statement_list_opt ENDP line_tail  */
#line 229 "parser.y"
                                                                { (yyval.node) = astnode_create_proc((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2774 "parser.c"
    break;

  case 57: /* struc_decl_statement: STRUC identifier line_tail statement_list_opt ENDS line_tail  */
#line 233 "parser.y"
                                                                 { (yyval.node) = astnode_create_struc_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2780 "parser.c"
    break;

  case 58: /* union_decl_statement: UNION identifier_opt line_tail statement_list_opt ENDS line_tail  */
#line 237 "parser.y"
                                                                     { (yyval.node) = astnode_create_union_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2786 "parser.c"
    break;

  case 59: /* enum_decl_statement: ENUM identifier line_tail enum_item_list ENDE line_tail  */
#line 241 "parser.y"
                                                            { (yyval.node) = astnode_create_enum_decl((yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 2792 "parser.c"
    break;

  case 60: /* enum_item_list: enum_item  */
#line 245 "parser.y"
              { (yyval.node) = (yyvsp[0].node); }
#line 2798 "parser.c"
    break;

  case 61: /* enum_item_list: enum_item_list enum_item  */
#line 246 "parser.y"
                               { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2804 "parser.c"
    break;

  case 62: /* enum_item: assign_statement  */
#line 250 "parser.y"
                     { (yyval.node) = (yyvsp[0].node); }
#line 2810 "parser.c"
    break;

  case 63: /* enum_item: identifier line_tail  */
#line 251 "parser.y"
                           { (yyval.node) = (yyvsp[-1].node); }
#line 2816 "parser.c"
    break;

  case 64: /* record_decl_statement: RECORD identifier record_field_list line_tail  */
#line 255 "parser.y"
                                                  { (yyval.node) = astnode_create_record_decl((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 2822 "parser.c"
    break;

  case 65: /* record_field_list: record_field  */
#line 259 "parser.y"
                 { (yyval.node) = (yyvsp[0].node); }
#line 2828 "parser.c"
    break;

  case 66: /* record_field_list: record_field_list ',' record_field  */
#line 260 "parser.y"
                                         { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 2834 "parser.c"
    break;

  case 67: /* record_field: identifier ':' expression  */
#line 264 "parser.y"
                              { (yyval.node) = astnode_create_bitfield_decl((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 2840 "parser.c"
    break;

  case 68: /* charmap_statement: CHARMAP file_specifier line_tail  */
#line 268 "parser.y"
                                     { (yyval.node) = astnode_create_charmap((yyvsp[-1].node), (yyloc)); }
#line 2846 "parser.c"
    break;

  case 69: /* dataseg_statement: DATASEG line_tail  */
#line 272 "parser.y"
                      { (yyval.node) = astnode_create_dataseg(0, (yyloc)); }
#line 2852 "parser.c"
    break;

  case 70: /* dataseg_statement: DATASEG ZEROPAGE line_tail  */
#line 273 "parser.y"
                                 { (yyval.node) = astnode_create_dataseg(ZEROPAGE_FLAG, (yyloc)); }
#line 2858 "parser.c"
    break;

  case 71: /* codeseg_statement: CODESEG line_tail  */
#line 276 "parser.y"
                      { (yyval.node) = astnode_create_codeseg((yyloc)); }
#line 2864 "parser.c"
    break;

  case 72: /* null_statement: line_tail  */
#line 280 "parser.y"
              { (yyval.node) = NULL; }
#line 2870 "parser.c"
    break;

  case 73: /* label_decl: label  */
#line 284 "parser.y"
          { (yyval.node) = (yyvsp[0].node); }
#line 2876 "parser.c"
    break;

  case 74: /* line_tail: newline  */
#line 288 "parser.y"
            { ; }
#line 2882 "parser.c"
    break;

  case 75: /* line_tail: ':'  */
#line 289 "parser.y"
          { ; }
#line 2888 "parser.c"
    break;

  case 76: /* newline: '\n'  */
#line 293 "parser.y"
         { ; }
#line 2894 "parser.c"
    break;

  case 77: /* instruction_statement: instruction line_tail  */
#line 297 "parser.y"
                          { (yyval.node) = (yyvsp[-1].node); }
#line 2900 "parser.c"
    break;

  case 78: /* instruction: MNEMONIC  */
#line 301 "parser.y"
             { (yyval.node) = astnode_create_instruction((yyvsp[0].mnemonic), IMPLIED_MODE, NULL, (yyloc)); }
#line 2906 "parser.c"
    break;

  case 79: /* instruction: MNEMONIC 'A'  */
#line 302 "parser.y"
                   { (yyval.node) = astnode_create_instruction((yyvsp[-1].mnemonic), ACCUMULATOR_MODE, NULL, (yyloc)); }
#line 2912 "parser.c"
    break;

  case 80: /* instruction: MNEMONIC '#' expression  */
#line 303 "parser.y"
                              { (yyval.node) = astnode_create_instruction((yyvsp[-2].mnemonic), IMMEDIATE_MODE, (yyvsp[0].node), (yyloc)); }
#line 2918 "parser.c"
    break;

  case 81: /* instruction: MNEMONIC expression  */
#line 304 "parser.y"
                          { (yyval.node) = astnode_create_instruction((yyvsp[-1].mnemonic), ABSOLUTE_MODE, (yyvsp[0].node), (yyloc)); }
#line 2924 "parser.c"
    break;

  case 82: /* instruction: MNEMONIC expression ',' 'X'  */
#line 305 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), ABSOLUTE_X_MODE, (yyvsp[-2].node), (yyloc)); }
#line 2930 "parser.c"
    break;

  case 83: /* instruction: MNEMONIC expression ',' 'Y'  */
#line 306 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), ABSOLUTE_Y_MODE, (yyvsp[-2].node), (yyloc)); }
#line 2936 "parser.c"
    break;

  case 84: /* instruction: MNEMONIC WIDE_MODIFIER expression  */
#line 307 "parser.y"
                                        { (yyval.node) = astnode_create_instruction((yyvsp[-2].mnemonic), ABSOLUTE_WIDE_MODE, (yyvsp[0].node), (yyloc)); }
#line 2942 "parser.c"
    break;

  case 85: /* instruction: MNEMONIC WIDE_MODIFIER expression ',' 'X'  */
#line 308 "parser.y"
                                                { (yyval.node) = astnode_create_instruction((yyvsp[-4].mnemonic), ABSOLUTE_X_WIDE_MODE, (yyvsp[-2].node), (yyloc)); }
#line 2948 "parser.c"
    break;

  case 86: /* instruction: MNEMONIC WIDE_MODIFIER expression ',' 'Y'  */
#line 309 "parser.y"
                                                { (yyval.node) = astnode_create_instruction((yyvsp[-4].mnemonic), ABSOLUTE_Y_WIDE_MODE, (yyvsp[-2].node), (yyloc)); }
#line 2954 "parser.c"
    break;

  case 87: /* instruction: MNEMONIC '[' expression ',' 'X' ']'  */
#line 310 "parser.y"
                                          { (yyval.node) = astnode_create_instruction((yyvsp[-5].mnemonic), PREINDEXED_INDIRECT_MODE, (yyvsp[-3].node), (yyloc)); }
#line 2960 "parser.c"
    break;

  case 88: /* instruction: MNEMONIC '[' expression ']' ',' 'Y'  */
#line 311 "parser.y"
                                          { (yyval.node) = astnode_create_instruction((yyvsp[-5].mnemonic), POSTINDEXED_INDIRECT_MODE, (yyvsp[-3].node), (yyloc)); }
#line 2966 "parser.c"
    break;

  case 89: /* instruction: MNEMONIC '[' expression ']'  */
#line 312 "parser.y"
                                  { (yyval.node) = astnode_create_instruction((yyvsp[-3].mnemonic), INDIRECT_MODE, (yyvsp[-1].node), (yyloc)); }
#line 2972 "parser.c"
    break;

  case 90: /* expression: indexed_identifier  */
#line 316 "parser.y"
                       { (yyval.node) = (yyvsp[0].node); }
#line 2978 "parser.c"
    break;

  case 91: /* expression: SIZEOF sizeof_arg  */
#line 317 "parser.y"
                        { (yyval.node) = astnode_create_sizeof((yyvsp[0].node), (yyloc)); }
#line 2984 "parser.c"
    break;

  case 92: /* expression: scope_access  */
#line 318 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2990 "parser.c"
    break;

  case 93: /* expression: struc_access  */
#line 319 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 2996 "parser.c"
    break;

  case 94: /* expression: local_id  */
#line 320 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3002 "parser.c"
    break;

  case 95: /* expression: literal  */
#line 321 "parser.y"
              { (yyval.node) = (yyvsp[0].node); }
#line 3008 "parser.c"
    break;

  case 96: /* expression: '$'  */
#line 322 "parser.y"
          { (yyval.node) = astnode_create_pc((yyloc)); }
#line 3014 "parser.c"
    break;

  case 97: /* expression: arithmetic_expression  */
#line 323 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 3020 "parser.c"
    break;

  case 98: /* expression: comparison_expression  */
#line 324 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 3026 "parser.c"
    break;

  case 99: /* expression: '(' expression ')'  */
#line 325 "parser.y"
                         { (yyval.node) = (yyvsp[-1].node); }
#line 3032 "parser.c"
    break;

  case 100: /* expression: '+'  */
#line 326 "parser.y"
          { (yyval.node) = astnode_create_forward_branch("+", (yyloc)); }
#line 3038 "parser.c"
    break;

  case 101: /* expression: '-'  */
#line 327 "parser.y"
          { (yyval.node) = astnode_create_backward_branch("-", (yyloc)); }
#line 3044 "parser.c"
    break;

  case 102: /* expression: FORWARD_BRANCH  */
#line 328 "parser.y"
                     { (yyval.node) = astnode_create_forward_branch((yyvsp[0].ident), (yyloc)); }
#line 3050 "parser.c"
    break;

  case 103: /* expression: BACKWARD_BRANCH  */
#line 329 "parser.y"
                      { (yyval.node) = astnode_create_backward_branch((yyvsp[0].ident), (yyloc)); }
#line 3056 "parser.c"
    break;

  case 104: /* expression: MASK scope_access  */
#line 330 "parser.y"
                        { (yyval.node) = astnode_create_mask((yyvsp[0].node), (yyloc)); }
#line 3062 "parser.c"
    break;

  case 105: /* indexed_identifier: identifier  */
#line 334 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3068 "parser.c"
    break;

  case 106: /* indexed_identifier: identifier '[' expression ']'  */
#line 335 "parser.y"
                                    { (yyval.node) = astnode_create_index((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3074 "parser.c"
    break;

  case 107: /* indexed_identifier: identifier '(' expression ')'  */
#line 336 "parser.y"
                                    { (yyval.node) = astnode_create_index((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3080 "parser.c"
    break;

  case 108: /* extended_expression: expression  */
#line 340 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3086 "parser.c"
    break;

  case 109: /* extended_expression: struc_initializer  */
#line 341 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3092 "parser.c"
    break;

  case 110: /* sizeof_arg: identifier  */
#line 345 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3098 "parser.c"
    break;

  case 111: /* sizeof_arg: datatype  */
#line 346 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3104 "parser.c"
    break;

  case 112: /* expression_opt: expression  */
#line 350 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3110 "parser.c"
    break;

  case 113: /* expression_opt: %empty  */
#line 351 "parser.y"
      { (yyval.node) = NULL; }
#line 3116 "parser.c"
    break;

  case 114: /* scope_access: identifier SCOPE_OP identifier  */
#line 355 "parser.y"
                                   { (yyval.node) = astnode_create_scope((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3122 "parser.c"
    break;

  case 115: /* struc_access: indexed_identifier '.' struc_access  */
#line 359 "parser.y"
                                        { (yyval.node) = astnode_create_dot((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3128 "parser.c"
    break;

  case 116: /* struc_access: indexed_identifier '.' indexed_identifier  */
#line 360 "parser.y"
                                                { (yyval.node) = astnode_create_dot((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3134 "parser.c"
    break;

  case 117: /* struc_initializer: '{' field_initializer_list_opt '}'  */
#line 364 "parser.y"
                                       { (yyval.node) = astnode_create_struc((yyvsp[-1].node), (yyloc)); }
#line 3140 "parser.c"
    break;

  case 118: /* field_initializer_list_opt: field_initializer_list  */
#line 368 "parser.y"
                           { (yyval.node) = (yyvsp[0].node); }
#line 3146 "parser.c"
    break;

  case 119: /* field_initializer_list_opt: %empty  */
#line 369 "parser.y"
      { (yyval.node) = NULL; }
#line 3152 "parser.c"
    break;

  case 120: /* field_initializer_list: field_initializer  */
#line 373 "parser.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 3158 "parser.c"
    break;

  case 121: /* field_initializer_list: field_initializer_list ',' field_initializer  */
#line 374 "parser.y"
                                                   { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3164 "parser.c"
    break;

  case 122: /* field_initializer: extended_expression  */
#line 378 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3170 "parser.c"
    break;

  case 123: /* local_id: LOCAL_ID  */
#line 382 "parser.y"
             { (yyval.node) = astnode_create_local_id((yyvsp[0].ident), (yyloc)); }
#line 3176 "parser.c"
    break;

  case 124: /* arithmetic_expression: expression '+' expression  */
#line 386 "parser.y"
                              { (yyval.node) = astnode_create_arithmetic(PLUS_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3182 "parser.c"
    break;

  case 125: /* arithmetic_expression: expression '-' expression  */
#line 387 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MINUS_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3188 "parser.c"
    break;

  case 126: /* arithmetic_expression: expression '*' expression  */
#line 388 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MUL_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3194 "parser.c"
    break;

  case 127: /* arithmetic_expression: expression '/' expression  */
#line 389 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(DIV_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3200 "parser.c"
    break;

  case 128: /* arithmetic_expression: expression '%' expression  */
#line 390 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(MOD_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3206 "parser.c"
    break;

  case 129: /* arithmetic_expression: expression '&' expression  */
#line 391 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(AND_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3212 "parser.c"
    break;

  case 130: /* arithmetic_expression: expression '|' expression  */
#line 392 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(OR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3218 "parser.c"
    break;

  case 131: /* arithmetic_expression: expression '^' expression  */
#line 393 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(XOR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3224 "parser.c"
    break;

  case 132: /* arithmetic_expression: expression SHL_OP expression  */
#line 394 "parser.y"
                                   { (yyval.node) = astnode_create_arithmetic(SHL_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3230 "parser.c"
    break;

  case 133: /* arithmetic_expression: expression SHR_OP expression  */
#line 395 "parser.y"
                                   { (yyval.node) = astnode_create_arithmetic(SHR_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3236 "parser.c"
    break;

  case 134: /* arithmetic_expression: '~' expression  */
#line 396 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(NEG_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3242 "parser.c"
    break;

  case 135: /* arithmetic_expression: '!' expression  */
#line 397 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(NOT_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3248 "parser.c"
    break;

  case 136: /* arithmetic_expression: '^' identifier  */
#line 398 "parser.y"
                     { (yyval.node) = astnode_create_arithmetic(BANK_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3254 "parser.c"
    break;

  case 137: /* arithmetic_expression: '<' expression  */
#line 399 "parser.y"
                                 { (yyval.node) = astnode_create_arithmetic(LO_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3260 "parser.c"
    break;

  case 138: /* arithmetic_expression: '>' expression  */
#line 400 "parser.y"
                                 { (yyval.node) = astnode_create_arithmetic(HI_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3266 "parser.c"
    break;

  case 139: /* arithmetic_expression: '-' expression  */
#line 401 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(UMINUS_OPERATOR, (yyvsp[0].node), NULL, (yyloc)); }
#line 3272 "parser.c"
    break;

  case 140: /* comparison_expression: expression EQ_OP expression  */
#line 405 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(EQ_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3278 "parser.c"
    break;

  case 141: /* comparison_expression: expression NE_OP expression  */
#line 406 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(NE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3284 "parser.c"
    break;

  case 142: /* comparison_expression: expression '>' expression  */
#line 407 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(GT_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3290 "parser.c"
    break;

  case 143: /* comparison_expression: expression '<' expression  */
#line 408 "parser.y"
                                { (yyval.node) = astnode_create_arithmetic(LT_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3296 "parser.c"
    break;

  case 144: /* comparison_expression: expression GE_OP expression  */
#line 409 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(GE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3302 "parser.c"
    break;

  case 145: /* comparison_expression: expression LE_OP expression  */
#line 410 "parser.y"
                                  { (yyval.node) = astnode_create_arithmetic(LE_OPERATOR, (yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3308 "parser.c"
    break;

  case 146: /* label: LABEL  */
#line 414 "parser.y"
          { (yyval.node) = astnode_create_label((yyvsp[0].label), NULL, NULL, (yyloc)); }
#line 3314 "parser.c"
    break;

  case 147: /* label: LOCAL_LABEL  */
#line 415 "parser.y"
                  { (yyval.node) = astnode_create_local_label((yyvsp[0].label), (yyloc)); }
#line 3320 "parser.c"
    break;

  case 148: /* label: '+'  */
#line 416 "parser.y"
          { (yyval.node) = astnode_create_forward_branch_decl("+", (yyloc)); }
#line 3326 "parser.c"
    break;

  case 149: /* label: '-'  */
#line 417 "parser.y"
          { (yyval.node) = astnode_create_backward_branch_decl("-", (yyloc)); }
#line 3332 "parser.c"
    break;

  case 150: /* label: FORWARD_BRANCH  */
#line 418 "parser.y"
                     { (yyval.node) = astnode_create_forward_branch_decl((yyvsp[0].ident), (yyloc)); }
#line 3338 "parser.c"
    break;

  case 151: /* label: BACKWARD_BRANCH  */
#line 419 "parser.y"
                      { (yyval.node) = astnode_create_backward_branch_decl((yyvsp[0].ident), (yyloc)); }
#line 3344 "parser.c"
    break;

  case 152: /* identifier: IDENTIFIER  */
#line 423 "parser.y"
               { (yyval.node) = astnode_create_identifier((yyvsp[0].ident), (yyloc)); }
#line 3350 "parser.c"
    break;

  case 153: /* identifier_opt: identifier  */
#line 427 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3356 "parser.c"
    break;

  case 154: /* identifier_opt: %empty  */
#line 428 "parser.y"
      { (yyval.node) = astnode_create_null((yyloc)); }
#line 3362 "parser.c"
    break;

  case 155: /* literal: INTEGER_LITERAL  */
#line 432 "parser.y"
                    { (yyval.node) = astnode_create_integer((yyvsp[0].integer), (yyloc)); }
#line 3368 "parser.c"
    break;

  case 156: /* literal: STRING_LITERAL  */
#line 433 "parser.y"
                     { (yyval.node) = astnode_create_string((yyvsp[0].string), (yyloc)); }
#line 3374 "parser.c"
    break;

  case 157: /* if_statement: IF expression line_tail statement_list_opt elif_statement_list_opt else_part_opt ENDIF line_tail  */
#line 437 "parser.y"
                                                                                                     { (yyval.node) = astnode_create_if((yyvsp[-6].node), (yyvsp[-4].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3380 "parser.c"
    break;

  case 158: /* elif_statement_list_opt: elif_statement_list  */
#line 441 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3386 "parser.c"
    break;

  case 159: /* elif_statement_list_opt: %empty  */
#line 442 "parser.y"
      { (yyval.node) = NULL; }
#line 3392 "parser.c"
    break;

  case 160: /* elif_statement_list: elif_statement  */
#line 446 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 3398 "parser.c"
    break;

  case 161: /* elif_statement_list: elif_statement_list elif_statement  */
#line 447 "parser.y"
                                         { (yyval.node) = (yyvsp[-1].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3404 "parser.c"
    break;

  case 162: /* elif_statement: ELIF expression line_tail statement_list_opt  */
#line 451 "parser.y"
                                                 { (yyval.node) = astnode_create_case((yyvsp[-2].node), (yyvsp[0].node), (yyloc)); }
#line 3410 "parser.c"
    break;

  case 163: /* else_part_opt: ELSE line_tail statement_list_opt  */
#line 455 "parser.y"
                                      { (yyval.node) = (yyvsp[0].node); }
#line 3416 "parser.c"
    break;

  case 164: /* else_part_opt: %empty  */
#line 456 "parser.y"
      { (yyval.node) = NULL; }
#line 3422 "parser.c"
    break;

  case 165: /* ifdef_statement: IFDEF identifier line_tail statement_list_opt else_part_opt ENDIF line_tail  */
#line 460 "parser.y"
                                                                                { (yyval.node) = astnode_create_ifdef((yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3428 "parser.c"
    break;

  case 166: /* ifndef_statement: IFNDEF identifier line_tail statement_list_opt else_part_opt ENDIF line_tail  */
#line 464 "parser.y"
                                                                                 { (yyval.node) = astnode_create_ifndef((yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[-2].node), (yyloc)); }
#line 3434 "parser.c"
    break;

  case 167: /* data_statement: named_data_statement line_tail  */
#line 468 "parser.y"
                                   { (yyval.node) = (yyvsp[-1].node); }
#line 3440 "parser.c"
    break;

  case 168: /* data_statement: unnamed_data_statement line_tail  */
#line 469 "parser.y"
                                       { (yyval.node) = (yyvsp[-1].node); }
#line 3446 "parser.c"
    break;

  case 169: /* named_data_statement: identifier unnamed_data_statement  */
#line 473 "parser.y"
                                      { (yyval.node) = astnode_create_var_decl(0, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3452 "parser.c"
    break;

  case 170: /* named_data_statement: ZEROPAGE identifier unnamed_data_statement  */
#line 474 "parser.y"
                                                 { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3458 "parser.c"
    break;

  case 171: /* named_data_statement: PUBLIC identifier unnamed_data_statement  */
#line 475 "parser.y"
                                               { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3464 "parser.c"
    break;

  case 172: /* named_data_statement: ZEROPAGE PUBLIC identifier unnamed_data_statement  */
#line 476 "parser.y"
                                                        { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG | PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3470 "parser.c"
    break;

  case 173: /* named_data_statement: PUBLIC ZEROPAGE identifier unnamed_data_statement  */
#line 477 "parser.y"
                                                        { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG | ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3476 "parser.c"
    break;

  case 174: /* unnamed_data_statement: datatype expression_list  */
#line 481 "parser.y"
                             { (yyval.node) = astnode_create_data((yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3482 "parser.c"
    break;

  case 175: /* unnamed_data_statement: datatype  */
#line 482 "parser.y"
               { (yyval.node) = astnode_create_storage((yyvsp[0].node), NULL, (yyloc)); }
#line 3488 "parser.c"
    break;

  case 176: /* unnamed_data_statement: datatype '[' expression ']'  */
#line 483 "parser.y"
                                  { (yyval.node) = astnode_create_storage((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3494 "parser.c"
    break;

  case 177: /* datatype: BYTE  */
#line 487 "parser.y"
         { (yyval.node) = astnode_create_datatype(BYTE_DATATYPE, NULL, (yyloc)); }
#line 3500 "parser.c"
    break;

  case 178: /* datatype: CHAR  */
#line 488 "parser.y"
           { (yyval.node) = astnode_create_datatype(CHAR_DATATYPE, NULL, (yyloc)); }
#line 3506 "parser.c"
    break;

  case 179: /* datatype: WORD  */
#line 489 "parser.y"
           { (yyval.node) = astnode_create_datatype(WORD_DATATYPE, NULL, (yyloc)); }
#line 3512 "parser.c"
    break;

  case 180: /* datatype: DWORD  */
#line 490 "parser.y"
            { (yyval.node) = astnode_create_datatype(DWORD_DATATYPE, NULL, (yyloc)); }
#line 3518 "parser.c"
    break;

  case 181: /* datatype: TAG identifier  */
#line 491 "parser.y"
                     { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3524 "parser.c"
    break;

  case 182: /* datatype: '.' identifier  */
#line 492 "parser.y"
                     { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3530 "parser.c"
    break;

  case 183: /* expression_list: extended_expression  */
#line 496 "parser.y"
                        { (yyval.node) = (yyvsp[0].node); }
#line 3536 "parser.c"
    break;

  case 184: /* expression_list: expression_list ',' extended_expression  */
#line 497 "parser.y"
                                              { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3542 "parser.c"
    break;

  case 185: /* incsrc_statement: INCSRC file_specifier line_tail  */
#line 501 "parser.y"
                                    { (yyval.node) = astnode_create_incsrc((yyvsp[-1].node), (yyloc)); handle_incsrc((yyval.node)); }
#line 3548 "parser.c"
    break;

  case 186: /* incbin_statement: INCBIN file_specifier line_tail  */
#line 505 "parser.y"
                                    { (yyval.node) = astnode_create_incbin((yyvsp[-1].node), (yyloc)); handle_incbin((yyval.node)); }
#line 3554 "parser.c"
    break;

  case 187: /* file_specifier: STRING_LITERAL  */
#line 509 "parser.y"
                   { (yyval.node) = astnode_create_string((yyvsp[0].string), (yyloc)); }
#line 3560 "parser.c"
    break;

  case 188: /* file_specifier: '<'  */
#line 510 "parser.y"
          { (yyval.node) = astnode_create_file_path(scan_include('>'), (yyloc)); }
#line 3566 "parser.c"
    break;

  case 189: /* macro_decl_statement: MACRO identifier param_list_opt line_tail statement_list_opt ENDM line_tail  */
#line 514 "parser.y"
                                                                                { (yyval.node) = astnode_create_macro_decl((yyvsp[-5].node), (yyvsp[-4].node), (yyvsp[-2].node), (yyloc)); }
#line 3572 "parser.c"
    break;

  case 190: /* param_list_opt: identifier_list  */
#line 518 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 3578 "parser.c"
    break;

  case 191: /* param_list_opt: %empty  */
#line 519 "parser.y"
      { (yyval.node) = NULL; }
#line 3584 "parser.c"
    break;

  case 192: /* macro_statement: identifier arg_list_opt line_tail  */
#line 523 "parser.y"
                                      { (yyval.node) = astnode_create_macro((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3590 "parser.c"
    break;

  case 193: /* arg_list_opt: expression_list  */
#line 527 "parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 3596 "parser.c"
    break;

  case 194: /* arg_list_opt: %empty  */
#line 528 "parser.y"
      { (yyval.node) = NULL; }
#line 3602 "parser.c"
    break;

  case 195: /* identifier_list: identifier  */
#line 532 "parser.y"
               { (yyval.node) = (yyvsp[0].node); }
#line 3608 "parser.c"
    break;

  case 196: /* identifier_list: identifier_list ',' identifier  */
#line 533 "parser.y"
                                     { (yyval.node) = (yyvsp[-2].node); astnode_add_sibling((yyval.node), (yyvsp[0].node)); }
#line 3614 "parser.c"
    break;

  case 197: /* equ_statement: identifier EQU extended_expression line_tail  */
#line 537 "parser.y"
                                                 { (yyval.node) = astnode_create_equ((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3620 "parser.c"
    break;

  case 198: /* assign_statement: identifier '=' extended_expression line_tail  */
#line 541 "parser.y"
                                                 { (yyval.node) = astnode_create_assign((yyvsp[-3].node), (yyvsp[-1].node), (yyloc)); }
#line 3626 "parser.c"
    break;

  case 199: /* define_statement: DEFINE identifier line_tail  */
#line 545 "parser.y"
                                { (yyval.node) = astnode_create_equ((yyvsp[-1].node), astnode_create_integer(0, (yyloc)), (yyloc)); }
#line 3632 "parser.c"
    break;

  case 200: /* define_statement: DEFINE identifier extended_expression line_tail  */
#line 546 "parser.y"
                                                      { (yyval.node) = astnode_create_equ((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3638 "parser.c"
    break;

  case 201: /* public_statement: PUBLIC identifier_list line_tail  */
#line 550 "parser.y"
                                     { (yyval.node) = astnode_create_public((yyvsp[-1].node), (yyloc)); }
#line 3644 "parser.c"
    break;

  case 202: /* extrn_statement: EXTRN identifier_list ':' symbol_type from_part_opt line_tail  */
#line 554 "parser.y"
                                                                  { (yyval.node) = astnode_create_extrn((yyvsp[-4].node), (yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3650 "parser.c"
    break;

  case 203: /* from_part_opt: '@' identifier  */
#line 558 "parser.y"
                   { (yyval.node) = (yyvsp[0].node); }
#line 3656 "parser.c"
    break;

  case 204: /* from_part_opt: %empty  */
#line 559 "parser.y"
      { (yyval.node) = NULL; }
#line 3662 "parser.c"
    break;

  case 205: /* symbol_type: datatype  */
#line 563 "parser.y"
             { (yyval.node) = (yyvsp[0].node); }
#line 3668 "parser.c"
    break;

  case 206: /* symbol_type: identifier  */
#line 564 "parser.y"
                 { (yyval.node) = astnode_create_datatype(USER_DATATYPE, (yyvsp[0].node), (yyloc)); }
#line 3674 "parser.c"
    break;

  case 207: /* symbol_type: PROC  */
#line 565 "parser.y"
           { (yyval.node) = astnode_create_integer(PROC_SYMBOL, (yyloc)); }
#line 3680 "parser.c"
    break;

  case 208: /* symbol_type: _LABEL_  */
#line 566 "parser.y"
              { (yyval.node) = astnode_create_integer(LABEL_SYMBOL, (yyloc)); }
#line 3686 "parser.c"
    break;

  case 209: /* storage_statement: named_storage_statement  */
#line 570 "parser.y"
                            { (yyval.node) = (yyvsp[0].node); }
#line 3692 "parser.c"
    break;

  case 210: /* storage_statement: unnamed_storage_statement  */
#line 571 "parser.y"
                                { (yyval.node) = (yyvsp[0].node); }
#line 3698 "parser.c"
    break;

  case 211: /* named_storage_statement: identifier unnamed_storage_statement  */
#line 575 "parser.y"
                                         { (yyval.node) = astnode_create_var_decl(0, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3704 "parser.c"
    break;

  case 212: /* named_storage_statement: ZEROPAGE identifier unnamed_storage_statement  */
#line 576 "parser.y"
                                                    { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3710 "parser.c"
    break;

  case 213: /* named_storage_statement: PUBLIC identifier unnamed_storage_statement  */
#line 577 "parser.y"
                                                  { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3716 "parser.c"
    break;

  case 214: /* named_storage_statement: ZEROPAGE PUBLIC identifier unnamed_storage_statement  */
#line 578 "parser.y"
                                                           { (yyval.node) = astnode_create_var_decl(ZEROPAGE_FLAG | PUBLIC_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3722 "parser.c"
    break;

  case 215: /* named_storage_statement: PUBLIC ZEROPAGE identifier unnamed_storage_statement  */
#line 579 "parser.y"
                                                           { (yyval.node) = astnode_create_var_decl(PUBLIC_FLAG | ZEROPAGE_FLAG, (yyvsp[-1].node), (yyvsp[0].node), (yyloc)); }
#line 3728 "parser.c"
    break;

  case 216: /* unnamed_storage_statement: storage expression_opt line_tail  */
#line 583 "parser.y"
                                     { (yyval.node) = astnode_create_storage((yyvsp[-2].node), (yyvsp[-1].node), (yyloc)); }
#line 3734 "parser.c"
    break;

  case 217: /* storage: DSB  */
#line 587 "parser.y"
        { (yyval.node) = astnode_create_datatype(BYTE_DATATYPE, NULL, (yyloc)); }
#line 3740 "parser.c"
    break;

  case 218: /* storage: DSW  */
#line 588 "parser.y"
          { (yyval.node) = astnode_create_datatype(WORD_DATATYPE, NULL, (yyloc)); }
#line 3746 "parser.c"
    break;

  case 219: /* storage: DSD  */
#line 589 "parser.y"
          { (yyval.node) = astnode_create_datatype(DWORD_DATATYPE, NULL, (yyloc)); }
#line 3752 "parser.c"
    break;


#line 3756 "parser.c"

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
          goto yyexhaustedlab;
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
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if 1
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif


/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
yyreturn:
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

#line 591 "parser.y"

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
        sprintf(errs, "could not open '%s' for reading", file->string);
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
    char errs[512];
    /* Get the node which describes the file to include */
    astnode *file = astnode_get_child(n, 0);
    const char *filename = file->string;
    int quoted_form = (astnode_get_type(file) == STRING_NODE) ? 1 : 0;
    /* Try to open it */
    fp = open_included_file(filename, quoted_form, NULL);
    if (fp) {
        /* Get filesize */
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        rewind(fp);
        if (size > 0) {
            /* Allocate buffer to hold file contents */
            data = (unsigned char *)malloc(size);
            /* Read file contents into buffer */
            fread(data, 1, size, fp);
            /* Insert binary node */
            astnode_add_sibling(n, astnode_create_binary(data, size, n->loc) );
        }
        /* Close file */
        fclose(fp);
    } else {
        /* Couldn't open file */
        sprintf(errs, "could not open '%s' for reading", file->string);
        yyerror(errs);
    }
}
