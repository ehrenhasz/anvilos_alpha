 

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h> 
#include <assert.h>
#include <string.h>

#include "py/lexer.h"
#include "py/parse.h"
#include "py/parsenum.h"
#include "py/runtime.h"
#include "py/objint.h"
#include "py/objstr.h"
#include "py/builtin.h"

#if MICROPY_ENABLE_COMPILER

#define RULE_ACT_ARG_MASK       (0x0f)
#define RULE_ACT_KIND_MASK      (0x30)
#define RULE_ACT_ALLOW_IDENT    (0x40)
#define RULE_ACT_ADD_BLANK      (0x80)
#define RULE_ACT_OR             (0x10)
#define RULE_ACT_AND            (0x20)
#define RULE_ACT_LIST           (0x30)

#define RULE_ARG_KIND_MASK      (0xf000)
#define RULE_ARG_ARG_MASK       (0x0fff)
#define RULE_ARG_TOK            (0x1000)
#define RULE_ARG_RULE           (0x2000)
#define RULE_ARG_OPT_RULE       (0x3000)



enum {

#define DEF_RULE(rule, comp, kind, ...) RULE_##rule,
#define DEF_RULE_NC(rule, kind, ...)
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
    RULE_const_object, 


#define DEF_RULE(rule, comp, kind, ...)
#define DEF_RULE_NC(rule, kind, ...) RULE_##rule,
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
};


static const uint8_t rule_act_table[] = {
#define or(n)                   (RULE_ACT_OR | n)
#define and(n)                  (RULE_ACT_AND | n)
#define and_ident(n)            (RULE_ACT_AND | n | RULE_ACT_ALLOW_IDENT)
#define and_blank(n)            (RULE_ACT_AND | n | RULE_ACT_ADD_BLANK)
#define one_or_more             (RULE_ACT_LIST | 2)
#define list                    (RULE_ACT_LIST | 1)
#define list_with_end           (RULE_ACT_LIST | 3)

#define DEF_RULE(rule, comp, kind, ...) kind,
#define DEF_RULE_NC(rule, kind, ...)
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC

    0, 

#define DEF_RULE(rule, comp, kind, ...)
#define DEF_RULE_NC(rule, kind, ...) kind,
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC

#undef or
#undef and
#undef and_ident
#undef and_blank
#undef one_or_more
#undef list
#undef list_with_end
};


static const uint16_t rule_arg_combined_table[] = {
#define tok(t)                  (RULE_ARG_TOK | MP_TOKEN_##t)
#define rule(r)                 (RULE_ARG_RULE | RULE_##r)
#define opt_rule(r)             (RULE_ARG_OPT_RULE | RULE_##r)

#define DEF_RULE(rule, comp, kind, ...) __VA_ARGS__,
#define DEF_RULE_NC(rule, kind, ...)
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC

#define DEF_RULE(rule, comp, kind, ...)
#define DEF_RULE_NC(rule, kind, ...)  __VA_ARGS__,
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC

#undef tok
#undef rule
#undef opt_rule
};


#define RULE_EXPAND(x) x
#define RULE_PADDING(rule, ...) RULE_PADDING2(rule, __VA_ARGS__, RULE_PADDING_IDS(rule))
#define RULE_PADDING2(rule, ...) RULE_EXPAND(RULE_PADDING3(rule, __VA_ARGS__))
#define RULE_PADDING3(rule, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, ...) __VA_ARGS__
#define RULE_PADDING_IDS(r) PAD13_##r, PAD12_##r, PAD11_##r, PAD10_##r, PAD9_##r, PAD8_##r, PAD7_##r, PAD6_##r, PAD5_##r, PAD4_##r, PAD3_##r, PAD2_##r, PAD1_##r,


enum {
#define DEF_RULE(rule, comp, kind, ...) RULE_PADDING(rule, __VA_ARGS__)
#define DEF_RULE_NC(rule, kind, ...)
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
#define DEF_RULE(rule, comp, kind, ...)
#define DEF_RULE_NC(rule, kind, ...) RULE_PADDING(rule, __VA_ARGS__)
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
};


#define RULE_ARG_OFFSET(rule, ...) RULE_ARG_OFFSET2(rule, __VA_ARGS__, RULE_ARG_OFFSET_IDS(rule))
#define RULE_ARG_OFFSET2(rule, ...) RULE_EXPAND(RULE_ARG_OFFSET3(rule, __VA_ARGS__))
#define RULE_ARG_OFFSET3(rule, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, ...) _14
#define RULE_ARG_OFFSET_IDS(r) PAD13_##r, PAD12_##r, PAD11_##r, PAD10_##r, PAD9_##r, PAD8_##r, PAD7_##r, PAD6_##r, PAD5_##r, PAD4_##r, PAD3_##r, PAD2_##r, PAD1_##r, PAD0_##r,





static const uint8_t rule_arg_offset_table[] = {
#define DEF_RULE(rule, comp, kind, ...) RULE_ARG_OFFSET(rule, __VA_ARGS__) & 0xff,
#define DEF_RULE_NC(rule, kind, ...)
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
    0, 
#define DEF_RULE(rule, comp, kind, ...)
#define DEF_RULE_NC(rule, kind, ...) RULE_ARG_OFFSET(rule, __VA_ARGS__) & 0xff,
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
};


static const size_t FIRST_RULE_WITH_OFFSET_ABOVE_255 =
#define DEF_RULE(rule, comp, kind, ...) RULE_ARG_OFFSET(rule, __VA_ARGS__) >= 0x100 ? RULE_##rule :
#define DEF_RULE_NC(rule, kind, ...)
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
#define DEF_RULE(rule, comp, kind, ...)
#define DEF_RULE_NC(rule, kind, ...) RULE_ARG_OFFSET(rule, __VA_ARGS__) >= 0x100 ? RULE_##rule :
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
0;

#if MICROPY_DEBUG_PARSE_RULE_NAME

static const char *const rule_name_table[] = {
#define DEF_RULE(rule, comp, kind, ...) #rule,
#define DEF_RULE_NC(rule, kind, ...)
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
    "", 
#define DEF_RULE(rule, comp, kind, ...)
#define DEF_RULE_NC(rule, kind, ...) #rule,
#include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
};
#endif



typedef struct _rule_stack_t {
    size_t src_line : (8 * sizeof(size_t) - 8); 
    size_t rule_id : 8; 
    size_t arg_i; 
} rule_stack_t;

