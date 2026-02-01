
 

#include <linux/bits.h>
#include <linux/export.h>
#include <linux/pci.h>
#include <linux/platform_data/x86/p2sb.h>

#include <asm/cpu_device_id.h>
#include <asm/intel-family.h>

#define P2SBC			0xe0
#define P2SBC_HIDE		BIT(8)

#define P2SB_DEVFN_DEFAULT	PCI_DEVFN(31, 1)

static const struct x86_cpu_id p2sb_cpu_ids[] = {
	X86_MATCH_INTEL_FAM6_MODEL(ATOM_GOLDMONT,	PCI_DEVFN(13, 0)),
	{}
};

static int p2sb_get_devfn(unsigned int *devfn)
{
	unsigned int fn = P2SB_DEVFN_DEFAULT;
	const struct x86_cpu_id *id;

	id = x86_match_cpu(p2sb_cpu_ids);
	if (id)
		fn = (unsigned int)id->driver_data;

	*devfn = fn;
	return 0;
}

 
static int p2sb_read_bar0(struct pci_dev *pdev, struct resource *mem)
{
	struct resource *bar0 = &pdev->resource[0];

	 
	memset(mem, 0, sizeof(*mem));

	 
	mem->start = bar0->start;
	mem->end = bar0->end;
	mem->flags = bar0->flags;
	mem->desc = bar0->desc;

	return 0;
}

static int p2sb_scan_and_read(struct pci_bus *bus, unsigned int devfn, struct resource *mem)
{
	struct pci_dev *pdev;
	int ret;

	pdev = pci_scan_single_device(bus, devfn);
	if (!pdev)
		return -ENODEV;

	ret = p2sb_read_bar0(pdev, mem);

	pci_stop_and_remove_bus_device(pdev);
	return ret;
}

 
int p2sb_bar(struct pci_bus *bus, unsigned int devfn, struct resource *mem)
{
	struct pci_dev *pdev_p2sb;
	unsigned int devfn_p2sb;
	u32 value = P2SBC_HIDE;
	int ret;

	 
	ret = p2sb_get_devfn(&devfn_p2sb);
	if (ret)
		return ret;

	 
	bus = bus ?: pci_find_bus(0, 0);

	 
	pci_lock_rescan_remove();

	 
	pci_bus_read_config_dword(bus, devfn_p2sb, P2SBC, &value);
	if (value & P2SBC_HIDE)
		pci_bus_write_config_dword(bus, devfn_p2sb, P2SBC, 0);

	pdev_p2sb = pci_scan_single_device(bus, devfn_p2sb);
	if (devfn)
		ret = p2sb_scan_and_read(bus, devfn, mem);
	else
		ret = p2sb_read_bar0(pdev_p2sb, mem);
	pci_stop_and_remove_bus_device(pdev_p2sb);

	 
	if (value & P2SBC_HIDE)
		pci_bus_write_config_dword(bus, devfn_p2sb, P2SBC, P2SBC_HIDE);

	pci_unlock_rescan_remove();

	if (ret)
		return ret;

	if (mem->flags == 0)
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL_GPL(p2sb_bar);
