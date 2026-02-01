 
#ifndef MICROPY_INCLUDED_PY_OBJSTRINGIO_H
#define MICROPY_INCLUDED_PY_OBJSTRINGIO_H

#include "py/obj.h"

typedef struct _mp_obj_stringio_t {
    mp_obj_base_t base;
    vstr_t *vstr;
    
    mp_uint_t pos;
    
    mp_obj_t ref_obj;
} mp_obj_stringio_t;

#endif 
