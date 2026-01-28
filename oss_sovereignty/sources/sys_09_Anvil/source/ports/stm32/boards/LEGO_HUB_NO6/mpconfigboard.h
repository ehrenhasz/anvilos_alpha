

#include <stdint.h>

#define MICROPY_HW_BOARD_NAME                    "LEGO Technic Hub No.6"
#define MICROPY_HW_MCU_NAME                      "STM32F413"

#define MICROPY_HW_HAS_SWITCH                    (0)
#define MICROPY_HW_HAS_FLASH                     (1)
#define MICROPY_PY_PYB_LEGACY                    (0)
#define MICROPY_HW_ENTER_BOOTLOADER_VIA_RESET    (0)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (0)
#define MICROPY_HW_ENABLE_RTC                    (1)
#define MICROPY_HW_ENABLE_RNG                    (1)
#define MICROPY_HW_ENABLE_DAC                    (1)
#define MICROPY_HW_ENABLE_USB                    (1)
#define MICROPY_HW_FLASH_FS_LABEL                "HUB_NO6"


#define MICROPY_HW_CLK_PLLM                      (16)
#define MICROPY_HW_CLK_PLLN                      (200)
#define MICROPY_HW_CLK_PLLP                      (RCC_PLLP_DIV2)
#define MICROPY_HW_CLK_PLLQ                      (4)
#define MICROPY_HW_CLK_AHB_DIV                   (RCC_SYSCLK_DIV1)
#define MICROPY_HW_CLK_APB1_DIV                  (RCC_HCLK_DIV2)
#define MICROPY_HW_CLK_APB2_DIV                  (RCC_HCLK_DIV1)


#define MICROPY_HW_FLASH_LATENCY                 FLASH_LATENCY_3



#define MICROPY_HW_UART2_CTS                     (pyb_pin_BT_CTS)
#define MICROPY_HW_UART2_RTS                     (pyb_pin_BT_RTS)
#define MICROPY_HW_UART2_TX                      (pyb_pin_BT_TX)
#define MICROPY_HW_UART2_RX                      (pyb_pin_BT_RX)

#define MICROPY_HW_UART4_TX                      (pyb_pin_PORTB_TX)
#define MICROPY_HW_UART4_RX                      (pyb_pin_PORTB_RX)

#define MICROPY_HW_UART5_TX                      (pyb_pin_PORTD_TX)
#define MICROPY_HW_UART5_RX                      (pyb_pin_PORTD_RX)

#define MICROPY_HW_UART7_TX                      (pyb_pin_PORTA_TX)
#define MICROPY_HW_UART7_RX                      (pyb_pin_PORTA_RX)

#define MICROPY_HW_UART8_TX                      (pyb_pin_PORTC_TX)
#define MICROPY_HW_UART8_RX                      (pyb_pin_PORTC_RX)

#define MICROPY_HW_UART9_TX                      (pyb_pin_PORTF_TX)
#define MICROPY_HW_UART9_RX                      (pyb_pin_PORTF_RX)

#define MICROPY_HW_UART10_TX                     (pyb_pin_PORTE_TX)
#define MICROPY_HW_UART10_RX                     (pyb_pin_PORTE_RX)


#define MICROPY_HW_SPI1_SCK                      (pyb_pin_TLC_SCLK)
#define MICROPY_HW_SPI1_MISO                     (pyb_pin_TLC_SOUT)
#define MICROPY_HW_SPI1_MOSI                     (pyb_pin_TLC_SIN)
#define MICROPY_HW_SPI2_NSS                      (pyb_pin_FLASH_NSS)
#define MICROPY_HW_SPI2_SCK                      (pyb_pin_FLASH_SCK)
#define MICROPY_HW_SPI2_MISO                     (pyb_pin_FLASH_MISO)
#define MICROPY_HW_SPI2_MOSI                     (pyb_pin_FLASH_MOSI)


#define MICROPY_HW_USB_VBUS_DETECT_PIN           (pyb_pin_USB_VBUS)
#define MICROPY_HW_USB_FS                        (1)
#define MICROPY_HW_USB_MSC                       (1)


#define MICROPY_HW_BLE_UART_ID                   (PYB_UART_2)
#define MICROPY_HW_BLE_UART_BAUDRATE             (115200)
#define MICROPY_HW_BLE_UART_BAUDRATE_SECONDARY   (921600)
#define MICROPY_HW_BLE_BTSTACK_CHIPSET_INSTANCE  btstack_chipset_cc256x_instance()



#define MICROPY_HW_SPI_ADDR_IS_32BIT(addr)       (1)




#define MICROPY_HW_SPIFLASH_OFFSET_BYTES         (1024 * 1024)
#define MICROPY_HW_SPIFLASH_BLOCKMAP(bl)         ((bl) + MICROPY_HW_SPIFLASH_OFFSET_BYTES / FLASH_BLOCK_SIZE)
#define MICROPY_HW_SPIFLASH_BLOCKMAP_EXT(bl)     ((bl) + MICROPY_HW_SPIFLASH_OFFSET_BYTES / MP_SPIFLASH_ERASE_BLOCK_SIZE)
#define MICROPY_HW_SPIFLASH_ENABLE_CACHE         (1)
#define MICROPY_HW_SPIFLASH_SIZE_BITS            (256 * 1024 * 1024 - MICROPY_HW_SPIFLASH_OFFSET_BYTES * 8)
#define MICROPY_HW_SPIFLASH_CS                   (MICROPY_HW_SPI2_NSS)
#define MICROPY_HW_SPIFLASH_SCK                  (MICROPY_HW_SPI2_SCK)
#define MICROPY_HW_SPIFLASH_MISO                 (MICROPY_HW_SPI2_MISO)
#define MICROPY_HW_SPIFLASH_MOSI                 (MICROPY_HW_SPI2_MOSI)


