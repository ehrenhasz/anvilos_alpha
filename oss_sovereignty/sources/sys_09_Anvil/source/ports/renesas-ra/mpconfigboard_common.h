




#include RA_HAL_H





#ifndef MICROPY_PY_RA
#define MICROPY_PY_RA (1)
#endif


#ifndef MICROPY_PY_PYB
#define MICROPY_PY_PYB (1)
#endif


#ifndef MICROPY_PY_PYB_LEGACY
#define MICROPY_PY_PYB_LEGACY (1)
#endif


#ifndef MICROPY_HW_ENTER_BOOTLOADER_VIA_RESET
#define MICROPY_HW_ENTER_BOOTLOADER_VIA_RESET (1)
#endif


#ifndef MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)
#endif


#ifndef MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE_SEGMENT2 (0)
#endif


#ifndef MICROPY_HW_HAS_QSPI_FLASH
#define MICROPY_HW_HAS_QSPI_FLASH (0)
#endif


#ifndef MICROPY_HW_HAS_SDHI_CARD
#define MICROPY_HW_HAS_SDHI_CARD (0)
#endif


#ifndef MICROPY_HW_ENABLE_RTC
#define MICROPY_HW_ENABLE_RTC (0)
#endif


#ifndef MICROPY_HW_ENABLE_RNG
#define MICROPY_HW_ENABLE_RNG (0)
#endif


#ifndef MICROPY_HW_ENABLE_ADC
#define MICROPY_HW_ENABLE_ADC (1)
#endif


#ifndef MICROPY_HW_ENABLE_DAC
#define MICROPY_HW_ENABLE_DAC (0)
#endif


#ifndef MICROPY_HW_ENABLE_DCMI
#define MICROPY_HW_ENABLE_DCMI (0)
#endif


#ifndef MICROPY_HW_ENABLE_SERVO
#define MICROPY_HW_ENABLE_SERVO (0)
#endif


#ifndef MICROPY_HW_HAS_SWITCH
#define MICROPY_HW_HAS_SWITCH (0)
#endif


#ifndef MICROPY_HW_HAS_FLASH
#define MICROPY_HW_HAS_FLASH (0)
#endif


#ifndef MICROPY_HW_HAS_MMA7660
#define MICROPY_HW_HAS_MMA7660 (0)
#endif


#ifndef MICROPY_HW_HAS_LCD
#define MICROPY_HW_HAS_LCD (0)
#endif


#ifndef MICROPY_HW_FLASH_MOUNT_AT_BOOT
#define MICROPY_HW_FLASH_MOUNT_AT_BOOT (MICROPY_HW_ENABLE_STORAGE)
#endif


#ifndef MICROPY_HW_FLASH_FS_LABEL
#define MICROPY_HW_FLASH_FS_LABEL "raflash"
#endif


#ifndef MICROPY_HW_I2C_IS_RESERVED
#define MICROPY_HW_I2C_IS_RESERVED(i2c_id) (false)
#endif


#ifndef MICROPY_HW_SPI_IS_RESERVED
#define MICROPY_HW_SPI_IS_RESERVED(spi_id) (false)
#endif


#ifndef MICROPY_HW_TIM_IS_RESERVED
#define MICROPY_HW_TIM_IS_RESERVED(tim_id) (false)
#endif


#ifndef MICROPY_HW_UART_IS_RESERVED
#define MICROPY_HW_UART_IS_RESERVED(uart_id) (false)
#endif





#ifndef MICROPY_HEAP_START
#define MICROPY_HEAP_START &_heap_start
#endif
#ifndef MICROPY_HEAP_END
#define MICROPY_HEAP_END &_heap_end
#endif


#if defined(RA4M1)

#define MP_HAL_UNIQUE_ID_ADDRESS (0x1ffff7ac)

#define PYB_EXTI_NUM_VECTORS (17)
#define MICROPY_HW_MAX_TIMER (2)
#define MICROPY_HW_MAX_UART (10)
#define MICROPY_HW_MAX_LPUART (0)

#elif defined(RA4M3)

#define MP_HAL_UNIQUE_ID_ADDRESS (0x1ffff7ac)   

#define PYB_EXTI_NUM_VECTORS (17)
#define MICROPY_HW_MAX_TIMER (2)
#define MICROPY_HW_MAX_UART (10)
#define MICROPY_HW_MAX_LPUART (0)

#elif defined(RA4W1)

#define MP_HAL_UNIQUE_ID_ADDRESS (0x1ffff7ac)   

