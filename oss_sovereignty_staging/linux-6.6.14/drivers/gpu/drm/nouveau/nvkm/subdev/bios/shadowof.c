 
#include "priv.h"

#include <core/pci.h>

#if defined(__powerpc__)
struct priv {
	const void __iomem *data;
	int size;
};

static u32
of_read(void *data, u32 offset, u32 length, struct nvkm_bios *bios)
{
	struct priv *priv = data;
	if (offset < priv->size) {
		length = min_t(u32, length, priv->size - offset);
		memcpy_fromio(bios->data + offset, priv->data + offset, length);
		return length;
	}
	return 0;
}

static u32
of_size(void *data)
{
	struct priv *priv = data;
	return priv->size;
}

static void *
of_init(struct nvkm_bios *bios, const char *name)
{
	struct nvkm_device *device = bios->subdev.device;
	struct pci_dev *pdev = device->func->pci(device)->pdev;
	struct device_node *dn;
	struct priv *priv;
	if (!(dn = pci_device_to_OF_node(pdev)))
		return ERR_PTR(-ENODEV);
	if (!(priv = kzalloc(sizeof(*priv), GFP_KERNEL)))
		return ERR_PTR(-ENOMEM);
	if ((priv->data = of_get_property(dn, "NVDA,BMP", &priv->size)))
		return priv;
	kfree(priv);
	return ERR_PTR(-EINVAL);
}

const struct nvbios_source
nvbios_of = {
	.name = "OpenFirmware",
	.init = of_init,
	.fini = (void(*)(void *))kfree,
	.read = of_read,
	.size = of_size,
	.rw = false,
	.ignore_checksum = true,
	.no_pcir = true,
};
#else
const struct nvbios_source
nvbios_of = {
};
#endif
