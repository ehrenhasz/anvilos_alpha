#define MICROPY_HW_BOARD_NAME       "NUCLEO-F091RC"
#define MICROPY_HW_MCU_NAME         "STM32F091RCT6"

#define MICROPY_QSTR_BYTES_IN_HASH  (0)

#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)
#define MICROPY_OPT_COMPUTED_GOTO   (0)
#define MICROPY_PY_BUILTINS_COMPLEX (0)
#define MICROPY_PY_SOCKET           (0)
#define MICROPY_PY_NETWORK          (0)
#define MICROPY_PY_STM              (0)
#define MICROPY_PY_PYB_LEGACY       (0)
#define MICROPY_PY_HEAPQ            (0)
#define MICROPY_PY_FRAMEBUF         (0)

#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_ADC       (1)
#define MICROPY_HW_ENABLE_DAC       (1)
#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_HAS_FLASH        (1)



#define MICROPY_HW_CLK_USE_HSI48    (1) 


#if MICROPY_HW_CLK_USE_HSE
#define MICROPY_HW_CLK_USE_BYPASS   (1) 
#endif


#define MICROPY_HW_RTC_USE_LSE      (1)


#define MICROPY_HW_UART1_TX     (pin_B6)
#define MICROPY_HW_UART1_RX     (pin_B7)
#define MICROPY_HW_UART2_TX     (pin_A2)
#define MICROPY_HW_UART2_RX     (pin_A3)
#define MICROPY_HW_UART3_TX     (pin_C10)
#define MICROPY_HW_UART3_RX     (pin_C11)
#define MICROPY_HW_UART4_TX     (pin_A0)
#define MICROPY_HW_UART4_RX     (pin_A1)
#define MICROPY_HW_UART5_TX     (pin_B3)
#define MICROPY_HW_UART5_RX     (pin_B4)
#define MICROPY_HW_UART6_TX     (pin_C0)
#define MICROPY_HW_UART6_RX     (pin_C1)
#define MICROPY_HW_UART7_TX     (pin_C6)
#define MICROPY_HW_UART7_RX     (pin_C7)
#define MICROPY_HW_UART8_TX     (pin_C2)
#define MICROPY_HW_UART8_RX     (pin_C3)


#define MICROPY_HW_UART_REPL PYB_UART_2
#define MICROPY_HW_UART_REPL_BAUD 115200


#define MICROPY_HW_I2C1_SCL (pin_B8)        
#define MICROPY_HW_I2C1_SDA (pin_B9)        
#define MICROPY_HW_I2C2_SCL (pin_B10)       
#define MICROPY_HW_I2C2_SDA (pin_B11)       


#define MICROPY_HW_SPI1_NSS     (pin_A15)   
#define MICROPY_HW_SPI1_SCK     (pin_A5)    
#define MICROPY_HW_SPI1_MISO    (pin_A6)    
#define MICROPY_HW_SPI1_MOSI    (pin_A7)    
#define MICROPY_HW_SPI2_NSS     (pin_B12)   
#define MICROPY_HW_SPI2_SCK     (pin_B13)   
#define MICROPY_HW_SPI2_MISO    (pin_B14)   
#define MICROPY_HW_SPI2_MOSI    (pin_B15)   


#define MICROPY_HW_USRSW_PIN        (pin_C13)
#define MICROPY_HW_USRSW_PULL       (0)
#define MICROPY_HW_USRSW_EXTI_MODE  (GPIO_MODE_IT_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)


#define MICROPY_HW_LED1             (pin_A5) 
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))
