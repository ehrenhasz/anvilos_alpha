

#define MICROPY_HW_BOARD_NAME       "PCA10059"
#define MICROPY_HW_MCU_NAME         "NRF52840"
#define MICROPY_PY_SYS_PLATFORM     "nrf52840-Dongle"

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
#define MICROPY_HW_LED_COUNT        (4)
#define MICROPY_HW_LED_PULLUP       (1)

#define MICROPY_HW_LED1             (6)  
#define MICROPY_HW_LED2             (8)  
#define MICROPY_HW_LED3             (41) 
#define MICROPY_HW_LED4             (12) 


#define MICROPY_HW_UART1_RX         (13)
#define MICROPY_HW_UART1_TX         (15)
#define MICROPY_HW_UART1_CTS        (17)
#define MICROPY_HW_UART1_RTS        (20)
#define MICROPY_HW_UART1_HWFC       (1)


#define MICROPY_HW_SPI0_NAME        "SPI0"

#define MICROPY_HW_SPI0_SCK         (22)
#define MICROPY_HW_SPI0_MOSI        (32)
#define MICROPY_HW_SPI0_MISO        (24)

#define MICROPY_HW_PWM0_NAME        "PWM0"
#define MICROPY_HW_PWM1_NAME        "PWM1"
#define MICROPY_HW_PWM2_NAME        "PWM2"
#if 0
#define MICROPY_HW_PWM3_NAME        "PWM3"
#endif

#define HELP_TEXT_BOARD_LED         "1,2,3,4"
