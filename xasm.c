/*
 * $Id: xasm.c,v 1.22 2007/11/11 22:35:22 khansen Exp $
 * $Log: xasm.c,v $
 * Revision 1.22  2007/11/11 22:35:22  khansen
 * compile on mac
 *
 * Revision 1.21  2007/08/19 11:18:56  khansen
 * --case-insensitive option
 *
 * Revision 1.20  2007/08/12 18:58:12  khansen
 * ability to generate pure 6502 binary (--pure-binary switch)
 *
 * Revision 1.19  2007/08/11 01:24:36  khansen
 * includepaths support (-I option)
 *
 * Revision 1.18  2007/08/10 20:21:02  khansen
 * *** empty log message ***
 *
 * Revision 1.17  2007/08/07 22:42:53  khansen
 * version
 *
 * Revision 1.16  2007/07/22 14:49:40  khansen
 * don't crash in change_extension()
 *
 * Revision 1.15  2007/07/22 13:33:26  khansen
 * convert tabs to whitespaces
 *
 * Revision 1.14  2005/01/09 11:19:23  kenth
 * xorcyst 1.4.5
 *
 * Revision 1.13  2005/01/05 09:37:32  kenth
 * xorcyst 1.4.4
 *
 * Revision 1.12  2005/01/05 01:52:13  kenth
 * xorcyst 1.4.3
 *
 * Revision 1.11  2005/01/04 21:35:10  kenth
 * return error code from main() when error count > 0
 *
 * Revision 1.10  2004/12/29 21:43:50  kenth
 * xorcyst 1.4.2
 *
 * Revision 1.9  2004/12/25 02:23:19  kenth
 * xorcyst 1.4.1
 *
 * Revision 1.8  2004/12/19 19:58:46  kenth
 * xorcyst 1.4.0
 *
 * Revision 1.7  2004/12/18 17:01:21  kenth
 * --debug switch, multiple verbose levels
 *
 * Revision 1.6  2004/12/16 13:20:35  kenth
 * xorcyst 1.3.5
 *
 * Revision 1.5  2004/12/14 01:50:12  kenth
 * xorcyst 1.3.0
 *
 * Revision 1.4  2004/12/11 02:06:27  kenth
 * xorcyst 1.2.0
 *
 * Revision 1.3  2004/12/06 04:53:02  kenth
 * xorcyst 1.1.0
 *
 * Revision 1.2  2004/06/30 23:37:54  kenth
 * replaced argp with something else
 *
 * Revision 1.1  2004/06/30 07:56:02  kenth
 * Initial revision
 *
 */

/**
 *    (C) 2004 Kent Hansen
 *
 *    The XORcyst is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    The XORcyst is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with The XORcyst; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * The main program.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "getopt.h"
#include "astnode.h"
#include "astproc.h"
#include "symtab.h"
#include "codegen.h"
#include "listing.h"
#include "xasm.h"

/*---------------------------------------------------------------------------*/

/* Parser stuff we need. */
int yyparse(void);
extern int yydebug;
extern int yynerrs;

/* Scanner stuff we need. */
int yybegin(const char *, int, int);

/* Other. */
astnode *root_node;
static symtab *symbol_table;
char *xasm_path;

/*---------------------------------------------------------------------------*/
/* Argument parsing stuff. */

static char program_version[] = "xasm 1.6.1";

/* Argument variables set by arg parser. */
xasm_arguments xasm_args;

/* Long options for getopt_long(). */
static struct option long_options[] = {
  { "define",   required_argument, 0, 'D' },
  { "include-path", required_argument, 0, 'I' },
  { "output",   required_argument, 0, 'o' },
  { "listing",  required_argument, 0, 'L' },
  { "lst",      required_argument, 0, 'L' },
  { "listing-format", required_argument, 0, 0 },
  { "xref", required_argument, 0, 0 },
  { "xref-format", required_argument, 0, 0 },
  { "xref-include-locals", required_argument, 0, 0 },
  { "xref-include-anon", required_argument, 0, 0 },
  { "audit-raw-addresses", no_argument, 0, 0 },
  { "audit-level", required_argument, 0, 0 },
  { "audit-rom-range", required_argument, 0, 0 },
  { "audit-output-format", required_argument, 0, 0 },
  { "compare", required_argument, 0, 0 },
  { "compare-offset", required_argument, 0, 0 },
  { "compare-length", required_argument, 0, 0 },
  { "compare-max-mismatches", required_argument, 0, 0 },
  { "compare-format", required_argument, 0, 0 },
  { "compare-cpu-base", required_argument, 0, 0 },
  { "quiet",    no_argument, 0, 'q' },
  { "silent",   no_argument, 0, 's' },
  { "verbose",  no_argument, 0, 'v' },
  { "debug",    no_argument, 0, 'g' },
  { "help", no_argument, 0, 0 },
  { "usage",    no_argument, 0, 0 },
  { "version",  no_argument, 0, 'V' },
  { "swap-parens", no_argument, 0, 0 },
  { "pure-binary", no_argument, 0, 0 },
  { "case-insensitive", no_argument, 0, 0 },
  { "no-warn",  no_argument, 0, 0 },
  { "warn-unused-equ", no_argument, 0, 0 },
  { "Werror", required_argument, 0, 0 },
  { "Wno-unused-equ", no_argument, 0, 0 },
  { 0 }
};

