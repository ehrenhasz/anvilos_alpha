
#ifndef MICROPY_INCLUDED_STM32H5XX_HAL_CONF_BASE_H
#define MICROPY_INCLUDED_STM32H5XX_HAL_CONF_BASE_H


#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_CRC_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
#define HAL_DCMI_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FDCAN_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_HASH_MODULE_ENABLED
#define HAL_HCD_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_I2S_MODULE_ENABLED
#define HAL_ICACHE_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_SD_MODULE_ENABLED
#define HAL_SDRAM_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_USART_MODULE_ENABLED
#define HAL_WWDG_MODULE_ENABLED


#define CSI_VALUE (4000000)
#define HSI_VALUE (64000000)
#define HSI48_VALUE (48000000)
#define LSI_VALUE (32000)


#define TICK_INT_PRIORITY (0x00)


#define USE_RTOS            0
#define USE_SPI_CRC         1


#include "stm32h5xx_hal_dma.h"
#include "stm32h5xx_hal_rcc.h"
#include "stm32h5xx_hal_adc.h"
#include "stm32h5xx_hal_cortex.h"
#include "stm32h5xx_hal_crc.h"
#include "stm32h5xx_hal_dac.h"
#include "stm32h5xx_hal_dcmi.h"
#include "stm32h5xx_hal_fdcan.h"
#include "stm32h5xx_hal_flash.h"
#include "stm32h5xx_hal_gpio.h"
#include "stm32h5xx_hal_hash.h"
#include "stm32h5xx_hal_hcd.h"
#include "stm32h5xx_hal_i2c.h"
#include "stm32h5xx_hal_i2s.h"
#include "stm32h5xx_hal_icache.h"
#include "stm32h5xx_hal_iwdg.h"
#include "stm32h5xx_hal_pcd.h"
#include "stm32h5xx_hal_pwr.h"
#include "stm32h5xx_hal_rtc.h"
#include "stm32h5xx_hal_sd.h"
#include "stm32h5xx_hal_sdram.h"
#include "stm32h5xx_hal_spi.h"
#include "stm32h5xx_hal_tim.h"
#include "stm32h5xx_hal_uart.h"
#include "stm32h5xx_hal_usart.h"
#include "stm32h5xx_hal_wwdg.h"
#include "stm32h5xx_ll_adc.h"
#include "stm32h5xx_ll_lpuart.h"
#include "stm32h5xx_ll_pwr.h"
#include "stm32h5xx_ll_rcc.h"
#include "stm32h5xx_ll_rtc.h"
#include "stm32h5xx_ll_usart.h"


#define assert_param(expr) ((void)0)

#endif 
