
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/mfd/core.h>

 
#define VX855_CFG_PMIO_OFFSET	0x88

 
#define VX855_PMIO_ACPI		0x00
#define VX855_PMIO_ACPI_LEN	0x0b

 
#define VX855_PMIO_PPM		0x10
#define VX855_PMIO_PPM_LEN	0x08

 
#define VX855_PMIO_GPPM		0x20
#define VX855_PMIO_R_GPI	0x48
#define VX855_PMIO_R_GPO	0x4c
#define VX855_PMIO_GPPM_LEN	0x33

#define VSPIC_MMIO_SIZE	0x1000

static struct resource vx855_gpio_resources[] = {
	{
		.flags = IORESOURCE_IO,
	},
	{
		.flags = IORESOURCE_IO,
	},
};

static const struct mfd_cell vx855_cells[] = {
	{
		.name = "vx855_gpio",
		.num_resources = ARRAY_SIZE(vx855_gpio_resources),
		.resources = vx855_gpio_resources,

		 
		.ignore_resource_conflicts = true,
	},
};

static int vx855_probe(struct pci_dev *pdev,
				 const struct pci_device_id *id)
{
	int ret;
	u16 gpio_io_offset;

	ret = pci_enable_device(pdev);
	if (ret)
		return -ENODEV;

	pci_read_config_word(pdev, VX855_CFG_PMIO_OFFSET, &gpio_io_offset);
	if (!gpio_io_offset) {
		dev_warn(&pdev->dev,
			"BIOS did not assign PMIO base offset?!?\n");
		ret = -ENODEV;
		goto out;
	}

	 
	gpio_io_offset &= 0xff80;

	 
	vx855_gpio_resources[0].start = gpio_io_offset + VX855_PMIO_R_GPI;
	vx855_gpio_resources[0].end = vx855_gpio_resources[0].start + 3;
	vx855_gpio_resources[1].start = gpio_io_offset + VX855_PMIO_R_GPO;
	vx855_gpio_resources[1].end = vx855_gpio_resources[1].start + 3;

	ret = mfd_add_devices(&pdev->dev, -1, vx855_cells, ARRAY_SIZE(vx855_cells),
			NULL, 0, NULL);

	 
	return -ENODEV;
out:
	pci_disable_device(pdev);
	return ret;
}

static void vx855_remove(struct pci_dev *pdev)
{
	mfd_remove_devices(&pdev->dev);
	pci_disable_device(pdev);
}

static const struct pci_device_id vx855_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_VX855) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, vx855_pci_tbl);

static struct pci_driver vx855_pci_driver = {
	.name		= "vx855",
	.id_table	= vx855_pci_tbl,
	.probe		= vx855_probe,
	.remove		= vx855_remove,
};

module_pci_driver(vx855_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harald Welte <HaraldWelte@viatech.com>");
MODULE_DESCRIPTION("Driver for the VIA VX855 chipset");
