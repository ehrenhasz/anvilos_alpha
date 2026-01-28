
#ifndef MICROPY_INCLUDED_STM32_SDIO_H
#define MICROPY_INCLUDED_STM32_SDIO_H

#include <stdbool.h>
#include <stdint.h>

void sdio_init(uint32_t irq_pri);
void sdio_deinit(void);
void sdio_reenable(void);
void sdio_enable_irq(bool enable);
void sdio_enable_high_speed_4bit(void);
int sdio_transfer(uint32_t cmd, uint32_t arg, uint32_t *resp);
int sdio_transfer_cmd53(bool write, uint32_t block_size, uint32_t arg, size_t len, uint8_t *buf);

#endif 
