 

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "py/scope.h"
#include "py/emit.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/asmbase.h"
#include "py/nativeglue.h"
#include "py/persistentcode.h"
#include "py/smallint.h"

#if MICROPY_ENABLE_COMPILER

#define INVALID_LABEL (0xffff)

typedef enum {

#define DEF_RULE(rule, comp, kind, ...) PN_##rule,
#define DEF_RULE_NC(rule, kind, ...)
    #include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
    PN_const_object, 

#define DEF_RULE(rule, comp, kind, ...)
#define DEF_RULE_NC(rule, kind, ...) PN_##rule,
    #include "py/grammar.h"
#undef DEF_RULE
#undef DEF_RULE_NC
} pn_kind_t;



#define MP_PARSE_NODE_TESTLIST_COMP_HAS_COMP_FOR(pns) \
    (MP_PARSE_NODE_STRUCT_NUM_NODES(pns) == 2 && \
    MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[1], PN_comp_for))

#define NEED_METHOD_TABLE MICROPY_EMIT_NATIVE

#if NEED_METHOD_TABLE


#define EMIT(fun) (comp->emit_method_table->fun(comp->emit))
#define EMIT_ARG(fun, ...) (comp->emit_method_table->fun(comp->emit, __VA_ARGS__))
#define EMIT_LOAD_FAST(qst, local_num) (comp->emit_method_table->load_id.local(comp->emit, qst, local_num, MP_EMIT_IDOP_LOCAL_FAST))
#define EMIT_LOAD_GLOBAL(qst) (comp->emit_method_table->load_id.global(comp->emit, qst, MP_EMIT_IDOP_GLOBAL_GLOBAL))

#else


#define EMIT(fun) (mp_emit_bc_##fun(comp->emit))
#define EMIT_ARG(fun, ...) (mp_emit_bc_##fun(comp->emit, __VA_ARGS__))
#define EMIT_LOAD_FAST(qst, local_num) (mp_emit_bc_load_local(comp->emit, qst, local_num, MP_EMIT_IDOP_LOCAL_FAST))
#define EMIT_LOAD_GLOBAL(qst) (mp_emit_bc_load_global(comp->emit, qst, MP_EMIT_IDOP_GLOBAL_GLOBAL))

#endif

#if MICROPY_EMIT_NATIVE && MICROPY_DYNAMIC_COMPILER

#define NATIVE_EMITTER(f) emit_native_table[mp_dynamic_compiler.native_arch]->emit_##f
#define NATIVE_EMITTER_TABLE (emit_native_table[mp_dynamic_compiler.native_arch])

static const emit_method_table_t *emit_native_table[] = {
    NULL,
    &emit_native_x86_method_table,
    &emit_native_x64_method_table,
    &emit_native_arm_method_table,
    &emit_native_thumb_method_table,
    &emit_native_thumb_method_table,
    &emit_native_thumb_method_table,
    &emit_native_thumb_method_table,
    &emit_native_thumb_method_table,
    &emit_native_xtensa_method_table,
    &emit_native_xtensawin_method_table,
};

#elif MICROPY_EMIT_NATIVE

#if MICROPY_EMIT_X64
#define NATIVE_EMITTER(f) emit_native_x64_##f
#elif MICROPY_EMIT_X86
#define NATIVE_EMITTER(f) emit_native_x86_##f
#elif MICROPY_EMIT_THUMB
#define NATIVE_EMITTER(f) emit_native_thumb_##f
#elif MICROPY_EMIT_ARM
#define NATIVE_EMITTER(f) emit_native_arm_##f
#elif MICROPY_EMIT_XTENSA
#define NATIVE_EMITTER(f) emit_native_xtensa_##f
#elif MICROPY_EMIT_XTENSAWIN
#define NATIVE_EMITTER(f) emit_native_xtensawin_##f
#else
#error "unknown native emitter"
#endif
#define NATIVE_EMITTER_TABLE (&NATIVE_EMITTER(method_table))
#endif

#if MICROPY_EMIT_INLINE_ASM && MICROPY_DYNAMIC_COMPILER

#define ASM_EMITTER(f) emit_asm_table[mp_dynamic_compiler.native_arch]->asm_##f
#define ASM_EMITTER_TABLE emit_asm_table[mp_dynamic_compiler.native_arch]

static const emit_inline_asm_method_table_t *emit_asm_table[] = {
    NULL,
    NULL,
    NULL,
    &emit_inline_thumb_method_table,
    &emit_inline_thumb_method_table,
    &emit_inline_thumb_method_table,
    &emit_inline_thumb_method_table,
    &emit_inline_thumb_method_table,
    &emit_inline_thumb_method_table,
    &emit_inline_xtensa_method_table,
    NULL,
};

#elif MICROPY_EMIT_INLINE_ASM

#if MICROPY_EMIT_INLINE_THUMB
#define ASM_DECORATOR_QSTR MP_QSTR_asm_thumb
#define ASM_EMITTER(f) emit_inline_thumb_##f
#elif MICROPY_EMIT_INLINE_XTENSA
#define ASM_DECORATOR_QSTR MP_QSTR_asm_xtensa
#define ASM_EMITTER(f) emit_inline_xtensa_##f
#else
#error "unknown asm emitter"
#endif
#define ASM_EMITTER_TABLE &ASM_EMITTER(method_table)
#endif

#define EMIT_INLINE_ASM(fun) (comp->emit_inline_asm_method_table->fun(comp->emit_inline_asm))
#define EMIT_INLINE_ASM_ARG(fun, ...) (comp->emit_inline_asm_method_table->fun(comp->emit_inline_asm, __VA_ARGS__))


typedef struct _compiler_t {
    uint8_t is_repl;
    uint8_t pass; 
    uint8_t have_star;

    
    mp_obj_t compile_error; 
    size_t compile_error_line; 

    uint next_label;

    uint16_t num_dict_params;
    uint16_t num_default_params;

    uint16_t break_label; 
    uint16_t continue_label;
    uint16_t cur_except_level; 
    uint16_t break_continue_except_level;

    scope_t *scope_head;
    scope_t *scope_cur;

    emit_t *emit;                                   
    #if NEED_METHOD_TABLE
    const emit_method_table_t *emit_method_table;   
    #endif

    #if MICROPY_EMIT_INLINE_ASM
    emit_inline_asm_t *emit_inline_asm;                                   
    const emit_inline_asm_method_table_t *emit_inline_asm_method_table;   
    #endif

    mp_emit_common_t emit_common;
} compiler_t;

#if MICROPY_COMP_ALLOW_TOP_LEVEL_AWAIT
bool mp_compile_allow_top_level_await = false;
#endif

 



static void mp_emit_common_init(mp_emit_common_t *emit, qstr source_file) {
    #if MICROPY_EMIT_BYTECODE_USES_QSTR_TABLE
    mp_map_init(&emit->qstr_map, 1);

    
    mp_map_elem_t *elem = mp_map_lookup(&emit->qstr_map, MP_OBJ_NEW_QSTR(source_file), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
    elem->value = MP_OBJ_NEW_SMALL_INT(0);
    #endif
    mp_obj_list_init(&emit->const_obj_list, 0);
}

static void mp_emit_common_start_pass(mp_emit_common_t *emit, pass_kind_t pass) {
    emit->pass = pass;
    if (pass == MP_PASS_CODE_SIZE) {
        if (emit->ct_cur_child == 0) {
            emit->children = NULL;
        } else {
            emit->children = m_new0(mp_raw_code_t *, emit->ct_cur_child);
        }
    }
    emit->ct_cur_child = 0;
}

static void mp_emit_common_populate_module_context(mp_emit_common_t *emit, qstr source_file, mp_module_context_t *context) {
    #if MICROPY_EMIT_BYTECODE_USES_QSTR_TABLE
    size_t qstr_map_used = emit->qstr_map.used;
    mp_module_context_alloc_tables(context, qstr_map_used, emit->const_obj_list.len);
    for (size_t i = 0; i < emit->qstr_map.alloc; ++i) {
        if (mp_map_slot_is_filled(&emit->qstr_map, i)) {
            size_t idx = MP_OBJ_SMALL_INT_VALUE(emit->qstr_map.table[i].value);
            qstr qst = MP_OBJ_QSTR_VALUE(emit->qstr_map.table[i].key);
            context->constants.qstr_table[idx] = qst;
        }
    }
    #else
    mp_module_context_alloc_tables(context, 0, emit->const_obj_list.len);
    context->constants.source_file = source_file;
    #endif

    for (size_t i = 0; i < emit->const_obj_list.len; ++i) {
        context->constants.obj_table[i] = emit->const_obj_list.items[i];
    }
}

 

static void compile_error_set_line(compiler_t *comp, mp_parse_node_t pn) {
    
    if (comp->compile_error_line == 0 && MP_PARSE_NODE_IS_STRUCT(pn)) {
        comp->compile_error_line = ((mp_parse_node_struct_t *)pn)->source_line;
    }
}

static void compile_syntax_error(compiler_t *comp, mp_parse_node_t pn, mp_rom_error_text_t msg) {
    
    if (comp->compile_error == MP_OBJ_NULL) {
        comp->compile_error = mp_obj_new_exception_msg(&mp_type_SyntaxError, msg);
        compile_error_set_line(comp, pn);
    }
}

static void compile_trailer_paren_helper(compiler_t *comp, mp_parse_node_t pn_arglist, bool is_method_call, int n_positional_extra);
static void compile_comprehension(compiler_t *comp, mp_parse_node_struct_t *pns, scope_kind_t kind);
static void compile_atom_brace_helper(compiler_t *comp, mp_parse_node_struct_t *pns, bool create_map);
static void compile_node(compiler_t *comp, mp_parse_node_t pn);

static uint comp_next_label(compiler_t *comp) {
    return comp->next_label++;
}

#if MICROPY_EMIT_NATIVE
static void reserve_labels_for_native(compiler_t *comp, int n) {
    if (comp->scope_cur->emit_options != MP_EMIT_OPT_BYTECODE) {
        comp->next_label += n;
    }
}
#else
#define reserve_labels_for_native(comp, n)
#endif

static void compile_increase_except_level(compiler_t *comp, uint label, int kind) {
    EMIT_ARG(setup_block, label, kind);
    comp->cur_except_level += 1;
    if (comp->cur_except_level > comp->scope_cur->exc_stack_size) {
        comp->scope_cur->exc_stack_size = comp->cur_except_level;
    }
}

static void compile_decrease_except_level(compiler_t *comp) {
    assert(comp->cur_except_level > 0);
    comp->cur_except_level -= 1;
    EMIT(end_finally);
    reserve_labels_for_native(comp, 1);
}

static scope_t *scope_new_and_link(compiler_t *comp, scope_kind_t kind, mp_parse_node_t pn, uint emit_options) {
    scope_t *scope = scope_new(kind, pn, emit_options);
    scope->parent = comp->scope_cur;
    scope->next = NULL;
    if (comp->scope_head == NULL) {
        comp->scope_head = scope;
    } else {
        scope_t *s = comp->scope_head;
        while (s->next != NULL) {
            s = s->next;
        }
        s->next = scope;
    }
    return scope;
}

typedef void (*apply_list_fun_t)(compiler_t *comp, mp_parse_node_t pn);

static void apply_to_single_or_list(compiler_t *comp, mp_parse_node_t pn, pn_kind_t pn_list_kind, apply_list_fun_t f) {
    if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, pn_list_kind)) {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        int num_nodes = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
        for (int i = 0; i < num_nodes; i++) {
            f(comp, pns->nodes[i]);
        }
    } else if (!MP_PARSE_NODE_IS_NULL(pn)) {
        f(comp, pn);
    }
}

static void compile_generic_all_nodes(compiler_t *comp, mp_parse_node_struct_t *pns) {
    int num_nodes = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
    for (int i = 0; i < num_nodes; i++) {
        compile_node(comp, pns->nodes[i]);
        if (comp->compile_error != MP_OBJ_NULL) {
            
            compile_error_set_line(comp, pns->nodes[i]);
            return;
        }
    }
}

static void compile_load_id(compiler_t *comp, qstr qst) {
    if (comp->pass == MP_PASS_SCOPE) {
        mp_emit_common_get_id_for_load(comp->scope_cur, qst);
    } else {
        #if NEED_METHOD_TABLE
        mp_emit_common_id_op(comp->emit, &comp->emit_method_table->load_id, comp->scope_cur, qst);
        #else
        mp_emit_common_id_op(comp->emit, &mp_emit_bc_method_table_load_id_ops, comp->scope_cur, qst);
        #endif
    }
}

static void compile_store_id(compiler_t *comp, qstr qst) {
    if (comp->pass == MP_PASS_SCOPE) {
        mp_emit_common_get_id_for_modification(comp->scope_cur, qst);
    } else {
        #if NEED_METHOD_TABLE
        mp_emit_common_id_op(comp->emit, &comp->emit_method_table->store_id, comp->scope_cur, qst);
        #else
        mp_emit_common_id_op(comp->emit, &mp_emit_bc_method_table_store_id_ops, comp->scope_cur, qst);
        #endif
    }
}

static void compile_delete_id(compiler_t *comp, qstr qst) {
    if (comp->pass == MP_PASS_SCOPE) {
        mp_emit_common_get_id_for_modification(comp->scope_cur, qst);
    } else {
        #if NEED_METHOD_TABLE
        mp_emit_common_id_op(comp->emit, &comp->emit_method_table->delete_id, comp->scope_cur, qst);
        #else
        mp_emit_common_id_op(comp->emit, &mp_emit_bc_method_table_delete_id_ops, comp->scope_cur, qst);
        #endif
    }
}

static void compile_generic_tuple(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    size_t num_nodes = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
    for (size_t i = 0; i < num_nodes; i++) {
        compile_node(comp, pns->nodes[i]);
    }
    EMIT_ARG(build, num_nodes, MP_EMIT_BUILD_TUPLE);
}

static void c_if_cond(compiler_t *comp, mp_parse_node_t pn, bool jump_if, int label) {
    if (mp_parse_node_is_const_false(pn)) {
        if (jump_if == false) {
            EMIT_ARG(jump, label);
        }
        return;
    } else if (mp_parse_node_is_const_true(pn)) {
        if (jump_if == true) {
            EMIT_ARG(jump, label);
        }
        return;
    } else if (MP_PARSE_NODE_IS_STRUCT(pn)) {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        int n = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
        if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_or_test) {
            if (jump_if == false) {
            and_or_logic1:;
                uint label2 = comp_next_label(comp);
                for (int i = 0; i < n - 1; i++) {
                    c_if_cond(comp, pns->nodes[i], !jump_if, label2);
                }
                c_if_cond(comp, pns->nodes[n - 1], jump_if, label);
                EMIT_ARG(label_assign, label2);
            } else {
            and_or_logic2:
                for (int i = 0; i < n; i++) {
                    c_if_cond(comp, pns->nodes[i], jump_if, label);
                }
            }
            return;
        } else if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_and_test) {
            if (jump_if == false) {
                goto and_or_logic2;
            } else {
                goto and_or_logic1;
            }
        } else if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_not_test_2) {
            c_if_cond(comp, pns->nodes[0], !jump_if, label);
            return;
        }
    }

    
    compile_node(comp, pn);
    EMIT_ARG(pop_jump_if, jump_if, label);
}

typedef enum { ASSIGN_STORE, ASSIGN_AUG_LOAD, ASSIGN_AUG_STORE } assign_kind_t;
static void c_assign(compiler_t *comp, mp_parse_node_t pn, assign_kind_t kind);

static void c_assign_atom_expr(compiler_t *comp, mp_parse_node_struct_t *pns, assign_kind_t assign_kind) {
    if (assign_kind != ASSIGN_AUG_STORE) {
        compile_node(comp, pns->nodes[0]);
    }

    if (MP_PARSE_NODE_IS_STRUCT(pns->nodes[1])) {
        mp_parse_node_struct_t *pns1 = (mp_parse_node_struct_t *)pns->nodes[1];
        if (MP_PARSE_NODE_STRUCT_KIND(pns1) == PN_atom_expr_trailers) {
            int n = MP_PARSE_NODE_STRUCT_NUM_NODES(pns1);
            if (assign_kind != ASSIGN_AUG_STORE) {
                for (int i = 0; i < n - 1; i++) {
                    compile_node(comp, pns1->nodes[i]);
                }
            }
            assert(MP_PARSE_NODE_IS_STRUCT(pns1->nodes[n - 1]));
            pns1 = (mp_parse_node_struct_t *)pns1->nodes[n - 1];
        }
        if (MP_PARSE_NODE_STRUCT_KIND(pns1) == PN_trailer_bracket) {
            if (assign_kind == ASSIGN_AUG_STORE) {
                EMIT(rot_three);
                EMIT_ARG(subscr, MP_EMIT_SUBSCR_STORE);
            } else {
                compile_node(comp, pns1->nodes[0]);
                if (assign_kind == ASSIGN_AUG_LOAD) {
                    EMIT(dup_top_two);
                    EMIT_ARG(subscr, MP_EMIT_SUBSCR_LOAD);
                } else {
                    EMIT_ARG(subscr, MP_EMIT_SUBSCR_STORE);
                }
            }
            return;
        } else if (MP_PARSE_NODE_STRUCT_KIND(pns1) == PN_trailer_period) {
            assert(MP_PARSE_NODE_IS_ID(pns1->nodes[0]));
            if (assign_kind == ASSIGN_AUG_LOAD) {
                EMIT(dup_top);
                EMIT_ARG(attr, MP_PARSE_NODE_LEAF_ARG(pns1->nodes[0]), MP_EMIT_ATTR_LOAD);
            } else {
                if (assign_kind == ASSIGN_AUG_STORE) {
                    EMIT(rot_two);
                }
                EMIT_ARG(attr, MP_PARSE_NODE_LEAF_ARG(pns1->nodes[0]), MP_EMIT_ATTR_STORE);
            }
            return;
        }
    }

    compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("can't assign to expression"));
}

