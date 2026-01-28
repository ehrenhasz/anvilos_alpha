#define MICROPY_HW_BOARD_NAME       "NUCLEO-F411RE"
#define MICROPY_HW_MCU_NAME         "STM32F411xE"

#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_SERVO     (1)


#define MICROPY_HW_CLK_PLLM (8)
#define MICROPY_HW_CLK_PLLN (192)
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV2)
#define MICROPY_HW_CLK_PLLQ (4)


#define MICROPY_HW_UART2_TX     (pin_A2)
#define MICROPY_HW_UART2_RX     (pin_A3)
#define MICROPY_HW_UART6_TX     (pin_C6)
#define MICROPY_HW_UART6_RX     (pin_C7)


#define MICROPY_HW_UART_REPL        PYB_UART_2
#define MICROPY_HW_UART_REPL_BAUD   115200


#define MICROPY_HW_I2C1_SCL (pin_B8)        
#define MICROPY_HW_I2C1_SDA (pin_B9)        
#define MICROPY_HW_I2C2_SCL (pin_B10)       
#define MICROPY_HW_I2C2_SDA (pin_B3)        
#define MICROPY_HW_I2C3_SCL (pin_A8)        
#define MICROPY_HW_I2C3_SDA (pin_C9)        


#define MICROPY_HW_SPI1_NSS     (pin_A15)   
#define MICROPY_HW_SPI1_SCK     (pin_A5)    
#define MICROPY_HW_SPI1_MISO    (pin_A6)    
#define MICROPY_HW_SPI1_MOSI    (pin_A7)    

#define MICROPY_HW_SPI2_NSS     (pin_B12)   
#define MICROPY_HW_SPI2_SCK     (pin_B13)   
#define MICROPY_HW_SPI2_MISO    (pin_B14)   
#define MICROPY_HW_SPI2_MOSI    (pin_B15)   

#define MICROPY_HW_SPI3_NSS     (pin_A4)    
#define MICROPY_HW_SPI3_SCK     (pin_B3)    
#define MICROPY_HW_SPI3_MISO    (pin_B4)    
#define MICROPY_HW_SPI3_MOSI    (pin_B5)    

#define MICROPY_HW_SPI4_NSS     (pin_B12)   
#define MICROPY_HW_SPI4_SCK     (pin_B13)   
#define MICROPY_HW_SPI4_MISO    (pin_A1)    
#define MICROPY_HW_SPI4_MOSI    (pin_A11)   


#define MICROPY_HW_SPI5_NSS     (pin_B1)    
#define MICROPY_HW_SPI5_SCK     (pin_A10)   
#define MICROPY_HW_SPI5_MISO    (pin_A12)   
#define MICROPY_HW_SPI5_MOSI    (pin_B0)    


#define MICROPY_HW_USRSW_PIN        (pin_C13)
#define MICROPY_HW_USRSW_PULL       (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE  (GPIO_MODE_IT_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)


#define MICROPY_HW_LED1             (pin_A5) 
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))