typedef struct _mp_parse_chunk_t {
    size_t alloc;
    union {
        size_t used;
        struct _mp_parse_chunk_t *next;
    } union_;
    byte data[];
} mp_parse_chunk_t;

typedef struct _parser_t {
    size_t rule_stack_alloc;
    size_t rule_stack_top;
    rule_stack_t *rule_stack;

    size_t result_stack_alloc;
    size_t result_stack_top;
    mp_parse_node_t *result_stack;

    mp_lexer_t *lexer;

    mp_parse_tree_t tree;
    mp_parse_chunk_t *cur_chunk;

    #if MICROPY_COMP_CONST
    mp_map_t consts;
    #endif
} parser_t;

static void push_result_rule(parser_t *parser, size_t src_line, uint8_t rule_id, size_t num_args);

static const uint16_t *get_rule_arg(uint8_t r_id) {
    size_t off = rule_arg_offset_table[r_id];
    if (r_id >= FIRST_RULE_WITH_OFFSET_ABOVE_255) {
        off |= 0x100;
    }
    return &rule_arg_combined_table[off];
}

static void *parser_alloc(parser_t *parser, size_t num_bytes) {
    

    mp_parse_chunk_t *chunk = parser->cur_chunk;

    if (chunk != NULL && chunk->union_.used + num_bytes > chunk->alloc) {
        
        mp_parse_chunk_t *new_data = (mp_parse_chunk_t *)m_renew_maybe(byte, chunk,
            sizeof(mp_parse_chunk_t) + chunk->alloc,
            sizeof(mp_parse_chunk_t) + chunk->alloc + num_bytes, false);
        if (new_data == NULL) {
            
            (void)m_renew_maybe(byte, chunk, sizeof(mp_parse_chunk_t) + chunk->alloc,
                sizeof(mp_parse_chunk_t) + chunk->union_.used, false);
            chunk->alloc = chunk->union_.used;
            chunk->union_.next = parser->tree.chunk;
            parser->tree.chunk = chunk;
            chunk = NULL;
        } else {
            
            chunk->alloc += num_bytes;
        }
    }

    if (chunk == NULL) {
        
        size_t alloc = MICROPY_ALLOC_PARSE_CHUNK_INIT;
        if (alloc < num_bytes) {
            alloc = num_bytes;
        }
        chunk = (mp_parse_chunk_t *)m_new(byte, sizeof(mp_parse_chunk_t) + alloc);
        chunk->alloc = alloc;
        chunk->union_.used = 0;
        parser->cur_chunk = chunk;
    }

    byte *ret = chunk->data + chunk->union_.used;
    chunk->union_.used += num_bytes;
    return ret;
}

#if MICROPY_COMP_CONST_TUPLE
static void parser_free_parse_node_struct(parser_t *parser, mp_parse_node_struct_t *pns) {
    mp_parse_chunk_t *chunk = parser->cur_chunk;
    if (chunk->data <= (byte *)pns && (byte *)pns < chunk->data + chunk->union_.used) {
        size_t num_bytes = sizeof(mp_parse_node_struct_t) + sizeof(mp_parse_node_t) * MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
        chunk->union_.used -= num_bytes;
    }
}
#endif

static void push_rule(parser_t *parser, size_t src_line, uint8_t rule_id, size_t arg_i) {
    if (parser->rule_stack_top >= parser->rule_stack_alloc) {
        rule_stack_t *rs = m_renew(rule_stack_t, parser->rule_stack, parser->rule_stack_alloc, parser->rule_stack_alloc + MICROPY_ALLOC_PARSE_RULE_INC);
        parser->rule_stack = rs;
        parser->rule_stack_alloc += MICROPY_ALLOC_PARSE_RULE_INC;
    }
    rule_stack_t *rs = &parser->rule_stack[parser->rule_stack_top++];
    rs->src_line = src_line;
    rs->rule_id = rule_id;
    rs->arg_i = arg_i;
}

static void push_rule_from_arg(parser_t *parser, size_t arg) {
    assert((arg & RULE_ARG_KIND_MASK) == RULE_ARG_RULE || (arg & RULE_ARG_KIND_MASK) == RULE_ARG_OPT_RULE);
    size_t rule_id = arg & RULE_ARG_ARG_MASK;
    push_rule(parser, parser->lexer->tok_line, rule_id, 0);
}

static uint8_t pop_rule(parser_t *parser, size_t *arg_i, size_t *src_line) {
    parser->rule_stack_top -= 1;
    uint8_t rule_id = parser->rule_stack[parser->rule_stack_top].rule_id;
    *arg_i = parser->rule_stack[parser->rule_stack_top].arg_i;
    *src_line = parser->rule_stack[parser->rule_stack_top].src_line;
    return rule_id;
}

#if MICROPY_COMP_CONST_TUPLE
static uint8_t peek_rule(parser_t *parser, size_t n) {
    assert(parser->rule_stack_top > n);
    return parser->rule_stack[parser->rule_stack_top - 1 - n].rule_id;
}
#endif

bool mp_parse_node_get_int_maybe(mp_parse_node_t pn, mp_obj_t *o) {
    if (MP_PARSE_NODE_IS_SMALL_INT(pn)) {
        *o = MP_OBJ_NEW_SMALL_INT(MP_PARSE_NODE_LEAF_SMALL_INT(pn));
        return true;
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, RULE_const_object)) {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        *o = mp_parse_node_extract_const_object(pns);
        return mp_obj_is_int(*o);
    } else {
        return false;
    }
}

#if MICROPY_COMP_CONST_TUPLE || MICROPY_COMP_CONST
static bool mp_parse_node_is_const(mp_parse_node_t pn) {
    if (MP_PARSE_NODE_IS_SMALL_INT(pn)) {
        
        return true;
    } else if (MP_PARSE_NODE_IS_LEAF(pn)) {
        
        uintptr_t kind = MP_PARSE_NODE_LEAF_KIND(pn);
        if (kind == MP_PARSE_NODE_STRING) {
            return true;
        } else if (kind == MP_PARSE_NODE_TOKEN) {
            uintptr_t arg = MP_PARSE_NODE_LEAF_ARG(pn);
            return arg == MP_TOKEN_KW_NONE
                   || arg == MP_TOKEN_KW_FALSE
                   || arg == MP_TOKEN_KW_TRUE
                   || arg == MP_TOKEN_ELLIPSIS;
        }
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, RULE_const_object)) {
        
        return true;
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, RULE_atom_paren)) {
        
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        return MP_PARSE_NODE_IS_NULL(pns->nodes[0]);
    }
    return false;
}

