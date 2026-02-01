 
#ifndef __AMDGPU_VM_H__
#define __AMDGPU_VM_H__

#include <linux/idr.h>
#include <linux/kfifo.h>
#include <linux/rbtree.h>
#include <drm/gpu_scheduler.h>
#include <drm/drm_file.h>
#include <drm/ttm/ttm_bo.h>
#include <linux/sched/mm.h>

#include "amdgpu_sync.h"
#include "amdgpu_ring.h"
#include "amdgpu_ids.h"

struct drm_exec;

struct amdgpu_bo_va;
struct amdgpu_job;
struct amdgpu_bo_list_entry;
struct amdgpu_bo_vm;
struct amdgpu_mem_stats;

 

 
#define AMDGPU_VM_MAX_UPDATE_SIZE	0x3FFFF

 
#define AMDGPU_VM_PTE_COUNT(adev) (1 << (adev)->vm_manager.block_size)

#define AMDGPU_PTE_VALID	(1ULL << 0)
#define AMDGPU_PTE_SYSTEM	(1ULL << 1)
#define AMDGPU_PTE_SNOOPED	(1ULL << 2)

 
#define AMDGPU_PTE_TMZ		(1ULL << 3)

 
#define AMDGPU_PTE_EXECUTABLE	(1ULL << 4)

#define AMDGPU_PTE_READABLE	(1ULL << 5)
#define AMDGPU_PTE_WRITEABLE	(1ULL << 6)

#define AMDGPU_PTE_FRAG(x)	((x & 0x1fULL) << 7)

 
#define AMDGPU_PTE_PRT		(1ULL << 51)

 
#define AMDGPU_PDE_PTE		(1ULL << 54)

#define AMDGPU_PTE_LOG          (1ULL << 55)

 
#define AMDGPU_PTE_TF		(1ULL << 56)

 
#define AMDGPU_PTE_NOALLOC	(1ULL << 58)

 
#define AMDGPU_PDE_BFS(a)	((uint64_t)a << 59)

 
#define AMDGPU_VM_NORETRY_FLAGS	(AMDGPU_PTE_EXECUTABLE | AMDGPU_PDE_PTE | \
				AMDGPU_PTE_TF)

 
#define AMDGPU_VM_NORETRY_FLAGS_TF (AMDGPU_PTE_VALID | AMDGPU_PTE_SYSTEM | \
				   AMDGPU_PTE_PRT)
 
#define AMDGPU_PTE_MTYPE_VG10(a)	((uint64_t)(a) << 57)
#define AMDGPU_PTE_MTYPE_VG10_MASK	AMDGPU_PTE_MTYPE_VG10(3ULL)

#define AMDGPU_MTYPE_NC 0
#define AMDGPU_MTYPE_CC 2

#define AMDGPU_PTE_DEFAULT_ATC  (AMDGPU_PTE_SYSTEM      \
                                | AMDGPU_PTE_SNOOPED    \
                                | AMDGPU_PTE_EXECUTABLE \
                                | AMDGPU_PTE_READABLE   \
                                | AMDGPU_PTE_WRITEABLE  \
                                | AMDGPU_PTE_MTYPE_VG10(AMDGPU_MTYPE_CC))

 
#define AMDGPU_PTE_MTYPE_NV10(a)       ((uint64_t)(a) << 48)
#define AMDGPU_PTE_MTYPE_NV10_MASK     AMDGPU_PTE_MTYPE_NV10(7ULL)

 
#define AMDGPU_VM_FAULT_STOP_NEVER	0
#define AMDGPU_VM_FAULT_STOP_FIRST	1
#define AMDGPU_VM_FAULT_STOP_ALWAYS	2

 
#define AMDGPU_VM_RESERVED_VRAM		(8ULL << 20)

 
#define AMDGPU_MAX_VMHUBS			13
#define AMDGPU_GFXHUB(x)			(x)
#define AMDGPU_MMHUB0(x)			(8 + x)
#define AMDGPU_MMHUB1(x)			(8 + 4 + x)

 
#define AMDGPU_VA_RESERVED_SIZE			(2ULL << 20)

 
#define AMDGPU_VM_USE_CPU_FOR_GFX (1 << 0)
#define AMDGPU_VM_USE_CPU_FOR_COMPUTE (1 << 1)

 
enum amdgpu_vm_level {
	AMDGPU_VM_PDB2,
	AMDGPU_VM_PDB1,
	AMDGPU_VM_PDB0,
	AMDGPU_VM_PTB
};

 
struct amdgpu_vm_bo_base {
	 
