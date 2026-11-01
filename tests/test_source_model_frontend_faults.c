/* Test-only observation across the complete native shadow-resolution path. */
#define source_model_prepared observed_prepared
#define source_model_value observed_value
#include "../ir/frontend.c"
#undef source_model_prepared
#undef source_model_value

extern astnode *source_model_prepared(size_t);
extern void source_model_value(size_t, ir_id, ir_id);
static symtab_entry *observed_symbol;
static int observed_count;

astnode *observed_prepared(size_t i)
{
    astnode *node = source_model_prepared(i);
    observed_symbol = node && node->type == IDENTIFIER_NODE ? symtab_lookup(node->ident) : NULL;
    if (observed_symbol) observed_count = observed_symbol->ref_count;
    return node;
}
void observed_value(size_t i, ir_id value, ir_id pc)
{
    if (observed_symbol)
        fprintf(stderr, "SHADOW_USE_DELTA %d\n", observed_symbol->ref_count - observed_count);
    source_model_value(i, value, pc);
}
