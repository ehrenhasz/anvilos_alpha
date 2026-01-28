#define MICROPY_HW_BOARD_NAME       "CustomPCB"
#define MICROPY_HW_MCU_NAME         "STM32F439"

#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_RNG       (1)
#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_DAC       (1)
#define MICROPY_HW_ENABLE_USB       (1)
#define MICROPY_HW_ENABLE_SERVO     (1)
#define MICROPY_HW_ENABLE_SDCARD    (1) 


#if MICROPY_HW_ENABLE_SDCARD
#define MICROPY_HW_SDCARD_DETECT_PIN        (pin_A8)
#define MICROPY_HW_SDCARD_DETECT_PULL       (GPIO_PULLUP)
#define MICROPY_HW_SDCARD_DETECT_PRESENT    (1)
#endif


#define MICROPY_HW_CLK_PLLM (8) 
#define MICROPY_HW_CLK_PLLN (384) 
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV2) 
#define MICROPY_HW_CLK_PLLQ (8) 


#define MICROPY_HW_USB_FS (1)


#define MICROPY_HW_UART1_TX     (pin_A9)
#define MICROPY_HW_UART1_RX     (pin_A10)
#define MICROPY_HW_UART2_TX     (pin_D5)
#define MICROPY_HW_UART2_RX     (pin_D6)
#define MICROPY_HW_UART2_RTS    (pin_D1)
#define MICROPY_HW_UART2_CTS    (pin_D0)
#define MICROPY_HW_UART3_TX     (pin_D8)
#define MICROPY_HW_UART3_RX     (pin_D9)
#define MICROPY_HW_UART3_RTS    (pin_D12)
#define MICROPY_HW_UART3_CTS    (pin_D11)
#define MICROPY_HW_UART4_TX     (pin_A0)
#define MICROPY_HW_UART4_RX     (pin_A1)
#define MICROPY_HW_UART6_TX     (pin_C6)
#define MICROPY_HW_UART6_RX     (pin_C7)


#define MICROPY_HW_I2C1_SCL (pin_A8)
#define MICROPY_HW_I2C1_SDA (pin_C9)


#define MICROPY_HW_SPI1_NSS     (pin_A4)
#define MICROPY_HW_SPI1_SCK     (pin_A5)
#define MICROPY_HW_SPI1_MISO    (pin_A6)
#define MICROPY_HW_SPI1_MOSI    (pin_A7)
#if MICROPY_HW_USB_HS_IN_FS

#else
#define MICROPY_HW_SPI2_NSS  (pin_B12)
#define MICROPY_HW_SPI2_SCK  (pin_B13)
#define MICROPY_HW_SPI2_MISO (pin_B14)
#define MICROPY_HW_SPI2_MOSI (pin_B15)
#endif
#define MICROPY_HW_SPI3_NSS     (pin_E11)
#define MICROPY_HW_SPI3_SCK     (pin_E12)
#define MICROPY_HW_SPI3_MISO    (pin_E13)
#define MICROPY_HW_SPI3_MOSI    (pin_E14)














#define MICROPY_HW_CAN1_TX (pin_B9)
#define MICROPY_HW_CAN1_RX (pin_B8)
#define MICROPY_HW_CAN2_TX (pin_B13)
#define MICROPY_HW_CAN2_RX (pin_B12)


#define MICROPY_HW_USRSW_PIN        (pin_A0)
#define MICROPY_HW_USRSW_PULL       (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE  (GPIO_MODE_IT_RISING)
#define MICROPY_HW_USRSW_PRESSED    (1)