static void c_assign_tuple(compiler_t *comp, uint num_tail, mp_parse_node_t *nodes_tail) {
    
    uint have_star_index = -1;
    for (uint i = 0; i < num_tail; i++) {
        if (MP_PARSE_NODE_IS_STRUCT_KIND(nodes_tail[i], PN_star_expr)) {
            if (have_star_index == (uint)-1) {
                EMIT_ARG(unpack_ex, i, num_tail - i - 1);
                have_star_index = i;
            } else {
                compile_syntax_error(comp, nodes_tail[i], MP_ERROR_TEXT("multiple *x in assignment"));
                return;
            }
        }
    }
    if (have_star_index == (uint)-1) {
        EMIT_ARG(unpack_sequence, num_tail);
    }
    for (uint i = 0; i < num_tail; i++) {
        if (i == have_star_index) {
            c_assign(comp, ((mp_parse_node_struct_t *)nodes_tail[i])->nodes[0], ASSIGN_STORE);
        } else {
            c_assign(comp, nodes_tail[i], ASSIGN_STORE);
        }
    }
}


static void c_assign(compiler_t *comp, mp_parse_node_t pn, assign_kind_t assign_kind) {
    assert(!MP_PARSE_NODE_IS_NULL(pn));
    if (MP_PARSE_NODE_IS_LEAF(pn)) {
        if (MP_PARSE_NODE_IS_ID(pn)) {
            qstr arg = MP_PARSE_NODE_LEAF_ARG(pn);
            switch (assign_kind) {
                case ASSIGN_STORE:
                case ASSIGN_AUG_STORE:
                    compile_store_id(comp, arg);
                    break;
                case ASSIGN_AUG_LOAD:
                default:
                    compile_load_id(comp, arg);
                    break;
            }
        } else {
            goto cannot_assign;
        }
    } else {
        
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        switch (MP_PARSE_NODE_STRUCT_KIND(pns)) {
            case PN_atom_expr_normal:
                
                c_assign_atom_expr(comp, pns, assign_kind);
                break;

            case PN_testlist_star_expr:
            case PN_exprlist:
                
                if (assign_kind != ASSIGN_STORE) {
                    goto cannot_assign;
                }
                c_assign_tuple(comp, MP_PARSE_NODE_STRUCT_NUM_NODES(pns), pns->nodes);
                break;

            case PN_atom_paren:
                
                if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
                    
                    goto cannot_assign;
                } else {
                    assert(MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_testlist_comp));
                    if (assign_kind != ASSIGN_STORE) {
                        goto cannot_assign;
                    }
                    pns = (mp_parse_node_struct_t *)pns->nodes[0];
                    goto testlist_comp;
                }
                break;

            case PN_atom_bracket:
                
                if (assign_kind != ASSIGN_STORE) {
                    goto cannot_assign;
                }
                if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
                    
                    c_assign_tuple(comp, 0, NULL);
                } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_testlist_comp)) {
                    pns = (mp_parse_node_struct_t *)pns->nodes[0];
                    goto testlist_comp;
                } else {
                    
                    c_assign_tuple(comp, 1, pns->nodes);
                }
                break;

            default:
                goto cannot_assign;
        }
        return;

    testlist_comp:
        
        if (MP_PARSE_NODE_TESTLIST_COMP_HAS_COMP_FOR(pns)) {
            goto cannot_assign;
        }
        c_assign_tuple(comp, MP_PARSE_NODE_STRUCT_NUM_NODES(pns), pns->nodes);
        return;
    }
    return;

cannot_assign:
    compile_syntax_error(comp, pn, MP_ERROR_TEXT("can't assign to expression"));
}





static void close_over_variables_etc(compiler_t *comp, scope_t *this_scope, int n_pos_defaults, int n_kw_defaults) {
    assert(n_pos_defaults >= 0);
    assert(n_kw_defaults >= 0);

    
    if (n_kw_defaults > 0) {
        this_scope->scope_flags |= MP_SCOPE_FLAG_DEFKWARGS;
    }
    this_scope->num_def_pos_args = n_pos_defaults;

    #if MICROPY_EMIT_NATIVE
    
    comp->scope_cur->scope_flags |= MP_SCOPE_FLAG_REFGLOBALS | MP_SCOPE_FLAG_HASCONSTS;
    #endif

    
    
    int nfree = 0;
    if (comp->scope_cur->kind != SCOPE_MODULE) {
        for (int i = 0; i < comp->scope_cur->id_info_len; i++) {
            id_info_t *id = &comp->scope_cur->id_info[i];
            if (id->kind == ID_INFO_KIND_CELL || id->kind == ID_INFO_KIND_FREE) {
                for (int j = 0; j < this_scope->id_info_len; j++) {
                    id_info_t *id2 = &this_scope->id_info[j];
                    if (id2->kind == ID_INFO_KIND_FREE && id->qst == id2->qst) {
                        
                        EMIT_LOAD_FAST(id->qst, id->local_num);
                        nfree += 1;
                    }
                }
            }
        }
    }

    
    if (nfree == 0) {
        EMIT_ARG(make_function, this_scope, n_pos_defaults, n_kw_defaults);
    } else {
        EMIT_ARG(make_closure, this_scope, nfree, n_pos_defaults, n_kw_defaults);
    }
}

static void compile_funcdef_lambdef_param(compiler_t *comp, mp_parse_node_t pn) {
    
    int pn_kind;
    if (MP_PARSE_NODE_IS_ID(pn)) {
        pn_kind = -1;
    } else {
        assert(MP_PARSE_NODE_IS_STRUCT(pn));
        pn_kind = MP_PARSE_NODE_STRUCT_KIND((mp_parse_node_struct_t *)pn);
    }

    if (pn_kind == PN_typedargslist_star || pn_kind == PN_varargslist_star) {
        comp->have_star = true;
         

    } else if (pn_kind == PN_typedargslist_dbl_star || pn_kind == PN_varargslist_dbl_star) {
         
         

    } else {
        mp_parse_node_t pn_id;
        mp_parse_node_t pn_equal;
        if (pn_kind == -1) {
             

            pn_id = pn;
            pn_equal = MP_PARSE_NODE_NULL;

        } else if (pn_kind == PN_typedargslist_name) {
             

            mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
            pn_id = pns->nodes[0];
             
            pn_equal = pns->nodes[2];

        } else {
            assert(pn_kind == PN_varargslist_name);  
             

            mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
            pn_id = pns->nodes[0];
            pn_equal = pns->nodes[1];
        }

        if (MP_PARSE_NODE_IS_NULL(pn_equal)) {
             

             
            if (!comp->have_star && comp->num_default_params != 0) {
                compile_syntax_error(comp, pn, MP_ERROR_TEXT("non-default argument follows default argument"));
                return;
            }

        } else {
             
             

            if (comp->have_star) {
                comp->num_dict_params += 1;
                
                if (comp->num_dict_params == 1) {
                    
                    
                    if (comp->num_default_params > 0) {
                        EMIT_ARG(build, comp->num_default_params, MP_EMIT_BUILD_TUPLE);
                    } else {
                        EMIT(load_null); 
                    }
                    
                    EMIT_ARG(build, 0, MP_EMIT_BUILD_MAP);
                }

                
                compile_node(comp, pn_equal);
                EMIT_ARG(load_const_str, MP_PARSE_NODE_LEAF_ARG(pn_id));
                EMIT(store_map);
            } else {
                comp->num_default_params += 1;
                compile_node(comp, pn_equal);
            }
        }
    }
}

static void compile_funcdef_lambdef(compiler_t *comp, scope_t *scope, mp_parse_node_t pn_params, pn_kind_t pn_list_kind) {
    
    
    
    bool orig_have_star = comp->have_star;
    uint16_t orig_num_dict_params = comp->num_dict_params;
    uint16_t orig_num_default_params = comp->num_default_params;

    
    comp->have_star = false;
    comp->num_dict_params = 0;
    comp->num_default_params = 0;
    apply_to_single_or_list(comp, pn_params, pn_list_kind, compile_funcdef_lambdef_param);

    if (comp->compile_error != MP_OBJ_NULL) {
        return;
    }

    
    
    if (comp->num_default_params > 0 && comp->num_dict_params == 0) {
        EMIT_ARG(build, comp->num_default_params, MP_EMIT_BUILD_TUPLE);
        EMIT(load_null); 
    }

    
    close_over_variables_etc(comp, scope, comp->num_default_params, comp->num_dict_params);

    
    comp->have_star = orig_have_star;
    comp->num_dict_params = orig_num_dict_params;
    comp->num_default_params = orig_num_default_params;
}



static qstr compile_funcdef_helper(compiler_t *comp, mp_parse_node_struct_t *pns, uint emit_options) {
    if (comp->pass == MP_PASS_SCOPE) {
        
        scope_t *s = scope_new_and_link(comp, SCOPE_FUNCTION, (mp_parse_node_t)pns, emit_options);
        
        pns->nodes[4] = (mp_parse_node_t)s;
    }

    
    scope_t *fscope = (scope_t *)pns->nodes[4];

    
    compile_funcdef_lambdef(comp, fscope, pns->nodes[1], PN_typedargslist);

    
    return fscope->simple_name;
}



static qstr compile_classdef_helper(compiler_t *comp, mp_parse_node_struct_t *pns, uint emit_options) {
    if (comp->pass == MP_PASS_SCOPE) {
        
        scope_t *s = scope_new_and_link(comp, SCOPE_CLASS, (mp_parse_node_t)pns, emit_options);
        
        pns->nodes[3] = (mp_parse_node_t)s;
    }

    EMIT(load_build_class);

    
    scope_t *cscope = (scope_t *)pns->nodes[3];

    
    close_over_variables_etc(comp, cscope, 0, 0);

    
    EMIT_ARG(load_const_str, cscope->simple_name);

    
    
    mp_parse_node_t parents = pns->nodes[1];
    if (MP_PARSE_NODE_IS_STRUCT_KIND(parents, PN_classdef_2)) {
        parents = MP_PARSE_NODE_NULL;
    }
    compile_trailer_paren_helper(comp, parents, false, 2);

    
    return cscope->simple_name;
}


static bool compile_built_in_decorator(compiler_t *comp, size_t name_len, mp_parse_node_t *name_nodes, uint *emit_options) {
    if (MP_PARSE_NODE_LEAF_ARG(name_nodes[0]) != MP_QSTR_micropython) {
        return false;
    }

    if (name_len != 2) {
        compile_syntax_error(comp, name_nodes[0], MP_ERROR_TEXT("invalid micropython decorator"));
        return true;
    }

    qstr attr = MP_PARSE_NODE_LEAF_ARG(name_nodes[1]);
    if (attr == MP_QSTR_bytecode) {
        *emit_options = MP_EMIT_OPT_BYTECODE;
    #if MICROPY_EMIT_NATIVE
    } else if (attr == MP_QSTR_native) {
        *emit_options = MP_EMIT_OPT_NATIVE_PYTHON;
    } else if (attr == MP_QSTR_viper) {
        *emit_options = MP_EMIT_OPT_VIPER;
    #endif
        #if MICROPY_EMIT_INLINE_ASM
    #if MICROPY_DYNAMIC_COMPILER
    } else if (attr == MP_QSTR_asm_thumb) {
        *emit_options = MP_EMIT_OPT_ASM;
    } else if (attr == MP_QSTR_asm_xtensa) {
        *emit_options = MP_EMIT_OPT_ASM;
    #else
    } else if (attr == ASM_DECORATOR_QSTR) {
        *emit_options = MP_EMIT_OPT_ASM;
    #endif
        #endif
    } else {
        compile_syntax_error(comp, name_nodes[1], MP_ERROR_TEXT("invalid micropython decorator"));
    }

    #if MICROPY_EMIT_NATIVE && MICROPY_DYNAMIC_COMPILER
    if (*emit_options == MP_EMIT_OPT_NATIVE_PYTHON || *emit_options == MP_EMIT_OPT_VIPER) {
        if (emit_native_table[mp_dynamic_compiler.native_arch] == NULL) {
            compile_syntax_error(comp, name_nodes[1], MP_ERROR_TEXT("invalid arch"));
        }
    } else if (*emit_options == MP_EMIT_OPT_ASM) {
        if (emit_asm_table[mp_dynamic_compiler.native_arch] == NULL) {
            compile_syntax_error(comp, name_nodes[1], MP_ERROR_TEXT("invalid arch"));
        }
    }
    #endif

    return true;
}

static void compile_decorated(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    mp_parse_node_t *nodes;
    size_t n = mp_parse_node_extract_list(&pns->nodes[0], PN_decorators, &nodes);

    
    uint emit_options = comp->scope_cur->emit_options;

    
    size_t num_built_in_decorators = 0;
    for (size_t i = 0; i < n; i++) {
        assert(MP_PARSE_NODE_IS_STRUCT_KIND(nodes[i], PN_decorator)); 
        mp_parse_node_struct_t *pns_decorator = (mp_parse_node_struct_t *)nodes[i];

        
        mp_parse_node_t *name_nodes;
        size_t name_len = mp_parse_node_extract_list(&pns_decorator->nodes[0], PN_dotted_name, &name_nodes);

        
        if (compile_built_in_decorator(comp, name_len, name_nodes, &emit_options)) {
            
            num_built_in_decorators += 1;

        } else {
            

            
            compile_node(comp, name_nodes[0]);
            for (size_t j = 1; j < name_len; j++) {
                assert(MP_PARSE_NODE_IS_ID(name_nodes[j])); 
                EMIT_ARG(attr, MP_PARSE_NODE_LEAF_ARG(name_nodes[j]), MP_EMIT_ATTR_LOAD);
            }

            
            if (!MP_PARSE_NODE_IS_NULL(pns_decorator->nodes[1])) {
                
                compile_node(comp, pns_decorator->nodes[1]);
            }
        }
    }

    
    mp_parse_node_struct_t *pns_body = (mp_parse_node_struct_t *)pns->nodes[1];
    qstr body_name = 0;
    if (MP_PARSE_NODE_STRUCT_KIND(pns_body) == PN_funcdef) {
        body_name = compile_funcdef_helper(comp, pns_body, emit_options);
    #if MICROPY_PY_ASYNC_AWAIT
    } else if (MP_PARSE_NODE_STRUCT_KIND(pns_body) == PN_async_funcdef) {
        assert(MP_PARSE_NODE_IS_STRUCT(pns_body->nodes[0]));
        mp_parse_node_struct_t *pns0 = (mp_parse_node_struct_t *)pns_body->nodes[0];
        body_name = compile_funcdef_helper(comp, pns0, emit_options);
        scope_t *fscope = (scope_t *)pns0->nodes[4];
        fscope->scope_flags |= MP_SCOPE_FLAG_GENERATOR;
    #endif
    } else {
        assert(MP_PARSE_NODE_STRUCT_KIND(pns_body) == PN_classdef); 
        body_name = compile_classdef_helper(comp, pns_body, emit_options);
    }

    
    for (size_t i = 0; i < n - num_built_in_decorators; i++) {
        EMIT_ARG(call_function, 1, 0, 0);
    }

    
    compile_store_id(comp, body_name);
}

static void compile_funcdef(compiler_t *comp, mp_parse_node_struct_t *pns) {
    qstr fname = compile_funcdef_helper(comp, pns, comp->scope_cur->emit_options);
    
    compile_store_id(comp, fname);
}

static void c_del_stmt(compiler_t *comp, mp_parse_node_t pn) {
    if (MP_PARSE_NODE_IS_ID(pn)) {
        compile_delete_id(comp, MP_PARSE_NODE_LEAF_ARG(pn));
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_atom_expr_normal)) {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;

        compile_node(comp, pns->nodes[0]); 

        if (MP_PARSE_NODE_IS_STRUCT(pns->nodes[1])) {
            mp_parse_node_struct_t *pns1 = (mp_parse_node_struct_t *)pns->nodes[1];
            if (MP_PARSE_NODE_STRUCT_KIND(pns1) == PN_atom_expr_trailers) {
                int n = MP_PARSE_NODE_STRUCT_NUM_NODES(pns1);
                for (int i = 0; i < n - 1; i++) {
                    compile_node(comp, pns1->nodes[i]);
                }
                assert(MP_PARSE_NODE_IS_STRUCT(pns1->nodes[n - 1]));
                pns1 = (mp_parse_node_struct_t *)pns1->nodes[n - 1];
            }
            if (MP_PARSE_NODE_STRUCT_KIND(pns1) == PN_trailer_bracket) {
                compile_node(comp, pns1->nodes[0]);
                EMIT_ARG(subscr, MP_EMIT_SUBSCR_DELETE);
            } else if (MP_PARSE_NODE_STRUCT_KIND(pns1) == PN_trailer_period) {
                assert(MP_PARSE_NODE_IS_ID(pns1->nodes[0]));
                EMIT_ARG(attr, MP_PARSE_NODE_LEAF_ARG(pns1->nodes[0]), MP_EMIT_ATTR_DELETE);
            } else {
                goto cannot_delete;
            }
        } else {
            goto cannot_delete;
        }

    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_atom_paren)) {
        pn = ((mp_parse_node_struct_t *)pn)->nodes[0];
        if (MP_PARSE_NODE_IS_NULL(pn)) {
            goto cannot_delete;
        } else {
            assert(MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_testlist_comp));
            mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
            if (MP_PARSE_NODE_TESTLIST_COMP_HAS_COMP_FOR(pns)) {
                goto cannot_delete;
            }
            for (size_t i = 0; i < MP_PARSE_NODE_STRUCT_NUM_NODES(pns); ++i) {
                c_del_stmt(comp, pns->nodes[i]);
            }
        }
    } else {
        
        goto cannot_delete;
    }

    return;

