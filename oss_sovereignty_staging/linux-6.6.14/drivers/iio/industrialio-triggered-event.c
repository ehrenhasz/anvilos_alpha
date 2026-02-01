
 

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/iio/iio.h>
#include <linux/iio/triggered_event.h>
#include <linux/iio/trigger_consumer.h>

 
int iio_triggered_event_setup(struct iio_dev *indio_dev,
			      irqreturn_t (*h)(int irq, void *p),
			      irqreturn_t (*thread)(int irq, void *p))
{
	indio_dev->pollfunc_event = iio_alloc_pollfunc(h,
						       thread,
						       IRQF_ONESHOT,
						       indio_dev,
						       "%s_consumer%d",
						       indio_dev->name,
						       iio_device_id(indio_dev));
	if (indio_dev->pollfunc_event == NULL)
		return -ENOMEM;

	 
	indio_dev->modes |= INDIO_EVENT_TRIGGERED;

	return 0;
}
EXPORT_SYMBOL(iio_triggered_event_setup);

 
void iio_triggered_event_cleanup(struct iio_dev *indio_dev)
{
	indio_dev->modes &= ~INDIO_EVENT_TRIGGERED;
	iio_dealloc_pollfunc(indio_dev->pollfunc_event);
}
EXPORT_SYMBOL(iio_triggered_event_cleanup);

MODULE_AUTHOR("Vladimir Barinov");
MODULE_DESCRIPTION("IIO helper functions for setting up triggered events");
MODULE_LICENSE("GPL");