	struct amdgpu_vm		*vm;
	struct amdgpu_bo		*bo;

	 
	struct amdgpu_vm_bo_base	*next;

	 
	struct list_head		vm_status;

	 
	bool				moved;
};

 
struct amdgpu_vm_pte_funcs {
	 
	unsigned	copy_pte_num_dw;

	 
	void (*copy_pte)(struct amdgpu_ib *ib,
			 uint64_t pe, uint64_t src,
			 unsigned count);

	 
	void (*write_pte)(struct amdgpu_ib *ib, uint64_t pe,
			  uint64_t value, unsigned count,
			  uint32_t incr);
	 
	void (*set_pte_pde)(struct amdgpu_ib *ib,
			    uint64_t pe,
			    uint64_t addr, unsigned count,
			    uint32_t incr, uint64_t flags);
};

struct amdgpu_task_info {
	char	process_name[TASK_COMM_LEN];
	char	task_name[TASK_COMM_LEN];
	pid_t	pid;
	pid_t	tgid;
};

 
struct amdgpu_vm_update_params {

	 
	struct amdgpu_device *adev;

	 
	struct amdgpu_vm *vm;

	 
	bool immediate;

	 
	bool unlocked;

	 
	dma_addr_t *pages_addr;

	 
	struct amdgpu_job *job;

	 
	unsigned int num_dw_left;

	 
	bool table_freed;
};

struct amdgpu_vm_update_funcs {
	int (*map_table)(struct amdgpu_bo_vm *bo);
	int (*prepare)(struct amdgpu_vm_update_params *p, struct dma_resv *resv,
		       enum amdgpu_sync_mode sync_mode);
	int (*update)(struct amdgpu_vm_update_params *p,
		      struct amdgpu_bo_vm *bo, uint64_t pe, uint64_t addr,
		      unsigned count, uint32_t incr, uint64_t flags);
	int (*commit)(struct amdgpu_vm_update_params *p,
		      struct dma_fence **fence);
};

struct amdgpu_vm {
	 
	struct rb_root_cached	va;

	 
	struct mutex		eviction_lock;
	bool			evicting;
	unsigned int		saved_flags;

	 
	spinlock_t		status_lock;

	 
	struct list_head	evicted;

	 
	struct list_head	relocated;

	 
	struct list_head	moved;

	 
	struct list_head	idle;

	 
	struct list_head	invalidated;

	 
	struct list_head	freed;

	 
	struct list_head        done;

	 
	struct list_head	pt_freed;
	struct work_struct	pt_free_work;

	 
	struct amdgpu_vm_bo_base     root;
	struct dma_fence	*last_update;

	 
	struct drm_sched_entity	immediate;
	struct drm_sched_entity	delayed;

	 
	atomic64_t		tlb_seq;
	struct dma_fence	*last_tlb_flush;

	 
	uint64_t		generation;

	 
	struct dma_fence	*last_unlocked;

	unsigned int		pasid;
	bool			reserved_vmid[AMDGPU_MAX_VMHUBS];

	 
	bool					use_cpu_for_update;

	 
	const struct amdgpu_vm_update_funcs	*update_funcs;

	 
	bool			pte_support_ats;

	 
	DECLARE_KFIFO(faults, u64, 128);

	 
	struct amdkfd_process_info *process_info;

	 
	struct list_head	vm_list_node;

	 
	uint64_t		pd_phys_addr;

	 
	struct amdgpu_task_info task_info;

	 
	struct ttm_lru_bulk_move lru_bulk_move;
	 
	bool			is_compute_context;

	 
	int8_t			mem_id;
};

struct amdgpu_vm_manager {
	 
	struct amdgpu_vmid_mgr			id_mgr[AMDGPU_MAX_VMHUBS];
	unsigned int				first_kfd_vmid;
	bool					concurrent_flush;

	 
	u64					fence_context;
	unsigned				seqno[AMDGPU_MAX_RINGS];

	uint64_t				max_pfn;
	uint32_t				num_level;
	uint32_t				block_size;
	uint32_t				fragment_size;
	enum amdgpu_vm_level			root_level;
	 
	u64					vram_base_offset;
	 