static mp_obj_t mp_parse_node_convert_to_obj(mp_parse_node_t pn) {
    assert(mp_parse_node_is_const(pn));
    if (MP_PARSE_NODE_IS_SMALL_INT(pn)) {
        mp_int_t arg = MP_PARSE_NODE_LEAF_SMALL_INT(pn);
        #if MICROPY_DYNAMIC_COMPILER
        mp_uint_t sign_mask = -((mp_uint_t)1 << (mp_dynamic_compiler.small_int_bits - 1));
        if (!((arg & sign_mask) == 0 || (arg & sign_mask) == sign_mask)) {
            
            return mp_obj_new_int_from_ll(arg);
        }
        #endif
        return MP_OBJ_NEW_SMALL_INT(arg);
    } else if (MP_PARSE_NODE_IS_LEAF(pn)) {
        uintptr_t kind = MP_PARSE_NODE_LEAF_KIND(pn);
        uintptr_t arg = MP_PARSE_NODE_LEAF_ARG(pn);
        if (kind == MP_PARSE_NODE_STRING) {
            return MP_OBJ_NEW_QSTR(arg);
        } else {
            assert(MP_PARSE_NODE_LEAF_KIND(pn) == MP_PARSE_NODE_TOKEN);
            switch (arg) {
                case MP_TOKEN_KW_NONE:
                    return mp_const_none;
                case MP_TOKEN_KW_FALSE:
                    return mp_const_false;
                case MP_TOKEN_KW_TRUE:
                    return mp_const_true;
                default:
                    assert(arg == MP_TOKEN_ELLIPSIS);
                    return MP_OBJ_FROM_PTR(&mp_const_ellipsis_obj);
            }
        }
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, RULE_const_object)) {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        return mp_parse_node_extract_const_object(pns);
    } else {
        assert(MP_PARSE_NODE_IS_STRUCT_KIND(pn, RULE_atom_paren));
        assert(MP_PARSE_NODE_IS_NULL(((mp_parse_node_struct_t *)pn)->nodes[0]));
        return mp_const_empty_tuple;
    }
}
#endif

static bool parse_node_is_const_bool(mp_parse_node_t pn, bool value) {
    
    #if MICROPY_COMP_CONST_TUPLE || MICROPY_COMP_CONST
    return mp_parse_node_is_const(pn) && mp_obj_is_true(mp_parse_node_convert_to_obj(pn)) == value;
    #else
    return MP_PARSE_NODE_IS_TOKEN_KIND(pn, value ? MP_TOKEN_KW_TRUE : MP_TOKEN_KW_FALSE)
           || (MP_PARSE_NODE_IS_SMALL_INT(pn) && !!MP_PARSE_NODE_LEAF_SMALL_INT(pn) == value);
    #endif
}

bool mp_parse_node_is_const_false(mp_parse_node_t pn) {
    return parse_node_is_const_bool(pn, false);
}

bool mp_parse_node_is_const_true(mp_parse_node_t pn) {
    return parse_node_is_const_bool(pn, true);
}

size_t mp_parse_node_extract_list(mp_parse_node_t *pn, size_t pn_kind, mp_parse_node_t **nodes) {
    if (MP_PARSE_NODE_IS_NULL(*pn)) {
        *nodes = NULL;
        return 0;
    } else if (MP_PARSE_NODE_IS_LEAF(*pn)) {
        *nodes = pn;
        return 1;
    } else {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)(*pn);
        if (MP_PARSE_NODE_STRUCT_KIND(pns) != pn_kind) {
            *nodes = pn;
            return 1;
        } else {
            *nodes = pns->nodes;
            return MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
        }
    }
}

#if MICROPY_DEBUG_PRINTERS
void mp_parse_node_print(const mp_print_t *print, mp_parse_node_t pn, size_t indent) {
    if (MP_PARSE_NODE_IS_STRUCT(pn)) {
        mp_printf(print, "[% 4d] ", (int)((mp_parse_node_struct_t *)pn)->source_line);
    } else {
        mp_printf(print, "       ");
    }
    for (size_t i = 0; i < indent; i++) {
        mp_printf(print, " ");
    }
    if (MP_PARSE_NODE_IS_NULL(pn)) {
        mp_printf(print, "NULL\n");
    } else if (MP_PARSE_NODE_IS_SMALL_INT(pn)) {
        mp_int_t arg = MP_PARSE_NODE_LEAF_SMALL_INT(pn);
        mp_printf(print, "int(" INT_FMT ")\n", arg);
    } else if (MP_PARSE_NODE_IS_LEAF(pn)) {
        uintptr_t arg = MP_PARSE_NODE_LEAF_ARG(pn);
        switch (MP_PARSE_NODE_LEAF_KIND(pn)) {
            case MP_PARSE_NODE_ID:
                mp_printf(print, "id(%s)\n", qstr_str(arg));
                break;
            case MP_PARSE_NODE_STRING:
                mp_printf(print, "str(%s)\n", qstr_str(arg));
                break;
            default:
                assert(MP_PARSE_NODE_LEAF_KIND(pn) == MP_PARSE_NODE_TOKEN);
                mp_printf(print, "tok(%u)\n", (uint)arg);
                break;
        }
    } else {
        
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        if (MP_PARSE_NODE_STRUCT_KIND(pns) == RULE_const_object) {
            mp_obj_t obj = mp_parse_node_extract_const_object(pns);
            #if MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_D
            mp_printf(print, "literal const(%016llx)=", obj);
            #else
            mp_printf(print, "literal const(%p)=", obj);
            #endif
            mp_obj_print_helper(print, obj, PRINT_REPR);
            mp_printf(print, "\n");
        } else {
            size_t n = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
            #if MICROPY_DEBUG_PARSE_RULE_NAME
            mp_printf(print, "%s(%u) (n=%u)\n", rule_name_table[MP_PARSE_NODE_STRUCT_KIND(pns)], (uint)MP_PARSE_NODE_STRUCT_KIND(pns), (uint)n);
            #else
            mp_printf(print, "rule(%u) (n=%u)\n", (uint)MP_PARSE_NODE_STRUCT_KIND(pns), (uint)n);
            #endif
            for (size_t i = 0; i < n; i++) {
                mp_parse_node_print(print, pns->nodes[i], indent + 2);
            }
        }
    }
}
#endif 

 

