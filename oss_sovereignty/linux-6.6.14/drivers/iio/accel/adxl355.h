#ifndef _ADXL355_H_
#define _ADXL355_H_
#include <linux/regmap.h>
enum adxl355_device_type {
	ADXL355,
	ADXL359,
};
struct adxl355_fractional_type {
	int integer;
	int decimal;
};
struct device;
struct adxl355_chip_info {
	const char			*name;
	u8				part_id;
	struct adxl355_fractional_type	accel_scale;
	struct adxl355_fractional_type	temp_offset;
};
extern const struct regmap_access_table adxl355_readable_regs_tbl;
extern const struct regmap_access_table adxl355_writeable_regs_tbl;
extern const struct adxl355_chip_info adxl35x_chip_info[];
int adxl355_core_probe(struct device *dev, struct regmap *regmap,
		       const struct adxl355_chip_info *chip_info);
#endif  
