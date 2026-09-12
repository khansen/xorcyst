/* Exercise the public symbol-table API and independently check AVL invariants. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../symtab.h"

#define COUNT 2048

static int visited;
static const char *previous;

static int check_tree(const symtab_entry *node, const symtab_entry *parent)
{
    int left, right, height;
    if (node == NULL) return 0;
    assert(++visited <= COUNT);
    assert(node->parent == parent);
    left = check_tree(node->left, node);
    /* Enumeration has always used descending identifier order. */
    if (previous != NULL) assert(strcmp(previous, node->id) > 0);
    previous = node->id;
    right = check_tree(node->right, node);
    height = 1 + (left > right ? left : right);
    /* Compute heights from actual links, independently of cached metadata.
       Balance at every node bounds lookup depth logarithmically. */
    assert(left - right >= -1 && left - right <= 1);
    assert(node->height == height);
    return height;
}

static void check_balance(const symtab *table, int count)
{
    visited = 0;
    previous = NULL;
    check_tree(table->root, NULL);
    assert(visited == count);
}

static symtab_entry *enter_key(int index)
{
    char name[32];
    symtab_entry *entry;
    snprintf(name, sizeof(name), "Key%05d", index);
    entry = symtab_enter(name, LABEL_SYMBOL, NULL, PUBLIC_FLAG | ADDR_FLAG);
    assert(entry != NULL);
    entry->tag = index;
    entry->address = index * 7;
    entry->align = index % 8;
    entry->ref_count = index * 3;
    assert(symtab_enter(name, LABEL_SYMBOL, NULL, 0) == NULL);
    return entry;
}

static void check_entries(symtab *table, symtab_entry **expected, int count)
{
    char name[32];
    symbol_ident_list names;
    symtab_entry *root = table->root;
    int i, actual = 0;
    check_balance(table, count);
    assert(symtab_list(&names) == count);
    for (i = 0; i < COUNT; i++) {
        symtab_entry *entry;
        snprintf(name, sizeof(name), "Key%05d", i);
        entry = symtab_lookup(name);
        assert(entry == expected[i]);
        if (entry != NULL) {
            assert(strcmp(entry->id, name) == 0);
            assert(entry->type == LABEL_SYMBOL && entry->def == NULL);
            assert(entry->flags == (PUBLIC_FLAG | ADDR_FLAG));
            assert(entry->tag == i && entry->address == i * 7);
            assert(entry->align == i % 8 && entry->ref_count == i * 3);
            actual++;
        }
    }
    assert(actual == count);
    for (i = 1; i < names.size; i++) assert(strcmp(names.idents[i - 1], names.idents[i]) > 0);
    symtab_list_finalize(&names);
    assert(symtab_lookup("NoSuchKey") == NULL);
    assert(table->root == root); /* Reads must not restructure the table. */
    check_balance(table, count);
}

static void test_rotations(void)
{
    const int orders[][3] = {{0, 1, 2}, {2, 1, 0}, {0, 2, 1}, {2, 0, 1}};
    int order, i;
    for (order = 0; order < 4; order++) {
        symtab *table = symtab_create();
        symtab_entry *entries[3];
        assert(table != NULL);
        for (i = 0; i < 3; i++) {
            entries[orders[order][i]] = enter_key(orders[order][i]);
            check_balance(table, i + 1);
        }
        assert(table->root == entries[1]);
        /* Two-child root with an immediate successor, then one/zero children. */
        for (i = 1; i >= 0; i--) {
            symtab_remove(table->root->id);
            check_balance(table, i + 1);
        }
        symtab_remove(table->root->id);
        check_balance(table, 0);
        assert(symtab_pop() == table);
        symtab_finalize(table);
    }
}

static void test_ordered_and_permuted_keys(void)
{
    int pass, i;
    for (pass = 0; pass < 3; pass++) {
        symtab *table = symtab_create();
        symtab_entry *entries[COUNT] = {0};
        char name[32];
        assert(table != NULL);
        for (i = 0; i < COUNT; i++) {
            int index = pass == 0 ? i : pass == 1 ? COUNT - 1 - i : (i * 73) % COUNT;
            entries[index] = enter_key(index);
            check_balance(table, i + 1);
        }
        check_entries(table, entries, COUNT);
        for (i = 0; i < COUNT; i++) {
            int index = (i * 181) % COUNT;
            snprintf(name, sizeof(name), "Key%05d", index);
            symtab_remove(name);
            entries[index] = NULL;
            check_balance(table, COUNT - i - 1);
            assert(symtab_lookup(name) == NULL);
            symtab_remove(name); /* Removing a missing key remains harmless. */
            if (i % 256 == 255) check_entries(table, entries, COUNT - i - 1);
        }
        /* Reuse a completely emptied table and exercise bulk removal. */
        for (i = 0; i < COUNT; i++) entries[i] = enter_key(i);
        check_entries(table, entries, COUNT);
        symtab_remove_by_type(LABEL_SYMBOL);
        check_balance(table, 0);
        assert(symtab_pop() == table);
        symtab_finalize(table);
    }
}

static astnode *integer(int value)
{
    location loc = {0};
    astnode *node = astnode_create_integer(value, loc);
    assert(node != NULL);
    return node;
}

