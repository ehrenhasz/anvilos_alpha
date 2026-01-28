

#define MICROPY_HW_BOARD_NAME       "DVK-BL652"
#define MICROPY_HW_MCU_NAME         "NRF52832"
#define MICROPY_PY_SYS_PLATFORM     "bl652"

#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_HW_PWM   (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (1)
#define MICROPY_PY_MACHINE_TEMP     (1)

#define MICROPY_HW_ENABLE_RNG       (1)

#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_LED_COUNT        (2)
#define MICROPY_HW_LED_PULLUP       (0)

#define MICROPY_HW_LED1             (17) 
#define MICROPY_HW_LED2             (19) 


#define MICROPY_HW_UART1_RX         (8)
#define MICROPY_HW_UART1_TX         (6)
#define MICROPY_HW_UART1_CTS        (7)
#define MICROPY_HW_UART1_RTS        (5)
#define MICROPY_HW_UART1_HWFC       (1)


#define MICROPY_HW_SPI0_NAME        "SPI0"
#define MICROPY_HW_SPI0_SCK         (25)
#define MICROPY_HW_SPI0_MOSI        (23)
#define MICROPY_HW_SPI0_MISO        (24)

#define MICROPY_HW_PWM0_NAME        "PWM0"
#define MICROPY_HW_PWM1_NAME        "PWM1"
#define MICROPY_HW_PWM2_NAME        "PWM2"

#define HELP_TEXT_BOARD_LED         "1,2"
