

#ifndef __ST_SENSORS_CORE_H
#define __ST_SENSORS_CORE_H
struct iio_dev;
int st_sensors_write_data_with_mask(struct iio_dev *indio_dev,
				    u8 reg_addr, u8 mask, u8 data);
#endif