cannot_delete:
    compile_syntax_error(comp, (mp_parse_node_t)pn, MP_ERROR_TEXT("can't delete expression"));
}

static void compile_del_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    apply_to_single_or_list(comp, pns->nodes[0], PN_exprlist, c_del_stmt);
}

static void compile_break_cont_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    uint16_t label;
    if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_break_stmt) {
        label = comp->break_label;
    } else {
        label = comp->continue_label;
    }
    if (label == INVALID_LABEL) {
        compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("'break'/'continue' outside loop"));
    }
    assert(comp->cur_except_level >= comp->break_continue_except_level);
    EMIT_ARG(unwind_jump, label, comp->cur_except_level - comp->break_continue_except_level);
}

static void compile_return_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    #if MICROPY_CPYTHON_COMPAT
    if (comp->scope_cur->kind != SCOPE_FUNCTION) {
        compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("'return' outside function"));
        return;
    }
    #endif
    if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
        
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
    } else if (MICROPY_COMP_RETURN_IF_EXPR
               && MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_test_if_expr)) {
        
        mp_parse_node_struct_t *pns_test_if_expr = (mp_parse_node_struct_t *)pns->nodes[0];
        mp_parse_node_struct_t *pns_test_if_else = (mp_parse_node_struct_t *)pns_test_if_expr->nodes[1];

        uint l_fail = comp_next_label(comp);
        c_if_cond(comp, pns_test_if_else->nodes[0], false, l_fail); 
        compile_node(comp, pns_test_if_expr->nodes[0]); 
        EMIT(return_value);
        EMIT_ARG(label_assign, l_fail);
        compile_node(comp, pns_test_if_else->nodes[1]); 
    } else {
        compile_node(comp, pns->nodes[0]);
    }
    EMIT(return_value);
}

static void compile_yield_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    compile_node(comp, pns->nodes[0]);
    EMIT(pop_top);
}

static void compile_raise_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
        
        EMIT_ARG(raise_varargs, 0);
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_raise_stmt_arg)) {
        
        pns = (mp_parse_node_struct_t *)pns->nodes[0];
        compile_node(comp, pns->nodes[0]);
        compile_node(comp, pns->nodes[1]);
        EMIT_ARG(raise_varargs, 2);
    } else {
        
        compile_node(comp, pns->nodes[0]);
        EMIT_ARG(raise_varargs, 1);
    }
}




static void do_import_name(compiler_t *comp, mp_parse_node_t pn, qstr *q_base) {
    bool is_as = false;
    if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_dotted_as_name)) {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        
        *q_base = MP_PARSE_NODE_LEAF_ARG(pns->nodes[1]);
        pn = pns->nodes[0];
        is_as = true;
    }
    if (MP_PARSE_NODE_IS_NULL(pn)) {
        
        *q_base = MP_QSTR_;
        EMIT_ARG(import, MP_QSTR_, MP_EMIT_IMPORT_NAME); 
    } else if (MP_PARSE_NODE_IS_ID(pn)) {
        
        qstr q_full = MP_PARSE_NODE_LEAF_ARG(pn);
        if (!is_as) {
            *q_base = q_full;
        }
        EMIT_ARG(import, q_full, MP_EMIT_IMPORT_NAME);
    } else {
        assert(MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_dotted_name)); 
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        {
            
            if (!is_as) {
                *q_base = MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]);
            }
            size_t n = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
            if (n == 0) {
                
                
                MP_UNREACHABLE;
            }
            size_t len = n - 1;
            for (size_t i = 0; i < n; i++) {
                len += qstr_len(MP_PARSE_NODE_LEAF_ARG(pns->nodes[i]));
            }
            char *q_ptr = mp_local_alloc(len);
            char *str_dest = q_ptr;
            for (size_t i = 0; i < n; i++) {
                if (i > 0) {
                    *str_dest++ = '.';
                }
                size_t str_src_len;
                const byte *str_src = qstr_data(MP_PARSE_NODE_LEAF_ARG(pns->nodes[i]), &str_src_len);
                memcpy(str_dest, str_src, str_src_len);
                str_dest += str_src_len;
            }
            qstr q_full = qstr_from_strn(q_ptr, len);
            mp_local_free(q_ptr);
            EMIT_ARG(import, q_full, MP_EMIT_IMPORT_NAME);
            if (is_as) {
                for (size_t i = 1; i < n; i++) {
                    EMIT_ARG(attr, MP_PARSE_NODE_LEAF_ARG(pns->nodes[i]), MP_EMIT_ATTR_LOAD);
                }
            }
        }
    }
}

static void compile_dotted_as_name(compiler_t *comp, mp_parse_node_t pn) {
    EMIT_ARG(load_const_small_int, 0); 
    EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE); 
    qstr q_base;
    do_import_name(comp, pn, &q_base);
    compile_store_id(comp, q_base);
}

static void compile_import_name(compiler_t *comp, mp_parse_node_struct_t *pns) {
    apply_to_single_or_list(comp, pns->nodes[0], PN_dotted_as_names, compile_dotted_as_name);
}

static void compile_import_from(compiler_t *comp, mp_parse_node_struct_t *pns) {
    mp_parse_node_t pn_import_source = pns->nodes[0];

    
    uint import_level = 0;
    do {
        mp_parse_node_t pn_rel;
        if (MP_PARSE_NODE_IS_TOKEN(pn_import_source) || MP_PARSE_NODE_IS_STRUCT_KIND(pn_import_source, PN_one_or_more_period_or_ellipsis)) {
            
            pn_rel = pn_import_source;
            pn_import_source = MP_PARSE_NODE_NULL;
        } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn_import_source, PN_import_from_2b)) {
            
            mp_parse_node_struct_t *pns_2b = (mp_parse_node_struct_t *)pn_import_source;
            pn_rel = pns_2b->nodes[0];
            pn_import_source = pns_2b->nodes[1];
            assert(!MP_PARSE_NODE_IS_NULL(pn_import_source)); 
        } else {
            
            break;
        }

        
        mp_parse_node_t *nodes;
        size_t n = mp_parse_node_extract_list(&pn_rel, PN_one_or_more_period_or_ellipsis, &nodes);

        
        for (size_t i = 0; i < n; i++) {
            if (MP_PARSE_NODE_IS_TOKEN_KIND(nodes[i], MP_TOKEN_DEL_PERIOD)) {
                import_level++;
            } else {
                
                import_level += 3;
            }
        }
    } while (0);

    if (MP_PARSE_NODE_IS_TOKEN_KIND(pns->nodes[1], MP_TOKEN_OP_STAR)) {
        #if MICROPY_CPYTHON_COMPAT
        if (comp->scope_cur->kind != SCOPE_MODULE) {
            compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("import * not at module level"));
            return;
        }
        #endif

        EMIT_ARG(load_const_small_int, import_level);

        
        EMIT_ARG(load_const_str, MP_QSTR__star_);
        EMIT_ARG(build, 1, MP_EMIT_BUILD_TUPLE);

        
        qstr dummy_q;
        do_import_name(comp, pn_import_source, &dummy_q);
        EMIT_ARG(import, MP_QSTRnull, MP_EMIT_IMPORT_STAR);

    } else {
        EMIT_ARG(load_const_small_int, import_level);

        
        mp_parse_node_t *pn_nodes;
        size_t n = mp_parse_node_extract_list(&pns->nodes[1], PN_import_as_names, &pn_nodes);
        for (size_t i = 0; i < n; i++) {
            assert(MP_PARSE_NODE_IS_STRUCT_KIND(pn_nodes[i], PN_import_as_name));
            mp_parse_node_struct_t *pns3 = (mp_parse_node_struct_t *)pn_nodes[i];
            qstr id2 = MP_PARSE_NODE_LEAF_ARG(pns3->nodes[0]); 
            EMIT_ARG(load_const_str, id2);
        }
        EMIT_ARG(build, n, MP_EMIT_BUILD_TUPLE);

        
        qstr dummy_q;
        do_import_name(comp, pn_import_source, &dummy_q);
        for (size_t i = 0; i < n; i++) {
            assert(MP_PARSE_NODE_IS_STRUCT_KIND(pn_nodes[i], PN_import_as_name));
            mp_parse_node_struct_t *pns3 = (mp_parse_node_struct_t *)pn_nodes[i];
            qstr id2 = MP_PARSE_NODE_LEAF_ARG(pns3->nodes[0]); 
            EMIT_ARG(import, id2, MP_EMIT_IMPORT_FROM);
            if (MP_PARSE_NODE_IS_NULL(pns3->nodes[1])) {
                compile_store_id(comp, id2);
            } else {
                compile_store_id(comp, MP_PARSE_NODE_LEAF_ARG(pns3->nodes[1]));
            }
        }
        EMIT(pop_top);
    }
}

static void compile_declare_global(compiler_t *comp, mp_parse_node_t pn, id_info_t *id_info) {
    if (id_info->kind != ID_INFO_KIND_UNDECIDED && id_info->kind != ID_INFO_KIND_GLOBAL_EXPLICIT) {
        compile_syntax_error(comp, pn, MP_ERROR_TEXT("identifier redefined as global"));
        return;
    }
    id_info->kind = ID_INFO_KIND_GLOBAL_EXPLICIT;

    
    id_info = scope_find_global(comp->scope_cur, id_info->qst);
    if (id_info != NULL) {
        id_info->kind = ID_INFO_KIND_GLOBAL_EXPLICIT;
    }
}

static void compile_declare_nonlocal(compiler_t *comp, mp_parse_node_t pn, id_info_t *id_info) {
    if (id_info->kind == ID_INFO_KIND_UNDECIDED) {
        id_info->kind = ID_INFO_KIND_GLOBAL_IMPLICIT;
        scope_check_to_close_over(comp->scope_cur, id_info);
        if (id_info->kind == ID_INFO_KIND_GLOBAL_IMPLICIT) {
            compile_syntax_error(comp, pn, MP_ERROR_TEXT("no binding for nonlocal found"));
        }
    } else if (id_info->kind != ID_INFO_KIND_FREE) {
        compile_syntax_error(comp, pn, MP_ERROR_TEXT("identifier redefined as nonlocal"));
    }
}

static void compile_declare_global_or_nonlocal(compiler_t *comp, mp_parse_node_t pn, id_info_t *id_info, bool is_global) {
    if (is_global) {
        compile_declare_global(comp, pn, id_info);
    } else {
        compile_declare_nonlocal(comp, pn, id_info);
    }
}

static void compile_global_nonlocal_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    if (comp->pass == MP_PASS_SCOPE) {
        bool is_global = MP_PARSE_NODE_STRUCT_KIND(pns) == PN_global_stmt;

        if (!is_global && comp->scope_cur->kind == SCOPE_MODULE) {
            compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("can't declare nonlocal in outer code"));
            return;
        }

        mp_parse_node_t *nodes;
        size_t n = mp_parse_node_extract_list(&pns->nodes[0], PN_name_list, &nodes);
        for (size_t i = 0; i < n; i++) {
            qstr qst = MP_PARSE_NODE_LEAF_ARG(nodes[i]);
            id_info_t *id_info = scope_find_or_add_id(comp->scope_cur, qst, ID_INFO_KIND_UNDECIDED);
            compile_declare_global_or_nonlocal(comp, (mp_parse_node_t)pns, id_info, is_global);
        }
    }
}

static void compile_assert_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    if (MP_STATE_VM(mp_optimise_value) != 0) {
        return;
    }

    uint l_end = comp_next_label(comp);
    c_if_cond(comp, pns->nodes[0], true, l_end);
    EMIT_LOAD_GLOBAL(MP_QSTR_AssertionError); 
    if (!MP_PARSE_NODE_IS_NULL(pns->nodes[1])) {
        
        compile_node(comp, pns->nodes[1]);
        EMIT_ARG(call_function, 1, 0, 0);
    }
    EMIT_ARG(raise_varargs, 1);
    EMIT_ARG(label_assign, l_end);
}

static void compile_if_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    uint l_end = comp_next_label(comp);

    
    if (!mp_parse_node_is_const_false(pns->nodes[0])) {
        uint l_fail = comp_next_label(comp);
        c_if_cond(comp, pns->nodes[0], false, l_fail); 

        compile_node(comp, pns->nodes[1]); 

        
        if (mp_parse_node_is_const_true(pns->nodes[0])) {
            goto done;
        }

        
        if (!(MP_PARSE_NODE_IS_NULL(pns->nodes[2]) && MP_PARSE_NODE_IS_NULL(pns->nodes[3]))) {
            
            EMIT_ARG(jump, l_end);
        }

        EMIT_ARG(label_assign, l_fail);
    }

    
    mp_parse_node_t *pn_elif;
    size_t n_elif = mp_parse_node_extract_list(&pns->nodes[2], PN_if_stmt_elif_list, &pn_elif);
    for (size_t i = 0; i < n_elif; i++) {
        assert(MP_PARSE_NODE_IS_STRUCT_KIND(pn_elif[i], PN_if_stmt_elif)); 
        mp_parse_node_struct_t *pns_elif = (mp_parse_node_struct_t *)pn_elif[i];

        
        if (!mp_parse_node_is_const_false(pns_elif->nodes[0])) {
            uint l_fail = comp_next_label(comp);
            c_if_cond(comp, pns_elif->nodes[0], false, l_fail); 

            compile_node(comp, pns_elif->nodes[1]); 

            
            if (mp_parse_node_is_const_true(pns_elif->nodes[0])) {
                goto done;
            }

            EMIT_ARG(jump, l_end);
            EMIT_ARG(label_assign, l_fail);
        }
    }

    
    compile_node(comp, pns->nodes[3]); 

done:
    EMIT_ARG(label_assign, l_end);
}

#define START_BREAK_CONTINUE_BLOCK \
    uint16_t old_break_label = comp->break_label; \
    uint16_t old_continue_label = comp->continue_label; \
    uint16_t old_break_continue_except_level = comp->break_continue_except_level; \
    uint break_label = comp_next_label(comp); \
    uint continue_label = comp_next_label(comp); \
    comp->break_label = break_label; \
    comp->continue_label = continue_label; \
    comp->break_continue_except_level = comp->cur_except_level;

#define END_BREAK_CONTINUE_BLOCK \
    comp->break_label = old_break_label; \
    comp->continue_label = old_continue_label; \
    comp->break_continue_except_level = old_break_continue_except_level;

static void compile_while_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    START_BREAK_CONTINUE_BLOCK

    if (!mp_parse_node_is_const_false(pns->nodes[0])) { 
        uint top_label = comp_next_label(comp);
        if (!mp_parse_node_is_const_true(pns->nodes[0])) { 
            EMIT_ARG(jump, continue_label);
        }
        EMIT_ARG(label_assign, top_label);
        compile_node(comp, pns->nodes[1]); 
        EMIT_ARG(label_assign, continue_label);
        c_if_cond(comp, pns->nodes[0], true, top_label); 
    }

    
    END_BREAK_CONTINUE_BLOCK

    compile_node(comp, pns->nodes[2]); 

    EMIT_ARG(label_assign, break_label);
}

