	const struct amdgpu_vm_pte_funcs	*vm_pte_funcs;
	struct drm_gpu_scheduler		*vm_pte_scheds[AMDGPU_MAX_RINGS];
	unsigned				vm_pte_num_scheds;
	struct amdgpu_ring			*page_fault;

	 
	spinlock_t				prt_lock;
	atomic_t				num_prt_users;

	 
	int					vm_update_mode;

	 
	struct xarray				pasids;
};

struct amdgpu_bo_va_mapping;

#define amdgpu_vm_copy_pte(adev, ib, pe, src, count) ((adev)->vm_manager.vm_pte_funcs->copy_pte((ib), (pe), (src), (count)))
#define amdgpu_vm_write_pte(adev, ib, pe, value, count, incr) ((adev)->vm_manager.vm_pte_funcs->write_pte((ib), (pe), (value), (count), (incr)))
#define amdgpu_vm_set_pte_pde(adev, ib, pe, addr, count, incr, flags) ((adev)->vm_manager.vm_pte_funcs->set_pte_pde((ib), (pe), (addr), (count), (incr), (flags)))

extern const struct amdgpu_vm_update_funcs amdgpu_vm_cpu_funcs;
extern const struct amdgpu_vm_update_funcs amdgpu_vm_sdma_funcs;

void amdgpu_vm_manager_init(struct amdgpu_device *adev);
void amdgpu_vm_manager_fini(struct amdgpu_device *adev);

int amdgpu_vm_set_pasid(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			u32 pasid);

long amdgpu_vm_wait_idle(struct amdgpu_vm *vm, long timeout);
int amdgpu_vm_init(struct amdgpu_device *adev, struct amdgpu_vm *vm, int32_t xcp_id);
int amdgpu_vm_make_compute(struct amdgpu_device *adev, struct amdgpu_vm *vm);
void amdgpu_vm_release_compute(struct amdgpu_device *adev, struct amdgpu_vm *vm);
void amdgpu_vm_fini(struct amdgpu_device *adev, struct amdgpu_vm *vm);
int amdgpu_vm_lock_pd(struct amdgpu_vm *vm, struct drm_exec *exec,
		      unsigned int num_fences);
bool amdgpu_vm_ready(struct amdgpu_vm *vm);
uint64_t amdgpu_vm_generation(struct amdgpu_device *adev, struct amdgpu_vm *vm);
int amdgpu_vm_validate_pt_bos(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			      int (*callback)(void *p, struct amdgpu_bo *bo),
			      void *param);
int amdgpu_vm_flush(struct amdgpu_ring *ring, struct amdgpu_job *job, bool need_pipe_sync);
int amdgpu_vm_update_pdes(struct amdgpu_device *adev,
			  struct amdgpu_vm *vm, bool immediate);
int amdgpu_vm_clear_freed(struct amdgpu_device *adev,
			  struct amdgpu_vm *vm,
			  struct dma_fence **fence);
int amdgpu_vm_handle_moved(struct amdgpu_device *adev,
			   struct amdgpu_vm *vm);
void amdgpu_vm_bo_base_init(struct amdgpu_vm_bo_base *base,
			    struct amdgpu_vm *vm, struct amdgpu_bo *bo);
int amdgpu_vm_update_range(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			   bool immediate, bool unlocked, bool flush_tlb,
			   struct dma_resv *resv, uint64_t start, uint64_t last,
			   uint64_t flags, uint64_t offset, uint64_t vram_base,
			   struct ttm_resource *res, dma_addr_t *pages_addr,
			   struct dma_fence **fence);
int amdgpu_vm_bo_update(struct amdgpu_device *adev,
			struct amdgpu_bo_va *bo_va,
			bool clear);
bool amdgpu_vm_evictable(struct amdgpu_bo *bo);
void amdgpu_vm_bo_invalidate(struct amdgpu_device *adev,
			     struct amdgpu_bo *bo, bool evicted);
uint64_t amdgpu_vm_map_gart(const dma_addr_t *pages_addr, uint64_t addr);
struct amdgpu_bo_va *amdgpu_vm_bo_find(struct amdgpu_vm *vm,
				       struct amdgpu_bo *bo);
struct amdgpu_bo_va *amdgpu_vm_bo_add(struct amdgpu_device *adev,
				      struct amdgpu_vm *vm,
				      struct amdgpu_bo *bo);
int amdgpu_vm_bo_map(struct amdgpu_device *adev,
		     struct amdgpu_bo_va *bo_va,
		     uint64_t addr, uint64_t offset,
		     uint64_t size, uint64_t flags);
