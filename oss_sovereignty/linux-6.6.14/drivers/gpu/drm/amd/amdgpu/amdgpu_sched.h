 

#ifndef __AMDGPU_SCHED_H__
#define __AMDGPU_SCHED_H__

enum drm_sched_priority;

struct drm_device;
struct drm_file;

int amdgpu_to_sched_priority(int amdgpu_priority,
			     enum drm_sched_priority *prio);
int amdgpu_sched_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *filp);

#endif 
