
 

#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/reset/reset-simple.h>
#include <linux/reset/sunxi.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

static int sunxi_reset_init(struct device_node *np)
{
	struct reset_simple_data *data;
	struct resource res;
	resource_size_t size;
	int ret;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		goto err_alloc;

	size = resource_size(&res);
	if (!request_mem_region(res.start, size, np->name)) {
		ret = -EBUSY;
		goto err_alloc;
	}

	data->membase = ioremap(res.start, size);
	if (!data->membase) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	spin_lock_init(&data->lock);

	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = size * 8;
	data->rcdev.ops = &reset_simple_ops;
	data->rcdev.of_node = np;
	data->active_low = true;

	return reset_controller_register(&data->rcdev);

err_alloc:
	kfree(data);
	return ret;
};

 
static const struct of_device_id sunxi_early_reset_dt_ids[] __initconst = {
	{ .compatible = "allwinner,sun6i-a31-ahb1-reset", },
	{   },
};

void __init sun6i_reset_init(void)
{
	struct device_node *np;

	for_each_matching_node(np, sunxi_early_reset_dt_ids)
		sunxi_reset_init(np);
}
