 
 

#ifndef INV_MPU_IIO_H_
#define INV_MPU_IIO_H_

#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/mutex.h>
#include <linux/platform_data/invensense_mpu6050.h>
#include <linux/regmap.h>

#include <linux/iio/buffer.h>
#include <linux/iio/common/inv_sensors_timestamp.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/sysfs.h>

 
struct inv_mpu6050_reg_map {
	u8 sample_rate_div;
	u8 lpf;
	u8 accel_lpf;
	u8 user_ctrl;
	u8 fifo_en;
	u8 gyro_config;
	u8 accl_config;
	u8 fifo_count_h;
	u8 fifo_r_w;
	u8 raw_gyro;
	u8 raw_accl;
	u8 temperature;
	u8 int_enable;
	u8 int_status;
	u8 pwr_mgmt_1;
	u8 pwr_mgmt_2;
	u8 int_pin_cfg;
	u8 accl_offset;
	u8 gyro_offset;
	u8 i2c_if;
};

 
enum inv_devices {
	INV_MPU6050,
	INV_MPU6500,
	INV_MPU6515,
	INV_MPU6880,
	INV_MPU6000,
	INV_MPU9150,
	INV_MPU9250,
	INV_MPU9255,
	INV_ICM20608,
	INV_ICM20608D,
	INV_ICM20609,
	INV_ICM20689,
	INV_ICM20600,
	INV_ICM20602,
	INV_ICM20690,
	INV_IAM20680,
	INV_NUM_PARTS
};

 
#define INV_MPU6050_SENSOR_ACCL		BIT(0)
#define INV_MPU6050_SENSOR_GYRO		BIT(1)
#define INV_MPU6050_SENSOR_TEMP		BIT(2)
#define INV_MPU6050_SENSOR_MAGN		BIT(3)

 
struct inv_mpu6050_chip_config {
	unsigned int clk:3;
	unsigned int fsr:2;
	unsigned int lpf:3;
	unsigned int accl_fs:2;
	unsigned int accl_en:1;
	unsigned int gyro_en:1;
	unsigned int temp_en:1;
	unsigned int magn_en:1;
	unsigned int accl_fifo_enable:1;
	unsigned int gyro_fifo_enable:1;
	unsigned int temp_fifo_enable:1;
	unsigned int magn_fifo_enable:1;
	u8 divider;
	u8 user_ctrl;
};

 
#define INV_MPU6050_OUTPUT_DATA_SIZE         32

 
struct inv_mpu6050_hw {
	u8 whoami;
	u8 *name;
	const struct inv_mpu6050_reg_map *reg;
	const struct inv_mpu6050_chip_config *config;
	size_t fifo_size;
	struct {
		int offset;
		int scale;
	} temp;
	struct {
		unsigned int accel;
		unsigned int gyro;
	} startup_time;
};

 
struct inv_mpu6050_state {
	struct mutex lock;
	struct iio_trigger  *trig;
	struct inv_mpu6050_chip_config chip_config;
	const struct inv_mpu6050_reg_map *reg;
	const struct inv_mpu6050_hw *hw;
	enum   inv_devices chip_type;
	struct i2c_mux_core *muxc;
	struct i2c_client *mux_client;
	struct inv_mpu6050_platform_data plat_data;
	struct iio_mount_matrix orientation;
	struct regmap *map;
	int irq;
	u8 irq_mask;
	unsigned skip_samples;
	struct inv_sensors_timestamp timestamp;
	struct regulator *vdd_supply;
	struct regulator *vddio_supply;
	bool magn_disabled;
	s32 magn_raw_to_gauss[3];
	struct iio_mount_matrix magn_orient;
	unsigned int suspended_sensors;
	u8 *data;
};

 
#define INV_MPU6050_REG_ACCEL_OFFSET        0x06
#define INV_MPU6050_REG_GYRO_OFFSET         0x13

#define INV_MPU6050_REG_SAMPLE_RATE_DIV     0x19
#define INV_MPU6050_REG_CONFIG              0x1A
#define INV_MPU6050_REG_GYRO_CONFIG         0x1B
#define INV_MPU6050_REG_ACCEL_CONFIG        0x1C

