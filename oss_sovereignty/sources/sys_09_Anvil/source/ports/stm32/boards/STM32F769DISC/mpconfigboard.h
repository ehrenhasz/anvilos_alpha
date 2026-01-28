



#define MICROPY_HW_BOARD_NAME       "F769DISC"
#define MICROPY_HW_MCU_NAME         "STM32F769"

#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_RNG       (1)
#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_USB       (1)
#define MICROPY_HW_ENABLE_SERVO     (1)
#define MICROPY_HW_ENABLE_SDCARD    (1)

#define MICROPY_BOARD_EARLY_INIT    board_early_init
void board_early_init(void);





#define MICROPY_HW_CLK_PLLM (25)
#define MICROPY_HW_CLK_PLLN (432)
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV2)
#define MICROPY_HW_CLK_PLLQ (9)

#define MICROPY_HW_FLASH_LATENCY    FLASH_LATENCY_7 


#define MICROPY_HW_QSPIFLASH_SIZE_BITS_LOG2 (29)
#define MICROPY_HW_QSPIFLASH_CS     (pin_B6)
#define MICROPY_HW_QSPIFLASH_SCK    (pin_B2)
#define MICROPY_HW_QSPIFLASH_IO0    (pin_C9)
#define MICROPY_HW_QSPIFLASH_IO1    (pin_C10)
#define MICROPY_HW_QSPIFLASH_IO2    (pin_E2)
#define MICROPY_HW_QSPIFLASH_IO3    (pin_D13)


extern const struct _mp_spiflash_config_t spiflash_config;
extern struct _spi_bdev_t spi_bdev;
#if !USE_QSPI_XIP
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (0)
#define MICROPY_HW_SPIFLASH_ENABLE_CACHE (1)
#define MICROPY_HW_BDEV_SPIFLASH    (&spi_bdev)
#define MICROPY_HW_BDEV_SPIFLASH_CONFIG (&spiflash_config)
#define MICROPY_HW_BDEV_SPIFLASH_SIZE_BYTES ((1 << MICROPY_HW_QSPIFLASH_SIZE_BITS_LOG2) / 8)
#define MICROPY_HW_BDEV_SPIFLASH_EXTENDED (&spi_bdev) 
#endif


#define MICROPY_HW_UART1_TX         (pin_A9)
#define MICROPY_HW_UART1_RX         (pin_A10)
#define MICROPY_HW_UART5_TX         (pin_C12)
#define MICROPY_HW_UART5_RX         (pin_D2)
#define MICROPY_HW_UART_REPL        PYB_UART_1
#define MICROPY_HW_UART_REPL_BAUD   115200


#define MICROPY_HW_I2C1_SCL         (pin_B8)
#define MICROPY_HW_I2C1_SDA         (pin_B9)
#define MICROPY_HW_I2C3_SCL         (pin_H7)
#define MICROPY_HW_I2C3_SDA         (pin_H8)


#define MICROPY_HW_SPI2_NSS         (pin_A11)
#define MICROPY_HW_SPI2_SCK         (pin_A12)
#define MICROPY_HW_SPI2_MISO        (pin_B14)
#define MICROPY_HW_SPI2_MOSI        (pin_B15)


#define MICROPY_HW_CAN1_TX          (pin_B9)
#define MICROPY_HW_CAN1_RX          (pin_B8)
#define MICROPY_HW_CAN2_TX          (pin_B13)
#define MICROPY_HW_CAN2_RX          (pin_B12)


#define MICROPY_HW_USRSW_PIN        (pin_A0)
#define MICROPY_HW_USRSW_PULL       (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE  (GPIO_MODE_IT_RISING)
#define MICROPY_HW_USRSW_PRESSED    (1)


#define MICROPY_HW_LED1             (pin_J13) 
#define MICROPY_HW_LED2             (pin_J5) 
#define MICROPY_HW_LED3             (pin_A12) 
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))


#define MICROPY_HW_SDCARD_SDMMC             (2)
#define MICROPY_HW_SDCARD_CK                (pin_D6)
#define MICROPY_HW_SDCARD_CMD               (pin_D7)
#define MICROPY_HW_SDCARD_D0                (pin_G9)
#define MICROPY_HW_SDCARD_D1                (pin_G10)
#define MICROPY_HW_SDCARD_D2                (pin_B3)
#define MICROPY_HW_SDCARD_D3                (pin_B4)
#define MICROPY_HW_SDCARD_DETECT_PIN        (pin_I15)
#define MICROPY_HW_SDCARD_DETECT_PULL       (GPIO_PULLUP)
#define MICROPY_HW_SDCARD_DETECT_PRESENT    (GPIO_PIN_RESET)


