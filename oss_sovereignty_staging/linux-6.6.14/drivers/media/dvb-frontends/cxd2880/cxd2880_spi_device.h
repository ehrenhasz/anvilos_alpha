 
 

#ifndef CXD2880_SPI_DEVICE_H
#define CXD2880_SPI_DEVICE_H

#include "cxd2880_spi.h"

struct cxd2880_spi_device {
	struct spi_device *spi;
};

int cxd2880_spi_device_initialize(struct cxd2880_spi_device *spi_device,
				  enum cxd2880_spi_mode mode,
				  u32 speedHz);

int cxd2880_spi_device_create_spi(struct cxd2880_spi *spi,
				  struct cxd2880_spi_device *spi_device);

#endif  
