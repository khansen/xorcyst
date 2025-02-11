%{
/*
 * $Id: scanner.l,v 1.15 2007/11/11 22:35:46 khansen Exp $
 * $Log: scanner.l,v $
 * Revision 1.15  2007/11/11 22:35:46  khansen
 * compile on mac
 *
 * Revision 1.14  2007/08/19 11:19:47  khansen
 * --case-insensitive option
 *
 * Revision 1.13  2007/08/12 18:58:49  khansen
 * ability to generate pure 6502 binary
 *
 * Revision 1.12  2007/08/11 01:25:39  khansen
 * includepaths support (-I option)
 *
 * Revision 1.11  2007/08/10 20:21:36  khansen
 * *** empty log message ***
 *
 * Revision 1.10  2007/07/22 13:34:16  khansen
 * convert tabs to whitespaces
 *
 * Revision 1.9  2005/01/09 11:20:31  kenth
 * xorcyst 1.4.5
 * BLT, BGE
 *
 * Revision 1.8  2004/12/19 19:59:08  kenth
 * xorcyst 1.4.0
 *
 * Revision 1.7  2004/12/16 13:22:08  kenth
 * added DEFINE parsing
 *
 * Revision 1.6  2004/12/14 01:50:33  kenth
 * xorcyst 1.3.0
 *
 * Revision 1.5  2004/12/11 02:12:32  kenth
 * xorcyst 1.2.0
 *
 * Revision 1.4  2004/12/10 22:43:53  kenth
 * removed FILE_PATH recognition: ".db <a, >b" is erronously parsed otherwise
 *
 * Revision 1.3  2004/12/09 11:15:28  kenth
 * bugfix: close file handle
 * added: "WARNING", "ERROR" recognition
 *
 * Revision 1.2  2004/12/06 05:06:29  kenth
 * xorcyst 1.1.0
 *
 * Revision 1.1  2004/06/30 07:56:59  kenth
 * Initial revision
 *
 */
int yyparswap;
int yyignorecase;
void yyerror(const char *);   /* See below */
char *strip_label();    /* See below */
const char *maybe_ignorecase(char *);
#include "loc.h"
#include "astnode.h"
#include "parser.h"
#include "xasm.h"
#include <string.h>
#include <ctype.h>
#include <assert.h>

static int mnemonic_impl(instr_mnemonic mnc, int wide)
{
    yylval.mnemonic.value = mnc;
    yylval.mnemonic.wide = wide;
    return MNEMONIC;
}

static int mnemonic(instr_mnemonic mnc)
{
    return mnemonic_impl(mnc, 0);
}

static int mnemonic_wide(instr_mnemonic mnc)
{
    return mnemonic_impl(mnc, 1);
}

/* Each time we match a string, move the end cursor to its end. */
#define YY_USER_ACTION  yylloc.last_column += yyleng;
%}
whitespace      [ \t]+
comment         ";"[^\n]*
string_literal      \"[^\n"]*\"
binary_literal      [0-1]+
octal_literal       0[0-7]*
decimal_literal     [1-9][0-9]*
hex_literal     [0-9A-F]+
hex_literal2        [0-9][0-9A-F]*
hex_literal3        0[A-F][0-9A-F]*
identifier      [A-Z_][A-Z0-9_]{0,254}
local_id        [0-9A-Z_]{1,255}
pragma_prefix       "."?
label_prefix        ^[ \t]*
%%
%{
/* At each yylex invocation, mark the current position as the
    start of the next token.  */
LOCATION_STEP (yylloc);
%}
{whitespace}        { LOCATION_STEP (yylloc); }
{comment}       { LOCATION_STEP (yylloc); }
"%"{binary_literal} { yylval.integer = strtol(&yytext[1], NULL, 2); return(INTEGER_LITERAL); }
{binary_literal}"B" { yylval.integer = strtol(yytext, NULL, 2); return(INTEGER_LITERAL); }
{octal_literal}     { yylval.integer = strtol(yytext, NULL, 8); return(INTEGER_LITERAL); }
{decimal_literal}   { yylval.integer = strtol(yytext, NULL, 10); return(INTEGER_LITERAL); }
"0X"{hex_literal}   { yylval.integer = strtol(&yytext[2], NULL, 16); return(INTEGER_LITERAL); }
"$"{hex_literal}    { yylval.integer = strtol(&yytext[1], NULL, 16); return(INTEGER_LITERAL); }
{hex_literal3}"H"   { yylval.integer = strtol(yytext, NULL, 16); return(INTEGER_LITERAL); }
{hex_literal2}"H"   { yylval.integer = strtol(yytext, NULL, 16); return(INTEGER_LITERAL); }
"'"."'"         { yylval.integer = yytext[1]; return(INTEGER_LITERAL); }
"ADC"           { return mnemonic(ADC_MNEMONIC); }
"ADC.W"         { return mnemonic_wide(ADC_MNEMONIC); }
"AND"           { return mnemonic(AND_MNEMONIC); }
"AND.W"         { return mnemonic_wide(AND_MNEMONIC); }
"ASL"           { return mnemonic(ASL_MNEMONIC); }
"ASL.W"         { return mnemonic_wide(ASL_MNEMONIC); }
"BCC"           { return mnemonic(BCC_MNEMONIC); }
"BCS"           { return mnemonic(BCS_MNEMONIC); }
"BLT"           { return mnemonic(BCC_MNEMONIC); }
"BGE"           { return mnemonic(BCS_MNEMONIC); }
"BEQ"           { return mnemonic(BEQ_MNEMONIC); }
"BIT"           { return mnemonic(BIT_MNEMONIC); }
"BIT.W"         { return mnemonic_wide(BIT_MNEMONIC); }
"BMI"           { return mnemonic(BMI_MNEMONIC); }
"BNE"           { return mnemonic(BNE_MNEMONIC); }
"BPL"           { return mnemonic(BPL_MNEMONIC); }
"BRK"           { return mnemonic(BRK_MNEMONIC); }
"BVC"           { return mnemonic(BVC_MNEMONIC); }
"BVS"           { return mnemonic(BVS_MNEMONIC); }
"CLC"           { return mnemonic(CLC_MNEMONIC); }
"CLD"           { return mnemonic(CLD_MNEMONIC); }
"CLI"           { return mnemonic(CLI_MNEMONIC); }
"CLV"           { return mnemonic(CLV_MNEMONIC); }
"CMP"           { return mnemonic(CMP_MNEMONIC); }
"CMP.W"         { return mnemonic_wide(CMP_MNEMONIC); }
"CPX"           { return mnemonic(CPX_MNEMONIC); }
"CPX.W"         { return mnemonic_wide(CPX_MNEMONIC); }
"CPY"           { return mnemonic(CPY_MNEMONIC); }
"CPY.W"         { return mnemonic_wide(CPY_MNEMONIC); }
"DEC"           { return mnemonic(DEC_MNEMONIC); }
"DEC.W"         { return mnemonic_wide(DEC_MNEMONIC); }
"DEX"           { return mnemonic(DEX_MNEMONIC); }
"DEX.W"         { return mnemonic_wide(DEX_MNEMONIC); }
"DEY"           { return mnemonic(DEY_MNEMONIC); }
"EOR"           { return mnemonic(EOR_MNEMONIC); }
"EOR.W"         { return mnemonic_wide(EOR_MNEMONIC); }
"INC"           { return mnemonic(INC_MNEMONIC); }
"INC.W"         { return mnemonic_wide(INC_MNEMONIC); }
"INX"           { return mnemonic(INX_MNEMONIC); }
"INY"           { return mnemonic(INY_MNEMONIC); }
"JMP"           { return mnemonic(JMP_MNEMONIC); }
"JSR"           { return mnemonic(JSR_MNEMONIC); }
"LDA"           { return mnemonic(LDA_MNEMONIC); }
"LDA.W"         { return mnemonic_wide(LDA_MNEMONIC); }
"LDX"           { return mnemonic(LDX_MNEMONIC); }
"LDX.W"         { return mnemonic_wide(LDX_MNEMONIC); }
"LDY"           { return mnemonic(LDY_MNEMONIC); }
"LDY.W"         { return mnemonic_wide(LDY_MNEMONIC); }
"LSR"           { return mnemonic(LSR_MNEMONIC); }
"LSR.W"         { return mnemonic_wide(LSR_MNEMONIC); }
"NOP"           { return mnemonic(NOP_MNEMONIC); }
"ORA"           { return mnemonic(ORA_MNEMONIC); }
"ORA.W"         { return mnemonic_wide(ORA_MNEMONIC); }
"PHA"           { return mnemonic(PHA_MNEMONIC); }
"PHP"           { return mnemonic(PHP_MNEMONIC); }
"PLA"           { return mnemonic(PLA_MNEMONIC); }
"PLP"           { return mnemonic(PLP_MNEMONIC); }
"ROL"           { return mnemonic(ROL_MNEMONIC); }
"ROL.W"         { return mnemonic_wide(ROL_MNEMONIC); }
"ROR"           { return mnemonic(ROR_MNEMONIC); }
"ROR.W"         { return mnemonic_wide(ROR_MNEMONIC); }
"RTI"           { return mnemonic(RTI_MNEMONIC); }
"RTS"           { return mnemonic(RTS_MNEMONIC); }
"SBC"           { return mnemonic(SBC_MNEMONIC); }
"SBC.W"         { return mnemonic_wide(SBC_MNEMONIC); }
"SEC"           { return mnemonic(SEC_MNEMONIC); }
"SED"           { return mnemonic(SED_MNEMONIC); }
"SEI"           { return mnemonic(SEI_MNEMONIC); }
"STA"           { return mnemonic(STA_MNEMONIC); }
"STA.W"         { return mnemonic_wide(STA_MNEMONIC); }
"STX"           { return mnemonic(STX_MNEMONIC); }
"STX.W"         { return mnemonic_wide(STX_MNEMONIC); }
"STY"           { return mnemonic(STY_MNEMONIC); }
"STY.W"         { return mnemonic_wide(STY_MNEMONIC); }
"TAX"           { return mnemonic(TAX_MNEMONIC); }
"TAY"           { return mnemonic(TAY_MNEMONIC); }
"TSX"           { return mnemonic(TSX_MNEMONIC); }
"TXA"           { return mnemonic(TXA_MNEMONIC); }
"TXS"           { return mnemonic(TXS_MNEMONIC); }
"TYA"           { return mnemonic(TYA_MNEMONIC); }
"X"         { return('X'); }
"Y"         { return('Y'); }
"A"         { return('A'); }
"MASK"          { return(MASK); }
"SIZEOF"        { return(SIZEOF); }
{pragma_prefix}"DATA"   { return(DATASEG); }
{pragma_prefix}"CODE"   { return(CODESEG); }
{pragma_prefix}"DATASEG" { return(DATASEG); }
{pragma_prefix}"CODESEG" { return(CODESEG); }
{pragma_prefix}"IF" { return(IF); }
{pragma_prefix}"IFDEF"  { return(IFDEF); }
{pragma_prefix}"IFNDEF" { return(IFNDEF); }
{pragma_prefix}"ELSE"   { return(ELSE); }
{pragma_prefix}"ELIF"   { return(ELIF); }
{pragma_prefix}"ENDIF"  { return(ENDIF); }
{pragma_prefix}"MACRO"  { return(MACRO); }
{pragma_prefix}"END"    { return(END); }
{pragma_prefix}"ENDE"   { return(ENDE); }
{pragma_prefix}"ENDM"   { return(ENDM); }
{pragma_prefix}"ENDP"   { return(ENDP); }
{pragma_prefix}"ENDS"   { return(ENDS); }
{pragma_prefix}"EQU"    { return(EQU); }
{pragma_prefix}"DEFINE" { return(DEFINE); }
{pragma_prefix}"INCLUDE" { return(INCSRC); }
{pragma_prefix}"INCSRC" { return(INCSRC); }
{pragma_prefix}"INCBIN" { return(INCBIN); }
{pragma_prefix}"ALIGN"  { return(ALIGN); }
{pragma_prefix}"PUBLIC" { return(PUBLIC); }
{pragma_prefix}"EXTRN"  { return(EXTRN); }
{pragma_prefix}"ZEROPAGE" { return(ZEROPAGE); }
{pragma_prefix}"CHARMAP" { return(CHARMAP); }
{pragma_prefix}"STRUC"  { return(STRUC); }
{pragma_prefix}"UNION"  { return(UNION); }
{pragma_prefix}"RECORD" { return(RECORD); }
{pragma_prefix}"ENUM"   { return(ENUM); }
{pragma_prefix}"PROC"   { return(PROC); }
{pragma_prefix}"REPT"   { return(REPT); }
{pragma_prefix}"TAG"    { return(TAG); }
{pragma_prefix}"TYPE"   { return(TAG); }
{pragma_prefix}"LABEL"  { return(_LABEL_); }
{pragma_prefix}"MESSAGE" { return(MESSAGE); }
{pragma_prefix}"WARNING" { return(WARNING); }
{pragma_prefix}"ERROR"  { return(ERROR); }
{pragma_prefix}"WHILE"  { return(WHILE); }
{pragma_prefix}"PAD"    { return(DSB); }
{pragma_prefix}"DB" { return(BYTE); }
{pragma_prefix}"BYTE"   { return(BYTE); }
{pragma_prefix}"CHAR"   { return(CHAR); }
{pragma_prefix}"ASC"    { return(CHAR); }
{pragma_prefix}"DW" { return(WORD); }
{pragma_prefix}"WORD"   { return(WORD); }
{pragma_prefix}"DD" { return(DWORD); }
{pragma_prefix}"DWORD"  { return(DWORD); }
{pragma_prefix}"DSB"    { return(DSB); }
{pragma_prefix}"DSW"    { return(DSW); }
{pragma_prefix}"DSD"    { return(DSD); }
{pragma_prefix}"ORG"    { return(ORG); }
{label_prefix}{identifier}":" { yylval.label = maybe_ignorecase(strip_label()); return(LABEL); }
{label_prefix}"@@"{local_id}":" { yylval.label = maybe_ignorecase(strip_label()); return(LOCAL_LABEL); }
"@@"{local_id}      { yylval.ident = maybe_ignorecase(yytext); return(LOCAL_ID); }
{identifier}        { yylval.ident = maybe_ignorecase(yytext); return(IDENTIFIER); }
{string_literal}    { yytext[yyleng-1] = '\0'; yylval.string = &yytext[1]; return(STRING_LITERAL); }
"+"{2,8}        { yylval.ident = yytext; return(FORWARD_BRANCH); }
"-"{2,8}        { yylval.ident = yytext; return(BACKWARD_BRANCH); }
"::"            { return(SCOPE_OP); }
"."         { return('.'); }
\n          { LOCATION_LINES (yylloc, yyleng); LOCATION_STEP (yylloc); return('\n'); }
","         { return(','); }
":"         { return(':'); }
"#"         { return('#'); }
"="         { return('='); }
"("         { return( yyparswap ? '[' : '(' ); }
")"         { return( yyparswap ? ']' : ')' ); }
"["         { return( yyparswap ? '(' : '[' ); }
"]"         { return( yyparswap ? ')' : ']'); }
"{"         { return('{'); }
"}"         { return('}'); }
"&"         { return('&'); }
"!"         { return('!'); }
"~"         { return('~'); }
"-"         { return('-'); }
"+"         { return('+'); }
"*"         { return('*'); }
"/"         { return('/'); }
"%"         { return('%'); }
"<"         { return('<'); }
">"         { return('>'); }
"^"         { return('^'); }
"|"         { return('|'); }
">>"                    { return(SHR_OP); }
"<<"                    { return(SHL_OP); }
"<="                    { return(LE_OP); }
">="                    { return(GE_OP); }
"=="                    { return(EQ_OP); }
"!="                    { return(NE_OP); }
"$"         { return('$'); }
"@"         { return('@'); }
.           { yyerror("Skipping invalid character(s)"); LOCATION_STEP (yylloc); }
%%
/**
 * Describes a file stack record.
 * It holds name, location and buffer state for it.
 */
