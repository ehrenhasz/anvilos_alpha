 


#ifndef ST_LSM9DS0_H
#define ST_LSM9DS0_H

struct iio_dev;
struct regulator;

struct st_lsm9ds0 {
	struct device *dev;
	const char *name;
	int irq;
	struct iio_dev *accel;
	struct iio_dev *magn;
	struct regulator *vdd;
	struct regulator *vdd_io;
};

int st_lsm9ds0_probe(struct st_lsm9ds0 *lsm9ds0, struct regmap *regmap);

#endif  
