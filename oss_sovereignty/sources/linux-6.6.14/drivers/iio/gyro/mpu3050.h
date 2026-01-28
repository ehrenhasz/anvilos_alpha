
#include <linux/iio/iio.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>


enum mpu3050_fullscale {
	FS_250_DPS = 0,
	FS_500_DPS,
	FS_1000_DPS,
	FS_2000_DPS,
};


enum mpu3050_lpf {
	
	LPF_256_HZ_NOLPF = 0,
	
	LPF_188_HZ,
	LPF_98_HZ,
	LPF_42_HZ,
	LPF_20_HZ,
	LPF_10_HZ,
	LPF_5_HZ,
	LPF_2100_HZ_NOLPF,
};

enum mpu3050_axis {
	AXIS_X = 0,
	AXIS_Y,
	AXIS_Z,
	AXIS_MAX,
};


struct mpu3050 {
	struct device *dev;
	struct iio_mount_matrix orientation;
	struct regmap *map;
	struct mutex lock;
	int irq;
	struct regulator_bulk_data regs[2];
	enum mpu3050_fullscale fullscale;
	enum mpu3050_lpf lpf;
	u8 divisor;
	s16 calibration[3];
	struct iio_trigger *trig;
	bool hw_irq_trigger;
	bool irq_actl;
	bool irq_latch;
	bool irq_opendrain;
	bool pending_fifo_footer;
	s64 hw_timestamp;
	struct i2c_mux_core *i2cmux;
};


int mpu3050_common_probe(struct device *dev,
			 struct regmap *map,
			 int irq,
			 const char *name);
void mpu3050_common_remove(struct device *dev);


extern const struct dev_pm_ops mpu3050_dev_pm_ops;