#define MICROPY_HW_USB_HS (1)
#define MICROPY_HW_USB_HS_ULPI_NXT  (pin_H4)
#define MICROPY_HW_USB_HS_ULPI_DIR  (pin_I11)


#define MICROPY_HW_ETH_MDC          (pin_C1)
#define MICROPY_HW_ETH_MDIO         (pin_A2)
#define MICROPY_HW_ETH_RMII_REF_CLK (pin_A1)
#define MICROPY_HW_ETH_RMII_CRS_DV  (pin_A7)
#define MICROPY_HW_ETH_RMII_RXD0    (pin_C4)
#define MICROPY_HW_ETH_RMII_RXD1    (pin_C5)
#define MICROPY_HW_ETH_RMII_TX_EN   (pin_G11)
#define MICROPY_HW_ETH_RMII_TXD0    (pin_G13)
#define MICROPY_HW_ETH_RMII_TXD1    (pin_G14)

#if 0








#define MICROPY_HW_SDRAM_SIZE (128 * 1024 * 1024 / 8) 
#define MICROPY_HW_SDRAM_STARTUP_TEST (0)
#define MICROPY_HEAP_START sdram_start()
#define MICROPY_HEAP_END sdram_end()


#define MICROPY_HW_SDRAM_TIMING_TMRD        (2)
#define MICROPY_HW_SDRAM_TIMING_TXSR        (7)
#define MICROPY_HW_SDRAM_TIMING_TRAS        (4)
#define MICROPY_HW_SDRAM_TIMING_TRC         (7)
#define MICROPY_HW_SDRAM_TIMING_TWR         (2)
#define MICROPY_HW_SDRAM_TIMING_TRP         (2)
#define MICROPY_HW_SDRAM_TIMING_TRCD        (2)
#define MICROPY_HW_SDRAM_REFRESH_RATE       (64) 

#define MICROPY_HW_SDRAM_BURST_LENGTH       1
#define MICROPY_HW_SDRAM_CAS_LATENCY        2
#define MICROPY_HW_SDRAM_COLUMN_BITS_NUM    8
#define MICROPY_HW_SDRAM_ROW_BITS_NUM       12
#define MICROPY_HW_SDRAM_MEM_BUS_WIDTH      32
#define MICROPY_HW_SDRAM_INTERN_BANKS_NUM   4
#define MICROPY_HW_SDRAM_CLOCK_PERIOD       2
#define MICROPY_HW_SDRAM_RPIPE_DELAY        0
#define MICROPY_HW_SDRAM_RBURST             (1)
#define MICROPY_HW_SDRAM_WRITE_PROTECTION   (0)
#define MICROPY_HW_SDRAM_AUTOREFRESH_NUM    (8)


