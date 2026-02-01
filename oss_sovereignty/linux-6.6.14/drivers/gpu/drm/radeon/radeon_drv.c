 

 


#include <linux/compat.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/vga_switcheroo.h>
#include <linux/mmu_notifier.h>
#include <linux/pci.h>

#include <drm/drm_aperture.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_pciids.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <drm/radeon_drm.h>

#include "radeon_drv.h"
#include "radeon.h"
#include "radeon_kms.h"
#include "radeon_ttm.h"
#include "radeon_device.h"
#include "radeon_prime.h"

 
#define KMS_DRIVER_MAJOR	2
#define KMS_DRIVER_MINOR	50
#define KMS_DRIVER_PATCHLEVEL	0

int radeon_no_wb;
int radeon_modeset = -1;
int radeon_dynclks = -1;
int radeon_r4xx_atom;
int radeon_agpmode = -1;
int radeon_vram_limit;
int radeon_gart_size = -1;  
int radeon_benchmarking;
int radeon_testing;
int radeon_connector_table;
int radeon_tv = 1;
int radeon_audio = -1;
int radeon_disp_priority;
int radeon_hw_i2c;
int radeon_pcie_gen2 = -1;
int radeon_msi = -1;
int radeon_lockup_timeout = 10000;
int radeon_fastfb;
int radeon_dpm = -1;
int radeon_aspm = -1;
int radeon_runtime_pm = -1;
int radeon_hard_reset;
int radeon_vm_size = 8;
int radeon_vm_block_size = -1;
int radeon_deep_color;
int radeon_use_pflipirq = 2;
int radeon_bapm = -1;
int radeon_backlight = -1;
int radeon_auxch = -1;
int radeon_uvd = 1;
int radeon_vce = 1;

MODULE_PARM_DESC(no_wb, "Disable AGP writeback for scratch registers");
module_param_named(no_wb, radeon_no_wb, int, 0444);

MODULE_PARM_DESC(modeset, "Disable/Enable modesetting");
module_param_named(modeset, radeon_modeset, int, 0400);

MODULE_PARM_DESC(dynclks, "Disable/Enable dynamic clocks");
module_param_named(dynclks, radeon_dynclks, int, 0444);

MODULE_PARM_DESC(r4xx_atom, "Enable ATOMBIOS modesetting for R4xx");
module_param_named(r4xx_atom, radeon_r4xx_atom, int, 0444);

MODULE_PARM_DESC(vramlimit, "Restrict VRAM for testing, in megabytes");
module_param_named(vramlimit, radeon_vram_limit, int, 0600);

MODULE_PARM_DESC(agpmode, "AGP Mode (-1 == PCI)");
module_param_named(agpmode, radeon_agpmode, int, 0444);

MODULE_PARM_DESC(gartsize, "Size of PCIE/IGP gart to setup in megabytes (32, 64, etc., -1 = auto)");
module_param_named(gartsize, radeon_gart_size, int, 0600);

MODULE_PARM_DESC(benchmark, "Run benchmark");
module_param_named(benchmark, radeon_benchmarking, int, 0444);

MODULE_PARM_DESC(test, "Run tests");
module_param_named(test, radeon_testing, int, 0444);

MODULE_PARM_DESC(connector_table, "Force connector table");
module_param_named(connector_table, radeon_connector_table, int, 0444);

MODULE_PARM_DESC(tv, "TV enable (0 = disable)");
module_param_named(tv, radeon_tv, int, 0444);

MODULE_PARM_DESC(audio, "Audio enable (-1 = auto, 0 = disable, 1 = enable)");
module_param_named(audio, radeon_audio, int, 0444);

MODULE_PARM_DESC(disp_priority, "Display Priority (0 = auto, 1 = normal, 2 = high)");
module_param_named(disp_priority, radeon_disp_priority, int, 0444);

MODULE_PARM_DESC(hw_i2c, "hw i2c engine enable (0 = disable)");
module_param_named(hw_i2c, radeon_hw_i2c, int, 0444);

MODULE_PARM_DESC(pcie_gen2, "PCIE Gen2 mode (-1 = auto, 0 = disable, 1 = enable)");
module_param_named(pcie_gen2, radeon_pcie_gen2, int, 0444);

