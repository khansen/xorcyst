/* A Bison parser, made by GNU Bison 3.7.4.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_PARSER_H_INCLUDED
# define YY_YY_PARSER_H_INCLUDED
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

#line 212 "parser.h"

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

#endif /* !YY_YY_PARSER_H_INCLUDED  */
