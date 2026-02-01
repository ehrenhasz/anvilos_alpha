 

 

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include <drm/intel_lpe_audio.h>

#include "i915_drv.h"
#include "i915_irq.h"
#include "i915_reg.h"
#include "intel_de.h"
#include "intel_lpe_audio.h"
#include "intel_pci_config.h"

#define HAS_LPE_AUDIO(dev_priv) ((dev_priv)->display.audio.lpe.platdev != NULL)

static struct platform_device *
lpe_audio_platdev_create(struct drm_i915_private *dev_priv)
{
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);
	struct platform_device_info pinfo = {};
	struct resource *rsc;
	struct platform_device *platdev;
	struct intel_hdmi_lpe_audio_pdata *pdata;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	rsc = kcalloc(2, sizeof(*rsc), GFP_KERNEL);
	if (!rsc) {
		kfree(pdata);
		return ERR_PTR(-ENOMEM);
	}

	rsc[0].start    = rsc[0].end = dev_priv->display.audio.lpe.irq;
	rsc[0].flags    = IORESOURCE_IRQ;
	rsc[0].name     = "hdmi-lpe-audio-irq";

	rsc[1].start    = pci_resource_start(pdev, GEN4_GTTMMADR_BAR) +
		I915_HDMI_LPE_AUDIO_BASE;
	rsc[1].end      = pci_resource_start(pdev, GEN4_GTTMMADR_BAR) +
		I915_HDMI_LPE_AUDIO_BASE + I915_HDMI_LPE_AUDIO_SIZE - 1;
	rsc[1].flags    = IORESOURCE_MEM;
	rsc[1].name     = "hdmi-lpe-audio-mmio";

	pinfo.parent = dev_priv->drm.dev;
	pinfo.name = "hdmi-lpe-audio";
	pinfo.id = -1;
	pinfo.res = rsc;
	pinfo.num_res = 2;
	pinfo.data = pdata;
	pinfo.size_data = sizeof(*pdata);
	pinfo.dma_mask = DMA_BIT_MASK(32);

	pdata->num_pipes = INTEL_NUM_PIPES(dev_priv);
	pdata->num_ports = IS_CHERRYVIEW(dev_priv) ? 3 : 2;  
	pdata->port[0].pipe = -1;
	pdata->port[1].pipe = -1;
	pdata->port[2].pipe = -1;
	spin_lock_init(&pdata->lpe_audio_slock);

	platdev = platform_device_register_full(&pinfo);
	kfree(rsc);
	kfree(pdata);

	if (IS_ERR(platdev)) {
		drm_err(&dev_priv->drm,
			"Failed to allocate LPE audio platform device\n");
		return platdev;
	}

	pm_runtime_no_callbacks(&platdev->dev);

	return platdev;
}

static void lpe_audio_platdev_destroy(struct drm_i915_private *dev_priv)
{
	 

	platform_device_unregister(dev_priv->display.audio.lpe.platdev);
}

static void lpe_audio_irq_unmask(struct irq_data *d)
{
}

static void lpe_audio_irq_mask(struct irq_data *d)
{
}

static struct irq_chip lpe_audio_irqchip = {
	.name = "hdmi_lpe_audio_irqchip",
	.irq_mask = lpe_audio_irq_mask,
	.irq_unmask = lpe_audio_irq_unmask,
};

static int lpe_audio_irq_init(struct drm_i915_private *dev_priv)
{
	int irq = dev_priv->display.audio.lpe.irq;

	drm_WARN_ON(&dev_priv->drm, !intel_irqs_enabled(dev_priv));
	irq_set_chip_and_handler_name(irq,
				&lpe_audio_irqchip,
				handle_simple_irq,
				"hdmi_lpe_audio_irq_handler");

	return irq_set_chip_data(irq, dev_priv);
}

static bool lpe_audio_detect(struct drm_i915_private *dev_priv)
{
	int lpe_present = false;

	if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) {
		static const struct pci_device_id atom_hdaudio_ids[] = {
			 
			{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x0f04)},
			 
			{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2284)},
			{}
		};

		if (!pci_dev_present(atom_hdaudio_ids)) {
			drm_info(&dev_priv->drm,
				 "HDaudio controller not detected, using LPE audio instead\n");
			lpe_present = true;
		}
	}
	return lpe_present;
}

