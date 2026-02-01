 
#ifndef MICROPY_INCLUDED_DRIVERS_BUS_SPI_H
#define MICROPY_INCLUDED_DRIVERS_BUS_SPI_H

#include "py/mphal.h"

enum {
    MP_SPI_IOCTL_INIT,
    MP_SPI_IOCTL_DEINIT,
};

typedef struct _mp_spi_proto_t {
    int (*ioctl)(void *self, uint32_t cmd);
    void (*transfer)(void *self, size_t len, const uint8_t *src, uint8_t *dest);
} mp_spi_proto_t;

typedef struct _mp_soft_spi_obj_t {
    uint32_t delay_half; 
    uint8_t polarity;
    uint8_t phase;
    mp_hal_pin_obj_t sck;
    mp_hal_pin_obj_t mosi;
    mp_hal_pin_obj_t miso;
} mp_soft_spi_obj_t;

extern const mp_spi_proto_t mp_soft_spi_proto;

int mp_soft_spi_ioctl(void *self, uint32_t cmd);
void mp_soft_spi_transfer(void *self, size_t len, const uint8_t *src, uint8_t *dest);

#endif 
