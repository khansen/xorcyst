/* Exercise optional capture through the real parser/expander without enabling
 * any output product before its shared-layout collector exists. */
#define main assembler_main
#include "../xasm.c"
#undef main

static void mark_operands(astnode *n)
{
    astnode *child;
    if (!n) return;
    n->analysis_origin_id = 1000000;
    for (child = n->first_child; child; child = child->next_sibling)
        mark_operands(child);
}
static int capture_data(astnode *n)
{
    location parsed = astnode_source_location(n);
    fprintf(stderr, "CAPTURE %d %d\n", n->loc.first_line, parsed.first_line);
    mark_operands(n);
    return 1;
}
int main(int argc, char **argv)
{
    astnode_capture_source_locations(1);
    astproc_set_data_analysis_hook(capture_data);
    return assembler_main(argc, argv);
}