static void compile_for_stmt_optimised_range(compiler_t *comp, mp_parse_node_t pn_var, mp_parse_node_t pn_start, mp_parse_node_t pn_end, mp_parse_node_t pn_step, mp_parse_node_t pn_body, mp_parse_node_t pn_else) {
    START_BREAK_CONTINUE_BLOCK

    uint top_label = comp_next_label(comp);
    uint entry_label = comp_next_label(comp);

    
    bool end_on_stack = !MP_PARSE_NODE_IS_SMALL_INT(pn_end);
    if (end_on_stack) {
        compile_node(comp, pn_end);
    }

    
    compile_node(comp, pn_start);

    EMIT_ARG(jump, entry_label);
    EMIT_ARG(label_assign, top_label);

    
    EMIT(dup_top);
    c_assign(comp, pn_var, ASSIGN_STORE);

    
    compile_node(comp, pn_body);

    EMIT_ARG(label_assign, continue_label);

    
    compile_node(comp, pn_step);
    EMIT_ARG(binary_op, MP_BINARY_OP_INPLACE_ADD);

    EMIT_ARG(label_assign, entry_label);

    
    if (end_on_stack) {
        EMIT(dup_top_two);
        EMIT(rot_two);
    } else {
        EMIT(dup_top);
        compile_node(comp, pn_end);
    }
    assert(MP_PARSE_NODE_IS_SMALL_INT(pn_step));
    if (MP_PARSE_NODE_LEAF_SMALL_INT(pn_step) >= 0) {
        EMIT_ARG(binary_op, MP_BINARY_OP_LESS);
    } else {
        EMIT_ARG(binary_op, MP_BINARY_OP_MORE);
    }
    EMIT_ARG(pop_jump_if, true, top_label);

    
    END_BREAK_CONTINUE_BLOCK

    
    
    uint end_label = 0;
    if (!MP_PARSE_NODE_IS_NULL(pn_else)) {
        
        EMIT(pop_top);
        if (end_on_stack) {
            EMIT(pop_top);
        }
        compile_node(comp, pn_else);
        end_label = comp_next_label(comp);
        EMIT_ARG(jump, end_label);
        EMIT_ARG(adjust_stack_size, 1 + end_on_stack);
    }

    EMIT_ARG(label_assign, break_label);

    
    EMIT(pop_top);

    
    if (end_on_stack) {
        EMIT(pop_top);
    }

    if (!MP_PARSE_NODE_IS_NULL(pn_else)) {
        EMIT_ARG(label_assign, end_label);
    }
}

static void compile_for_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    
    
    if (  MP_PARSE_NODE_IS_ID(pns->nodes[0]) && MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[1], PN_atom_expr_normal)) {
        mp_parse_node_struct_t *pns_it = (mp_parse_node_struct_t *)pns->nodes[1];
        if (MP_PARSE_NODE_IS_ID(pns_it->nodes[0])
            && MP_PARSE_NODE_LEAF_ARG(pns_it->nodes[0]) == MP_QSTR_range
            && MP_PARSE_NODE_STRUCT_KIND((mp_parse_node_struct_t *)pns_it->nodes[1]) == PN_trailer_paren) {
            mp_parse_node_t pn_range_args = ((mp_parse_node_struct_t *)pns_it->nodes[1])->nodes[0];
            mp_parse_node_t *args;
            size_t n_args = mp_parse_node_extract_list(&pn_range_args, PN_arglist, &args);
            mp_parse_node_t pn_range_start;
            mp_parse_node_t pn_range_end;
            mp_parse_node_t pn_range_step;
            bool optimize = false;
            if (1 <= n_args && n_args <= 3) {
                optimize = true;
                if (n_args == 1) {
                    pn_range_start = mp_parse_node_new_small_int(0);
                    pn_range_end = args[0];
                    pn_range_step = mp_parse_node_new_small_int(1);
                } else if (n_args == 2) {
                    pn_range_start = args[0];
                    pn_range_end = args[1];
                    pn_range_step = mp_parse_node_new_small_int(1);
                } else {
                    pn_range_start = args[0];
                    pn_range_end = args[1];
                    pn_range_step = args[2];
                    
                    if (!MP_PARSE_NODE_IS_SMALL_INT(pn_range_step)
                        || MP_PARSE_NODE_LEAF_SMALL_INT(pn_range_step) == 0) {
                        optimize = false;
                    }
                }
                
                if (optimize && MP_PARSE_NODE_IS_STRUCT(pn_range_start)) {
                    int k = MP_PARSE_NODE_STRUCT_KIND((mp_parse_node_struct_t *)pn_range_start);
                    if (k == PN_arglist_star || k == PN_arglist_dbl_star || k == PN_argument) {
                        optimize = false;
                    }
                }
                if (optimize && MP_PARSE_NODE_IS_STRUCT(pn_range_end)) {
                    int k = MP_PARSE_NODE_STRUCT_KIND((mp_parse_node_struct_t *)pn_range_end);
                    if (k == PN_arglist_star || k == PN_arglist_dbl_star || k == PN_argument) {
                        optimize = false;
                    }
                }
            }
            if (optimize) {
                compile_for_stmt_optimised_range(comp, pns->nodes[0], pn_range_start, pn_range_end, pn_range_step, pns->nodes[2], pns->nodes[3]);
                return;
            }
        }
    }

    START_BREAK_CONTINUE_BLOCK
    comp->break_label |= MP_EMIT_BREAK_FROM_FOR;

    uint pop_label = comp_next_label(comp);

    compile_node(comp, pns->nodes[1]); 
    EMIT_ARG(get_iter, true);
    EMIT_ARG(label_assign, continue_label);
    EMIT_ARG(for_iter, pop_label);
    c_assign(comp, pns->nodes[0], ASSIGN_STORE); 
    compile_node(comp, pns->nodes[2]); 
    EMIT_ARG(jump, continue_label);
    EMIT_ARG(label_assign, pop_label);
    EMIT(for_iter_end);

    
    END_BREAK_CONTINUE_BLOCK

    compile_node(comp, pns->nodes[3]); 

    EMIT_ARG(label_assign, break_label);
}

static void compile_try_except(compiler_t *comp, mp_parse_node_t pn_body, int n_except, mp_parse_node_t *pn_excepts, mp_parse_node_t pn_else) {
    
    uint l1 = comp_next_label(comp);
    uint success_label = comp_next_label(comp);

    compile_increase_except_level(comp, l1, MP_EMIT_SETUP_BLOCK_EXCEPT);

    compile_node(comp, pn_body); 
    EMIT_ARG(pop_except_jump, success_label, false); 

    EMIT_ARG(label_assign, l1); 
    EMIT(start_except_handler);

    

    uint l2 = comp_next_label(comp);

    for (int i = 0; i < n_except; i++) {
        assert(MP_PARSE_NODE_IS_STRUCT_KIND(pn_excepts[i], PN_try_stmt_except)); 
        mp_parse_node_struct_t *pns_except = (mp_parse_node_struct_t *)pn_excepts[i];

        qstr qstr_exception_local = 0;
        uint end_finally_label = comp_next_label(comp);
        #if MICROPY_PY_SYS_SETTRACE
        EMIT_ARG(set_source_line, pns_except->source_line);
        #endif

        if (MP_PARSE_NODE_IS_NULL(pns_except->nodes[0])) {
            
            if (i + 1 != n_except) {
                compile_syntax_error(comp, pn_excepts[i], MP_ERROR_TEXT("default 'except' must be last"));
                compile_decrease_except_level(comp);
                return;
            }
        } else {
            
            mp_parse_node_t pns_exception_expr = pns_except->nodes[0];
            if (MP_PARSE_NODE_IS_STRUCT(pns_exception_expr)) {
                mp_parse_node_struct_t *pns3 = (mp_parse_node_struct_t *)pns_exception_expr;
                if (MP_PARSE_NODE_STRUCT_KIND(pns3) == PN_try_stmt_as_name) {
                    
                    pns_exception_expr = pns3->nodes[0];
                    qstr_exception_local = MP_PARSE_NODE_LEAF_ARG(pns3->nodes[1]);
                }
            }
            EMIT(dup_top);
            compile_node(comp, pns_exception_expr);
            EMIT_ARG(binary_op, MP_BINARY_OP_EXCEPTION_MATCH);
            EMIT_ARG(pop_jump_if, false, end_finally_label);
        }

        
        if (qstr_exception_local == 0) {
            EMIT(pop_top);
        } else {
            compile_store_id(comp, qstr_exception_local);
        }

        
        
        
        
        
        
        
        
        
        uint l3 = 0;
        if (qstr_exception_local != 0) {
            l3 = comp_next_label(comp);
            compile_increase_except_level(comp, l3, MP_EMIT_SETUP_BLOCK_FINALLY);
        }
        compile_node(comp, pns_except->nodes[1]); 
        if (qstr_exception_local != 0) {
            EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
            EMIT_ARG(label_assign, l3);
            EMIT_ARG(adjust_stack_size, 1); 
            EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
            compile_store_id(comp, qstr_exception_local);
            compile_delete_id(comp, qstr_exception_local);
            EMIT_ARG(adjust_stack_size, -1);
            compile_decrease_except_level(comp);
        }

        EMIT_ARG(pop_except_jump, l2, true);
        EMIT_ARG(label_assign, end_finally_label);
        EMIT_ARG(adjust_stack_size, 1); 
    }

    compile_decrease_except_level(comp);
    EMIT(end_except_handler);

    EMIT_ARG(label_assign, success_label);
    compile_node(comp, pn_else); 
    EMIT_ARG(label_assign, l2);
}

static void compile_try_finally(compiler_t *comp, mp_parse_node_t pn_body, int n_except, mp_parse_node_t *pn_except, mp_parse_node_t pn_else, mp_parse_node_t pn_finally) {
    uint l_finally_block = comp_next_label(comp);

    compile_increase_except_level(comp, l_finally_block, MP_EMIT_SETUP_BLOCK_FINALLY);

    if (n_except == 0) {
        assert(MP_PARSE_NODE_IS_NULL(pn_else));
        EMIT_ARG(adjust_stack_size, 3); 
        compile_node(comp, pn_body);
        EMIT_ARG(adjust_stack_size, -3);
    } else {
        compile_try_except(comp, pn_body, n_except, pn_except, pn_else);
    }

    
    
    EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);

    
    
    
    EMIT_ARG(label_assign, l_finally_block);
    EMIT_ARG(adjust_stack_size, 1);
    compile_node(comp, pn_finally);
    EMIT_ARG(adjust_stack_size, -1);

    compile_decrease_except_level(comp);
}

static void compile_try_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    assert(MP_PARSE_NODE_IS_STRUCT(pns->nodes[1])); 
    {
        mp_parse_node_struct_t *pns2 = (mp_parse_node_struct_t *)pns->nodes[1];
        if (MP_PARSE_NODE_STRUCT_KIND(pns2) == PN_try_stmt_finally) {
            
            compile_try_finally(comp, pns->nodes[0], 0, NULL, MP_PARSE_NODE_NULL, pns2->nodes[0]);
        } else if (MP_PARSE_NODE_STRUCT_KIND(pns2) == PN_try_stmt_except_and_more) {
            
            mp_parse_node_t *pn_excepts;
            size_t n_except = mp_parse_node_extract_list(&pns2->nodes[0], PN_try_stmt_except_list, &pn_excepts);
            if (MP_PARSE_NODE_IS_NULL(pns2->nodes[2])) {
                
                compile_try_except(comp, pns->nodes[0], n_except, pn_excepts, pns2->nodes[1]);
            } else {
                
                compile_try_finally(comp, pns->nodes[0], n_except, pn_excepts, pns2->nodes[1], ((mp_parse_node_struct_t *)pns2->nodes[2])->nodes[0]);
            }
        } else {
            
            mp_parse_node_t *pn_excepts;
            size_t n_except = mp_parse_node_extract_list(&pns->nodes[1], PN_try_stmt_except_list, &pn_excepts);
            compile_try_except(comp, pns->nodes[0], n_except, pn_excepts, MP_PARSE_NODE_NULL);
        }
    }
}

static void compile_with_stmt_helper(compiler_t *comp, size_t n, mp_parse_node_t *nodes, mp_parse_node_t body) {
    if (n == 0) {
        
        compile_node(comp, body);
    } else {
        uint l_end = comp_next_label(comp);
        if (MP_PARSE_NODE_IS_STRUCT_KIND(nodes[0], PN_with_item)) {
            
            mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)nodes[0];
            compile_node(comp, pns->nodes[0]);
            compile_increase_except_level(comp, l_end, MP_EMIT_SETUP_BLOCK_WITH);
            c_assign(comp, pns->nodes[1], ASSIGN_STORE);
        } else {
            
            compile_node(comp, nodes[0]);
            compile_increase_except_level(comp, l_end, MP_EMIT_SETUP_BLOCK_WITH);
            EMIT(pop_top);
        }
        
        compile_with_stmt_helper(comp, n - 1, nodes + 1, body);
        
        EMIT_ARG(with_cleanup, l_end);
        reserve_labels_for_native(comp, 3); 
        compile_decrease_except_level(comp);
    }
}

static void compile_with_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    mp_parse_node_t *nodes;
    size_t n = mp_parse_node_extract_list(&pns->nodes[0], PN_with_stmt_list, &nodes);
    assert(n > 0);

    
    compile_with_stmt_helper(comp, n, nodes, pns->nodes[1]);
}

static void compile_yield_from(compiler_t *comp) {
    EMIT_ARG(get_iter, false);
    EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
    EMIT_ARG(yield, MP_EMIT_YIELD_FROM);
    reserve_labels_for_native(comp, 3);
}

#if MICROPY_PY_ASYNC_AWAIT
static void compile_await_object_method(compiler_t *comp, qstr method) {
    EMIT_ARG(load_method, method, false);
    EMIT_ARG(call_method, 0, 0, 0);
    compile_yield_from(comp);
}

static void compile_async_for_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    uint while_else_label = comp_next_label(comp);
    uint try_exception_label = comp_next_label(comp);
    uint try_else_label = comp_next_label(comp);
    uint try_finally_label = comp_next_label(comp);

    

    
    compile_node(comp, pns->nodes[1]); 
    
    EMIT_ARG(load_method, MP_QSTR___aiter__, false);
    
    EMIT_ARG(call_method, 0, 0, 0);
    

    START_BREAK_CONTINUE_BLOCK

    EMIT_ARG(label_assign, continue_label);

    compile_increase_except_level(comp, try_exception_label, MP_EMIT_SETUP_BLOCK_EXCEPT);

    EMIT(dup_top);
    

    
    compile_await_object_method(comp, MP_QSTR___anext__);
    

    c_assign(comp, pns->nodes[0], ASSIGN_STORE); 
    
    EMIT_ARG(pop_except_jump, try_else_label, false);

    EMIT_ARG(label_assign, try_exception_label);
    EMIT(start_except_handler);
    EMIT(dup_top);
    EMIT_LOAD_GLOBAL(MP_QSTR_StopAsyncIteration);
    EMIT_ARG(binary_op, MP_BINARY_OP_EXCEPTION_MATCH);
    EMIT_ARG(pop_jump_if, false, try_finally_label);
    EMIT(pop_top); 
    EMIT_ARG(pop_except_jump, while_else_label, true);

    EMIT_ARG(label_assign, try_finally_label);
    EMIT_ARG(adjust_stack_size, 1); 
    compile_decrease_except_level(comp);
    EMIT(end_except_handler);

    

    EMIT_ARG(label_assign, try_else_label);
    compile_node(comp, pns->nodes[2]); 

    EMIT_ARG(jump, continue_label);
    
    END_BREAK_CONTINUE_BLOCK

    EMIT_ARG(label_assign, while_else_label);
    compile_node(comp, pns->nodes[3]); 

    EMIT_ARG(label_assign, break_label);
    

    EMIT(pop_top);
    
}

static void compile_async_with_stmt_helper(compiler_t *comp, size_t n, mp_parse_node_t *nodes, mp_parse_node_t body) {
    if (n == 0) {
        
        compile_node(comp, body);
    } else {
        uint l_finally_block = comp_next_label(comp);
        uint l_aexit_no_exc = comp_next_label(comp);
        uint l_ret_unwind_jump = comp_next_label(comp);
        uint l_end = comp_next_label(comp);

        if (MP_PARSE_NODE_IS_STRUCT_KIND(nodes[0], PN_with_item)) {
            
            mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)nodes[0];
            compile_node(comp, pns->nodes[0]);
            EMIT(dup_top);
            compile_await_object_method(comp, MP_QSTR___aenter__);
            c_assign(comp, pns->nodes[1], ASSIGN_STORE);
        } else {
            
            compile_node(comp, nodes[0]);
            EMIT(dup_top);
            compile_await_object_method(comp, MP_QSTR___aenter__);
            EMIT(pop_top);
        }

        
        
        

        
        compile_increase_except_level(comp, l_finally_block, MP_EMIT_SETUP_BLOCK_FINALLY);

        
        EMIT_ARG(adjust_stack_size, 3); 
        compile_async_with_stmt_helper(comp, n - 1, nodes + 1, body);
        EMIT_ARG(adjust_stack_size, -3);

        

        
        
        
        

        
        
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE); 
        EMIT(rot_two);
        EMIT_ARG(jump, l_aexit_no_exc); 

        
        
        EMIT_ARG(label_assign, l_finally_block);

        
        EMIT(dup_top);
        EMIT_LOAD_GLOBAL(MP_QSTR_BaseException);
        EMIT_ARG(binary_op, MP_BINARY_OP_EXCEPTION_MATCH);
        EMIT_ARG(pop_jump_if, false, l_ret_unwind_jump); 

        
        
        EMIT(dup_top);
        EMIT(rot_three);
        EMIT(rot_two);
        EMIT_ARG(load_method, MP_QSTR___aexit__, false);
        EMIT(rot_three);
        EMIT(rot_three);
        EMIT(dup_top);
        #if MICROPY_CPYTHON_COMPAT
        EMIT_ARG(attr, MP_QSTR___class__, MP_EMIT_ATTR_LOAD); 
        #else
        compile_load_id(comp, MP_QSTR_type);
        EMIT(rot_two);
        EMIT_ARG(call_function, 1, 0, 0); 
        #endif
        EMIT(rot_two);
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE); 
        
        EMIT_ARG(call_method, 3, 0, 0);
        compile_yield_from(comp);
        EMIT_ARG(pop_jump_if, false, l_end);
        EMIT(pop_top); 
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE); 
        EMIT_ARG(jump, l_end);
        EMIT_ARG(adjust_stack_size, 2);

        
        
        EMIT_ARG(label_assign, l_ret_unwind_jump);
        EMIT(rot_three);
        EMIT(rot_three);
        EMIT_ARG(label_assign, l_aexit_no_exc);
        EMIT_ARG(load_method, MP_QSTR___aexit__, false);
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
        EMIT(dup_top);
        EMIT(dup_top);
        EMIT_ARG(call_method, 3, 0, 0);
        compile_yield_from(comp);
        EMIT(pop_top);
        EMIT_ARG(adjust_stack_size, -1);

        
        
        
        
        
        EMIT_ARG(label_assign, l_end);
        compile_decrease_except_level(comp);
    }
}

