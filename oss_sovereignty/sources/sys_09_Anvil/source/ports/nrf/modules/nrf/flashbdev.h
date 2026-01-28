

#ifndef MICROPY_INCLUDED_NRF_FLASHBDEV_H
#define MICROPY_INCLUDED_NRF_FLASHBDEV_H

#include "py/obj.h"
#include "extmod/vfs_fat.h"

extern const struct _mp_obj_type_t nrf_flashbdev_type;
extern struct _nrf_flash_obj_t nrf_flash_obj;

void flashbdev_init(void);

#endif 
