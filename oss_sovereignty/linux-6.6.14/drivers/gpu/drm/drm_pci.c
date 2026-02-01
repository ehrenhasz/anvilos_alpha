 

#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include <drm/drm.h>
#include <drm/drm_drv.h>
#include <drm/drm_print.h>

#include "drm_internal.h"
#include "drm_legacy.h"

#ifdef CONFIG_DRM_LEGACY
 
static LIST_HEAD(legacy_dev_list);
static DEFINE_MUTEX(legacy_dev_list_lock);
#endif

static int drm_get_pci_domain(struct drm_device *dev)
{
#ifndef __alpha__
	 
	if (dev->if_version < 0x10004)
		return 0;
#endif  

	return pci_domain_nr(to_pci_dev(dev->dev)->bus);
}

int drm_pci_set_busid(struct drm_device *dev, struct drm_master *master)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);

	master->unique = kasprintf(GFP_KERNEL, "pci:%04x:%02x:%02x.%d",
					drm_get_pci_domain(dev),
					pdev->bus->number,
					PCI_SLOT(pdev->devfn),
					PCI_FUNC(pdev->devfn));
	if (!master->unique)
		return -ENOMEM;

	master->unique_len = strlen(master->unique);
	return 0;
}

#ifdef CONFIG_DRM_LEGACY

static int drm_legacy_pci_irq_by_busid(struct drm_device *dev, struct drm_irq_busid *p)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);

	if ((p->busnum >> 8) != drm_get_pci_domain(dev) ||
	    (p->busnum & 0xff) != pdev->bus->number ||
	    p->devnum != PCI_SLOT(pdev->devfn) || p->funcnum != PCI_FUNC(pdev->devfn))
		return -EINVAL;

	p->irq = pdev->irq;

	DRM_DEBUG("%d:%d:%d => IRQ %d\n", p->busnum, p->devnum, p->funcnum,
		  p->irq);
	return 0;
}

 
int drm_legacy_irq_by_busid(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct drm_irq_busid *p = data;

	if (!drm_core_check_feature(dev, DRIVER_LEGACY))
		return -EOPNOTSUPP;

	 
	if (WARN_ON(!dev_is_pci(dev->dev)))
		return -EINVAL;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
		return -EOPNOTSUPP;

	return drm_legacy_pci_irq_by_busid(dev, p);
}

void drm_legacy_pci_agp_destroy(struct drm_device *dev)
{
	if (dev->agp) {
		arch_phys_wc_del(dev->agp->agp_mtrr);
		drm_legacy_agp_clear(dev);
		kfree(dev->agp);
		dev->agp = NULL;
	}
}

static void drm_legacy_pci_agp_init(struct drm_device *dev)
{
	if (drm_core_check_feature(dev, DRIVER_USE_AGP)) {
		if (pci_find_capability(to_pci_dev(dev->dev), PCI_CAP_ID_AGP))
			dev->agp = drm_legacy_agp_init(dev);
		if (dev->agp) {
			dev->agp->agp_mtrr = arch_phys_wc_add(
				dev->agp->agp_info.aper_base,
				dev->agp->agp_info.aper_size *
				1024 * 1024);
		}
	}
}

static int drm_legacy_get_pci_dev(struct pci_dev *pdev,
				  const struct pci_device_id *ent,
				  const struct drm_driver *driver)
{
	struct drm_device *dev;
	int ret;

	DRM_DEBUG("\n");

	dev = drm_dev_alloc(driver, &pdev->dev);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	ret = pci_enable_device(pdev);
	if (ret)
		goto err_free;

#ifdef __alpha__
	dev->hose = pdev->sysdata;
#endif

	drm_legacy_pci_agp_init(dev);

	ret = drm_dev_register(dev, ent->driver_data);
	if (ret)
		goto err_agp;

	if (drm_core_check_feature(dev, DRIVER_LEGACY)) {
		mutex_lock(&legacy_dev_list_lock);
		list_add_tail(&dev->legacy_dev_list, &legacy_dev_list);
		mutex_unlock(&legacy_dev_list_lock);
	}

	return 0;

err_agp:
	drm_legacy_pci_agp_destroy(dev);
	pci_disable_device(pdev);
err_free:
	drm_dev_put(dev);
	return ret;
}

 
int drm_legacy_pci_init(const struct drm_driver *driver,
			struct pci_driver *pdriver)
{
	struct pci_dev *pdev = NULL;
	const struct pci_device_id *pid;
	int i;

	DRM_DEBUG("\n");

	if (WARN_ON(!(driver->driver_features & DRIVER_LEGACY)))
		return -EINVAL;

	 
	for (i = 0; pdriver->id_table[i].vendor != 0; i++) {
		pid = &pdriver->id_table[i];

		 
		pdev = NULL;
		while ((pdev =
			pci_get_subsys(pid->vendor, pid->device, pid->subvendor,
				       pid->subdevice, pdev)) != NULL) {
			if ((pdev->class & pid->class_mask) != pid->class)
				continue;

			 
			pci_dev_get(pdev);
			drm_legacy_get_pci_dev(pdev, pid, driver);
		}
	}
	return 0;
}
EXPORT_SYMBOL(drm_legacy_pci_init);

 
void drm_legacy_pci_exit(const struct drm_driver *driver,
			 struct pci_driver *pdriver)
{
	struct drm_device *dev, *tmp;

	DRM_DEBUG("\n");

	if (!(driver->driver_features & DRIVER_LEGACY)) {
		WARN_ON(1);
	} else {
		mutex_lock(&legacy_dev_list_lock);
		list_for_each_entry_safe(dev, tmp, &legacy_dev_list,
					 legacy_dev_list) {
			if (dev->driver == driver) {
				list_del(&dev->legacy_dev_list);
				drm_put_dev(dev);
			}
		}
		mutex_unlock(&legacy_dev_list_lock);
	}
	DRM_INFO("Module unloaded\n");
}
EXPORT_SYMBOL(drm_legacy_pci_exit);

#endif
