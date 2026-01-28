

#define PCA10090

#define MICROPY_HW_BOARD_NAME       "PCA10090"
#define MICROPY_HW_MCU_NAME         "NRF9160"
#define MICROPY_PY_SYS_PLATFORM     "nrf9160-DK"

#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_TIMER_NRF (0)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (0)
#define MICROPY_PY_MACHINE_TEMP     (0)

#define MICROPY_MBFS                (0)

#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_HAS_SWITCH       (0)
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

#define MICROPY_HW_LED_COUNT        (4)
#define MICROPY_HW_LED_PULLUP       (0)

#define MICROPY_HW_LED1             (2) 
#define MICROPY_HW_LED2             (3) 
#define MICROPY_HW_LED3             (4) 
#define MICROPY_HW_LED4             (5) 



#define MICROPY_HW_UART1_RX         (28)
#define MICROPY_HW_UART1_TX         (29)
#define MICROPY_HW_UART1_CTS        (26)
#define MICROPY_HW_UART1_RTS        (27)
#define MICROPY_HW_UART1_HWFC       (1)




#define MICROPY_HW_SPI0_NAME        "SPI0"

#define MICROPY_HW_SPI0_SCK         (13)
#define MICROPY_HW_SPI0_MOSI        (11)
#define MICROPY_HW_SPI0_MISO        (12)

#define HELP_TEXT_BOARD_LED         "1,2,3,4"