static mp_parse_node_t pop_result(parser_t *parser) {
    assert(parser->result_stack_top > 0);
    return parser->result_stack[--parser->result_stack_top];
}

static mp_parse_node_t peek_result(parser_t *parser, size_t pos) {
    assert(parser->result_stack_top > pos);
    return parser->result_stack[parser->result_stack_top - 1 - pos];
}

static void push_result_node(parser_t *parser, mp_parse_node_t pn) {
    if (parser->result_stack_top >= parser->result_stack_alloc) {
        mp_parse_node_t *stack = m_renew(mp_parse_node_t, parser->result_stack, parser->result_stack_alloc, parser->result_stack_alloc + MICROPY_ALLOC_PARSE_RESULT_INC);
        parser->result_stack = stack;
        parser->result_stack_alloc += MICROPY_ALLOC_PARSE_RESULT_INC;
    }
    parser->result_stack[parser->result_stack_top++] = pn;
}

static mp_parse_node_t make_node_const_object(parser_t *parser, size_t src_line, mp_obj_t obj) {
    mp_parse_node_struct_t *pn = parser_alloc(parser, sizeof(mp_parse_node_struct_t) + sizeof(mp_obj_t));
    pn->source_line = src_line;
    #if MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_D
    
    pn->kind_num_nodes = RULE_const_object | (2 << 8);
    pn->nodes[0] = (uint64_t)obj;
    pn->nodes[1] = (uint64_t)obj >> 32;
    #else
    pn->kind_num_nodes = RULE_const_object | (1 << 8);
    pn->nodes[0] = (uintptr_t)obj;
    #endif
    return (mp_parse_node_t)pn;
}



static mp_parse_node_t make_node_const_object_optimised(parser_t *parser, size_t src_line, mp_obj_t obj) {
    if (mp_obj_is_small_int(obj)) {
        mp_int_t val = MP_OBJ_SMALL_INT_VALUE(obj);
        #if MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_D
        
        if (((val ^ (val << 1)) & 0xffffffff80000000) != 0) {
            return make_node_const_object(parser, src_line, obj);
        }
        #endif
        #if MICROPY_DYNAMIC_COMPILER
        
        mp_uint_t sign_mask = -((mp_uint_t)1 << (mp_dynamic_compiler.small_int_bits - 1));
        if (!((val & sign_mask) == 0 || (val & sign_mask) == sign_mask)) {
            return make_node_const_object(parser, src_line, obj);
        }
        #endif
        return mp_parse_node_new_small_int(val);
    } else {
        return make_node_const_object(parser, src_line, obj);
    }
}

static void push_result_token(parser_t *parser, uint8_t rule_id) {
    mp_parse_node_t pn;
    mp_lexer_t *lex = parser->lexer;
    if (lex->tok_kind == MP_TOKEN_NAME) {
        qstr id = qstr_from_strn(lex->vstr.buf, lex->vstr.len);
        #if MICROPY_COMP_CONST
        
        mp_map_elem_t *elem;
        if (rule_id == RULE_atom
            && (elem = mp_map_lookup(&parser->consts, MP_OBJ_NEW_QSTR(id), MP_MAP_LOOKUP)) != NULL) {
            pn = make_node_const_object_optimised(parser, lex->tok_line, elem->value);
        } else {
            pn = mp_parse_node_new_leaf(MP_PARSE_NODE_ID, id);
        }
        #else
        (void)rule_id;
        pn = mp_parse_node_new_leaf(MP_PARSE_NODE_ID, id);
        #endif
    } else if (lex->tok_kind == MP_TOKEN_INTEGER) {
        mp_obj_t o = mp_parse_num_integer(lex->vstr.buf, lex->vstr.len, 0, lex);
        pn = make_node_const_object_optimised(parser, lex->tok_line, o);
    } else if (lex->tok_kind == MP_TOKEN_FLOAT_OR_IMAG) {
        mp_obj_t o = mp_parse_num_float(lex->vstr.buf, lex->vstr.len, true, lex);
        pn = make_node_const_object(parser, lex->tok_line, o);
    } else if (lex->tok_kind == MP_TOKEN_STRING) {
        
        
        qstr qst = MP_QSTRnull;
        if (lex->vstr.len <= MICROPY_ALLOC_PARSE_INTERN_STRING_LEN) {
            
            qst = qstr_from_strn(lex->vstr.buf, lex->vstr.len);
        } else {
            
            qst = qstr_find_strn(lex->vstr.buf, lex->vstr.len);
        }
        if (qst != MP_QSTRnull) {
            
            pn = mp_parse_node_new_leaf(MP_PARSE_NODE_STRING, qst);
        } else {
            
            mp_obj_t o = mp_obj_new_str_copy(&mp_type_str, (const byte *)lex->vstr.buf, lex->vstr.len);
            pn = make_node_const_object(parser, lex->tok_line, o);
        }
    } else if (lex->tok_kind == MP_TOKEN_BYTES) {
        
        mp_obj_t o = mp_obj_new_bytes((const byte *)lex->vstr.buf, lex->vstr.len);
        pn = make_node_const_object(parser, lex->tok_line, o);
    } else {
        pn = mp_parse_node_new_leaf(MP_PARSE_NODE_TOKEN, lex->tok_kind);
    }
    push_result_node(parser, pn);
}

#if MICROPY_COMP_CONST_FOLDING

#if MICROPY_COMP_MODULE_CONST
static const mp_rom_map_elem_t mp_constants_table[] = {
    #if MICROPY_PY_ERRNO
    { MP_ROM_QSTR(MP_QSTR_errno), MP_ROM_PTR(&mp_module_errno) },
    #endif
    #if MICROPY_PY_UCTYPES
    { MP_ROM_QSTR(MP_QSTR_uctypes), MP_ROM_PTR(&mp_module_uctypes) },
    #endif
    
    MICROPY_PORT_CONSTANTS
};
static MP_DEFINE_CONST_MAP(mp_constants_map, mp_constants_table);
#endif

#if MICROPY_COMP_CONST_FOLDING_COMPILER_WORKAROUND


