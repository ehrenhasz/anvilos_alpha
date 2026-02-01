
  

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

 
int iio_triggered_buffer_setup_ext(struct iio_dev *indio_dev,
	irqreturn_t (*h)(int irq, void *p),
	irqreturn_t (*thread)(int irq, void *p),
	enum iio_buffer_direction direction,
	const struct iio_buffer_setup_ops *setup_ops,
	const struct iio_dev_attr **buffer_attrs)
{
	struct iio_buffer *buffer;
	int ret;

	 
	if (indio_dev->buffer)
		return -EADDRINUSE;

	buffer = iio_kfifo_allocate();
	if (!buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}

	indio_dev->pollfunc = iio_alloc_pollfunc(h,
						 thread,
						 IRQF_ONESHOT,
						 indio_dev,
						 "%s_consumer%d",
						 indio_dev->name,
						 iio_device_id(indio_dev));
	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_kfifo_free;
	}

	 
	indio_dev->setup_ops = setup_ops;

	 
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	buffer->direction = direction;
	buffer->attrs = buffer_attrs;

	ret = iio_device_attach_buffer(indio_dev, buffer);
	if (ret < 0)
		goto error_dealloc_pollfunc;

	return 0;

error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_kfifo_free:
	iio_kfifo_free(buffer);
error_ret:
	return ret;
}
EXPORT_SYMBOL(iio_triggered_buffer_setup_ext);

 
void iio_triggered_buffer_cleanup(struct iio_dev *indio_dev)
{
	iio_dealloc_pollfunc(indio_dev->pollfunc);
	iio_kfifo_free(indio_dev->buffer);
}
EXPORT_SYMBOL(iio_triggered_buffer_cleanup);

static void devm_iio_triggered_buffer_clean(void *indio_dev)
{
	iio_triggered_buffer_cleanup(indio_dev);
}

int devm_iio_triggered_buffer_setup_ext(struct device *dev,
					struct iio_dev *indio_dev,
					irqreturn_t (*h)(int irq, void *p),
					irqreturn_t (*thread)(int irq, void *p),
					enum iio_buffer_direction direction,
					const struct iio_buffer_setup_ops *ops,
					const struct iio_dev_attr **buffer_attrs)
{
	int ret;

	ret = iio_triggered_buffer_setup_ext(indio_dev, h, thread, direction,
					     ops, buffer_attrs);
	if (ret)
		return ret;

	return devm_add_action_or_reset(dev, devm_iio_triggered_buffer_clean,
					indio_dev);
}
EXPORT_SYMBOL_GPL(devm_iio_triggered_buffer_setup_ext);

MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_DESCRIPTION("IIO helper functions for setting up triggered buffers");
MODULE_LICENSE("GPL");
