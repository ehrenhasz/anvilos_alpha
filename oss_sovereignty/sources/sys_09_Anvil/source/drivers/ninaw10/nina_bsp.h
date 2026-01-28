
#ifndef MICROPY_INCLUDED_DRIVERS_NINAW10_NINA_BSP_H
#define MICROPY_INCLUDED_DRIVERS_NINAW10_NINA_BSP_H

int nina_bsp_init(void);
int nina_bsp_deinit(void);
int nina_bsp_atomic_enter(void);
int nina_bsp_atomic_exit(void);
int nina_bsp_read_irq(void);
int nina_bsp_spi_slave_select(uint32_t timeout);
int nina_bsp_spi_slave_deselect(void);
int nina_bsp_spi_transfer(const uint8_t *tx_buf, uint8_t *rx_buf, uint32_t size);

#endif 
