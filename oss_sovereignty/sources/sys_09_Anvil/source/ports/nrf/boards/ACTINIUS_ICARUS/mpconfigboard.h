

#define ACTINIUS_ICARUS

#define MICROPY_HW_BOARD_NAME       "Actinius Icarus"
#define MICROPY_HW_MCU_NAME         "NRF9160"
#define MICROPY_PY_SYS_PLATFORM     "nrf9160"

#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (0)
#define MICROPY_PY_MACHINE_TEMP     (0)
#define MICROPY_PY_RANDOM_HW_RNG    (0)

#define MICROPY_MBFS                (0)
#define MICROPY_VFS                 (1)

#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_HAS_FLASH        (0)
#define MICROPY_HW_HAS_SDCARD       (0)
#define MICROPY_HW_HAS_MMA7660      (0)
#define MICROPY_HW_HAS_LIS3DSH      (0)
#define MICROPY_HW_HAS_LCD          (0)
#define MICROPY_HW_ENABLE_RNG       (0)
#define MICROPY_HW_ENABLE_RTC       (0)
#define MICROPY_HW_ENABLE_TIMER     (0)
#define MICROPY_HW_ENABLE_SERVO     (0)
#define MICROPY_HW_ENABLE_DAC       (0)
#define MICROPY_HW_ENABLE_CAN       (0)

#define MICROPY_HW_LED_TRICOLOR     (1)
#define MICROPY_HW_LED_PULLUP       (1)

#define MICROPY_HW_LED_RED          (10) 
#define MICROPY_HW_LED_GREEN        (11) 
#define MICROPY_HW_LED_BLUE         (12) 


#define MICROPY_HW_UART1_RX         (6)
#define MICROPY_HW_UART1_TX         (9)
#define MICROPY_HW_UART1_CTS        (25)
#define MICROPY_HW_UART1_RTS        (7)
#define MICROPY_HW_UART1_HWFC       (1)


#define MICROPY_HW_SPI0_NAME        "SPI0"

#define MICROPY_HW_SPI0_SCK         (20)
#define MICROPY_HW_SPI0_MOSI        (21)
#define MICROPY_HW_SPI0_MISO        (22)

#define HELP_TEXT_BOARD_LED         "1,2,3"
