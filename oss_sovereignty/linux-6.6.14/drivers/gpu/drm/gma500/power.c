 

#include "gem.h"
#include "power.h"
#include "psb_drv.h"
#include "psb_reg.h"
#include "psb_intel_reg.h"
#include "psb_irq.h"
#include <linux/mutex.h>
#include <linux/pm_runtime.h>

 
void gma_power_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = to_drm_psb_private(dev);

	 
	dev_priv->apm_base = dev_priv->apm_reg & 0xffff;
	dev_priv->ospm_base &= 0xffff;

	if (dev_priv->ops->init_pm)
		dev_priv->ops->init_pm(dev);

	 
	pm_runtime_get(dev->dev);

	dev_priv->pm_initialized = true;
}

 
void gma_power_uninit(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = to_drm_psb_private(dev);

	if (!dev_priv->pm_initialized)
		return;

	pm_runtime_put_noidle(dev->dev);
}

 
static void gma_suspend_display(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = to_drm_psb_private(dev);

	dev_priv->ops->save_regs(dev);
	dev_priv->ops->power_down(dev);
}

 
static void gma_resume_display(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = to_drm_psb_private(dev);

	 
	dev_priv->ops->power_up(dev);

	PSB_WVDC32(dev_priv->pge_ctl | _PSB_PGETBL_ENABLED, PSB_PGETBL_CTL);
	pci_write_config_word(pdev, PSB_GMCH_CTRL,
			dev_priv->gmch_ctrl | _PSB_GMCH_ENABLED);

	 
	psb_gtt_resume(dev);
	psb_gem_mm_resume(dev);
	dev_priv->ops->restore_regs(dev);
}

 
static void gma_suspend_pci(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = to_drm_psb_private(dev);
	int bsm, vbt;

	pci_save_state(pdev);
	pci_read_config_dword(pdev, 0x5C, &bsm);
	dev_priv->regs.saveBSM = bsm;
	pci_read_config_dword(pdev, 0xFC, &vbt);
	dev_priv->regs.saveVBT = vbt;

	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);
}

 
static int gma_resume_pci(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = to_drm_psb_private(dev);

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	pci_write_config_dword(pdev, 0x5c, dev_priv->regs.saveBSM);
	pci_write_config_dword(pdev, 0xFC, dev_priv->regs.saveVBT);

	return pci_enable_device(pdev);
}

 
int gma_power_suspend(struct device *_dev)
{
	struct pci_dev *pdev = to_pci_dev(_dev);
	struct drm_device *dev = pci_get_drvdata(pdev);

	gma_irq_uninstall(dev);
	gma_suspend_display(dev);
	gma_suspend_pci(pdev);
	return 0;
}

 
int gma_power_resume(struct device *_dev)
{
	struct pci_dev *pdev = to_pci_dev(_dev);
	struct drm_device *dev = pci_get_drvdata(pdev);

	gma_resume_pci(pdev);
	gma_resume_display(pdev);
	gma_irq_install(dev);
	return 0;
}

 
bool gma_power_begin(struct drm_device *dev, bool force_on)
{
	if (force_on)
		return pm_runtime_resume_and_get(dev->dev) == 0;
	else
		return pm_runtime_get_if_in_use(dev->dev) == 1;
}

 
void gma_power_end(struct drm_device *dev)
{
	pm_runtime_put(dev->dev);
}
