
 

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>

#include "inv_mpu_aux.h"
#include "inv_mpu_iio.h"
#include "inv_mpu_magn.h"

 
#define INV_MPU_MAGN_I2C_ADDR		0x0C

#define INV_MPU_MAGN_REG_WIA		0x00
#define INV_MPU_MAGN_BITS_WIA		0x48

#define INV_MPU_MAGN_REG_ST1		0x02
#define INV_MPU_MAGN_BIT_DRDY		0x01
#define INV_MPU_MAGN_BIT_DOR		0x02

#define INV_MPU_MAGN_REG_DATA		0x03

#define INV_MPU_MAGN_REG_ST2		0x09
#define INV_MPU_MAGN_BIT_HOFL		0x08
#define INV_MPU_MAGN_BIT_BITM		0x10

#define INV_MPU_MAGN_REG_CNTL1		0x0A
#define INV_MPU_MAGN_BITS_MODE_PWDN	0x00
#define INV_MPU_MAGN_BITS_MODE_SINGLE	0x01
#define INV_MPU_MAGN_BITS_MODE_FUSE	0x0F
#define INV_MPU9250_MAGN_BIT_OUTPUT_BIT	0x10

#define INV_MPU9250_MAGN_REG_CNTL2	0x0B
#define INV_MPU9250_MAGN_BIT_SRST	0x01

#define INV_MPU_MAGN_REG_ASAX		0x10
#define INV_MPU_MAGN_REG_ASAY		0x11
#define INV_MPU_MAGN_REG_ASAZ		0x12

