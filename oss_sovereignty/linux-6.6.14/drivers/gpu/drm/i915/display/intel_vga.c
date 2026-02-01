
 

#include <linux/pci.h>
#include <linux/vgaarb.h>

#include <video/vga.h>

#include "soc/intel_gmch.h"

#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_de.h"
#include "intel_vga.h"

static i915_reg_t intel_vga_cntrl_reg(struct drm_i915_private *i915)
{
	if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915))
		return VLV_VGACNTRL;
	else if (DISPLAY_VER(i915) >= 5)
		return CPU_VGACNTRL;
	else
		return VGACNTRL;
}

 
void intel_vga_disable(struct drm_i915_private *dev_priv)
{
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);
	i915_reg_t vga_reg = intel_vga_cntrl_reg(dev_priv);
	u8 sr1;

	if (intel_de_read(dev_priv, vga_reg) & VGA_DISP_DISABLE)
		return;

	 
	vga_get_uninterruptible(pdev, VGA_RSRC_LEGACY_IO);
	outb(0x01, VGA_SEQ_I);
	sr1 = inb(VGA_SEQ_D);
	outb(sr1 | VGA_SR01_SCREEN_OFF, VGA_SEQ_D);
	vga_put(pdev, VGA_RSRC_LEGACY_IO);
	udelay(300);

	intel_de_write(dev_priv, vga_reg, VGA_DISP_DISABLE);
	intel_de_posting_read(dev_priv, vga_reg);
}

void intel_vga_redisable_power_on(struct drm_i915_private *dev_priv)
{
	i915_reg_t vga_reg = intel_vga_cntrl_reg(dev_priv);

	if (!(intel_de_read(dev_priv, vga_reg) & VGA_DISP_DISABLE)) {
		drm_dbg_kms(&dev_priv->drm,
			    "Something enabled VGA plane, disabling it\n");
		intel_vga_disable(dev_priv);
	}
}

void intel_vga_redisable(struct drm_i915_private *i915)
{
	intel_wakeref_t wakeref;

	 
	wakeref = intel_display_power_get_if_enabled(i915, POWER_DOMAIN_VGA);
	if (!wakeref)
		return;

	intel_vga_redisable_power_on(i915);

	intel_display_power_put(i915, POWER_DOMAIN_VGA, wakeref);
}

void intel_vga_reset_io_mem(struct drm_i915_private *i915)
{
	struct pci_dev *pdev = to_pci_dev(i915->drm.dev);

	 
	vga_get_uninterruptible(pdev, VGA_RSRC_LEGACY_IO);
	outb(inb(VGA_MIS_R), VGA_MIS_W);
	vga_put(pdev, VGA_RSRC_LEGACY_IO);
}

static unsigned int
intel_vga_set_decode(struct pci_dev *pdev, bool enable_decode)
{
	struct drm_i915_private *i915 = pdev_to_i915(pdev);

	intel_gmch_vga_set_state(i915, enable_decode);

	if (enable_decode)
		return VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM |
		       VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;
	else
		return VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;
}

int intel_vga_register(struct drm_i915_private *i915)
{

	struct pci_dev *pdev = to_pci_dev(i915->drm.dev);
	int ret;

	 
	ret = vga_client_register(pdev, intel_vga_set_decode);
	if (ret && ret != -ENODEV)
		return ret;

	return 0;
}

void intel_vga_unregister(struct drm_i915_private *i915)
{
	struct pci_dev *pdev = to_pci_dev(i915->drm.dev);

	vga_client_unregister(pdev);
}