static void test_owned_payloads_and_deep_successors(void)
{
    const symbol_type types[] = {CONSTANT_SYMBOL, VAR_SYMBOL, MACRO_SYMBOL,
                                STRUC_SYMBOL, UNION_SYMBOL, RECORD_SYMBOL, ENUM_SYMBOL};
    symtab *table = symtab_create();
    symtab_entry *entries[31], snapshots[31];
    int i, count, deep_successors = 0;
    assert(table != NULL);
    for (i = 0; i < 31; i++) {
        symtab_entry *entry = enter_key(i);
        entries[i] = entry;
        entry->type = types[i % 7];
        if (entry->type == CONSTANT_SYMBOL || entry->type == MACRO_SYMBOL || entry->type == VAR_SYMBOL) {
            entry->def = integer(i);
            if (entry->type == VAR_SYMBOL) {
                entry->field.offset = integer(i * 2);
                entry->field.size = integer(2);
            }
        } else {
            symtab_entry *field;
            entry->symtab = symtab_create();
            assert(entry->symtab != NULL);
            field = symtab_enter("Field", CONSTANT_SYMBOL, integer(i), 0);
            assert(field != NULL);
            assert(symtab_pop() == entry->symtab);
            if (entry->type != ENUM_SYMBOL) {
                entry->struc.size = integer(i + 1);
                entry->struc.fields = malloc(sizeof(ordered_field_list));
                assert(entry->struc.fields != NULL);
                entry->struc.fields->entry = field;
                entry->struc.fields->next = NULL;
            }
        }
        snapshots[i] = *entry;
    }
    /* Repeated root deletion forces successor transplants of different types.
       Check every surviving pointer and owned attribute after each removal. */
    for (count = 31; count > 0; count--) {
        symtab_entry *removed = table->root;
        check_balance(table, count);
        if (removed->left && removed->right && removed->right->left) deep_successors++;
        entries[removed->tag] = NULL;
        symtab_remove(removed->id);
        for (i = 0; i < 31; i++) {
            symtab_entry *entry = entries[i], *saved = &snapshots[i];
            if (entry == NULL) continue;
            assert(symtab_lookup(saved->id) == entry);
            assert(entry->type == saved->type && entry->def == saved->def);
            assert(entry->tag == i && entry->address == i * 7);
            assert(entry->align == i % 8 && entry->ref_count == i * 3);
            assert(entry->flags == saved->flags && entry->symtab == saved->symtab);
            if (entry->symtab != NULL) {
                symtab_push(entry->symtab);
                assert(symtab_lookup("Field")->def->integer == i);
                assert(symtab_pop() == entry->symtab);
                if (entry->type != ENUM_SYMBOL) {
                    assert(entry->struc.size == saved->struc.size);
                    assert(entry->struc.size->integer == i + 1);
                    assert(entry->struc.fields == saved->struc.fields);
                    assert(entry->struc.fields->entry->def->integer == i);
                }
            } else {
                assert(entry->def->integer == i);
                if (entry->type == VAR_SYMBOL) {
                    assert(entry->field.offset == saved->field.offset && entry->field.offset->integer == i * 2);
                    assert(entry->field.size == saved->field.size && entry->field.size->integer == 2);
                }
            }
        }
    }
    assert(deep_successors > 0);
    check_balance(table, 0);
    assert(symtab_pop() == table);
    symtab_finalize(table);
}

static void test_scopes_and_type_enumeration(void)
{
    symtab *global = symtab_create(), *child;
    symtab_entry *outer, *inner;
    symbol_ident_list names;
    int i;
    assert(global != NULL);
    for (i = 0; i < 64; i++) enter_key(i);
    outer = symtab_lookup("Key00001");
    child = symtab_create();
    assert(child != NULL && child->parent == global);
    for (i = 0; i < 64; i += 2) enter_key(i);
    assert(symtab_lookup("Key00001") == NULL);
    assert(symtab_lookup_recursive("Key00001") == outer);
    inner = symtab_enter("Key00001", CONSTANT_SYMBOL, integer(42), EQU_FLAG);
    assert(inner != NULL && symtab_lookup_recursive("Key00001") == inner);
    assert(symtab_global_lookup("Key00001") == outer);
    assert(symtab_list_type(CONSTANT_SYMBOL, &names) == 1);
    assert(strcmp(names.idents[0], "Key00001") == 0);
    symtab_list_finalize(&names);
    symtab_remove_by_type(CONSTANT_SYMBOL);
    assert(symtab_lookup_recursive("Key00001") == outer);
    assert(symtab_type_count(LABEL_SYMBOL) == 32 && symtab_size() == 32);
    check_balance(child, 32);
    assert(symtab_pop() == child);
    symtab_finalize(child);
    assert(symtab_tos() == global && symtab_lookup("Key00001") == outer);
    check_balance(global, 64);
    assert(symtab_pop() == global);
    symtab_finalize(global);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (strcmp(argv[1], "rotations") == 0) test_rotations();
    else if (strcmp(argv[1], "ordered") == 0) test_ordered_and_permuted_keys();
    else if (strcmp(argv[1], "ownership") == 0) test_owned_payloads_and_deep_successors();
    else if (strcmp(argv[1], "scopes") == 0) test_scopes_and_type_enumeration();
    else return 1;
    assert(symtab_tos() == NULL);
    return 0;
}
