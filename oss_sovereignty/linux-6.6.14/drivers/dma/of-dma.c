
 

#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_dma.h>

#include "dmaengine.h"

static LIST_HEAD(of_dma_list);
static DEFINE_MUTEX(of_dma_lock);

 
static struct of_dma *of_dma_find_controller(struct of_phandle_args *dma_spec)
{
	struct of_dma *ofdma;

	list_for_each_entry(ofdma, &of_dma_list, of_dma_controllers)
		if (ofdma->of_node == dma_spec->np)
			return ofdma;

	pr_debug("%s: can't find DMA controller %pOF\n", __func__,
		 dma_spec->np);

	return NULL;
}

 
static struct dma_chan *of_dma_router_xlate(struct of_phandle_args *dma_spec,
					    struct of_dma *ofdma)
{
	struct dma_chan		*chan;
	struct of_dma		*ofdma_target;
	struct of_phandle_args	dma_spec_target;
	void			*route_data;

	 
	memcpy(&dma_spec_target, dma_spec, sizeof(dma_spec_target));
	route_data = ofdma->of_dma_route_allocate(&dma_spec_target, ofdma);
	if (IS_ERR(route_data))
		return NULL;

	ofdma_target = of_dma_find_controller(&dma_spec_target);
	if (!ofdma_target) {
		ofdma->dma_router->route_free(ofdma->dma_router->dev,
					      route_data);
		chan = ERR_PTR(-EPROBE_DEFER);
		goto err;
	}

	chan = ofdma_target->of_dma_xlate(&dma_spec_target, ofdma_target);
	if (IS_ERR_OR_NULL(chan)) {
		ofdma->dma_router->route_free(ofdma->dma_router->dev,
					      route_data);
	} else {
		int ret = 0;

		chan->router = ofdma->dma_router;
		chan->route_data = route_data;

		if (chan->device->device_router_config)
			ret = chan->device->device_router_config(chan);

		if (ret) {
			dma_release_channel(chan);
			chan = ERR_PTR(ret);
		}
	}

err:
	 
	of_node_put(dma_spec_target.np);
	return chan;
}

 
int of_dma_controller_register(struct device_node *np,
				struct dma_chan *(*of_dma_xlate)
				(struct of_phandle_args *, struct of_dma *),
				void *data)
{
	struct of_dma	*ofdma;

	if (!np || !of_dma_xlate) {
		pr_err("%s: not enough information provided\n", __func__);
		return -EINVAL;
	}

	ofdma = kzalloc(sizeof(*ofdma), GFP_KERNEL);
	if (!ofdma)
		return -ENOMEM;

	ofdma->of_node = np;
	ofdma->of_dma_xlate = of_dma_xlate;
	ofdma->of_dma_data = data;

	 
	mutex_lock(&of_dma_lock);
	list_add_tail(&ofdma->of_dma_controllers, &of_dma_list);
	mutex_unlock(&of_dma_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(of_dma_controller_register);

 
void of_dma_controller_free(struct device_node *np)
{
	struct of_dma *ofdma;

	mutex_lock(&of_dma_lock);

	list_for_each_entry(ofdma, &of_dma_list, of_dma_controllers)
		if (ofdma->of_node == np) {
			list_del(&ofdma->of_dma_controllers);
			kfree(ofdma);
			break;
		}

	mutex_unlock(&of_dma_lock);
}
EXPORT_SYMBOL_GPL(of_dma_controller_free);

 
int of_dma_router_register(struct device_node *np,
			   void *(*of_dma_route_allocate)
			   (struct of_phandle_args *, struct of_dma *),
			   struct dma_router *dma_router)
{
	struct of_dma	*ofdma;

	if (!np || !of_dma_route_allocate || !dma_router) {
		pr_err("%s: not enough information provided\n", __func__);
		return -EINVAL;
	}

	ofdma = kzalloc(sizeof(*ofdma), GFP_KERNEL);
	if (!ofdma)
		return -ENOMEM;

	ofdma->of_node = np;
	ofdma->of_dma_xlate = of_dma_router_xlate;
	ofdma->of_dma_route_allocate = of_dma_route_allocate;
	ofdma->dma_router = dma_router;

	 
	mutex_lock(&of_dma_lock);
	list_add_tail(&ofdma->of_dma_controllers, &of_dma_list);
	mutex_unlock(&of_dma_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(of_dma_router_register);

 
static int of_dma_match_channel(struct device_node *np, const char *name,
				int index, struct of_phandle_args *dma_spec)
{
	const char *s;

	if (of_property_read_string_index(np, "dma-names", index, &s))
		return -ENODEV;

	if (strcmp(name, s))
		return -ENODEV;

	if (of_parse_phandle_with_args(np, "dmas", "#dma-cells", index,
				       dma_spec))
		return -ENODEV;

	return 0;
}

 
struct dma_chan *of_dma_request_slave_channel(struct device_node *np,
					      const char *name)
{
	struct of_phandle_args	dma_spec;
	struct of_dma		*ofdma;
	struct dma_chan		*chan;
	int			count, i, start;
	int			ret_no_channel = -ENODEV;
	static atomic_t		last_index;

	if (!np || !name) {
		pr_err("%s: not enough information provided\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	 
	if (!of_property_present(np, "dmas"))
		return ERR_PTR(-ENODEV);

	count = of_property_count_strings(np, "dma-names");
	if (count < 0) {
		pr_err("%s: dma-names property of node '%pOF' missing or empty\n",
			__func__, np);
		return ERR_PTR(-ENODEV);
	}

	 
	start = atomic_inc_return(&last_index);
	for (i = 0; i < count; i++) {
		if (of_dma_match_channel(np, name,
					 (i + start) % count,
					 &dma_spec))
			continue;

		mutex_lock(&of_dma_lock);
		ofdma = of_dma_find_controller(&dma_spec);

		if (ofdma) {
			chan = ofdma->of_dma_xlate(&dma_spec, ofdma);
		} else {
			ret_no_channel = -EPROBE_DEFER;
			chan = NULL;
		}

		mutex_unlock(&of_dma_lock);

		of_node_put(dma_spec.np);

		if (chan)
			return chan;
	}

	return ERR_PTR(ret_no_channel);
}
EXPORT_SYMBOL_GPL(of_dma_request_slave_channel);

 
struct dma_chan *of_dma_simple_xlate(struct of_phandle_args *dma_spec,
						struct of_dma *ofdma)
{
	int count = dma_spec->args_count;
	struct of_dma_filter_info *info = ofdma->of_dma_data;

	if (!info || !info->filter_fn)
		return NULL;

	if (count != 1)
		return NULL;

	return __dma_request_channel(&info->dma_cap, info->filter_fn,
				     &dma_spec->args[0], dma_spec->np);
}
EXPORT_SYMBOL_GPL(of_dma_simple_xlate);

 
struct dma_chan *of_dma_xlate_by_chan_id(struct of_phandle_args *dma_spec,
					 struct of_dma *ofdma)
{
	struct dma_device *dev = ofdma->of_dma_data;
	struct dma_chan *chan, *candidate = NULL;

	if (!dev || dma_spec->args_count != 1)
		return NULL;

	list_for_each_entry(chan, &dev->channels, device_node)
		if (chan->chan_id == dma_spec->args[0]) {
			candidate = chan;
			break;
		}

	if (!candidate)
		return NULL;

	return dma_get_slave_channel(candidate);
}
EXPORT_SYMBOL_GPL(of_dma_xlate_by_chan_id);
