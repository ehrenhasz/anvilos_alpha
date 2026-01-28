




#include "nrf_gpio.h"

enum {
    PORT_A,
    PORT_B,
};

enum {
    AF_FN_UART,
    AF_FN_SPI,
};

enum {
    AF_PIN_TYPE_UART_TX = 0,
    AF_PIN_TYPE_UART_RX,
    AF_PIN_TYPE_UART_CTS,
    AF_PIN_TYPE_UART_RTS,

    AF_PIN_TYPE_SPI_MOSI = 0,
    AF_PIN_TYPE_SPI_MISO,
    AF_PIN_TYPE_SPI_SCK,
    AF_PIN_TYPE_SPI_NSS,
};

#if defined(NRF51) || defined(NRF52_SERIES)
#define PIN_DEFS_PORT_AF_UNION \
    NRF_UART_Type *UART;


#elif defined(NRF91_SERIES)
#define PIN_DEFS_PORT_AF_UNION \
    NRF_UARTE_Type *UART;
#endif

enum {
    PIN_ADC1 = (1 << 0),
};

typedef NRF_GPIO_Type pin_gpio_t;
