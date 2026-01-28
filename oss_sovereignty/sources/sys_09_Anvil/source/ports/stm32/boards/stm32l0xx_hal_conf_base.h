
#ifndef MICROPY_INCLUDED_STM32L0XX_HAL_CONF_BASE_H
#define MICROPY_INCLUDED_STM32L0XX_HAL_CONF_BASE_H


#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_CRC_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_I2S_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_USART_MODULE_ENABLED
#define HAL_WWDG_MODULE_ENABLED


#define HSI_VALUE   (16000000)
#define HSI48_VALUE (48000000)
#define LSI_VALUE   (37000)
#define MSI_VALUE   (2097152)


#define TICK_INT_PRIORITY (0x00)


#define PREFETCH_ENABLE         1
#define PREREAD_ENABLE          0
#define BUFFER_CACHE_DISABLE    0
#define USE_RTOS                0
#define USE_SPI_CRC             0


#include "stm32l0xx_hal_dma.h"
#include "stm32l0xx_hal_adc.h"
#include "stm32l0xx_hal_cortex.h"
#include "stm32l0xx_hal_crc.h"
#include "stm32l0xx_hal_dac.h"
#include "stm32l0xx_hal_flash.h"
#include "stm32l0xx_hal_gpio.h"
#include "stm32l0xx_hal_i2c.h"
#include "stm32l0xx_hal_i2s.h"
#include "stm32l0xx_hal_iwdg.h"
#include "stm32l0xx_hal_pcd.h"
#include "stm32l0xx_hal_pwr.h"
#include "stm32l0xx_hal_rcc.h"
#include "stm32l0xx_hal_rtc.h"
#include "stm32l0xx_hal_spi.h"
#include "stm32l0xx_hal_tim.h"
#include "stm32l0xx_hal_uart.h"
#include "stm32l0xx_hal_usart.h"
#include "stm32l0xx_hal_wwdg.h"
#include "stm32l0xx_ll_adc.h"
#include "stm32l0xx_ll_lpuart.h"
#include "stm32l0xx_ll_rtc.h"
#include "stm32l0xx_ll_usart.h"


#define assert_param(expr) ((void)0)

#endif 
