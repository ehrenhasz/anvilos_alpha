#ifndef FIMC_IS_SENSOR_H_
#define FIMC_IS_SENSOR_H_
#include <linux/of.h>
#include <linux/types.h>
#define S5K6A3_OPEN_TIMEOUT		2000  
#define S5K6A3_SENSOR_WIDTH		1392
#define S5K6A3_SENSOR_HEIGHT		1392
enum fimc_is_sensor_id {
	FIMC_IS_SENSOR_ID_S5K3H2 = 1,
	FIMC_IS_SENSOR_ID_S5K6A3,
	FIMC_IS_SENSOR_ID_S5K4E5,
	FIMC_IS_SENSOR_ID_S5K3H7,
	FIMC_IS_SENSOR_ID_CUSTOM,
	FIMC_IS_SENSOR_ID_END
};
#define IS_SENSOR_CTRL_BUS_I2C0		0
#define IS_SENSOR_CTRL_BUS_I2C1		1
struct sensor_drv_data {
	enum fimc_is_sensor_id id;
	unsigned short open_timeout;
};
struct fimc_is_sensor {
	const struct sensor_drv_data *drvdata;
	unsigned int i2c_bus;
	u8 test_pattern;
};
const struct sensor_drv_data *fimc_is_sensor_get_drvdata(
				struct device_node *node);
#endif  