static void compile_async_with_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    mp_parse_node_t *nodes;
    size_t n = mp_parse_node_extract_list(&pns->nodes[0], PN_with_stmt_list, &nodes);
    assert(n > 0);

    
    compile_async_with_stmt_helper(comp, n, nodes, pns->nodes[1]);
}

static void compile_async_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    assert(MP_PARSE_NODE_IS_STRUCT(pns->nodes[0]));
    mp_parse_node_struct_t *pns0 = (mp_parse_node_struct_t *)pns->nodes[0];
    if (MP_PARSE_NODE_STRUCT_KIND(pns0) == PN_funcdef) {
        
        compile_funcdef(comp, pns0);
        scope_t *fscope = (scope_t *)pns0->nodes[4];
        fscope->scope_flags |= MP_SCOPE_FLAG_GENERATOR;
    } else {
        
        int scope_flags = comp->scope_cur->scope_flags;
        if (!(scope_flags & MP_SCOPE_FLAG_GENERATOR)) {
            compile_syntax_error(comp, (mp_parse_node_t)pns0,
                MP_ERROR_TEXT("async for/with outside async function"));
            return;
        }

        if (MP_PARSE_NODE_STRUCT_KIND(pns0) == PN_for_stmt) {
            
            compile_async_for_stmt(comp, pns0);
        } else {
            
            assert(MP_PARSE_NODE_STRUCT_KIND(pns0) == PN_with_stmt);
            compile_async_with_stmt(comp, pns0);
        }
    }
}
#endif

static void compile_expr_stmt(compiler_t *comp, mp_parse_node_struct_t *pns) {
    mp_parse_node_t pn_rhs = pns->nodes[1];
    if (MP_PARSE_NODE_IS_NULL(pn_rhs)) {
        if (comp->is_repl && comp->scope_cur->kind == SCOPE_MODULE) {
            
            compile_load_id(comp, MP_QSTR___repl_print__);
            compile_node(comp, pns->nodes[0]);
            EMIT_ARG(call_function, 1, 0, 0);
            EMIT(pop_top);

        } else {
            
            if ((MP_PARSE_NODE_IS_LEAF(pns->nodes[0]) && !MP_PARSE_NODE_IS_ID(pns->nodes[0]))
                || MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_const_object)) {
                
            } else {
                compile_node(comp, pns->nodes[0]); 
                EMIT(pop_top); 
            }
        }
    } else if (MP_PARSE_NODE_IS_STRUCT(pn_rhs)) {
        mp_parse_node_struct_t *pns1 = (mp_parse_node_struct_t *)pn_rhs;
        int kind = MP_PARSE_NODE_STRUCT_KIND(pns1);
        if (kind == PN_annassign) {
            
            if (MP_PARSE_NODE_IS_NULL(pns1->nodes[1])) {
                
                
                if (comp->scope_cur->kind == SCOPE_FUNCTION) {
                    if (MP_PARSE_NODE_IS_ID(pns->nodes[0])) {
                        qstr lhs = MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]);
                        scope_find_or_add_id(comp->scope_cur, lhs, ID_INFO_KIND_LOCAL);
                    }
                }
            } else {
                
                pn_rhs = pns1->nodes[1];
                goto plain_assign;
            }
        } else if (kind == PN_expr_stmt_augassign) {
            c_assign(comp, pns->nodes[0], ASSIGN_AUG_LOAD); 
            compile_node(comp, pns1->nodes[1]); 
            assert(MP_PARSE_NODE_IS_TOKEN(pns1->nodes[0]));
            mp_token_kind_t tok = MP_PARSE_NODE_LEAF_ARG(pns1->nodes[0]);
            mp_binary_op_t op = MP_BINARY_OP_INPLACE_OR + (tok - MP_TOKEN_DEL_PIPE_EQUAL);
            EMIT_ARG(binary_op, op);
            c_assign(comp, pns->nodes[0], ASSIGN_AUG_STORE); 
        } else if (kind == PN_expr_stmt_assign_list) {
            int rhs = MP_PARSE_NODE_STRUCT_NUM_NODES(pns1) - 1;
            compile_node(comp, pns1->nodes[rhs]); 
            
            if (rhs > 0) {
                EMIT(dup_top);
            }
            c_assign(comp, pns->nodes[0], ASSIGN_STORE); 
            for (int i = 0; i < rhs; i++) {
                if (i + 1 < rhs) {
                    EMIT(dup_top);
                }
                c_assign(comp, pns1->nodes[i], ASSIGN_STORE); 
            }
        } else {
        plain_assign:
            #if MICROPY_COMP_DOUBLE_TUPLE_ASSIGN
            if (MP_PARSE_NODE_IS_STRUCT_KIND(pn_rhs, PN_testlist_star_expr)
                && MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_testlist_star_expr)) {
                mp_parse_node_struct_t *pns0 = (mp_parse_node_struct_t *)pns->nodes[0];
                pns1 = (mp_parse_node_struct_t *)pn_rhs;
                uint32_t n_pns0 = MP_PARSE_NODE_STRUCT_NUM_NODES(pns0);
                
                
                
                
                if (n_pns0 == MP_PARSE_NODE_STRUCT_NUM_NODES(pns1)
                    && (n_pns0 == 2
                        #if MICROPY_COMP_TRIPLE_TUPLE_ASSIGN
                        || n_pns0 == 3
                        #endif
                        )
                    && !MP_PARSE_NODE_IS_STRUCT_KIND(pns0->nodes[0], PN_star_expr)
                    && !MP_PARSE_NODE_IS_STRUCT_KIND(pns0->nodes[1], PN_star_expr)
                    #if MICROPY_COMP_TRIPLE_TUPLE_ASSIGN
                    && (n_pns0 == 2 || !MP_PARSE_NODE_IS_STRUCT_KIND(pns0->nodes[2], PN_star_expr))
                    #endif
                    ) {
                    
                    compile_node(comp, pns1->nodes[0]); 
                    compile_node(comp, pns1->nodes[1]); 
                    #if MICROPY_COMP_TRIPLE_TUPLE_ASSIGN
                    if (n_pns0 == 3) {
                        compile_node(comp, pns1->nodes[2]); 
                        EMIT(rot_three);
                    }
                    #endif
                    EMIT(rot_two);
                    c_assign(comp, pns0->nodes[0], ASSIGN_STORE); 
                    c_assign(comp, pns0->nodes[1], ASSIGN_STORE); 
                    #if MICROPY_COMP_TRIPLE_TUPLE_ASSIGN
                    if (n_pns0 == 3) {
                        c_assign(comp, pns0->nodes[2], ASSIGN_STORE); 
                    }
                    #endif
                    return;
                }
            }
            #endif

            compile_node(comp, pn_rhs); 
            c_assign(comp, pns->nodes[0], ASSIGN_STORE); 
        }
    } else {
        goto plain_assign;
    }
}

static void compile_test_if_expr(compiler_t *comp, mp_parse_node_struct_t *pns) {
    assert(MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[1], PN_test_if_else));
    mp_parse_node_struct_t *pns_test_if_else = (mp_parse_node_struct_t *)pns->nodes[1];

    uint l_fail = comp_next_label(comp);
    uint l_end = comp_next_label(comp);
    c_if_cond(comp, pns_test_if_else->nodes[0], false, l_fail); 
    compile_node(comp, pns->nodes[0]); 
    EMIT_ARG(jump, l_end);
    EMIT_ARG(label_assign, l_fail);
    EMIT_ARG(adjust_stack_size, -1); 
    compile_node(comp, pns_test_if_else->nodes[1]); 
    EMIT_ARG(label_assign, l_end);
}

static void compile_lambdef(compiler_t *comp, mp_parse_node_struct_t *pns) {
    if (comp->pass == MP_PASS_SCOPE) {
        
        scope_t *s = scope_new_and_link(comp, SCOPE_LAMBDA, (mp_parse_node_t)pns, comp->scope_cur->emit_options);
        
        pns->nodes[2] = (mp_parse_node_t)s;
    }

    
    scope_t *this_scope = (scope_t *)pns->nodes[2];

    
    compile_funcdef_lambdef(comp, this_scope, pns->nodes[0], PN_varargslist);
}

#if MICROPY_PY_ASSIGN_EXPR
static void compile_namedexpr_helper(compiler_t *comp, mp_parse_node_t pn_name, mp_parse_node_t pn_expr) {
    if (!MP_PARSE_NODE_IS_ID(pn_name)) {
        compile_syntax_error(comp, (mp_parse_node_t)pn_name, MP_ERROR_TEXT("can't assign to expression"));
    }
    compile_node(comp, pn_expr);
    EMIT(dup_top);

    qstr target = MP_PARSE_NODE_LEAF_ARG(pn_name);

    
    
    
    if (comp->pass == MP_PASS_SCOPE && SCOPE_IS_COMP_LIKE(comp->scope_cur->kind)) {
        id_info_t *id_info_parent = mp_emit_common_get_id_for_modification(comp->scope_cur->parent, target);
        if (id_info_parent->kind == ID_INFO_KIND_GLOBAL_EXPLICIT) {
            scope_find_or_add_id(comp->scope_cur, target, ID_INFO_KIND_GLOBAL_EXPLICIT);
        } else {
            id_info_t *id_info = scope_find_or_add_id(comp->scope_cur, target, ID_INFO_KIND_UNDECIDED);
            bool is_global = comp->scope_cur->parent->parent == NULL; 
            if (!is_global && id_info->kind == ID_INFO_KIND_GLOBAL_IMPLICIT) {
                
                
                id_info->kind = ID_INFO_KIND_UNDECIDED;
            }
            compile_declare_global_or_nonlocal(comp, pn_name, id_info, is_global);
        }
    }

    
    compile_store_id(comp, target);
}

static void compile_namedexpr(compiler_t *comp, mp_parse_node_struct_t *pns) {
    compile_namedexpr_helper(comp, pns->nodes[0], pns->nodes[1]);
}
#endif

static void compile_or_and_test(compiler_t *comp, mp_parse_node_struct_t *pns) {
    bool cond = MP_PARSE_NODE_STRUCT_KIND(pns) == PN_or_test;
    uint l_end = comp_next_label(comp);
    int n = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
    for (int i = 0; i < n; i += 1) {
        compile_node(comp, pns->nodes[i]);
        if (i + 1 < n) {
            EMIT_ARG(jump_if_or_pop, cond, l_end);
        }
    }
    EMIT_ARG(label_assign, l_end);
}

static void compile_not_test_2(compiler_t *comp, mp_parse_node_struct_t *pns) {
    compile_node(comp, pns->nodes[0]);
    EMIT_ARG(unary_op, MP_UNARY_OP_NOT);
}

static void compile_comparison(compiler_t *comp, mp_parse_node_struct_t *pns) {
    int num_nodes = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
    compile_node(comp, pns->nodes[0]);
    bool multi = (num_nodes > 3);
    uint l_fail = 0;
    if (multi) {
        l_fail = comp_next_label(comp);
    }
    for (int i = 1; i + 1 < num_nodes; i += 2) {
        compile_node(comp, pns->nodes[i + 1]);
        if (i + 2 < num_nodes) {
            EMIT(dup_top);
            EMIT(rot_three);
        }
        if (MP_PARSE_NODE_IS_TOKEN(pns->nodes[i])) {
            mp_token_kind_t tok = MP_PARSE_NODE_LEAF_ARG(pns->nodes[i]);
            mp_binary_op_t op;
            if (tok == MP_TOKEN_KW_IN) {
                op = MP_BINARY_OP_IN;
            } else {
                op = MP_BINARY_OP_LESS + (tok - MP_TOKEN_OP_LESS);
            }
            EMIT_ARG(binary_op, op);
        } else {
            assert(MP_PARSE_NODE_IS_STRUCT(pns->nodes[i])); 
            mp_parse_node_struct_t *pns2 = (mp_parse_node_struct_t *)pns->nodes[i];
            int kind = MP_PARSE_NODE_STRUCT_KIND(pns2);
            if (kind == PN_comp_op_not_in) {
                EMIT_ARG(binary_op, MP_BINARY_OP_NOT_IN);
            } else {
                assert(kind == PN_comp_op_is); 
                if (MP_PARSE_NODE_IS_NULL(pns2->nodes[0])) {
                    EMIT_ARG(binary_op, MP_BINARY_OP_IS);
                } else {
                    EMIT_ARG(binary_op, MP_BINARY_OP_IS_NOT);
                }
            }
        }
        if (i + 2 < num_nodes) {
            EMIT_ARG(jump_if_or_pop, false, l_fail);
        }
    }
    if (multi) {
        uint l_end = comp_next_label(comp);
        EMIT_ARG(jump, l_end);
        EMIT_ARG(label_assign, l_fail);
        EMIT_ARG(adjust_stack_size, 1);
        EMIT(rot_two);
        EMIT(pop_top);
        EMIT_ARG(label_assign, l_end);
    }
}

static void compile_star_expr(compiler_t *comp, mp_parse_node_struct_t *pns) {
    compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("*x must be assignment target"));
}

static void compile_binary_op(compiler_t *comp, mp_parse_node_struct_t *pns) {
    MP_STATIC_ASSERT(MP_BINARY_OP_OR + PN_xor_expr - PN_expr == MP_BINARY_OP_XOR);
    MP_STATIC_ASSERT(MP_BINARY_OP_OR + PN_and_expr - PN_expr == MP_BINARY_OP_AND);
    mp_binary_op_t binary_op = MP_BINARY_OP_OR + MP_PARSE_NODE_STRUCT_KIND(pns) - PN_expr;
    int num_nodes = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
    compile_node(comp, pns->nodes[0]);
    for (int i = 1; i < num_nodes; ++i) {
        compile_node(comp, pns->nodes[i]);
        EMIT_ARG(binary_op, binary_op);
    }
}

static void compile_term(compiler_t *comp, mp_parse_node_struct_t *pns) {
    int num_nodes = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
    compile_node(comp, pns->nodes[0]);
    for (int i = 1; i + 1 < num_nodes; i += 2) {
        compile_node(comp, pns->nodes[i + 1]);
        mp_token_kind_t tok = MP_PARSE_NODE_LEAF_ARG(pns->nodes[i]);
        mp_binary_op_t op = MP_BINARY_OP_LSHIFT + (tok - MP_TOKEN_OP_DBL_LESS);
        EMIT_ARG(binary_op, op);
    }
}

static void compile_factor_2(compiler_t *comp, mp_parse_node_struct_t *pns) {
    compile_node(comp, pns->nodes[1]);
    mp_token_kind_t tok = MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]);
    mp_unary_op_t op;
    if (tok == MP_TOKEN_OP_TILDE) {
        op = MP_UNARY_OP_INVERT;
    } else {
        assert(tok == MP_TOKEN_OP_PLUS || tok == MP_TOKEN_OP_MINUS);
        op = MP_UNARY_OP_POSITIVE + (tok - MP_TOKEN_OP_PLUS);
    }
    EMIT_ARG(unary_op, op);
}

