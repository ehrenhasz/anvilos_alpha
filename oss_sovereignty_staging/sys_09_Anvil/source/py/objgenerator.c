 

#include <stdlib.h>
#include <assert.h>

#include "py/runtime.h"
#include "py/bc.h"
#include "py/objstr.h"
#include "py/objgenerator.h"
#include "py/objfun.h"
#include "py/stackctrl.h"


const mp_obj_exception_t mp_const_GeneratorExit_obj = {{&mp_type_GeneratorExit}, 0, 0, NULL, (mp_obj_tuple_t *)&mp_const_empty_tuple_obj};

 
 

typedef struct _mp_obj_gen_instance_t {
    mp_obj_base_t base;
    
    
    
    mp_obj_t pend_exc;
    mp_code_state_t code_state;
} mp_obj_gen_instance_t;

static mp_obj_t gen_wrap_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    
    mp_obj_fun_bc_t *self_fun = MP_OBJ_TO_PTR(self_in);

    
    const uint8_t *ip = self_fun->bytecode;
    MP_BC_PRELUDE_SIG_DECODE(ip);

    
    mp_obj_gen_instance_t *o = mp_obj_malloc_var(mp_obj_gen_instance_t, code_state.state, byte,
        n_state * sizeof(mp_obj_t) + n_exc_stack * sizeof(mp_exc_stack_t),
        &mp_type_gen_instance);

    o->pend_exc = mp_const_none;
    o->code_state.fun_bc = self_fun;
    o->code_state.n_state = n_state;
    mp_setup_code_state(&o->code_state, n_args, n_kw, args);
    return MP_OBJ_FROM_PTR(o);
}

#if MICROPY_PY_FUNCTION_ATTRS
#define GEN_WRAP_TYPE_ATTR attr, mp_obj_fun_bc_attr,
#else
#define GEN_WRAP_TYPE_ATTR
#endif

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_gen_wrap,
    MP_QSTR_generator,
    MP_TYPE_FLAG_BINDS_SELF,
    GEN_WRAP_TYPE_ATTR
    call, gen_wrap_call
    );

 


#if MICROPY_EMIT_NATIVE


typedef struct _mp_obj_gen_instance_native_t {
    mp_obj_base_t base;
    mp_obj_t pend_exc;
    mp_code_state_native_t code_state;
} mp_obj_gen_instance_native_t;

static mp_obj_t native_gen_wrap_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    
    mp_obj_fun_bc_t *self_fun = MP_OBJ_TO_PTR(self_in);

    
    const uint8_t *prelude_ptr = mp_obj_fun_native_get_prelude_ptr(self_fun);

    
    const uint8_t *ip = prelude_ptr;
    MP_BC_PRELUDE_SIG_DECODE(ip);

    
    mp_obj_gen_instance_native_t *o = mp_obj_malloc_var(mp_obj_gen_instance_native_t, code_state.state, byte, n_state * sizeof(mp_obj_t), &mp_type_gen_instance);

    
    o->pend_exc = mp_const_none;
    o->code_state.fun_bc = self_fun;
    o->code_state.n_state = n_state;
    mp_setup_code_state_native(&o->code_state, n_args, n_kw, args);

    
    o->code_state.exc_sp_idx = MP_CODE_STATE_EXC_SP_IDX_SENTINEL;

    
    o->code_state.ip = mp_obj_fun_native_get_generator_start(self_fun);

    return MP_OBJ_FROM_PTR(o);
}

#if MICROPY_PY_FUNCTION_ATTRS
#define NATIVE_GEN_WRAP_TYPE_ATTR , attr, mp_obj_fun_bc_attr
#else
#define NATIVE_GEN_WRAP_TYPE_ATTR
#endif

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_native_gen_wrap,
    MP_QSTR_generator,
    MP_TYPE_FLAG_BINDS_SELF,
    call, native_gen_wrap_call
    NATIVE_GEN_WRAP_TYPE_ATTR
    );

#endif 

 
 

static void gen_instance_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_gen_instance_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<generator object '%q' at %p>", mp_obj_fun_get_name(MP_OBJ_FROM_PTR(self->code_state.fun_bc)), self);
}

