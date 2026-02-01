
 

#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/reset/reset-simple.h>
#include <linux/reset/socfpga.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define SOCFPGA_NR_BANKS	8

static int a10_reset_init(struct device_node *np)
{
	struct reset_simple_data *data;
	struct resource res;
	resource_size_t size;
	int ret;
	u32 reg_offset = 0x10;

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
		goto release_region;
	}

	if (of_property_read_u32(np, "altr,modrst-offset", &reg_offset))
		pr_warn("missing altr,modrst-offset property, assuming 0x10\n");
	data->membase += reg_offset;

	spin_lock_init(&data->lock);

	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = SOCFPGA_NR_BANKS * 32;
	data->rcdev.ops = &reset_simple_ops;
	data->rcdev.of_node = np;
	data->status_active_low = true;

	ret = reset_controller_register(&data->rcdev);
	if (ret)
		pr_err("unable to register device\n");

	return ret;

release_region:
	release_mem_region(res.start, size);

err_alloc:
	kfree(data);
	return ret;
};

 
static const struct of_device_id socfpga_early_reset_dt_ids[] __initconst = {
	{ .compatible = "altr,rst-mgr", },
	{   },
};

void __init socfpga_reset_init(void)
{
	struct device_node *np;

	for_each_matching_node(np, socfpga_early_reset_dt_ids)
		a10_reset_init(np);
}

 
static const struct of_device_id socfpga_reset_dt_ids[] = {
	{ .compatible = "altr,rst-mgr", },
	{   },
};

static int reset_simple_probe(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver reset_socfpga_driver = {
	.probe	= reset_simple_probe,
	.driver = {
		.name		= "socfpga-reset",
		.of_match_table	= socfpga_reset_dt_ids,
	},
};
builtin_platform_driver(reset_socfpga_driver);
