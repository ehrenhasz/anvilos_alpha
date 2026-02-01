 
#ifndef _PSB_POWERMGMT_H_
#define _PSB_POWERMGMT_H_

#include <linux/pci.h>

struct device;
struct drm_device;

void gma_power_init(struct drm_device *dev);
void gma_power_uninit(struct drm_device *dev);

 
int gma_power_suspend(struct device *dev);
int gma_power_resume(struct device *dev);

 
bool gma_power_begin(struct drm_device *dev, bool force);
void gma_power_end(struct drm_device *dev);

#endif  
