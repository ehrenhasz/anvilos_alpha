 

#include "display/intel_de.h"
#include "display/intel_gmbus.h"
#include "display/intel_vga.h"

#include "i915_drv.h"
#include "i915_reg.h"
#include "i915_suspend.h"
#include "intel_pci_config.h"

static void intel_save_swf(struct drm_i915_private *dev_priv)
{
	int i;

	 
	if (GRAPHICS_VER(dev_priv) == 2 && IS_MOBILE(dev_priv)) {
		for (i = 0; i < 7; i++) {
			dev_priv->regfile.saveSWF0[i] = intel_de_read(dev_priv, SWF0(i));
			dev_priv->regfile.saveSWF1[i] = intel_de_read(dev_priv, SWF1(i));
		}
		for (i = 0; i < 3; i++)
			dev_priv->regfile.saveSWF3[i] = intel_de_read(dev_priv, SWF3(i));
	} else if (GRAPHICS_VER(dev_priv) == 2) {
		for (i = 0; i < 7; i++)
			dev_priv->regfile.saveSWF1[i] = intel_de_read(dev_priv, SWF1(i));
	} else if (HAS_GMCH(dev_priv)) {
		for (i = 0; i < 16; i++) {
			dev_priv->regfile.saveSWF0[i] = intel_de_read(dev_priv, SWF0(i));
			dev_priv->regfile.saveSWF1[i] = intel_de_read(dev_priv, SWF1(i));
		}
		for (i = 0; i < 3; i++)
			dev_priv->regfile.saveSWF3[i] = intel_de_read(dev_priv, SWF3(i));
	}
}

static void intel_restore_swf(struct drm_i915_private *dev_priv)
{
	int i;

	 
	if (GRAPHICS_VER(dev_priv) == 2 && IS_MOBILE(dev_priv)) {
		for (i = 0; i < 7; i++) {
			intel_de_write(dev_priv, SWF0(i), dev_priv->regfile.saveSWF0[i]);
			intel_de_write(dev_priv, SWF1(i), dev_priv->regfile.saveSWF1[i]);
		}
		for (i = 0; i < 3; i++)
			intel_de_write(dev_priv, SWF3(i), dev_priv->regfile.saveSWF3[i]);
	} else if (GRAPHICS_VER(dev_priv) == 2) {
		for (i = 0; i < 7; i++)
			intel_de_write(dev_priv, SWF1(i), dev_priv->regfile.saveSWF1[i]);
	} else if (HAS_GMCH(dev_priv)) {
		for (i = 0; i < 16; i++) {
			intel_de_write(dev_priv, SWF0(i), dev_priv->regfile.saveSWF0[i]);
			intel_de_write(dev_priv, SWF1(i), dev_priv->regfile.saveSWF1[i]);
		}
		for (i = 0; i < 3; i++)
			intel_de_write(dev_priv, SWF3(i), dev_priv->regfile.saveSWF3[i]);
	}
}

void i915_save_display(struct drm_i915_private *dev_priv)
{
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);

	if (!HAS_DISPLAY(dev_priv))
		return;

	 
	if (GRAPHICS_VER(dev_priv) <= 4)
		dev_priv->regfile.saveDSPARB = intel_de_read(dev_priv, DSPARB);

	if (GRAPHICS_VER(dev_priv) == 4)
		pci_read_config_word(pdev, GCDGMBUS,
				     &dev_priv->regfile.saveGCDGMBUS);

	intel_save_swf(dev_priv);
}

void i915_restore_display(struct drm_i915_private *dev_priv)
{
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);

	if (!HAS_DISPLAY(dev_priv))
		return;

	intel_restore_swf(dev_priv);

	if (GRAPHICS_VER(dev_priv) == 4)
		pci_write_config_word(pdev, GCDGMBUS,
				      dev_priv->regfile.saveGCDGMBUS);

	 
	if (GRAPHICS_VER(dev_priv) <= 4)
		intel_de_write(dev_priv, DSPARB, dev_priv->regfile.saveDSPARB);

	intel_vga_redisable(dev_priv);

	intel_gmbus_reset(dev_priv);
}
