#define MICROPY_HW_BOARD_NAME               "STM32H573I-DK"
#define MICROPY_HW_MCU_NAME                 "STM32H573IIK3Q"

#define MICROPY_PY_PYB_LEGACY               (0)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (0)
#define MICROPY_HW_ENABLE_RTC               (1)
#define MICROPY_HW_ENABLE_RNG               (1)
#define MICROPY_HW_ENABLE_ADC               (1)
#define MICROPY_HW_ENABLE_DAC               (1)
#define MICROPY_HW_ENABLE_USB               (1)
#define MICROPY_HW_ENABLE_SDCARD            (1)
#define MICROPY_HW_HAS_SWITCH               (1)
#define MICROPY_HW_HAS_FLASH                (1)

#if 1

#define MICROPY_HW_CLK_USE_BYPASS           (1)
#define MICROPY_HW_CLK_PLLM                 (5)
#define MICROPY_HW_CLK_PLLN                 (100)
#define MICROPY_HW_CLK_PLLP                 (2)
#define MICROPY_HW_CLK_PLLQ                 (2)
#define MICROPY_HW_CLK_PLLR                 (2)
#define MICROPY_HW_CLK_PLLVCI_LL            (LL_RCC_PLLINPUTRANGE_4_8)
#define MICROPY_HW_CLK_PLLVCO_LL            (LL_RCC_PLLVCORANGE_WIDE)
#define MICROPY_HW_CLK_PLLFRAC              (0)
#define MICROPY_HW_FLASH_LATENCY            FLASH_LATENCY_5
#else

#define MICROPY_HW_CLK_USE_HSI              (1) 
#define MICROPY_HW_CLK_PLLM                 (16)
#define MICROPY_HW_CLK_PLLN                 (100)
#define MICROPY_HW_CLK_PLLP                 (2)
#define MICROPY_HW_CLK_PLLQ                 (2)
#define MICROPY_HW_CLK_PLLR                 (2)
#define MICROPY_HW_CLK_PLLVCI_LL            (LL_RCC_PLLINPUTRANGE_4_8)
#define MICROPY_HW_CLK_PLLVCO_LL            (LL_RCC_PLLVCORANGE_WIDE)
#define MICROPY_HW_CLK_PLLFRAC              (0)
#define MICROPY_HW_FLASH_LATENCY            FLASH_LATENCY_4 
#endif


#define MICROPY_HW_CLK_PLL3M                (25)
#define MICROPY_HW_CLK_PLL3N                (192)
#define MICROPY_HW_CLK_PLL3P                (2)
#define MICROPY_HW_CLK_PLL3Q                (4)
#define MICROPY_HW_CLK_PLL3R                (2)
#define MICROPY_HW_CLK_PLL3FRAC             (0)
#define MICROPY_HW_CLK_PLL3VCI_LL           (LL_RCC_PLLINPUTRANGE_1_2)
#define MICROPY_HW_CLK_PLL3VCO_LL           (LL_RCC_PLLVCORANGE_MEDIUM)


#define MICROPY_HW_RTC_USE_LSE              (1)


#if !MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE
#define MICROPY_HW_OSPIFLASH_SIZE_BITS_LOG2 (29)
#define MICROPY_HW_OSPIFLASH_CS             (pin_G6)
#define MICROPY_HW_OSPIFLASH_SCK            (pin_F10)
#define MICROPY_HW_OSPIFLASH_IO0            (pin_B1)
#define MICROPY_HW_OSPIFLASH_IO1            (pin_D12)
#define MICROPY_HW_OSPIFLASH_IO2            (pin_C2)
#define MICROPY_HW_OSPIFLASH_IO3            (pin_D13)
#define MICROPY_HW_OSPIFLASH_IO4            (pin_H2)
#define MICROPY_HW_OSPIFLASH_IO5            (pin_H3)
#define MICROPY_HW_OSPIFLASH_IO6            (pin_G9)
#define MICROPY_HW_OSPIFLASH_IO7            (pin_C0)
#define MICROPY_HW_SPIFLASH_ENABLE_CACHE    (1)
#define MICROPY_HW_BDEV_SPIFLASH            (&spi_bdev)
#define MICROPY_HW_BDEV_SPIFLASH_CONFIG     (&spiflash_config)
#define MICROPY_HW_BDEV_SPIFLASH_SIZE_BYTES ((1 << MICROPY_HW_OSPIFLASH_SIZE_BITS_LOG2) / 8)
#define MICROPY_HW_BDEV_SPIFLASH_EXTENDED   (&spi_bdev) 
#endif


#define MICROPY_HW_UART1_TX                 (pin_A9)
#define MICROPY_HW_UART1_RX                 (pin_A10)
#define MICROPY_HW_UART3_TX                 (pin_B10) 
#define MICROPY_HW_UART3_RX                 (pin_B11) 
#define MICROPY_HW_UART_REPL                PYB_UART_1
#define MICROPY_HW_UART_REPL_BAUD           115200


#define MICROPY_HW_I2C1_SCL                 (pin_B6) 
#define MICROPY_HW_I2C1_SDA                 (pin_B7) 


#define MICROPY_HW_SPI2_NSS                 (pin_A3) 
#define MICROPY_HW_SPI2_SCK                 (pin_I1) 
#define MICROPY_HW_SPI2_MISO                (pin_I2) 
#define MICROPY_HW_SPI2_MOSI                (pin_B15) 


#define MICROPY_HW_USRSW_PIN                (pin_C13)
#define MICROPY_HW_USRSW_PULL               (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE          (GPIO_MODE_IT_RISING)
#define MICROPY_HW_USRSW_PRESSED            (1)


#define MICROPY_HW_LED1                     (pin_I9) 
#define MICROPY_HW_LED2                     (pin_I8) 
#define MICROPY_HW_LED3                     (pin_F1) 
#define MICROPY_HW_LED4                     (pin_F4) 
#define MICROPY_HW_LED_ON(pin)              (mp_hal_pin_low(pin))
#define MICROPY_HW_LED_OFF(pin)             (mp_hal_pin_high(pin))


#define MICROPY_HW_USB_FS                   (1)
#define MICROPY_HW_USB_MAIN_DEV             (USB_PHY_FS_ID)


#define MICROPY_HW_SDCARD_DETECT_PIN        (pin_H14)
#define MICROPY_HW_SDCARD_DETECT_PULL       (GPIO_PULLUP)
#define MICROPY_HW_SDCARD_DETECT_PRESENT    (GPIO_PIN_RESET)


#define MICROPY_HW_ETH_MDC                  (pin_C1)
#define MICROPY_HW_ETH_MDIO                 (pin_A2)
#define MICROPY_HW_ETH_RMII_REF_CLK         (pin_A1)
#define MICROPY_HW_ETH_RMII_CRS_DV          (pin_A7)
#define MICROPY_HW_ETH_RMII_RXD0            (pin_C4)
#define MICROPY_HW_ETH_RMII_RXD1            (pin_C5)
#define MICROPY_HW_ETH_RMII_TX_EN           (pin_G11)
#define MICROPY_HW_ETH_RMII_TXD0            (pin_G13)
#define MICROPY_HW_ETH_RMII_TXD1            (pin_G12)




extern const struct _mp_spiflash_config_t spiflash_config;
extern struct _spi_bdev_t spi_bdev;
