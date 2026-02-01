 

#include <linux/pci.h>

#include <drm/drm_device.h>
#include <drm/radeon_drm.h>

#include "radeon.h"

#if IS_ENABLED(CONFIG_AGP)

struct radeon_agpmode_quirk {
	u32 hostbridge_vendor;
	u32 hostbridge_device;
	u32 chip_vendor;
	u32 chip_device;
	u32 subsys_vendor;
	u32 subsys_device;
	u32 default_mode;
};

static struct radeon_agpmode_quirk radeon_agpmode_quirk_list[] = {
	 
	{ PCI_VENDOR_ID_INTEL, 0x2550, PCI_VENDOR_ID_ATI, 0x4152, 0x1458, 0x4038, 4},
	 
	{ PCI_VENDOR_ID_INTEL, 0x2570, PCI_VENDOR_ID_ATI, 0x4a4e, PCI_VENDOR_ID_DELL, 0x5106, 4},
	 
	{ PCI_VENDOR_ID_INTEL, 0x2570, PCI_VENDOR_ID_ATI, 0x5964,
		0x148c, 0x2073, 4},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3340, PCI_VENDOR_ID_ATI, 0x4c59,
		PCI_VENDOR_ID_IBM, 0x052f, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3340, PCI_VENDOR_ID_ATI, 0x4e50,
		PCI_VENDOR_ID_IBM, 0x0550, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3340, PCI_VENDOR_ID_ATI, 0x4c66,
		PCI_VENDOR_ID_IBM, 0x054d, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3340, PCI_VENDOR_ID_ATI, 0x4c57,
		PCI_VENDOR_ID_IBM, 0x0530, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3340, PCI_VENDOR_ID_ATI, 0x4e54,
		PCI_VENDOR_ID_IBM, 0x054f, 2},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3340, PCI_VENDOR_ID_ATI, 0x5c61,
		PCI_VENDOR_ID_SONY, 0x816b, 2},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3340, PCI_VENDOR_ID_ATI, 0x5c61,
		PCI_VENDOR_ID_SONY, 0x8195, 8},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3575, PCI_VENDOR_ID_ATI, 0x4c59,
		PCI_VENDOR_ID_DELL, 0x00e3, 2},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3580, PCI_VENDOR_ID_ATI, 0x4c66,
		PCI_VENDOR_ID_DELL, 0x0149, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3340, PCI_VENDOR_ID_ATI, 0x4c66,
		PCI_VENDOR_ID_IBM, 0x0531, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3580, PCI_VENDOR_ID_ATI, 0x4e50,
		0x1025, 0x0061, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3580, PCI_VENDOR_ID_ATI, 0x4e50,
		0x1025, 0x0064, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3580, PCI_VENDOR_ID_ATI, 0x4e50,
		PCI_VENDOR_ID_ASUSTEK, 0x1942, 1},
	 
	{ PCI_VENDOR_ID_INTEL, 0x3580, PCI_VENDOR_ID_ATI, 0x4e50,
		0x10cf, 0x127f, 1},
	 
	{ 0x1849, 0x3189, PCI_VENDOR_ID_ATI, 0x5960,
		0x1787, 0x5960, 4},
	 
	{ PCI_VENDOR_ID_VIA, 0x0204, PCI_VENDOR_ID_ATI, 0x5960,
		0x17af, 0x2020, 4},
	 
	{ PCI_VENDOR_ID_VIA, 0x0269, PCI_VENDOR_ID_ATI, 0x4153,
		PCI_VENDOR_ID_ASUSTEK, 0x003c, 4},
	 
	{ PCI_VENDOR_ID_VIA, 0x0305, PCI_VENDOR_ID_ATI, 0x514c,
		PCI_VENDOR_ID_ATI, 0x013a, 2},
	 
	{ PCI_VENDOR_ID_VIA, 0x0691, PCI_VENDOR_ID_ATI, 0x5960,
		PCI_VENDOR_ID_ASUSTEK, 0x004c, 2},
	 
	{ PCI_VENDOR_ID_VIA, 0x0691, PCI_VENDOR_ID_ATI, 0x5960,
		PCI_VENDOR_ID_ASUSTEK, 0x0054, 2},
	 
	{ PCI_VENDOR_ID_VIA, 0x3189, PCI_VENDOR_ID_ATI, 0x514d,
		0x174b, 0x7149, 4},
	 
	{ PCI_VENDOR_ID_VIA, 0x3189, PCI_VENDOR_ID_ATI, 0x5960,
		0x1462, 0x0380, 4},
	 
	{ PCI_VENDOR_ID_VIA, 0x3189, PCI_VENDOR_ID_ATI, 0x5964,
		0x148c, 0x2073, 4},
	 
	{ PCI_VENDOR_ID_ATI, 0xcbb2, PCI_VENDOR_ID_ATI, 0x5c61,
		PCI_VENDOR_ID_SONY, 0x8175, 1},
	{ 0, 0, 0, 0, 0, 0, 0 },
};

