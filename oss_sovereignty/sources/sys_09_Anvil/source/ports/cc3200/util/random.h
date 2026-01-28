
#ifndef MICROPY_INCLUDED_CC3200_UTIL_RANDOM_H
#define MICROPY_INCLUDED_CC3200_UTIL_RANDOM_H

void rng_init0 (void);
uint32_t rng_get (void);

MP_DECLARE_CONST_FUN_OBJ_0(machine_rng_get_obj);

#endif 
