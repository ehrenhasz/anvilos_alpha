
 
#include <linux/pci.h>
#include <drm/drm_file.h>
#include "vbox_drv.h"

int vbox_mm_init(struct vbox_private *vbox)
{
	int ret;
	resource_size_t base, size;
	struct drm_device *dev = &vbox->ddev;
	struct pci_dev *pdev = to_pci_dev(dev->dev);

	base = pci_resource_start(pdev, 0);
	size = pci_resource_len(pdev, 0);

	 
	devm_arch_phys_wc_add(&pdev->dev, base, size);

	ret = drmm_vram_helper_init(dev, base, vbox->available_vram_size);
	if (ret) {
		DRM_ERROR("Error initializing VRAM MM; %d\n", ret);
		return ret;
	}

	return 0;
}
