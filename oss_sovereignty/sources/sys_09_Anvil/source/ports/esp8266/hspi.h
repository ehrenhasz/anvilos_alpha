

#ifndef SPI_APP_H
#define SPI_APP_H

#include "hspi_register.h"
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"


#define SPI 0
#define HSPI 1

#define SPI_CLK_USE_DIV 0
#define SPI_CLK_80MHZ_NODIV 1

#define SPI_BYTE_ORDER_HIGH_TO_LOW 1
#define SPI_BYTE_ORDER_LOW_TO_HIGH 0

#ifndef CPU_CLK_FREQ 
#define CPU_CLK_FREQ (80 * 1000000)
#endif


#define SPI_CLK_PREDIV 10
#define SPI_CLK_CNTDIV 2
#define SPI_CLK_FREQ (CPU_CLK_FREQ / (SPI_CLK_PREDIV * SPI_CLK_CNTDIV))


void spi_init(uint8_t spi_no);
void spi_mode(uint8_t spi_no, uint8_t spi_cpha, uint8_t spi_cpol);
void spi_init_gpio(uint8_t spi_no, uint8_t sysclk_as_spiclk);
void spi_clock(uint8_t spi_no, uint16_t prediv, uint8_t cntdiv);
void spi_tx_byte_order(uint8_t spi_no, uint8_t byte_order);
void spi_rx_byte_order(uint8_t spi_no, uint8_t byte_order);
uint32_t spi_transaction(uint8_t spi_no, uint8_t cmd_bits, uint16_t cmd_data,
    uint32_t addr_bits, uint32_t addr_data,
    uint32_t dout_bits, uint32_t dout_data,
    uint32_t din_bits, uint32_t dummy_bits);
void spi_tx8fast(uint8_t spi_no, uint8_t dout_data);


#define spi_busy(spi_no) READ_PERI_REG(SPI_CMD(spi_no)) & SPI_USR

#define spi_txd(spi_no, bits, data) spi_transaction(spi_no, 0, 0, 0, 0, bits, (uint32_t)data, 0, 0)
#define spi_tx8(spi_no, data)       spi_transaction(spi_no, 0, 0, 0, 0, 8,    (uint32_t)data, 0, 0)
#define spi_tx16(spi_no, data)      spi_transaction(spi_no, 0, 0, 0, 0, 16,   (uint32_t)data, 0, 0)
#define spi_tx32(spi_no, data)      spi_transaction(spi_no, 0, 0, 0, 0, 32,   (uint32_t)data, 0, 0)

#define spi_rxd(spi_no, bits) spi_transaction(spi_no, 0, 0, 0, 0, 0, 0, bits, 0)
#define spi_rx8(spi_no)       spi_transaction(spi_no, 0, 0, 0, 0, 0, 0, 8,    0)
#define spi_rx16(spi_no)      spi_transaction(spi_no, 0, 0, 0, 0, 0, 0, 16,   0)
#define spi_rx32(spi_no)      spi_transaction(spi_no, 0, 0, 0, 0, 0, 0, 32,   0)

#endif
