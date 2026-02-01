 

#ifndef __NOUVEAU_PM_H__
#define __NOUVEAU_PM_H__

struct nouveau_hwmon {
	struct drm_device *dev;
	struct device *hwmon;
};

static inline struct nouveau_hwmon *
nouveau_hwmon(struct drm_device *dev)
{
	return nouveau_drm(dev)->hwmon;
}

 
int  nouveau_hwmon_init(struct drm_device *dev);
void nouveau_hwmon_fini(struct drm_device *dev);

#endif
