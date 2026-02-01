 
 

#ifndef __MSM_GPU_H__
#define __MSM_GPU_H__

#include <linux/adreno-smmu-priv.h>
#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/interconnect.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include "msm_drv.h"
#include "msm_fence.h"
#include "msm_ringbuffer.h"
#include "msm_gem.h"

struct msm_gem_submit;
struct msm_gpu_perfcntr;
struct msm_gpu_state;
struct msm_file_private;

struct msm_gpu_config {
	const char *ioname;
	unsigned int nr_rings;
};

 
struct msm_gpu_funcs {
	int (*get_param)(struct msm_gpu *gpu, struct msm_file_private *ctx,
			 uint32_t param, uint64_t *value, uint32_t *len);
	int (*set_param)(struct msm_gpu *gpu, struct msm_file_private *ctx,
			 uint32_t param, uint64_t value, uint32_t len);
	int (*hw_init)(struct msm_gpu *gpu);

	 
	int (*ucode_load)(struct msm_gpu *gpu);

	int (*pm_suspend)(struct msm_gpu *gpu);
	int (*pm_resume)(struct msm_gpu *gpu);
	void (*submit)(struct msm_gpu *gpu, struct msm_gem_submit *submit);
	void (*flush)(struct msm_gpu *gpu, struct msm_ringbuffer *ring);
	irqreturn_t (*irq)(struct msm_gpu *irq);
	struct msm_ringbuffer *(*active_ring)(struct msm_gpu *gpu);
	void (*recover)(struct msm_gpu *gpu);
	void (*destroy)(struct msm_gpu *gpu);
#if defined(CONFIG_DEBUG_FS) || defined(CONFIG_DEV_COREDUMP)
	 
	void (*show)(struct msm_gpu *gpu, struct msm_gpu_state *state,
			struct drm_printer *p);
	 
	void (*debugfs_init)(struct msm_gpu *gpu, struct drm_minor *minor);
#endif
	 
	u64 (*gpu_busy)(struct msm_gpu *gpu, unsigned long *out_sample_rate);
	struct msm_gpu_state *(*gpu_state_get)(struct msm_gpu *gpu);
	int (*gpu_state_put)(struct msm_gpu_state *state);
	unsigned long (*gpu_get_freq)(struct msm_gpu *gpu);
	 
	void (*gpu_set_freq)(struct msm_gpu *gpu, struct dev_pm_opp *opp,
			     bool suspended);
	struct msm_gem_address_space *(*create_address_space)
		(struct msm_gpu *gpu, struct platform_device *pdev);
	struct msm_gem_address_space *(*create_private_address_space)
		(struct msm_gpu *gpu);
	uint32_t (*get_rptr)(struct msm_gpu *gpu, struct msm_ringbuffer *ring);

	 
	bool (*progress)(struct msm_gpu *gpu, struct msm_ringbuffer *ring);
};

 
struct msm_gpu_fault_info {
	u64 ttbr0;
	unsigned long iova;
	int flags;
	const char *type;
	const char *block;
};

 
struct msm_gpu_devfreq {
	 
	struct devfreq *devfreq;

	 
	struct mutex lock;

	 
	unsigned long idle_freq;

	 
	struct dev_pm_qos_request boost_freq;

	 
	u64 busy_cycles;

	 
	ktime_t time;

	 
	ktime_t idle_time;

	 
	struct msm_hrtimer_work idle_work;

	 
	struct msm_hrtimer_work boost_work;

	 
	bool suspended;
};

struct msm_gpu {
	const char *name;
	struct drm_device *dev;
	struct platform_device *pdev;
	const struct msm_gpu_funcs *funcs;

	struct adreno_smmu_priv adreno_smmu;

	 
	spinlock_t perf_lock;
	bool perfcntr_active;
	struct {
		bool active;
		ktime_t time;
	} last_sample;
	uint32_t totaltime, activetime;     
	uint32_t last_cntrs[5];             
	const struct msm_gpu_perfcntr *perfcntrs;
	uint32_t num_perfcntrs;

	struct msm_ringbuffer *rb[MSM_GPU_MAX_RINGS];
	int nr_rings;

	 
	refcount_t sysprof_active;

	 
	int cur_ctx_seqno;

	 
	struct mutex lock;

	 
	int active_submits;

	 
	struct mutex active_lock;

	 
	bool needs_hw_init;

	 
	int global_faults;

	void __iomem *mmio;
	int irq;

