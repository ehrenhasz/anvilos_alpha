 
#include "priv.h"

int
g92_pcie_version_supported(struct nvkm_pci *pci)
{
	if ((nvkm_pci_rd32(pci, 0x460) & 0x200) == 0x200)
		return 2;
	return 1;
}

static const struct nvkm_pci_func
g92_pci_func = {
	.init = g84_pci_init,
	.rd32 = nv40_pci_rd32,
	.wr08 = nv40_pci_wr08,
	.wr32 = nv40_pci_wr32,
	.msi_rearm = nv46_pci_msi_rearm,

	.pcie.init = g84_pcie_init,
	.pcie.set_link = g84_pcie_set_link,

	.pcie.max_speed = g84_pcie_max_speed,
	.pcie.cur_speed = g84_pcie_cur_speed,

	.pcie.set_version = g84_pcie_set_version,
	.pcie.version = g84_pcie_version,
	.pcie.version_supported = g92_pcie_version_supported,
};

int
g92_pci_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	    struct nvkm_pci **ppci)
{
	return nvkm_pci_new_(&g92_pci_func, device, type, inst, ppci);
}
