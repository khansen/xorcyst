/* Inject incomplete analysis after a successful collection, before publication. */
#define collect_analysis collect_complete_analysis
#include "../listing.c"
#undef collect_analysis

analysis_result *collect_analysis(astnode *root, const analysis_options *options)
{
    analysis_result *analysis = collect_complete_analysis(root, options);
    if (analysis != NULL && getenv("XASM_TEST_UNRESOLVED_DATA") != NULL
        && analysis->facts.data_directive_ref_count > 0) {
        /* Model one unevaluable data operand without failing the binary writer. */
        analysis->facts.data_directive_refs[0].emitted_value = 0;
        analysis->facts.unresolved_values = 1;
    }
    return analysis;
}
