/*
 * $Id: symtab.c,v 1.7 2007/11/11 22:35:22 khansen Exp $
 * $Log: symtab.c,v $
 * Revision 1.7  2007/11/11 22:35:22  khansen
 * compile on mac
 *
 * Revision 1.6  2007/08/09 20:33:57  khansen
 * progress
 *
 * Revision 1.5  2007/08/08 22:40:43  khansen
 * recursive symbol table lookup
 *
 * Revision 1.4  2007/07/22 13:33:26  khansen
 * convert tabs to whitespaces
 *
 * Revision 1.3  2004/12/14 01:50:05  kenth
 * xorcyst 1.3.0
 *
 * Revision 1.2  2004/12/06 04:52:53  kenth
 * xorcyst 1.1.0
 *
 * Revision 1.1  2004/06/30 07:55:57  kenth
 * Initial revision
 *
 */

/**
 *    (C) 2004 Kent Hansen
 *
 *    The XORcyst is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    The XORcyst is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with The XORcyst; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * This file contains functions for symbol table management.
 */

#include "symtab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SAFE_FREE(a) if (a) { free(a); a = NULL; }

/* Stack of symbol tables */
static symtab *symtab_stack[32] = { NULL };
static int stack_index = 0;

static void symtab_entry_finalize(symtab_entry *);

/*---------------------------------------------------------------------------*/

/**
 * Gets the entry in the tree with minimum key.
 */
static symtab_entry *binary_min(symtab_entry *p)
{
    if (p == NULL) { return NULL; }
    while (p->left != NULL) {
        p = p->left;
    }
    return p;
}

#if 0
/**
 * Gets the entry in the tree with maximum key.
 */
static symtab_entry *binary_max(symtab_entry *p)
{
    if (p == NULL) { return NULL; }
    while (p->right != NULL) {
        p = p->right;
    }
    return p;
}
#endif

/**
 * Gets the successor of an entry.
 */
static symtab_entry *binary_succ(symtab_entry *p)
{
    symtab_entry *e;
    if (p == NULL) { return NULL; }
    if (p->right != NULL) { return binary_min(p->right); }
    e = p->parent;
    while ((e != NULL) && (p == e->right)) {
        p = e;
        e = e->parent;
    }
    return e;
}

#if 0
/**
 * Gets the predecessor of an entry.
 */
static symtab_entry *binary_pred(symtab_entry *p)
{
    symtab_entry *e;
    if (p == NULL) { return NULL; }
    if (p->left != NULL) { return binary_max(p->left); }
    e = p->parent;
    while ((e != NULL) && (p == e->left)) {
        p = e;
        e = e->parent;
    }
    return e;
}
#endif

/* AVL balancing bounds lookup work independently of declaration order.
   Preserve the existing ordering: larger identifiers go to the left. */
static int tree_height(const symtab_entry *node)
{
    return node != NULL ? node->height : 0;
}

static void update_height(symtab_entry *node)
{
    int left = tree_height(node->left), right = tree_height(node->right);
    node->height = 1 + (left > right ? left : right);
}

static void replace_tree_child(symtab *st, symtab_entry *old, symtab_entry *replacement)
{
    if (old->parent == NULL) {
        st->root = replacement;
    } else if (old->parent->left == old) {
        old->parent->left = replacement;
    } else {
        old->parent->right = replacement;
    }
    if (replacement != NULL) {
        replacement->parent = old->parent;
    }
}

/* Promote a child over its parent without moving either entry's payload. */
static void rotate_up(symtab *st, symtab_entry *node)
{
    symtab_entry *parent = node->parent;
    replace_tree_child(st, parent, node);
    if (parent->left == node) {
        parent->left = node->right;
        if (parent->left != NULL) {
            parent->left->parent = parent;
        }
        node->right = parent;
    } else {
        parent->right = node->left;
        if (parent->right != NULL) {
            parent->right->parent = parent;
        }
        node->left = parent;
    }
    parent->parent = node;
    update_height(parent);
    update_height(node);
}

