









#define MICROPY_HW_BOARD_NAME "SparkFun STM32 MicroMod Processor"
#define MICROPY_HW_MCU_NAME "STM32F405RG"



#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (0)

#define MICROPY_HW_HAS_FLASH (1)
#define MICROPY_HW_ENABLE_RNG (1)
#define MICROPY_HW_ENABLE_RTC (1)
#define MICROPY_HW_ENABLE_DAC (1)
#define MICROPY_HW_ENABLE_USB (1)


#if !MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE


#define MICROPY_HW_SPIFLASH_SIZE_BITS (128 * 1024 * 1024)

#define MICROPY_HW_SPIFLASH_CS (pin_C3)
#define MICROPY_HW_SPIFLASH_SCK (pin_C10)
#define MICROPY_HW_SPIFLASH_MOSI (pin_C12)
#define MICROPY_HW_SPIFLASH_MISO (pin_C11)

#define MICROPY_BOARD_EARLY_INIT board_early_init
void board_early_init(void);

extern const struct _mp_spiflash_config_t spiflash_config;
extern struct _spi_bdev_t spi_bdev;
#define MICROPY_HW_SPIFLASH_ENABLE_CACHE (1)
#define MICROPY_HW_BDEV_SPIFLASH (&spi_bdev)
#define MICROPY_HW_BDEV_SPIFLASH_CONFIG (&spiflash_config)
#define MICROPY_HW_BDEV_SPIFLASH_SIZE_BYTES (MICROPY_HW_SPIFLASH_SIZE_BITS / 8)
#define MICROPY_HW_BDEV_SPIFLASH_EXTENDED (&spi_bdev) 

#endif 



#define MICROPY_HW_CLK_PLLM (12)
#define MICROPY_HW_CLK_PLLN (336)
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV2)
#define MICROPY_HW_CLK_PLLQ (7)
#define MICROPY_HW_CLK_LAST_FREQ (1)



#define MICROPY_HW_RTC_USE_LSE (1)
#define MICROPY_HW_RTC_USE_US (0)
#define MICROPY_HW_RTC_USE_CALOUT (1)


#define MICROPY_HW_UART2_NAME "UART1"
#define MICROPY_HW_UART2_TX (pin_A2)
#define MICROPY_HW_UART2_RX (pin_A3)


#define MICROPY_HW_CAN1_NAME "CAN"
#define MICROPY_HW_CAN1_TX (pin_B9)
#define MICROPY_HW_CAN1_RX (pin_B8)


#define MICROPY_HW_I2C1_NAME "I2C1"
#define MICROPY_HW_I2C1_SCL (pin_B6)
#define MICROPY_HW_I2C1_SDA (pin_B7)


#define MICROPY_HW_I2C2_NAME "I2C"
#define MICROPY_HW_I2C2_SCL (pin_B10)
#define MICROPY_HW_I2C2_SDA (pin_B11)


#define MICROPY_HW_SPI1_NAME "SPI"
#define MICROPY_HW_SPI1_NSS (pin_C4)
#define MICROPY_HW_SPI1_SCK (pin_A5)
#define MICROPY_HW_SPI1_MISO (pin_A6)
#define MICROPY_HW_SPI1_MOSI (pin_A7)



#define MICROPY_HW_LED1 (pin_A15)
#define MICROPY_HW_LED_ON(pin) (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin) (mp_hal_pin_low(pin))


#define MICROPY_HW_USB_FS (1)
