


#ifndef INV_MPU_MAGN_H_
#define INV_MPU_MAGN_H_

#include <linux/kernel.h>

#include "inv_mpu_iio.h"


#define INV_MPU_MAGN_FREQ_HZ_MAX	50

int inv_mpu_magn_probe(struct inv_mpu6050_state *st);


static inline int inv_mpu_magn_get_scale(const struct inv_mpu6050_state *st,
					 const struct iio_chan_spec *chan,
					 int *val, int *val2)
{
	*val = 0;
	*val2 = st->magn_raw_to_gauss[chan->address];
	return IIO_VAL_INT_PLUS_MICRO;
}

int inv_mpu_magn_set_rate(const struct inv_mpu6050_state *st, int fifo_rate);

int inv_mpu_magn_set_orient(struct inv_mpu6050_state *st);

int inv_mpu_magn_read(struct inv_mpu6050_state *st, int axis, int *val);

#endif		
