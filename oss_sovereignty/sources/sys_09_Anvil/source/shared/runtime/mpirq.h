
#ifndef MICROPY_INCLUDED_LIB_UTILS_MPIRQ_H
#define MICROPY_INCLUDED_LIB_UTILS_MPIRQ_H

#include "py/runtime.h"



enum {
    MP_IRQ_ARG_INIT_handler = 0,
    MP_IRQ_ARG_INIT_trigger,
    MP_IRQ_ARG_INIT_hard,
    MP_IRQ_ARG_INIT_NUM_ARGS,
};



typedef mp_uint_t (*mp_irq_trigger_fun_t)(mp_obj_t self, mp_uint_t trigger);
typedef mp_uint_t (*mp_irq_info_fun_t)(mp_obj_t self, mp_uint_t info_type);

enum {
    MP_IRQ_INFO_FLAGS,
    MP_IRQ_INFO_TRIGGERS,
};

typedef struct _mp_irq_methods_t {
    mp_irq_trigger_fun_t trigger;
    mp_irq_info_fun_t info;
} mp_irq_methods_t;

typedef struct _mp_irq_obj_t {
    mp_obj_base_t base;
    mp_irq_methods_t *methods;
    mp_obj_t parent;
    mp_obj_t handler;
    bool ishard;
} mp_irq_obj_t;



extern const mp_arg_t mp_irq_init_args[];
extern const mp_obj_type_t mp_irq_type;



mp_irq_obj_t *mp_irq_new(const mp_irq_methods_t *methods, mp_obj_t parent);
void mp_irq_init(mp_irq_obj_t *self, const mp_irq_methods_t *methods, mp_obj_t parent);
void mp_irq_handler(mp_irq_obj_t *self);

#endif 