static void compile_atom_expr_normal(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    compile_node(comp, pns->nodes[0]);

    
    if (MP_PARSE_NODE_IS_NULL(pns->nodes[1])) {
        return;
    }

    
    size_t num_trail = 1;
    mp_parse_node_struct_t **pns_trail = (mp_parse_node_struct_t **)&pns->nodes[1];
    if (MP_PARSE_NODE_STRUCT_KIND(pns_trail[0]) == PN_atom_expr_trailers) {
        num_trail = MP_PARSE_NODE_STRUCT_NUM_NODES(pns_trail[0]);
        pns_trail = (mp_parse_node_struct_t **)&pns_trail[0]->nodes[0];
    }

    
    size_t i = 0;

    
    if (comp->scope_cur->kind == SCOPE_FUNCTION
        && MP_PARSE_NODE_IS_ID(pns->nodes[0])
        && MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]) == MP_QSTR_super
        && MP_PARSE_NODE_STRUCT_KIND(pns_trail[0]) == PN_trailer_paren
        && MP_PARSE_NODE_IS_NULL(pns_trail[0]->nodes[0])) {
        

        
        compile_load_id(comp, MP_QSTR___class__);

        
        bool found = false;
        id_info_t *id = &comp->scope_cur->id_info[0];
        for (size_t n = comp->scope_cur->id_info_len; n > 0; --n, ++id) {
            if (id->flags & ID_FLAG_IS_PARAM) {
                
                compile_load_id(comp, id->qst);
                found = true;
                break;
            }
        }
        if (!found) {
            compile_syntax_error(comp, (mp_parse_node_t)pns_trail[0],
                MP_ERROR_TEXT("super() can't find self")); 
            return;
        }

        if (num_trail >= 3
            && MP_PARSE_NODE_STRUCT_KIND(pns_trail[1]) == PN_trailer_period
            && MP_PARSE_NODE_STRUCT_KIND(pns_trail[2]) == PN_trailer_paren) {
            
            mp_parse_node_struct_t *pns_period = pns_trail[1];
            mp_parse_node_struct_t *pns_paren = pns_trail[2];
            EMIT_ARG(load_method, MP_PARSE_NODE_LEAF_ARG(pns_period->nodes[0]), true);
            compile_trailer_paren_helper(comp, pns_paren->nodes[0], true, 0);
            i = 3;
        } else {
            
            EMIT_ARG(call_function, 2, 0, 0);
            i = 1;
        }

        #if MICROPY_COMP_CONST_LITERAL && MICROPY_PY_COLLECTIONS_ORDEREDDICT
        
    } else if (MP_PARSE_NODE_IS_ID(pns->nodes[0])
               && MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]) == MP_QSTR_OrderedDict
               && MP_PARSE_NODE_STRUCT_KIND(pns_trail[0]) == PN_trailer_paren
               && MP_PARSE_NODE_IS_STRUCT_KIND(pns_trail[0]->nodes[0], PN_atom_brace)) {
        

        EMIT_ARG(call_function, 0, 0, 0);
        mp_parse_node_struct_t *pns_dict = (mp_parse_node_struct_t *)pns_trail[0]->nodes[0];
        compile_atom_brace_helper(comp, pns_dict, false);
        i = 1;
        #endif
    }

    
    for (; i < num_trail; i++) {
        if (i + 1 < num_trail
            && MP_PARSE_NODE_STRUCT_KIND(pns_trail[i]) == PN_trailer_period
            && MP_PARSE_NODE_STRUCT_KIND(pns_trail[i + 1]) == PN_trailer_paren) {
            
            mp_parse_node_struct_t *pns_period = pns_trail[i];
            mp_parse_node_struct_t *pns_paren = pns_trail[i + 1];
            EMIT_ARG(load_method, MP_PARSE_NODE_LEAF_ARG(pns_period->nodes[0]), false);
            compile_trailer_paren_helper(comp, pns_paren->nodes[0], true, 0);
            i += 1;
        } else {
            
            compile_node(comp, (mp_parse_node_t)pns_trail[i]);
        }
    }
}

static void compile_power(compiler_t *comp, mp_parse_node_struct_t *pns) {
    compile_generic_all_nodes(comp, pns); 
    EMIT_ARG(binary_op, MP_BINARY_OP_POWER);
}

static void compile_trailer_paren_helper(compiler_t *comp, mp_parse_node_t pn_arglist, bool is_method_call, int n_positional_extra) {
    

    
    mp_parse_node_t *args;
    size_t n_args = mp_parse_node_extract_list(&pn_arglist, PN_arglist, &args);

    
    
    
    
    int n_positional = n_positional_extra;
    uint n_keyword = 0;
    uint star_flags = 0;
    mp_uint_t star_args = 0;
    for (size_t i = 0; i < n_args; i++) {
        if (MP_PARSE_NODE_IS_STRUCT(args[i])) {
            mp_parse_node_struct_t *pns_arg = (mp_parse_node_struct_t *)args[i];
            if (MP_PARSE_NODE_STRUCT_KIND(pns_arg) == PN_arglist_star) {
                if (star_flags & MP_EMIT_STAR_FLAG_DOUBLE) {
                    compile_syntax_error(comp, (mp_parse_node_t)pns_arg, MP_ERROR_TEXT("* arg after **"));
                    return;
                }
                #if MICROPY_DYNAMIC_COMPILER
                if (i >= (size_t)mp_dynamic_compiler.small_int_bits - 1)
                #else
                if (i >= MP_SMALL_INT_BITS - 1)
                #endif
                {
                    
                    
                    compile_syntax_error(comp, (mp_parse_node_t)pns_arg, MP_ERROR_TEXT("too many args"));
                    return;
                }
                star_flags |= MP_EMIT_STAR_FLAG_SINGLE;
                star_args |= (mp_uint_t)1 << i;
                compile_node(comp, pns_arg->nodes[0]);
                n_positional++;
            } else if (MP_PARSE_NODE_STRUCT_KIND(pns_arg) == PN_arglist_dbl_star) {
                star_flags |= MP_EMIT_STAR_FLAG_DOUBLE;
                
                EMIT(load_null);
                compile_node(comp, pns_arg->nodes[0]);
                n_keyword++;
            } else if (MP_PARSE_NODE_STRUCT_KIND(pns_arg) == PN_argument) {
                #if MICROPY_PY_ASSIGN_EXPR
                if (MP_PARSE_NODE_IS_STRUCT_KIND(pns_arg->nodes[1], PN_argument_3)) {
                    compile_namedexpr_helper(comp, pns_arg->nodes[0], ((mp_parse_node_struct_t *)pns_arg->nodes[1])->nodes[0]);
                    n_positional++;
                } else
                #endif
                if (!MP_PARSE_NODE_IS_STRUCT_KIND(pns_arg->nodes[1], PN_comp_for)) {
                    if (!MP_PARSE_NODE_IS_ID(pns_arg->nodes[0])) {
                        compile_syntax_error(comp, (mp_parse_node_t)pns_arg, MP_ERROR_TEXT("LHS of keyword arg must be an id"));
                        return;
                    }
                    EMIT_ARG(load_const_str, MP_PARSE_NODE_LEAF_ARG(pns_arg->nodes[0]));
                    compile_node(comp, pns_arg->nodes[1]);
                    n_keyword++;
                } else {
                    compile_comprehension(comp, pns_arg, SCOPE_GEN_EXPR);
                    n_positional++;
                }
            } else {
                goto normal_argument;
            }
        } else {
        normal_argument:
            if (star_flags & MP_EMIT_STAR_FLAG_DOUBLE) {
                compile_syntax_error(comp, args[i], MP_ERROR_TEXT("positional arg after **"));
                return;
            }
            if (n_keyword > 0) {
                compile_syntax_error(comp, args[i], MP_ERROR_TEXT("positional arg after keyword arg"));
                return;
            }
            compile_node(comp, args[i]);
            n_positional++;
        }
    }

    if (star_flags != 0) {
        
        EMIT_ARG(load_const_small_int, star_args);
    }

    
    if (is_method_call) {
        EMIT_ARG(call_method, n_positional, n_keyword, star_flags);
    } else {
        EMIT_ARG(call_function, n_positional, n_keyword, star_flags);
    }
}


static void compile_comprehension(compiler_t *comp, mp_parse_node_struct_t *pns, scope_kind_t kind) {
    assert(MP_PARSE_NODE_STRUCT_NUM_NODES(pns) == 2);
    assert(MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[1], PN_comp_for));
    mp_parse_node_struct_t *pns_comp_for = (mp_parse_node_struct_t *)pns->nodes[1];

    if (comp->pass == MP_PASS_SCOPE) {
        
        scope_t *s = scope_new_and_link(comp, kind, (mp_parse_node_t)pns, comp->scope_cur->emit_options);
        
        pns_comp_for->nodes[3] = (mp_parse_node_t)s;
    }

    
    scope_t *this_scope = (scope_t *)pns_comp_for->nodes[3];

    
    close_over_variables_etc(comp, this_scope, 0, 0);

    compile_node(comp, pns_comp_for->nodes[1]); 
    if (kind == SCOPE_GEN_EXPR) {
        EMIT_ARG(get_iter, false);
    }
    EMIT_ARG(call_function, 1, 0, 0);
}

static void compile_atom_paren(compiler_t *comp, mp_parse_node_struct_t *pns) {
    if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
        
        EMIT_ARG(build, 0, MP_EMIT_BUILD_TUPLE);
    } else {
        assert(MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_testlist_comp));
        pns = (mp_parse_node_struct_t *)pns->nodes[0];
        if (MP_PARSE_NODE_TESTLIST_COMP_HAS_COMP_FOR(pns)) {
            
            compile_comprehension(comp, pns, SCOPE_GEN_EXPR);
        } else {
            
            compile_generic_tuple(comp, pns);
        }
    }
}

static void compile_atom_bracket(compiler_t *comp, mp_parse_node_struct_t *pns) {
    if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
        
        EMIT_ARG(build, 0, MP_EMIT_BUILD_LIST);
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_testlist_comp)) {
        mp_parse_node_struct_t *pns2 = (mp_parse_node_struct_t *)pns->nodes[0];
        if (MP_PARSE_NODE_TESTLIST_COMP_HAS_COMP_FOR(pns2)) {
            
            compile_comprehension(comp, pns2, SCOPE_LIST_COMP);
        } else {
            
            compile_generic_all_nodes(comp, pns2);
            EMIT_ARG(build, MP_PARSE_NODE_STRUCT_NUM_NODES(pns2), MP_EMIT_BUILD_LIST);
        }
    } else {
        
        compile_node(comp, pns->nodes[0]);
        EMIT_ARG(build, 1, MP_EMIT_BUILD_LIST);
    }
}

static void compile_atom_brace_helper(compiler_t *comp, mp_parse_node_struct_t *pns, bool create_map) {
    mp_parse_node_t pn = pns->nodes[0];
    if (MP_PARSE_NODE_IS_NULL(pn)) {
        
        if (create_map) {
            EMIT_ARG(build, 0, MP_EMIT_BUILD_MAP);
        }
    } else if (MP_PARSE_NODE_IS_STRUCT(pn)) {
        pns = (mp_parse_node_struct_t *)pn;
        if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_dictorsetmaker_item) {
            
            if (create_map) {
                EMIT_ARG(build, 1, MP_EMIT_BUILD_MAP);
            }
            compile_node(comp, pn);
            EMIT(store_map);
        } else if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_dictorsetmaker) {
            assert(MP_PARSE_NODE_IS_STRUCT(pns->nodes[1])); 
            mp_parse_node_struct_t *pns1 = (mp_parse_node_struct_t *)pns->nodes[1];
            if (MP_PARSE_NODE_STRUCT_KIND(pns1) == PN_dictorsetmaker_list) {
                

                
                mp_parse_node_t *nodes;
                size_t n = mp_parse_node_extract_list(&pns1->nodes[0], PN_dictorsetmaker_list2, &nodes);

                
                bool is_dict;
                if (!MICROPY_PY_BUILTINS_SET || MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_dictorsetmaker_item)) {
                    
                    if (create_map) {
                        EMIT_ARG(build, 1 + n, MP_EMIT_BUILD_MAP);
                    }
                    compile_node(comp, pns->nodes[0]);
                    EMIT(store_map);
                    is_dict = true;
                } else {
                    
                    compile_node(comp, pns->nodes[0]); 
                    is_dict = false;
                }

                
                for (size_t i = 0; i < n; i++) {
                    mp_parse_node_t pn_i = nodes[i];
                    bool is_key_value = MP_PARSE_NODE_IS_STRUCT_KIND(pn_i, PN_dictorsetmaker_item);
                    compile_node(comp, pn_i);
                    if (is_dict) {
                        if (!is_key_value) {
                            #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
                            compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("invalid syntax"));
                            #else
                            compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("expecting key:value for dict"));
                            #endif
                            return;
                        }
                        EMIT(store_map);
                    } else {
                        if (is_key_value) {
                            #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
                            compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("invalid syntax"));
                            #else
                            compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("expecting just a value for set"));
                            #endif
                            return;
                        }
                    }
                }

                #if MICROPY_PY_BUILTINS_SET
                
                if (!is_dict) {
                    EMIT_ARG(build, 1 + n, MP_EMIT_BUILD_SET);
                }
                #endif
            } else {
                assert(MP_PARSE_NODE_STRUCT_KIND(pns1) == PN_comp_for); 
                
                if (!MICROPY_PY_BUILTINS_SET || MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_dictorsetmaker_item)) {
                    
                    compile_comprehension(comp, pns, SCOPE_DICT_COMP);
                } else {
                    
                    compile_comprehension(comp, pns, SCOPE_SET_COMP);
                }
            }
        } else {
            
            goto set_with_one_element;
        }
    } else {
        
    set_with_one_element:
        #if MICROPY_PY_BUILTINS_SET
        compile_node(comp, pn);
        EMIT_ARG(build, 1, MP_EMIT_BUILD_SET);
        #else
        assert(0);
        #endif
    }
}

static void compile_atom_brace(compiler_t *comp, mp_parse_node_struct_t *pns) {
    compile_atom_brace_helper(comp, pns, true);
}

static void compile_trailer_paren(compiler_t *comp, mp_parse_node_struct_t *pns) {
    compile_trailer_paren_helper(comp, pns->nodes[0], false, 0);
}

static void compile_trailer_bracket(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    compile_node(comp, pns->nodes[0]); 
    EMIT_ARG(subscr, MP_EMIT_SUBSCR_LOAD);
}

static void compile_trailer_period(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    EMIT_ARG(attr, MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]), MP_EMIT_ATTR_LOAD); 
}

#if MICROPY_PY_BUILTINS_SLICE
static void compile_subscript(compiler_t *comp, mp_parse_node_struct_t *pns) {
    if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_subscript_2) {
        compile_node(comp, pns->nodes[0]); 
        assert(MP_PARSE_NODE_IS_STRUCT(pns->nodes[1])); 
        pns = (mp_parse_node_struct_t *)pns->nodes[1];
    } else {
        
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
    }

    assert(MP_PARSE_NODE_STRUCT_KIND(pns) == PN_subscript_3); 
    mp_parse_node_t pn = pns->nodes[0];
    if (MP_PARSE_NODE_IS_NULL(pn)) {
        
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
        EMIT_ARG(build, 2, MP_EMIT_BUILD_SLICE);
    } else if (MP_PARSE_NODE_IS_STRUCT(pn)) {
        pns = (mp_parse_node_struct_t *)pn;
        if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_subscript_3c) {
            EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
            pn = pns->nodes[0];
            if (MP_PARSE_NODE_IS_NULL(pn)) {
                
                EMIT_ARG(build, 2, MP_EMIT_BUILD_SLICE);
            } else {
                
                compile_node(comp, pn);
                EMIT_ARG(build, 3, MP_EMIT_BUILD_SLICE);
            }
        } else if (MP_PARSE_NODE_STRUCT_KIND(pns) == PN_subscript_3d) {
            compile_node(comp, pns->nodes[0]);
            assert(MP_PARSE_NODE_IS_STRUCT(pns->nodes[1])); 
            pns = (mp_parse_node_struct_t *)pns->nodes[1];
            assert(MP_PARSE_NODE_STRUCT_KIND(pns) == PN_sliceop); 
            if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
                
                EMIT_ARG(build, 2, MP_EMIT_BUILD_SLICE);
            } else {
                
                compile_node(comp, pns->nodes[0]);
                EMIT_ARG(build, 3, MP_EMIT_BUILD_SLICE);
            }
        } else {
            
            compile_node(comp, pn);
            EMIT_ARG(build, 2, MP_EMIT_BUILD_SLICE);
        }
    } else {
        
        compile_node(comp, pn);
        EMIT_ARG(build, 2, MP_EMIT_BUILD_SLICE);
    }
}
#endif 

static void compile_dictorsetmaker_item(compiler_t *comp, mp_parse_node_struct_t *pns) {
    
    compile_node(comp, pns->nodes[1]); 
    compile_node(comp, pns->nodes[0]); 
}

static void compile_classdef(compiler_t *comp, mp_parse_node_struct_t *pns) {
    qstr cname = compile_classdef_helper(comp, pns, comp->scope_cur->emit_options);
    
    compile_store_id(comp, cname);
}

static void compile_yield_expr(compiler_t *comp, mp_parse_node_struct_t *pns) {
    if (comp->scope_cur->kind != SCOPE_FUNCTION && comp->scope_cur->kind != SCOPE_LAMBDA) {
        compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("'yield' outside function"));
        return;
    }
    if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
        EMIT_ARG(yield, MP_EMIT_YIELD_VALUE);
        reserve_labels_for_native(comp, 1);
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_yield_arg_from)) {
        pns = (mp_parse_node_struct_t *)pns->nodes[0];
        compile_node(comp, pns->nodes[0]);
        compile_yield_from(comp);
    } else {
        compile_node(comp, pns->nodes[0]);
        EMIT_ARG(yield, MP_EMIT_YIELD_VALUE);
        reserve_labels_for_native(comp, 1);
    }
}

#if MICROPY_PY_ASYNC_AWAIT
static void compile_atom_expr_await(compiler_t *comp, mp_parse_node_struct_t *pns) {
    if (comp->scope_cur->kind != SCOPE_FUNCTION && comp->scope_cur->kind != SCOPE_LAMBDA) {
        #if MICROPY_COMP_ALLOW_TOP_LEVEL_AWAIT
        if (!mp_compile_allow_top_level_await)
        #endif
        {
            compile_syntax_error(comp, (mp_parse_node_t)pns, MP_ERROR_TEXT("'await' outside function"));
            return;
        }
    }
    compile_atom_expr_normal(comp, pns);
    compile_yield_from(comp);
}
#endif

static mp_obj_t get_const_object(mp_parse_node_struct_t *pns) {
    return mp_parse_node_extract_const_object(pns);
}

