
#ifndef MICROPY_INCLUDED_DRIVERS_MEMORY_SPIFLASH_H
#define MICROPY_INCLUDED_DRIVERS_MEMORY_SPIFLASH_H

#include "drivers/bus/spi.h"
#include "drivers/bus/qspi.h"

#define MP_SPIFLASH_ERASE_BLOCK_SIZE (4096) 

enum {
    MP_SPIFLASH_BUS_SPI,
    MP_SPIFLASH_BUS_QSPI,
};

struct _mp_spiflash_t;

#if MICROPY_HW_SPIFLASH_ENABLE_CACHE


typedef struct _mp_spiflash_cache_t {
    uint8_t buf[MP_SPIFLASH_ERASE_BLOCK_SIZE] __attribute__((aligned(4)));
    struct _mp_spiflash_t *user; 
    uint32_t block; 
} mp_spiflash_cache_t;
#endif

typedef struct _mp_spiflash_config_t {
    uint32_t bus_kind;
    union {
        struct {
            mp_hal_pin_obj_t cs;
            void *data;
            const mp_spi_proto_t *proto;
        } u_spi;
        struct {
            void *data;
            const mp_qspi_proto_t *proto;
        } u_qspi;
    } bus;
    #if MICROPY_HW_SPIFLASH_ENABLE_CACHE
    mp_spiflash_cache_t *cache; 
    #endif
} mp_spiflash_config_t;

typedef struct _mp_spiflash_t {
    const mp_spiflash_config_t *config;
    volatile uint32_t flags;
} mp_spiflash_t;

void mp_spiflash_init(mp_spiflash_t *self);
void mp_spiflash_deepsleep(mp_spiflash_t *self, int value);


int mp_spiflash_erase_block(mp_spiflash_t *self, uint32_t addr);
int mp_spiflash_read(mp_spiflash_t *self, uint32_t addr, size_t len, uint8_t *dest);
int mp_spiflash_write(mp_spiflash_t *self, uint32_t addr, size_t len, const uint8_t *src);

#if MICROPY_HW_SPIFLASH_ENABLE_CACHE

int mp_spiflash_cache_flush(mp_spiflash_t *self);
int mp_spiflash_cached_read(mp_spiflash_t *self, uint32_t addr, size_t len, uint8_t *dest);
int mp_spiflash_cached_write(mp_spiflash_t *self, uint32_t addr, size_t len, const uint8_t *src);
#endif

#endif 
