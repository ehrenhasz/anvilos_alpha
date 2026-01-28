









#define MICROPY_HW_BOARD_NAME       "EVK_NINA_B3"
#define MICROPY_HW_MCU_NAME         "NRF52840"
#define MICROPY_PY_SYS_PLATFORM     "nrf52"


#define MICROPY_EMIT_THUMB          (1)
#define MICROPY_EMIT_INLINE_THUMB   (1)


#define MICROPY_PY_ERRNO            (1)
#define MICROPY_PY_HASHLIB          (1)


#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_HW_PWM   (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (1)
#define MICROPY_PY_MACHINE_TEMP     (1)
#define MICROPY_HW_ENABLE_RNG       (1)


#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_LED_COUNT        (3)
#define MICROPY_HW_LED_PULLUP       (1)
#define MICROPY_HW_LED1             (13) 
#define MICROPY_HW_LED2             (25) 
#define MICROPY_HW_LED3             (32) 


#define MICROPY_HW_UART1_RX         (29)
#define MICROPY_HW_UART1_TX         (45)
#define MICROPY_HW_UART1_CTS        (44)
#define MICROPY_HW_UART1_RTS        (31)
#define MICROPY_HW_UART1_HWFC       (1)


#define MICROPY_HW_SPI0_NAME        "SPI0"
#define MICROPY_HW_SPI0_SCK         (12)
#define MICROPY_HW_SPI0_MOSI        (14)
#define MICROPY_HW_SPI0_MISO        (15)


#define MICROPY_HW_PWM0_NAME        "PWM0"
#define MICROPY_HW_PWM1_NAME        "PWM1"
#define MICROPY_HW_PWM2_NAME        "PWM2"


#define HELP_TEXT_BOARD_LED         "1,2,3"
