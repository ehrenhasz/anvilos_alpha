


#ifndef _ADXL372_H_
#define _ADXL372_H_

#define ADXL372_REVID	0x03

int adxl372_probe(struct device *dev, struct regmap *regmap,
		  int irq, const char *name);
bool adxl372_readable_noinc_reg(struct device *dev, unsigned int reg);

#endif 
