 
#ifndef MICROPY_INCLUDED_PY_OBJGENERATOR_H
#define MICROPY_INCLUDED_PY_OBJGENERATOR_H

#include "py/obj.h"
#include "py/runtime.h"

mp_vm_return_kind_t mp_obj_gen_resume(mp_obj_t self_in, mp_obj_t send_val, mp_obj_t throw_val, mp_obj_t *ret_val);

#endif 