static bool inv_magn_supported(const struct inv_mpu6050_state *st)
{
	switch (st->chip_type) {
	case INV_MPU9150:
	case INV_MPU9250:
	case INV_MPU9255:
		return true;
	default:
		return false;
	}
}

 
static int inv_magn_init(struct inv_mpu6050_state *st)
{
	uint8_t val;
	uint8_t asa[3];
	int32_t sensitivity;
	int ret;

	 
	ret = inv_mpu_aux_read(st, INV_MPU_MAGN_I2C_ADDR, INV_MPU_MAGN_REG_WIA,
			       &val, sizeof(val));
	if (ret)
		return ret;
	if (val != INV_MPU_MAGN_BITS_WIA)
		return -ENODEV;

	 
	switch (st->chip_type) {
	case INV_MPU9250:
	case INV_MPU9255:
		ret = inv_mpu_aux_write(st, INV_MPU_MAGN_I2C_ADDR,
					INV_MPU9250_MAGN_REG_CNTL2,
					INV_MPU9250_MAGN_BIT_SRST);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	 
	ret = inv_mpu_aux_write(st, INV_MPU_MAGN_I2C_ADDR,
				INV_MPU_MAGN_REG_CNTL1,
				INV_MPU_MAGN_BITS_MODE_FUSE);
	if (ret)
		return ret;

	ret = inv_mpu_aux_read(st, INV_MPU_MAGN_I2C_ADDR, INV_MPU_MAGN_REG_ASAX,
			       asa, sizeof(asa));
	if (ret)
		return ret;

	 
	ret = inv_mpu_aux_write(st, INV_MPU_MAGN_I2C_ADDR,
				INV_MPU_MAGN_REG_CNTL1,
				INV_MPU_MAGN_BITS_MODE_PWDN);
	if (ret)
		return ret;

	 
	switch (st->chip_type) {
	case INV_MPU9150:
		 
		sensitivity = 3000;
		break;
	case INV_MPU9250:
	case INV_MPU9255:
		 
		sensitivity = 1500;
		break;
	default:
		return -EINVAL;
	}

	 
	st->magn_raw_to_gauss[0] = (((int32_t)asa[0] + 128) * sensitivity) / 256;
	st->magn_raw_to_gauss[1] = (((int32_t)asa[1] + 128) * sensitivity) / 256;
	st->magn_raw_to_gauss[2] = (((int32_t)asa[2] + 128) * sensitivity) / 256;

	return 0;
}

 
int inv_mpu_magn_probe(struct inv_mpu6050_state *st)
{
	uint8_t val;
	int ret;

	 
	if (!inv_magn_supported(st))
		return 0;

	 
	ret = inv_mpu_aux_init(st);
	if (ret)
		return ret;

	 
	ret = inv_magn_init(st);
	if (ret)
		return ret;

	 
	ret = regmap_write(st->map, INV_MPU6050_REG_I2C_SLV_ADDR(0),
			   INV_MPU6050_BIT_I2C_SLV_RNW | INV_MPU_MAGN_I2C_ADDR);
	if (ret)
		return ret;

	ret = regmap_write(st->map, INV_MPU6050_REG_I2C_SLV_REG(0),
			   INV_MPU_MAGN_REG_DATA);
	if (ret)
		return ret;

	ret = regmap_write(st->map, INV_MPU6050_REG_I2C_SLV_CTRL(0),
			   INV_MPU6050_BIT_SLV_EN |
			   INV_MPU6050_BIT_SLV_BYTE_SW |
			   INV_MPU6050_BIT_SLV_GRP |
			   INV_MPU9X50_BYTES_MAGN);
	if (ret)
		return ret;

	 
	ret = regmap_write(st->map, INV_MPU6050_REG_I2C_SLV_ADDR(1),
			   INV_MPU_MAGN_I2C_ADDR);
	if (ret)
		return ret;

	ret = regmap_write(st->map, INV_MPU6050_REG_I2C_SLV_REG(1),
			   INV_MPU_MAGN_REG_CNTL1);
	if (ret)
		return ret;

	 
	val = INV_MPU_MAGN_BITS_MODE_SINGLE;
	switch (st->chip_type) {
	case INV_MPU9250:
	case INV_MPU9255:
		val |= INV_MPU9250_MAGN_BIT_OUTPUT_BIT;
		break;
	default:
		break;
	}
	ret = regmap_write(st->map, INV_MPU6050_REG_I2C_SLV_DO(1), val);
	if (ret)
		return ret;

	return regmap_write(st->map, INV_MPU6050_REG_I2C_SLV_CTRL(1),
			    INV_MPU6050_BIT_SLV_EN | 1);
}

 
int inv_mpu_magn_set_rate(const struct inv_mpu6050_state *st, int fifo_rate)
{
	uint8_t d;

	 
	if (!inv_magn_supported(st))
		return 0;

	 
	if (fifo_rate > INV_MPU_MAGN_FREQ_HZ_MAX)
		d = fifo_rate / INV_MPU_MAGN_FREQ_HZ_MAX - 1;
	else
		d = 0;

	return regmap_write(st->map, INV_MPU6050_REG_I2C_SLV4_CTRL, d);
}

 
int inv_mpu_magn_set_orient(struct inv_mpu6050_state *st)
{
	struct device *dev = regmap_get_device(st->map);
	const char *orient;
	char *str;
	int i;

	 
	switch (st->chip_type) {
	case INV_MPU9150:
	case INV_MPU9250:
	case INV_MPU9255:
		 
		st->magn_orient.rotation[0] = st->orientation.rotation[3];
		st->magn_orient.rotation[1] = st->orientation.rotation[4];
		st->magn_orient.rotation[2] = st->orientation.rotation[5];
		 
		st->magn_orient.rotation[3] = st->orientation.rotation[0];
		st->magn_orient.rotation[4] = st->orientation.rotation[1];
		st->magn_orient.rotation[5] = st->orientation.rotation[2];
		 
		for (i = 6; i < 9; ++i) {
			orient = st->orientation.rotation[i];

			 
			if (orient[0] == '-')
				str = devm_kstrdup(dev, orient + 1, GFP_KERNEL);
			else if (!strcmp(orient, "0"))
				str = devm_kstrdup(dev, orient, GFP_KERNEL);
			else
				str = devm_kasprintf(dev, GFP_KERNEL, "-%s", orient);
			if (!str)
				return -ENOMEM;

			st->magn_orient.rotation[i] = str;
		}
		break;
	default:
		st->magn_orient = st->orientation;
		break;
	}

	return 0;
}

 
int inv_mpu_magn_read(struct inv_mpu6050_state *st, int axis, int *val)
{
	unsigned int status;
	__be16 data;
	uint8_t addr;
	int ret;

	 
	if (!inv_magn_supported(st))
		return -ENODEV;

	 
	switch (axis) {
	case IIO_MOD_X:
		addr = 0;
		break;
	case IIO_MOD_Y:
		addr = 2;
		break;
	case IIO_MOD_Z:
		addr = 4;
		break;
	default:
		return -EINVAL;
	}
	addr += INV_MPU6050_REG_EXT_SENS_DATA;

	 
	ret = regmap_read(st->map, INV_MPU6050_REG_I2C_MST_STATUS, &status);
	if (ret)
		return ret;

	if (status & INV_MPU6050_BIT_I2C_SLV0_NACK ||
			status & INV_MPU6050_BIT_I2C_SLV1_NACK)
		return -EIO;

	ret = regmap_bulk_read(st->map, addr, &data, sizeof(data));
	if (ret)
		return ret;

	*val = (int16_t)be16_to_cpu(data);

	return IIO_VAL_INT;
}
