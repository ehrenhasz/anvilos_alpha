 
#include "priv.h"
#include "agp.h"

#include <core/option.h>
#include <core/pci.h>

void
nvkm_pci_msi_rearm(struct nvkm_device *device)
{
	struct nvkm_pci *pci = device->pci;

	if (pci && pci->msi)
		pci->func->msi_rearm(pci);
}

u32
nvkm_pci_rd32(struct nvkm_pci *pci, u16 addr)
{
	return pci->func->rd32(pci, addr);
}

void
nvkm_pci_wr08(struct nvkm_pci *pci, u16 addr, u8 data)
{
	pci->func->wr08(pci, addr, data);
}

void
nvkm_pci_wr32(struct nvkm_pci *pci, u16 addr, u32 data)
{
	pci->func->wr32(pci, addr, data);
}

u32
nvkm_pci_mask(struct nvkm_pci *pci, u16 addr, u32 mask, u32 value)
{
	u32 data = pci->func->rd32(pci, addr);
	pci->func->wr32(pci, addr, (data & ~mask) | value);
	return data;
}

void
nvkm_pci_rom_shadow(struct nvkm_pci *pci, bool shadow)
{
	u32 data = nvkm_pci_rd32(pci, 0x0050);
	if (shadow)
		data |=  0x00000001;
	else
		data &= ~0x00000001;
	nvkm_pci_wr32(pci, 0x0050, data);
}

static int
nvkm_pci_fini(struct nvkm_subdev *subdev, bool suspend)
{
	struct nvkm_pci *pci = nvkm_pci(subdev);

	if (pci->agp.bridge)
		nvkm_agp_fini(pci);

	return 0;
}

static int
nvkm_pci_preinit(struct nvkm_subdev *subdev)
{
	struct nvkm_pci *pci = nvkm_pci(subdev);
	if (pci->agp.bridge)
		nvkm_agp_preinit(pci);
	return 0;
}

static int
nvkm_pci_oneinit(struct nvkm_subdev *subdev)
{
	struct nvkm_pci *pci = nvkm_pci(subdev);
	int ret;

	if (pci_is_pcie(pci->pdev)) {
		ret = nvkm_pcie_oneinit(pci);
		if (ret)
			return ret;
	}

	return 0;
}

static int
nvkm_pci_init(struct nvkm_subdev *subdev)
{
	struct nvkm_pci *pci = nvkm_pci(subdev);
	int ret;

	if (pci->agp.bridge) {
		ret = nvkm_agp_init(pci);
		if (ret)
			return ret;
	} else if (pci_is_pcie(pci->pdev)) {
		nvkm_pcie_init(pci);
	}

	if (pci->func->init)
		pci->func->init(pci);

	 
	if (pci->msi)
		pci->func->msi_rearm(pci);

	return 0;
}

static void *
nvkm_pci_dtor(struct nvkm_subdev *subdev)
{
	struct nvkm_pci *pci = nvkm_pci(subdev);

	nvkm_agp_dtor(pci);

	if (pci->msi)
		pci_disable_msi(pci->pdev);

	return nvkm_pci(subdev);
}

static const struct nvkm_subdev_func
nvkm_pci_func = {
	.dtor = nvkm_pci_dtor,
	.oneinit = nvkm_pci_oneinit,
	.preinit = nvkm_pci_preinit,
	.init = nvkm_pci_init,
	.fini = nvkm_pci_fini,
};

int
nvkm_pci_new_(const struct nvkm_pci_func *func, struct nvkm_device *device,
	      enum nvkm_subdev_type type, int inst, struct nvkm_pci **ppci)
{
	struct nvkm_pci *pci;

	if (!(pci = *ppci = kzalloc(sizeof(**ppci), GFP_KERNEL)))
		return -ENOMEM;
	nvkm_subdev_ctor(&nvkm_pci_func, device, type, inst, &pci->subdev);
	pci->func = func;
	pci->pdev = device->func->pci(device)->pdev;
	pci->pcie.speed = -1;
	pci->pcie.width = -1;

	if (device->type == NVKM_DEVICE_AGP)
		nvkm_agp_ctor(pci);

	switch (pci->pdev->device & 0x0ff0) {
	case 0x00f0:
	case 0x02e0:
		 
		break;
	default:
		switch (device->chipset) {
		case 0xaa:
			 
			break;
		default:
			pci->msi = true;
			break;
		}
	}

#ifdef __BIG_ENDIAN
	pci->msi = false;
#endif

	pci->msi = nvkm_boolopt(device->cfgopt, "NvMSI", pci->msi);
	if (pci->msi && func->msi_rearm) {
		pci->msi = pci_enable_msi(pci->pdev) == 0;
		if (pci->msi)
			nvkm_debug(&pci->subdev, "MSI enabled\n");
	} else {
		pci->msi = false;
	}

	return 0;
}
