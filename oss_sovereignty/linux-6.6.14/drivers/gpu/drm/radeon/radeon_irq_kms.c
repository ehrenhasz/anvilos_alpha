 

#include <linux/pci.h>
#include <linux/pm_runtime.h>

#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <drm/radeon_drm.h>

#include "atom.h"
#include "radeon.h"
#include "radeon_kms.h"
#include "radeon_reg.h"


#define RADEON_WAIT_IDLE_TIMEOUT 200

 
static irqreturn_t radeon_driver_irq_handler_kms(int irq, void *arg)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct radeon_device *rdev = dev->dev_private;
	irqreturn_t ret;

	ret = radeon_irq_process(rdev);
	if (ret == IRQ_HANDLED)
		pm_runtime_mark_last_busy(dev->dev);
	return ret;
}

 
 
static void radeon_hotplug_work_func(struct work_struct *work)
{
	struct radeon_device *rdev = container_of(work, struct radeon_device,
						  hotplug_work.work);
	struct drm_device *dev = rdev->ddev;
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct drm_connector *connector;

	 
	if (!rdev->mode_info.mode_config_initialized)
		return;

	mutex_lock(&mode_config->mutex);
	list_for_each_entry(connector, &mode_config->connector_list, head)
		radeon_connector_hotplug(connector);
	mutex_unlock(&mode_config->mutex);
	 
	drm_helper_hpd_irq_event(dev);
}

static void radeon_dp_work_func(struct work_struct *work)
{
	struct radeon_device *rdev = container_of(work, struct radeon_device,
						  dp_work);
	struct drm_device *dev = rdev->ddev;
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct drm_connector *connector;

	mutex_lock(&mode_config->mutex);
	list_for_each_entry(connector, &mode_config->connector_list, head)
		radeon_connector_hotplug(connector);
	mutex_unlock(&mode_config->mutex);
}

 
static void radeon_driver_irq_preinstall_kms(struct drm_device *dev)
{
	struct radeon_device *rdev = dev->dev_private;
	unsigned long irqflags;
	unsigned i;

	spin_lock_irqsave(&rdev->irq.lock, irqflags);
	 
	for (i = 0; i < RADEON_NUM_RINGS; i++)
		atomic_set(&rdev->irq.ring_int[i], 0);
	rdev->irq.dpm_thermal = false;
	for (i = 0; i < RADEON_MAX_HPD_PINS; i++)
		rdev->irq.hpd[i] = false;
	for (i = 0; i < RADEON_MAX_CRTCS; i++) {
		rdev->irq.crtc_vblank_int[i] = false;
		atomic_set(&rdev->irq.pflip[i], 0);
		rdev->irq.afmt[i] = false;
	}
	radeon_irq_set(rdev);
	spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
	 
	radeon_irq_process(rdev);
}

 
static int radeon_driver_irq_postinstall_kms(struct drm_device *dev)
{
	struct radeon_device *rdev = dev->dev_private;

	if (ASIC_IS_AVIVO(rdev))
		dev->max_vblank_count = 0x00ffffff;
	else
		dev->max_vblank_count = 0x001fffff;

	return 0;
}

 
static void radeon_driver_irq_uninstall_kms(struct drm_device *dev)
{
	struct radeon_device *rdev = dev->dev_private;
	unsigned long irqflags;
	unsigned i;

	if (rdev == NULL) {
		return;
	}
	spin_lock_irqsave(&rdev->irq.lock, irqflags);
	 
	for (i = 0; i < RADEON_NUM_RINGS; i++)
		atomic_set(&rdev->irq.ring_int[i], 0);
	rdev->irq.dpm_thermal = false;
	for (i = 0; i < RADEON_MAX_HPD_PINS; i++)
		rdev->irq.hpd[i] = false;
	for (i = 0; i < RADEON_MAX_CRTCS; i++) {
		rdev->irq.crtc_vblank_int[i] = false;
		atomic_set(&rdev->irq.pflip[i], 0);
		rdev->irq.afmt[i] = false;
	}
	radeon_irq_set(rdev);
	spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
}

