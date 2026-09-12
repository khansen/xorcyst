/* Test-only counters around the real parser, source cache, and entry point.
   Build each section separately; the production assembler needs no hooks. */
#if defined(TEST_PARSER_WORK)
#include <stddef.h>
#include "../astnode.h"

unsigned long test_parser_prefix_nodes;

static void count_parser_prefix(const astnode *first)
{
    const astnode *node;
    for (node = first; node != NULL; node = node->next_sibling)
        test_parser_prefix_nodes++;
}

static void counted_parser_append(astnode *before, astnode *added)
{
    /* Both tail lookup and the existing append API walk their input prefix.
       Bound the work requested by the parser without a wall-clock limit. */
    if (added != NULL) count_parser_prefix(before);
    astnode_add_sibling(before, added);
}

static astnode *counted_parser_tail(const astnode *first)
{
    count_parser_prefix(first);
    return astnode_get_last_sibling(first);
}

#define astnode_add_sibling counted_parser_append
#define astnode_get_last_sibling counted_parser_tail
#include "../parser.c"

#elif defined(TEST_SOURCE_CACHE_WORK)
#include <stdio.h>

unsigned long test_source_rewinds;

static void counted_source_rewind(FILE *fp)
{
    test_source_rewinds++;
    rewind(fp);
}

#define rewind counted_source_rewind
#include "../listing.c"
#undef rewind

int test_read_source_lines(const char *first, const char *second)
{
    puts(get_source_line(first, 3));
    puts(get_source_line(first, 3));
    puts(get_source_line(first, 5));
    puts(get_source_line(first, 1));
    puts(get_source_line(second, 3));
    puts(get_source_line(first, 1));
    close_source_cache();
    return 0;
}

#else
#define main assembler_main
#include "../xasm.c"
#undef main

extern unsigned long test_parser_prefix_nodes;
extern unsigned long test_source_rewinds;
int test_read_source_lines(const char *first, const char *second);

int main(int argc, char **argv)
{
    int result;
    if (argc == 4 && strcmp(argv[1], "--test-read-source-lines") == 0)
        result = test_read_source_lines(argv[2], argv[3]);
    else
        result = assembler_main(argc, argv);
    fprintf(stderr, "FRONTEND_WORK prefix_nodes=%lu source_rewinds=%lu\n",
            test_parser_prefix_nodes, test_source_rewinds);
    return result;
}
#endif