static void rebalance(symtab *st, symtab_entry *node)
{
    while (node != NULL) {
        update_height(node);
        if (tree_height(node->left) - tree_height(node->right) > 1) {
            symtab_entry *left = node->left;
            if (tree_height(left->right) > tree_height(left->left)) {
                rotate_up(st, left->right);
            }
            left = node->left;
            rotate_up(st, left);
            node = left;
        } else if (tree_height(node->right) - tree_height(node->left) > 1) {
            symtab_entry *right = node->right;
            if (tree_height(right->left) > tree_height(right->right)) {
                rotate_up(st, right->left);
            }
            right = node->right;
            rotate_up(st, right);
            node = right;
        }
        node = node->parent;
    }
}

/**
 * Inserts a new entry; the caller then balances the path to the root.
 */
static void binary_insert(symtab_entry *p, symtab_entry *e)
{
    int r = strcmp(p->id, e->id);
    if (r < 0) {
        /* Follow left branch */
        if (p->left == NULL) {
            p->left = e;    /* Entry inserted successfully */
            e->parent = p;
        }
        else {
            binary_insert(p->left, e);   /* Call recursively */
        }
    }
    else if (r > 0) {
        /* Follow right branch */
        if (p->right == NULL) {
            p->right = e;   /* Entry inserted successfully */
            e->parent = p;
        }
        else {
            binary_insert(p->right, e);  /* Call recursively */
        }
    }
    else {
        /* Symbol already present */
    }
}

/**
 * Deletes an entry from a binary tree.
 * Surviving entries keep their identity and all type-specific attributes.
 */
static void binary_delete(symtab *st, symtab_entry *node)
{
    symtab_entry *from;
    if (st == NULL || node == NULL) { return; }
    if (node->left != NULL && node->right != NULL) {
        /* Move the successor node, rather than copying its owned payload.
           Balance from its former parent up through its new position. */
        symtab_entry *successor = binary_min(node->right);
        from = successor->parent;
        if (from != node) {
            replace_tree_child(st, successor, successor->right);
            successor->right = node->right;
            successor->right->parent = successor;
        } else {
            from = successor;
        }
        replace_tree_child(st, node, successor);
        successor->left = node->left;
        successor->left->parent = successor;
        update_height(successor);
    } else {
        from = node->parent;
        replace_tree_child(st, node, node->left != NULL ? node->left : node->right);
    }
    node->left = node->right = node->parent = NULL;
    symtab_entry_finalize(node);
    rebalance(st, from);
}

/**
 * Searches for an entry in a binary tree.
 * @param p Root node
 * @param id Identifier (search key)
 */
static symtab_entry *binary_search(symtab_entry *p, const char *id)
{
    int r;
    while ((p != NULL) && ((r = strcmp(p->id, id)) != 0)) {
        p = (r < 0) ? p->left : p->right;
    }
    return p;
}

/*---------------------------------------------------------------------------*/

static symtab_entry *lookup(symtab *st, const char *id, int recurse)
{
    symtab_entry *e;
    do {
        e = binary_search(st->root, id);
        if (e != NULL) { return e; }
        st = st->parent;
    } while (recurse && st);
    return NULL;
}

/**
 * Looks up the specified id in the current symbol table.
 */
symtab_entry *symtab_lookup(const char *id)
{
    return lookup(symtab_tos(), id, 0);
}

/**
 * Looks up the specified id in the current symbol table.
 */
symtab_entry *symtab_lookup_recursive(const char *id)
{
    return lookup(symtab_tos(), id, 1);
}

/**
 * Looks up the specified id in the global symbol table.
 */
symtab_entry *symtab_global_lookup(const char *id)
{
    return lookup(symtab_stack[0], id, 0);
}

/**
 * Enters something into a symbol table.
 * @param id Identifier
 * @param type Type of symbol (*_SYMBOL)
 * @param def External definition (may be <code>NULL</code>)
 * @param flags Initial flags
 */
