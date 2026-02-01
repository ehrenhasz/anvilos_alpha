


#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/component.h>
#include <sound/core.h>
#include <sound/hdaudio.h>
#include <sound/hda_component.h>
#include <sound/hda_register.h>

static void hdac_acomp_release(struct device *dev, void *res)
{
}

static struct drm_audio_component *hdac_get_acomp(struct device *dev)
{
	return devres_find(dev, hdac_acomp_release, NULL, NULL);
}

 
int snd_hdac_set_codec_wakeup(struct hdac_bus *bus, bool enable)
{
	struct drm_audio_component *acomp = bus->audio_component;

	if (!acomp || !acomp->ops)
		return -ENODEV;

	if (!acomp->ops->codec_wake_override)
		return 0;

	dev_dbg(bus->dev, "%s codec wakeup\n",
		enable ? "enable" : "disable");

	acomp->ops->codec_wake_override(acomp->dev, enable);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_set_codec_wakeup);

 
void snd_hdac_display_power(struct hdac_bus *bus, unsigned int idx, bool enable)
{
	struct drm_audio_component *acomp = bus->audio_component;

	dev_dbg(bus->dev, "display power %s\n",
		enable ? "enable" : "disable");

	mutex_lock(&bus->lock);
	if (enable)
		set_bit(idx, &bus->display_power_status);
	else
		clear_bit(idx, &bus->display_power_status);

	if (!acomp || !acomp->ops)
		goto unlock;

	if (bus->display_power_status) {
		if (!bus->display_power_active) {
			unsigned long cookie = -1;

			if (acomp->ops->get_power)
				cookie = acomp->ops->get_power(acomp->dev);

			snd_hdac_set_codec_wakeup(bus, true);
			snd_hdac_set_codec_wakeup(bus, false);
			bus->display_power_active = cookie;
		}
	} else {
		if (bus->display_power_active) {
			unsigned long cookie = bus->display_power_active;

			if (acomp->ops->put_power)
				acomp->ops->put_power(acomp->dev, cookie);

			bus->display_power_active = 0;
		}
	}
 unlock:
	mutex_unlock(&bus->lock);
}
EXPORT_SYMBOL_GPL(snd_hdac_display_power);

 
int snd_hdac_sync_audio_rate(struct hdac_device *codec, hda_nid_t nid,
			     int dev_id, int rate)
{
	struct hdac_bus *bus = codec->bus;
	struct drm_audio_component *acomp = bus->audio_component;
	int port, pipe;

	if (!acomp || !acomp->ops || !acomp->ops->sync_audio_rate)
		return -ENODEV;
	port = nid;
	if (acomp->audio_ops && acomp->audio_ops->pin2port) {
		port = acomp->audio_ops->pin2port(codec, nid);
		if (port < 0)
			return -EINVAL;
	}
	pipe = dev_id;
	return acomp->ops->sync_audio_rate(acomp->dev, port, pipe, rate);
}
EXPORT_SYMBOL_GPL(snd_hdac_sync_audio_rate);

 
int snd_hdac_acomp_get_eld(struct hdac_device *codec, hda_nid_t nid, int dev_id,
			   bool *audio_enabled, char *buffer, int max_bytes)
{
	struct hdac_bus *bus = codec->bus;
	struct drm_audio_component *acomp = bus->audio_component;
	int port, pipe;

	if (!acomp || !acomp->ops || !acomp->ops->get_eld)
		return -ENODEV;

	port = nid;
	if (acomp->audio_ops && acomp->audio_ops->pin2port) {
		port = acomp->audio_ops->pin2port(codec, nid);
		if (port < 0)
			return -EINVAL;
	}
	pipe = dev_id;
	return acomp->ops->get_eld(acomp->dev, port, pipe, audio_enabled,
				   buffer, max_bytes);
}
EXPORT_SYMBOL_GPL(snd_hdac_acomp_get_eld);

static int hdac_component_master_bind(struct device *dev)
{
	struct drm_audio_component *acomp = hdac_get_acomp(dev);
	int ret;

	if (WARN_ON(!acomp))
		return -EINVAL;

	ret = component_bind_all(dev, acomp);
	if (ret < 0)
		return ret;

	if (WARN_ON(!(acomp->dev && acomp->ops))) {
		ret = -EINVAL;
		goto out_unbind;
	}

	 
	if (!try_module_get(acomp->ops->owner)) {
		ret = -ENODEV;
		goto out_unbind;
	}

	if (acomp->audio_ops && acomp->audio_ops->master_bind) {
		ret = acomp->audio_ops->master_bind(dev, acomp);
		if (ret < 0)
			goto module_put;
	}

	complete_all(&acomp->master_bind_complete);
	return 0;

 module_put:
	module_put(acomp->ops->owner);
out_unbind:
	component_unbind_all(dev, acomp);
	complete_all(&acomp->master_bind_complete);

	return ret;
}

static void hdac_component_master_unbind(struct device *dev)
{
	struct drm_audio_component *acomp = hdac_get_acomp(dev);

	if (acomp->audio_ops && acomp->audio_ops->master_unbind)
		acomp->audio_ops->master_unbind(dev, acomp);
	module_put(acomp->ops->owner);
	component_unbind_all(dev, acomp);
	WARN_ON(acomp->ops || acomp->dev);
}

static const struct component_master_ops hdac_component_master_ops = {
	.bind = hdac_component_master_bind,
	.unbind = hdac_component_master_unbind,
};

 
int snd_hdac_acomp_register_notifier(struct hdac_bus *bus,
				    const struct drm_audio_component_audio_ops *aops)
{
	if (!bus->audio_component)
		return -ENODEV;

	bus->audio_component->audio_ops = aops;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_acomp_register_notifier);

 
int snd_hdac_acomp_init(struct hdac_bus *bus,
			const struct drm_audio_component_audio_ops *aops,
			int (*match_master)(struct device *, int, void *),
			size_t extra_size)
{
	struct component_match *match = NULL;
	struct device *dev = bus->dev;
	struct drm_audio_component *acomp;
	int ret;

	if (WARN_ON(hdac_get_acomp(dev)))
		return -EBUSY;

	acomp = devres_alloc(hdac_acomp_release, sizeof(*acomp) + extra_size,
			     GFP_KERNEL);
	if (!acomp)
		return -ENOMEM;
	acomp->audio_ops = aops;
	init_completion(&acomp->master_bind_complete);
	bus->audio_component = acomp;
	devres_add(dev, acomp);

	component_match_add_typed(dev, &match, match_master, bus);
	ret = component_master_add_with_match(dev, &hdac_component_master_ops,
					      match);
	if (ret < 0)
		goto out_err;

	return 0;

out_err:
	bus->audio_component = NULL;
	devres_destroy(dev, hdac_acomp_release, NULL, NULL);
	dev_info(dev, "failed to add audio component master (%d)\n", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(snd_hdac_acomp_init);

 
int snd_hdac_acomp_exit(struct hdac_bus *bus)
{
	struct device *dev = bus->dev;
	struct drm_audio_component *acomp = bus->audio_component;

	if (!acomp)
		return 0;

	if (WARN_ON(bus->display_power_active) && acomp->ops)
		acomp->ops->put_power(acomp->dev, bus->display_power_active);

	bus->display_power_active = 0;
	bus->display_power_status = 0;

	component_master_del(dev, &hdac_component_master_ops);

	bus->audio_component = NULL;
	devres_destroy(dev, hdac_acomp_release, NULL, NULL);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_acomp_exit);
