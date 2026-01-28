#ifndef KFD_DEVICE_QUEUE_MANAGER_H_
#define KFD_DEVICE_QUEUE_MANAGER_H_
#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/sched/mm.h>
#include "kfd_priv.h"
#include "kfd_mqd_manager.h"
#define VMID_NUM 16
#define KFD_MES_PROCESS_QUANTUM		100000
#define KFD_MES_GANG_QUANTUM		10000
#define USE_DEFAULT_GRACE_PERIOD 0xffffffff
struct device_process_node {
	struct qcm_process_device *qpd;
	struct list_head list;
};
union SQ_CMD_BITS {
	struct {
		uint32_t cmd:3;
		uint32_t:1;
		uint32_t mode:3;
		uint32_t check_vmid:1;
		uint32_t trap_id:3;
		uint32_t:5;
		uint32_t wave_id:4;
		uint32_t simd_id:2;
		uint32_t:2;
		uint32_t queue_id:3;
		uint32_t:1;
		uint32_t vm_id:4;
	} bitfields, bits;
	uint32_t u32All;
	signed int i32All;
	float f32All;
};
union GRBM_GFX_INDEX_BITS {
	struct {
		uint32_t instance_index:8;
		uint32_t sh_index:8;
		uint32_t se_index:8;
		uint32_t:5;
		uint32_t sh_broadcast_writes:1;
		uint32_t instance_broadcast_writes:1;
		uint32_t se_broadcast_writes:1;
	} bitfields, bits;
	uint32_t u32All;
	signed int i32All;
	float f32All;
};
struct device_queue_manager_ops {
	int	(*create_queue)(struct device_queue_manager *dqm,
				struct queue *q,
				struct qcm_process_device *qpd,
				const struct kfd_criu_queue_priv_data *qd,
				const void *restore_mqd,
				const void *restore_ctl_stack);
	int	(*destroy_queue)(struct device_queue_manager *dqm,
				struct qcm_process_device *qpd,
				struct queue *q);
	int	(*update_queue)(struct device_queue_manager *dqm,
				struct queue *q, struct mqd_update_info *minfo);
	int	(*register_process)(struct device_queue_manager *dqm,
					struct qcm_process_device *qpd);
	int	(*unregister_process)(struct device_queue_manager *dqm,
					struct qcm_process_device *qpd);
	int	(*initialize)(struct device_queue_manager *dqm);
	int	(*start)(struct device_queue_manager *dqm);
	int	(*stop)(struct device_queue_manager *dqm);
	void	(*pre_reset)(struct device_queue_manager *dqm);
	void	(*uninitialize)(struct device_queue_manager *dqm);
	int	(*create_kernel_queue)(struct device_queue_manager *dqm,
					struct kernel_queue *kq,
					struct qcm_process_device *qpd);
	void	(*destroy_kernel_queue)(struct device_queue_manager *dqm,
					struct kernel_queue *kq,
					struct qcm_process_device *qpd);
	bool	(*set_cache_memory_policy)(struct device_queue_manager *dqm,
					   struct qcm_process_device *qpd,
					   enum cache_policy default_policy,
					   enum cache_policy alternate_policy,
					   void __user *alternate_aperture_base,
					   uint64_t alternate_aperture_size);
	int (*process_termination)(struct device_queue_manager *dqm,
			struct qcm_process_device *qpd);
	int (*evict_process_queues)(struct device_queue_manager *dqm,
				    struct qcm_process_device *qpd);
	int (*restore_process_queues)(struct device_queue_manager *dqm,
				      struct qcm_process_device *qpd);
	int	(*get_wave_state)(struct device_queue_manager *dqm,
				  struct queue *q,
				  void __user *ctl_stack,
				  u32 *ctl_stack_used_size,
				  u32 *save_area_used_size);
	int (*reset_queues)(struct device_queue_manager *dqm,
					uint16_t pasid);
	void	(*get_queue_checkpoint_info)(struct device_queue_manager *dqm,
				  const struct queue *q, u32 *mqd_size,
				  u32 *ctl_stack_size);
	int	(*checkpoint_mqd)(struct device_queue_manager *dqm,
				  const struct queue *q,
				  void *mqd,
				  void *ctl_stack);
};
struct device_queue_manager_asic_ops {
	int	(*update_qpd)(struct device_queue_manager *dqm,
					struct qcm_process_device *qpd);
	bool	(*set_cache_memory_policy)(struct device_queue_manager *dqm,
					   struct qcm_process_device *qpd,
					   enum cache_policy default_policy,
					   enum cache_policy alternate_policy,
					   void __user *alternate_aperture_base,
					   uint64_t alternate_aperture_size);
	void	(*init_sdma_vm)(struct device_queue_manager *dqm,
				struct queue *q,
				struct qcm_process_device *qpd);
	struct mqd_manager *	(*mqd_manager_init)(enum KFD_MQD_TYPE type,
				 struct kfd_node *dev);
};
struct device_queue_manager {
	struct device_queue_manager_ops ops;
	struct device_queue_manager_asic_ops asic_ops;
	struct mqd_manager	*mqd_mgrs[KFD_MQD_TYPE_MAX];
	struct packet_manager	packet_mgr;
	struct kfd_node		*dev;
	struct mutex		lock_hidden;  
	struct list_head	queues;
	unsigned int		saved_flags;
	unsigned int		processes_count;
	unsigned int		active_queue_count;
	unsigned int		active_cp_queue_count;
	unsigned int		gws_queue_count;
	unsigned int		total_queue_count;
	unsigned int		next_pipe_to_allocate;
	unsigned int		*allocated_queues;
	DECLARE_BITMAP(sdma_bitmap, KFD_MAX_SDMA_QUEUES);
	DECLARE_BITMAP(xgmi_sdma_bitmap, KFD_MAX_SDMA_QUEUES);
	uint16_t		vmid_pasid[VMID_NUM];
	uint64_t		pipelines_addr;
	uint64_t		fence_gpu_addr;
	uint64_t		*fence_addr;
	struct kfd_mem_obj	*fence_mem;
	bool			active_runlist;
	int			sched_policy;
	uint32_t		trap_debug_vmid;
	bool			is_hws_hang;
	bool			is_resetting;
	struct work_struct	hw_exception_work;
	struct kfd_mem_obj	hiq_sdma_mqd;
	bool			sched_running;
	uint32_t		current_logical_xcc_start;
	uint32_t		wait_times;
	wait_queue_head_t	destroy_wait;
};
void device_queue_manager_init_cik(
		struct device_queue_manager_asic_ops *asic_ops);