MP_NOINLINE
#endif
static bool fold_logical_constants(parser_t *parser, uint8_t rule_id, size_t *num_args) {
    if (rule_id == RULE_or_test
        || rule_id == RULE_and_test) {
        
        size_t copy_to = *num_args;
        for (size_t i = copy_to; i > 0;) {
            mp_parse_node_t pn = peek_result(parser, --i);
            parser->result_stack[parser->result_stack_top - copy_to] = pn;
            if (i == 0) {
                
                break;
            }
            if (rule_id == RULE_or_test) {
                if (mp_parse_node_is_const_true(pn)) {
                    
                    break;
                } else if (!mp_parse_node_is_const_false(pn)) {
                    copy_to -= 1;
                }
            } else {
                
                if (mp_parse_node_is_const_false(pn)) {
                    break;
                } else if (!mp_parse_node_is_const_true(pn)) {
                    copy_to -= 1;
                }
            }
        }
        copy_to -= 1; 

        
        for (size_t i = 0; i < copy_to; ++i) {
            pop_result(parser);
        }
        *num_args -= copy_to;

        
        return *num_args == 1;

    } else if (rule_id == RULE_not_test_2) {
        
        mp_parse_node_t pn = peek_result(parser, 0);
        if (mp_parse_node_is_const_false(pn)) {
            pn = mp_parse_node_new_leaf(MP_PARSE_NODE_TOKEN, MP_TOKEN_KW_TRUE);
        } else if (mp_parse_node_is_const_true(pn)) {
            pn = mp_parse_node_new_leaf(MP_PARSE_NODE_TOKEN, MP_TOKEN_KW_FALSE);
        } else {
            return false;
        }
        pop_result(parser);
        push_result_node(parser, pn);
        return true;
    }

    return false;
}

