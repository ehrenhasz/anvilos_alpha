#ifndef _ADXL345_H_
#define _ADXL345_H_
enum adxl345_device_type {
	ADXL345	= 1,
	ADXL375 = 2,
};
int adxl345_core_probe(struct device *dev, struct regmap *regmap);
#endif  
