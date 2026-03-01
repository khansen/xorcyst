/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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
    UNDEF = 292,                   /* UNDEF  */
    DEFINE = 293,                  /* DEFINE  */
    END = 294,                     /* END  */
    PUBLIC = 295,                  /* PUBLIC  */
    EXTRN = 296,                   /* EXTRN  */
    CHARMAP = 297,                 /* CHARMAP  */
    STRUC = 298,                   /* STRUC  */
    UNION = 299,                   /* UNION  */
    ENDS = 300,                    /* ENDS  */
    RECORD = 301,                  /* RECORD  */
    ENUM = 302,                    /* ENUM  */
    ENDE = 303,                    /* ENDE  */
    PROC = 304,                    /* PROC  */
    ENDP = 305,                    /* ENDP  */
    SIZEOF = 306,                  /* SIZEOF  */
    MASK = 307,                    /* MASK  */
    TAG = 308,                     /* TAG  */
    MESSAGE = 309,                 /* MESSAGE  */
    WARNING = 310,                 /* WARNING  */
    ERROR = 311,                   /* ERROR  */
    ZEROPAGE = 312,                /* ZEROPAGE  */
    ORG = 313,                     /* ORG  */
    SCOPE_OP = 314,                /* SCOPE_OP  */
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

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 69 "parser.y"

    long integer;
    instruction_mnemonic mnemonic;
    const char *string;
    const char *label;
    const char *ident;
    astnode *node;

#line 141 "parser.h"

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