#define MICROPY_HW_BDEV_IOCTL(op, arg) ( \
    (op) == BDEV_IOCTL_NUM_BLOCKS ? (MICROPY_HW_SPIFLASH_SIZE_BITS / 8 / FLASH_BLOCK_SIZE) : \
    (op) == BDEV_IOCTL_INIT ? spi_bdev_ioctl(&spi_bdev, (op), (uint32_t)&spiflash_config) : \
    spi_bdev_ioctl(&spi_bdev, (op), (arg)) \
    )


#define MICROPY_HW_BDEV_READBLOCKS(dest, bl, n) \
    spi_bdev_readblocks(&spi_bdev, (dest), MICROPY_HW_SPIFLASH_BLOCKMAP(bl), (n))
#define MICROPY_HW_BDEV_WRITEBLOCKS(src, bl, n) \
    spi_bdev_writeblocks(&spi_bdev, (src), MICROPY_HW_SPIFLASH_BLOCKMAP(bl), (n))


#define MICROPY_HW_BDEV_BLOCKSIZE_EXT (MP_SPIFLASH_ERASE_BLOCK_SIZE)
#define MICROPY_HW_BDEV_READBLOCKS_EXT(dest, bl, off, len) \
    (spi_bdev_readblocks_raw(&spi_bdev, (dest), MICROPY_HW_SPIFLASH_BLOCKMAP_EXT(bl), (off), (len)))
#define MICROPY_HW_BDEV_WRITEBLOCKS_EXT(src, bl, off, len) \
    (spi_bdev_writeblocks_raw(&spi_bdev, (src), MICROPY_HW_SPIFLASH_BLOCKMAP_EXT(bl), (off), (len)))
#define MICROPY_HW_BDEV_ERASEBLOCKS_EXT(bl, len) \
    (spi_bdev_eraseblocks_raw(&spi_bdev, MICROPY_HW_SPIFLASH_BLOCKMAP_EXT(bl), (len)))


#define MICROPY_BOARD_STARTUP                    board_init





#define MBOOT_CLK_PLLM                           (MICROPY_HW_CLK_VALUE / 1000000)
#define MBOOT_CLK_PLLN                           (192)
#define MBOOT_CLK_PLLP                           (RCC_PLLP_DIV2)
#define MBOOT_CLK_PLLQ                           (4)
#define MBOOT_CLK_AHB_DIV                        (RCC_SYSCLK_DIV1)
#define MBOOT_CLK_APB1_DIV                       (RCC_HCLK_DIV4)
#define MBOOT_CLK_APB2_DIV                       (RCC_HCLK_DIV2)
#define MBOOT_FLASH_LATENCY                      FLASH_LATENCY_3

#define MBOOT_FSLOAD                             (1)
#define MBOOT_VFS_FAT                            (1)
#define MBOOT_LEAVE_BOOTLOADER_VIA_RESET         (0)

#define MBOOT_SPIFLASH_ADDR                      (0x80000000)
#define MBOOT_SPIFLASH_BYTE_SIZE                 (32 * 1024 * 1024)
#define MBOOT_SPIFLASH_LAYOUT                    "/0x80000000/8192*4Kg"
#define MBOOT_SPIFLASH_ERASE_BLOCKS_PER_PAGE     (1)
#define MBOOT_SPIFLASH_SPIFLASH                  (&board_mboot_spiflash)
#define MBOOT_SPIFLASH_CONFIG                    (&board_mboot_spiflash_config)

#define MBOOT_LED1                               0
#define MBOOT_LED2                               1
#define MBOOT_LED3                               2
#define MBOOT_BOARD_LED_INIT                     board_mboot_led_init
#define MBOOT_BOARD_LED_STATE                    board_mboot_led_state

#define MBOOT_BOARD_EARLY_INIT(initial_r0)       board_init()
#define MBOOT_BOARD_CLEANUP                      board_mboot_cleanup
#define MBOOT_BOARD_GET_RESET_MODE               board_mboot_get_reset_mode
#define MBOOT_BOARD_STATE_CHANGE                 board_mboot_state_change




extern const struct _mp_spiflash_config_t spiflash_config;
extern struct _spi_bdev_t spi_bdev;
extern const struct _mp_spiflash_config_t board_mboot_spiflash_config;
extern struct _mp_spiflash_t board_mboot_spiflash;

void board_init(void);
void board_mboot_cleanup(int reset_mode);
void board_mboot_led_init(void);
void board_mboot_led_state(int led, int state);
int board_mboot_get_reset_mode(uint32_t *initial_r0);
void board_mboot_state_change(int state, uint32_t arg);
void *btstack_chipset_cc256x_instance(void);