typedef struct tag_file_stack_rec {
    const char *name; /* Name of the file */
    char *path; /* Absolute path */
    YY_BUFFER_STATE buf;    /* flex input buffer */
    YYLTYPE loc;    /* Current position in file */
    FILE *fp;   /* File handle */
} file_stack_rec;

/* Max. nesting depth */
#define FILE_STACK_SIZE 32

/* Stack of files */
static file_stack_rec file_stack[FILE_STACK_SIZE];

/* File stack pointer */
static int file_stack_ptr;

/* Macro to access top of file stack */
#define FTOS() file_stack[file_stack_ptr]

/**
 * Resets the location described by yylloc.
 */
void yyresetloc(void)
{
    LOCATION_RESET(yylloc);
}

/**
 * Helper function used to handle INCSRC and INCBIN
 * statements during parsing.
 */
FILE *open_included_file(const char *filename, int quoted_form, char **path_out)
{
    FILE *fp = 0;
    char *path = 0;
    if (filename[0] == '/') {
        /* Absolute path */
        fp = fopen(filename, "rt");
        if (fp) {
            char *lastslash = strrchr(filename, '/');
            int len = lastslash - filename;
            assert(len > 0);
            path = (char *)malloc(len + 1);
            strncpy(path, filename, len);
            path[len] = '\0';
        }
    } else {
        /* Not absolute path */
        int i;
        if (quoted_form) {
            /* Try parent includes */
            for (i = file_stack_ptr; i >= 0; --i) {
                const char *include_path = file_stack[i].path;
                char *tmp = (char *)malloc(
                    strlen(include_path) + strlen("/") + strlen(filename) + 1);
                strcpy(tmp, include_path);
                strcat(tmp, "/");
                strcat(tmp, filename);
                fp = fopen(tmp, "rt");
                if (fp) {
                    char *lastslash = strrchr(tmp, '/');
                    int len = lastslash - tmp;
                    assert(len > 0);
                    path = (char *)malloc(len + 1);
                    strncpy(path, tmp, len);
                    path[len] = '\0';
                    free(tmp);
                    break;
                }
                free(tmp);
                /* We only search the current dir for now. */
                break;
            }
        }
        if (!fp) {
            /* Try search paths */
            for (i = 0; i < xasm_args.include_path_count; ++i) {
                const char *include_path = xasm_args.include_paths[i];
                char *tmp = (char *)malloc(
                    strlen(include_path) + strlen("/") + strlen(filename) + 1);
                strcpy(tmp, include_path);
                strcat(tmp, "/");
                strcat(tmp, filename);
                fp = fopen(tmp, "rt");
                if (fp) {
                    char *lastslash = strrchr(tmp, '/');
                    int len = lastslash - tmp;
                    assert(len > 0);
                    path = (char *)malloc(len + 1);
                    strncpy(path, tmp, len);
                    path[len] = '\0';
                    free(tmp);
                    break;
                }
                free(tmp);
             }
        }
    }
    if (path_out)
        *path_out = path;
    else
        free(path);
    return fp;
}

