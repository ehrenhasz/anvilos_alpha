 
#include "priv.h"

static void
gp100_pci_msi_rearm(struct nvkm_pci *pci)
{
	nvkm_pci_wr32(pci, 0x0704, 0x00000000);
}

static const struct nvkm_pci_func
gp100_pci_func = {
	.rd32 = nv40_pci_rd32,
	.wr08 = nv40_pci_wr08,
	.wr32 = nv40_pci_wr32,
	.msi_rearm = gp100_pci_msi_rearm,
};

int
gp100_pci_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_pci **ppci)
{
	return nvkm_pci_new_(&gp100_pci_func, device, type, inst, ppci);
}