struct radeon_agp_head *radeon_agp_head_init(struct drm_device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct radeon_agp_head *head;

	head = kzalloc(sizeof(*head), GFP_KERNEL);
	if (!head)
		return NULL;
	head->bridge = agp_find_bridge(pdev);
	if (!head->bridge) {
		head->bridge = agp_backend_acquire(pdev);
		if (!head->bridge) {
			kfree(head);
			return NULL;
		}
		agp_copy_info(head->bridge, &head->agp_info);
		agp_backend_release(head->bridge);
	} else {
		agp_copy_info(head->bridge, &head->agp_info);
	}
	if (head->agp_info.chipset == NOT_SUPPORTED) {
		kfree(head);
		return NULL;
	}
	INIT_LIST_HEAD(&head->memory);
	head->cant_use_aperture = head->agp_info.cant_use_aperture;
	head->page_mask = head->agp_info.page_mask;
	head->base = head->agp_info.aper_base;

	return head;
}

static int radeon_agp_head_acquire(struct radeon_device *rdev)
{
	struct drm_device *dev = rdev->ddev;
	struct pci_dev *pdev = to_pci_dev(dev->dev);

	if (!rdev->agp)
		return -ENODEV;
	if (rdev->agp->acquired)
		return -EBUSY;
	rdev->agp->bridge = agp_backend_acquire(pdev);
	if (!rdev->agp->bridge)
		return -ENODEV;
	rdev->agp->acquired = 1;
	return 0;
}

static int radeon_agp_head_release(struct radeon_device *rdev)
{
	if (!rdev->agp || !rdev->agp->acquired)
		return -EINVAL;
	agp_backend_release(rdev->agp->bridge);
	rdev->agp->acquired = 0;
	return 0;
}

static int radeon_agp_head_enable(struct radeon_device *rdev, struct radeon_agp_mode mode)
{
	if (!rdev->agp || !rdev->agp->acquired)
		return -EINVAL;

	rdev->agp->mode = mode.mode;
	agp_enable(rdev->agp->bridge, mode.mode);
	rdev->agp->enabled = 1;
	return 0;
}

static int radeon_agp_head_info(struct radeon_device *rdev, struct radeon_agp_info *info)
{
	struct agp_kern_info *kern;

	if (!rdev->agp || !rdev->agp->acquired)
		return -EINVAL;

	kern = &rdev->agp->agp_info;
	info->agp_version_major = kern->version.major;
	info->agp_version_minor = kern->version.minor;
	info->mode = kern->mode;
	info->aperture_base = kern->aper_base;
	info->aperture_size = kern->aper_size * 1024 * 1024;
	info->memory_allowed = kern->max_memory << PAGE_SHIFT;
	info->memory_used = kern->current_memory << PAGE_SHIFT;
	info->id_vendor = kern->device->vendor;
	info->id_device = kern->device->device;

	return 0;
}
#endif

