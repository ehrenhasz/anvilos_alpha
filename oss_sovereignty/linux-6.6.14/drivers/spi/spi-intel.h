 
 

#ifndef SPI_INTEL_H
#define SPI_INTEL_H

#include <linux/platform_data/x86/spi-intel.h>

struct resource;

int intel_spi_probe(struct device *dev, struct resource *mem,
		    const struct intel_spi_boardinfo *info);

#endif  
