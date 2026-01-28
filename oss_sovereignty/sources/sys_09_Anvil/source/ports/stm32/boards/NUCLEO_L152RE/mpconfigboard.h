#define MICROPY_HW_BOARD_NAME       "NUCLEO-L152RE"
#define MICROPY_HW_MCU_NAME         "STM32L152xE"

#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_ENABLE_RTC       (1)

#define MICROPY_HW_RTC_USE_LSE      (1)
#define MICROPY_HW_ENABLE_SERVO     (1)
#define MICROPY_HW_ENABLE_DAC       (1)
#define MICROPY_HW_ENABLE_USB       (0)





#define MICROPY_HW_CLK_USE_HSI (1)

#if MICROPY_HW_CLK_USE_HSI
#define MICROPY_HW_CLK_PLLMUL (RCC_CFGR_PLLMUL6)
#define MICROPY_HW_CLK_PLLDIV (RCC_CFGR_PLLDIV3)
#else
#define MICROPY_HW_CLK_PLLMUL (RCC_CFGR_PLLMUL12)
#define MICROPY_HW_CLK_PLLDIV (RCC_CFGR_PLLDIV3)
#endif


#define MICROPY_HW_UART1_TX     (pin_A9)
#define MICROPY_HW_UART1_RX     (pin_A10)
#define MICROPY_HW_UART2_TX     (pin_A2)
#define MICROPY_HW_UART2_RX     (pin_A3)
#define MICROPY_HW_UART3_TX     (pin_B10)
#define MICROPY_HW_UART3_RX     (pin_B11)
#define MICROPY_HW_UART4_TX     (pin_C10)
#define MICROPY_HW_UART4_RX     (pin_C11)
#define MICROPY_HW_UART5_TX     (pin_C12)
#define MICROPY_HW_UART5_RX     (pin_D2)


#define MICROPY_HW_UART_REPL        PYB_UART_2
#define MICROPY_HW_UART_REPL_BAUD   115200


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

#define MICROPY_HW_SPI3_NSS     (pin_A4)    
#define MICROPY_HW_SPI3_SCK     (pin_C10)   
#define MICROPY_HW_SPI3_MISO    (pin_C11)   
#define MICROPY_HW_SPI3_MOSI    (pin_C12)   


#define MICROPY_HW_USRSW_PIN        (pin_C13)
#define MICROPY_HW_USRSW_PULL       (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE  (GPIO_MODE_IT_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)


#define MICROPY_HW_LED1             (pin_A5) 
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))

#define MICROPY_HW_FLASH_LATENCY FLASH_LATENCY_1


#if MICROPY_HW_ENABLE_USB
#if MICROPY_HW_CLK_USE_HSI
#error STM32L152RE cannot use USB feature with HSI clock. Use HSE.
#endif
#define MICROPY_HW_USB_FS (1)
#endif