static int radeon_irq_install(struct radeon_device *rdev, int irq)
{
	struct drm_device *dev = rdev->ddev;
	int ret;

	if (irq == IRQ_NOTCONNECTED)
		return -ENOTCONN;

	radeon_driver_irq_preinstall_kms(dev);

	 
	ret = request_irq(irq, radeon_driver_irq_handler_kms,
			  IRQF_SHARED, dev->driver->name, dev);
	if (ret)
		return ret;

	radeon_driver_irq_postinstall_kms(dev);

	return 0;
}

static void radeon_irq_uninstall(struct radeon_device *rdev)
{
	struct drm_device *dev = rdev->ddev;
	struct pci_dev *pdev = to_pci_dev(dev->dev);

	radeon_driver_irq_uninstall_kms(dev);
	free_irq(pdev->irq, dev);
}

 
static bool radeon_msi_ok(struct radeon_device *rdev)
{
	 
	if (rdev->family < CHIP_RV380)
		return false;

	 
	if (rdev->flags & RADEON_IS_AGP)
		return false;

	 
	if (rdev->family < CHIP_BONAIRE) {
		dev_info(rdev->dev, "radeon: MSI limited to 32-bit\n");
		rdev->pdev->no_64bit_msi = 1;
	}

	 
	if (radeon_msi == 1)
		return true;
	else if (radeon_msi == 0)
		return false;

	 
	 
	if ((rdev->pdev->device == 0x791f) &&
	    (rdev->pdev->subsystem_vendor == 0x103c) &&
	    (rdev->pdev->subsystem_device == 0x30c2))
		return true;

	 
	if ((rdev->pdev->device == 0x791f) &&
	    (rdev->pdev->subsystem_vendor == 0x1028) &&
	    (rdev->pdev->subsystem_device == 0x01fc))
		return true;

	 
	if ((rdev->pdev->device == 0x791f) &&
	    (rdev->pdev->subsystem_vendor == 0x1028) &&
	    (rdev->pdev->subsystem_device == 0x01fd))
		return true;

	 
	if ((rdev->pdev->device == 0x791f) &&
	    (rdev->pdev->subsystem_vendor == 0x107b) &&
	    (rdev->pdev->subsystem_device == 0x0185))
		return true;

	 
	if (rdev->family == CHIP_RS690)
		return true;

	 
	if (rdev->family == CHIP_RV515)
		return false;
	if (rdev->flags & RADEON_IS_IGP) {
		 
		if (rdev->family >= CHIP_PALM)
			return true;
		 
		return false;
	}

	return true;
}

 
int radeon_irq_kms_init(struct radeon_device *rdev)
{
	int r = 0;

	spin_lock_init(&rdev->irq.lock);

	 
	rdev->ddev->vblank_disable_immediate = true;

	r = drm_vblank_init(rdev->ddev, rdev->num_crtc);
	if (r) {
		return r;
	}

	 
	rdev->msi_enabled = 0;

	if (radeon_msi_ok(rdev)) {
		int ret = pci_enable_msi(rdev->pdev);
		if (!ret) {
			rdev->msi_enabled = 1;
			dev_info(rdev->dev, "radeon: using MSI.\n");
		}
	}

	INIT_DELAYED_WORK(&rdev->hotplug_work, radeon_hotplug_work_func);
	INIT_WORK(&rdev->dp_work, radeon_dp_work_func);
	INIT_WORK(&rdev->audio_work, r600_audio_update_hdmi);

	rdev->irq.installed = true;
	r = radeon_irq_install(rdev, rdev->pdev->irq);
	if (r) {
		rdev->irq.installed = false;
		flush_delayed_work(&rdev->hotplug_work);
		return r;
	}

	DRM_INFO("radeon: irq initialized.\n");
	return 0;
}

 
void radeon_irq_kms_fini(struct radeon_device *rdev)
{
	if (rdev->irq.installed) {
		radeon_irq_uninstall(rdev);
		rdev->irq.installed = false;
		if (rdev->msi_enabled)
			pci_disable_msi(rdev->pdev);
		flush_delayed_work(&rdev->hotplug_work);
	}
}

 
void radeon_irq_kms_sw_irq_get(struct radeon_device *rdev, int ring)
{
	unsigned long irqflags;

	if (!rdev->irq.installed)
		return;

	if (atomic_inc_return(&rdev->irq.ring_int[ring]) == 1) {
		spin_lock_irqsave(&rdev->irq.lock, irqflags);
		radeon_irq_set(rdev);
		spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
	}
}

 
bool radeon_irq_kms_sw_irq_get_delayed(struct radeon_device *rdev, int ring)
{
	return atomic_inc_return(&rdev->irq.ring_int[ring]) == 1;
}

 
void radeon_irq_kms_sw_irq_put(struct radeon_device *rdev, int ring)
{
	unsigned long irqflags;

	if (!rdev->irq.installed)
		return;

	if (atomic_dec_and_test(&rdev->irq.ring_int[ring])) {
		spin_lock_irqsave(&rdev->irq.lock, irqflags);
		radeon_irq_set(rdev);
		spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
	}
}

 
void radeon_irq_kms_pflip_irq_get(struct radeon_device *rdev, int crtc)
{
	unsigned long irqflags;

	if (crtc < 0 || crtc >= rdev->num_crtc)
		return;

	if (!rdev->irq.installed)
		return;

	if (atomic_inc_return(&rdev->irq.pflip[crtc]) == 1) {
		spin_lock_irqsave(&rdev->irq.lock, irqflags);
		radeon_irq_set(rdev);
		spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
	}
}

 
void radeon_irq_kms_pflip_irq_put(struct radeon_device *rdev, int crtc)
{
	unsigned long irqflags;

	if (crtc < 0 || crtc >= rdev->num_crtc)
		return;

	if (!rdev->irq.installed)
		return;

	if (atomic_dec_and_test(&rdev->irq.pflip[crtc])) {
		spin_lock_irqsave(&rdev->irq.lock, irqflags);
		radeon_irq_set(rdev);
		spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
	}
}

 
void radeon_irq_kms_enable_afmt(struct radeon_device *rdev, int block)
{
	unsigned long irqflags;

	if (!rdev->irq.installed)
		return;

	spin_lock_irqsave(&rdev->irq.lock, irqflags);
	rdev->irq.afmt[block] = true;
	radeon_irq_set(rdev);
	spin_unlock_irqrestore(&rdev->irq.lock, irqflags);

}

 
void radeon_irq_kms_disable_afmt(struct radeon_device *rdev, int block)
{
	unsigned long irqflags;

	if (!rdev->irq.installed)
		return;

	spin_lock_irqsave(&rdev->irq.lock, irqflags);
	rdev->irq.afmt[block] = false;
	radeon_irq_set(rdev);
	spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
}

 
void radeon_irq_kms_enable_hpd(struct radeon_device *rdev, unsigned hpd_mask)
{
	unsigned long irqflags;
	int i;

	if (!rdev->irq.installed)
		return;

	spin_lock_irqsave(&rdev->irq.lock, irqflags);
	for (i = 0; i < RADEON_MAX_HPD_PINS; ++i)
		rdev->irq.hpd[i] |= !!(hpd_mask & (1 << i));
	radeon_irq_set(rdev);
	spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
}

 
void radeon_irq_kms_disable_hpd(struct radeon_device *rdev, unsigned hpd_mask)
{
	unsigned long irqflags;
	int i;

	if (!rdev->irq.installed)
		return;

	spin_lock_irqsave(&rdev->irq.lock, irqflags);
	for (i = 0; i < RADEON_MAX_HPD_PINS; ++i)
		rdev->irq.hpd[i] &= !(hpd_mask & (1 << i));
	radeon_irq_set(rdev);
	spin_unlock_irqrestore(&rdev->irq.lock, irqflags);
}

 
void radeon_irq_kms_set_irq_n_enabled(struct radeon_device *rdev,
				      u32 reg, u32 mask,
				      bool enable, const char *name, unsigned n)
{
	u32 tmp = RREG32(reg);

	 
	if (!!(tmp & mask) == enable)
		return;

	if (enable) {
		DRM_DEBUG("%s%d interrupts enabled\n", name, n);
		WREG32(reg, tmp |= mask);
	} else {
		DRM_DEBUG("%s%d interrupts disabled\n", name, n);
		WREG32(reg, tmp & ~mask);
	}
}