int radeon_agp_init(struct radeon_device *rdev)
{
#if IS_ENABLED(CONFIG_AGP)
	struct radeon_agpmode_quirk *p = radeon_agpmode_quirk_list;
	struct radeon_agp_mode mode;
	struct radeon_agp_info info;
	uint32_t agp_status;
	int default_mode;
	bool is_v3;
	int ret;

	 
	ret = radeon_agp_head_acquire(rdev);
	if (ret) {
		DRM_ERROR("Unable to acquire AGP: %d\n", ret);
		return ret;
	}

	ret = radeon_agp_head_info(rdev, &info);
	if (ret) {
		radeon_agp_head_release(rdev);
		DRM_ERROR("Unable to get AGP info: %d\n", ret);
		return ret;
	}

	if (rdev->agp->agp_info.aper_size < 32) {
		radeon_agp_head_release(rdev);
		dev_warn(rdev->dev, "AGP aperture too small (%zuM) "
			"need at least 32M, disabling AGP\n",
			rdev->agp->agp_info.aper_size);
		return -EINVAL;
	}

	mode.mode = info.mode;
	 
	if (rdev->family <= CHIP_RV350)
		agp_status = (RREG32(RADEON_AGP_STATUS) | RADEON_AGPv3_MODE) & mode.mode;
	else
		agp_status = mode.mode;
	is_v3 = !!(agp_status & RADEON_AGPv3_MODE);

	if (is_v3) {
		default_mode = (agp_status & RADEON_AGPv3_8X_MODE) ? 8 : 4;
	} else {
		if (agp_status & RADEON_AGP_4X_MODE) {
			default_mode = 4;
		} else if (agp_status & RADEON_AGP_2X_MODE) {
			default_mode = 2;
		} else {
			default_mode = 1;
		}
	}

	 
	while (p && p->chip_device != 0) {
		if (info.id_vendor == p->hostbridge_vendor &&
		    info.id_device == p->hostbridge_device &&
		    rdev->pdev->vendor == p->chip_vendor &&
		    rdev->pdev->device == p->chip_device &&
		    rdev->pdev->subsystem_vendor == p->subsys_vendor &&
		    rdev->pdev->subsystem_device == p->subsys_device) {
			default_mode = p->default_mode;
		}
		++p;
	}

	if (radeon_agpmode > 0) {
		if ((radeon_agpmode < (is_v3 ? 4 : 1)) ||
		    (radeon_agpmode > (is_v3 ? 8 : 4)) ||
		    (radeon_agpmode & (radeon_agpmode - 1))) {
			DRM_ERROR("Illegal AGP Mode: %d (valid %s), leaving at %d\n",
				  radeon_agpmode, is_v3 ? "4, 8" : "1, 2, 4",
				  default_mode);
			radeon_agpmode = default_mode;
		} else {
			DRM_INFO("AGP mode requested: %d\n", radeon_agpmode);
		}
	} else {
		radeon_agpmode = default_mode;
	}

	mode.mode &= ~RADEON_AGP_MODE_MASK;
	if (is_v3) {
		switch (radeon_agpmode) {
		case 8:
			mode.mode |= RADEON_AGPv3_8X_MODE;
			break;
		case 4:
		default:
			mode.mode |= RADEON_AGPv3_4X_MODE;
			break;
		}
	} else {
		switch (radeon_agpmode) {
		case 4:
			mode.mode |= RADEON_AGP_4X_MODE;
			break;
		case 2:
			mode.mode |= RADEON_AGP_2X_MODE;
			break;
		case 1:
		default:
			mode.mode |= RADEON_AGP_1X_MODE;
			break;
		}
	}

	mode.mode &= ~RADEON_AGP_FW_MODE;  
	ret = radeon_agp_head_enable(rdev, mode);
	if (ret) {
		DRM_ERROR("Unable to enable AGP (mode = 0x%lx)\n", mode.mode);
		radeon_agp_head_release(rdev);
		return ret;
	}

	rdev->mc.agp_base = rdev->agp->agp_info.aper_base;
	rdev->mc.gtt_size = rdev->agp->agp_info.aper_size << 20;
	rdev->mc.gtt_start = rdev->mc.agp_base;
	rdev->mc.gtt_end = rdev->mc.gtt_start + rdev->mc.gtt_size - 1;
	dev_info(rdev->dev, "GTT: %lluM 0x%08llX - 0x%08llX\n",
		rdev->mc.gtt_size >> 20, rdev->mc.gtt_start, rdev->mc.gtt_end);

	 
	if (rdev->family < CHIP_R200) {
		WREG32(RADEON_AGP_CNTL, RREG32(RADEON_AGP_CNTL) | 0x000e0000);
	}
	return 0;
#else
	return 0;
#endif
}

void radeon_agp_resume(struct radeon_device *rdev)
{
#if IS_ENABLED(CONFIG_AGP)
	int r;
	if (rdev->flags & RADEON_IS_AGP) {
		r = radeon_agp_init(rdev);
		if (r)
			dev_warn(rdev->dev, "radeon AGP reinit failed\n");
	}
#endif
}

void radeon_agp_fini(struct radeon_device *rdev)
{
#if IS_ENABLED(CONFIG_AGP)
	if (rdev->agp && rdev->agp->acquired) {
		radeon_agp_head_release(rdev);
	}
#endif
}

void radeon_agp_suspend(struct radeon_device *rdev)
{
	radeon_agp_fini(rdev);
}
