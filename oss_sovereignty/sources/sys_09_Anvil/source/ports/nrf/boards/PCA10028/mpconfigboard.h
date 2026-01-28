

#define MICROPY_HW_BOARD_NAME       "PCA10028"
#define MICROPY_HW_MCU_NAME         "NRF51822"
#define MICROPY_PY_SYS_PLATFORM     "nrf51-DK"

#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_SOFT_PWM (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (1)
#define MICROPY_PY_MACHINE_TEMP     (1)

#define MICROPY_HW_ENABLE_RNG       (1)

#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_LED_COUNT        (4)
#define MICROPY_HW_LED_PULLUP       (1)

#define MICROPY_HW_LED1             (21) 
#define MICROPY_HW_LED2             (22) 
#define MICROPY_HW_LED3             (23) 
#define MICROPY_HW_LED4             (24) 


#define MICROPY_HW_UART1_RX         (11)
#define MICROPY_HW_UART1_TX         (9)
#define MICROPY_HW_UART1_CTS        (10)
#define MICROPY_HW_UART1_RTS        (8)
#define MICROPY_HW_UART1_HWFC       (1)


#define MICROPY_HW_SPI0_NAME        "SPI0"
#define MICROPY_HW_SPI0_SCK         (29)
#define MICROPY_HW_SPI0_MOSI        (25)
#define MICROPY_HW_SPI0_MISO        (28)

#define HELP_TEXT_BOARD_LED         "1,2,3,4"
