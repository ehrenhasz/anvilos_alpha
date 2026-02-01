 
#ifndef __AMDGPU_SMI_H__
#define __AMDGPU_SMI_H__

#include <linux/idr.h>
#include <linux/kfifo.h>
#include <linux/rbtree.h>
#include <drm/gpu_scheduler.h>
#include <drm/drm_file.h>
#include <linux/sched/mm.h>

#include "amdgpu_sync.h"
#include "amdgpu_ring.h"
#include "amdgpu_ids.h"

uint32_t amdgpu_get_ip_count(struct amdgpu_device *adev, int id);
void amdgpu_show_fdinfo(struct drm_printer *p, struct drm_file *file);

#endif
