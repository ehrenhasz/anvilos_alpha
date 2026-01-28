
#ifndef MICROPY_INCLUDED_PY_PARSE_H
#define MICROPY_INCLUDED_PY_PARSE_H

#include <stddef.h>
#include <stdint.h>

#include "py/obj.h"

struct _mp_lexer_t;









#define MP_PARSE_NODE_NULL      (0)
#define MP_PARSE_NODE_SMALL_INT (0x1)
#define MP_PARSE_NODE_ID        (0x02)
#define MP_PARSE_NODE_STRING    (0x06)
#define MP_PARSE_NODE_TOKEN     (0x0a)

typedef uintptr_t mp_parse_node_t; 

typedef struct _mp_parse_node_struct_t {
    uint32_t source_line;       
    uint32_t kind_num_nodes;    
    mp_parse_node_t nodes[];    
} mp_parse_node_struct_t;




#define MP_PARSE_NODE_IS_NULL(pn) ((pn) == MP_PARSE_NODE_NULL)
#define MP_PARSE_NODE_IS_LEAF(pn) ((pn) & 3)
#define MP_PARSE_NODE_IS_STRUCT(pn) ((pn) != MP_PARSE_NODE_NULL && ((pn) & 3) == 0)
#define MP_PARSE_NODE_IS_STRUCT_KIND(pn, k) ((pn) != MP_PARSE_NODE_NULL && ((pn) & 3) == 0 && MP_PARSE_NODE_STRUCT_KIND((mp_parse_node_struct_t *)(pn)) == (k))

#define MP_PARSE_NODE_IS_SMALL_INT(pn) (((pn) & 0x1) == MP_PARSE_NODE_SMALL_INT)
#define MP_PARSE_NODE_IS_ID(pn) (((pn) & 0x0f) == MP_PARSE_NODE_ID)
#define MP_PARSE_NODE_IS_TOKEN(pn) (((pn) & 0x0f) == MP_PARSE_NODE_TOKEN)
#define MP_PARSE_NODE_IS_TOKEN_KIND(pn, k) ((pn) == (MP_PARSE_NODE_TOKEN | ((k) << 4)))

#define MP_PARSE_NODE_LEAF_KIND(pn) ((pn) & 0x0f)
#define MP_PARSE_NODE_LEAF_ARG(pn) (((uintptr_t)(pn)) >> 4)
#define MP_PARSE_NODE_LEAF_SMALL_INT(pn) (((mp_int_t)(intptr_t)(pn)) >> 1)
#define MP_PARSE_NODE_STRUCT_KIND(pns) ((pns)->kind_num_nodes & 0xff)
#define MP_PARSE_NODE_STRUCT_NUM_NODES(pns) ((pns)->kind_num_nodes >> 8)

static inline mp_parse_node_t mp_parse_node_new_small_int(mp_int_t val) {
    return (mp_parse_node_t)(MP_PARSE_NODE_SMALL_INT | ((mp_uint_t)val << 1));
}

static inline mp_parse_node_t mp_parse_node_new_leaf(size_t kind, mp_int_t arg) {
    return (mp_parse_node_t)(kind | ((mp_uint_t)arg << 4));
}

static inline mp_obj_t mp_parse_node_extract_const_object(mp_parse_node_struct_t *pns) {
    #if MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_D
    
    return (uint64_t)pns->nodes[0] | ((uint64_t)pns->nodes[1] << 32);
    #else
    return (mp_obj_t)pns->nodes[0];
    #endif
}

bool mp_parse_node_is_const_false(mp_parse_node_t pn);
bool mp_parse_node_is_const_true(mp_parse_node_t pn);
bool mp_parse_node_get_int_maybe(mp_parse_node_t pn, mp_obj_t *o);
size_t mp_parse_node_extract_list(mp_parse_node_t *pn, size_t pn_kind, mp_parse_node_t **nodes);
void mp_parse_node_print(const mp_print_t *print, mp_parse_node_t pn, size_t indent);

typedef enum {
    MP_PARSE_SINGLE_INPUT,
    MP_PARSE_FILE_INPUT,
    MP_PARSE_EVAL_INPUT,
} mp_parse_input_kind_t;

typedef struct _mp_parse_t {
    mp_parse_node_t root;
    struct _mp_parse_chunk_t *chunk;
} mp_parse_tree_t;



mp_parse_tree_t mp_parse(struct _mp_lexer_t *lex, mp_parse_input_kind_t input_kind);
void mp_parse_tree_clear(mp_parse_tree_t *tree);

#endif 