mp_vm_return_kind_t mp_obj_gen_resume(mp_obj_t self_in, mp_obj_t send_value, mp_obj_t throw_value, mp_obj_t *ret_val) {
    MP_STACK_CHECK();
    mp_check_self(mp_obj_is_type(self_in, &mp_type_gen_instance));
    mp_obj_gen_instance_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->code_state.ip == 0) {
        
        
        *ret_val = mp_const_none;
        return MP_VM_RETURN_NORMAL;
    }

    
    if (self->pend_exc == MP_OBJ_NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("generator already executing"));
    }

    #if MICROPY_PY_GENERATOR_PEND_THROW
    
    if (self->pend_exc != mp_const_none) {
        throw_value = self->pend_exc;
    }
    #endif

    
    void *state_start = self->code_state.state - 1;
    #if MICROPY_EMIT_NATIVE
    if (self->code_state.exc_sp_idx == MP_CODE_STATE_EXC_SP_IDX_SENTINEL) {
        state_start = ((mp_obj_gen_instance_native_t *)self)->code_state.state - 1;
    }
    #endif
    if (self->code_state.sp == state_start) {
        if (send_value != mp_const_none) {
            mp_raise_TypeError(MP_ERROR_TEXT("can't send non-None value to a just-started generator"));
        }
    } else {
        *self->code_state.sp = send_value;
    }

    
    self->pend_exc = MP_OBJ_NULL;

    
    self->code_state.old_globals = mp_globals_get();
    mp_globals_set(self->code_state.fun_bc->context->module.globals);

    mp_vm_return_kind_t ret_kind;

    #if MICROPY_EMIT_NATIVE
    if (self->code_state.exc_sp_idx == MP_CODE_STATE_EXC_SP_IDX_SENTINEL) {
        
        typedef uintptr_t (*mp_fun_native_gen_t)(void *, mp_obj_t);
        mp_fun_native_gen_t fun = mp_obj_fun_native_get_generator_resume(self->code_state.fun_bc);
        ret_kind = fun((void *)&self->code_state, throw_value);
    } else
    #endif
    {
        
        ret_kind = mp_execute_bytecode(&self->code_state, throw_value);
    }

    mp_globals_set(self->code_state.old_globals);

    
    self->pend_exc = mp_const_none;

    switch (ret_kind) {
        case MP_VM_RETURN_NORMAL:
        default:
            
            
            
            self->code_state.ip = 0;
            
            *ret_val = *self->code_state.sp;
            break;

        case MP_VM_RETURN_YIELD:
            *ret_val = *self->code_state.sp;
            #if MICROPY_PY_GENERATOR_PEND_THROW
            *self->code_state.sp = mp_const_none;
            #endif
            break;

        case MP_VM_RETURN_EXCEPTION: {
            self->code_state.ip = 0;
            #if MICROPY_EMIT_NATIVE
            if (self->code_state.exc_sp_idx == MP_CODE_STATE_EXC_SP_IDX_SENTINEL) {
                *ret_val = ((mp_obj_gen_instance_native_t *)self)->code_state.state[0];
            } else
            #endif
            {
                *ret_val = self->code_state.state[0];
            }
            
            if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(mp_obj_get_type(*ret_val)), MP_OBJ_FROM_PTR(&mp_type_StopIteration))) {
                *ret_val = mp_obj_new_exception_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("generator raised StopIteration"));
            }
            break;
        }
    }

    return ret_kind;
}

static mp_obj_t gen_resume_and_raise(mp_obj_t self_in, mp_obj_t send_value, mp_obj_t throw_value, bool raise_stop_iteration) {
    mp_obj_t ret;
    switch (mp_obj_gen_resume(self_in, send_value, throw_value, &ret)) {
        case MP_VM_RETURN_NORMAL:
        default:
            
            
            if (ret == mp_const_none) {
                ret = MP_OBJ_NULL;
            }
            if (raise_stop_iteration) {
                mp_raise_StopIteration(ret);
            } else {
                return mp_make_stop_iteration(ret);
            }

        case MP_VM_RETURN_YIELD:
            return ret;

        case MP_VM_RETURN_EXCEPTION:
            nlr_raise(ret);
    }
}

static mp_obj_t gen_instance_iternext(mp_obj_t self_in) {
    return gen_resume_and_raise(self_in, mp_const_none, MP_OBJ_NULL, false);
}

static mp_obj_t gen_instance_send(mp_obj_t self_in, mp_obj_t send_value) {
    return gen_resume_and_raise(self_in, send_value, MP_OBJ_NULL, true);
}
static MP_DEFINE_CONST_FUN_OBJ_2(gen_instance_send_obj, gen_instance_send);

static mp_obj_t gen_instance_throw(size_t n_args, const mp_obj_t *args) {
    
    
    
    
    
    
    
    
    
    
    

    mp_obj_t exc = args[1];
    if (n_args > 2 && args[2] != mp_const_none) {
        exc = args[2];
    }

    return gen_resume_and_raise(args[0], mp_const_none, exc, true);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(gen_instance_throw_obj, 2, 4, gen_instance_throw);

static mp_obj_t gen_instance_close(mp_obj_t self_in) {
    mp_obj_t ret;
    switch (mp_obj_gen_resume(self_in, mp_const_none, MP_OBJ_FROM_PTR(&mp_const_GeneratorExit_obj), &ret)) {
        case MP_VM_RETURN_YIELD:
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("generator ignored GeneratorExit"));

        
        case MP_VM_RETURN_EXCEPTION:
            
            if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(mp_obj_get_type(ret)), MP_OBJ_FROM_PTR(&mp_type_GeneratorExit))) {
                return mp_const_none;
            }
            nlr_raise(ret);

        default:
            
            return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_1(gen_instance_close_obj, gen_instance_close);

#if MICROPY_PY_GENERATOR_PEND_THROW
static mp_obj_t gen_instance_pend_throw(mp_obj_t self_in, mp_obj_t exc_in) {
    mp_obj_gen_instance_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->pend_exc == MP_OBJ_NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("generator already executing"));
    }
    mp_obj_t prev = self->pend_exc;
    self->pend_exc = exc_in;
    return prev;
}
static MP_DEFINE_CONST_FUN_OBJ_2(gen_instance_pend_throw_obj, gen_instance_pend_throw);
#endif

static const mp_rom_map_elem_t gen_instance_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&gen_instance_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&gen_instance_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_throw), MP_ROM_PTR(&gen_instance_throw_obj) },
    #if MICROPY_PY_GENERATOR_PEND_THROW
    { MP_ROM_QSTR(MP_QSTR_pend_throw), MP_ROM_PTR(&gen_instance_pend_throw_obj) },
    #endif
};

static MP_DEFINE_CONST_DICT(gen_instance_locals_dict, gen_instance_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_gen_instance,
    MP_QSTR_generator,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    print, gen_instance_print,
    iter, gen_instance_iternext,
    locals_dict, &gen_instance_locals_dict
    );