symtab_entry *symtab_enter(const char *id, symbol_type type, astnode *def, int flags)
{
    symtab *st = symtab_tos();
    /* See if this id already exists */
    symtab_entry *e = symtab_lookup(id);
    if (e != NULL) {
        /* Duplicate symbol. */
//      printf("error: symtab_enter(): `%s' already exists\n", id);
        return NULL;
    }
//  printf("symtab_enter(): '%s' @ %.8X\n", id, st);
    /* Allocate new entry */
    e = (symtab_entry *)malloc(sizeof(symtab_entry));
    if (e != NULL) {
        /* Set its fields */
        e->id = (char *)malloc(strlen(id)+1);
        if (e->id != NULL) {
            strcpy(e->id, id);
        }
        e->type = type;
        e->flags = flags;
        e->ref_count = 0;
        e->def = def;
        /* Zap! */
        e->struc.size = NULL;
        e->struc.fields = NULL;
        e->field.offset = NULL;
        e->field.size = NULL;
        e->left = NULL;
        e->right = NULL;
        e->parent = NULL;
        e->height = 1;
        e->symtab = NULL;
        /* Put it into symbol table */
        if (st->root == NULL) {
            st->root = e;   /* This is the root element */
        }
        else {
        /* Insert entry in binary tree */
            binary_insert(st->root, e);
            rebalance(st, e->parent);
        }
    }
    /* Return the newly created symbol table entry */
    return e;
}

/*---------------------------------------------------------------------------*/

/**
 * Finalizes a single symbol table entry.
 */
static void symtab_entry_finalize(symtab_entry *e)
{
    ordered_field_list *list;
    ordered_field_list *next;
//  printf("symtab_finalize(): `%s'\n", e->id);
    /* Special finalizing */
    switch (e->type) {
        case LABEL_SYMBOL:
        break;

        case CONSTANT_SYMBOL:
        astnode_finalize(e->def);
        break;

        case MACRO_SYMBOL:
        astnode_finalize(e->def);
        break;

        case STRUC_SYMBOL:
        case UNION_SYMBOL:
        case RECORD_SYMBOL:
        /* Free list of in-order field identifiers */
        for (list = e->struc.fields; list != NULL; list = next) {
            next = list->next;
            free(list);
        }
        symtab_finalize(e->symtab);
        astnode_finalize(e->struc.size);
        break;

        case ENUM_SYMBOL:
        symtab_finalize(e->symtab);
        break;

        case VAR_SYMBOL:
        astnode_finalize(e->def);
        astnode_finalize(e->field.offset);
        astnode_finalize(e->field.size);
        break;

        case PROC_SYMBOL:
        break;

        default:
        /* Nada */
        break;
    }
    /* Common finalizing */
    SAFE_FREE(e->id);
    SAFE_FREE(e);
}

/**
 * Finalizes a symbol table entry recursively,
 * by first finalizing its children before finalizing itself.
 */
static void symtab_entry_finalize_recursive(symtab_entry *e)
{
    if (e == NULL) return;
    symtab_entry_finalize_recursive(e->left);
    symtab_entry_finalize_recursive(e->right);
    symtab_entry_finalize(e);
}

/**
 * Finalizes a symbol table.
 * @param st The symbol table to finalize
 */
void symtab_finalize(symtab *st)
{
    if (st == NULL) return;
    symtab_entry_finalize_recursive(st->root);
    SAFE_FREE(st);
}

/*---------------------------------------------------------------------------*/

/**
 * Gets top of symbol table stack.
 */
symtab *symtab_tos()
{
    if (stack_index == 0) { return NULL; }
    return symtab_stack[stack_index-1];
}

/**
 * Creates a symbol table and pushes it on the symbol table stack.
 */
symtab *symtab_create()
{
    symtab *st = (symtab *)malloc(sizeof(symtab));
    if (st != NULL) {
        st->root = NULL;
        st->parent = symtab_tos();
        symtab_push(st);
    }
    return st;
}

/**
 * Pushes a symbol table onto the stack.
 */
void symtab_push(symtab *st)
{
    symtab_stack[stack_index++] = st;
}

/**
 * Pops a symbol table from the stack.
 */
symtab *symtab_pop()
{
    return symtab_stack[--stack_index];
}

/**
 * Removes an entry from the symbol table.
 * The memory associated with the entry is freed.
 * @param id Identifier of the entry to remove
 */