static bool fold_constants(parser_t *parser, uint8_t rule_id, size_t num_args) {
    
    

    mp_obj_t arg0;
    if (rule_id == RULE_expr
        || rule_id == RULE_xor_expr
        || rule_id == RULE_and_expr
        || rule_id == RULE_power) {
        
        mp_parse_node_t pn = peek_result(parser, num_args - 1);
        if (!mp_parse_node_get_int_maybe(pn, &arg0)) {
            return false;
        }
        mp_binary_op_t op;
        if (rule_id == RULE_expr) {
            op = MP_BINARY_OP_OR;
        } else if (rule_id == RULE_xor_expr) {
            op = MP_BINARY_OP_XOR;
        } else if (rule_id == RULE_and_expr) {
            op = MP_BINARY_OP_AND;
        } else {
            op = MP_BINARY_OP_POWER;
        }
        for (ssize_t i = num_args - 2; i >= 0; --i) {
            pn = peek_result(parser, i);
            mp_obj_t arg1;
            if (!mp_parse_node_get_int_maybe(pn, &arg1)) {
                return false;
            }
            if (op == MP_BINARY_OP_POWER && mp_obj_int_sign(arg1) < 0) {
                
                return false;
            }
            arg0 = mp_binary_op(op, arg0, arg1);
        }
    } else if (rule_id == RULE_shift_expr
               || rule_id == RULE_arith_expr
               || rule_id == RULE_term) {
        
        mp_parse_node_t pn = peek_result(parser, num_args - 1);
        if (!mp_parse_node_get_int_maybe(pn, &arg0)) {
            return false;
        }
        for (ssize_t i = num_args - 2; i >= 1; i -= 2) {
            pn = peek_result(parser, i - 1);
            mp_obj_t arg1;
            if (!mp_parse_node_get_int_maybe(pn, &arg1)) {
                return false;
            }
            mp_token_kind_t tok = MP_PARSE_NODE_LEAF_ARG(peek_result(parser, i));
            if (tok == MP_TOKEN_OP_AT || tok == MP_TOKEN_OP_SLASH) {
                
                return false;
            }
            mp_binary_op_t op = MP_BINARY_OP_LSHIFT + (tok - MP_TOKEN_OP_DBL_LESS);
            int rhs_sign = mp_obj_int_sign(arg1);
            if (op <= MP_BINARY_OP_RSHIFT) {
                
                if (rhs_sign < 0) {
                    return false;
                }
            } else if (op >= MP_BINARY_OP_FLOOR_DIVIDE) {
                
                if (rhs_sign == 0) {
                    return false;
                }
            }
            arg0 = mp_binary_op(op, arg0, arg1);
        }
    } else if (rule_id == RULE_factor_2) {
        
        mp_parse_node_t pn = peek_result(parser, 0);
        if (!mp_parse_node_get_int_maybe(pn, &arg0)) {
            return false;
        }
        mp_token_kind_t tok = MP_PARSE_NODE_LEAF_ARG(peek_result(parser, 1));
        mp_unary_op_t op;
        if (tok == MP_TOKEN_OP_TILDE) {
            op = MP_UNARY_OP_INVERT;
        } else {
            assert(tok == MP_TOKEN_OP_PLUS || tok == MP_TOKEN_OP_MINUS); 
            op = MP_UNARY_OP_POSITIVE + (tok - MP_TOKEN_OP_PLUS);
        }
        arg0 = mp_unary_op(op, arg0);

    #if MICROPY_COMP_CONST
    } else if (rule_id == RULE_expr_stmt) {
        mp_parse_node_t pn1 = peek_result(parser, 0);
        if (!MP_PARSE_NODE_IS_NULL(pn1)
            && !(MP_PARSE_NODE_IS_STRUCT_KIND(pn1, RULE_expr_stmt_augassign)
                 || MP_PARSE_NODE_IS_STRUCT_KIND(pn1, RULE_expr_stmt_assign_list))) {
            
            mp_parse_node_t pn0 = peek_result(parser, 1);
            if (MP_PARSE_NODE_IS_ID(pn0)
                && MP_PARSE_NODE_IS_STRUCT_KIND(pn1, RULE_atom_expr_normal)
                && MP_PARSE_NODE_IS_ID(((mp_parse_node_struct_t *)pn1)->nodes[0])
                && MP_PARSE_NODE_LEAF_ARG(((mp_parse_node_struct_t *)pn1)->nodes[0]) == MP_QSTR_const
                && MP_PARSE_NODE_IS_STRUCT_KIND(((mp_parse_node_struct_t *)pn1)->nodes[1], RULE_trailer_paren)
                ) {
                

                
                qstr id = MP_PARSE_NODE_LEAF_ARG(pn0);

                
                mp_parse_node_t pn_value = ((mp_parse_node_struct_t *)((mp_parse_node_struct_t *)pn1)->nodes[1])->nodes[0];
                if (!mp_parse_node_is_const(pn_value)) {
                    mp_obj_t exc = mp_obj_new_exception_msg(&mp_type_SyntaxError,
                        MP_ERROR_TEXT("not a constant"));
                    mp_obj_exception_add_traceback(exc, parser->lexer->source_name,
                        ((mp_parse_node_struct_t *)pn1)->source_line, MP_QSTRnull);
                    nlr_raise(exc);
                }
                mp_obj_t value = mp_parse_node_convert_to_obj(pn_value);

                
                mp_map_elem_t *elem = mp_map_lookup(&parser->consts, MP_OBJ_NEW_QSTR(id), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
                assert(elem->value == MP_OBJ_NULL);
                elem->value = value;

                
                
                if (qstr_str(id)[0] == '_') {
                    pop_result(parser); 
                    pop_result(parser); 
                    push_result_rule(parser, 0, RULE_pass_stmt, 0); 
                    return true;
                }

                
                pop_result(parser);
                push_result_node(parser, pn_value);

                
                return false;
            }
        }
        return false;
    #endif

    #if MICROPY_COMP_MODULE_CONST
    } else if (rule_id == RULE_atom_expr_normal) {
        mp_parse_node_t pn0 = peek_result(parser, 1);
        mp_parse_node_t pn1 = peek_result(parser, 0);
        if (!(MP_PARSE_NODE_IS_ID(pn0)
              && MP_PARSE_NODE_IS_STRUCT_KIND(pn1, RULE_trailer_period))) {
            return false;
        }
        
        
        mp_parse_node_struct_t *pns1 = (mp_parse_node_struct_t *)pn1;
        assert(MP_PARSE_NODE_IS_ID(pns1->nodes[0]));
        qstr q_base = MP_PARSE_NODE_LEAF_ARG(pn0);
        qstr q_attr = MP_PARSE_NODE_LEAF_ARG(pns1->nodes[0]);
        mp_map_elem_t *elem = mp_map_lookup((mp_map_t *)&mp_constants_map, MP_OBJ_NEW_QSTR(q_base), MP_MAP_LOOKUP);
        if (elem == NULL) {
            return false;
        }
        mp_obj_t dest[2];
        mp_load_method_maybe(elem->value, q_attr, dest);
        if (!(dest[0] != MP_OBJ_NULL && mp_obj_is_int(dest[0]) && dest[1] == MP_OBJ_NULL)) {
            return false;
        }
        arg0 = dest[0];
    #endif

    } else {
        return false;
    }

    

    for (size_t i = num_args; i > 0; i--) {
        pop_result(parser);
    }
    push_result_node(parser, make_node_const_object_optimised(parser, 0, arg0));

    return true;
}

#endif 

#if MICROPY_COMP_CONST_TUPLE
static bool build_tuple_from_stack(parser_t *parser, size_t src_line, size_t num_args) {
    for (size_t i = num_args; i > 0;) {
        mp_parse_node_t pn = peek_result(parser, --i);
        if (!mp_parse_node_is_const(pn)) {
            return false;
        }
    }
    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(mp_obj_new_tuple(num_args, NULL));
    for (size_t i = num_args; i > 0;) {
        mp_parse_node_t pn = pop_result(parser);
        tuple->items[--i] = mp_parse_node_convert_to_obj(pn);
        if (MP_PARSE_NODE_IS_STRUCT(pn)) {
            parser_free_parse_node_struct(parser, (mp_parse_node_struct_t *)pn);
        }
    }
    push_result_node(parser, make_node_const_object(parser, src_line, MP_OBJ_FROM_PTR(tuple)));
    return true;
}

static bool build_tuple(parser_t *parser, size_t src_line, uint8_t rule_id, size_t num_args) {
    if (rule_id == RULE_testlist_comp) {
        if (peek_rule(parser, 0) == RULE_atom_paren) {
            
            return build_tuple_from_stack(parser, src_line, num_args);
        }
    }
    if (rule_id == RULE_testlist_comp_3c) {
        assert(peek_rule(parser, 0) == RULE_testlist_comp_3b);
        assert(peek_rule(parser, 1) == RULE_testlist_comp);
        if (peek_rule(parser, 2) == RULE_atom_paren) {
            
            if (build_tuple_from_stack(parser, src_line, num_args)) {
                parser->rule_stack_top -= 2; 
                return true;
            }
        }
    }
    if (rule_id == RULE_testlist_star_expr
        || rule_id == RULE_testlist
        || rule_id == RULE_subscriptlist) {
        
        
        
        
        
        return build_tuple_from_stack(parser, src_line, num_args);
    }

    return false;
}
#endif

static void push_result_rule(parser_t *parser, size_t src_line, uint8_t rule_id, size_t num_args) {
    
    if (rule_id == RULE_atom_paren) {
        
        
        
        mp_parse_node_t pn = peek_result(parser, 0);
        if (MP_PARSE_NODE_IS_NULL(pn)) {
            
        } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, RULE_testlist_comp)) {
            
        } else {
            
            return;
        }
    } else if (rule_id == RULE_testlist_comp) {
        
        
        assert(num_args == 2);
        mp_parse_node_t pn = peek_result(parser, 0);
        if (MP_PARSE_NODE_IS_STRUCT(pn)) {
            mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
            if (MP_PARSE_NODE_STRUCT_KIND(pns) == RULE_testlist_comp_3b) {
                
                pop_result(parser);
                --num_args;
            } else if (MP_PARSE_NODE_STRUCT_KIND(pns) == RULE_testlist_comp_3c) {
                
                pop_result(parser);
                assert(pn == peek_result(parser, 0));
                pns->kind_num_nodes = rule_id | MP_PARSE_NODE_STRUCT_NUM_NODES(pns) << 8;
                return;
            } else if (MP_PARSE_NODE_STRUCT_KIND(pns) == RULE_comp_for) {
                
            } else {
                
            }
        } else {
            
        }
    } else if (rule_id == RULE_testlist_comp_3c) {
        
        ++num_args;
    }

    #if MICROPY_COMP_CONST_FOLDING
    if (fold_logical_constants(parser, rule_id, &num_args)) {
        
        return;
    }
    if (fold_constants(parser, rule_id, num_args)) {
        
        return;
    }
    #endif

    #if MICROPY_COMP_CONST_TUPLE
    if (build_tuple(parser, src_line, rule_id, num_args)) {
        
        return;
    }
    #endif

    mp_parse_node_struct_t *pn = parser_alloc(parser, sizeof(mp_parse_node_struct_t) + sizeof(mp_parse_node_t) * num_args);
    pn->source_line = src_line;
    pn->kind_num_nodes = (rule_id & 0xff) | (num_args << 8);
    for (size_t i = num_args; i > 0; i--) {
        pn->nodes[i - 1] = pop_result(parser);
    }
    if (rule_id == RULE_testlist_comp_3c) {
        
        push_result_node(parser, (mp_parse_node_t)pn);
    }
    push_result_node(parser, (mp_parse_node_t)pn);
}

