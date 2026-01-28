#ifndef KFD_SVM_H_
#define KFD_SVM_H_
#if IS_ENABLED(CONFIG_HSA_AMD_SVM)
#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/sched/mm.h>
#include <linux/hmm.h>
#include "amdgpu.h"
#include "kfd_priv.h"
#define SVM_RANGE_VRAM_DOMAIN (1UL << 0)
#define SVM_ADEV_PGMAP_OWNER(adev)\
			((adev)->hive ? (void *)(adev)->hive : (void *)(adev))
struct svm_range_bo {
	struct amdgpu_bo		*bo;
	struct kref			kref;
	struct list_head		range_list;  
	spinlock_t			list_lock;
	struct amdgpu_amdkfd_fence	*eviction_fence;
	struct work_struct		eviction_work;
	uint32_t			evicting;
	struct work_struct		release_work;
	struct kfd_node			*node;
};
enum svm_work_list_ops {
	SVM_OP_NULL,
	SVM_OP_UNMAP_RANGE,
	SVM_OP_UPDATE_RANGE_NOTIFIER,
	SVM_OP_UPDATE_RANGE_NOTIFIER_AND_MAP,
	SVM_OP_ADD_RANGE,
	SVM_OP_ADD_RANGE_AND_MAP
};
struct svm_work_list_item {
	enum svm_work_list_ops op;
	struct mm_struct *mm;
};
struct svm_range {
	struct svm_range_list		*svms;
	struct mutex			migrate_mutex;
	unsigned long			start;
	unsigned long			last;
	struct interval_tree_node	it_node;
	struct list_head		list;
	struct list_head		update_list;
	uint64_t			npages;
	dma_addr_t			*dma_addr[MAX_GPU_INSTANCE];
	struct ttm_resource		*ttm_res;
	uint64_t			offset;
	struct svm_range_bo		*svm_bo;
	struct list_head		svm_bo_list;
	struct mutex                    lock;
	unsigned int                    saved_flags;
	uint32_t			flags;
	uint32_t			preferred_loc;
	uint32_t			prefetch_loc;
	uint32_t			actual_loc;
	uint8_t				granularity;
	atomic_t			invalid;
	ktime_t				validate_timestamp;
	struct mmu_interval_notifier	notifier;
	struct svm_work_list_item	work_item;
	struct list_head		deferred_list;
	struct list_head		child_list;
	DECLARE_BITMAP(bitmap_access, MAX_GPU_INSTANCE);
	DECLARE_BITMAP(bitmap_aip, MAX_GPU_INSTANCE);
	bool				mapped_to_gpu;
};
static inline void svm_range_lock(struct svm_range *prange)
{
	mutex_lock(&prange->lock);
	prange->saved_flags = memalloc_noreclaim_save();
}
static inline void svm_range_unlock(struct svm_range *prange)
{
	memalloc_noreclaim_restore(prange->saved_flags);
	mutex_unlock(&prange->lock);
}
static inline struct svm_range_bo *svm_range_bo_ref(struct svm_range_bo *svm_bo)
{
	if (svm_bo)
		kref_get(&svm_bo->kref);
	return svm_bo;
}
int svm_range_list_init(struct kfd_process *p);
void svm_range_list_fini(struct kfd_process *p);
int svm_ioctl(struct kfd_process *p, enum kfd_ioctl_svm_op op, uint64_t start,
	      uint64_t size, uint32_t nattrs,
	      struct kfd_ioctl_svm_attribute *attrs);
struct svm_range *svm_range_from_addr(struct svm_range_list *svms,
				      unsigned long addr,
				      struct svm_range **parent);
struct kfd_node *svm_range_get_node_by_id(struct svm_range *prange,
					  uint32_t gpu_id);
int svm_range_vram_node_new(struct kfd_node *node, struct svm_range *prange,
			    bool clear);
void svm_range_vram_node_free(struct svm_range *prange);
int svm_range_split_by_granularity(struct kfd_process *p, struct mm_struct *mm,
			       unsigned long addr, struct svm_range *parent,
			       struct svm_range *prange);
int svm_range_restore_pages(struct amdgpu_device *adev, unsigned int pasid,
			    uint32_t vmid, uint32_t node_id, uint64_t addr,
			    bool write_fault);
int svm_range_schedule_evict_svm_bo(struct amdgpu_amdkfd_fence *fence);
void svm_range_add_list_work(struct svm_range_list *svms,
			     struct svm_range *prange, struct mm_struct *mm,
			     enum svm_work_list_ops op);
void schedule_deferred_list_work(struct svm_range_list *svms);
void svm_range_dma_unmap(struct device *dev, dma_addr_t *dma_addr,
			 unsigned long offset, unsigned long npages);
void svm_range_free_dma_mappings(struct svm_range *prange, bool unmap_dma);
int svm_range_get_info(struct kfd_process *p, uint32_t *num_svm_ranges,
		       uint64_t *svm_priv_data_size);
int kfd_criu_checkpoint_svm(struct kfd_process *p,
			    uint8_t __user *user_priv_data,
			    uint64_t *priv_offset);
int kfd_criu_restore_svm(struct kfd_process *p,
			 uint8_t __user *user_priv_ptr,
			 uint64_t *priv_data_offset,
			 uint64_t max_priv_data_size);
int kfd_criu_resume_svm(struct kfd_process *p);
struct kfd_process_device *
svm_range_get_pdd_by_node(struct svm_range *prange, struct kfd_node *node);
void svm_range_list_lock_and_flush_work(struct svm_range_list *svms, struct mm_struct *mm);
#define KFD_IS_SVM_API_SUPPORTED(adev) ((adev)->kfd.pgmap.type != 0 ||\
					(adev)->gmc.is_app_apu)
void svm_range_bo_unref_async(struct svm_range_bo *svm_bo);
void svm_range_set_max_pages(struct amdgpu_device *adev);
int svm_range_switch_xnack_reserve_mem(struct kfd_process *p, bool xnack_enabled);
#else
struct kfd_process;
static inline int svm_range_list_init(struct kfd_process *p)
{
	return 0;
}
static inline void svm_range_list_fini(struct kfd_process *p)
{
}
static inline int svm_range_restore_pages(struct amdgpu_device *adev,
					  unsigned int pasid,
					  uint32_t client_id, uint32_t node_id,
					  uint64_t addr, bool write_fault)
{
	return -EFAULT;
}
static inline int svm_range_schedule_evict_svm_bo(
		struct amdgpu_amdkfd_fence *fence)
{
	WARN_ONCE(1, "SVM eviction fence triggered, but SVM is disabled");
	return -EINVAL;
}
static inline int svm_range_get_info(struct kfd_process *p,
				     uint32_t *num_svm_ranges,
				     uint64_t *svm_priv_data_size)
{
	*num_svm_ranges = 0;
	*svm_priv_data_size = 0;
	return 0;
}
static inline int kfd_criu_checkpoint_svm(struct kfd_process *p,
					  uint8_t __user *user_priv_data,
					  uint64_t *priv_offset)
{
	return 0;
}
static inline int kfd_criu_restore_svm(struct kfd_process *p,
				       uint8_t __user *user_priv_ptr,
				       uint64_t *priv_data_offset,
				       uint64_t max_priv_data_size)
{
	return -EINVAL;
}
static inline int kfd_criu_resume_svm(struct kfd_process *p)
{
	return 0;
}
static inline void svm_range_set_max_pages(struct amdgpu_device *adev)
{
}
#define KFD_IS_SVM_API_SUPPORTED(dev) false
#endif  
#endif  
