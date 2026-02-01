

 

#include <linux/acpi.h>
#include <linux/amba/bus.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "init.h"

static const struct acpi_device_id amba_id_list[] = {
	{"ARMH0061", 0},  
	{"ARMH0330", 0},  
	{"ARMHC501", 0},  
	{"ARMHC502", 0},  
	{"ARMHC503", 0},  
	{"ARMHC979", 0},  
	{"ARMHC97C", 0},  
	{"ARMHC98D", 0},  
	{"ARMHC9CA", 0},  
	{"ARMHC9FF", 0},  
	{"", 0},
};

static void amba_register_dummy_clk(void)
{
	static struct clk *amba_dummy_clk;

	 
	if (amba_dummy_clk)
		return;

	amba_dummy_clk = clk_register_fixed_rate(NULL, "apb_pclk", NULL, 0, 0);
	clk_register_clkdev(amba_dummy_clk, "apb_pclk", NULL);
}

static int amba_handler_attach(struct acpi_device *adev,
				const struct acpi_device_id *id)
{
	struct acpi_device *parent = acpi_dev_parent(adev);
	struct amba_device *dev;
	struct resource_entry *rentry;
	struct list_head resource_list;
	bool address_found = false;
	int irq_no = 0;
	int ret;

	 
	if (adev->physical_node_count)
		return 0;

	dev = amba_device_alloc(dev_name(&adev->dev), 0, 0);
	if (!dev) {
		dev_err(&adev->dev, "%s(): amba_device_alloc() failed\n",
			__func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&resource_list);
	ret = acpi_dev_get_resources(adev, &resource_list, NULL, NULL);
	if (ret < 0)
		goto err_free;

	list_for_each_entry(rentry, &resource_list, node) {
		switch (resource_type(rentry->res)) {
		case IORESOURCE_MEM:
			if (!address_found) {
				dev->res = *rentry->res;
				dev->res.name = dev_name(&dev->dev);
				address_found = true;
			}
			break;
		case IORESOURCE_IRQ:
			if (irq_no < AMBA_NR_IRQS)
				dev->irq[irq_no++] = rentry->res->start;
			break;
		default:
			dev_warn(&adev->dev, "Invalid resource\n");
			break;
		}
	}

	acpi_dev_free_resource_list(&resource_list);

	 
	if (parent)
		dev->dev.parent = acpi_get_first_physical_node(parent);

	ACPI_COMPANION_SET(&dev->dev, adev);

	ret = amba_device_add(dev, &iomem_resource);
	if (ret) {
		dev_err(&adev->dev, "%s(): amba_device_add() failed (%d)\n",
		       __func__, ret);
		goto err_free;
	}

	return 1;

err_free:
	amba_device_put(dev);
	return ret;
}

static struct acpi_scan_handler amba_handler = {
	.ids = amba_id_list,
	.attach = amba_handler_attach,
};

void __init acpi_amba_init(void)
{
	amba_register_dummy_clk();
	acpi_scan_add_handler(&amba_handler);
}
