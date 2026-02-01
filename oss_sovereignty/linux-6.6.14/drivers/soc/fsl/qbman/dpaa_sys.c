 

#include <linux/dma-mapping.h>
#include "dpaa_sys.h"

 
int qbman_init_private_mem(struct device *dev, int idx, dma_addr_t *addr,
				size_t *size)
{
	struct device_node *mem_node;
	struct reserved_mem *rmem;
	int err;
	__be32 *res_array;

	mem_node = of_parse_phandle(dev->of_node, "memory-region", idx);
	if (!mem_node) {
		dev_err(dev, "No memory-region found for index %d\n", idx);
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(mem_node);
	if (!rmem) {
		dev_err(dev, "of_reserved_mem_lookup() returned NULL\n");
		return -ENODEV;
	}
	*addr = rmem->base;
	*size = rmem->size;

	 
	if (!of_property_present(mem_node, "reg")) {
		struct property *prop;

		prop = devm_kzalloc(dev, sizeof(*prop), GFP_KERNEL);
		if (!prop)
			return -ENOMEM;
		prop->value = res_array = devm_kzalloc(dev, sizeof(__be32) * 4,
						       GFP_KERNEL);
		if (!prop->value)
			return -ENOMEM;
		res_array[0] = cpu_to_be32(upper_32_bits(*addr));
		res_array[1] = cpu_to_be32(lower_32_bits(*addr));
		res_array[2] = cpu_to_be32(upper_32_bits(*size));
		res_array[3] = cpu_to_be32(lower_32_bits(*size));
		prop->length = sizeof(__be32) * 4;
		prop->name = devm_kstrdup(dev, "reg", GFP_KERNEL);
		if (!prop->name)
			return -ENOMEM;
		err = of_add_property(mem_node, prop);
		if (err)
			return err;
	}

	return 0;
}
