 

#include "drivers/bus/spi.h"

int mp_soft_spi_ioctl(void *self_in, uint32_t cmd) {
    mp_soft_spi_obj_t *self = (mp_soft_spi_obj_t*)self_in;

    switch (cmd) {
        case MP_SPI_IOCTL_INIT:
            mp_hal_pin_write(self->sck, self->polarity);
            mp_hal_pin_output(self->sck);
            mp_hal_pin_output(self->mosi);
            mp_hal_pin_input(self->miso);
            break;

        case MP_SPI_IOCTL_DEINIT:
            break;
    }

    return 0;
}

void mp_soft_spi_transfer(void *self_in, size_t len, const uint8_t *src, uint8_t *dest) {
    mp_soft_spi_obj_t *self = (mp_soft_spi_obj_t*)self_in;
    uint32_t delay_half = self->delay_half;

    

    
    
    
    #ifdef MICROPY_HW_SOFTSPI_MIN_DELAY
    if (delay_half == MICROPY_HW_SOFTSPI_MIN_DELAY) {
        for (size_t i = 0; i < len; ++i) {
            uint8_t data_out = src[i];
            uint8_t data_in = 0;
            for (int j = 0; j < 8; ++j, data_out <<= 1) {
                mp_hal_pin_write(self->mosi, (data_out >> 7) & 1);
                mp_hal_pin_write(self->sck, 1 - self->polarity);
                data_in = (data_in << 1) | mp_hal_pin_read(self->miso);
                mp_hal_pin_write(self->sck, self->polarity);
            }
            if (dest != NULL) {
                dest[i] = data_in;
            }
        }
        return;
    }
    #endif

    for (size_t i = 0; i < len; ++i) {
        uint8_t data_out = src[i];
        uint8_t data_in = 0;
        for (int j = 0; j < 8; ++j, data_out <<= 1) {
            mp_hal_pin_write(self->mosi, (data_out >> 7) & 1);
            if (self->phase == 0) {
                mp_hal_delay_us_fast(delay_half);
                mp_hal_pin_write(self->sck, 1 - self->polarity);
            } else {
                mp_hal_pin_write(self->sck, 1 - self->polarity);
                mp_hal_delay_us_fast(delay_half);
            }
            data_in = (data_in << 1) | mp_hal_pin_read(self->miso);
            if (self->phase == 0) {
                mp_hal_delay_us_fast(delay_half);
                mp_hal_pin_write(self->sck, self->polarity);
            } else {
                mp_hal_pin_write(self->sck, self->polarity);
                mp_hal_delay_us_fast(delay_half);
            }
        }
        if (dest != NULL) {
            dest[i] = data_in;
        }
    }
}

const mp_spi_proto_t mp_soft_spi_proto = {
    .ioctl = mp_soft_spi_ioctl,
    .transfer = mp_soft_spi_transfer,
};
