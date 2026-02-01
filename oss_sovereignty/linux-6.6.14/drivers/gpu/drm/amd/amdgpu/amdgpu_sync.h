 
#ifndef __AMDGPU_SYNC_H__
#define __AMDGPU_SYNC_H__

#include <linux/hashtable.h>

struct dma_fence;
struct dma_resv;
struct amdgpu_device;
struct amdgpu_ring;
struct amdgpu_job;

enum amdgpu_sync_mode {
	AMDGPU_SYNC_ALWAYS,
	AMDGPU_SYNC_NE_OWNER,
	AMDGPU_SYNC_EQ_OWNER,
	AMDGPU_SYNC_EXPLICIT
};

 
struct amdgpu_sync {
	DECLARE_HASHTABLE(fences, 4);
};

void amdgpu_sync_create(struct amdgpu_sync *sync);
int amdgpu_sync_fence(struct amdgpu_sync *sync, struct dma_fence *f);
int amdgpu_sync_resv(struct amdgpu_device *adev, struct amdgpu_sync *sync,
		     struct dma_resv *resv, enum amdgpu_sync_mode mode,
		     void *owner);
struct dma_fence *amdgpu_sync_peek_fence(struct amdgpu_sync *sync,
				     struct amdgpu_ring *ring);
struct dma_fence *amdgpu_sync_get_fence(struct amdgpu_sync *sync);
int amdgpu_sync_clone(struct amdgpu_sync *source, struct amdgpu_sync *clone);
int amdgpu_sync_push_to_job(struct amdgpu_sync *sync, struct amdgpu_job *job);
int amdgpu_sync_wait(struct amdgpu_sync *sync, bool intr);
void amdgpu_sync_free(struct amdgpu_sync *sync);
int amdgpu_sync_init(void);
void amdgpu_sync_fini(void);

#endif
