



#define MICROPY_HW_BOARD_NAME       "WT51822-S4AT"
#define MICROPY_HW_MCU_NAME         "NRF51822"
#define MICROPY_PY_SYS_PLATFORM     "nrf51"

#define MICROPY_PY_MACHINE_UART     (1)
#define MICROPY_PY_MACHINE_RTCOUNTER (1)
#define MICROPY_PY_MACHINE_I2C      (1)
#define MICROPY_PY_MACHINE_ADC      (1)
#define MICROPY_PY_MACHINE_TEMP     (1)

#define MICROPY_HW_ENABLE_RNG       (1)

#define MICROPY_HW_HAS_LED          (0)


#define MICROPY_HW_UART1_RX         (1)
#define MICROPY_HW_UART1_TX         (2)
#define MICROPY_HW_UART1_HWFC       (0)


#define MICROPY_HW_SPI0_NAME        "SPI0"
#define MICROPY_HW_SPI0_SCK         (9)
#define MICROPY_HW_SPI0_MOSI        (10)
#define MICROPY_HW_SPI0_MISO        (13)
