

#define MICROPY_HW_BOARD_NAME       "XIAO nRF52840 Sense"
#define MICROPY_HW_MCU_NAME         "NRF52840"
#define MICROPY_PY_SYS_PLATFORM     "nrf52"

#define MICROPY_BOARD_EARLY_INIT        XIAO_board_early_init
#define MICROPY_BOARD_DEINIT            XIAO_board_deinit
#define MICROPY_BOARD_ENTER_BOOTLOADER(nargs, args) XIAO_board_enter_bootloader()

#define MICROPY_HW_USB_CDC          (1)
#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_HW_PWM   (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (1)
#define MICROPY_PY_MACHINE_TEMP     (1)
#define MICROPY_HW_HAS_FLASH        (1)

#define MICROPY_HW_ENABLE_RNG       (1)

#define MICROPY_HW_HAS_LED          (1)
#define MICROPY_HW_LED_COUNT        (4)
#define MICROPY_HW_LED_PULLUP       (1)

#define MICROPY_HW_LED1             (17) 
#define MICROPY_HW_LED2             (26) 
#define MICROPY_HW_LED3             (30) 
#define MICROPY_HW_LED4             (6)  


#define MICROPY_HW_UART1_TX         (32 + 11)
#define MICROPY_HW_UART1_RX         (32 + 12)


#define MICROPY_HW_SPI0_NAME        "SPI0"

#define MICROPY_HW_SPI0_SCK         (32 + 13)
#define MICROPY_HW_SPI0_MISO        (32 + 14)
#define MICROPY_HW_SPI0_MOSI        (32 + 15)

#define MICROPY_HW_PWM0_NAME        "PWM0"
#define MICROPY_HW_PWM1_NAME        "PWM1"
#define MICROPY_HW_PWM2_NAME        "PWM2"

#define HELP_TEXT_BOARD_LED         "1,2,3,4"




#define MICROPY_HW_USB_VID          (0x2886)
#define MICROPY_HW_USB_PID          (0x0045)
#define MICROPY_HW_USB_CDC_1200BPS_TOUCH (1)

void XIAO_board_early_init(void);
void XIAO_board_deinit(void);
void XIAO_board_enter_bootloader(void);
