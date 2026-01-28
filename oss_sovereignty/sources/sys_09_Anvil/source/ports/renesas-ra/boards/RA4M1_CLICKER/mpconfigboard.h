
#define MICROPY_HW_BOARD_NAME       "RA4M1 CLICKER"
#define MICROPY_HW_MCU_NAME         "RA4M1"
#define MICROPY_HW_MCU_SYSCLK       48000000
#define MICROPY_HW_MCU_PCLK         48000000


#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)
#define MICROPY_PY_BUILTINS_COMPLEX (0)
#define MICROPY_PY_GENERATOR_PEND_THROW (0)
#define MICROPY_PY_MATH             (0)
#define MICROPY_PY_HEAPQ            (0)
#define MICROPY_PY_THREAD           (0)


#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_RTC_SOURCE       (0)         
#define MICROPY_HW_ENABLE_ADC       (1)
#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)




#define MICROPY_HW_UART0_TX         (pin_P411) 
#define MICROPY_HW_UART0_RX         (pin_P410) 

#define MICROPY_HW_UART1_TX         (pin_P401) 
#define MICROPY_HW_UART1_RX         (pin_P402) 
#define MICROPY_HW_UART_REPL        HW_UART_1
#define MICROPY_HW_UART_REPL_BAUD   115200




#define MICROPY_HW_I2C1_SCL         (pin_P205)
#define MICROPY_HW_I2C1_SDA         (pin_P206)


#define MICROPY_HW_SPI0_SSL         (pin_P103)
#define MICROPY_HW_SPI0_RSPCK       (pin_P102)
#define MICROPY_HW_SPI0_MISO        (pin_P100)
#define MICROPY_HW_SPI0_MOSI        (pin_P101)


#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_USRSW_PIN        (pin_P304)
#define MICROPY_HW_USRSW_PULL       (MP_HAL_PIN_PULL_NONE)
#define MICROPY_HW_USRSW_EXTI_MODE  (MP_HAL_PIN_TRIGGER_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)


#define MICROPY_HW_LED1             (pin_P409)
#define MICROPY_HW_LED2             (pin_P408)
#define MICROPY_HW_LED_ON(pin)      mp_hal_pin_high(pin)
#define MICROPY_HW_LED_OFF(pin)     mp_hal_pin_low(pin)
#define MICROPY_HW_LED_TOGGLE(pin)  mp_hal_pin_toggle(pin)
