#ifndef __IIO_DMAENGINE_H__
#define __IIO_DMAENGINE_H__
struct iio_dev;
struct device;
int devm_iio_dmaengine_buffer_setup(struct device *dev,
				    struct iio_dev *indio_dev,
				    const char *channel);
#endif
