#ifndef CXD2880_SPI_H
#define CXD2880_SPI_H
#include "cxd2880_common.h"
enum cxd2880_spi_mode {
	CXD2880_SPI_MODE_0,
	CXD2880_SPI_MODE_1,
	CXD2880_SPI_MODE_2,
	CXD2880_SPI_MODE_3
};
struct cxd2880_spi {
	int (*read)(struct cxd2880_spi *spi, u8 *data,
		    u32 size);
	int (*write)(struct cxd2880_spi *spi, const u8 *data,
		     u32 size);
	int (*write_read)(struct cxd2880_spi *spi,
			  const u8 *tx_data, u32 tx_size,
			  u8 *rx_data, u32 rx_size);
	u32 flags;
	void *user;
};
#endif
