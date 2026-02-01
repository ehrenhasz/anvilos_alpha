
 

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/bitmap.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "iio_simple_dummy.h"

 

static const s16 fakedata[] = {
	[DUMMY_INDEX_VOLTAGE_0] = 7,
	[DUMMY_INDEX_DIFFVOLTAGE_1M2] = -33,
	[DUMMY_INDEX_DIFFVOLTAGE_3M4] = -2,
	[DUMMY_INDEX_ACCELX] = 344,
};

 
static irqreturn_t iio_simple_dummy_trigger_h(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	int i = 0, j;
	u16 *data;

	data = kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (!data)
		goto done;

	 
	for_each_set_bit(j, indio_dev->active_scan_mask, indio_dev->masklength)
		data[i++] = fakedata[j];

	iio_push_to_buffers_with_timestamp(indio_dev, data,
					   iio_get_time_ns(indio_dev));

	kfree(data);

done:
	 
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static const struct iio_buffer_setup_ops iio_simple_dummy_buffer_setup_ops = {
};

int iio_simple_dummy_configure_buffer(struct iio_dev *indio_dev)
{
	return iio_triggered_buffer_setup(indio_dev, NULL,
					  iio_simple_dummy_trigger_h,
					  &iio_simple_dummy_buffer_setup_ops);
}

 
void iio_simple_dummy_unconfigure_buffer(struct iio_dev *indio_dev)
{
	iio_triggered_buffer_cleanup(indio_dev);
}