	struct msm_gem_address_space *aspace;

	 
	struct regulator *gpu_reg, *gpu_cx;
	struct clk_bulk_data *grp_clks;
	int nr_clocks;
	struct clk *ebi1_clk, *core_clk, *rbbmtimer_clk;
	uint32_t fast_rate;

	 
#define DRM_MSM_INACTIVE_PERIOD   66  

#define DRM_MSM_HANGCHECK_DEFAULT_PERIOD 500  
#define DRM_MSM_HANGCHECK_PROGRESS_RETRIES 3
	struct timer_list hangcheck_timer;

	 
	struct msm_gpu_fault_info fault_info;

	 
	struct kthread_work fault_work;

	 
	struct kthread_work recover_work;

	 
	wait_queue_head_t retire_event;

	 
	struct kthread_work retire_work;

	 
	struct kthread_worker *worker;

	struct drm_gem_object *memptrs_bo;

	struct msm_gpu_devfreq devfreq;

	uint32_t suspend_count;

	struct msm_gpu_state *crashstate;

	 
	bool hw_apriv;

	 
	bool allow_relocs;

	struct thermal_cooling_device *cooling;
};

static inline struct msm_gpu *dev_to_gpu(struct device *dev)
{
	struct adreno_smmu_priv *adreno_smmu = dev_get_drvdata(dev);

	if (!adreno_smmu)
		return NULL;

	return container_of(adreno_smmu, struct msm_gpu, adreno_smmu);
}

 
#define MSM_GPU_RINGBUFFER_SZ SZ_32K
#define MSM_GPU_RINGBUFFER_BLKSIZE 32

#define MSM_GPU_RB_CNTL_DEFAULT \
		(AXXX_CP_RB_CNTL_BUFSZ(ilog2(MSM_GPU_RINGBUFFER_SZ / 8)) | \
		AXXX_CP_RB_CNTL_BLKSZ(ilog2(MSM_GPU_RINGBUFFER_BLKSIZE / 8)))

static inline bool msm_gpu_active(struct msm_gpu *gpu)
{
	int i;

	for (i = 0; i < gpu->nr_rings; i++) {
		struct msm_ringbuffer *ring = gpu->rb[i];

		if (fence_after(ring->fctx->last_fence, ring->memptrs->fence))
			return true;
	}

	return false;
}

 

struct msm_gpu_perfcntr {
	uint32_t select_reg;
	uint32_t sample_reg;
	uint32_t select_val;
	const char *name;
};

 
#define NR_SCHED_PRIORITIES (1 + DRM_SCHED_PRIORITY_HIGH - DRM_SCHED_PRIORITY_MIN)

 
struct msm_file_private {
	rwlock_t queuelock;
	struct list_head submitqueues;
	int queueid;
	struct msm_gem_address_space *aspace;
	struct kref ref;
	int seqno;

	 
	int sysprof;

	 
	char *comm;

	 
	char *cmdline;

	 
	uint64_t elapsed_ns;

	 
	uint64_t cycles;

	 
	struct drm_sched_entity *entities[NR_SCHED_PRIORITIES * MSM_GPU_MAX_RINGS];
};

 
static inline int msm_gpu_convert_priority(struct msm_gpu *gpu, int prio,
		unsigned *ring_nr, enum drm_sched_priority *sched_prio)
{
	unsigned rn, sp;

	rn = div_u64_rem(prio, NR_SCHED_PRIORITIES, &sp);

	 
	sp = NR_SCHED_PRIORITIES - sp - 1;

	if (rn >= gpu->nr_rings)
		return -EINVAL;

	*ring_nr = rn;
	*sched_prio = sp;

	return 0;
}

 
struct msm_gpu_submitqueue {
	int id;
	u32 flags;
	u32 ring_nr;
	int faults;
	uint32_t last_fence;
	struct msm_file_private *ctx;
	struct list_head node;
	struct idr fence_idr;
	struct spinlock idr_lock;
	struct mutex lock;
	struct kref ref;
	struct drm_sched_entity *entity;
};

struct msm_gpu_state_bo {
	u64 iova;
	size_t size;
	void *data;
	bool encoded;
	char name[32];
};

struct msm_gpu_state {
	struct kref ref;
	struct timespec64 time;

	struct {
		u64 iova;
		u32 fence;
		u32 seqno;
		u32 rptr;
		u32 wptr;
		void *data;
		int data_size;
		bool encoded;
	} ring[MSM_GPU_MAX_RINGS];

	int nr_registers;
	u32 *registers;

	u32 rbbm_status;

	char *comm;
	char *cmd;

	struct msm_gpu_fault_info fault_info;

	int nr_bos;
	struct msm_gpu_state_bo *bos;
};

static inline void gpu_write(struct msm_gpu *gpu, u32 reg, u32 data)
{
	msm_writel(data, gpu->mmio + (reg << 2));
}

static inline u32 gpu_read(struct msm_gpu *gpu, u32 reg)
{
	return msm_readl(gpu->mmio + (reg << 2));
}

static inline void gpu_rmw(struct msm_gpu *gpu, u32 reg, u32 mask, u32 or)
{
	msm_rmw(gpu->mmio + (reg << 2), mask, or);
}