/**
 * Function called by lexer upon EOF(yyin).
 */
int yywrap()
{
    /* Discard of buffer that has been scanned */
    yy_delete_buffer(YY_CURRENT_BUFFER);
    /* Close current file */
    fclose(FTOS().fp);
    free(FTOS().path);
    /* Resume scanning of previous buffer, if any */
    if (file_stack_ptr > 0) {
        /* Pop stack */
        file_stack_ptr--;
        /* Restore location */
        yylloc = FTOS().loc;
        /* Resume scanning from this buffer */
        yy_switch_to_buffer(FTOS().buf);
        /* Not end of all input. */
        return(0);
    }
    else {
        /* We have reached end of all input. */
        return(1);
    }
}

/**
 * Opens a file and resumes scanning from there.
 * The current file's status is pushed on stack.
 * @param filename Name of new file to start scanning
 * @return error code (0, 1 or 2)
 */
int yypushandrestart(const char *filename, int quoted_form)
{
    FILE *fp = 0;
    char *path = 0;
    /* Check for stack overflow */
    if (file_stack_ptr == FILE_STACK_SIZE-1) {
        /* Error, stack overflow */
        return 2;
    }
    fp = open_included_file(filename, quoted_form, &path);
    if (fp) {
        /* Save position in current file */
        FTOS().loc = yylloc;
        /* Push the new file */
        file_stack_ptr++;
        FTOS().name = filename;
        FTOS().path = path;
        FTOS().buf = yy_create_buffer(fp, YY_BUF_SIZE);
        FTOS().fp = fp;
        /* Reset file location */
        yyresetloc();
        /* Resume scanning from the new buffer */
        yy_switch_to_buffer(FTOS().buf);
        /* Success */
        return 0;
    } else {
        /* Error, couldn't open file */
        return 1;
    }
}