mp_parse_tree_t mp_parse(mp_lexer_t *lex, mp_parse_input_kind_t input_kind) {
    
    MP_DEFINE_NLR_JUMP_CALLBACK_FUNCTION_1(ctx, mp_lexer_free, lex);
    nlr_push_jump_callback(&ctx.callback, mp_call_function_1_from_nlr_jump_callback);

    

    parser_t parser;

    parser.rule_stack_alloc = MICROPY_ALLOC_PARSE_RULE_INIT;
    parser.rule_stack_top = 0;
    parser.rule_stack = m_new(rule_stack_t, parser.rule_stack_alloc);

    parser.result_stack_alloc = MICROPY_ALLOC_PARSE_RESULT_INIT;
    parser.result_stack_top = 0;
    parser.result_stack = m_new(mp_parse_node_t, parser.result_stack_alloc);

    parser.lexer = lex;

    parser.tree.chunk = NULL;
    parser.cur_chunk = NULL;

    #if MICROPY_COMP_CONST
    mp_map_init(&parser.consts, 0);
    #endif

    
    size_t top_level_rule;
    switch (input_kind) {
        case MP_PARSE_SINGLE_INPUT:
            top_level_rule = RULE_single_input;
            break;
        case MP_PARSE_EVAL_INPUT:
            top_level_rule = RULE_eval_input;
            break;
        default:
            top_level_rule = RULE_file_input;
    }
    push_rule(&parser, lex->tok_line, top_level_rule, 0);

    

    bool backtrack = false;

    for (;;) {
    next_rule:
        if (parser.rule_stack_top == 0) {
            break;
        }

        
        size_t i; 
        size_t rule_src_line; 
        uint8_t rule_id = pop_rule(&parser, &i, &rule_src_line);
        uint8_t rule_act = rule_act_table[rule_id];
        const uint16_t *rule_arg = get_rule_arg(rule_id);
        size_t n = rule_act & RULE_ACT_ARG_MASK;

        #if 0
        
        printf("depth=" UINT_FMT " ", parser.rule_stack_top);
        for (int j = 0; j < parser.rule_stack_top; ++j) {
            printf(" ");
        }
        printf("%s n=" UINT_FMT " i=" UINT_FMT " bt=%d\n", rule_name_table[rule_id], n, i, backtrack);
        #endif

        switch (rule_act & RULE_ACT_KIND_MASK) {
            case RULE_ACT_OR:
                if (i > 0 && !backtrack) {
                    goto next_rule;
                } else {
                    backtrack = false;
                }
                for (; i < n; ++i) {
                    uint16_t kind = rule_arg[i] & RULE_ARG_KIND_MASK;
                    if (kind == RULE_ARG_TOK) {
                        if (lex->tok_kind == (rule_arg[i] & RULE_ARG_ARG_MASK)) {
                            push_result_token(&parser, rule_id);
                            mp_lexer_to_next(lex);
                            goto next_rule;
                        }
                    } else {
                        assert(kind == RULE_ARG_RULE);
                        if (i + 1 < n) {
                            push_rule(&parser, rule_src_line, rule_id, i + 1); 
                        }
                        push_rule_from_arg(&parser, rule_arg[i]); 
                        goto next_rule;
                    }
                }
                backtrack = true;
                break;

            case RULE_ACT_AND: {

                
                if (backtrack) {
                    assert(i > 0);
                    if ((rule_arg[i - 1] & RULE_ARG_KIND_MASK) == RULE_ARG_OPT_RULE) {
                        
                        push_result_node(&parser, MP_PARSE_NODE_NULL);
                        backtrack = false;
                    } else {
                        
                        if (i > 1) {
                            
                            goto syntax_error;
                        } else {
                            goto next_rule;
                        }
                    }
                }

                
                for (; i < n; ++i) {
                    if ((rule_arg[i] & RULE_ARG_KIND_MASK) == RULE_ARG_TOK) {
                        
                        mp_token_kind_t tok_kind = rule_arg[i] & RULE_ARG_ARG_MASK;
                        if (lex->tok_kind == tok_kind) {
                            
                            if (tok_kind == MP_TOKEN_NAME) {
                                push_result_token(&parser, rule_id);
                            }
                            mp_lexer_to_next(lex);
                        } else {
                            
                            if (i > 0) {
                                
                                goto syntax_error;
                            } else {
                                
                                backtrack = true;
                                goto next_rule;
                            }
                        }
                    } else {
                        push_rule(&parser, rule_src_line, rule_id, i + 1); 
                        push_rule_from_arg(&parser, rule_arg[i]); 
                        goto next_rule;
                    }
                }

                assert(i == n);

                

                #if !MICROPY_ENABLE_DOC_STRING
                
                if (input_kind != MP_PARSE_SINGLE_INPUT && rule_id == RULE_expr_stmt && peek_result(&parser, 0) == MP_PARSE_NODE_NULL) {
                    mp_parse_node_t p = peek_result(&parser, 1);
                    if ((MP_PARSE_NODE_IS_LEAF(p) && !MP_PARSE_NODE_IS_ID(p))
                        || MP_PARSE_NODE_IS_STRUCT_KIND(p, RULE_const_object)) {
                        pop_result(&parser); 
                        pop_result(&parser); 
                        
                        
                        
                        push_result_rule(&parser, rule_src_line, RULE_pass_stmt, 0);
                        break;
                    }
                }
                #endif

                
                i = 0;
                size_t num_not_nil = 0;
                for (size_t x = n; x > 0;) {
                    --x;
                    if ((rule_arg[x] & RULE_ARG_KIND_MASK) == RULE_ARG_TOK) {
                        mp_token_kind_t tok_kind = rule_arg[x] & RULE_ARG_ARG_MASK;
                        if (tok_kind == MP_TOKEN_NAME) {
                            
                            i += 1;
                            num_not_nil += 1;
                        }
                    } else {
                        
                        if (peek_result(&parser, i) != MP_PARSE_NODE_NULL) {
                            num_not_nil += 1;
                        }
                        i += 1;
                    }
                }

                if (num_not_nil == 1 && (rule_act & RULE_ACT_ALLOW_IDENT)) {
                    
                    mp_parse_node_t pn = MP_PARSE_NODE_NULL;
                    for (size_t x = 0; x < i; ++x) {
                        mp_parse_node_t pn2 = pop_result(&parser);
                        if (pn2 != MP_PARSE_NODE_NULL) {
                            pn = pn2;
                        }
                    }
                    push_result_node(&parser, pn);
                } else {
                    

                    if (rule_act & RULE_ACT_ADD_BLANK) {
                        
                        push_result_node(&parser, MP_PARSE_NODE_NULL);
                        i += 1;
                    }

                    push_result_rule(&parser, rule_src_line, rule_id, i);
                }
                break;
            }

            default: {
                assert((rule_act & RULE_ACT_KIND_MASK) == RULE_ACT_LIST);

                
                
                
                bool had_trailing_sep;
                if (backtrack) {
                list_backtrack:
                    had_trailing_sep = false;
                    if (n == 2) {
                        if (i == 1) {
                            
                            goto next_rule;
                        } else {
                            
                            backtrack = false;
                        }
                    } else {
                        if (i == 1) {
                            
                            goto next_rule;
                        } else if ((i & 1) == 1) {
                            
                            if (n == 3) {
                                
                                had_trailing_sep = true;
                                backtrack = false;
                            } else {
                                
                                goto syntax_error;
                            }
                        } else {
                            
                            backtrack = false;
                        }
                    }
                } else {
                    for (;;) {
                        size_t arg = rule_arg[i & 1 & n];
                        if ((arg & RULE_ARG_KIND_MASK) == RULE_ARG_TOK) {
                            if (lex->tok_kind == (arg & RULE_ARG_ARG_MASK)) {
                                if (i & 1 & n) {
                                    
                                } else {
                                    push_result_token(&parser, rule_id);
                                }
                                mp_lexer_to_next(lex);
                                
                                i += 1;
                            } else {
                                
                                i += 1;
                                backtrack = true;
                                goto list_backtrack;
                            }
                        } else {
                            assert((arg & RULE_ARG_KIND_MASK) == RULE_ARG_RULE);
                            push_rule(&parser, rule_src_line, rule_id, i + 1); 
                            push_rule_from_arg(&parser, arg); 
                            goto next_rule;
                        }
                    }
                }
                assert(i >= 1);

                
                i -= 1;
                if ((n & 1) && (rule_arg[1] & RULE_ARG_KIND_MASK) == RULE_ARG_TOK) {
                    
                    i = (i + 1) / 2;
                }

                if (i == 1) {
                    
                    if (had_trailing_sep) {
                        
                        push_result_rule(&parser, rule_src_line, rule_id, i);
                    } else {
                        
                    }
                } else {
                    push_result_rule(&parser, rule_src_line, rule_id, i);
                }
                break;
            }
        }
    }

    #if MICROPY_COMP_CONST
    mp_map_deinit(&parser.consts);
    #endif

    
    if (parser.cur_chunk != NULL) {
        (void)m_renew_maybe(byte, parser.cur_chunk,
            sizeof(mp_parse_chunk_t) + parser.cur_chunk->alloc,
            sizeof(mp_parse_chunk_t) + parser.cur_chunk->union_.used,
            false);
        parser.cur_chunk->alloc = parser.cur_chunk->union_.used;
        parser.cur_chunk->union_.next = parser.tree.chunk;
        parser.tree.chunk = parser.cur_chunk;
    }

    if (
        lex->tok_kind != MP_TOKEN_END 
        || parser.result_stack_top == 0 
        ) {
    syntax_error:;
        mp_obj_t exc;
        if (lex->tok_kind == MP_TOKEN_INDENT) {
            exc = mp_obj_new_exception_msg(&mp_type_IndentationError,
                MP_ERROR_TEXT("unexpected indent"));
        } else if (lex->tok_kind == MP_TOKEN_DEDENT_MISMATCH) {
            exc = mp_obj_new_exception_msg(&mp_type_IndentationError,
                MP_ERROR_TEXT("unindent doesn't match any outer indent level"));
        #if MICROPY_PY_FSTRINGS
        } else if (lex->tok_kind == MP_TOKEN_MALFORMED_FSTRING) {
            exc = mp_obj_new_exception_msg(&mp_type_SyntaxError,
                MP_ERROR_TEXT("malformed f-string"));
        } else if (lex->tok_kind == MP_TOKEN_FSTRING_RAW) {
            exc = mp_obj_new_exception_msg(&mp_type_SyntaxError,
                MP_ERROR_TEXT("raw f-strings are not supported"));
        #endif
        } else {
            exc = mp_obj_new_exception_msg(&mp_type_SyntaxError,
                MP_ERROR_TEXT("invalid syntax"));
        }
        
        
        mp_obj_exception_add_traceback(exc, lex->source_name, lex->tok_line, MP_QSTRnull);
        nlr_raise(exc);
    }

    
    assert(parser.result_stack_top == 1);
    parser.tree.root = parser.result_stack[0];

    
    m_del(rule_stack_t, parser.rule_stack, parser.rule_stack_alloc);
    m_del(mp_parse_node_t, parser.result_stack, parser.result_stack_alloc);

    
    nlr_pop_jump_callback(true);

    return parser.tree;
}

void mp_parse_tree_clear(mp_parse_tree_t *tree) {
    mp_parse_chunk_t *chunk = tree->chunk;
    while (chunk != NULL) {
        mp_parse_chunk_t *next = chunk->union_.next;
        m_del(byte, chunk, sizeof(mp_parse_chunk_t) + chunk->alloc);
        chunk = next;
    }
    tree->chunk = NULL; 
}

#endif 
