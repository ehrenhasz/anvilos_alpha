
#ifndef MICROPY_INCLUDED_CC3200_MISC_MPIRQ_H
#define MICROPY_INCLUDED_CC3200_MISC_MPIRQ_H


#define mp_irq_INIT_NUM_ARGS                    4


typedef mp_obj_t (*mp_irq_init_t) (size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
typedef void (*mp_irq_void_method_t) (mp_obj_t self);
typedef int (*mp_irq_int_method_t)  (mp_obj_t self);

typedef struct {
    mp_irq_init_t init;
    mp_irq_void_method_t enable;
    mp_irq_void_method_t disable;
    mp_irq_int_method_t flags;
} mp_irq_methods_t;

typedef struct {
    mp_obj_base_t base;
    mp_obj_t parent;
    mp_obj_t handler;
    mp_irq_methods_t *methods;
    bool isenabled;
} mp_irq_obj_t;


extern const mp_arg_t mp_irq_init_args[];
extern const mp_obj_type_t mp_irq_type;


void mp_irq_init0 (void);
mp_obj_t mp_irq_new (mp_obj_t parent, mp_obj_t handler, const mp_irq_methods_t *methods);
mp_irq_obj_t *mp_irq_find (mp_obj_t parent);
void mp_irq_wake_all (void);
void mp_irq_disable_all (void);
void mp_irq_remove (const mp_obj_t parent);
void mp_irq_handler (mp_obj_t self_in);
uint mp_irq_translate_priority (uint priority);

#endif 
