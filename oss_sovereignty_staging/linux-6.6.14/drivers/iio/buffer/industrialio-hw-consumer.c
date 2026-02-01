
 

#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/iio/hw-consumer.h>
#include <linux/iio/buffer_impl.h>

 
struct iio_hw_consumer {
	struct list_head buffers;
	struct iio_channel *channels;
};

struct hw_consumer_buffer {
	struct list_head head;
	struct iio_dev *indio_dev;
	struct iio_buffer buffer;
	long scan_mask[];
};

static struct hw_consumer_buffer *iio_buffer_to_hw_consumer_buffer(
	struct iio_buffer *buffer)
{
	return container_of(buffer, struct hw_consumer_buffer, buffer);
}

static void iio_hw_buf_release(struct iio_buffer *buffer)
{
	struct hw_consumer_buffer *hw_buf =
		iio_buffer_to_hw_consumer_buffer(buffer);
	kfree(hw_buf);
}

static const struct iio_buffer_access_funcs iio_hw_buf_access = {
	.release = &iio_hw_buf_release,
	.modes = INDIO_BUFFER_HARDWARE,
};

static struct hw_consumer_buffer *iio_hw_consumer_get_buffer(
	struct iio_hw_consumer *hwc, struct iio_dev *indio_dev)
{
	struct hw_consumer_buffer *buf;

	list_for_each_entry(buf, &hwc->buffers, head) {
		if (buf->indio_dev == indio_dev)
			return buf;
	}

	buf = kzalloc(struct_size(buf, scan_mask, BITS_TO_LONGS(indio_dev->masklength)),
		      GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->buffer.access = &iio_hw_buf_access;
	buf->indio_dev = indio_dev;
	buf->buffer.scan_mask = buf->scan_mask;

	iio_buffer_init(&buf->buffer);
	list_add_tail(&buf->head, &hwc->buffers);

	return buf;
}

 
struct iio_hw_consumer *iio_hw_consumer_alloc(struct device *dev)
{
	struct hw_consumer_buffer *buf;
	struct iio_hw_consumer *hwc;
	struct iio_channel *chan;
	int ret;

	hwc = kzalloc(sizeof(*hwc), GFP_KERNEL);
	if (!hwc)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&hwc->buffers);

	hwc->channels = iio_channel_get_all(dev);
	if (IS_ERR(hwc->channels)) {
		ret = PTR_ERR(hwc->channels);
		goto err_free_hwc;
	}

	chan = &hwc->channels[0];
	while (chan->indio_dev) {
		buf = iio_hw_consumer_get_buffer(hwc, chan->indio_dev);
		if (!buf) {
			ret = -ENOMEM;
			goto err_put_buffers;
		}
		set_bit(chan->channel->scan_index, buf->buffer.scan_mask);
		chan++;
	}

	return hwc;

err_put_buffers:
	list_for_each_entry(buf, &hwc->buffers, head)
		iio_buffer_put(&buf->buffer);
	iio_channel_release_all(hwc->channels);
err_free_hwc:
	kfree(hwc);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(iio_hw_consumer_alloc);

 
void iio_hw_consumer_free(struct iio_hw_consumer *hwc)
{
	struct hw_consumer_buffer *buf, *n;

	iio_channel_release_all(hwc->channels);
	list_for_each_entry_safe(buf, n, &hwc->buffers, head)
		iio_buffer_put(&buf->buffer);
	kfree(hwc);
}
EXPORT_SYMBOL_GPL(iio_hw_consumer_free);

static void devm_iio_hw_consumer_release(void *iio_hwc)
{
	iio_hw_consumer_free(iio_hwc);
}

 
struct iio_hw_consumer *devm_iio_hw_consumer_alloc(struct device *dev)
{
	struct iio_hw_consumer *iio_hwc;
	int ret;

	iio_hwc = iio_hw_consumer_alloc(dev);
	if (IS_ERR(iio_hwc))
		return iio_hwc;

	ret = devm_add_action_or_reset(dev, devm_iio_hw_consumer_release,
				       iio_hwc);
	if (ret)
		return ERR_PTR(ret);

	return iio_hwc;
}
EXPORT_SYMBOL_GPL(devm_iio_hw_consumer_alloc);

 
int iio_hw_consumer_enable(struct iio_hw_consumer *hwc)
{
	struct hw_consumer_buffer *buf;
	int ret;

	list_for_each_entry(buf, &hwc->buffers, head) {
		ret = iio_update_buffers(buf->indio_dev, &buf->buffer, NULL);
		if (ret)
			goto err_disable_buffers;
	}

	return 0;

err_disable_buffers:
	list_for_each_entry_continue_reverse(buf, &hwc->buffers, head)
		iio_update_buffers(buf->indio_dev, NULL, &buf->buffer);
	return ret;
}
EXPORT_SYMBOL_GPL(iio_hw_consumer_enable);

 
void iio_hw_consumer_disable(struct iio_hw_consumer *hwc)
{
	struct hw_consumer_buffer *buf;

	list_for_each_entry(buf, &hwc->buffers, head)
		iio_update_buffers(buf->indio_dev, NULL, &buf->buffer);
}
EXPORT_SYMBOL_GPL(iio_hw_consumer_disable);

MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_DESCRIPTION("Hardware consumer buffer the IIO framework");
MODULE_LICENSE("GPL v2");
