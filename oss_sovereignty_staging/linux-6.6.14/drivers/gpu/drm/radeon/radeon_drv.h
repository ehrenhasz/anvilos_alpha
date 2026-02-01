 

#ifndef __RADEON_DRV_H__
#define __RADEON_DRV_H__

#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <drm/drm_legacy.h>

#include "radeon_family.h"

 

#define DRIVER_AUTHOR		"Gareth Hughes, Keith Whitwell, others."

#define DRIVER_NAME		"radeon"
#define DRIVER_DESC		"ATI Radeon"
#define DRIVER_DATE		"20080528"

 
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		34
#define DRIVER_PATCHLEVEL	0

long radeon_drm_ioctl(struct file *filp,
		      unsigned int cmd, unsigned long arg);

int radeon_driver_load_kms(struct drm_device *dev, unsigned long flags);
void radeon_driver_unload_kms(struct drm_device *dev);
int radeon_driver_open_kms(struct drm_device *dev, struct drm_file *file_priv);
void radeon_driver_postclose_kms(struct drm_device *dev,
				 struct drm_file *file_priv);

 
#if defined(CONFIG_VGA_SWITCHEROO)
void radeon_register_atpx_handler(void);
void radeon_unregister_atpx_handler(void);
bool radeon_has_atpx_dgpu_power_cntl(void);
bool radeon_is_atpx_hybrid(void);
#else
static inline void radeon_register_atpx_handler(void) {}
static inline void radeon_unregister_atpx_handler(void) {}
static inline bool radeon_has_atpx_dgpu_power_cntl(void) { return false; }
static inline bool radeon_is_atpx_hybrid(void) { return false; }
#endif

#endif				 
