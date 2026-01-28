
#ifndef MICROPY_INCLUDED_RP2_MODRP2_H
#define MICROPY_INCLUDED_RP2_MODRP2_H

#include "py/obj.h"

extern const mp_obj_type_t rp2_flash_type;
extern const mp_obj_type_t rp2_pio_type;
extern const mp_obj_type_t rp2_state_machine_type;
extern const mp_obj_type_t rp2_dma_type;

void rp2_pio_init(void);
void rp2_pio_deinit(void);

void rp2_dma_init(void);
void rp2_dma_deinit(void);

#endif 
