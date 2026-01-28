
#ifndef MICROPY_INCLUDED_STM32WBXX_HAL_CONF_BASE_H
#define MICROPY_INCLUDED_STM32WBXX_HAL_CONF_BASE_H


#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
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


#define MSI_VALUE   (4000000)


#define TICK_INT_PRIORITY (0x00)


#define DATA_CACHE_ENABLE           1
#define INSTRUCTION_CACHE_ENABLE    1
#define PREFETCH_ENABLE             0
#define USE_SPI_CRC                 0
#define USE_RTOS                    0


#include "stm32wbxx_hal_dma.h"
#include "stm32wbxx_hal_adc.h"
#include "stm32wbxx_hal_cortex.h"
#include "stm32wbxx_hal_flash.h"
#include "stm32wbxx_hal_gpio.h"
#include "stm32wbxx_hal_i2c.h"
#include "stm32wbxx_hal_pcd.h"
#include "stm32wbxx_hal_pwr.h"
#include "stm32wbxx_hal_rcc.h"
#include "stm32wbxx_hal_rtc.h"
#include "stm32wbxx_hal_spi.h"
#include "stm32wbxx_hal_tim.h"
#include "stm32wbxx_hal_uart.h"
#include "stm32wbxx_hal_usart.h"
#include "stm32wbxx_ll_adc.h"
#include "stm32wbxx_ll_hsem.h"
#include "stm32wbxx_ll_lpuart.h"
#include "stm32wbxx_ll_rtc.h"
#include "stm32wbxx_ll_usart.h"


#define assert_param(expr) ((void)0)






#define CFG_HW_PWR_STANDBY_SEMID                10


#define CFG_HW_THREAD_NVM_SRAM_SEMID            9


#define CFG_HW_BLE_NVM_SRAM_SEMID               8


#define CFG_HW_BLOCK_FLASH_REQ_BY_CPU2_SEMID    7


#define CFG_HW_BLOCK_FLASH_REQ_BY_CPU1_SEMID    6


#define CFG_HW_CLK48_CONFIG_SEMID               5


#define CFG_HW_ENTRY_STOP_MODE_SEMID            4


#define CFG_HW_RCC_SEMID                        3


#define CFG_HW_FLASH_SEMID                      2


#define CFG_HW_PKA_SEMID                        1


#define CFG_HW_RNG_SEMID                        0

#endif 
