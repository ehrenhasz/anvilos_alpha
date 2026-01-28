
#ifndef MICROPY_INCLUDED_STM32WLXX_HAL_CONF_BASE_H
#define MICROPY_INCLUDED_STM32WLXX_HAL_CONF_BASE_H


#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_USART_MODULE_ENABLED


#define MSI_VALUE (4000000)


#define TICK_INT_PRIORITY (0x00)


#define DATA_CACHE_ENABLE           1
#define INSTRUCTION_CACHE_ENABLE    1
#define PREFETCH_ENABLE             0
#define USE_SPI_CRC                 0
#define USE_RTOS                    0


#include "stm32wlxx_hal_dma.h"
#include "stm32wlxx_hal_adc.h"
#include "stm32wlxx_hal_cortex.h"
#include "stm32wlxx_hal_flash.h"
#include "stm32wlxx_hal_gpio.h"
#include "stm32wlxx_hal_i2c.h"
#include "stm32wlxx_hal_pwr.h"
#include "stm32wlxx_hal_rcc.h"
#include "stm32wlxx_hal_rtc.h"
#include "stm32wlxx_hal_spi.h"
#include "stm32wlxx_hal_tim.h"
#include "stm32wlxx_hal_uart.h"
#include "stm32wlxx_hal_usart.h"
#include "stm32wlxx_ll_adc.h"
#include "stm32wlxx_ll_lpuart.h"
#include "stm32wlxx_ll_rtc.h"
#include "stm32wlxx_ll_usart.h"


#define assert_param(expr) ((void)0)

#endif 
