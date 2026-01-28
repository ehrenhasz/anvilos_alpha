


#ifndef ST_MAGN_H
#define ST_MAGN_H

#include <linux/types.h>
#include <linux/iio/common/st_sensors.h>

#define LSM303DLH_MAGN_DEV_NAME		"lsm303dlh_magn"
#define LSM303DLHC_MAGN_DEV_NAME	"lsm303dlhc_magn"
#define LSM303DLM_MAGN_DEV_NAME		"lsm303dlm_magn"
#define LIS3MDL_MAGN_DEV_NAME		"lis3mdl"
#define LSM303AGR_MAGN_DEV_NAME		"lsm303agr_magn"
#define LIS2MDL_MAGN_DEV_NAME		"lis2mdl"
#define LSM9DS1_MAGN_DEV_NAME		"lsm9ds1_magn"
#define IIS2MDC_MAGN_DEV_NAME		"iis2mdc"
#define LSM303C_MAGN_DEV_NAME		"lsm303c_magn"

#ifdef CONFIG_IIO_BUFFER
int st_magn_allocate_ring(struct iio_dev *indio_dev);
int st_magn_trig_set_state(struct iio_trigger *trig, bool state);
#define ST_MAGN_TRIGGER_SET_STATE (&st_magn_trig_set_state)
#else 
static inline int st_magn_allocate_ring(struct iio_dev *indio_dev)
{
	return 0;
}
#define ST_MAGN_TRIGGER_SET_STATE NULL
#endif 

#endif 