#define INV_MPU6050_REG_FIFO_EN             0x23
#define INV_MPU6050_BIT_SLAVE_0             0x01
#define INV_MPU6050_BIT_SLAVE_1             0x02
#define INV_MPU6050_BIT_SLAVE_2             0x04
#define INV_MPU6050_BIT_ACCEL_OUT           0x08
#define INV_MPU6050_BITS_GYRO_OUT           0x70
#define INV_MPU6050_BIT_TEMP_OUT            0x80

#define INV_MPU6050_REG_I2C_MST_CTRL        0x24
#define INV_MPU6050_BITS_I2C_MST_CLK_400KHZ 0x0D
#define INV_MPU6050_BIT_I2C_MST_P_NSR       0x10
#define INV_MPU6050_BIT_SLV3_FIFO_EN        0x20
#define INV_MPU6050_BIT_WAIT_FOR_ES         0x40
#define INV_MPU6050_BIT_MULT_MST_EN         0x80

 
#define INV_MPU6050_REG_I2C_SLV_ADDR(_x)    (0x25 + 3 * (_x))
#define INV_MPU6050_BIT_I2C_SLV_RNW         0x80

#define INV_MPU6050_REG_I2C_SLV_REG(_x)     (0x26 + 3 * (_x))

#define INV_MPU6050_REG_I2C_SLV_CTRL(_x)    (0x27 + 3 * (_x))
#define INV_MPU6050_BIT_SLV_GRP             0x10
#define INV_MPU6050_BIT_SLV_REG_DIS         0x20
#define INV_MPU6050_BIT_SLV_BYTE_SW         0x40
#define INV_MPU6050_BIT_SLV_EN              0x80

 
#define INV_MPU6050_REG_I2C_SLV4_CTRL       0x34
#define INV_MPU6050_BITS_I2C_MST_DLY(_x)    ((_x) & 0x1F)

#define INV_MPU6050_REG_I2C_MST_STATUS      0x36
#define INV_MPU6050_BIT_I2C_SLV0_NACK       0x01
#define INV_MPU6050_BIT_I2C_SLV1_NACK       0x02
#define INV_MPU6050_BIT_I2C_SLV2_NACK       0x04
#define INV_MPU6050_BIT_I2C_SLV3_NACK       0x08

#define INV_MPU6050_REG_INT_ENABLE          0x38
#define INV_MPU6050_BIT_DATA_RDY_EN         0x01
#define INV_MPU6050_BIT_DMP_INT_EN          0x02

#define INV_MPU6050_REG_RAW_ACCEL           0x3B
#define INV_MPU6050_REG_TEMPERATURE         0x41
#define INV_MPU6050_REG_RAW_GYRO            0x43

#define INV_MPU6050_REG_INT_STATUS          0x3A
#define INV_MPU6050_BIT_FIFO_OVERFLOW_INT   0x10
#define INV_MPU6050_BIT_RAW_DATA_RDY_INT    0x01

#define INV_MPU6050_REG_EXT_SENS_DATA       0x49

 
#define INV_MPU6050_REG_I2C_SLV_DO(_x)      (0x63 + (_x))

#define INV_MPU6050_REG_I2C_MST_DELAY_CTRL  0x67
#define INV_MPU6050_BIT_I2C_SLV0_DLY_EN     0x01
#define INV_MPU6050_BIT_I2C_SLV1_DLY_EN     0x02
#define INV_MPU6050_BIT_I2C_SLV2_DLY_EN     0x04
#define INV_MPU6050_BIT_I2C_SLV3_DLY_EN     0x08
#define INV_MPU6050_BIT_DELAY_ES_SHADOW     0x80

#define INV_MPU6050_REG_SIGNAL_PATH_RESET   0x68
#define INV_MPU6050_BIT_TEMP_RST            BIT(0)
#define INV_MPU6050_BIT_ACCEL_RST           BIT(1)
#define INV_MPU6050_BIT_GYRO_RST            BIT(2)

#define INV_MPU6050_REG_USER_CTRL           0x6A
#define INV_MPU6050_BIT_SIG_COND_RST        0x01
#define INV_MPU6050_BIT_FIFO_RST            0x04
#define INV_MPU6050_BIT_DMP_RST             0x08
#define INV_MPU6050_BIT_I2C_MST_EN          0x20
#define INV_MPU6050_BIT_FIFO_EN             0x40
#define INV_MPU6050_BIT_DMP_EN              0x80
#define INV_MPU6050_BIT_I2C_IF_DIS          0x10

