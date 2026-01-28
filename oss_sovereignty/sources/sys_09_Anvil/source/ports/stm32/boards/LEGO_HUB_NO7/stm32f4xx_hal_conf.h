
#ifndef MICROPY_INCLUDED_STM32F4XX_HAL_CONF_H
#define MICROPY_INCLUDED_STM32F4XX_HAL_CONF_H

#define HAL_FMPI2C_MODULE_ENABLED


#define HSE_VALUE (16000000)
#define LSE_VALUE (32768)
#define EXTERNAL_CLOCK_VALUE (12288000)


#define HSE_STARTUP_TIMEOUT (100)
#define LSE_STARTUP_TIMEOUT (5000)

#include "boards/stm32f4xx_hal_conf_base.h"
#include "stm32f4xx_hal_fmpi2c.h"

#endif 
