
#define MICROPY_HW_BOARD_NAME                   "Raspberry Pi Pico W"


#define MICROPY_HW_FLASH_STORAGE_BYTES          (848 * 1024)


#define MICROPY_PY_NETWORK 1
#define MICROPY_PY_NETWORK_HOSTNAME_DEFAULT     "PicoW"


#define CYW43_USE_SPI (1)
#define CYW43_LWIP (1)
#define CYW43_GPIO (1)
#define CYW43_SPI_PIO (1)





#define MICROPY_HW_PIN_EXT_COUNT    CYW43_WL_GPIO_COUNT

#define MICROPY_HW_PIN_RESERVED(i) ((i) == CYW43_PIN_WL_HOST_WAKE || (i) == CYW43_PIN_WL_REG_ON)
