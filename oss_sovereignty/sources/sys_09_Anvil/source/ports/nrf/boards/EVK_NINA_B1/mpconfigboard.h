



#define MICROPY_HW_BOARD_NAME       "EVK_NINA_B1"
#define MICROPY_HW_MCU_NAME         "NRF52832"
#define MICROPY_PY_SYS_PLATFORM     "nrf52"

#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_HW_PWM   (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (1)
#define MICROPY_PY_MACHINE_TEMP     (1)

#define MICROPY_HW_ENABLE_RNG       (1)

#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_LED_TRICOLOR     (1)
#define MICROPY_HW_LED_PULLUP       (1)

#define MICROPY_HW_LED_RED          (8)   
#define MICROPY_HW_LED_GREEN        (16)  
#define MICROPY_HW_LED_BLUE         (18)  







#define MICROPY_HW_UART1_RX         (5)
#define MICROPY_HW_UART1_TX         (6)
#define MICROPY_HW_UART1_CTS        (7)
#define MICROPY_HW_UART1_RTS        (31)
#define MICROPY_HW_UART1_HWFC       (1)


#define MICROPY_HW_SPI0_NAME        "SPI0"
#define MICROPY_HW_SPI0_SCK         (14)
#define MICROPY_HW_SPI0_MOSI        (13)
#define MICROPY_HW_SPI0_MISO        (12)

#define MICROPY_HW_PWM0_NAME        "PWM0"
#define MICROPY_HW_PWM1_NAME        "PWM1"
#define MICROPY_HW_PWM2_NAME        "PWM2"

#define HELP_TEXT_BOARD_LED         "1,2,3"