static void compile_const_object(compiler_t *comp, mp_parse_node_struct_t *pns) {
    EMIT_ARG(load_const_obj, get_const_object(pns));
}

typedef void (*compile_function_t)(compiler_t *, mp_parse_node_struct_t *);
static const compile_function_t compile_function[] = {

#define c(f) compile_##f
#define DEF_RULE(rule, comp, kind, ...) comp,
#define DEF_RULE_NC(rule, kind, ...)
    #include "py/grammar.h"
#undef c
#undef DEF_RULE
#undef DEF_RULE_NC
    compile_const_object,
};

static void compile_node(compiler_t *comp, mp_parse_node_t pn) {
    if (MP_PARSE_NODE_IS_NULL(pn)) {
        
    } else if (MP_PARSE_NODE_IS_SMALL_INT(pn)) {
        mp_int_t arg = MP_PARSE_NODE_LEAF_SMALL_INT(pn);
        EMIT_ARG(load_const_small_int, arg);
    } else if (MP_PARSE_NODE_IS_LEAF(pn)) {
        uintptr_t arg = MP_PARSE_NODE_LEAF_ARG(pn);
        switch (MP_PARSE_NODE_LEAF_KIND(pn)) {
            case MP_PARSE_NODE_ID:
                compile_load_id(comp, arg);
                break;
            case MP_PARSE_NODE_STRING:
                EMIT_ARG(load_const_str, arg);
                break;
            case MP_PARSE_NODE_TOKEN:
            default:
                if (arg == MP_TOKEN_NEWLINE) {
                    
                    
                    
                } else {
                    EMIT_ARG(load_const_tok, arg);
                }
                break;
        }
    } else {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        EMIT_ARG(set_source_line, pns->source_line);
        assert(MP_PARSE_NODE_STRUCT_KIND(pns) <= PN_const_object);
        compile_function_t f = compile_function[MP_PARSE_NODE_STRUCT_KIND(pns)];
        f(comp, pns);
    }
}

#if MICROPY_EMIT_NATIVE
static int compile_viper_type_annotation(compiler_t *comp, mp_parse_node_t pn_annotation) {
    int native_type = MP_NATIVE_TYPE_OBJ;
    if (MP_PARSE_NODE_IS_NULL(pn_annotation)) {
        
    } else if (MP_PARSE_NODE_IS_ID(pn_annotation)) {
        qstr type_name = MP_PARSE_NODE_LEAF_ARG(pn_annotation);
        native_type = mp_native_type_from_qstr(type_name);
        if (native_type < 0) {
            comp->compile_error = mp_obj_new_exception_msg_varg(&mp_type_ViperTypeError, MP_ERROR_TEXT("unknown type '%q'"), type_name);
            native_type = 0;
        }
    } else {
        compile_syntax_error(comp, pn_annotation, MP_ERROR_TEXT("annotation must be an identifier"));
    }
    return native_type;
}
#endif

static void compile_scope_func_lambda_param(compiler_t *comp, mp_parse_node_t pn, pn_kind_t pn_name, pn_kind_t pn_star, pn_kind_t pn_dbl_star) {
    (void)pn_dbl_star;

    
    if ((comp->scope_cur->scope_flags & MP_SCOPE_FLAG_VARKEYWORDS) != 0) {
        compile_syntax_error(comp, pn, MP_ERROR_TEXT("invalid syntax"));
        return;
    }

    qstr param_name = MP_QSTRnull;
    uint param_flag = ID_FLAG_IS_PARAM;
    mp_parse_node_struct_t *pns = NULL;
    if (MP_PARSE_NODE_IS_ID(pn)) {
        param_name = MP_PARSE_NODE_LEAF_ARG(pn);
        if (comp->have_star) {
            
            comp->scope_cur->num_kwonly_args += 1;
        } else {
            
            comp->scope_cur->num_pos_args += 1;
        }
    } else {
        assert(MP_PARSE_NODE_IS_STRUCT(pn));
        pns = (mp_parse_node_struct_t *)pn;
        if (MP_PARSE_NODE_STRUCT_KIND(pns) == pn_name) {
            
            param_name = MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]);
            if (comp->have_star) {
                
                comp->scope_cur->num_kwonly_args += 1;
            } else {
                
                comp->scope_cur->num_pos_args += 1;
            }
        } else if (MP_PARSE_NODE_STRUCT_KIND(pns) == pn_star) {
            if (comp->have_star) {
                
                compile_syntax_error(comp, pn, MP_ERROR_TEXT("invalid syntax"));
                return;
            }
            comp->have_star = true;
            param_flag = ID_FLAG_IS_PARAM | ID_FLAG_IS_STAR_PARAM;
            if (MP_PARSE_NODE_IS_NULL(pns->nodes[0])) {
                
                
                
                pns = NULL;
            } else if (MP_PARSE_NODE_IS_ID(pns->nodes[0])) {
                
                comp->scope_cur->scope_flags |= MP_SCOPE_FLAG_VARARGS;
                param_name = MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]);
                pns = NULL;
            } else {
                assert(MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_tfpdef)); 
                
                comp->scope_cur->scope_flags |= MP_SCOPE_FLAG_VARARGS;
                pns = (mp_parse_node_struct_t *)pns->nodes[0];
                param_name = MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]);
            }
        } else {
            
            assert(MP_PARSE_NODE_STRUCT_KIND(pns) == pn_dbl_star); 
            param_name = MP_PARSE_NODE_LEAF_ARG(pns->nodes[0]);
            param_flag = ID_FLAG_IS_PARAM | ID_FLAG_IS_DBL_STAR_PARAM;
            comp->scope_cur->scope_flags |= MP_SCOPE_FLAG_VARKEYWORDS;
        }
    }

    if (param_name != MP_QSTRnull) {
        id_info_t *id_info = scope_find_or_add_id(comp->scope_cur, param_name, ID_INFO_KIND_UNDECIDED);
        if (id_info->kind != ID_INFO_KIND_UNDECIDED) {
            compile_syntax_error(comp, pn, MP_ERROR_TEXT("argument name reused"));
            return;
        }
        id_info->kind = ID_INFO_KIND_LOCAL;
        id_info->flags = param_flag;

        #if MICROPY_EMIT_NATIVE
        if (comp->scope_cur->emit_options == MP_EMIT_OPT_VIPER && pn_name == PN_typedargslist_name && pns != NULL) {
            id_info->flags |= compile_viper_type_annotation(comp, pns->nodes[1]) << ID_FLAG_VIPER_TYPE_POS;
        }
        #else
        (void)pns;
        #endif
    }
}

static void compile_scope_func_param(compiler_t *comp, mp_parse_node_t pn) {
    compile_scope_func_lambda_param(comp, pn, PN_typedargslist_name, PN_typedargslist_star, PN_typedargslist_dbl_star);
}

static void compile_scope_lambda_param(compiler_t *comp, mp_parse_node_t pn) {
    compile_scope_func_lambda_param(comp, pn, PN_varargslist_name, PN_varargslist_star, PN_varargslist_dbl_star);
}

static void compile_scope_comp_iter(compiler_t *comp, mp_parse_node_struct_t *pns_comp_for, mp_parse_node_t pn_inner_expr, int for_depth) {
    uint l_top = comp_next_label(comp);
    uint l_end = comp_next_label(comp);
    EMIT_ARG(label_assign, l_top);
    EMIT_ARG(for_iter, l_end);
    c_assign(comp, pns_comp_for->nodes[0], ASSIGN_STORE);
    mp_parse_node_t pn_iter = pns_comp_for->nodes[2];

tail_recursion:
    if (MP_PARSE_NODE_IS_NULL(pn_iter)) {
        
        compile_node(comp, pn_inner_expr);
        if (comp->scope_cur->kind == SCOPE_GEN_EXPR) {
            EMIT_ARG(yield, MP_EMIT_YIELD_VALUE);
            reserve_labels_for_native(comp, 1);
            EMIT(pop_top);
        } else {
            EMIT_ARG(store_comp, comp->scope_cur->kind, 4 * for_depth + 5);
        }
    } else if (MP_PARSE_NODE_STRUCT_KIND((mp_parse_node_struct_t *)pn_iter) == PN_comp_if) {
        
        mp_parse_node_struct_t *pns_comp_if = (mp_parse_node_struct_t *)pn_iter;
        c_if_cond(comp, pns_comp_if->nodes[0], false, l_top);
        pn_iter = pns_comp_if->nodes[1];
        goto tail_recursion;
    } else {
        assert(MP_PARSE_NODE_STRUCT_KIND((mp_parse_node_struct_t *)pn_iter) == PN_comp_for); 
        
        mp_parse_node_struct_t *pns_comp_for2 = (mp_parse_node_struct_t *)pn_iter;
        compile_node(comp, pns_comp_for2->nodes[1]);
        EMIT_ARG(get_iter, true);
        compile_scope_comp_iter(comp, pns_comp_for2, pn_inner_expr, for_depth + 1);
    }

    EMIT_ARG(jump, l_top);
    EMIT_ARG(label_assign, l_end);
    EMIT(for_iter_end);
}

static void check_for_doc_string(compiler_t *comp, mp_parse_node_t pn) {
    #if MICROPY_ENABLE_DOC_STRING
    

    
    if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_expr_stmt)) {
        
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_file_input_2)) {
        
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        int num_nodes = MP_PARSE_NODE_STRUCT_NUM_NODES(pns);
        for (int i = 0; i < num_nodes; i++) {
            pn = pns->nodes[i];
            if (!(MP_PARSE_NODE_IS_LEAF(pn) && MP_PARSE_NODE_LEAF_KIND(pn) == MP_PARSE_NODE_TOKEN && MP_PARSE_NODE_LEAF_ARG(pn) == MP_TOKEN_NEWLINE)) {
                
                break;
            }
        }
        
    } else if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_suite_block_stmts)) {
        
        pn = ((mp_parse_node_struct_t *)pn)->nodes[0];
    } else {
        return;
    }

    
    if (MP_PARSE_NODE_IS_STRUCT_KIND(pn, PN_expr_stmt)) {
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)pn;
        if ((MP_PARSE_NODE_IS_LEAF(pns->nodes[0])
             && MP_PARSE_NODE_LEAF_KIND(pns->nodes[0]) == MP_PARSE_NODE_STRING)
            || (MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[0], PN_const_object)
                && mp_obj_is_str(get_const_object((mp_parse_node_struct_t *)pns->nodes[0])))) {
            
            compile_node(comp, pns->nodes[0]);
            
            compile_store_id(comp, MP_QSTR___doc__);
        }
    }
    #else
    (void)comp;
    (void)pn;
    #endif
}

static bool compile_scope(compiler_t *comp, scope_t *scope, pass_kind_t pass) {
    comp->pass = pass;
    comp->scope_cur = scope;
    comp->next_label = 0;
    mp_emit_common_start_pass(&comp->emit_common, pass);
    EMIT_ARG(start_pass, pass, scope);
    reserve_labels_for_native(comp, 6); 

    if (comp->pass == MP_PASS_SCOPE) {
        
        
        scope->stack_size = 0;
        scope->exc_stack_size = 0;
    }

    
    if (MP_PARSE_NODE_IS_STRUCT_KIND(scope->pn, PN_eval_input)) {
        assert(scope->kind == SCOPE_MODULE);
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)scope->pn;
        compile_node(comp, pns->nodes[0]); 
        EMIT(return_value);
    } else if (scope->kind == SCOPE_MODULE) {
        if (!comp->is_repl) {
            check_for_doc_string(comp, scope->pn);
        }
        compile_node(comp, scope->pn);
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
        EMIT(return_value);
    } else if (scope->kind == SCOPE_FUNCTION) {
        assert(MP_PARSE_NODE_IS_STRUCT(scope->pn));
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)scope->pn;
        assert(MP_PARSE_NODE_STRUCT_KIND(pns) == PN_funcdef);

        
        
        if (comp->pass == MP_PASS_SCOPE) {
            comp->have_star = false;
            apply_to_single_or_list(comp, pns->nodes[1], PN_typedargslist, compile_scope_func_param);

            #if MICROPY_EMIT_NATIVE
            if (scope->emit_options == MP_EMIT_OPT_VIPER) {
                
                scope->scope_flags |= compile_viper_type_annotation(comp, pns->nodes[2]) << MP_SCOPE_FLAG_VIPERRET_POS;
            }
            #endif 
        }

        compile_node(comp, pns->nodes[3]); 
        EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
        EMIT(return_value);
    } else if (scope->kind == SCOPE_LAMBDA) {
        assert(MP_PARSE_NODE_IS_STRUCT(scope->pn));
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)scope->pn;
        assert(MP_PARSE_NODE_STRUCT_NUM_NODES(pns) == 3);

        
        EMIT_ARG(set_source_line, pns->source_line);

        
        
        if (comp->pass == MP_PASS_SCOPE) {
            comp->have_star = false;
            apply_to_single_or_list(comp, pns->nodes[0], PN_varargslist, compile_scope_lambda_param);
        }

        compile_node(comp, pns->nodes[1]); 

        
        if (scope->scope_flags & MP_SCOPE_FLAG_GENERATOR) {
            EMIT(pop_top);
            EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
        }
        EMIT(return_value);
    } else if (SCOPE_IS_COMP_LIKE(scope->kind)) {
        

        assert(MP_PARSE_NODE_IS_STRUCT(scope->pn));
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)scope->pn;
        assert(MP_PARSE_NODE_STRUCT_NUM_NODES(pns) == 2);
        assert(MP_PARSE_NODE_IS_STRUCT_KIND(pns->nodes[1], PN_comp_for));
        mp_parse_node_struct_t *pns_comp_for = (mp_parse_node_struct_t *)pns->nodes[1];

        
        
        
        
        qstr qstr_arg = MP_QSTR_;
        if (comp->pass == MP_PASS_SCOPE) {
            scope_find_or_add_id(comp->scope_cur, qstr_arg, ID_INFO_KIND_LOCAL);
            scope->num_pos_args = 1;
        }

        
        EMIT_ARG(set_source_line, pns->source_line);

        if (scope->kind == SCOPE_LIST_COMP) {
            EMIT_ARG(build, 0, MP_EMIT_BUILD_LIST);
        } else if (scope->kind == SCOPE_DICT_COMP) {
            EMIT_ARG(build, 0, MP_EMIT_BUILD_MAP);
        #if MICROPY_PY_BUILTINS_SET
        } else if (scope->kind == SCOPE_SET_COMP) {
            EMIT_ARG(build, 0, MP_EMIT_BUILD_SET);
        #endif
        }

        
        
        if (scope->kind == SCOPE_GEN_EXPR) {
            MP_STATIC_ASSERT(MP_OBJ_ITER_BUF_NSLOTS == 4);
            EMIT(load_null);
            compile_load_id(comp, qstr_arg);
            EMIT(load_null);
            EMIT(load_null);
        } else {
            compile_load_id(comp, qstr_arg);
            EMIT_ARG(get_iter, true);
        }

        compile_scope_comp_iter(comp, pns_comp_for, pns->nodes[0], 0);

        if (scope->kind == SCOPE_GEN_EXPR) {
            EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
        }
        EMIT(return_value);
    } else {
        assert(scope->kind == SCOPE_CLASS);
        assert(MP_PARSE_NODE_IS_STRUCT(scope->pn));
        mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)scope->pn;
        assert(MP_PARSE_NODE_STRUCT_KIND(pns) == PN_classdef);

        if (comp->pass == MP_PASS_SCOPE) {
            scope_find_or_add_id(scope, MP_QSTR___class__, ID_INFO_KIND_LOCAL);
        }

        #if MICROPY_PY_SYS_SETTRACE
        EMIT_ARG(set_source_line, pns->source_line);
        #endif
        compile_load_id(comp, MP_QSTR___name__);
        compile_store_id(comp, MP_QSTR___module__);
        EMIT_ARG(load_const_str, MP_PARSE_NODE_LEAF_ARG(pns->nodes[0])); 
        compile_store_id(comp, MP_QSTR___qualname__);

        check_for_doc_string(comp, pns->nodes[2]);
        compile_node(comp, pns->nodes[2]); 

        id_info_t *id = scope_find(scope, MP_QSTR___class__);
        assert(id != NULL);
        if (id->kind == ID_INFO_KIND_LOCAL) {
            EMIT_ARG(load_const_tok, MP_TOKEN_KW_NONE);
        } else {
            EMIT_LOAD_FAST(MP_QSTR___class__, id->local_num);
        }
        EMIT(return_value);
    }

    bool pass_complete = EMIT(end_pass);

    
    assert(comp->cur_except_level == 0);

    return pass_complete;
}

#if MICROPY_EMIT_INLINE_ASM

