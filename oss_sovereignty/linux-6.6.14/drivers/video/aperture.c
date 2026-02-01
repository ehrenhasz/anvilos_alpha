

#include <linux/aperture.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfb.h>
#include <linux/types.h>
#include <linux/vgaarb.h>

#include <video/vga.h>

 

struct aperture_range {
	struct device *dev;
	resource_size_t base;
	resource_size_t size;
	struct list_head lh;
	void (*detach)(struct device *dev);
};

static LIST_HEAD(apertures);
static DEFINE_MUTEX(apertures_lock);

static bool overlap(resource_size_t base1, resource_size_t end1,
		    resource_size_t base2, resource_size_t end2)
{
	return (base1 < end2) && (end1 > base2);
}

static void devm_aperture_acquire_release(void *data)
{
	struct aperture_range *ap = data;
	bool detached = !ap->dev;

	if (detached)
		return;

	mutex_lock(&apertures_lock);
	list_del(&ap->lh);
	mutex_unlock(&apertures_lock);
}

static int devm_aperture_acquire(struct device *dev,
				 resource_size_t base, resource_size_t size,
				 void (*detach)(struct device *))
{
	size_t end = base + size;
	struct list_head *pos;
	struct aperture_range *ap;

	mutex_lock(&apertures_lock);

	list_for_each(pos, &apertures) {
		ap = container_of(pos, struct aperture_range, lh);
		if (overlap(base, end, ap->base, ap->base + ap->size)) {
			mutex_unlock(&apertures_lock);
			return -EBUSY;
		}
	}

	ap = devm_kzalloc(dev, sizeof(*ap), GFP_KERNEL);
	if (!ap) {
		mutex_unlock(&apertures_lock);
		return -ENOMEM;
	}

	ap->dev = dev;
	ap->base = base;
	ap->size = size;
	ap->detach = detach;
	INIT_LIST_HEAD(&ap->lh);

	list_add(&ap->lh, &apertures);

	mutex_unlock(&apertures_lock);

	return devm_add_action_or_reset(dev, devm_aperture_acquire_release, ap);
}

static void aperture_detach_platform_device(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	 
	platform_device_unregister(pdev);
}

 
int devm_aperture_acquire_for_platform_device(struct platform_device *pdev,
					      resource_size_t base,
					      resource_size_t size)
{
	return devm_aperture_acquire(&pdev->dev, base, size, aperture_detach_platform_device);
}
EXPORT_SYMBOL(devm_aperture_acquire_for_platform_device);

static void aperture_detach_devices(resource_size_t base, resource_size_t size)
{
	resource_size_t end = base + size;
	struct list_head *pos, *n;

	mutex_lock(&apertures_lock);

	list_for_each_safe(pos, n, &apertures) {
		struct aperture_range *ap = container_of(pos, struct aperture_range, lh);
		struct device *dev = ap->dev;

		if (WARN_ON_ONCE(!dev))
			continue;

		if (!overlap(base, end, ap->base, ap->base + ap->size))
			continue;

		ap->dev = NULL;  
		list_del(&ap->lh);

		ap->detach(dev);
	}

	mutex_unlock(&apertures_lock);
}

 
int aperture_remove_conflicting_devices(resource_size_t base, resource_size_t size,
					const char *name)
{
	 
	sysfb_disable();

	aperture_detach_devices(base, size);

	return 0;
}
EXPORT_SYMBOL(aperture_remove_conflicting_devices);

 
int __aperture_remove_legacy_vga_devices(struct pci_dev *pdev)
{
	 
	aperture_detach_devices(VGA_FB_PHYS_BASE, VGA_FB_PHYS_SIZE);

	 
	return vga_remove_vgacon(pdev);
}
EXPORT_SYMBOL(__aperture_remove_legacy_vga_devices);

 
int aperture_remove_conflicting_pci_devices(struct pci_dev *pdev, const char *name)
{
	bool primary = false;
	resource_size_t base, size;
	int bar, ret = 0;

	if (pdev == vga_default_device())
		primary = true;

	if (primary)
		sysfb_disable();

	for (bar = 0; bar < PCI_STD_NUM_BARS; ++bar) {
		if (!(pci_resource_flags(pdev, bar) & IORESOURCE_MEM))
			continue;

		base = pci_resource_start(pdev, bar);
		size = pci_resource_len(pdev, bar);
		aperture_detach_devices(base, size);
	}

	 
	if (primary)
		ret = __aperture_remove_legacy_vga_devices(pdev);

	return ret;

}
EXPORT_SYMBOL(aperture_remove_conflicting_pci_devices);
