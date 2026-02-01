
 

#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>

#include <drm/drm_encoder.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/gpu_scheduler.h>

#include "uapi/drm/v3d_drm.h"

struct clk;
struct platform_device;
struct reset_control;

#define GMP_GRANULARITY (128 * 1024)

#define V3D_MAX_QUEUES (V3D_CACHE_CLEAN + 1)

struct v3d_queue_state {
	struct drm_gpu_scheduler sched;

	u64 fence_context;
	u64 emit_seqno;
};

 
struct v3d_perfmon {
	 
	refcount_t refcnt;

	 
	struct mutex lock;

	 
	u8 ncounters;

	 
	u8 counters[DRM_V3D_MAX_PERF_COUNTERS];

	 
	u64 values[];
};

struct v3d_dev {
	struct drm_device drm;

	 
	int ver;
	bool single_irq_line;

	void __iomem *hub_regs;
	void __iomem *core_regs[3];
	void __iomem *bridge_regs;
	void __iomem *gca_regs;
	struct clk *clk;
	struct reset_control *reset;

	 
	volatile u32 *pt;
	dma_addr_t pt_paddr;

	 
	void *mmu_scratch;
	dma_addr_t mmu_scratch_paddr;
	 
	int va_width;

	 
	u32 cores;

	 
	struct drm_mm mm;
	spinlock_t mm_lock;

	struct work_struct overflow_mem_work;

	struct v3d_bin_job *bin_job;
	struct v3d_render_job *render_job;
	struct v3d_tfu_job *tfu_job;
	struct v3d_csd_job *csd_job;

	struct v3d_queue_state queue[V3D_MAX_QUEUES];

	 
	spinlock_t job_lock;

	 
	struct v3d_perfmon *active_perfmon;

	 
	struct mutex bo_lock;

	 
	struct mutex reset_lock;

	 
	struct mutex sched_lock;

	 
	struct mutex cache_clean_lock;

	struct {
		u32 num_allocated;
		u32 pages_allocated;
	} bo_stats;
};

static inline struct v3d_dev *
to_v3d_dev(struct drm_device *dev)
{
	return container_of(dev, struct v3d_dev, drm);
}

static inline bool
v3d_has_csd(struct v3d_dev *v3d)
{
	return v3d->ver >= 41;
}

#define v3d_to_pdev(v3d) to_platform_device((v3d)->drm.dev)

 
struct v3d_file_priv {
	struct v3d_dev *v3d;

	struct {
		struct idr idr;
		struct mutex lock;
	} perfmon;

	struct drm_sched_entity sched_entity[V3D_MAX_QUEUES];
};

struct v3d_bo {
	struct drm_gem_shmem_object base;

	struct drm_mm_node node;

	 
	struct list_head unref_head;
};

static inline struct v3d_bo *
to_v3d_bo(struct drm_gem_object *bo)
{
	return (struct v3d_bo *)bo;
}

struct v3d_fence {
	struct dma_fence base;
	struct drm_device *dev;
	 
	u64 seqno;
	enum v3d_queue queue;
};

static inline struct v3d_fence *
to_v3d_fence(struct dma_fence *fence)
{
	return (struct v3d_fence *)fence;
}

#define V3D_READ(offset) readl(v3d->hub_regs + offset)
#define V3D_WRITE(offset, val) writel(val, v3d->hub_regs + offset)

#define V3D_BRIDGE_READ(offset) readl(v3d->bridge_regs + offset)
#define V3D_BRIDGE_WRITE(offset, val) writel(val, v3d->bridge_regs + offset)

#define V3D_GCA_READ(offset) readl(v3d->gca_regs + offset)
#define V3D_GCA_WRITE(offset, val) writel(val, v3d->gca_regs + offset)

#define V3D_CORE_READ(core, offset) readl(v3d->core_regs[core] + offset)
#define V3D_CORE_WRITE(core, offset, val) writel(val, v3d->core_regs[core] + offset)

struct v3d_job {
	struct drm_sched_job base;

	struct kref refcount;

	struct v3d_dev *v3d;

	 
	struct drm_gem_object **bo;
	u32 bo_count;

	 
	struct dma_fence *irq_fence;

	 
	struct dma_fence *done_fence;

	 
	struct v3d_perfmon *perfmon;

	 
	void (*free)(struct kref *ref);
};

struct v3d_bin_job {
	struct v3d_job base;

	 
	u32 start, end;

	u32 timedout_ctca, timedout_ctra;

	 
	struct v3d_render_job *render;

	 
	u32 qma, qms, qts;
};

struct v3d_render_job {
	struct v3d_job base;

	 
	u32 start, end;

	u32 timedout_ctca, timedout_ctra;

	 
	struct list_head unref_list;
};

struct v3d_tfu_job {
	struct v3d_job base;

