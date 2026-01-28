#ifndef __AMDGPU_DRV_H__
#define __AMDGPU_DRV_H__
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include "amd_shared.h"
#define DRIVER_AUTHOR		"AMD linux driver team"
#define DRIVER_NAME		"amdgpu"
#define DRIVER_DESC		"AMD GPU"
#define DRIVER_DATE		"20150101"
extern const struct drm_driver amdgpu_partition_driver;
long amdgpu_drm_ioctl(struct file *filp,
		      unsigned int cmd, unsigned long arg);
long amdgpu_kms_compat_ioctl(struct file *filp,
			     unsigned int cmd, unsigned long arg);
#endif
