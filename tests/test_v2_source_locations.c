/* Optional parsed spans must survive expansion and node ownership changes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../astnode.h"

static size_t requested_size, allocations;
static int fail_next;
static void *node_calloc(size_t count, size_t size)
{
    requested_size = count * size;
    ++allocations;
    if (fail_next) { fail_next = 0; return NULL; }
    return calloc(count, size);
}
#define calloc node_calloc
#include "../astnode.c"
#undef calloc

const char *yy_current_filename(void) { return "fallback.asm"; }

static location source(void)
{
    location loc = {4, 3, 4, 9, "shown.asm", "/input/macros.asm"};
    return loc;
}
static void check_source(const astnode *n)
{
    location loc = astnode_source_location(n);
    assert(!strcmp(loc.file, "/input/macros.asm"));
    assert(loc.first_line == 4 && loc.first_column == 3);
    assert(loc.last_line == 4 && loc.last_column == 9);
}
static void plain(void)
{
    astnode *n;
    astnode_capture_source_locations(0);
    n = astnode_create_integer(7, source());
    assert(n && requested_size == sizeof(astnode) && allocations == 1);
    check_source(n);
    n->loc.first_line = 12;
    assert(astnode_source_location(n).first_line == 12);
    astnode_capture_source_locations(1);
    /* Changing the allocation policy cannot make an older node acquire a tail. */
    assert(astnode_source_location(n).first_line == 12);
    astnode_finalize(n);
}
static void expanded(void)
{
    astnode *original, *first, *nested;
    location use = {20, 1, 20, 7, "main.asm", "/input/main.asm"};
    astnode_capture_source_locations(1);
    original = astnode_create_integer(7, source());
    assert(original && requested_size == sizeof(astnode) + sizeof(location));
    original->flags |= 0x80 | 0x10000 | 0x40000;
    first = astnode_clone(original, use);
    assert(first && first->loc.first_line == 20);
    assert(!(first->flags & (0x80 | 0x40000)));
    assert(first->flags & 0x10000);
    check_source(first);
    use.first_line = use.last_line = 60;
    nested = astnode_clone(first, use);
    astnode_finalize(original);
    astnode_finalize(first);
    assert(nested && nested->loc.first_line == 60 && nested->integer == 7);
    check_source(nested);
    astnode_capture_source_locations(0);
    check_source(nested); /* Ownership is per node, not the current policy. */
    astnode_finalize(nested);
}
static void identities(void)
{
    astnode *parent, *old, *replacement, *copy;
    astnode_capture_source_locations(1);
    old = astnode_create_string("old", source());
    parent = astnode_create_list(old);
    old->analysis_origin_id = 73;
    old->frontend_id = 19;
    copy = astnode_clone(old, loc_preserve);
    assert(copy->analysis_origin_id == 73 && copy->frontend_id == 19);
    copy->frontend_id = 101;
    assert(copy->analysis_origin_id == 73 && old->frontend_id == 19);
    astnode_finalize(copy);
    replacement = astnode_create_string("new", source());
    replacement->frontend_id = 41;
    astnode_replace(old, replacement);
    astnode_finalize(old);
    assert(parent->first_child == replacement);
    assert(replacement->analysis_origin_id == 73 && replacement->frontend_id == 41);
    check_source(replacement);
    astnode_finalize(parent);
}
static void allocation(void)
{
    int mode;
    for (mode = 0; mode < 2; ++mode) {
        astnode *n;
        astnode_capture_source_locations(mode);
        fail_next = 1;
        assert(!astnode_create(INTEGER_NODE, source()));
        n = astnode_create_integer(7, source());
        assert(n);
        astnode_finalize(n);
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "plain")) plain();
    else if (!strcmp(argv[1], "expanded")) expanded();
    else if (!strcmp(argv[1], "identities")) identities();
    else if (!strcmp(argv[1], "allocation")) allocation();
    else abort();
    return 0;
}