#define PYB_EXTI_NUM_VECTORS (17)
#define MICROPY_HW_MAX_TIMER (2)
#define MICROPY_HW_MAX_UART (10)
#define MICROPY_HW_MAX_LPUART (0)

#elif defined(RA6M1)

#define MP_HAL_UNIQUE_ID_ADDRESS (0x0100A150)   

#define PYB_EXTI_NUM_VECTORS (17)
#define MICROPY_HW_MAX_TIMER (2)
#define MICROPY_HW_MAX_UART (10)
#define MICROPY_HW_MAX_LPUART (0)

#elif defined(RA6M2)

#define MP_HAL_UNIQUE_ID_ADDRESS (0x1ffff7ac)   

#define PYB_EXTI_NUM_VECTORS (17)
#define MICROPY_HW_MAX_TIMER (2)
#define MICROPY_HW_MAX_UART (10)
#define MICROPY_HW_MAX_LPUART (0)

#elif defined(RA6M3)

#define MP_HAL_UNIQUE_ID_ADDRESS (0x1ffff7ac)   

#define PYB_EXTI_NUM_VECTORS (17)
#define MICROPY_HW_MAX_TIMER (2)
#define MICROPY_HW_MAX_UART (10)
#define MICROPY_HW_MAX_LPUART (0)

#elif defined(RA6M5)

#define MP_HAL_UNIQUE_ID_ADDRESS (0x1ffff7ac)   

#define PYB_EXTI_NUM_VECTORS (17)
#define MICROPY_HW_MAX_TIMER (2)
#define MICROPY_HW_MAX_UART (10)
#define MICROPY_HW_MAX_LPUART (0)

#else
#error Unsupported MCU series
#endif



#ifndef MICROPY_HW_RTC_USE_BYPASS
#define MICROPY_HW_RTC_USE_BYPASS (0)
#endif

#if MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE

#define MICROPY_HW_BDEV_IOCTL flash_bdev_ioctl
#define MICROPY_HW_BDEV_READBLOCK flash_bdev_readblock
#define MICROPY_HW_BDEV_WRITEBLOCK flash_bdev_writeblock
#endif


#if defined(MICROPY_HW_BDEV_IOCTL)
#define MICROPY_HW_ENABLE_STORAGE (1)
#else
#define MICROPY_HW_ENABLE_STORAGE (0)
#endif


#if defined(MICROPY_HW_I2C0_SCL) || defined(MICROPY_HW_I2C1_SCL) \
    || defined(MICROPY_HW_I2C2_SCL)
#define MICROPY_PY_MACHINE_I2C (1)
#else
#define MICROPY_PY_MACHINE_I2C (0)
#endif

#if defined(MICROPY_HW_PWM_0A) || defined(MICROPY_HW_PWM_0B) \
    || defined(MICROPY_HW_PWM_1A) || defined(MICROPY_HW_PWM_1B) \
    || defined(MICROPY_HW_PWM_2A) || defined(MICROPY_HW_PWM_2B) \
    || defined(MICROPY_HW_PWM_3A) || defined(MICROPY_HW_PWM_3B) \
    || defined(MICROPY_HW_PWM_4A) || defined(MICROPY_HW_PWM_4B) \
    || defined(MICROPY_HW_PWM_5A) || defined(MICROPY_HW_PWM_5B) \
    || defined(MICROPY_HW_PWM_6A) || defined(MICROPY_HW_PWM_6B) \
    || defined(MICROPY_HW_PWM_7A) || defined(MICROPY_HW_PWM_7B) \
    || defined(MICROPY_HW_PWM_8A) || defined(MICROPY_HW_PWM_8B) \
    || defined(MICROPY_HW_PWM_9A) || defined(MICROPY_HW_PWM_9B) \
    || defined(MICROPY_HW_PWM_10A) || defined(MICROPY_HW_PWM_10B) \
    || defined(MICROPY_HW_PWM_11A) || defined(MICROPY_HW_PWM_11B) \
    || defined(MICROPY_HW_PWM_12A) || defined(MICROPY_HW_PWM_12B) \
    || defined(MICROPY_HW_PWM_13A) || defined(MICROPY_HW_PWM_13B)
#define MICROPY_HW_ENABLE_HW_PWM (1)
#else
#define MICROPY_HW_ENABLE_HW_PWM (0)
#endif

#if defined(MICROPY_HW_DAC0) || defined(MICROPY_HW_DAC1)
#define MICROPY_HW_ENABLE_HW_DAC (1)
#else
#define MICROPY_HW_ENABLE_HW_DAC (0)
#endif


#define MICROPY_PIN_DEFS_PORT_H "pin_defs_ra.h"
