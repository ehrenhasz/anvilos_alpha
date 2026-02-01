 

 

#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_print.h>

#include "drm_internal.h"
#include "drm_legacy.h"

void drm_legacy_init_members(struct drm_device *dev)
{
	INIT_LIST_HEAD(&dev->ctxlist);
	INIT_LIST_HEAD(&dev->vmalist);
	INIT_LIST_HEAD(&dev->maplist);
	spin_lock_init(&dev->buf_lock);
	mutex_init(&dev->ctxlist_mutex);
}

void drm_legacy_destroy_members(struct drm_device *dev)
{
	mutex_destroy(&dev->ctxlist_mutex);
}

int drm_legacy_setup(struct drm_device * dev)
{
	int ret;

	if (dev->driver->firstopen &&
	    drm_core_check_feature(dev, DRIVER_LEGACY)) {
		ret = dev->driver->firstopen(dev);
		if (ret != 0)
			return ret;
	}

	ret = drm_legacy_dma_setup(dev);
	if (ret < 0)
		return ret;


	DRM_DEBUG("\n");
	return 0;
}

void drm_legacy_dev_reinit(struct drm_device *dev)
{
	if (dev->irq_enabled)
		drm_legacy_irq_uninstall(dev);

	mutex_lock(&dev->struct_mutex);

	drm_legacy_agp_clear(dev);

	drm_legacy_sg_cleanup(dev);
	drm_legacy_vma_flush(dev);
	drm_legacy_dma_takedown(dev);

	mutex_unlock(&dev->struct_mutex);

	dev->sigdata.lock = NULL;

	dev->context_flag = 0;
	dev->last_context = 0;
	dev->if_version = 0;

	DRM_DEBUG("lastclose completed\n");
}

void drm_master_legacy_init(struct drm_master *master)
{
	spin_lock_init(&master->lock.spinlock);
	init_waitqueue_head(&master->lock.lock_queue);
}
