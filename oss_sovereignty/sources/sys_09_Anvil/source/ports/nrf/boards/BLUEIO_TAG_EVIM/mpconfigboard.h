

#define MICROPY_HW_BOARD_NAME       "BLUEIO-TAG-EVIM"
#define MICROPY_HW_MCU_NAME         "NRF52832"
#define MICROPY_PY_SYS_PLATFORM     "BLYST Nano"

#define MICROPY_PY_MACHINE_SOFT_PWM (1)
#define MICROPY_PY_MUSIC            (1)

#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_HW_PWM   (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (1)
#define MICROPY_PY_MACHINE_TEMP     (1)

#define MICROPY_HW_ENABLE_RNG       (1)

#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_LED_COUNT        (4)
#define MICROPY_HW_LED_PULLUP       (1)

#define MICROPY_HW_LED1             (30) 
#define MICROPY_HW_LED1_PULLUP      (0)
#define MICROPY_HW_LED2             (20) 
#define MICROPY_HW_LED3             (19) 
#define MICROPY_HW_LED4             (18) 


#define MICROPY_HW_UART1_RX         (8)
#define MICROPY_HW_UART1_TX         (7)
#define MICROPY_HW_UART1_CTS        (12)
#define MICROPY_HW_UART1_RTS        (11)
#define MICROPY_HW_UART1_HWFC       (1)


#define MICROPY_HW_SPI0_NAME        "SPI0"
#define MICROPY_HW_SPI0_SCK         (23) 
#define MICROPY_HW_SPI0_MOSI        (24) 
#define MICROPY_HW_SPI0_MISO        (25) 

#define MICROPY_HW_PWM0_NAME        "PWM0"
#define MICROPY_HW_PWM1_NAME        "PWM1"
#define MICROPY_HW_PWM2_NAME        "PWM2"


#define MICROPY_HW_MUSIC_PIN        (14)

#define HELP_TEXT_BOARD_LED         "1,2,3,4"