static inline u64 gpu_read64(struct msm_gpu *gpu, u32 reg)
{
	u64 val;

	 

	 
	val = (u64) msm_readl(gpu->mmio + (reg << 2));
	val |= ((u64) msm_readl(gpu->mmio + ((reg + 1) << 2)) << 32);

	return val;
}

static inline void gpu_write64(struct msm_gpu *gpu, u32 reg, u64 val)
{
	 
	msm_writel(lower_32_bits(val), gpu->mmio + (reg << 2));
	msm_writel(upper_32_bits(val), gpu->mmio + ((reg + 1) << 2));
}

int msm_gpu_pm_suspend(struct msm_gpu *gpu);
int msm_gpu_pm_resume(struct msm_gpu *gpu);

void msm_gpu_show_fdinfo(struct msm_gpu *gpu, struct msm_file_private *ctx,
			 struct drm_printer *p);

int msm_submitqueue_init(struct drm_device *drm, struct msm_file_private *ctx);
struct msm_gpu_submitqueue *msm_submitqueue_get(struct msm_file_private *ctx,
		u32 id);
int msm_submitqueue_create(struct drm_device *drm,
		struct msm_file_private *ctx,
		u32 prio, u32 flags, u32 *id);
int msm_submitqueue_query(struct drm_device *drm, struct msm_file_private *ctx,
		struct drm_msm_submitqueue_query *args);
int msm_submitqueue_remove(struct msm_file_private *ctx, u32 id);
void msm_submitqueue_close(struct msm_file_private *ctx);

void msm_submitqueue_destroy(struct kref *kref);

int msm_file_private_set_sysprof(struct msm_file_private *ctx,
				 struct msm_gpu *gpu, int sysprof);
void __msm_file_private_destroy(struct kref *kref);

static inline void msm_file_private_put(struct msm_file_private *ctx)
{
	kref_put(&ctx->ref, __msm_file_private_destroy);
}

static inline struct msm_file_private *msm_file_private_get(
	struct msm_file_private *ctx)
{
	kref_get(&ctx->ref);
	return ctx;
}

void msm_devfreq_init(struct msm_gpu *gpu);
void msm_devfreq_cleanup(struct msm_gpu *gpu);
void msm_devfreq_resume(struct msm_gpu *gpu);
void msm_devfreq_suspend(struct msm_gpu *gpu);
void msm_devfreq_boost(struct msm_gpu *gpu, unsigned factor);
void msm_devfreq_active(struct msm_gpu *gpu);
void msm_devfreq_idle(struct msm_gpu *gpu);

int msm_gpu_hw_init(struct msm_gpu *gpu);

void msm_gpu_perfcntr_start(struct msm_gpu *gpu);
void msm_gpu_perfcntr_stop(struct msm_gpu *gpu);
int msm_gpu_perfcntr_sample(struct msm_gpu *gpu, uint32_t *activetime,
		uint32_t *totaltime, uint32_t ncntrs, uint32_t *cntrs);

void msm_gpu_retire(struct msm_gpu *gpu);
void msm_gpu_submit(struct msm_gpu *gpu, struct msm_gem_submit *submit);

int msm_gpu_init(struct drm_device *drm, struct platform_device *pdev,
		struct msm_gpu *gpu, const struct msm_gpu_funcs *funcs,
		const char *name, struct msm_gpu_config *config);

struct msm_gem_address_space *
msm_gpu_create_private_address_space(struct msm_gpu *gpu, struct task_struct *task);

void msm_gpu_cleanup(struct msm_gpu *gpu);

struct msm_gpu *adreno_load_gpu(struct drm_device *dev);
void __init adreno_register(void);
void __exit adreno_unregister(void);

static inline void msm_submitqueue_put(struct msm_gpu_submitqueue *queue)
{
	if (queue)
		kref_put(&queue->ref, msm_submitqueue_destroy);
}

static inline struct msm_gpu_state *msm_gpu_crashstate_get(struct msm_gpu *gpu)
{
	struct msm_gpu_state *state = NULL;

	mutex_lock(&gpu->lock);

	if (gpu->crashstate) {
		kref_get(&gpu->crashstate->ref);
		state = gpu->crashstate;
	}

	mutex_unlock(&gpu->lock);

	return state;
}

static inline void msm_gpu_crashstate_put(struct msm_gpu *gpu)
{
	mutex_lock(&gpu->lock);

	if (gpu->crashstate) {
		if (gpu->funcs->gpu_state_put(gpu->crashstate))
			gpu->crashstate = NULL;
	}

	mutex_unlock(&gpu->lock);
}

 
#define check_apriv(gpu, flags) \
	(((gpu)->hw_apriv ? MSM_BO_MAP_PRIV : 0) | (flags))


#endif  
