/* Read-only construction graph views for object serialization. No memo or
 * placement result is exported. Views are borrowed until the unit changes. */
#ifndef XORCYST_V2_GRAPH_H
#define XORCYST_V2_GRAPH_H
#include "extent.h"
#include "heap.h"
v2_unit *v2_unit_create_heap(v2_heap *);
v2_heap *v2_graph_heap(v2_unit *);
typedef enum
{
    V2_GRAPH_INTEGER,
    V2_GRAPH_STRING,
    V2_GRAPH_REFERENCE,
    V2_GRAPH_ADDRESS,
    V2_GRAPH_UNARY,
    V2_GRAPH_BINARY,
    V2_GRAPH_EXTENT_LITERAL,
    V2_GRAPH_COUNT,
    V2_GRAPH_SUM,
    V2_GRAPH_REPEAT,
    V2_GRAPH_REMAINDER,
    V2_GRAPH_BOUND,
    V2_GRAPH_STRING_LENGTH,
    V2_GRAPH_EXTENT_INTEGER
} v2_graph_kind;
typedef struct
{
    v2_graph_kind kind;
    v2_operator op;
    v2_id left, right;
    v2_location location;
    int32_t integer;
    uint64_t extent;
    uint32_t minimum, maximum;
    const unsigned char *bytes;
    size_t length;
} v2_graph_node;
size_t v2_graph_nodes(const v2_unit *);
size_t v2_graph_definitions(const v2_unit *);
size_t v2_graph_anchors(const v2_unit *);
int v2_graph_read(const v2_unit *, v2_id, v2_graph_node *);
int v2_graph_definition(const v2_unit *, v2_id, v2_id *expression, v2_location *);
int v2_graph_anchor(const v2_unit *, v2_id, v2_location *);
/* Internal source-span interning shared by graph and layout storage. */
v2_id v2_graph_location_id(v2_unit *, v2_location);
v2_location v2_graph_location(const v2_unit *, v2_id);
/* Drop the construction-only index. Spans and IDs stay valid; a subsequent
 * constructor rebuilds the index on demand. */
void v2_graph_release_location_index(v2_unit *);
#endif
