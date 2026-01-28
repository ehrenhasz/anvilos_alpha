
#ifndef MICROPY_INCLUDED_PY_OBJARRAY_H
#define MICROPY_INCLUDED_PY_OBJARRAY_H

#include "py/obj.h"


#define MP_OBJ_ARRAY_TYPECODE_FLAG_RW (0x80)


#define MP_OBJ_ARRAY_FREE_SIZE_BITS (8 * sizeof(size_t) - 8)




typedef struct _mp_obj_array_t {
    mp_obj_base_t base;
    size_t typecode : 8;
    
    
    
    
    
    
    size_t free : MP_OBJ_ARRAY_FREE_SIZE_BITS;
    size_t len; 
    void *items;
} mp_obj_array_t;

#if MICROPY_PY_BUILTINS_MEMORYVIEW
static inline void mp_obj_memoryview_init(mp_obj_array_t *self, size_t typecode, size_t offset, size_t len, void *items) {
    self->base.type = &mp_type_memoryview;
    self->typecode = typecode;
    self->free = offset;
    self->len = len;
    self->items = items;
}
#endif

#if MICROPY_PY_ARRAY || MICROPY_PY_BUILTINS_BYTEARRAY
MP_DECLARE_CONST_FUN_OBJ_2(mp_obj_array_append_obj);
MP_DECLARE_CONST_FUN_OBJ_2(mp_obj_array_extend_obj);
#endif

#endif 
