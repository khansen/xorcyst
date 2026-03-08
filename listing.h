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
                  int include_data,
                  int include_owner,
                  int include_locals,
                  int include_anon,
                  const char *source_file,
                  const char *output_file,
                  int pure_binary);
int generate_xref_summary(astnode *root,
                          const char *output_path,
                          xref_summary_format format,
                          int include_locals,
                          int include_anon,
                          const char *kind_filter,
                          int limit,
                          int top_referrers,
                          int nearby_window,
                          const char *include_regex,
                          const char *exclude_regex,
                          const char *source_file,
                          const char *output_file,
                          int pure_binary);
int generate_index_patterns(astnode *root,
                            const char *output_path,
                            index_patterns_format format,
                            int include_locals,
                            int include_anon,
                            const char *split_pairs,
                            int pure_binary);
int generate_data_consumers(astnode *root,
                            const char *output_path,
                            data_consumers_format format,
                            int include_overlaps,
                            const char *split_pairs,
                            int pure_binary);
int generate_data_coverage(astnode *root,
                           const char *output_path,
                           data_coverage_format format,
                           int include_overlaps,
                           const char *split_pairs,
                           int pure_binary);
int run_raw_address_audit(astnode *root,
                          int level_error,
                          int output_json,
                          int rom_range_set,
                          long rom_lo,
                          long rom_hi);

#endif