#define INV_MPU6050_REG_PWR_MGMT_1          0x6B
#define INV_MPU6050_BIT_H_RESET             0x80
#define INV_MPU6050_BIT_SLEEP               0x40
#define INV_MPU6050_BIT_TEMP_DIS            0x08
#define INV_MPU6050_BIT_CLK_MASK            0x7

#define INV_MPU6050_REG_PWR_MGMT_2          0x6C
#define INV_MPU6050_BIT_PWR_ACCL_STBY       0x38
#define INV_MPU6050_BIT_PWR_GYRO_STBY       0x07

 
#define INV_ICM20602_REG_I2C_IF             0x70
#define INV_ICM20602_BIT_I2C_IF_DIS         0x40

#define INV_MPU6050_REG_FIFO_COUNT_H        0x72
#define INV_MPU6050_REG_FIFO_R_W            0x74

#define INV_MPU6050_BYTES_PER_3AXIS_SENSOR   6
#define INV_MPU6050_FIFO_COUNT_BYTE          2

 
#define INV_MPU9X50_BYTES_MAGN               7

 
#define INV_MPU6050_BYTES_PER_TEMP_SENSOR   2

 
#define INV_MPU6500_REG_ACCEL_CONFIG_2      0x1D
#define INV_ICM20689_BITS_FIFO_SIZE_MAX     0xC0
#define INV_MPU6500_REG_ACCEL_OFFSET        0x77

 
#define INV_MPU6050_POWER_UP_TIME            100
#define INV_MPU6050_TEMP_UP_TIME             100
#define INV_MPU6050_ACCEL_STARTUP_TIME       20
#define INV_MPU6050_GYRO_STARTUP_TIME        60
#define INV_MPU6050_GYRO_DOWN_TIME           150
#define INV_MPU6050_SUSPEND_DELAY_MS         2000

#define INV_MPU6500_GYRO_STARTUP_TIME        70
#define INV_MPU6500_ACCEL_STARTUP_TIME       30

#define INV_ICM20602_GYRO_STARTUP_TIME       100
#define INV_ICM20602_ACCEL_STARTUP_TIME      20

#define INV_ICM20690_GYRO_STARTUP_TIME       80
#define INV_ICM20690_ACCEL_STARTUP_TIME      10


 
#define INV_MPU6050_REG_UP_TIME_MIN          5000
#define INV_MPU6050_REG_UP_TIME_MAX          10000

#define INV_MPU6050_TEMP_OFFSET	             12420
#define INV_MPU6050_TEMP_SCALE               2941176
#define INV_MPU6050_MAX_GYRO_FS_PARAM        3
#define INV_MPU6050_MAX_ACCL_FS_PARAM        3
#define INV_MPU6050_THREE_AXIS               3
#define INV_MPU6050_GYRO_CONFIG_FSR_SHIFT    3
#define INV_ICM20690_GYRO_CONFIG_FSR_SHIFT   2
#define INV_MPU6050_ACCL_CONFIG_FSR_SHIFT    3

#define INV_MPU6500_TEMP_OFFSET              7011
#define INV_MPU6500_TEMP_SCALE               2995178

#define INV_ICM20608_TEMP_OFFSET	     8170
#define INV_ICM20608_TEMP_SCALE		     3059976

#define INV_MPU6050_REG_INT_PIN_CFG	0x37
#define INV_MPU6050_ACTIVE_HIGH		0x00
#define INV_MPU6050_ACTIVE_LOW		0x80
 
#define INV_MPU6050_LATCH_INT_EN	0x20
#define INV_MPU6050_BIT_BYPASS_EN	0x2

 
#define INV_MPU6050_TS_PERIOD_JITTER	4

 
#define INV_MPU6050_MAX_FIFO_RATE            1000
#define INV_MPU6050_MIN_FIFO_RATE            4

 
#define INV_MPU6050_INTERNAL_FREQ_HZ		1000
 
#define INV_MPU6050_FREQ_DIVIDER(st)					\
	((st)->chip_config.divider + 1)
 
#define INV_MPU6050_FIFO_RATE_TO_DIVIDER(fifo_rate)			\
	((INV_MPU6050_INTERNAL_FREQ_HZ / (fifo_rate)) - 1)
#define INV_MPU6050_DIVIDER_TO_FIFO_RATE(divider)			\
	(INV_MPU6050_INTERNAL_FREQ_HZ / ((divider) + 1))

