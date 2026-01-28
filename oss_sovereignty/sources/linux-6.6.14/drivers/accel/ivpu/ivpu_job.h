


#ifndef __IVPU_JOB_H__
#define __IVPU_JOB_H__

#include <linux/kref.h>
#include <linux/idr.h>

#include "ivpu_gem.h"

struct ivpu_device;
struct ivpu_file_priv;


struct ivpu_cmdq {
	struct vpu_job_queue *jobq;
	struct ivpu_bo *mem;
	u32 entry_count;
	u32 db_id;
	bool db_registered;
};


struct ivpu_job {
	struct kref ref;
	struct ivpu_device *vdev;
	struct ivpu_file_priv *file_priv;
	struct dma_fence *done_fence;
	u64 cmd_buf_vpu_addr;
	u32 job_id;
	u32 engine_idx;
	size_t bo_count;
	struct ivpu_bo *bos[];
};

int ivpu_submit_ioctl(struct drm_device *dev, void *data, struct drm_file *file);

void ivpu_cmdq_release_all(struct ivpu_file_priv *file_priv);
void ivpu_cmdq_reset_all_contexts(struct ivpu_device *vdev);

int ivpu_job_done_thread_init(struct ivpu_device *vdev);
void ivpu_job_done_thread_fini(struct ivpu_device *vdev);

void ivpu_jobs_abort_all(struct ivpu_device *vdev);

#endif 
