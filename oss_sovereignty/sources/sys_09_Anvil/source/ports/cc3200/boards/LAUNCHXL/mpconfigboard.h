

#define LAUNCHXL

#define MICROPY_HW_BOARD_NAME                       "LaunchPad"
#define MICROPY_HW_MCU_NAME                         "CC3200"

#define MICROPY_HW_ANTENNA_DIVERSITY                (0)

#define MICROPY_STDIO_UART                          1
#define MICROPY_STDIO_UART_BAUD                     115200

#define MICROPY_SYS_LED_PRCM                        PRCM_GPIOA1
#define MICROPY_SAFE_BOOT_PRCM                      PRCM_GPIOA2
#define MICROPY_SYS_LED_PORT                        GPIOA1_BASE
#define MICROPY_SAFE_BOOT_PORT                      GPIOA2_BASE
#define MICROPY_SYS_LED_GPIO                        pin_GP9
#define MICROPY_SYS_LED_PIN_NUM                     PIN_64      
#define MICROPY_SAFE_BOOT_PIN_NUM                   PIN_15      
#define MICROPY_SYS_LED_PORT_PIN                    GPIO_PIN_1
#define MICROPY_SAFE_BOOT_PORT_PIN                  GPIO_PIN_6

#define MICROPY_PORT_SFLASH_BLOCK_COUNT             32