/**
 * Initializes the lexer, tells it to start tokenizing from the given file.
 * @param filename Name of file where scanning is initiated
 * @param swap_parens Use ( ) for indirection if 1, [ ] otherwise
 * @return 1 if success, 0 otherwise
 */
int yybegin(const char *filename, int swap_parens, int ignore_case)
{
    FILE *fp;
    yyparswap = swap_parens;
    yyignorecase = ignore_case;
    /* Attempt to open the given file */
    fp = fopen(filename, "rt");
    if (fp) {
        /* Figure out path */
        char *path;
        char *lastslash = strrchr(filename, '/');
        if (filename[0] != '/') {
            /* Not absolute */
            if (lastslash) {
                int len = lastslash - filename;
                assert(len > 0);
                path = (char *)malloc(strlen(xasm_path) + 1 + len + 1);
                strcpy(path, xasm_path);
                strcat(path, "/");
                strncat(path, filename, len);
            } else {
                path = (char *)malloc(strlen(xasm_path) + 1);
                strcpy(path, xasm_path);
            }
        } else {
            /* Absolute */
            int len = lastslash - filename;
            assert(len > 0);
            path = (char *)malloc(len + 1);
            strncpy(path, filename, len);
            path[len] = '\0';
        }
        /* Init stack */
        file_stack_ptr = 0;
        FTOS().name = filename;
        FTOS().path = path;
        FTOS().buf = yy_create_buffer(fp, YY_BUF_SIZE);
        FTOS().fp = fp;
        /* Reset file location */
        yyresetloc();
        /* Use the new buffer */
        yy_switch_to_buffer(FTOS().buf);
        /* Success */
        return 1;
    } else {
        /* Couldn't open file */
        return 0;
    }
}