int amdgpu_vm_bo_replace_map(struct amdgpu_device *adev,
			     struct amdgpu_bo_va *bo_va,
			     uint64_t addr, uint64_t offset,
			     uint64_t size, uint64_t flags);
int amdgpu_vm_bo_unmap(struct amdgpu_device *adev,
		       struct amdgpu_bo_va *bo_va,
		       uint64_t addr);
int amdgpu_vm_bo_clear_mappings(struct amdgpu_device *adev,
				struct amdgpu_vm *vm,
				uint64_t saddr, uint64_t size);
struct amdgpu_bo_va_mapping *amdgpu_vm_bo_lookup_mapping(struct amdgpu_vm *vm,
							 uint64_t addr);
void amdgpu_vm_bo_trace_cs(struct amdgpu_vm *vm, struct ww_acquire_ctx *ticket);
void amdgpu_vm_bo_del(struct amdgpu_device *adev,
		      struct amdgpu_bo_va *bo_va);
void amdgpu_vm_adjust_size(struct amdgpu_device *adev, uint32_t min_vm_size,
			   uint32_t fragment_size_default, unsigned max_level,
			   unsigned max_bits);
int amdgpu_vm_ioctl(struct drm_device *dev, void *data, struct drm_file *filp);
bool amdgpu_vm_need_pipeline_sync(struct amdgpu_ring *ring,
				  struct amdgpu_job *job);
void amdgpu_vm_check_compute_bug(struct amdgpu_device *adev);

void amdgpu_vm_get_task_info(struct amdgpu_device *adev, u32 pasid,
			     struct amdgpu_task_info *task_info);
bool amdgpu_vm_handle_fault(struct amdgpu_device *adev, u32 pasid,
			    u32 vmid, u32 node_id, uint64_t addr,
			    bool write_fault);

void amdgpu_vm_set_task_info(struct amdgpu_vm *vm);

void amdgpu_vm_move_to_lru_tail(struct amdgpu_device *adev,
				struct amdgpu_vm *vm);
void amdgpu_vm_get_memory(struct amdgpu_vm *vm,
			  struct amdgpu_mem_stats *stats);

int amdgpu_vm_pt_clear(struct amdgpu_device *adev, struct amdgpu_vm *vm,
		       struct amdgpu_bo_vm *vmbo, bool immediate);
int amdgpu_vm_pt_create(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			int level, bool immediate, struct amdgpu_bo_vm **vmbo,
			int32_t xcp_id);
void amdgpu_vm_pt_free_root(struct amdgpu_device *adev, struct amdgpu_vm *vm);
bool amdgpu_vm_pt_is_root_clean(struct amdgpu_device *adev,
				struct amdgpu_vm *vm);

int amdgpu_vm_pde_update(struct amdgpu_vm_update_params *params,
			 struct amdgpu_vm_bo_base *entry);
int amdgpu_vm_ptes_update(struct amdgpu_vm_update_params *params,
			  uint64_t start, uint64_t end,
			  uint64_t dst, uint64_t flags);
void amdgpu_vm_pt_free_work(struct work_struct *work);

#if defined(CONFIG_DEBUG_FS)
void amdgpu_debugfs_vm_bo_info(struct amdgpu_vm *vm, struct seq_file *m);
#endif

int amdgpu_vm_pt_map_tables(struct amdgpu_device *adev, struct amdgpu_vm *vm);

 
static inline uint64_t amdgpu_vm_tlb_seq(struct amdgpu_vm *vm)
{
	unsigned long flags;
	spinlock_t *lock;

	 
	rcu_read_lock();
	lock = vm->last_tlb_flush->lock;
	rcu_read_unlock();

	spin_lock_irqsave(lock, flags);
	spin_unlock_irqrestore(lock, flags);

	return atomic64_read(&vm->tlb_seq);
}

 
static inline void amdgpu_vm_eviction_lock(struct amdgpu_vm *vm)
{
	mutex_lock(&vm->eviction_lock);
	vm->saved_flags = memalloc_noreclaim_save();
}

static inline bool amdgpu_vm_eviction_trylock(struct amdgpu_vm *vm)
{
	if (mutex_trylock(&vm->eviction_lock)) {
		vm->saved_flags = memalloc_noreclaim_save();
		return true;
	}
	return false;
}

static inline void amdgpu_vm_eviction_unlock(struct amdgpu_vm *vm)
{
	memalloc_noreclaim_restore(vm->saved_flags);
	mutex_unlock(&vm->eviction_lock);
}

#endif
