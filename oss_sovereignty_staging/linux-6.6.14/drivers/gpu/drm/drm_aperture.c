

#include <linux/aperture.h>
#include <linux/platform_device.h>

#include <drm/drm_aperture.h>
#include <drm/drm_drv.h>
#include <drm/drm_print.h>

 

 
int devm_aperture_acquire_from_firmware(struct drm_device *dev, resource_size_t base,
					resource_size_t size)
{
	struct platform_device *pdev;

	if (drm_WARN_ON(dev, !dev_is_platform(dev->dev)))
		return -EINVAL;

	pdev = to_platform_device(dev->dev);

	return devm_aperture_acquire_for_platform_device(pdev, base, size);
}
EXPORT_SYMBOL(devm_aperture_acquire_from_firmware);

 
int drm_aperture_remove_conflicting_framebuffers(resource_size_t base, resource_size_t size,
						 const struct drm_driver *req_driver)
{
	return aperture_remove_conflicting_devices(base, size, req_driver->name);
}
EXPORT_SYMBOL(drm_aperture_remove_conflicting_framebuffers);

 
int drm_aperture_remove_conflicting_pci_framebuffers(struct pci_dev *pdev,
						     const struct drm_driver *req_driver)
{
	return aperture_remove_conflicting_pci_devices(pdev, req_driver->name);
}
EXPORT_SYMBOL(drm_aperture_remove_conflicting_pci_framebuffers);
