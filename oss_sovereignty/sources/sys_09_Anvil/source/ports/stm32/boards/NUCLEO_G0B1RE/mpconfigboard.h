#define MICROPY_HW_BOARD_NAME       "NUCLEO-G0B1RE"
#define MICROPY_HW_MCU_NAME         "STM32G0B1xE"

#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_RNG       (0)
#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_DAC       (0)
#define MICROPY_HW_ENABLE_USB       (0) 
#define MICROPY_PY_PYB_LEGACY       (0)

#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)



#define MICROPY_HW_CLK_USE_HSI (1)

#define MICROPY_HW_FLASH_LATENCY    FLASH_LATENCY_2

#if MICROPY_HW_CLK_USE_HSI
#define MICROPY_HW_CLK_PLLM (16)
#else
#define MICROPY_HW_CLK_PLLM (8)
#endif
#define MICROPY_HW_CLK_PLLN (336)
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV4)
#define MICROPY_HW_CLK_PLLQ (7)


#define MICROPY_HW_UART1_TX         (pin_A9)
#define MICROPY_HW_UART1_RX         (pin_A10)

#define MICROPY_HW_UART2_TX         (pin_A2)
#define MICROPY_HW_UART2_RX         (pin_A3)

#define MICROPY_HW_UART3_TX         (pin_C4)
#define MICROPY_HW_UART3_RX         (pin_C5)
#define MICROPY_HW_UART3_RTS        (pin_B14)
#define MICROPY_HW_UART3_CTS        (pin_B13)

#define MICROPY_HW_UART4_TX         (pin_A0)
#define MICROPY_HW_UART4_RX         (pin_A1)

#define MICROPY_HW_UART5_TX         (pin_B0)
#define MICROPY_HW_UART5_RX         (pin_B1)

#define MICROPY_HW_UART6_TX         (pin_C0)
#define MICROPY_HW_UART6_RX         (pin_C1)

#define MICROPY_HW_LPUART1_TX       (pin_C1)
#define MICROPY_HW_LPUART1_RX       (pin_C0)

#define MICROPY_HW_LPUART2_TX       (pin_C6)
#define MICROPY_HW_LPUART2_RX       (pin_C7)

#define MICROPY_HW_UART_REPL        PYB_UART_2
#define MICROPY_HW_UART_REPL_BAUD   115200


#define MICROPY_HW_I2C1_SCL         (pin_B8)
#define MICROPY_HW_I2C1_SDA         (pin_B9)
#define MICROPY_HW_I2C2_SCL         (pin_B10)
#define MICROPY_HW_I2C2_SDA         (pin_B11)


#define MICROPY_HW_SPI1_NSS         (pin_A4)
#define MICROPY_HW_SPI1_SCK         (pin_B3)
#define MICROPY_HW_SPI1_MISO        (pin_B4)
#define MICROPY_HW_SPI1_MOSI        (pin_B5)
#define MICROPY_HW_SPI2_NSS         (pin_B12)
#define MICROPY_HW_SPI2_SCK         (pin_B13)
#define MICROPY_HW_SPI2_MISO        (pin_B14)
#define MICROPY_HW_SPI2_MOSI        (pin_B15)








#define MICROPY_HW_USRSW_PIN        (pin_C13)
#define MICROPY_HW_USRSW_PULL       (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE  (GPIO_MODE_IT_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)


#define MICROPY_HW_LED1             (pin_A5) 
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))


#define MICROPY_HW_USB_FS           (1)
#define MICROPY_HW_USB_MAIN_DEV     (USB_PHY_FS_ID)
#define MICROPY_HW_USB_MSC          (0)
#define MICROPY_HW_USB_HID          (0)