void device_queue_manager_init_vi(
		struct device_queue_manager_asic_ops *asic_ops);
void device_queue_manager_init_v9(
		struct device_queue_manager_asic_ops *asic_ops);
void device_queue_manager_init_v10(
		struct device_queue_manager_asic_ops *asic_ops);
void device_queue_manager_init_v11(
		struct device_queue_manager_asic_ops *asic_ops);
void program_sh_mem_settings(struct device_queue_manager *dqm,
					struct qcm_process_device *qpd);
unsigned int get_cp_queues_num(struct device_queue_manager *dqm);
unsigned int get_queues_per_pipe(struct device_queue_manager *dqm);
unsigned int get_pipes_per_mec(struct device_queue_manager *dqm);
unsigned int get_num_sdma_queues(struct device_queue_manager *dqm);
unsigned int get_num_xgmi_sdma_queues(struct device_queue_manager *dqm);
int reserve_debug_trap_vmid(struct device_queue_manager *dqm,
			struct qcm_process_device *qpd);
int release_debug_trap_vmid(struct device_queue_manager *dqm,
			struct qcm_process_device *qpd);
int suspend_queues(struct kfd_process *p,
			uint32_t num_queues,
			uint32_t grace_period,
			uint64_t exception_clear_mask,
			uint32_t *usr_queue_id_array);
int resume_queues(struct kfd_process *p,
		uint32_t num_queues,
		uint32_t *usr_queue_id_array);
void set_queue_snapshot_entry(struct queue *q,
			      uint64_t exception_clear_mask,
			      struct kfd_queue_snapshot_entry *qss_entry);
int debug_lock_and_unmap(struct device_queue_manager *dqm);
int debug_map_and_unlock(struct device_queue_manager *dqm);
int debug_refresh_runlist(struct device_queue_manager *dqm);
static inline unsigned int get_sh_mem_bases_32(struct kfd_process_device *pdd)
{
	return (pdd->lds_base >> 16) & 0xFF;
}
static inline unsigned int
get_sh_mem_bases_nybble_64(struct kfd_process_device *pdd)
{
	return (pdd->lds_base >> 60) & 0x0E;
}
static inline void dqm_lock(struct device_queue_manager *dqm)
{
	mutex_lock(&dqm->lock_hidden);
	dqm->saved_flags = memalloc_noreclaim_save();
}
static inline void dqm_unlock(struct device_queue_manager *dqm)
{
	memalloc_noreclaim_restore(dqm->saved_flags);
	mutex_unlock(&dqm->lock_hidden);
}
static inline int read_sdma_queue_counter(uint64_t __user *q_rptr, uint64_t *val)
{
	return get_user(*val, q_rptr + 1);
}
#endif  