/**
 * Gets the name of the file that is currently being tokenized.
 */
const char *yy_current_filename()
{
    return FTOS().name;
}

/**
 * Called by the parser to report an error during parsing.
 * @param s Error message
 */
void yyerror(const char *s) {
    extern int yynerrs;
    int i;
    /* Print line of form: file:pos:message */
    fprintf(stderr, "%s:", yy_current_filename());
    LOCATION_PRINT(stderr, yylloc);
    fprintf(stderr, ": error: %s\n", s);
    /* Print include-trace */
    for (i=file_stack_ptr-1; i>=0; i--) {
        fprintf(stderr, "(%s:", file_stack[i].name);
        LOCATION_PRINT(stderr, file_stack[i].loc);
        fprintf(stderr, ")\n");
    }
    ++yynerrs;
}

/**
 * Call when yytext is a label to remove whitespace and punctuation.
 */
char *strip_label()
{
    int start;
    /* Kill the ':' at the end */
    yytext[strlen(yytext)-1] = 0;
    /* Skip the whitespace, if any */
    start = 0;
    while ((yytext[start] == ' ') || (yytext[start] == '\t')) start++;
    return &yytext[start];
}

static char *__strupr(char *str)
{
    char *p = str;
    while (*p) {
        *p = toupper(*p);
        ++p;
    }
    return str;
}

const char *maybe_ignorecase(char *str)
{
    if (yyignorecase)
        __strupr(str);
    return str;
}

char *scan_include(int terminator)
{
    static char buf[1024];
    int i = 0;
    while (1) {
        int c = input();
        if (c == terminator)
            break;
        else if (c == '\n') {
            yyerror("unterminated include filename");
            unput(c);
            break;
        } else if (i == 1023) {
            yyerror("include filename is too long");
            break;
        } else {
            buf[i++] = (char)c;
        }
    }
    buf[i] = '\0';
    LOCATION_STEP (yylloc); 
    return buf;
}
