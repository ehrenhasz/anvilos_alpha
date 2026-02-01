
 

#include <linux/io.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>

struct rmem {
	struct device *dev;
	struct nvmem_device *nvmem;
	struct reserved_mem *mem;

	phys_addr_t size;
};

static int rmem_read(void *context, unsigned int offset,
		     void *val, size_t bytes)
{
	struct rmem *priv = context;
	size_t available = priv->mem->size;
	loff_t off = offset;
	void *addr;
	int count;

	 
	addr = memremap(priv->mem->base, available, MEMREMAP_WB);
	if (!addr) {
		dev_err(priv->dev, "Failed to remap memory region\n");
		return -ENOMEM;
	}

	count = memory_read_from_buffer(val, bytes, &off, addr, available);

	memunmap(addr);

	return count;
}

static int rmem_probe(struct platform_device *pdev)
{
	struct nvmem_config config = { };
	struct device *dev = &pdev->dev;
	struct reserved_mem *mem;
	struct rmem *priv;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->dev = dev;

	mem = of_reserved_mem_lookup(dev->of_node);
	if (!mem) {
		dev_err(dev, "Failed to lookup reserved memory\n");
		return -EINVAL;
	}
	priv->mem = mem;

	config.dev = dev;
	config.priv = priv;
	config.name = "rmem";
	config.id = NVMEM_DEVID_AUTO;
	config.size = mem->size;
	config.reg_read = rmem_read;

	return PTR_ERR_OR_ZERO(devm_nvmem_register(dev, &config));
}

static const struct of_device_id rmem_match[] = {
	{ .compatible = "nvmem-rmem", },
	{   },
};
MODULE_DEVICE_TABLE(of, rmem_match);

static struct platform_driver rmem_driver = {
	.probe = rmem_probe,
	.driver = {
		.name = "rmem",
		.of_match_table = rmem_match,
	},
};
module_platform_driver(rmem_driver);

MODULE_AUTHOR("Nicolas Saenz Julienne <nsaenzjulienne@suse.de>");
MODULE_DESCRIPTION("Reserved Memory Based nvmem Driver");
MODULE_LICENSE("GPL");
