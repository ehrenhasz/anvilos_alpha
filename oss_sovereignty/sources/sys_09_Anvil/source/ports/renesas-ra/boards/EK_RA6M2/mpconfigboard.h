
#define MICROPY_HW_BOARD_NAME       "EK-RA6M2"
#define MICROPY_HW_MCU_NAME         "RA6M2"
#define MICROPY_HW_MCU_SYSCLK       120000000
#define MICROPY_HW_MCU_PCLK         60000000


#define MICROPY_EMIT_THUMB          (1)
#define MICROPY_EMIT_INLINE_THUMB   (1)
#define MICROPY_PY_BUILTINS_COMPLEX (1)
#define MICROPY_PY_GENERATOR_PEND_THROW (1)
#define MICROPY_PY_MATH             (1)
#define MICROPY_PY_HEAPQ            (1)
#define MICROPY_PY_THREAD           (0) 


#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_RTC_SOURCE       (0)     
#define MICROPY_HW_ENABLE_ADC       (1)
#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)




#define MICROPY_HW_UART0_TX         (pin_P411) 
#define MICROPY_HW_UART0_RX         (pin_P410) 



















#define MICROPY_HW_UART7_TX         (pin_P401) 
#define MICROPY_HW_UART7_RX         (pin_P402) 
#define MICROPY_HW_UART7_CTS        (pin_P403) 


#define MICROPY_HW_UART9_TX         (pin_P602)
#define MICROPY_HW_UART9_RX         (pin_P601)
#define MICROPY_HW_UART9_CTS        (pin_P603)
#define MICROPY_HW_UART_REPL        HW_UART_0
#define MICROPY_HW_UART_REPL_BAUD   115200






#define MICROPY_HW_I2C2_SCL         (pin_P512)
#define MICROPY_HW_I2C2_SDA         (pin_P511)


#define MICROPY_HW_SPI0_SSL         (pin_P103) 
#define MICROPY_HW_SPI0_RSPCK       (pin_P102)
#define MICROPY_HW_SPI0_MISO        (pin_P100)
#define MICROPY_HW_SPI0_MOSI        (pin_P101)
#define MICROPY_HW_SPI1_SSL         (pin_P703) 
#define MICROPY_HW_SPI1_RSPCK       (pin_P702)
#define MICROPY_HW_SPI1_MISO        (pin_P700)
#define MICROPY_HW_SPI1_MOSI        (pin_P701)


#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_USRSW_PIN        (pin_P105)
#define MICROPY_HW_USRSW_PULL       (MP_HAL_PIN_PULL_NONE)
#define MICROPY_HW_USRSW_EXTI_MODE  (MP_HAL_PIN_TRIGGER_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)


#define MICROPY_HW_LED1             (pin_P106)
#define MICROPY_HW_LED_ON(pin)      mp_hal_pin_high(pin)
#define MICROPY_HW_LED_OFF(pin)     mp_hal_pin_low(pin)
#define MICROPY_HW_LED_TOGGLE(pin)  mp_hal_pin_toggle(pin)
