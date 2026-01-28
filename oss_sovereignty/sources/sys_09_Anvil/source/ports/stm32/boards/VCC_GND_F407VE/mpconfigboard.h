








#define MICROPY_HW_BOARD_NAME       "VCC-GND STM32F407VE"
#define MICROPY_HW_MCU_NAME         "STM32F407VE"
#define MICROPY_HW_FLASH_FS_LABEL   "VCCGNDF407VE"



#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)

#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_RNG       (1)
#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_DAC       (1)
#define MICROPY_HW_ENABLE_USB       (1)
#define MICROPY_HW_ENABLE_SDCARD    (1)


#define MICROPY_HW_CLK_PLLM (25)            
#define MICROPY_HW_CLK_PLLN (336)           
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV2) 
#define MICROPY_HW_CLK_PLLQ (7)             


#define MICROPY_HW_RTC_USE_LSE      (1)
#define MICROPY_HW_RTC_USE_US       (0)



#define MICROPY_HW_UART1_TX     (pin_A9)   
#define MICROPY_HW_UART1_RX     (pin_A10)  


#define MICROPY_HW_UART2_TX     (pin_A2)  
#define MICROPY_HW_UART2_RX     (pin_A3)  
#define MICROPY_HW_UART2_RTS    (pin_A1)  
#define MICROPY_HW_UART2_CTS    (pin_A0)  


#define MICROPY_HW_UART3_TX     (pin_D8)  
#define MICROPY_HW_UART3_RX     (pin_D9)  
#define MICROPY_HW_UART3_RTS    (pin_D12) 
#define MICROPY_HW_UART3_CTS    (pin_D11) 


#define MICROPY_HW_UART4_TX     (pin_A0)  
#define MICROPY_HW_UART4_RX     (pin_A1)  


#define MICROPY_HW_UART5_TX     (pin_C12) 
#define MICROPY_HW_UART5_RX     (pin_D2)  


#define MICROPY_HW_UART6_TX     (pin_C6) 
#define MICROPY_HW_UART6_RX     (pin_C7) 


#define MICROPY_HW_I2C1_SCL     (pin_B6)  
#define MICROPY_HW_I2C1_SDA     (pin_B7)  
#define MICROPY_HW_I2C2_SCL     (pin_B10) 
#define MICROPY_HW_I2C2_SDA     (pin_B11) 
#define MICROPY_HW_I2C3_SCL     (pin_A8)  
#define MICROPY_HW_I2C3_SDA     (pin_C9)  













#define MICROPY_HW_SPI1_NSS     (pin_A4)  
#define MICROPY_HW_SPI1_SCK     (pin_A5)  
#define MICROPY_HW_SPI1_MISO    (pin_A6)  
#define MICROPY_HW_SPI1_MOSI    (pin_A7)  

#define MICROPY_HW_SPI2_NSS     (pin_B12) 
#define MICROPY_HW_SPI2_SCK     (pin_B13) 
#define MICROPY_HW_SPI2_MISO    (pin_B14) 
#define MICROPY_HW_SPI2_MOSI    (pin_B15) 

#define MICROPY_HW_SPI3_NSS     (pin_A15) 
#define MICROPY_HW_SPI3_SCK     (pin_B3)  
#define MICROPY_HW_SPI3_MISO    (pin_B4)  
#define MICROPY_HW_SPI3_MOSI    (pin_B5)  


#define MICROPY_HW_CAN1_TX      (pin_B9)  
#define MICROPY_HW_CAN1_RX      (pin_B8)  
#define MICROPY_HW_CAN2_TX      (pin_B13) 
#define MICROPY_HW_CAN2_RX      (pin_B12) 






#define MICROPY_HW_LED1         (pin_B9) 
#define MICROPY_HW_LED_ON(pin)  (mp_hal_pin_low(pin))
#define MICROPY_HW_LED_OFF(pin) (mp_hal_pin_high(pin))


#if !MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE


#define MICROPY_HW_SPIFLASH_SIZE_BITS (4 * 1024 * 1024) 




#define MICROPY_HW_SPIFLASH_CS      (pin_A4) 
#define MICROPY_HW_SPIFLASH_SCK     (pin_A5)
#define MICROPY_HW_SPIFLASH_MISO    (pin_A6)
#define MICROPY_HW_SPIFLASH_MOSI    (pin_A7)

#define MICROPY_BOARD_EARLY_INIT    VCC_GND_F407VE_board_early_init
void VCC_GND_F407VE_board_early_init(void);

extern const struct _mp_spiflash_config_t spiflash_config;
extern struct _spi_bdev_t spi_bdev;
#define MICROPY_HW_SPIFLASH_ENABLE_CACHE (1)
#define MICROPY_HW_BDEV_SPIFLASH    (&spi_bdev)
#define MICROPY_HW_BDEV_SPIFLASH_CONFIG (&spiflash_config)
#define MICROPY_HW_BDEV_SPIFLASH_SIZE_BYTES (MICROPY_HW_SPIFLASH_SIZE_BITS / 8)

#endif


#define MICROPY_HW_SDCARD_DETECT_PIN        (pin_A8)
#define MICROPY_HW_SDCARD_DETECT_PULL       (GPIO_PULLUP)
#define MICROPY_HW_SDCARD_DETECT_PRESENT    (GPIO_PIN_RESET)












#define MICROPY_HW_USB_FS (1)


