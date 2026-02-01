 
#ifndef MICROPY_INCLUDED_PY_OBJSTR_H
#define MICROPY_INCLUDED_PY_OBJSTR_H

#include "py/obj.h"
#include "py/objarray.h"

typedef struct _mp_obj_str_t {
    mp_obj_base_t base;
    size_t hash;
    
    size_t len;
    const byte *data;
} mp_obj_str_t;




#define MP_STATIC_ASSERT_STR_ARRAY_COMPATIBLE \
    MP_STATIC_ASSERT(offsetof(mp_obj_str_t, len) == offsetof(mp_obj_array_t, len) \
    && offsetof(mp_obj_str_t, data) == offsetof(mp_obj_array_t, items))

#define MP_DEFINE_STR_OBJ(obj_name, str) mp_obj_str_t obj_name = {{&mp_type_str}, 0, sizeof(str) - 1, (const byte *)str}



#define GET_STR_HASH(str_obj_in, str_hash) \
    size_t str_hash; \
    if (mp_obj_is_qstr(str_obj_in)) { \
        str_hash = qstr_hash(MP_OBJ_QSTR_VALUE(str_obj_in)); \
    } else { \
        str_hash = ((mp_obj_str_t *)MP_OBJ_TO_PTR(str_obj_in))->hash; \
    }


#define GET_STR_LEN(str_obj_in, str_len) \
    size_t str_len; \
    if (mp_obj_is_qstr(str_obj_in)) { \
        str_len = qstr_len(MP_OBJ_QSTR_VALUE(str_obj_in)); \
    } else { \
        str_len = ((mp_obj_str_t *)MP_OBJ_TO_PTR(str_obj_in))->len; \
    }


#if MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_C || MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_D
const byte *mp_obj_str_get_data_no_check(mp_obj_t self_in, size_t *len);
#define GET_STR_DATA_LEN(str_obj_in, str_data, str_len) \
    size_t str_len; \
    const byte *str_data = mp_obj_str_get_data_no_check(str_obj_in, &str_len);
#else
#define GET_STR_DATA_LEN(str_obj_in, str_data, str_len) \
    const byte *str_data; \
    size_t str_len; \
    if (mp_obj_is_qstr(str_obj_in)) { \
        str_data = qstr_data(MP_OBJ_QSTR_VALUE(str_obj_in), &str_len); \
    } else { \
        MP_STATIC_ASSERT_STR_ARRAY_COMPATIBLE; \
        str_len = ((mp_obj_str_t *)MP_OBJ_TO_PTR(str_obj_in))->len; \
        str_data = ((mp_obj_str_t *)MP_OBJ_TO_PTR(str_obj_in))->data; \
    }
#endif

mp_obj_t mp_obj_str_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args);
void mp_str_print_json(const mp_print_t *print, const byte *str_data, size_t str_len);
mp_obj_t mp_obj_str_format(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs);
mp_obj_t mp_obj_str_split(size_t n_args, const mp_obj_t *args);
mp_obj_t mp_obj_new_str_copy(const mp_obj_type_t *type, const byte *data, size_t len); 
mp_obj_t mp_obj_new_str_of_type(const mp_obj_type_t *type, const byte *data, size_t len); 

mp_obj_t mp_obj_str_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in);
mp_int_t mp_obj_str_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags);

void mp_obj_str_set_data(mp_obj_str_t *str, const byte *data, size_t len);

const byte *str_index_to_ptr(const mp_obj_type_t *type, const byte *self_data, size_t self_len,
    mp_obj_t index, bool is_slice);
const byte *find_subbytes(const byte *haystack, size_t hlen, const byte *needle, size_t nlen, int direction);

#define MP_DEFINE_BYTES_OBJ(obj_name, target, len) mp_obj_str_t obj_name = {{&mp_type_bytes}, 0, (len), (const byte *)(target)}

mp_obj_t mp_obj_bytes_hex(size_t n_args, const mp_obj_t *args, const mp_obj_type_t *type);
mp_obj_t mp_obj_bytes_fromhex(mp_obj_t type_in, mp_obj_t data);

extern const mp_obj_dict_t mp_obj_str_locals_dict;

#if MICROPY_PY_BUILTINS_MEMORYVIEW && MICROPY_PY_BUILTINS_BYTES_HEX
extern const mp_obj_dict_t mp_obj_memoryview_locals_dict;
#endif

#if MICROPY_PY_BUILTINS_BYTEARRAY
extern const mp_obj_dict_t mp_obj_bytearray_locals_dict;
#endif

#if MICROPY_PY_ARRAY
extern const mp_obj_dict_t mp_obj_array_locals_dict;
#endif

#endif 