static void compile_scope_inline_asm(compiler_t *comp, scope_t *scope, pass_kind_t pass) {
    comp->pass = pass;
    comp->scope_cur = scope;
    comp->next_label = 0;

    if (scope->kind != SCOPE_FUNCTION) {
        compile_syntax_error(comp, MP_PARSE_NODE_NULL, MP_ERROR_TEXT("inline assembler must be a function"));
        return;
    }

    if (comp->pass > MP_PASS_SCOPE) {
        EMIT_INLINE_ASM_ARG(start_pass, comp->pass, &comp->compile_error);
    }

    
    assert(MP_PARSE_NODE_IS_STRUCT(scope->pn));
    mp_parse_node_struct_t *pns = (mp_parse_node_struct_t *)scope->pn;
    assert(MP_PARSE_NODE_STRUCT_KIND(pns) == PN_funcdef);

    

    
    if (comp->pass == MP_PASS_CODE_SIZE) {
        mp_parse_node_t *pn_params;
        size_t n_params = mp_parse_node_extract_list(&pns->nodes[1], PN_typedargslist, &pn_params);
        scope->num_pos_args = EMIT_INLINE_ASM_ARG(count_params, n_params, pn_params);
        if (comp->compile_error != MP_OBJ_NULL) {
            goto inline_asm_error;
        }
    }

    
    mp_uint_t type_sig = MP_NATIVE_TYPE_INT;
    mp_parse_node_t pn_annotation = pns->nodes[2];
    if (!MP_PARSE_NODE_IS_NULL(pn_annotation)) {
        
        if (MP_PARSE_NODE_IS_ID(pn_annotation)) {
            qstr ret_type = MP_PARSE_NODE_LEAF_ARG(pn_annotation);
            switch (ret_type) {
                case MP_QSTR_object:
                    type_sig = MP_NATIVE_TYPE_OBJ;
                    break;
                case MP_QSTR_bool:
                    type_sig = MP_NATIVE_TYPE_BOOL;
                    break;
                case MP_QSTR_int:
                    type_sig = MP_NATIVE_TYPE_INT;
                    break;
                case MP_QSTR_uint:
                    type_sig = MP_NATIVE_TYPE_UINT;
                    break;
                default:
                    compile_syntax_error(comp, pn_annotation, MP_ERROR_TEXT("unknown type"));
                    return;
            }
        } else {
            compile_syntax_error(comp, pn_annotation, MP_ERROR_TEXT("return annotation must be an identifier"));
        }
    }

    mp_parse_node_t pn_body = pns->nodes[3]; 
    mp_parse_node_t *nodes;
    size_t num = mp_parse_node_extract_list(&pn_body, PN_suite_block_stmts, &nodes);

    for (size_t i = 0; i < num; i++) {
        assert(MP_PARSE_NODE_IS_STRUCT(nodes[i]));
        mp_parse_node_struct_t *pns2 = (mp_parse_node_struct_t *)nodes[i];
        if (MP_PARSE_NODE_STRUCT_KIND(pns2) == PN_pass_stmt) {
            
            continue;
        } else if (MP_PARSE_NODE_STRUCT_KIND(pns2) != PN_expr_stmt) {
            
        not_an_instruction:
            compile_syntax_error(comp, nodes[i], MP_ERROR_TEXT("expecting an assembler instruction"));
            return;
        }

        
        assert(MP_PARSE_NODE_IS_STRUCT(pns2->nodes[0]));
        if (!MP_PARSE_NODE_IS_NULL(pns2->nodes[1])) {
            goto not_an_instruction;
        }
        pns2 = (mp_parse_node_struct_t *)pns2->nodes[0];
        if (MP_PARSE_NODE_STRUCT_KIND(pns2) != PN_atom_expr_normal) {
            goto not_an_instruction;
        }
        if (!MP_PARSE_NODE_IS_ID(pns2->nodes[0])) {
            goto not_an_instruction;
        }
        if (!MP_PARSE_NODE_IS_STRUCT_KIND(pns2->nodes[1], PN_trailer_paren)) {
            goto not_an_instruction;
        }

        
        
        qstr op = MP_PARSE_NODE_LEAF_ARG(pns2->nodes[0]);
        pns2 = (mp_parse_node_struct_t *)pns2->nodes[1]; 
        mp_parse_node_t *pn_arg;
        size_t n_args = mp_parse_node_extract_list(&pns2->nodes[0], PN_arglist, &pn_arg);

        
        if (op == MP_QSTR_label) {
            if (!(n_args == 1 && MP_PARSE_NODE_IS_ID(pn_arg[0]))) {
                compile_syntax_error(comp, nodes[i], MP_ERROR_TEXT("'label' requires 1 argument"));
                return;
            }
            uint lab = comp_next_label(comp);
            if (pass > MP_PASS_SCOPE) {
                if (!EMIT_INLINE_ASM_ARG(label, lab, MP_PARSE_NODE_LEAF_ARG(pn_arg[0]))) {
                    compile_syntax_error(comp, nodes[i], MP_ERROR_TEXT("label redefined"));
                    return;
                }
            }
        } else if (op == MP_QSTR_align) {
            if (!(n_args == 1 && MP_PARSE_NODE_IS_SMALL_INT(pn_arg[0]))) {
                compile_syntax_error(comp, nodes[i], MP_ERROR_TEXT("'align' requires 1 argument"));
                return;
            }
            if (pass > MP_PASS_SCOPE) {
                mp_asm_base_align((mp_asm_base_t *)comp->emit_inline_asm,
                    MP_PARSE_NODE_LEAF_SMALL_INT(pn_arg[0]));
            }
        } else if (op == MP_QSTR_data) {
            if (!(n_args >= 2 && MP_PARSE_NODE_IS_SMALL_INT(pn_arg[0]))) {
                compile_syntax_error(comp, nodes[i], MP_ERROR_TEXT("'data' requires at least 2 arguments"));
                return;
            }
            if (pass > MP_PASS_SCOPE) {
                mp_int_t bytesize = MP_PARSE_NODE_LEAF_SMALL_INT(pn_arg[0]);
                for (uint j = 1; j < n_args; j++) {
                    mp_obj_t int_obj;
                    if (!mp_parse_node_get_int_maybe(pn_arg[j], &int_obj)) {
                        compile_syntax_error(comp, nodes[i], MP_ERROR_TEXT("'data' requires integer arguments"));
                        return;
                    }
                    mp_asm_base_data((mp_asm_base_t *)comp->emit_inline_asm,
                        bytesize, mp_obj_int_get_truncated(int_obj));
                }
            }
        } else {
            if (pass > MP_PASS_SCOPE) {
                EMIT_INLINE_ASM_ARG(op, op, n_args, pn_arg);
            }
        }

        if (comp->compile_error != MP_OBJ_NULL) {
            pns = pns2; 
            goto inline_asm_error;
        }
    }

    if (comp->pass > MP_PASS_SCOPE) {
        EMIT_INLINE_ASM_ARG(end_pass, type_sig);

        if (comp->pass == MP_PASS_EMIT) {
            void *f = mp_asm_base_get_code((mp_asm_base_t *)comp->emit_inline_asm);
            mp_emit_glue_assign_native(comp->scope_cur->raw_code, MP_CODE_NATIVE_ASM,
                f, mp_asm_base_get_code_size((mp_asm_base_t *)comp->emit_inline_asm),
                NULL,
                #if MICROPY_PERSISTENT_CODE_SAVE
                0,
                0,
                #endif
                0, comp->scope_cur->num_pos_args, type_sig);
        }
    }

    if (comp->compile_error != MP_OBJ_NULL) {
        
    inline_asm_error:
        comp->compile_error_line = pns->source_line;
    }
}
#endif

static void scope_compute_things(scope_t *scope) {
    
    if (scope->scope_flags & MP_SCOPE_FLAG_VARARGS) {
        id_info_t *id_param = NULL;
        for (int i = scope->id_info_len - 1; i >= 0; i--) {
            id_info_t *id = &scope->id_info[i];
            if (id->flags & ID_FLAG_IS_STAR_PARAM) {
                if (id_param != NULL) {
                    
                    id_info_t temp = *id_param;
                    *id_param = *id;
                    *id = temp;
                }
                break;
            } else if (id_param == NULL && id->flags == ID_FLAG_IS_PARAM) {
                id_param = id;
            }
        }
    }

    
    
    scope->num_locals = 0;
    for (int i = 0; i < scope->id_info_len; i++) {
        id_info_t *id = &scope->id_info[i];
        if (scope->kind == SCOPE_CLASS && id->qst == MP_QSTR___class__) {
            
            continue;
        }
        if (SCOPE_IS_FUNC_LIKE(scope->kind) && id->kind == ID_INFO_KIND_GLOBAL_IMPLICIT) {
            id->kind = ID_INFO_KIND_GLOBAL_EXPLICIT;
        }
        #if MICROPY_EMIT_NATIVE
        if (id->kind == ID_INFO_KIND_GLOBAL_EXPLICIT) {
            
            if (scope->emit_options == MP_EMIT_OPT_VIPER
                && mp_native_type_from_qstr(id->qst) >= MP_NATIVE_TYPE_INT) {
                
            } else {
                scope->scope_flags |= MP_SCOPE_FLAG_REFGLOBALS;
            }
        }
        #endif
        
        if (id->kind == ID_INFO_KIND_LOCAL || (id->flags & ID_FLAG_IS_PARAM)) {
            id->local_num = scope->num_locals++;
        }
    }

    
    for (int i = 0; i < scope->id_info_len; i++) {
        id_info_t *id = &scope->id_info[i];
        
        
        
        if (id->kind == ID_INFO_KIND_CELL && !(id->flags & ID_FLAG_IS_PARAM)) {
            id->local_num = scope->num_locals;
            scope->num_locals += 1;
        }
    }

    
    
    if (scope->parent != NULL) {
        int num_free = 0;
        for (int i = 0; i < scope->parent->id_info_len; i++) {
            id_info_t *id = &scope->parent->id_info[i];
            if (id->kind == ID_INFO_KIND_CELL || id->kind == ID_INFO_KIND_FREE) {
                for (int j = 0; j < scope->id_info_len; j++) {
                    id_info_t *id2 = &scope->id_info[j];
                    if (id2->kind == ID_INFO_KIND_FREE && id->qst == id2->qst) {
                        assert(!(id2->flags & ID_FLAG_IS_PARAM)); 
                        
                        id2->local_num = num_free;
                        num_free += 1;
                    }
                }
            }
        }
        
        if (num_free > 0) {
            for (int i = 0; i < scope->id_info_len; i++) {
                id_info_t *id = &scope->id_info[i];
                if (id->kind != ID_INFO_KIND_FREE || (id->flags & ID_FLAG_IS_PARAM)) {
                    id->local_num += num_free;
                }
            }
            scope->num_pos_args += num_free; 
            scope->num_locals += num_free;
        }
    }
}

#if !MICROPY_PERSISTENT_CODE_SAVE
static
#endif
void mp_compile_to_raw_code(mp_parse_tree_t *parse_tree, qstr source_file, bool is_repl, mp_compiled_module_t *cm) {
    
    compiler_t comp_state = {0};
    compiler_t *comp = &comp_state;

    comp->is_repl = is_repl;
    comp->break_label = INVALID_LABEL;
    comp->continue_label = INVALID_LABEL;
    mp_emit_common_init(&comp->emit_common, source_file);

    
    #if MICROPY_EMIT_NATIVE
    const uint emit_opt = MP_STATE_VM(default_emit_opt);
    #else
    const uint emit_opt = MP_EMIT_OPT_NONE;
    #endif
    scope_t *module_scope = scope_new_and_link(comp, SCOPE_MODULE, parse_tree->root, emit_opt);

    
    emit_t *emit_bc = emit_bc_new(&comp->emit_common);

    
    comp->emit = emit_bc;
    #if MICROPY_EMIT_NATIVE
    comp->emit_method_table = &emit_bc_method_table;
    #endif
    uint max_num_labels = 0;
    for (scope_t *s = comp->scope_head; s != NULL && comp->compile_error == MP_OBJ_NULL; s = s->next) {
        #if MICROPY_EMIT_INLINE_ASM
        if (s->emit_options == MP_EMIT_OPT_ASM) {
            compile_scope_inline_asm(comp, s, MP_PASS_SCOPE);
        } else
        #endif
        {
            compile_scope(comp, s, MP_PASS_SCOPE);

            
            for (size_t i = 0; i < s->id_info_len; ++i) {
                id_info_t *id = &s->id_info[i];
                if (id->kind == ID_INFO_KIND_GLOBAL_IMPLICIT) {
                    scope_check_to_close_over(s, id);
                }
            }
        }

        
        if (comp->next_label > max_num_labels) {
            max_num_labels = comp->next_label;
        }
    }

    
    for (scope_t *s = comp->scope_head; s != NULL && comp->compile_error == MP_OBJ_NULL; s = s->next) {
        scope_compute_things(s);
    }

    
    emit_bc_set_max_num_labels(emit_bc, max_num_labels);

    
    #if MICROPY_EMIT_NATIVE
    emit_t *emit_native = NULL;
    #endif
    for (scope_t *s = comp->scope_head; s != NULL && comp->compile_error == MP_OBJ_NULL; s = s->next) {
        #if MICROPY_EMIT_INLINE_ASM
        if (s->emit_options == MP_EMIT_OPT_ASM) {
            
            if (comp->emit_inline_asm == NULL) {
                comp->emit_inline_asm = ASM_EMITTER(new)(max_num_labels);
            }
            comp->emit = NULL;
            comp->emit_inline_asm_method_table = ASM_EMITTER_TABLE;
            compile_scope_inline_asm(comp, s, MP_PASS_CODE_SIZE);
            #if MICROPY_EMIT_INLINE_XTENSA
            
            
            
            #if MICROPY_DYNAMIC_COMPILER
            if (mp_dynamic_compiler.native_arch == MP_NATIVE_ARCH_XTENSA)
            #endif
            {
                compile_scope_inline_asm(comp, s, MP_PASS_CODE_SIZE);
            }
            #endif
            if (comp->compile_error == MP_OBJ_NULL) {
                compile_scope_inline_asm(comp, s, MP_PASS_EMIT);
            }
        } else
        #endif
        {

            

            switch (s->emit_options) {

                #if MICROPY_EMIT_NATIVE
                case MP_EMIT_OPT_NATIVE_PYTHON:
                case MP_EMIT_OPT_VIPER:
                    if (emit_native == NULL) {
                        emit_native = NATIVE_EMITTER(new)(&comp->emit_common, &comp->compile_error, &comp->next_label, max_num_labels);
                    }
                    comp->emit_method_table = NATIVE_EMITTER_TABLE;
                    comp->emit = emit_native;
                    break;
                #endif 

                default:
                    comp->emit = emit_bc;
                    #if MICROPY_EMIT_NATIVE
                    comp->emit_method_table = &emit_bc_method_table;
                    #endif
                    break;
            }

            
            compile_scope(comp, s, MP_PASS_STACK_SIZE);

            
            if (comp->compile_error == MP_OBJ_NULL) {
                compile_scope(comp, s, MP_PASS_CODE_SIZE);
            }

            
            
            if (comp->compile_error == MP_OBJ_NULL) {
                while (!compile_scope(comp, s, MP_PASS_EMIT)) {
                }
            }
        }
    }

    if (comp->compile_error != MP_OBJ_NULL) {
        
        
        compile_error_set_line(comp, comp->scope_cur->pn);
        
        mp_obj_exception_add_traceback(comp->compile_error, source_file,
            comp->compile_error_line, comp->scope_cur->simple_name);
    }

    
    cm->rc = module_scope->raw_code;
    #if MICROPY_PERSISTENT_CODE_SAVE
    cm->has_native = false;
    #if MICROPY_EMIT_NATIVE
    if (emit_native != NULL) {
        cm->has_native = true;
    }
    #endif
    #if MICROPY_EMIT_INLINE_ASM
    if (comp->emit_inline_asm != NULL) {
        cm->has_native = true;
    }
    #endif
    cm->n_qstr = comp->emit_common.qstr_map.used;
    cm->n_obj = comp->emit_common.const_obj_list.len;
    #endif
    if (comp->compile_error == MP_OBJ_NULL) {
        mp_emit_common_populate_module_context(&comp->emit_common, source_file, cm->context);

        #if MICROPY_DEBUG_PRINTERS
        
        if (mp_verbose_flag >= 2) {
            for (scope_t *s = comp->scope_head; s != NULL; s = s->next) {
                mp_raw_code_t *rc = s->raw_code;
                if (rc->kind == MP_CODE_BYTECODE) {
                    mp_bytecode_print(&mp_plat_print, rc, s->raw_code_data_len, &cm->context->constants);
                }
            }
        }
        #endif
    }

    

    emit_bc_free(emit_bc);
    #if MICROPY_EMIT_NATIVE
    if (emit_native != NULL) {
        NATIVE_EMITTER(free)(emit_native);
    }
    #endif
    #if MICROPY_EMIT_INLINE_ASM
    if (comp->emit_inline_asm != NULL) {
        ASM_EMITTER(free)(comp->emit_inline_asm);
    }
    #endif

    
    mp_parse_tree_clear(parse_tree);

    
    for (scope_t *s = module_scope; s;) {
        scope_t *next = s->next;
        scope_free(s);
        s = next;
    }

    if (comp->compile_error != MP_OBJ_NULL) {
        nlr_raise(comp->compile_error);
    }
}

mp_obj_t mp_compile(mp_parse_tree_t *parse_tree, qstr source_file, bool is_repl) {
    mp_compiled_module_t cm;
    cm.context = m_new_obj(mp_module_context_t);
    cm.context->module.globals = mp_globals_get();
    mp_compile_to_raw_code(parse_tree, source_file, is_repl, &cm);
    
    return mp_make_function_from_proto_fun(cm.rc, cm.context, NULL);
}

#endif 