void symtab_remove(const char *id)
{
    symtab *st = symtab_tos();
    symtab_entry * e = symtab_lookup(id);
    if (e == NULL) { return; }  /* Nothing to remove */
    binary_delete(st, e);
}

/*---------------------------------------------------------------------------*/

/**
 * Gets the number of entries of the given type.
 */
int symtab_type_count(symbol_type type)
{
    symtab *st = symtab_tos();
    int count = 0;
    symtab_entry *e = binary_min(st->root);
    while (e != NULL) {
        if ((type == ANY_SYMBOL) || (e->type == type)) {
            count++;
        }
        e = binary_succ(e);
    }
    return count;
}

/**
 * Removes all entries of a given type from the symbol table.
 * The memory associated with the entries is freed.
 * @param type Type of symbols to remove
 */
void symtab_remove_by_type(symbol_type type)
{
    int i;
    /* Declare array to hold identifiers */
    symbol_ident_list list;
    /* List the entry identifiers */
    int count = symtab_list_type(type, &list);
    /* Remove the entries one by one */
    for (i=0; i<count; i++) {
        symtab_remove(list.idents[i]);
    }
    /* Finalize the array of identifiers */
    symtab_list_finalize(&list);
}

/*---------------------------------------------------------------------------*/
/*
static void indent(int n)
{
    int i;
    for (i=0; i<n; i++) {
        printf(" ");
    }
}
*/
/**
 *
 */
static void symtab_entry_print_recursive(symtab_entry *e, int level)
{
    if (e == NULL) { return; }
    //indent(level*2);
    symtab_entry_print_recursive(e->left, level+1);
    printf("%s\n", e->id);
    symtab_entry_print_recursive(e->right, level+1);
}

/**
 *
 */
void symtab_print()
{
    symtab *st = symtab_tos();
    symtab_entry_print_recursive(st->root, 0);
}

/*---------------------------------------------------------------------------*/

/**
 * Gets the number of entries in the symbol table.
 */
int symtab_size()
{
    return symtab_type_count(ANY_SYMBOL);
}

/**
 * Lists the entries in a symbol table that are of a certain type.
 * @param type The symbol type (*_SYMBOL)
 * @param list List which will receive the array of identifiers
 * @return Number of identifiers, or -1 on failure with an empty list.
 */
int symtab_list_type(symbol_type type, symbol_ident_list *list)
{
    symtab *st = symtab_tos();
    int count = symtab_type_count(type);
    symtab_entry *e;
    list->size = 0;
    list->idents = NULL;
    if (count == 0) return 0;
    list->idents = (char **)malloc((size_t)count * sizeof(char *));
    if (list->idents == NULL) return -1;
    for (e = binary_min(st->root); e != NULL; e = binary_succ(e)) {
        if ((type == ANY_SYMBOL) || (e->type == type)) {
            char *name = (char *)malloc(strlen(e->id) + 1);
            if (name == NULL) {
                symtab_list_finalize(list);
                return -1;
            }
            strcpy(name, e->id);
            list->idents[list->size++] = name;
        }
    }
    return list->size;
}

/**
 * Lists all identifiers in the symbol table.
 */
int symtab_list(symbol_ident_list *list)
{
    return symtab_list_type(ANY_SYMBOL, list);
}

/**
 * Lists all identifiers in the symbol table which has ONE OR MORE of the
 * given flags set.
 * If flag is zero it is ignored.
 */
int symtab_list_flag(int flag, symbol_ident_list *list)
{
    // TODO
    return 0;
}

/**
 * Lists all identifiers in the symbol table of a given type which has
 * ONE OR MORE of the given flags set.
 * If flag is zero it is ignored.
 */
int symtab_list_type_flag(symbol_type type, int flag, symbol_ident_list *list)
{
    // TODO
    return 0;
}

/**
 * Finalizes a list of identifiers.
 */
void symtab_list_finalize(symbol_ident_list *list)
{
    int i;
    for (i=0; i<list->size; i++) {
        SAFE_FREE(list->idents[i]);
    }
    SAFE_FREE(list->idents);
    list->size = 0;
}