/* Prints usage message and exits. */
static void usage()
{
    printf("\
Usage: xasm [-gqsvV] [-D IDENT[=VALUE]] [--define=IDENT]\n\
            [-o FILE] [--output=FILE] [--pure-binary]\n\
            [--include-path=DIR] [-I DIR] [--swap-parens]\n\
            [--case-insensitive] [--listing=FILE]\n\
            [--listing-format=text|json|ndjson]\n\
            [--xref=FILE] [--xref-format=text|csv|json]\n\
            [--xref-include-locals=true|false]\n\
            [--xref-include-anon=true|false]\n\
            [--audit-raw-addresses] [--audit-level=warn|error]\n\
            [--audit-rom-range=LO-HI] [--audit-output-format=text|json]\n\
            [--compare=FILE] [--compare-format=text|json]\n\
            [--compare-offset=N] [--compare-length=N]\n\
            [--compare-max-mismatches=N] [--compare-cpu-base=ADDR]\n\
            [--warn-unused-equ] [--Werror=unused-equ] [--Wno-unused-equ]\n\
            [--no-warn] [--verbose] [--quiet] [--silent] \n\
            [--debug] [--help] [--usage] [--version]\n\
            FILE\n\
");
    exit(0);
}

/* Prints help message and exits. */
static void help()
{
    printf("\
Usage: xasm [OPTION...] FILE\n\
The XORcyst Assembler -- it kicks the 6502's ass\n\
\n\
-D, --define=IDENT[=VALUE] Define IDENT\n\
-I, --include-path=DIR     Specify a search path for include files\n\
-o, --output=FILE          Output to FILE instead of standard output\n\
    --listing=FILE         Generate listing file\n\
    --listing-format=FMT   Listing format: text|json|ndjson\n\
    --xref=FILE            Generate cross-reference output\n\
    --xref-format=FMT      Xref format: text|csv|json\n\
    --xref-include-locals=BOOL\n\
                            Include local labels in xref (default false)\n\
    --xref-include-anon=BOOL\n\
                            Include anonymous labels in xref (default false)\n\
    --audit-raw-addresses   Enable raw-address audit diagnostics\n\
    --audit-level=LVL       Audit severity level: warn|error\n\
    --audit-rom-range=RNG   Audit ROM range, e.g. $C000-$FFFF\n\
    --audit-output-format=FMT\n\
                            Audit output format: text|json\n\
    --compare=FILE         Compare assembled output with FILE\n\
    --compare-offset=N     Compare start offset in bytes (default 0)\n\
    --compare-length=N     Compare byte count (default auto)\n\
    --compare-max-mismatches=N\n\
                            Maximum mismatches reported (default 1)\n\
    --compare-format=FMT   Compare output format: text|json\n\
    --compare-cpu-base=ADDR\n\
                            Fallback CPU base address (e.g. $C000)\n\
    --warn-unused-equ      Warn on unused EQU symbols (W0201)\n\
    --Werror=unused-equ    Promote unused EQU warnings to errors\n\
    --Wno-unused-equ       Disable unused EQU warnings\n\
    --pure-binary          Output pure 6502 binary\n\
    --swap-parens          Use ( ) instead of [ ] for indirection\n\
    --case-insensitive     Case-insensitive identifiers\n\
    --no-warn              Suppress warnings\n\
-q, -s, --quiet, --silent  Don't produce any output\n\
-v, --verbose              Produce verbose output\n\
-g, --debug                Retain file locations\n\
    --help                 Give this help list\n\
    --usage                Give a short usage message\n\
-V, --version              Print program version\n\
\n\
Mandatory or optional arguments to long options are also mandatory or optional\n\
for any corresponding short options.\n\
\n\
Report bugs to <kentmhan@gmail.com>.\n\
");
    exit(0);
}

/* Prints version and exits. */
static void version()
{
    printf("%s\n", program_version);
    exit(0);
}

static void cli_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(2);
}

static int parse_long_value(const char *s, long *out)
{
    char *endptr;
    long v;
    if (s == NULL || out == NULL || *s == '\0') {
        return 0;
    }
    if (s[0] == '$') {
        v = strtol(s + 1, &endptr, 16);
    } else {
        v = strtol(s, &endptr, 0);
    }
    if (*endptr != '\0') {
        return 0;
    }
    *out = v;
    return 1;
}

