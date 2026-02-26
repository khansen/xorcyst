/* $Id: xasm.h,v 1.7 2007/08/19 11:19:17 khansen Exp $
 * $Log: xasm.h,v $
 * Revision 1.7  2007/08/19 11:19:17  khansen
 * --case-insensitive option
 *
 * Revision 1.6  2007/08/12 18:59:10  khansen
 * ability to generate pure 6502 binary
 *
 * Revision 1.5  2007/08/11 01:25:18  khansen
 * includepaths support (-I option)
 *
 * Revision 1.4  2007/08/10 20:21:26  khansen
 * *** empty log message ***
 *
 * Revision 1.3  2007/07/22 13:35:20  khansen
 * convert tabs to whitespaces
 *
 * Revision 1.2  2004/12/18 11:32:11  kenth
 * added debug flag
 *
 * Revision 1.1  2004/12/18 11:31:48  kenth
 * Initial revision
 *
 */
#ifndef XASM_H
#define XASM_H

enum tag_listing_format {
    LISTING_FORMAT_TEXT = 0,
    LISTING_FORMAT_JSON,
    LISTING_FORMAT_NDJSON
};

typedef enum tag_listing_format listing_format;

enum tag_compare_format {
    COMPARE_FORMAT_TEXT = 0,
    COMPARE_FORMAT_JSON
};

typedef enum tag_compare_format compare_format;

enum tag_xref_format {
    XREF_FORMAT_JSON = 0,
    XREF_FORMAT_TEXT,
    XREF_FORMAT_CSV
};

typedef enum tag_xref_format xref_format;

struct tag_xasm_arguments {
    const char *input_file;
    int debug;
    int silent;
    int verbose;
    int swap_parens;
    int no_warn;
    int pure_binary;
    int case_insensitive;
    const char *output_file;
    const char *listing_file;
    int warn_unused_equ;
    int werror_unused_equ;
    int listing_format;
    int listing_format_set;
    const char *compare_file;
    long compare_offset;
    long compare_length;
    int compare_max_mismatches;
    int compare_format;
    int compare_cpu_base_set;
    long compare_cpu_base;
    const char *xref_file;
    int xref_format;
    int xref_include_locals;
    int xref_include_anon;
    char **include_paths;
    int include_path_count;
};

typedef struct tag_xasm_arguments xasm_arguments;

extern xasm_arguments xasm_args;

extern char *xasm_path;

#endif  /* !XASM_H */
