#ifndef __MFD_SIMPLE_MFD_I2C_H
#define __MFD_SIMPLE_MFD_I2C_H
#include <linux/mfd/core.h>
#include <linux/regmap.h>
struct simple_mfd_data {
	const struct regmap_config *regmap_config;
	const struct mfd_cell *mfd_cell;
	size_t mfd_cell_size;
};
#endif  
