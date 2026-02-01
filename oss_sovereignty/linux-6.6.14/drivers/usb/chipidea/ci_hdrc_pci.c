
 

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/usb/gadget.h>
#include <linux/usb/chipidea.h>
#include <linux/usb/usb_phy_generic.h>

 
#define UDC_DRIVER_NAME   "ci_hdrc_pci"

struct ci_hdrc_pci {
	struct platform_device	*ci;
	struct platform_device	*phy;
};

 
static struct ci_hdrc_platform_data pci_platdata = {
	.name		= UDC_DRIVER_NAME,
	.capoffset	= DEF_CAPOFFSET,
};

static struct ci_hdrc_platform_data langwell_pci_platdata = {
	.name		= UDC_DRIVER_NAME,
	.capoffset	= 0,
};

static struct ci_hdrc_platform_data penwell_pci_platdata = {
	.name		= UDC_DRIVER_NAME,
	.capoffset	= 0,
	.power_budget	= 200,
};

 
static int ci_hdrc_pci_probe(struct pci_dev *pdev,
				       const struct pci_device_id *id)
{
	struct ci_hdrc_platform_data *platdata = (void *)id->driver_data;
	struct ci_hdrc_pci *ci;
	struct resource res[3];
	int retval = 0, nres = 2;

	if (!platdata) {
		dev_err(&pdev->dev, "device doesn't provide driver data\n");
		return -ENODEV;
	}

	ci = devm_kzalloc(&pdev->dev, sizeof(*ci), GFP_KERNEL);
	if (!ci)
		return -ENOMEM;

	retval = pcim_enable_device(pdev);
	if (retval)
		return retval;

	if (!pdev->irq) {
		dev_err(&pdev->dev, "No IRQ, check BIOS/PCI setup!");
		return -ENODEV;
	}

	pci_set_master(pdev);
	pci_try_set_mwi(pdev);

	 
	ci->phy = usb_phy_generic_register();
	if (IS_ERR(ci->phy))
		return PTR_ERR(ci->phy);

	memset(res, 0, sizeof(res));
	res[0].start	= pci_resource_start(pdev, 0);
	res[0].end	= pci_resource_end(pdev, 0);
	res[0].flags	= IORESOURCE_MEM;
	res[1].start	= pdev->irq;
	res[1].flags	= IORESOURCE_IRQ;

	ci->ci = ci_hdrc_add_device(&pdev->dev, res, nres, platdata);
	if (IS_ERR(ci->ci)) {
		dev_err(&pdev->dev, "ci_hdrc_add_device failed!\n");
		usb_phy_generic_unregister(ci->phy);
		return PTR_ERR(ci->ci);
	}

	pci_set_drvdata(pdev, ci);

	return 0;
}

 
static void ci_hdrc_pci_remove(struct pci_dev *pdev)
{
	struct ci_hdrc_pci *ci = pci_get_drvdata(pdev);

	ci_hdrc_remove_device(ci->ci);
	usb_phy_generic_unregister(ci->phy);
}

 
static const struct pci_device_id ci_hdrc_pci_id_table[] = {
	{
		PCI_DEVICE(0x153F, 0x1004),
		.driver_data = (kernel_ulong_t)&pci_platdata,
	},
	{
		PCI_DEVICE(0x153F, 0x1006),
		.driver_data = (kernel_ulong_t)&pci_platdata,
	},
	{
		PCI_VDEVICE(INTEL, 0x0811),
		.driver_data = (kernel_ulong_t)&langwell_pci_platdata,
	},
	{
		PCI_VDEVICE(INTEL, 0x0829),
		.driver_data = (kernel_ulong_t)&penwell_pci_platdata,
	},
	{
		 
		PCI_VDEVICE(INTEL, 0xe006),
		.driver_data = (kernel_ulong_t)&penwell_pci_platdata,
	},
	{ 0 }  
};
MODULE_DEVICE_TABLE(pci, ci_hdrc_pci_id_table);

static struct pci_driver ci_hdrc_pci_driver = {
	.name         =	UDC_DRIVER_NAME,
	.id_table     =	ci_hdrc_pci_id_table,
	.probe        =	ci_hdrc_pci_probe,
	.remove       =	ci_hdrc_pci_remove,
};

module_pci_driver(ci_hdrc_pci_driver);

MODULE_AUTHOR("MIPS - David Lopo <dlopo@chipidea.mips.com>");
MODULE_DESCRIPTION("MIPS CI13XXX USB Peripheral Controller");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ci13xxx_pci");
