

#ifndef MICROPY_INCLUDED_STM32G0XX_HAL_CONF_BASE_H
#define MICROPY_INCLUDED_STM32G0XX_HAL_CONF_BASE_H


#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_USART_MODULE_ENABLED


#define HSI_VALUE    (16000000)
#define LSI_VALUE    (32000)
#if defined(STM32G0C1xx) || defined(STM32G0B1xx) || defined(STM32G0B0xx)
#define HSI48_VALUE   48000000
#endif


#define TICK_INT_PRIORITY (0x00)


#define  USE_RTOS                     0
#define  PREFETCH_ENABLE              1
#define  INSTRUCTION_CACHE_ENABLE     1
#define  USE_SPI_CRC                  1
#define  USE_HAL_CRYP_SUSPEND_RESUME  1


#include "stm32g0xx_hal_rcc.h"
#include "stm32g0xx_hal_gpio.h"
#include "stm32g0xx_hal_dma.h"
#include "stm32g0xx_hal_cortex.h"
#include "stm32g0xx_hal_adc.h"
#include "stm32g0xx_hal_adc_ex.h"
#include "stm32g0xx_hal_cec.h"
#include "stm32g0xx_hal_comp.h"
#include "stm32g0xx_hal_crc.h"
#include "stm32g0xx_hal_cryp.h"
#include "stm32g0xx_hal_dac.h"
#include "stm32g0xx_hal_exti.h"
#include "stm32g0xx_hal_fdcan.h"
#include "stm32g0xx_hal_flash.h"
#include "stm32g0xx_hal_i2c.h"
#include "stm32g0xx_hal_i2s.h"
#include "stm32g0xx_hal_irda.h"
#include "stm32g0xx_hal_iwdg.h"
#include "stm32g0xx_hal_lptim.h"
#include "stm32g0xx_hal_pcd.h"
#include "stm32g0xx_hal_hcd.h"
#include "stm32g0xx_hal_pwr.h"
#include "stm32g0xx_hal_rng.h"
#include "stm32g0xx_hal_rtc.h"
#include "stm32g0xx_hal_smartcard.h"
#include "stm32g0xx_hal_smbus.h"
#include "stm32g0xx_hal_spi.h"
#include "stm32g0xx_hal_tim.h"
#include "stm32g0xx_hal_uart.h"
#include "stm32g0xx_hal_usart.h"
#include "stm32g0xx_hal_wwdg.h"
#include "stm32g0xx_ll_lpuart.h"
#include "stm32g0xx_ll_rtc.h"
#include "stm32g0xx_ll_usart.h"


#define assert_param(expr) ((void)0)

#endif 