static int lpe_audio_setup(struct drm_i915_private *dev_priv)
{
	int ret;

	dev_priv->display.audio.lpe.irq = irq_alloc_desc(0);
	if (dev_priv->display.audio.lpe.irq < 0) {
		drm_err(&dev_priv->drm, "Failed to allocate IRQ desc: %d\n",
			dev_priv->display.audio.lpe.irq);
		ret = dev_priv->display.audio.lpe.irq;
		goto err;
	}

	drm_dbg(&dev_priv->drm, "irq = %d\n", dev_priv->display.audio.lpe.irq);

	ret = lpe_audio_irq_init(dev_priv);

	if (ret) {
		drm_err(&dev_priv->drm,
			"Failed to initialize irqchip for lpe audio: %d\n",
			ret);
		goto err_free_irq;
	}

	dev_priv->display.audio.lpe.platdev = lpe_audio_platdev_create(dev_priv);

	if (IS_ERR(dev_priv->display.audio.lpe.platdev)) {
		ret = PTR_ERR(dev_priv->display.audio.lpe.platdev);
		drm_err(&dev_priv->drm,
			"Failed to create lpe audio platform device: %d\n",
			ret);
		goto err_free_irq;
	}

	 
	intel_de_write(dev_priv, VLV_AUD_CHICKEN_BIT_REG,
		       VLV_CHICKEN_BIT_DBG_ENABLE);

	return 0;
err_free_irq:
	irq_free_desc(dev_priv->display.audio.lpe.irq);
err:
	dev_priv->display.audio.lpe.irq = -1;
	dev_priv->display.audio.lpe.platdev = NULL;
	return ret;
}

 
void intel_lpe_audio_irq_handler(struct drm_i915_private *dev_priv)
{
	int ret;

	if (!HAS_LPE_AUDIO(dev_priv))
		return;

	ret = generic_handle_irq(dev_priv->display.audio.lpe.irq);
	if (ret)
		drm_err_ratelimited(&dev_priv->drm,
				    "error handling LPE audio irq: %d\n", ret);
}

 
int intel_lpe_audio_init(struct drm_i915_private *dev_priv)
{
	int ret = -ENODEV;

	if (lpe_audio_detect(dev_priv)) {
		ret = lpe_audio_setup(dev_priv);
		if (ret < 0)
			drm_err(&dev_priv->drm,
				"failed to setup LPE Audio bridge\n");
	}
	return ret;
}

 
void intel_lpe_audio_teardown(struct drm_i915_private *dev_priv)
{
	if (!HAS_LPE_AUDIO(dev_priv))
		return;

	lpe_audio_platdev_destroy(dev_priv);

	irq_free_desc(dev_priv->display.audio.lpe.irq);

	dev_priv->display.audio.lpe.irq = -1;
	dev_priv->display.audio.lpe.platdev = NULL;
}

 
void intel_lpe_audio_notify(struct drm_i915_private *dev_priv,
			    enum transcoder cpu_transcoder, enum port port,
			    const void *eld, int ls_clock, bool dp_output)
{
	unsigned long irqflags;
	struct intel_hdmi_lpe_audio_pdata *pdata;
	struct intel_hdmi_lpe_audio_port_pdata *ppdata;
	u32 audio_enable;

	if (!HAS_LPE_AUDIO(dev_priv))
		return;

	pdata = dev_get_platdata(&dev_priv->display.audio.lpe.platdev->dev);
	ppdata = &pdata->port[port - PORT_B];

	spin_lock_irqsave(&pdata->lpe_audio_slock, irqflags);

	audio_enable = intel_de_read(dev_priv, VLV_AUD_PORT_EN_DBG(port));

	if (eld != NULL) {
		memcpy(ppdata->eld, eld, HDMI_MAX_ELD_BYTES);
		ppdata->pipe = cpu_transcoder;
		ppdata->ls_clock = ls_clock;
		ppdata->dp_output = dp_output;

		 
		intel_de_write(dev_priv, VLV_AUD_PORT_EN_DBG(port),
			       audio_enable & ~VLV_AMP_MUTE);
	} else {
		memset(ppdata->eld, 0, HDMI_MAX_ELD_BYTES);
		ppdata->pipe = -1;
		ppdata->ls_clock = 0;
		ppdata->dp_output = false;

		 
		intel_de_write(dev_priv, VLV_AUD_PORT_EN_DBG(port),
			       audio_enable | VLV_AMP_MUTE);
	}

	if (pdata->notify_audio_lpe)
		pdata->notify_audio_lpe(dev_priv->display.audio.lpe.platdev, port - PORT_B);

	spin_unlock_irqrestore(&pdata->lpe_audio_slock, irqflags);
}
