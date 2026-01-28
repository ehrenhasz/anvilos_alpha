#define MICROPY_HW_BOARD_NAME               "NUCLEO_H563ZI"
#define MICROPY_HW_MCU_NAME                 "STM32H563ZI"

#define MICROPY_PY_PYB_LEGACY               (0)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)
#define MICROPY_HW_ENABLE_RTC               (1)
#define MICROPY_HW_ENABLE_RNG               (1)
#define MICROPY_HW_ENABLE_ADC               (1)
#define MICROPY_HW_ENABLE_DAC               (1)
#define MICROPY_HW_ENABLE_USB               (1)
#define MICROPY_HW_HAS_SWITCH               (1)
#define MICROPY_HW_HAS_FLASH                (1)


#define MICROPY_HW_CLK_USE_BYPASS           (1)
#define MICROPY_HW_CLK_PLLM                 (2)
#define MICROPY_HW_CLK_PLLN                 (125)
#define MICROPY_HW_CLK_PLLP                 (2)
#define MICROPY_HW_CLK_PLLQ                 (2)
#define MICROPY_HW_CLK_PLLR                 (2)
#define MICROPY_HW_CLK_PLLVCI_LL            (LL_RCC_PLLINPUTRANGE_4_8)
#define MICROPY_HW_CLK_PLLVCO_LL            (LL_RCC_PLLVCORANGE_WIDE)
#define MICROPY_HW_CLK_PLLFRAC              (0)



#define MICROPY_HW_CLK_PLL3M                (8)
#define MICROPY_HW_CLK_PLL3N                (192)
#define MICROPY_HW_CLK_PLL3P                (2)
#define MICROPY_HW_CLK_PLL3Q                (4)
#define MICROPY_HW_CLK_PLL3R                (2)
#define MICROPY_HW_CLK_PLL3FRAC             (0)
#define MICROPY_HW_CLK_PLL3VCI_LL           (LL_RCC_PLLINPUTRANGE_1_2)
#define MICROPY_HW_CLK_PLL3VCO_LL           (LL_RCC_PLLVCORANGE_MEDIUM)


#define MICROPY_HW_FLASH_LATENCY            FLASH_LATENCY_5


#define MICROPY_HW_RTC_USE_LSE              (1)


#define MICROPY_HW_UART1_TX                 (pin_B6) 
#define MICROPY_HW_UART1_RX                 (pin_B7) 
#define MICROPY_HW_UART3_TX                 (pin_D8) 
#define MICROPY_HW_UART3_RX                 (pin_D9) 


#define MICROPY_HW_UART_REPL                PYB_UART_3
#define MICROPY_HW_UART_REPL_BAUD           115200


#define MICROPY_HW_I2C1_SCL                 (pin_B8) 
#define MICROPY_HW_I2C1_SDA                 (pin_B9) 
#define MICROPY_HW_I2C2_SCL                 (pin_F1) 
#define MICROPY_HW_I2C2_SDA                 (pin_F0) 



#define MICROPY_HW_SPI1_NSS                 (pin_D14) 
#define MICROPY_HW_SPI1_SCK                 (pin_A5) 
#define MICROPY_HW_SPI1_MISO                (pin_G9) 
#define MICROPY_HW_SPI1_MOSI                (pin_B5) 


#define MICROPY_HW_USRSW_PIN                (pin_C13)
#define MICROPY_HW_USRSW_PULL               (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE          (GPIO_MODE_IT_RISING)
#define MICROPY_HW_USRSW_PRESSED            (1)


#define MICROPY_HW_LED1                     (pin_B0) 
#define MICROPY_HW_LED2                     (pin_F4) 
#define MICROPY_HW_LED3                     (pin_G4) 
#define MICROPY_HW_LED_ON(pin)              (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)             (mp_hal_pin_low(pin))


#define MICROPY_HW_USB_FS                   (1)
#define MICROPY_HW_USB_MAIN_DEV             (USB_PHY_FS_ID)


#define MICROPY_HW_ETH_MDC          (pin_C1)
#define MICROPY_HW_ETH_MDIO         (pin_A2)
#define MICROPY_HW_ETH_RMII_REF_CLK (pin_A1)
#define MICROPY_HW_ETH_RMII_CRS_DV  (pin_A7)
#define MICROPY_HW_ETH_RMII_RXD0    (pin_C4)
#define MICROPY_HW_ETH_RMII_RXD1    (pin_C5)
#define MICROPY_HW_ETH_RMII_TX_EN   (pin_G11)
#define MICROPY_HW_ETH_RMII_TXD0    (pin_G13)
#define MICROPY_HW_ETH_RMII_TXD1    (pin_B15)
