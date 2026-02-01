 
#ifndef MICROPY_INCLUDED_PY_SCOPE_H
#define MICROPY_INCLUDED_PY_SCOPE_H

#include "py/parse.h"
#include "py/emitglue.h"

typedef enum {
    ID_INFO_KIND_UNDECIDED,
    ID_INFO_KIND_GLOBAL_IMPLICIT,
    ID_INFO_KIND_GLOBAL_IMPLICIT_ASSIGNED,
    ID_INFO_KIND_GLOBAL_EXPLICIT,
    ID_INFO_KIND_LOCAL, 
    ID_INFO_KIND_CELL,  
    ID_INFO_KIND_FREE,  
} id_info_kind_t;

enum {
    ID_FLAG_IS_PARAM = 0x01,
    ID_FLAG_IS_STAR_PARAM = 0x02,
    ID_FLAG_IS_DBL_STAR_PARAM = 0x04,
    ID_FLAG_VIPER_TYPE_POS = 4,
};

typedef struct _id_info_t {
    uint8_t kind;
    uint8_t flags;
    
    
    uint16_t local_num;
    qstr qst;
} id_info_t;

#define SCOPE_IS_FUNC_LIKE(s) ((s) >= SCOPE_LAMBDA)
#define SCOPE_IS_COMP_LIKE(s) (SCOPE_LIST_COMP <= (s) && (s) <= SCOPE_GEN_EXPR)


typedef enum {
    SCOPE_MODULE,
    SCOPE_CLASS,
    SCOPE_LAMBDA,
    SCOPE_LIST_COMP,
    SCOPE_DICT_COMP,
    SCOPE_SET_COMP,
    SCOPE_GEN_EXPR,
    SCOPE_FUNCTION,
} scope_kind_t;

typedef struct _scope_t {
    scope_kind_t kind;
    struct _scope_t *parent;
    struct _scope_t *next;
    mp_parse_node_t pn;
    mp_raw_code_t *raw_code;
    #if MICROPY_DEBUG_PRINTERS
    size_t raw_code_data_len; 
    #endif
    uint16_t simple_name; 
    uint16_t scope_flags;  
    uint16_t emit_options; 
    uint16_t num_pos_args;
    uint16_t num_kwonly_args;
    uint16_t num_def_pos_args;
    uint16_t num_locals;
    uint16_t stack_size;     
    uint16_t exc_stack_size; 
    uint16_t id_info_alloc;
    uint16_t id_info_len;
    id_info_t *id_info;
} scope_t;

scope_t *scope_new(scope_kind_t kind, mp_parse_node_t pn, mp_uint_t emit_options);
void scope_free(scope_t *scope);
id_info_t *scope_find_or_add_id(scope_t *scope, qstr qstr, id_info_kind_t kind);
id_info_t *scope_find(scope_t *scope, qstr qstr);
id_info_t *scope_find_global(scope_t *scope, qstr qstr);
void scope_check_to_close_over(scope_t *scope, id_info_t *id);

#endif 
