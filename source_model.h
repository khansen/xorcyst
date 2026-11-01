/* Lossless, opt-in assembler-owned source model. GPL-2.0-or-later. */
#ifndef XASM_SOURCE_MODEL_H
#define XASM_SOURCE_MODEL_H
#include "astnode.h"
#include "symtab.h"
#include "ir/layout.h"
#include "xasm.h"

static inline int source_model_enabled(void) { return xasm_args.source_model != NULL; }
void source_model_match(location, size_t);
void source_model_trivia(const char *);
void source_model_token(int);
void source_model_parse_error(location, const char *);
int source_model_capture(astnode *);
/* Called at the native first-pass lexical context, before destructive folds. */
void source_model_prepare(astnode *, int);
void source_model_bound(const astnode *, const symtab_entry *, int);
/* The frontend owns resolution and evaluation. The exporter only stores IDs. */
size_t source_model_node_count(void);
astnode *source_model_prepared(size_t);
size_t source_model_subtree_end(size_t);
int source_model_expression(size_t);
int source_model_context_pc(size_t);
int source_model_equ_context(size_t);
void source_model_binding(size_t, const location *);
void source_model_value(size_t, ir_id, ir_id);
void source_model_range(unsigned, int, size_t, size_t);
int source_model_finish(ir_unit *, ir_layout *);
int source_model_write(const char *, int, const char *, const char *, int);
void source_model_report(void);
int source_model_failed(void);
int source_model_collection_failed(void);
int source_model_has_source(void);
void source_model_clear(void);
#endif
