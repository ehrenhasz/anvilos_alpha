

#ifndef MICROPY_INCLUDED_ZEPHYR_MODZEPHYR_H
#define MICROPY_INCLUDED_ZEPHYR_MODZEPHYR_H

#include "py/obj.h"

#ifdef CONFIG_DISK_ACCESS
extern const mp_obj_type_t zephyr_disk_access_type;
#endif

#ifdef CONFIG_FLASH_MAP
extern const mp_obj_type_t zephyr_flash_area_type;
#endif

#endif 
