

#define MICROPY_HW_BOARD_NAME       "XENON"
#define MICROPY_HW_MCU_NAME         "NRF52840"
#define MICROPY_PY_SYS_PLATFORM     "PARTICLE-XENON"

#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_HW_PWM   (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (1)
#define MICROPY_PY_MACHINE_TEMP     (1)

#define MICROPY_HW_ENABLE_RNG       (1)

#define MICROPY_HW_ENABLE_USBDEV    (1)
#define MICROPY_HW_USB_CDC          (1)

#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_LED_TRICOLOR     (1)
#define MICROPY_HW_LED_PULLUP       (1)

#define MICROPY_HW_LED_RED          (13) 
#define MICROPY_HW_LED_GREEN        (14) 
#define MICROPY_HW_LED_BLUE         (15) 


#define MICROPY_HW_UART1_RX         (8)
#define MICROPY_HW_UART1_TX         (6)
#define MICROPY_HW_UART1_CTS        (32+2)
#define MICROPY_HW_UART1_RTS        (32+1)
#define MICROPY_HW_UART1_HWFC       (0)


#define MICROPY_HW_SPI0_NAME        "SPI0"

#define MICROPY_HW_SPI0_SCK         (32+15)
#define MICROPY_HW_SPI0_MOSI        (32+13)
#define MICROPY_HW_SPI0_MISO        (32+14)

#define MICROPY_HW_PWM0_NAME        "PWM0"
#define MICROPY_HW_PWM1_NAME        "PWM1"
#define MICROPY_HW_PWM2_NAME        "PWM2"
#if 0
#define MICROPY_HW_PWM3_NAME        "PWM3"
#endif

#define HELP_TEXT_BOARD_LED         "1,2,3"