#define INV_MPU6050_REG_WHOAMI			117

#define INV_MPU6000_WHOAMI_VALUE		0x68
#define INV_MPU6050_WHOAMI_VALUE		0x68
#define INV_MPU6500_WHOAMI_VALUE		0x70
#define INV_MPU6880_WHOAMI_VALUE		0x78
#define INV_MPU9150_WHOAMI_VALUE		0x68
#define INV_MPU9250_WHOAMI_VALUE		0x71
#define INV_MPU9255_WHOAMI_VALUE		0x73
#define INV_MPU6515_WHOAMI_VALUE		0x74
#define INV_ICM20608_WHOAMI_VALUE		0xAF
#define INV_ICM20608D_WHOAMI_VALUE		0xAE
#define INV_ICM20609_WHOAMI_VALUE		0xA6
#define INV_ICM20689_WHOAMI_VALUE		0x98
#define INV_ICM20600_WHOAMI_VALUE		0x11
#define INV_ICM20602_WHOAMI_VALUE		0x12
#define INV_ICM20690_WHOAMI_VALUE		0x20
#define INV_IAM20680_WHOAMI_VALUE		0xA9

 
enum inv_mpu6050_scan {
	INV_MPU6050_SCAN_ACCL_X,
	INV_MPU6050_SCAN_ACCL_Y,
	INV_MPU6050_SCAN_ACCL_Z,
	INV_MPU6050_SCAN_TEMP,
	INV_MPU6050_SCAN_GYRO_X,
	INV_MPU6050_SCAN_GYRO_Y,
	INV_MPU6050_SCAN_GYRO_Z,
	INV_MPU6050_SCAN_TIMESTAMP,

	INV_MPU9X50_SCAN_MAGN_X = INV_MPU6050_SCAN_GYRO_Z + 1,
	INV_MPU9X50_SCAN_MAGN_Y,
	INV_MPU9X50_SCAN_MAGN_Z,
	INV_MPU9X50_SCAN_TIMESTAMP,
};

enum inv_mpu6050_filter_e {
	INV_MPU6050_FILTER_NOLPF2 = 0,
	INV_MPU6050_FILTER_200HZ,
	INV_MPU6050_FILTER_100HZ,
	INV_MPU6050_FILTER_45HZ,
	INV_MPU6050_FILTER_20HZ,
	INV_MPU6050_FILTER_10HZ,
	INV_MPU6050_FILTER_5HZ,
	INV_MPU6050_FILTER_NOLPF,
	NUM_MPU6050_FILTER
};

 
enum INV_MPU6050_IIO_ATTR_ADDR {
	ATTR_GYRO_MATRIX,
	ATTR_ACCL_MATRIX,
};

enum inv_mpu6050_accl_fs_e {
	INV_MPU6050_FS_02G = 0,
	INV_MPU6050_FS_04G,
	INV_MPU6050_FS_08G,
	INV_MPU6050_FS_16G,
	NUM_ACCL_FSR
};

enum inv_mpu6050_fsr_e {
	INV_MPU6050_FSR_250DPS = 0,
	INV_MPU6050_FSR_500DPS,
	INV_MPU6050_FSR_1000DPS,
	INV_MPU6050_FSR_2000DPS,
	NUM_MPU6050_FSR
};

enum inv_mpu6050_clock_sel_e {
	INV_CLK_INTERNAL = 0,
	INV_CLK_PLL,
	NUM_CLK
};

irqreturn_t inv_mpu6050_read_fifo(int irq, void *p);
int inv_mpu6050_probe_trigger(struct iio_dev *indio_dev, int irq_type);
int inv_mpu6050_prepare_fifo(struct inv_mpu6050_state *st, bool enable);
int inv_mpu6050_switch_engine(struct inv_mpu6050_state *st, bool en,
			      unsigned int mask);
int inv_mpu6050_write_reg(struct inv_mpu6050_state *st, int reg, u8 val);
int inv_mpu_acpi_create_mux_client(struct i2c_client *client);
void inv_mpu_acpi_delete_mux_client(struct i2c_client *client);
int inv_mpu_core_probe(struct regmap *regmap, int irq, const char *name,
		int (*inv_mpu_bus_setup)(struct iio_dev *), int chip_type);
extern const struct dev_pm_ops inv_mpu_pmops;

#endif
