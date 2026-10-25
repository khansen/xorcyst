/* Isolated source frontend for the approved I3 prototype. GPL-2.0-or-later. */
#ifndef XORCYST_V2_FRONTEND_H
#define XORCYST_V2_FRONTEND_H
#include "../astnode.h"
#include "../symtab.h"
#include "../astproc.h"
#include <stdio.h>
void v2_frontend_process(astnode *root);
void v2_frontend_finish(void);
void v2_frontend_write(FILE *stream);
void v2_frontend_clear(void);
/* Read-only source rows over a successful layout. All payloads are borrowed
 * until frontend_clear; no parser AST or namespace is needed by this view. */
typedef struct {
    astnode_type kind;
    datatype data_type;
    location loc;
    unsigned address, output_offset, length;
    const unsigned char *bytes;
    const char *label;
    unsigned char opcode;
    int ram, scope_anchor;
    astnode *statement; /* owned analysis metadata, detached from the source tree */
} v2_analysis_row;
int v2_frontend_visit_rows(int (*visit)(const v2_analysis_row *, void *), void *);
/* Diagnostic-only traversal: no successful layout or emitted-byte claim.
 * Uses the remaining parser tree on early errors, owned metadata after lowering. */
int v2_frontend_analysis_ok(void);
unsigned char v2_frontend_diagnostic_opcode(const astnode *);
int v2_frontend_visit_diagnostic_rows(int (*)(const v2_analysis_row *, void *), void *);
void v2_frontend_walk_diagnostic(astnode *, void *, const astnodeprocmap *);
/* Shared-value bindings on owned analysis expressions. */
location v2_frontend_own_location(location);
void v2_frontend_own_analysis_locations(astnode *);
int v2_frontend_bind_analysis(astnode *, unsigned *anchor);
int v2_frontend_eval_analysis(astnode *, int pc, int *value);
int v2_frontend_analysis_elements(astnode *, int pc, size_t *);
symtab_entry *v2_frontend_analysis_symbol(const char *);
int v2_frontend_analysis_symbols(symbol_type, symbol_ident_list *);
#endif
