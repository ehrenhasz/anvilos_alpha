

#define MICROPY_HW_BOARD_NAME        "MDK-USB-DONGLE"
#define MICROPY_HW_MCU_NAME          "NRF52840"
#define MICROPY_PY_SYS_PLATFORM      "nrf52840-MDK-USB-Dongle"

#define MICROPY_PY_MACHINE_UART      (1)
#define MICROPY_PY_MACHINE_HW_PWM    (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C       (1)
#define MICROPY_PY_MACHINE_ADC       (1)
#define MICROPY_PY_MACHINE_TEMP      (1)

#define MICROPY_HW_ENABLE_RNG        (1)

#define MICROPY_HW_ENABLE_USBDEV     (1)
#define MICROPY_HW_USB_CDC           (1)

#define MICROPY_HW_HAS_LED           (1)
#define MICROPY_HW_LED_COUNT         (3)
#define MICROPY_HW_LED_PULLUP        (1)

#define MICROPY_HW_LED1              (22)  
#define MICROPY_HW_LED2              (23)  
#define MICROPY_HW_LED3              (24)  


#define MICROPY_HW_UART1_RX          (7)
#define MICROPY_HW_UART1_TX          (8)
#define MICROPY_HW_UART1_CTS         (9)
#define MICROPY_HW_UART1_RTS         (10)
#define MICROPY_HW_UART1_HWFC        (1)


#define MICROPY_HW_SPI0_NAME         "SPI0"

#define MICROPY_HW_SPI0_SCK          (19)
#define MICROPY_HW_SPI0_MOSI         (20)
#define MICROPY_HW_SPI0_MISO         (21)

#define MICROPY_HW_PWM0_NAME         "PWM0"
#define MICROPY_HW_PWM1_NAME         "PWM1"
#define MICROPY_HW_PWM2_NAME         "PWM2"
#define MICROPY_HW_PWM3_NAME         "PWM3"

#define HELP_TEXT_BOARD_LED          "1,2,3"
