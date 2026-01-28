#ifndef __A5XX_GPU_H__
#define __A5XX_GPU_H__
#include "adreno_gpu.h"
#undef ROP_COPY
#undef ROP_XOR
#include "a5xx.xml.h"
struct a5xx_gpu {
	struct adreno_gpu base;
	struct drm_gem_object *pm4_bo;
	uint64_t pm4_iova;
	struct drm_gem_object *pfp_bo;
	uint64_t pfp_iova;
	struct drm_gem_object *gpmu_bo;
	uint64_t gpmu_iova;
	uint32_t gpmu_dwords;
	uint32_t lm_leakage;
	struct msm_ringbuffer *cur_ring;
	struct msm_ringbuffer *next_ring;
	struct drm_gem_object *preempt_bo[MSM_GPU_MAX_RINGS];
	struct drm_gem_object *preempt_counters_bo[MSM_GPU_MAX_RINGS];
	struct a5xx_preempt_record *preempt[MSM_GPU_MAX_RINGS];
	uint64_t preempt_iova[MSM_GPU_MAX_RINGS];
	atomic_t preempt_state;
	struct timer_list preempt_timer;
	struct drm_gem_object *shadow_bo;
	uint64_t shadow_iova;
	uint32_t *shadow;
	bool has_whereami;
};
#define to_a5xx_gpu(x) container_of(x, struct a5xx_gpu, base)
#ifdef CONFIG_DEBUG_FS
void a5xx_debugfs_init(struct msm_gpu *gpu, struct drm_minor *minor);
#endif
enum preempt_state {
	PREEMPT_NONE = 0,
	PREEMPT_START,
	PREEMPT_ABORT,
	PREEMPT_TRIGGERED,
	PREEMPT_FAULTED,
	PREEMPT_PENDING,
};
struct a5xx_preempt_record {
	uint32_t magic;
	uint32_t info;
	uint32_t data;
	uint32_t cntl;
	uint32_t rptr;
	uint32_t wptr;
	uint64_t rptr_addr;
	uint64_t rbase;
	uint64_t counter;
};
#define A5XX_PREEMPT_RECORD_MAGIC 0x27C4BAFCUL
#define A5XX_PREEMPT_RECORD_SIZE (64 * 1024)
#define A5XX_PREEMPT_COUNTER_SIZE (16 * 4)
int a5xx_power_init(struct msm_gpu *gpu);
void a5xx_gpmu_ucode_init(struct msm_gpu *gpu);
static inline int spin_usecs(struct msm_gpu *gpu, uint32_t usecs,
		uint32_t reg, uint32_t mask, uint32_t value)
{
	while (usecs--) {
		udelay(1);
		if ((gpu_read(gpu, reg) & mask) == value)
			return 0;
		cpu_relax();
	}
	return -ETIMEDOUT;
}
#define shadowptr(a5xx_gpu, ring) ((a5xx_gpu)->shadow_iova + \
		((ring)->id * sizeof(uint32_t)))
bool a5xx_idle(struct msm_gpu *gpu, struct msm_ringbuffer *ring);
void a5xx_set_hwcg(struct msm_gpu *gpu, bool state);
void a5xx_preempt_init(struct msm_gpu *gpu);
void a5xx_preempt_hw_init(struct msm_gpu *gpu);
void a5xx_preempt_trigger(struct msm_gpu *gpu);
void a5xx_preempt_irq(struct msm_gpu *gpu);
void a5xx_preempt_fini(struct msm_gpu *gpu);
void a5xx_flush(struct msm_gpu *gpu, struct msm_ringbuffer *ring, bool sync);
static inline bool a5xx_in_preempt(struct a5xx_gpu *a5xx_gpu)
{
	int preempt_state = atomic_read(&a5xx_gpu->preempt_state);
	return !(preempt_state == PREEMPT_NONE ||
			preempt_state == PREEMPT_ABORT);
}
#endif  
