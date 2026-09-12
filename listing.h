#ifndef LISTING_H
#define LISTING_H

#include <stdio.h>
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
int prepare_xref_data_directive_provenance(astnode *root);
int finish_xref_data_directive_provenance(astnode *root);
void clear_xref_data_directive_provenance(void);
int prepare_xref_instruction_provenance(void);
const char *capture_xref_instruction_source(const char *filename, const char *directory, FILE *fp);
int finish_xref_instruction_provenance(astnode *root);
void clear_xref_instruction_provenance(void);
/* Collection owns resolved facts; it does not open output files. */
typedef struct tag_analysis_result analysis_result;
typedef struct tag_analysis_output_plan analysis_output_plan;

typedef struct {
    int pure_binary;
    int include_data;
    int include_instructions;
    int include_owner;
    int include_locals;
    int include_anon;
    int collect_rom_labels;
    int collect_ram_names;
    int mirror_16k;
} analysis_options;

/* Strings are borrowed for the lifetime of the output plan. */
typedef struct {
    const char *xref_file;
    xref_format format;
    int xref_instructions;
    const char *instruction_records_file;
    const char *rom_prefix;
    const char *ram_file;
    const char *source_file;
    const char *output_file;
} analysis_output_options;

analysis_result *collect_analysis(astnode *root, const analysis_options *options);
void free_analysis(analysis_result *analysis);
/* Expand exact filenames without opening or registering any destination. */
analysis_output_plan *plan_analysis_outputs(const analysis_result *analysis,
                                            const analysis_output_options *options);
/* Called once as part of invocation-wide validation, before any output opens. */
int validate_analysis_outputs(const analysis_output_plan *plan);
int write_analysis_outputs(analysis_result *analysis, const analysis_output_plan *plan);
void free_analysis_outputs(analysis_output_plan *plan);
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
