 
 

#ifndef MSM_DISP_SNAPSHOT_H_
#define MSM_DISP_SNAPSHOT_H_

#include <drm/drm_atomic_helper.h>
#include <drm/drm_device.h>
#include "../../../drm_crtc_internal.h"
#include <drm/drm_print.h>
#include <drm/drm_atomic.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/devcoredump.h>
#include "msm_kms.h"

#define MSM_DISP_SNAPSHOT_MAX_BLKS		10

 
#define MSM_DISP_SNAPSHOT_DUMP_IN_CONSOLE 0

 
#define REG_DUMP_ALIGN		16

 
struct msm_disp_state {
	struct device *dev;
	struct drm_device *drm_dev;

	struct list_head blocks;

	struct drm_atomic_state *atomic_state;

	struct timespec64 time;
};

 
struct msm_disp_state_block {
	char name[SZ_128];
	struct list_head node;
	unsigned int size;
	u32 *state;
	void __iomem *base_addr;
};

 
int msm_disp_snapshot_init(struct drm_device *drm_dev);

 
void msm_disp_snapshot_destroy(struct drm_device *drm_dev);

 
struct msm_disp_state *msm_disp_snapshot_state_sync(struct msm_kms *kms);

 
void msm_disp_snapshot_state(struct drm_device *drm_dev);

 
void msm_disp_state_print(struct msm_disp_state *disp_state, struct drm_printer *p);

 
void msm_disp_snapshot_capture_state(struct msm_disp_state *disp_state);

 
void msm_disp_state_free(void *data);

 
__printf(4, 5)
void msm_disp_snapshot_add_block(struct msm_disp_state *disp_state, u32 len,
		void __iomem *base_addr, const char *fmt, ...);

#endif  
