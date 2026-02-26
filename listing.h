#ifndef LISTING_H
#define LISTING_H

#include "astnode.h"
#include "xasm.h"

typedef struct tag_listing_lookup_result {
    const char *file;
    int line;
    int column;
    int cpu_address;
    int cpu_address_known;
} listing_lookup_result;

int generate_listing(astnode *root,
                     const char *filename,
                     listing_format format,
                     const char *source_file,
                     const char *output_file);
int listing_lookup_output_offset(astnode *root, long output_offset, listing_lookup_result *out);
int generate_xref(astnode *root,
                  const char *filename,
                  xref_format format,
                  int include_locals,
                  int include_anon,
                  const char *source_file,
                  const char *output_file,
                  int pure_binary);

#endif