	struct drm_v3d_submit_tfu args;
};

struct v3d_csd_job {
	struct v3d_job base;

	u32 timedout_batches;

	struct drm_v3d_submit_csd args;
};

struct v3d_submit_outsync {
	struct drm_syncobj *syncobj;
};

struct v3d_submit_ext {
	u32 flags;
	u32 wait_stage;

	u32 in_sync_count;
	u64 in_syncs;

	u32 out_sync_count;
	struct v3d_submit_outsync *out_syncs;
};

 
#define __wait_for(OP, COND, US, Wmin, Wmax) ({ \
	const ktime_t end__ = ktime_add_ns(ktime_get_raw(), 1000ll * (US)); \
	long wait__ = (Wmin);  	\
	int ret__;							\
	might_sleep();							\
	for (;;) {							\
		const bool expired__ = ktime_after(ktime_get_raw(), end__); \
		OP;							\
		 		\
		barrier();						\
		if (COND) {						\
			ret__ = 0;					\
			break;						\
		}							\
		if (expired__) {					\
			ret__ = -ETIMEDOUT;				\
			break;						\
		}							\
		usleep_range(wait__, wait__ * 2);			\
		if (wait__ < (Wmax))					\
			wait__ <<= 1;					\
	}								\
	ret__;								\
})

#define _wait_for(COND, US, Wmin, Wmax)	__wait_for(, (COND), (US), (Wmin), \
						   (Wmax))
#define wait_for(COND, MS)		_wait_for((COND), (MS) * 1000, 10, 1000)

static inline unsigned long nsecs_to_jiffies_timeout(const u64 n)
{
	 
	if ((NSEC_PER_SEC % HZ) != 0 &&
	    div_u64(n, NSEC_PER_SEC) >= MAX_JIFFY_OFFSET / HZ)
		return MAX_JIFFY_OFFSET;

	return min_t(u64, MAX_JIFFY_OFFSET, nsecs_to_jiffies64(n) + 1);
}

 
struct drm_gem_object *v3d_create_object(struct drm_device *dev, size_t size);
void v3d_free_object(struct drm_gem_object *gem_obj);
struct v3d_bo *v3d_bo_create(struct drm_device *dev, struct drm_file *file_priv,
			     size_t size);
int v3d_create_bo_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int v3d_mmap_bo_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
int v3d_get_bo_offset_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv);
struct drm_gem_object *v3d_prime_import_sg_table(struct drm_device *dev,
						 struct dma_buf_attachment *attach,
						 struct sg_table *sgt);

 
void v3d_debugfs_init(struct drm_minor *minor);

 
extern const struct dma_fence_ops v3d_fence_ops;
struct dma_fence *v3d_fence_create(struct v3d_dev *v3d, enum v3d_queue queue);

 
int v3d_gem_init(struct drm_device *dev);
void v3d_gem_destroy(struct drm_device *dev);
int v3d_submit_cl_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int v3d_submit_tfu_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int v3d_submit_csd_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int v3d_wait_bo_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
void v3d_job_cleanup(struct v3d_job *job);
void v3d_job_put(struct v3d_job *job);
void v3d_reset(struct v3d_dev *v3d);
void v3d_invalidate_caches(struct v3d_dev *v3d);
void v3d_clean_caches(struct v3d_dev *v3d);

 
int v3d_irq_init(struct v3d_dev *v3d);
void v3d_irq_enable(struct v3d_dev *v3d);
void v3d_irq_disable(struct v3d_dev *v3d);
void v3d_irq_reset(struct v3d_dev *v3d);

 
int v3d_mmu_get_offset(struct drm_file *file_priv, struct v3d_bo *bo,
		       u32 *offset);
int v3d_mmu_set_page_table(struct v3d_dev *v3d);
void v3d_mmu_insert_ptes(struct v3d_bo *bo);
void v3d_mmu_remove_ptes(struct v3d_bo *bo);

 
int v3d_sched_init(struct v3d_dev *v3d);
void v3d_sched_fini(struct v3d_dev *v3d);

 
void v3d_perfmon_get(struct v3d_perfmon *perfmon);
void v3d_perfmon_put(struct v3d_perfmon *perfmon);
void v3d_perfmon_start(struct v3d_dev *v3d, struct v3d_perfmon *perfmon);
void v3d_perfmon_stop(struct v3d_dev *v3d, struct v3d_perfmon *perfmon,
		      bool capture);
struct v3d_perfmon *v3d_perfmon_find(struct v3d_file_priv *v3d_priv, int id);
void v3d_perfmon_open_file(struct v3d_file_priv *v3d_priv);
void v3d_perfmon_close_file(struct v3d_file_priv *v3d_priv);
int v3d_perfmon_create_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
int v3d_perfmon_destroy_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int v3d_perfmon_get_values_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
