#ifndef __GPIO_PUBLIC_H_INCLUDED__
#define __GPIO_PUBLIC_H_INCLUDED__
#include "system_local.h"
STORAGE_CLASS_GPIO_H void gpio_reg_store(
    const gpio_ID_t	ID,
    const unsigned int		reg_addr,
    const hrt_data			value);
STORAGE_CLASS_GPIO_H hrt_data gpio_reg_load(
    const gpio_ID_t	ID,
    const unsigned int		reg_addr);
#endif  