static int parse_listing_format_value(const char *value, listing_format *out)
{
    if (strcmp(value, "text") == 0) {
        *out = LISTING_FORMAT_TEXT;
        return 1;
    }
    if (strcmp(value, "json") == 0) {
        *out = LISTING_FORMAT_JSON;
        return 1;
    }
    if (strcmp(value, "ndjson") == 0) {
        *out = LISTING_FORMAT_NDJSON;
        return 1;
    }
    return 0;
}

static int parse_compare_format_value(const char *value, compare_format *out)
{
    if (strcmp(value, "text") == 0) {
        *out = COMPARE_FORMAT_TEXT;
        return 1;
    }
    if (strcmp(value, "json") == 0) {
        *out = COMPARE_FORMAT_JSON;
        return 1;
    }
    return 0;
}

static int parse_xref_format_value(const char *value, xref_format *out)
{
    if (strcmp(value, "json") == 0) {
        *out = XREF_FORMAT_JSON;
        return 1;
    }
    if (strcmp(value, "text") == 0) {
        *out = XREF_FORMAT_TEXT;
        return 1;
    }
    if (strcmp(value, "csv") == 0) {
        *out = XREF_FORMAT_CSV;
        return 1;
    }
    return 0;
}

static int parse_bool_value(const char *value, int *out)
{
    if (strcmp(value, "true") == 0) {
        *out = 1;
        return 1;
    }
    if (strcmp(value, "false") == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int parse_audit_level_value(const char *value, int *out_level_error)
{
    if (strcmp(value, "warn") == 0) {
        *out_level_error = 0;
        return 1;
    }
    if (strcmp(value, "error") == 0) {
        *out_level_error = 1;
        return 1;
    }
    return 0;
}

static int parse_audit_output_value(const char *value, int *out_json)
{
    if (strcmp(value, "text") == 0) {
        *out_json = 0;
        return 1;
    }
    if (strcmp(value, "json") == 0) {
        *out_json = 1;
        return 1;
    }
    return 0;
}

static int parse_audit_rom_range_value(const char *value, long *out_lo, long *out_hi)
{
    const char *dash;
    char lo_buf[64];
    char hi_buf[64];
    long lo;
    long hi;
    int lo_len;
    int hi_len;

    if (value == NULL || out_lo == NULL || out_hi == NULL) {
        return 0;
    }

    dash = strchr(value, '-');
    if (dash == NULL || dash == value || *(dash + 1) == '\0') {
        return 0;
    }
    lo_len = (int)(dash - value);
    hi_len = (int)strlen(dash + 1);
    if (lo_len <= 0 || hi_len <= 0 || lo_len >= (int)sizeof(lo_buf) || hi_len >= (int)sizeof(hi_buf)) {
        return 0;
    }
    memcpy(lo_buf, value, (size_t)lo_len);
    lo_buf[lo_len] = '\0';
    memcpy(hi_buf, dash + 1, (size_t)hi_len);
    hi_buf[hi_len] = '\0';

    if (!parse_long_value(lo_buf, &lo) || !parse_long_value(hi_buf, &hi)) {
        return 0;
    }
    if (lo < 0 || hi < 0 || hi < lo) {
        return 0;
    }
    *out_lo = lo;
    *out_hi = hi;
    return 1;
}

static void warn_global(const char *code, const char *fmt, ...)
{
    va_list ap;
    if (xasm_args.no_warn) {
        return;
    }
    va_start(ap, fmt);
    fprintf(stderr, "warning %s: ", code);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/**
 * Checks if a character is alpha (a-z, A-Z).
 */
static int __isalpha(char c)
{
    return ( ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) );
}

/**
 * Checks if a character is alpha (a-z, A-Z) or numeric (0-9).
 */
static int __isalnum(char c)
{
    return ( __isalpha(c) || ((c >= '0') && (c <= '9')) );
}

/**
 * Checks that an identifier matches the regexp [a-zA-Z_][a-zA-Z0-9_]*
 * @param id Identifier to validate
 * @return 1 if OK, 0 otherwise
 */
static int validate_ident(char *id)
{
    int i;
    char c;
    c = id[0];
    if ( !__isalpha(c) && (c != '_') ) {
        return 0;
    }
    for (i=1; i<strlen(id); i++) {
        c = id[i];
        if ( !__isalnum(c) && (c != '_') ) {
            return 0;
        }
    }
    return 1;   /* OK */
}

/* Parses program arguments. */
static void
parse_arguments (int argc, char **argv)
{
    int key;
    /* getopt_long stores the option index here. */
    int index = 0;

    /* Set default values. */
    xasm_args.debug = 0;
    xasm_args.silent = 0;
    xasm_args.verbose = 0;
    xasm_args.swap_parens = 0;
    xasm_args.pure_binary = 0;
    xasm_args.case_insensitive = 0;
    xasm_args.no_warn = 0;
    xasm_args.warn_unused_equ = 0;
    xasm_args.werror_unused_equ = 0;
    xasm_args.audit_raw_addresses = 0;
    xasm_args.audit_level_error = 0;
    xasm_args.audit_output_json = 0;
    xasm_args.audit_rom_range_set = 0;
    xasm_args.audit_rom_lo = 0;
    xasm_args.audit_rom_hi = 0;
    xasm_args.input_file = NULL;
    xasm_args.output_file = NULL;
    xasm_args.include_paths = NULL;
    xasm_args.include_path_count = 0;
    xasm_args.listing_file = NULL;
    xasm_args.listing_format = LISTING_FORMAT_TEXT;
    xasm_args.listing_format_set = 0;
    xasm_args.compare_file = NULL;
    xasm_args.compare_offset = 0;
    xasm_args.compare_length = -1;
    xasm_args.compare_max_mismatches = 1;
    xasm_args.compare_format = COMPARE_FORMAT_TEXT;
    xasm_args.compare_cpu_base_set = 0;
    xasm_args.compare_cpu_base = 0;
    xasm_args.xref_file = NULL;
    xasm_args.xref_format = XREF_FORMAT_JSON;
    xasm_args.xref_include_locals = 0;
    xasm_args.xref_include_anon = 0;

    /* Parse options. */
    while ((key = getopt_long(argc, argv, "D:I:L:o:qsvV", long_options, &index)) != -1) {
        switch (key) {
            case 'L':
            xasm_args.listing_file = optarg;
            break;

            case 'g':
            xasm_args.debug = 1;
            break;

            case 'q': case 's':
            xasm_args.silent = 1;
            break;

            case 'v':
            xasm_args.verbose++;
            break;

            case 'o':
            xasm_args.output_file = optarg;
            break;

            case 'D': {
                char *id;
                char *str;
                astnode *val;
                static location loc = { 0, 0, 0, 0, NULL };
                if (strchr(optarg, '=') != NULL) {
                    /* IDENT=VALUE */
                    id = strtok(optarg, "=");
                    str = strtok(NULL, "\0");
                    if (str) {
                        /* Parse the value */
                        if (str[0] == '\"') {
                            /* Assume string */
                            str = strtok(&str[1], "\"");
                            val = astnode_create_string(str, loc);
                        } else {
                            /* Assume integer */
                            val = astnode_create_integer(strtol(str, NULL, 0), loc);
                        }
                    } else {
                        /* No value given -- use empty string */
                        val = astnode_create_string("", loc);
                    }
                } else {
                    id = optarg;
                    val = astnode_create_integer(0, loc);
                }
                if (validate_ident(id)) {
                    symtab_entry *e;
                    e = symtab_lookup(id);
                    if (e == NULL) {
                        symtab_enter(id, CONSTANT_SYMBOL, val, 0);
                    } else {
                        /* Error, redefinition */
                        fprintf(stderr, "--ident: `%s' already defined\n", id);
                    }
                } else {
                    /* Error, bad identifier */
                    fprintf(stderr, "--ident: `%s' is not a valid identifier\n", id);
                }
            }
            break;

            case 'I': {
                char *p;
                int count = xasm_args.include_path_count + 1;
                xasm_args.include_paths = (char **)realloc(
                    xasm_args.include_paths, sizeof(const char *) * count);
                p = (char *)malloc(strlen(optarg) + 1);
                strcpy(p, optarg);
                xasm_args.include_paths[count-1] = p;
                xasm_args.include_path_count = count;
            }
            break;

            case 0:
            /* Use index to differentiate between options */
            if (strcmp(long_options[index].name, "usage") == 0) {
                usage();
            } else if (strcmp(long_options[index].name, "help") == 0) {
                help();
            } else if (strcmp(long_options[index].name, "swap-parens") == 0) {
                xasm_args.swap_parens = 1;
            } else if (strcmp(long_options[index].name, "pure-binary") == 0) {
                xasm_args.pure_binary = 1;
            } else if (strcmp(long_options[index].name, "case-insensitive") == 0) {
                xasm_args.case_insensitive = 1;
            } else if (strcmp(long_options[index].name, "no-warn") == 0) {
                xasm_args.no_warn = 1;
            } else if (strcmp(long_options[index].name, "warn-unused-equ") == 0) {
                xasm_args.warn_unused_equ = 1;
            } else if (strcmp(long_options[index].name, "Werror") == 0) {
                if (strcmp(optarg, "unused-equ") == 0) {
                    xasm_args.warn_unused_equ = 1;
                    xasm_args.werror_unused_equ = 1;
                } else {
                    cli_error("invalid value for --Werror: `%s' (expected unused-equ)", optarg);
                }
            } else if (strcmp(long_options[index].name, "Wno-unused-equ") == 0) {
                xasm_args.warn_unused_equ = 0;
                xasm_args.werror_unused_equ = 0;
            } else if (strcmp(long_options[index].name, "audit-raw-addresses") == 0) {
                xasm_args.audit_raw_addresses = 1;
            } else if (strcmp(long_options[index].name, "audit-level") == 0) {
                if (!parse_audit_level_value(optarg, &xasm_args.audit_level_error)) {
                    cli_error("invalid value for --audit-level: `%s' (expected warn|error)", optarg);
                }
            } else if (strcmp(long_options[index].name, "audit-rom-range") == 0) {
                if (!parse_audit_rom_range_value(optarg, &xasm_args.audit_rom_lo, &xasm_args.audit_rom_hi)) {
                    cli_error("invalid value for --audit-rom-range: `%s' (expected LO-HI)", optarg);
                }
                xasm_args.audit_rom_range_set = 1;
            } else if (strcmp(long_options[index].name, "audit-output-format") == 0) {
                if (!parse_audit_output_value(optarg, &xasm_args.audit_output_json)) {
                    cli_error("invalid value for --audit-output-format: `%s' (expected text|json)", optarg);
                }
            } else if (strcmp(long_options[index].name, "listing-format") == 0) {
                listing_format fmt;
                if (!parse_listing_format_value(optarg, &fmt)) {
                    cli_error("invalid value for --listing-format: `%s' (expected text|json|ndjson)", optarg);
                }
                xasm_args.listing_format = fmt;
                xasm_args.listing_format_set = 1;
            } else if (strcmp(long_options[index].name, "xref") == 0) {
                xasm_args.xref_file = optarg;
            } else if (strcmp(long_options[index].name, "xref-format") == 0) {
                xref_format fmt;
                if (!parse_xref_format_value(optarg, &fmt)) {
                    cli_error("invalid value for --xref-format: `%s' (expected text|csv|json)", optarg);
                }
                xasm_args.xref_format = fmt;
            } else if (strcmp(long_options[index].name, "xref-include-locals") == 0) {
                if (!parse_bool_value(optarg, &xasm_args.xref_include_locals)) {
                    cli_error("invalid value for --xref-include-locals: `%s' (expected true|false)", optarg);
                }
            } else if (strcmp(long_options[index].name, "xref-include-anon") == 0) {
                if (!parse_bool_value(optarg, &xasm_args.xref_include_anon)) {
                    cli_error("invalid value for --xref-include-anon: `%s' (expected true|false)", optarg);
                }
            } else if (strcmp(long_options[index].name, "compare") == 0) {
                xasm_args.compare_file = optarg;
            } else if (strcmp(long_options[index].name, "compare-offset") == 0) {
                if (!parse_long_value(optarg, &xasm_args.compare_offset) || xasm_args.compare_offset < 0) {
                    cli_error("invalid value for --compare-offset: `%s' (must be >= 0)", optarg);
                }
            } else if (strcmp(long_options[index].name, "compare-length") == 0) {
                if (!parse_long_value(optarg, &xasm_args.compare_length) || xasm_args.compare_length < 0) {
                    cli_error("invalid value for --compare-length: `%s' (must be >= 0)", optarg);
                }
            } else if (strcmp(long_options[index].name, "compare-max-mismatches") == 0) {
                long max_mismatches;
                if (!parse_long_value(optarg, &max_mismatches) || max_mismatches <= 0) {
                    cli_error("invalid value for --compare-max-mismatches: `%s' (must be >= 1)", optarg);
                }
                xasm_args.compare_max_mismatches = (int)max_mismatches;
            } else if (strcmp(long_options[index].name, "compare-format") == 0) {
                compare_format fmt;
                if (!parse_compare_format_value(optarg, &fmt)) {
                    cli_error("invalid value for --compare-format: `%s' (expected text|json)", optarg);
                }
                xasm_args.compare_format = fmt;
            } else if (strcmp(long_options[index].name, "compare-cpu-base") == 0) {
                long cpu_base;
                if (!parse_long_value(optarg, &cpu_base) || cpu_base < 0) {
                    cli_error("invalid value for --compare-cpu-base: `%s' (must be >= 0)", optarg);
                }
                xasm_args.compare_cpu_base_set = 1;
                xasm_args.compare_cpu_base = cpu_base;
            }
            break;

            case 'V':
            version();
            break;

            case '?':
            /* Error message has been printed by getopt_long */
            exit(1);
            break;

            default:
            /* Forgot to handle a short option, most likely */
            fprintf(stderr, "internal error: unhandled option `%c'\n", key);
            exit(1);
            break;
        }
    }

    /* Must be one additional argument, which is the input file. */
    if (argc-1 != optind) {
        printf("Usage: xasm [OPTION...] FILE\nTry `xasm --help' or `xasm --usage' for more information.\n");
        exit(1);
    }
    else {
        xasm_args.input_file = argv[optind];
    }

    if (xasm_args.listing_file == NULL && xasm_args.listing_format_set) {
        warn_global("W0017", "--listing-format is ignored because --listing is not set");
    }
}

/*---------------------------------------------------------------------------*/

/**
 * Changes the extension of a filename.
 * @param infile Filename whose extension to change
 * @param ext New extension
 * @param outfile Destination filename
 */
static void change_extension(const char *infile, const char *ext, char *outfile)
{
    char *p;
    /* Find the last dot. */
    p = strrchr(infile, '.');
    if (p == NULL) {
        /* There is no dot, simply concatenate extension. */
        sprintf(outfile, "%s.%s", infile, ext);
    }
    else {
        /* Copy the name up to and including the last dot */
        strncpy(outfile, infile, p - infile + 1);
        outfile[p - infile + 1] = '\0';
        /* Then concatenate the extension. */
        strcat(outfile, ext);
    }
}

/*---------------------------------------------------------------------------*/

/**
 * Prints message only if --verbose option was given to assembler.
 */
static void verbose(const char *s)
{
    if (xasm_args.verbose) {
        printf("%s\n", s);
    }
}

/**
 * Gets total number of errors (parsing + semantics).
 */
static int total_errors()
{
    return yynerrs + astproc_err_count();
}

#define COMPARE_SOURCE_TEXT_MAX 1024

typedef struct tag_compare_mismatch {
    long index;
    long output_offset;
    unsigned char expected;
    unsigned char actual;
    int has_cpu_address;
    long cpu_address;
    int has_source;
    listing_lookup_result source;
    char source_text[COMPARE_SOURCE_TEXT_MAX];
} compare_mismatch;

static int read_file_bytes(const char *path, unsigned char **data, long *size)
{
    FILE *fp;
    long n;
    unsigned char *buf = NULL;
    size_t got;

    if (path == NULL || data == NULL || size == NULL) {
        return 0;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    n = ftell(fp);
    if (n < 0) {
        fclose(fp);
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }

    if (n > 0) {
        buf = (unsigned char *)malloc((size_t)n);
        if (buf == NULL) {
            fclose(fp);
            return 0;
        }
        got = fread(buf, 1, (size_t)n, fp);
        if ((long)got != n) {
            free(buf);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    *data = buf;
    *size = n;
    return 1;
}

static void read_source_line(const char *filename, int line, char *buf, int buf_size)
{
    FILE *fp;
    char *p;
    int current;

    if (buf == NULL || buf_size <= 0) {
        return;
    }
    buf[0] = '\0';

    if (filename == NULL || line <= 0) {
        return;
    }

    fp = fopen(filename, "r");
    if (fp == NULL) {
        return;
    }

    current = 0;
    while (current < line) {
        if (fgets(buf, buf_size, fp) == NULL) {
            buf[0] = '\0';
            fclose(fp);
            return;
        }
        current++;
    }
    fclose(fp);

    p = strrchr(buf, '\n');
    if (p != NULL) {
        *p = '\0';
    }
    p = strrchr(buf, '\r');
    if (p != NULL) {
        *p = '\0';
    }
}

static void print_json_escaped(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    fputc('"', fp);
    if (p != NULL) {
        while (*p != '\0') {
            unsigned char c = *p++;
            switch (c) {
                case '\"': fputs("\\\"", fp); break;
                case '\\': fputs("\\\\", fp); break;
                case '\b': fputs("\\b", fp); break;
                case '\f': fputs("\\f", fp); break;
                case '\n': fputs("\\n", fp); break;
                case '\r': fputs("\\r", fp); break;
                case '\t': fputs("\\t", fp); break;
                default:
                if (c < 0x20) {
                    fprintf(fp, "\\u%04X", c);
                } else {
                    fputc(c, fp);
                }
                break;
            }
        }
    }
    fputc('"', fp);
}

static int run_compare(astnode *root)
{
    unsigned char *assembled_data = NULL;
    unsigned char *reference_data = NULL;
    long assembled_size = 0;
    long reference_size = 0;
    long available_assembled;
    long available_reference;
    long compared_length;
    long i;
    int mismatch_total = 0;
    int mismatch_reported = 0;
    int max_mismatches = xasm_args.compare_max_mismatches;
    compare_mismatch *mismatches = NULL;
    int exit_code = 0;

    if (!read_file_bytes(xasm_args.output_file, &assembled_data, &assembled_size)) {
        fprintf(stderr, "error: could not read `%s'\n", xasm_args.output_file);
        return 3;
    }
    if (!read_file_bytes(xasm_args.compare_file, &reference_data, &reference_size)) {
        fprintf(stderr, "error: could not read `%s'\n", xasm_args.compare_file);
        free(assembled_data);
        return 3;
    }

    available_assembled = (xasm_args.compare_offset < assembled_size)
                        ? (assembled_size - xasm_args.compare_offset)
                        : 0;
    available_reference = (xasm_args.compare_offset < reference_size)
                        ? (reference_size - xasm_args.compare_offset)
                        : 0;

    if (xasm_args.compare_length >= 0) {
        compared_length = xasm_args.compare_length;
    } else {
        compared_length = (available_assembled < available_reference)
                        ? available_assembled
                        : available_reference;
    }
    if (compared_length > available_assembled) {
        compared_length = available_assembled;
    }
    if (compared_length > available_reference) {
        compared_length = available_reference;
    }
    if (compared_length < 0) {
        compared_length = 0;
    }

    mismatches = (compare_mismatch *)calloc((size_t)max_mismatches, sizeof(compare_mismatch));
    if (mismatches == NULL) {
        fprintf(stderr, "error: out of memory while comparing output\n");
        free(assembled_data);
        free(reference_data);
        return 3;
    }

    for (i = 0; i < compared_length; ++i) {
        long output_offset = xasm_args.compare_offset + i;
        unsigned char expected = reference_data[output_offset];
        unsigned char actual = assembled_data[output_offset];
        if (expected != actual) {
            compare_mismatch *m;
            listing_lookup_result lookup;

            mismatch_total++;
            if (mismatch_reported >= max_mismatches) {
                break;
            }

            m = &mismatches[mismatch_reported];
            m->index = mismatch_total;
            m->output_offset = output_offset;
            m->expected = expected;
            m->actual = actual;
            m->has_cpu_address = 0;
            m->cpu_address = 0;
            m->has_source = 0;
            m->source.file = NULL;
            m->source.line = 0;
            m->source.column = 0;
            m->source.cpu_address = 0;
            m->source.cpu_address_known = 0;
            m->source_text[0] = '\0';

            if (xasm_args.pure_binary && listing_lookup_output_offset(root, output_offset, &lookup)) {
                m->has_source = 1;
                m->source = lookup;
                read_source_line(lookup.file, lookup.line, m->source_text, sizeof(m->source_text));
                if (lookup.cpu_address_known) {
                    m->has_cpu_address = 1;
                    m->cpu_address = (long)(lookup.cpu_address & 0xFFFF);
                }
            }

            if (!m->has_cpu_address && xasm_args.compare_cpu_base_set) {
                m->has_cpu_address = 1;
                m->cpu_address = xasm_args.compare_cpu_base + output_offset;
            }

            mismatch_reported++;
        }
    }

    if (xasm_args.compare_format == COMPARE_FORMAT_JSON) {
        int j;
        printf("{\n");
        printf("  \"version\": \"1\",\n");
        printf("  \"reference_file\": ");
        print_json_escaped(stdout, xasm_args.compare_file);
        printf(",\n");
        printf("  \"assembled_file\": ");
        print_json_escaped(stdout, xasm_args.output_file);
        printf(",\n");
        printf("  \"compared_length\": %ld,\n", compared_length);
        printf("  \"match\": %s,\n", (mismatch_total == 0) ? "true" : "false");
        printf("  \"mismatches\": [");
        for (j = 0; j < mismatch_reported; ++j) {
            compare_mismatch *m = &mismatches[j];
            if (j > 0) {
                printf(",");
            }
            printf("\n    {");
            printf("\"index\": %ld,", m->index);
            printf("\"output_offset\": %ld,", m->output_offset);
            if (m->has_cpu_address) {
                printf("\"cpu_address\": \"0x%04lX\",", m->cpu_address & 0xFFFF);
            } else {
                printf("\"cpu_address\": null,");
            }
            printf("\"expected_hex\": \"%02X\",", m->expected);
            printf("\"actual_hex\": \"%02X\",", m->actual);
            printf("\"source\": ");
            if (m->has_source) {
                printf("{\"file\": ");
                print_json_escaped(stdout, m->source.file != NULL ? m->source.file : "");
                printf(", \"line\": %d, \"column\": %d, \"source_text\": ", m->source.line, m->source.column);
                print_json_escaped(stdout, m->source_text);
                printf("}");
            } else {
                printf("null");
            }
            printf("}");
        }
        if (mismatch_reported > 0) {
            printf("\n");
        }
        printf("  ]\n");
        printf("}\n");
    } else if (mismatch_total > 0) {
        int j;
        for (j = 0; j < mismatch_reported; ++j) {
            compare_mismatch *m = &mismatches[j];
            if (m->has_cpu_address) {
                printf("mismatch #%ld at output+0x%04lX (CPU $%04lX): expected %02X got %02X\n",
                       m->index,
                       m->output_offset & 0xFFFF,
                       m->cpu_address & 0xFFFF,
                       m->expected,
                       m->actual);
            } else {
                printf("mismatch #%ld at output+0x%04lX: expected %02X got %02X\n",
                       m->index,
                       m->output_offset & 0xFFFF,
                       m->expected,
                       m->actual);
            }
            if (m->has_source) {
                printf("source: %s:%d:%d  %s\n",
                       m->source.file != NULL ? m->source.file : "",
                       m->source.line,
                       m->source.column,
                       m->source_text);
            }
        }
    }

    if (mismatch_total > 0) {
        exit_code = 5;
    }

    free(mismatches);
    free(assembled_data);
    free(reference_data);
    return exit_code;
}

/**
 * Program entrypoint.
 */
int main(int argc, char *argv[]) {
    FILE *output_fp;
    int output_generated = 0;
    int exit_code = 0;
    char *default_outfile = 0;

    /* Working directory is needed for include statements */
    xasm_path = getcwd(NULL, 0);

    /* Create global symbol table (auto-pushed on stack) */
    symbol_table = symtab_create();

    /* Parse our arguments. */
    parse_arguments (argc, argv);

    /* Open input for scanning */
    if (!yybegin(xasm_args.input_file,
                 xasm_args.swap_parens,
                 xasm_args.case_insensitive)) {
        fprintf(stderr, "error: could not open `%s' for reading\n", xasm_args.input_file);
        symtab_finalize(symbol_table);
        return(1);
    }

 /* Parse it into a syntax tree */
    //yydebug = -1;
    verbose("Parsing input...");
    yyparse();

    if (root_node == NULL) {
        symtab_finalize(symbol_table);
        return(0);
    }

    /* First pass does a lot of stuff. */
    verbose("First pass...");
    astproc_first_pass(root_node);

    /* Second pass does more stuff. */
    verbose("Second pass...");
    astproc_second_pass(root_node);

    /* Third pass is fun. */
    verbose("Third pass...");
    astproc_third_pass(root_node);

    if (xasm_args.pure_binary) {
        /* Do another pass to prepare for writing pure 6502 */
        verbose("Fourth pass...");
        astproc_fourth_pass(root_node);
    }

    /* Print the final AST (debugging) */
//    astnode_print(root_node, 0);

    /* If no errors, proceed with code generation. */
    if (total_errors() == 0) {
        if (xasm_args.output_file == NULL) {
            /* Create default name of output */
            const char *default_ext = "o";
            int default_outfile_len = strlen(xasm_args.input_file)
                                    + /*dot*/1 + strlen(default_ext) + 1;
            default_outfile = (char *)malloc(default_outfile_len);
            change_extension(xasm_args.input_file, default_ext, default_outfile);
            xasm_args.output_file = default_outfile;
        }
        /* Attempt to open file for writing */
        output_fp = fopen(xasm_args.output_file, "wb");
        if (output_fp == NULL) {
            fprintf(stderr, "error: could not open `%s' for writing\n", xasm_args.output_file);
        } else {
            verbose("Generating final output...");
            if (xasm_args.pure_binary) {
                astproc_fifth_pass(root_node, output_fp);
            } else {
                codegen_write(root_node, output_fp);
            }
            fclose(output_fp);
            if (total_errors() != 0) {
                remove(xasm_args.output_file);
                output_generated = 0;
            } else {
                output_generated = 1;
            }
        }
    }

    if (output_generated && xasm_args.listing_file != NULL) {
        verbose("Generating listing...");
        if (!generate_listing(root_node,
                              xasm_args.listing_file,
                              (listing_format)xasm_args.listing_format,
                              xasm_args.input_file,
                              xasm_args.output_file)) {
            exit_code = 3;
        }
    }

    if ((exit_code == 0) && output_generated && xasm_args.xref_file != NULL) {
        verbose("Generating xref...");
        if (!generate_xref(root_node,
                           xasm_args.xref_file,
                           (xref_format)xasm_args.xref_format,
                           xasm_args.xref_include_locals,
                           xasm_args.xref_include_anon,
                           xasm_args.input_file,
                           xasm_args.output_file,
                           xasm_args.pure_binary)) {
            exit_code = 3;
        }
    }

    if ((exit_code == 0) && output_generated && xasm_args.audit_raw_addresses) {
        int findings;
        verbose("Running raw-address audit...");
        findings = run_raw_address_audit(root_node,
                                         xasm_args.audit_level_error,
                                         xasm_args.audit_output_json,
                                         xasm_args.audit_rom_range_set,
                                         xasm_args.audit_rom_lo,
                                         xasm_args.audit_rom_hi);
        if (findings < 0) {
            exit_code = 3;
        } else if (xasm_args.audit_level_error && findings > 0) {
            exit_code = 4;
        }
    }

    if ((exit_code == 0) && output_generated && xasm_args.compare_file != NULL) {
        verbose("Comparing output...");
        exit_code = run_compare(root_node);
    }

    /* Cleanup */
    verbose("cleaning up...");
    symtab_pop();
    symtab_finalize(symbol_table);
    astnode_finalize(root_node);

    if (default_outfile)
        free(default_outfile);

    if (xasm_args.include_path_count > 0) {
        int i;
        for (i = 0; i < xasm_args.include_path_count; ++i)
            free(xasm_args.include_paths[i]);
        free(xasm_args.include_paths);
    }

    free(xasm_path);

    if (total_errors() != 0) {
        return 1;
    }
    return exit_code;
}
