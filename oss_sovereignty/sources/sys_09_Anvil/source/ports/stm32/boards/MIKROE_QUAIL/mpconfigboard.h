#define MICROPY_HW_BOARD_NAME       "MikroE Quail"
#define MICROPY_HW_MCU_NAME         "STM32F427VI"



#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (0)

#define MICROPY_HW_ENABLE_RNG       (1)
#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_USB       (1)
#define MICROPY_HW_HAS_FLASH        (1)


#define MICROPY_HW_CLK_PLLM         (6)
#define MICROPY_HW_CLK_PLLN         (336)
#define MICROPY_HW_CLK_PLLP         (RCC_PLLP_DIV4)
#define MICROPY_HW_CLK_PLLQ         (14)
#define MICROPY_HW_CLK_LAST_FREQ    (1)


#define MICROPY_HW_RTC_USE_LSE      (0)
#define MICROPY_HW_RTC_USE_US       (0)
#define MICROPY_HW_RTC_USE_CALOUT   (0)  



#define MICROPY_HW_UART3_NAME       "SLOT1"
#define MICROPY_HW_UART3_TX         (pin_D8)
#define MICROPY_HW_UART3_RX         (pin_D9)

#define MICROPY_HW_UART2_NAME       "SLOT2"
#define MICROPY_HW_UART2_TX         (pin_D5)
#define MICROPY_HW_UART2_RX         (pin_D6)

#define MICROPY_HW_UART6_NAME       "SLOT3"
#define MICROPY_HW_UART6_TX         (pin_C6)
#define MICROPY_HW_UART6_RX         (pin_C7)

#define MICROPY_HW_UART1_NAME       "SLOT4"
#define MICROPY_HW_UART1_TX         (pin_A9)
#define MICROPY_HW_UART1_RX         (pin_A10)



#define MICROPY_HW_I2C1_NAME        "SLOT1234H"
#define MICROPY_HW_I2C1_SCL         (pin_B6)
#define MICROPY_HW_I2C1_SDA         (pin_B7)



#define MICROPY_HW_SPI1_NAME        "SLOT12H"
#define MICROPY_HW_SPI1_SCK         (pin_B3)
#define MICROPY_HW_SPI1_MISO        (pin_B4)
#define MICROPY_HW_SPI1_MOSI        (pin_B5)

#define MICROPY_HW_SPI3_NAME        "SLOT34F"
#define MICROPY_HW_SPI3_SCK         (pin_C10)
#define MICROPY_HW_SPI3_MISO        (pin_C11)
#define MICROPY_HW_SPI3_MOSI        (pin_C12)


#define MICROPY_HW_LED1             (pin_E15) 
#define MICROPY_HW_LED2             (pin_E10) 
#define MICROPY_HW_LED3             (pin_C3)  
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))


#define MICROPY_HW_USB_FS           (1)


#if !MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE

#define MICROPY_HW_SPIFLASH_SIZE_BITS (64 * 1024 * 1024) 

#define MICROPY_HW_SPIFLASH_CS (pin_A13)
#define MICROPY_HW_SPIFLASH_SCK (MICROPY_HW_SPI3_SCK)
#define MICROPY_HW_SPIFLASH_MISO (MICROPY_HW_SPI3_MISO)
#define MICROPY_HW_SPIFLASH_MOSI (MICROPY_HW_SPI3_MOSI)

extern const struct _mp_spiflash_config_t spiflash_config;
extern struct _spi_bdev_t spi_bdev;
#define MICROPY_HW_SPIFLASH_ENABLE_CACHE (1)
#define MICROPY_HW_BDEV_SPIFLASH    (&spi_bdev)
#define MICROPY_HW_BDEV_SPIFLASH_CONFIG (&spiflash_config)
#define MICROPY_HW_BDEV_SPIFLASH_SIZE_BYTES (MICROPY_HW_SPIFLASH_SIZE_BITS / 8)
#define MICROPY_HW_BDEV_SPIFLASH_EXTENDED (&spi_bdev) 

#endif 


#define MBOOT_I2C_PERIPH_ID         1
#define MBOOT_I2C_SCL               (pin_B6)
#define MBOOT_I2C_SDA               (pin_B7)
#define MBOOT_I2C_ALTFUNC           (4)
#define MBOOT_FSLOAD                (1)
#define MBOOT_VFS_FAT               (1)

#define MBOOT_SPIFLASH_ADDR         (0x80000000)
#define MBOOT_SPIFLASH_BYTE_SIZE    (8 * 1024 * 1024)
#define MBOOT_SPIFLASH_LAYOUT       "/0x80000000/512*8Kg"
#define MBOOT_SPIFLASH_ERASE_BLOCKS_PER_PAGE \
    (8 / 4)                                 
#define MBOOT_SPIFLASH_CONFIG       (&spiflash_config)
#define MBOOT_SPIFLASH_SPIFLASH     (&spi_bdev.spiflash)
