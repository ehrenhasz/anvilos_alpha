 

#include <stdio.h>

#include "py/runtime.h"
#include "py/gc.h"
#include "shared/runtime/mpirq.h"

#if MICROPY_ENABLE_SCHEDULER

 

const mp_arg_t mp_irq_init_args[] = {
    { MP_QSTR_handler, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
    { MP_QSTR_trigger, MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_hard, MP_ARG_BOOL, {.u_bool = false} },
};

 

 

mp_irq_obj_t *mp_irq_new(const mp_irq_methods_t *methods, mp_obj_t parent) {
    mp_irq_obj_t *self = m_new0(mp_irq_obj_t, 1);
    mp_irq_init(self, methods, parent);
    return self;
}

void mp_irq_init(mp_irq_obj_t *self, const mp_irq_methods_t *methods, mp_obj_t parent) {
    self->base.type = &mp_irq_type;
    self->methods = (mp_irq_methods_t *)methods;
    self->parent = parent;
    self->handler = mp_const_none;
    self->ishard = false;
}

void mp_irq_handler(mp_irq_obj_t *self) {
    if (self->handler != mp_const_none) {
        if (self->ishard) {
            
            
            
            mp_sched_lock();
            gc_lock();
            nlr_buf_t nlr;
            if (nlr_push(&nlr) == 0) {
                mp_call_function_1(self->handler, self->parent);
                nlr_pop();
            } else {
                
                self->methods->trigger(self->parent, 0);
                self->handler = mp_const_none;
                mp_printf(MICROPY_ERROR_PRINTER, "Uncaught exception in IRQ callback handler\n");
                mp_obj_print_exception(MICROPY_ERROR_PRINTER, MP_OBJ_FROM_PTR(nlr.ret_val));
            }
            gc_unlock();
            mp_sched_unlock();
        } else {
            
            mp_sched_schedule(self->handler, self->parent);
        }
    }
}

 


static mp_obj_t mp_irq_flags(mp_obj_t self_in) {
    mp_irq_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->methods->info(self->parent, MP_IRQ_INFO_FLAGS));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_irq_flags_obj, mp_irq_flags);

static mp_obj_t mp_irq_trigger(size_t n_args, const mp_obj_t *args) {
    mp_irq_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t ret_obj = mp_obj_new_int(self->methods->info(self->parent, MP_IRQ_INFO_TRIGGERS));
    if (n_args == 2) {
        
        self->methods->trigger(self->parent, mp_obj_get_int(args[1]));
    }
    return ret_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_irq_trigger_obj, 1, 2, mp_irq_trigger);

static mp_obj_t mp_irq_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    mp_irq_handler(MP_OBJ_TO_PTR(self_in));
    return mp_const_none;
}

static const mp_rom_map_elem_t mp_irq_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_flags),               MP_ROM_PTR(&mp_irq_flags_obj) },
    { MP_ROM_QSTR(MP_QSTR_trigger),             MP_ROM_PTR(&mp_irq_trigger_obj) },
};
static MP_DEFINE_CONST_DICT(mp_irq_locals_dict, mp_irq_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_irq_type,
    MP_QSTR_irq,
    MP_TYPE_FLAG_NONE,
    call, mp_irq_call,
    locals_dict, &mp_irq_locals_dict
    );

#endif 
