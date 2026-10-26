/* Source frontend for the shared backend. GPL-2.0-or-later. */
#ifndef XORCYST_IR_FRONTEND_H
#define XORCYST_IR_FRONTEND_H
#include "../astnode.h"
#include "../symtab.h"
#include "../astproc.h"
#include <stdio.h>
void ir_frontend_process(astnode *root);
void ir_frontend_finish(void);
void ir_frontend_write(FILE *stream);
void ir_frontend_clear(void);
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
} ir_analysis_row;
int ir_frontend_visit_rows(int (*visit)(const ir_analysis_row *, void *), void *);
/* Diagnostic-only traversal: no successful layout or emitted-byte claim.
 * Uses the remaining parser tree on early errors, owned metadata after lowering. */
int ir_frontend_analysis_ok(void);
unsigned char ir_frontend_diagnostic_opcode(const astnode *);
int ir_frontend_visit_diagnostic_rows(int (*)(const ir_analysis_row *, void *), void *);
void ir_frontend_walk_diagnostic(astnode *, void *, const astnodeprocmap *);
/* Shared-value bindings on owned analysis expressions. */
location ir_frontend_own_location(location);
void ir_frontend_own_analysis_locations(astnode *);
int ir_frontend_bind_analysis(astnode *, unsigned *anchor);
int ir_frontend_eval_analysis(astnode *, int pc, int *value);
int ir_frontend_analysis_elements(astnode *, int pc, size_t *);
symtab_entry *ir_frontend_analysis_symbol(const char *);
int ir_frontend_analysis_symbols(symbol_type, symbol_ident_list *);
#endif