#define MICROPY_HW_FMC_SDCKE0  (pyb_pin_FMC_SDCKE0)
#define MICROPY_HW_FMC_SDNE0   (pyb_pin_FMC_SDNE0)
#define MICROPY_HW_FMC_SDCLK   (pyb_pin_FMC_SDCLK)
#define MICROPY_HW_FMC_SDNCAS  (pyb_pin_FMC_SDNCAS)
#define MICROPY_HW_FMC_SDNRAS  (pyb_pin_FMC_SDNRAS)
#define MICROPY_HW_FMC_SDNWE   (pyb_pin_FMC_SDNWE)
#define MICROPY_HW_FMC_BA0     (pyb_pin_FMC_BA0)
#define MICROPY_HW_FMC_BA1     (pyb_pin_FMC_BA1)
#define MICROPY_HW_FMC_NBL0    (pyb_pin_FMC_NBL0)
#define MICROPY_HW_FMC_NBL1    (pyb_pin_FMC_NBL1)
#define MICROPY_HW_FMC_NBL2    (pyb_pin_FMC_NBL2)
#define MICROPY_HW_FMC_NBL3    (pyb_pin_FMC_NBL3)
#define MICROPY_HW_FMC_A0      (pyb_pin_FMC_A0)
#define MICROPY_HW_FMC_A1      (pyb_pin_FMC_A1)
#define MICROPY_HW_FMC_A2      (pyb_pin_FMC_A2)
#define MICROPY_HW_FMC_A3      (pyb_pin_FMC_A3)
#define MICROPY_HW_FMC_A4      (pyb_pin_FMC_A4)
#define MICROPY_HW_FMC_A5      (pyb_pin_FMC_A5)
#define MICROPY_HW_FMC_A6      (pyb_pin_FMC_A6)
#define MICROPY_HW_FMC_A7      (pyb_pin_FMC_A7)
#define MICROPY_HW_FMC_A8      (pyb_pin_FMC_A8)
#define MICROPY_HW_FMC_A9      (pyb_pin_FMC_A9)
#define MICROPY_HW_FMC_A10     (pyb_pin_FMC_A10)
#define MICROPY_HW_FMC_A11     (pyb_pin_FMC_A11)
#define MICROPY_HW_FMC_A12     (pyb_pin_FMC_A12)
#define MICROPY_HW_FMC_D0      (pyb_pin_FMC_D0)
#define MICROPY_HW_FMC_D1      (pyb_pin_FMC_D1)
#define MICROPY_HW_FMC_D2      (pyb_pin_FMC_D2)
#define MICROPY_HW_FMC_D3      (pyb_pin_FMC_D3)
#define MICROPY_HW_FMC_D4      (pyb_pin_FMC_D4)
#define MICROPY_HW_FMC_D5      (pyb_pin_FMC_D5)
#define MICROPY_HW_FMC_D6      (pyb_pin_FMC_D6)
#define MICROPY_HW_FMC_D7      (pyb_pin_FMC_D7)
#define MICROPY_HW_FMC_D8      (pyb_pin_FMC_D8)
#define MICROPY_HW_FMC_D9      (pyb_pin_FMC_D9)
#define MICROPY_HW_FMC_D10     (pyb_pin_FMC_D10)
#define MICROPY_HW_FMC_D11     (pyb_pin_FMC_D11)
#define MICROPY_HW_FMC_D12     (pyb_pin_FMC_D12)
#define MICROPY_HW_FMC_D13     (pyb_pin_FMC_D13)
#define MICROPY_HW_FMC_D14     (pyb_pin_FMC_D14)
#define MICROPY_HW_FMC_D15     (pyb_pin_FMC_D15)
#define MICROPY_HW_FMC_D16     (pyb_pin_FMC_D16)
#define MICROPY_HW_FMC_D17     (pyb_pin_FMC_D17)
#define MICROPY_HW_FMC_D18     (pyb_pin_FMC_D18)
#define MICROPY_HW_FMC_D19     (pyb_pin_FMC_D19)
#define MICROPY_HW_FMC_D20     (pyb_pin_FMC_D20)
#define MICROPY_HW_FMC_D21     (pyb_pin_FMC_D21)
#define MICROPY_HW_FMC_D22     (pyb_pin_FMC_D22)
#define MICROPY_HW_FMC_D23     (pyb_pin_FMC_D23)
#define MICROPY_HW_FMC_D24     (pyb_pin_FMC_D24)
#define MICROPY_HW_FMC_D25     (pyb_pin_FMC_D25)
#define MICROPY_HW_FMC_D26     (pyb_pin_FMC_D26)
#define MICROPY_HW_FMC_D27     (pyb_pin_FMC_D27)
#define MICROPY_HW_FMC_D28     (pyb_pin_FMC_D28)
#define MICROPY_HW_FMC_D29     (pyb_pin_FMC_D29)
#define MICROPY_HW_FMC_D30     (pyb_pin_FMC_D30)
#define MICROPY_HW_FMC_D31     (pyb_pin_FMC_D31)
#endif





#define MBOOT_SPIFLASH_ADDR                     (0x90000000)
#define MBOOT_SPIFLASH_BYTE_SIZE                (512 * 128 * 1024)
#define MBOOT_SPIFLASH_LAYOUT                   "/0x90000000/512*128Kg"
#define MBOOT_SPIFLASH_ERASE_BLOCKS_PER_PAGE    (128 / 4) 
#define MBOOT_SPIFLASH_CONFIG                   (&spiflash_config)
#define MBOOT_SPIFLASH_SPIFLASH                 (&spi_bdev.spiflash)
