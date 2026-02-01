
 

#include <linux/slab.h>
#include <linux/time.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <sound/core.h>

 
int snd_device_new(struct snd_card *card, enum snd_device_type type,
		   void *device_data, const struct snd_device_ops *ops)
{
	struct snd_device *dev;
	struct list_head *p;

	if (snd_BUG_ON(!card || !device_data || !ops))
		return -ENXIO;
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	INIT_LIST_HEAD(&dev->list);
	dev->card = card;
	dev->type = type;
	dev->state = SNDRV_DEV_BUILD;
	dev->device_data = device_data;
	dev->ops = ops;

	 
	list_for_each_prev(p, &card->devices) {
		struct snd_device *pdev = list_entry(p, struct snd_device, list);
		if ((unsigned int)pdev->type <= (unsigned int)type)
			break;
	}

	list_add(&dev->list, p);
	return 0;
}
EXPORT_SYMBOL(snd_device_new);

static void __snd_device_disconnect(struct snd_device *dev)
{
	if (dev->state == SNDRV_DEV_REGISTERED) {
		if (dev->ops->dev_disconnect &&
		    dev->ops->dev_disconnect(dev))
			dev_err(dev->card->dev, "device disconnect failure\n");
		dev->state = SNDRV_DEV_DISCONNECTED;
	}
}

static void __snd_device_free(struct snd_device *dev)
{
	 
	list_del(&dev->list);

	__snd_device_disconnect(dev);
	if (dev->ops->dev_free) {
		if (dev->ops->dev_free(dev))
			dev_err(dev->card->dev, "device free failure\n");
	}
	kfree(dev);
}

static struct snd_device *look_for_dev(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;

	list_for_each_entry(dev, &card->devices, list)
		if (dev->device_data == device_data)
			return dev;

	return NULL;
}

 
void snd_device_disconnect(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;

	if (snd_BUG_ON(!card || !device_data))
		return;
	dev = look_for_dev(card, device_data);
	if (dev)
		__snd_device_disconnect(dev);
	else
		dev_dbg(card->dev, "device disconnect %p (from %pS), not found\n",
			device_data, __builtin_return_address(0));
}
EXPORT_SYMBOL_GPL(snd_device_disconnect);

 
void snd_device_free(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;
	
	if (snd_BUG_ON(!card || !device_data))
		return;
	dev = look_for_dev(card, device_data);
	if (dev)
		__snd_device_free(dev);
	else
		dev_dbg(card->dev, "device free %p (from %pS), not found\n",
			device_data, __builtin_return_address(0));
}
EXPORT_SYMBOL(snd_device_free);

static int __snd_device_register(struct snd_device *dev)
{
	if (dev->state == SNDRV_DEV_BUILD) {
		if (dev->ops->dev_register) {
			int err = dev->ops->dev_register(dev);
			if (err < 0)
				return err;
		}
		dev->state = SNDRV_DEV_REGISTERED;
	}
	return 0;
}

 
int snd_device_register(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;

	if (snd_BUG_ON(!card || !device_data))
		return -ENXIO;
	dev = look_for_dev(card, device_data);
	if (dev)
		return __snd_device_register(dev);
	snd_BUG();
	return -ENXIO;
}
EXPORT_SYMBOL(snd_device_register);

 
int snd_device_register_all(struct snd_card *card)
{
	struct snd_device *dev;
	int err;
	
	if (snd_BUG_ON(!card))
		return -ENXIO;
	list_for_each_entry(dev, &card->devices, list) {
		err = __snd_device_register(dev);
		if (err < 0)
			return err;
	}
	return 0;
}

 
void snd_device_disconnect_all(struct snd_card *card)
{
	struct snd_device *dev;

	if (snd_BUG_ON(!card))
		return;
	list_for_each_entry_reverse(dev, &card->devices, list)
		__snd_device_disconnect(dev);
}

 
void snd_device_free_all(struct snd_card *card)
{
	struct snd_device *dev, *next;

	if (snd_BUG_ON(!card))
		return;
	list_for_each_entry_safe_reverse(dev, next, &card->devices, list) {
		 
		if (dev->type == SNDRV_DEV_CONTROL ||
		    dev->type == SNDRV_DEV_LOWLEVEL)
			continue;
		__snd_device_free(dev);
	}

	 
	list_for_each_entry_safe_reverse(dev, next, &card->devices, list)
		__snd_device_free(dev);
}

 
int snd_device_get_state(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;

	dev = look_for_dev(card, device_data);
	if (dev)
		return dev->state;
	return -1;
}
EXPORT_SYMBOL_GPL(snd_device_get_state);
