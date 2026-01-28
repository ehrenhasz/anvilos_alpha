

#ifndef MICROPY_INCLUDED_RA_SPI_H
#define MICROPY_INCLUDED_RA_SPI_H




#define SPI_TRANSFER_TIMEOUT(len) ((len) + 100)

void spi_init0(void);
void spi_deinit(uint32_t ch);
int spi_find_index(mp_obj_t id);
void spi_transfer(uint32_t ch, uint32_t bits, size_t len, const uint8_t *src, uint8_t *dest, uint32_t timeout);

#endif 
