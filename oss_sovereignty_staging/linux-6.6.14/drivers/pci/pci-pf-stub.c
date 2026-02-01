
 

#include <linux/module.h>
#include <linux/pci.h>

 
static const struct pci_device_id pci_pf_stub_whitelist[] = {
	{ PCI_VDEVICE(AMAZON, 0x0053) },
	 
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, pci_pf_stub_whitelist);

static int pci_pf_stub_probe(struct pci_dev *dev,
			     const struct pci_device_id *id)
{
	pci_info(dev, "claimed by pci-pf-stub\n");
	return 0;
}

static struct pci_driver pf_stub_driver = {
	.name			= "pci-pf-stub",
	.id_table		= pci_pf_stub_whitelist,
	.probe			= pci_pf_stub_probe,
	.sriov_configure	= pci_sriov_configure_simple,
};
module_pci_driver(pf_stub_driver);

MODULE_LICENSE("GPL");