MODULE_PARM_DESC(msi, "MSI support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(msi, radeon_msi, int, 0444);

MODULE_PARM_DESC(lockup_timeout, "GPU lockup timeout in ms (default 10000 = 10 seconds, 0 = disable)");
module_param_named(lockup_timeout, radeon_lockup_timeout, int, 0444);

MODULE_PARM_DESC(fastfb, "Direct FB access for IGP chips (0 = disable, 1 = enable)");
module_param_named(fastfb, radeon_fastfb, int, 0444);

MODULE_PARM_DESC(dpm, "DPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(dpm, radeon_dpm, int, 0444);

MODULE_PARM_DESC(aspm, "ASPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(aspm, radeon_aspm, int, 0444);

MODULE_PARM_DESC(runpm, "PX runtime pm (1 = force enable, 0 = disable, -1 = PX only default)");
module_param_named(runpm, radeon_runtime_pm, int, 0444);

MODULE_PARM_DESC(hard_reset, "PCI config reset (1 = force enable, 0 = disable (default))");
module_param_named(hard_reset, radeon_hard_reset, int, 0444);

MODULE_PARM_DESC(vm_size, "VM address space size in gigabytes (default 4GB)");
module_param_named(vm_size, radeon_vm_size, int, 0444);

MODULE_PARM_DESC(vm_block_size, "VM page table size in bits (default depending on vm_size)");
module_param_named(vm_block_size, radeon_vm_block_size, int, 0444);

MODULE_PARM_DESC(deep_color, "Deep Color support (1 = enable, 0 = disable (default))");
module_param_named(deep_color, radeon_deep_color, int, 0444);

MODULE_PARM_DESC(use_pflipirq, "Pflip irqs for pageflip completion (0 = disable, 1 = as fallback, 2 = exclusive (default))");
module_param_named(use_pflipirq, radeon_use_pflipirq, int, 0444);

MODULE_PARM_DESC(bapm, "BAPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(bapm, radeon_bapm, int, 0444);

MODULE_PARM_DESC(backlight, "backlight support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(backlight, radeon_backlight, int, 0444);

MODULE_PARM_DESC(auxch, "Use native auxch experimental support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(auxch, radeon_auxch, int, 0444);

MODULE_PARM_DESC(uvd, "uvd enable/disable uvd support (1 = enable, 0 = disable)");
module_param_named(uvd, radeon_uvd, int, 0444);

MODULE_PARM_DESC(vce, "vce enable/disable vce support (1 = enable, 0 = disable)");
module_param_named(vce, radeon_vce, int, 0444);

int radeon_si_support = 1;
MODULE_PARM_DESC(si_support, "SI support (1 = enabled (default), 0 = disabled)");
module_param_named(si_support, radeon_si_support, int, 0444);

int radeon_cik_support = 1;
MODULE_PARM_DESC(cik_support, "CIK support (1 = enabled (default), 0 = disabled)");
module_param_named(cik_support, radeon_cik_support, int, 0444);

static struct pci_device_id pciidlist[] = {
	radeon_PCI_IDS
};

MODULE_DEVICE_TABLE(pci, pciidlist);

static const struct drm_driver kms_driver;

static int radeon_pci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *ent)
{
	unsigned long flags = 0;
	struct drm_device *dev;
	int ret;

	if (!ent)
		return -ENODEV;  

	flags = ent->driver_data;

	if (!radeon_si_support) {
		switch (flags & RADEON_FAMILY_MASK) {
		case CHIP_TAHITI:
		case CHIP_PITCAIRN:
		case CHIP_VERDE:
		case CHIP_OLAND:
		case CHIP_HAINAN:
			dev_info(&pdev->dev,
				 "SI support disabled by module param\n");
			return -ENODEV;
		}
	}
	if (!radeon_cik_support) {
		switch (flags & RADEON_FAMILY_MASK) {
		case CHIP_KAVERI:
		case CHIP_BONAIRE:
		case CHIP_HAWAII:
		case CHIP_KABINI:
		case CHIP_MULLINS:
			dev_info(&pdev->dev,
				 "CIK support disabled by module param\n");
			return -ENODEV;
		}
	}

	if (vga_switcheroo_client_probe_defer(pdev))
		return -EPROBE_DEFER;

	 
	ret = drm_aperture_remove_conflicting_pci_framebuffers(pdev, &kms_driver);
	if (ret)
		return ret;

	dev = drm_dev_alloc(&kms_driver, &pdev->dev);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	ret = pci_enable_device(pdev);
	if (ret)
		goto err_free;

	pci_set_drvdata(pdev, dev);

	ret = drm_dev_register(dev, ent->driver_data);
	if (ret)
		goto err_agp;

	radeon_fbdev_setup(dev->dev_private);

	return 0;

err_agp:
	pci_disable_device(pdev);
err_free:
	drm_dev_put(dev);
	return ret;
}

static void
radeon_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	drm_put_dev(dev);
}

static void
radeon_pci_shutdown(struct pci_dev *pdev)
{
	 
	if (radeon_device_is_virtual())
		radeon_pci_remove(pdev);

#if defined(CONFIG_PPC64) || defined(CONFIG_MACH_LOONGSON64)
	 
	radeon_suspend_kms(pci_get_drvdata(pdev), true, true, false);
#endif
}

static int radeon_pmops_suspend(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	return radeon_suspend_kms(drm_dev, true, true, false);
}

static int radeon_pmops_resume(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	 
	if (radeon_is_px(drm_dev)) {
		pm_runtime_disable(dev);
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
	}

	return radeon_resume_kms(drm_dev, true, true);
}

static int radeon_pmops_freeze(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	return radeon_suspend_kms(drm_dev, false, true, true);
}

static int radeon_pmops_thaw(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	return radeon_resume_kms(drm_dev, false, true);
}

static int radeon_pmops_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	if (!radeon_is_px(drm_dev)) {
		pm_runtime_forbid(dev);
		return -EBUSY;
	}

	drm_dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;
	drm_kms_helper_poll_disable(drm_dev);

	radeon_suspend_kms(drm_dev, false, false, false);
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_ignore_hotplug(pdev);
	if (radeon_is_atpx_hybrid())
		pci_set_power_state(pdev, PCI_D3cold);
	else if (!radeon_has_atpx_dgpu_power_cntl())
		pci_set_power_state(pdev, PCI_D3hot);
	drm_dev->switch_power_state = DRM_SWITCH_POWER_DYNAMIC_OFF;

	return 0;
}

static int radeon_pmops_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	int ret;

	if (!radeon_is_px(drm_dev))
		return -EINVAL;

	drm_dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;

	if (radeon_is_atpx_hybrid() ||
	    !radeon_has_atpx_dgpu_power_cntl())
		pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	ret = pci_enable_device(pdev);
	if (ret)
		return ret;
	pci_set_master(pdev);

	ret = radeon_resume_kms(drm_dev, false, false);
	drm_kms_helper_poll_enable(drm_dev);
	drm_dev->switch_power_state = DRM_SWITCH_POWER_ON;
	return 0;
}

static int radeon_pmops_runtime_idle(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct drm_crtc *crtc;

	if (!radeon_is_px(drm_dev)) {
		pm_runtime_forbid(dev);
		return -EBUSY;
	}

	list_for_each_entry(crtc, &drm_dev->mode_config.crtc_list, head) {
		if (crtc->enabled) {
			DRM_DEBUG_DRIVER("failing to power off - crtc active\n");
			return -EBUSY;
		}
	}

	pm_runtime_mark_last_busy(dev);
	pm_runtime_autosuspend(dev);
	 
	return 1;
}

long radeon_drm_ioctl(struct file *filp,
		      unsigned int cmd, unsigned long arg)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev;
	long ret;

	dev = file_priv->minor->dev;
	ret = pm_runtime_get_sync(dev->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(dev->dev);
		return ret;
	}

	ret = drm_ioctl(filp, cmd, arg);

	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);
	return ret;
}

