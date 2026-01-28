#define MICROPY_HW_BOARD_NAME       "NUCLEO_G474RE"
#define MICROPY_HW_MCU_NAME         "STM32G474"

#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_RNG       (1)
#define MICROPY_HW_ENABLE_ADC       (1)
#define MICROPY_HW_ENABLE_DAC       (1) 
#define MICROPY_HW_ENABLE_USB       (0) 
#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_HAS_FLASH        (0) 

#define MICROPY_PY_ASYNCIO          (0)
#define MICROPY_PY_DEFLATE          (0)
#define MICROPY_PY_BINASCII         (0)
#define MICROPY_PY_HASHLIB          (0)
#define MICROPY_PY_JSON             (0)
#define MICROPY_PY_RE               (0)
#define MICROPY_PY_FRAMEBUF         (0)
#define MICROPY_PY_SOCKET           (0)
#define MICROPY_PY_NETWORK          (0)


#define MICROPY_HW_CLK_PLLM         (6)
#define MICROPY_HW_CLK_PLLN         (85)
#define MICROPY_HW_CLK_PLLP         (2)
#define MICROPY_HW_CLK_PLLQ         (8)
#define MICROPY_HW_CLK_PLLR         (2)

#define MICROPY_HW_CLK_USE_HSI48    (1) 


#define MICROPY_HW_FLASH_LATENCY    FLASH_LATENCY_8


#define MICROPY_HW_LPUART1_TX       (pin_A2)  
#define MICROPY_HW_LPUART1_RX       (pin_A3)  
#define MICROPY_HW_UART1_TX         (pin_C4)  
#define MICROPY_HW_UART1_RX         (pin_C5)  




#define MICROPY_HW_UART3_TX         (pin_B10) 
#define MICROPY_HW_UART3_RX         (pin_B11) 





#define MICROPY_HW_UART_REPL        (PYB_LPUART_1) 
#define MICROPY_HW_UART_REPL_BAUD   (115200)


#define MICROPY_HW_I2C1_SCL         (pin_B8) 
#define MICROPY_HW_I2C1_SDA         (pin_B9) 






#define MICROPY_HW_SPI1_NSS         (pin_A4) 
#define MICROPY_HW_SPI1_SCK         (pin_A5) 
#define MICROPY_HW_SPI1_MISO        (pin_A6) 
#define MICROPY_HW_SPI1_MOSI        (pin_A7) 










#define MICROPY_HW_USRSW_PIN        (pin_C13)
#define MICROPY_HW_USRSW_PULL       (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE  (GPIO_MODE_IT_RISING)
#define MICROPY_HW_USRSW_PRESSED    (1)


#define MICROPY_HW_LED1             (pin_A5)    
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))


#define MICROPY_HW_USB_FS              (1)





#define MICROPY_HW_CAN1_NAME        "FDCAN1"
#define MICROPY_HW_CAN1_TX          (pin_A12) 
#define MICROPY_HW_CAN1_RX          (pin_A11) 
