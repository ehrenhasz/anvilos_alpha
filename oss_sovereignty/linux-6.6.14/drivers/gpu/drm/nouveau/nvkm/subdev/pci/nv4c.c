 
#include "priv.h"

static const struct nvkm_pci_func
nv4c_pci_func = {
	.rd32 = nv40_pci_rd32,
	.wr08 = nv40_pci_wr08,
	.wr32 = nv40_pci_wr32,
};

int
nv4c_pci_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_pci **ppci)
{
	return nvkm_pci_new_(&nv4c_pci_func, device, type, inst, ppci);
}
