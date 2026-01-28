

#define MICROPY_HW_BOARD_NAME       "NUCLEO-L073RZ"
#define MICROPY_HW_MCU_NAME         "STM32L073RZT6"

#define MICROPY_QSTR_BYTES_IN_HASH  (0)

#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)
#define MICROPY_OPT_COMPUTED_GOTO   (0)
#define MICROPY_PY_BUILTINS_COMPLEX (0)
#define MICROPY_PY_GENERATOR_PEND_THROW (0)
#define MICROPY_PY_MATH             (0)
#define MICROPY_PY_FRAMEBUF         (0)
#define MICROPY_PY_SOCKET           (0)
#define MICROPY_PY_NETWORK          (0)
#define MICROPY_PY_ONEWIRE          (0)
#define MICROPY_PY_STM              (0)
#define MICROPY_PY_PYB_LEGACY       (0)
#define MICROPY_PY_HEAPQ            (0)

#define MICROPY_PY_MACHINE_BITSTREAM (0)

#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (0)
#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_ADC       (0)
#define MICROPY_HW_HAS_SWITCH       (1)


#define MICROPY_HW_UART1_TX     (pin_B6)
#define MICROPY_HW_UART1_RX     (pin_B7)
#define MICROPY_HW_UART2_TX     (pin_A2)
#define MICROPY_HW_UART2_RX     (pin_A3)


#define MICROPY_HW_UART_REPL PYB_UART_2
#define MICROPY_HW_UART_REPL_BAUD 115200


#define MICROPY_HW_I2C1_SCL (pin_B8)        
#define MICROPY_HW_I2C1_SDA (pin_B9)        


#define MICROPY_HW_SPI1_NSS     (pin_A15)   
#define MICROPY_HW_SPI1_SCK     (pin_A5)    
#define MICROPY_HW_SPI1_MISO    (pin_A6)    
#define MICROPY_HW_SPI1_MOSI    (pin_A7)    


#define MICROPY_HW_USRSW_PIN        (pin_C13)
#define MICROPY_HW_USRSW_PULL       (0)
#define MICROPY_HW_USRSW_EXTI_MODE  (GPIO_MODE_IT_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)


#define MICROPY_HW_LED1             (pin_A5) 
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))
