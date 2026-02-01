 
#include "ram.h"

int
nv1a_ram_new(struct nvkm_fb *fb, struct nvkm_ram **pram)
{
	struct pci_dev *bridge;
	u32 mem, mib;
	int domain = 0;
	struct pci_dev *pdev = NULL;

	if (dev_is_pci(fb->subdev.device->dev))
		pdev = to_pci_dev(fb->subdev.device->dev);

	if (pdev)
		domain = pci_domain_nr(pdev->bus);

	bridge = pci_get_domain_bus_and_slot(domain, 0, PCI_DEVFN(0, 1));
	if (!bridge) {
		nvkm_error(&fb->subdev, "no bridge device\n");
		return -ENODEV;
	}

	if (fb->subdev.device->chipset == 0x1a) {
		pci_read_config_dword(bridge, 0x7c, &mem);
		mib = ((mem >> 6) & 31) + 1;
	} else {
		pci_read_config_dword(bridge, 0x84, &mem);
		mib = ((mem >> 4) & 127) + 1;
	}

	return nvkm_ram_new_(&nv04_ram_func, fb, NVKM_RAM_TYPE_STOLEN,
			     mib * 1024 * 1024, pram);
}
