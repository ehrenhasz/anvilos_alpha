 
#include "priv.h"

#include <core/pci.h>

 
void
nv46_pci_msi_rearm(struct nvkm_pci *pci)
{
	struct nvkm_device *device = pci->subdev.device;
	struct pci_dev *pdev = device->func->pci(device)->pdev;
	pci_write_config_byte(pdev, 0x68, 0xff);
}

static const struct nvkm_pci_func
nv46_pci_func = {
	.rd32 = nv40_pci_rd32,
	.wr08 = nv40_pci_wr08,
	.wr32 = nv40_pci_wr32,
	.msi_rearm = nv46_pci_msi_rearm,
};

int
nv46_pci_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_pci **ppci)
{
	return nvkm_pci_new_(&nv46_pci_func, device, type, inst, ppci);
}
