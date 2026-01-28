

#define WIPY

#define MICROPY_HW_BOARD_NAME                       "WiPy"
#define MICROPY_HW_MCU_NAME                         "CC3200"

#define MICROPY_HW_ANTENNA_DIVERSITY                (1)

#define MICROPY_STDIO_UART                          1
#define MICROPY_STDIO_UART_BAUD                     115200

#define MICROPY_SYS_LED_PRCM                        PRCM_GPIOA3
#define MICROPY_SAFE_BOOT_PRCM                      PRCM_GPIOA3
#define MICROPY_SYS_LED_PORT                        GPIOA3_BASE
#define MICROPY_SAFE_BOOT_PORT                      GPIOA3_BASE
#define MICROPY_SYS_LED_GPIO                        pin_GP25
#define MICROPY_SYS_LED_PIN_NUM                     PIN_21      
#define MICROPY_SAFE_BOOT_PIN_NUM                   PIN_18      
#define MICROPY_SYS_LED_PORT_PIN                    GPIO_PIN_1
#define MICROPY_SAFE_BOOT_PORT_PIN                  GPIO_PIN_4

#define MICROPY_PORT_SFLASH_BLOCK_COUNT             96