#ifdef CONFIG_COMPAT
static long radeon_kms_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);

	if (nr < DRM_COMMAND_BASE)
		return drm_compat_ioctl(filp, cmd, arg);

	return radeon_drm_ioctl(filp, cmd, arg);
}
#endif

static const struct dev_pm_ops radeon_pm_ops = {
	.suspend = radeon_pmops_suspend,
	.resume = radeon_pmops_resume,
	.freeze = radeon_pmops_freeze,
	.thaw = radeon_pmops_thaw,
	.poweroff = radeon_pmops_freeze,
	.restore = radeon_pmops_resume,
	.runtime_suspend = radeon_pmops_runtime_suspend,
	.runtime_resume = radeon_pmops_runtime_resume,
	.runtime_idle = radeon_pmops_runtime_idle,
};

static const struct file_operations radeon_driver_kms_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = radeon_drm_ioctl,
	.mmap = drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = radeon_kms_compat_ioctl,
#endif
};

static const struct drm_ioctl_desc radeon_ioctls_kms[] = {
	DRM_IOCTL_DEF_DRV(RADEON_CP_INIT, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_START, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_STOP, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_RESET, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_IDLE, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CP_RESUME, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_RESET, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FULLSCREEN, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SWAP, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CLEAR, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_VERTEX, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INDICES, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_TEXTURE, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_STIPPLE, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INDIRECT, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_VERTEX2, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CMDBUF, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_GETPARAM, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FLIP, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_ALLOC, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FREE, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INIT_HEAP, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_IRQ_EMIT, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_IRQ_WAIT, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SETPARAM, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SURF_ALLOC, drm_invalid_op, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SURF_FREE, drm_invalid_op, DRM_AUTH),
	 
	DRM_IOCTL_DEF_DRV(RADEON_GEM_INFO, radeon_gem_info_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_CREATE, radeon_gem_create_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_MMAP, radeon_gem_mmap_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_SET_DOMAIN, radeon_gem_set_domain_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_WAIT_IDLE, radeon_gem_wait_idle_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_CS, radeon_cs_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_INFO, radeon_info_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_SET_TILING, radeon_gem_set_tiling_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_GET_TILING, radeon_gem_get_tiling_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_BUSY, radeon_gem_busy_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_VA, radeon_gem_va_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_OP, radeon_gem_op_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(RADEON_GEM_USERPTR, radeon_gem_userptr_ioctl, DRM_AUTH|DRM_RENDER_ALLOW),
};

static const struct drm_driver kms_driver = {
	.driver_features =
	    DRIVER_GEM | DRIVER_RENDER | DRIVER_MODESET,
	.load = radeon_driver_load_kms,
	.open = radeon_driver_open_kms,
	.postclose = radeon_driver_postclose_kms,
	.unload = radeon_driver_unload_kms,
	.ioctls = radeon_ioctls_kms,
	.num_ioctls = ARRAY_SIZE(radeon_ioctls_kms),
	.dumb_create = radeon_mode_dumb_create,
	.dumb_map_offset = radeon_mode_dumb_mmap,
	.fops = &radeon_driver_kms_fops,

	.gem_prime_import_sg_table = radeon_gem_prime_import_sg_table,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = KMS_DRIVER_MAJOR,
	.minor = KMS_DRIVER_MINOR,
	.patchlevel = KMS_DRIVER_PATCHLEVEL,
};

static struct pci_driver radeon_kms_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = radeon_pci_probe,
	.remove = radeon_pci_remove,
	.shutdown = radeon_pci_shutdown,
	.driver.pm = &radeon_pm_ops,
};

static int __init radeon_module_init(void)
{
	if (drm_firmware_drivers_only() && radeon_modeset == -1)
		radeon_modeset = 0;

	if (radeon_modeset == 0)
		return -EINVAL;

	DRM_INFO("radeon kernel modesetting enabled.\n");
	radeon_register_atpx_handler();

	return pci_register_driver(&radeon_kms_pci_driver);
}

static void __exit radeon_module_exit(void)
{
	pci_unregister_driver(&radeon_kms_pci_driver);
	radeon_unregister_atpx_handler();
	mmu_notifier_synchronize();
}

module_init(radeon_module_init);
module_exit(radeon_module_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
