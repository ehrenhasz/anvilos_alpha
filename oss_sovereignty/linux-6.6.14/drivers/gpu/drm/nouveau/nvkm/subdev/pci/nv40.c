 
#include "priv.h"

u32
nv40_pci_rd32(struct nvkm_pci *pci, u16 addr)
{
	struct nvkm_device *device = pci->subdev.device;
	return nvkm_rd32(device, 0x088000 + addr);
}

void
nv40_pci_wr08(struct nvkm_pci *pci, u16 addr, u8 data)
{
	struct nvkm_device *device = pci->subdev.device;
	nvkm_wr08(device, 0x088000 + addr, data);
}

void
nv40_pci_wr32(struct nvkm_pci *pci, u16 addr, u32 data)
{
	struct nvkm_device *device = pci->subdev.device;
	nvkm_wr32(device, 0x088000 + addr, data);
}

void
nv40_pci_msi_rearm(struct nvkm_pci *pci)
{
	nvkm_pci_wr08(pci, 0x0068, 0xff);
}

static const struct nvkm_pci_func
nv40_pci_func = {
	.rd32 = nv40_pci_rd32,
	.wr08 = nv40_pci_wr08,
	.wr32 = nv40_pci_wr32,
	.msi_rearm = nv40_pci_msi_rearm,
};

int
nv40_pci_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_pci **ppci)
{
	return nvkm_pci_new_(&nv40_pci_func, device, type, inst, ppci);
}
