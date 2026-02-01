 
 

#ifndef KFD_MQD_MANAGER_H_
#define KFD_MQD_MANAGER_H_

#include "kfd_priv.h"

#define KFD_MAX_NUM_SE 8
#define KFD_MAX_NUM_SH_PER_SE 2

 
extern int pipe_priority_map[];
struct mqd_manager {
	struct kfd_mem_obj*	(*allocate_mqd)(struct kfd_node *kfd,
		struct queue_properties *q);

	void	(*init_mqd)(struct mqd_manager *mm, void **mqd,
			struct kfd_mem_obj *mqd_mem_obj, uint64_t *gart_addr,
			struct queue_properties *q);

	int	(*load_mqd)(struct mqd_manager *mm, void *mqd,
				uint32_t pipe_id, uint32_t queue_id,
				struct queue_properties *p,
				struct mm_struct *mms);

	void	(*update_mqd)(struct mqd_manager *mm, void *mqd,
				struct queue_properties *q,
				struct mqd_update_info *minfo);

	int	(*destroy_mqd)(struct mqd_manager *mm, void *mqd,
				enum kfd_preempt_type type,
				unsigned int timeout, uint32_t pipe_id,
				uint32_t queue_id);

	void	(*free_mqd)(struct mqd_manager *mm, void *mqd,
				struct kfd_mem_obj *mqd_mem_obj);

	bool	(*is_occupied)(struct mqd_manager *mm, void *mqd,
				uint64_t queue_address,	uint32_t pipe_id,
				uint32_t queue_id);

	int	(*get_wave_state)(struct mqd_manager *mm, void *mqd,
				  struct queue_properties *q,
				  void __user *ctl_stack,
				  u32 *ctl_stack_used_size,
				  u32 *save_area_used_size);

	void	(*get_checkpoint_info)(struct mqd_manager *mm, void *mqd, uint32_t *ctl_stack_size);

	void	(*checkpoint_mqd)(struct mqd_manager *mm,
				  void *mqd,
				  void *mqd_dst,
				  void *ctl_stack_dst);

	void	(*restore_mqd)(struct mqd_manager *mm, void **mqd,
				struct kfd_mem_obj *mqd_mem_obj, uint64_t *gart_addr,
				struct queue_properties *p,
				const void *mqd_src,
				const void *ctl_stack_src,
				const u32 ctl_stack_size);

#if defined(CONFIG_DEBUG_FS)
	int	(*debugfs_show_mqd)(struct seq_file *m, void *data);
#endif
	uint32_t (*read_doorbell_id)(void *mqd);
	uint64_t (*mqd_stride)(struct mqd_manager *mm,
				struct queue_properties *p);

	struct mutex	mqd_mutex;
	struct kfd_node	*dev;
	uint32_t mqd_size;
};

struct kfd_mem_obj *allocate_hiq_mqd(struct kfd_node *dev,
				struct queue_properties *q);

struct kfd_mem_obj *allocate_sdma_mqd(struct kfd_node *dev,
					struct queue_properties *q);
void free_mqd_hiq_sdma(struct mqd_manager *mm, void *mqd,
				struct kfd_mem_obj *mqd_mem_obj);

void mqd_symmetrically_map_cu_mask(struct mqd_manager *mm,
		const uint32_t *cu_mask, uint32_t cu_mask_count,
		uint32_t *se_mask, uint32_t inst);

int kfd_hiq_load_mqd_kiq(struct mqd_manager *mm, void *mqd,
		uint32_t pipe_id, uint32_t queue_id,
		struct queue_properties *p, struct mm_struct *mms);

int kfd_destroy_mqd_cp(struct mqd_manager *mm, void *mqd,
		enum kfd_preempt_type type, unsigned int timeout,
		uint32_t pipe_id, uint32_t queue_id);

void kfd_free_mqd_cp(struct mqd_manager *mm, void *mqd,
		struct kfd_mem_obj *mqd_mem_obj);

bool kfd_is_occupied_cp(struct mqd_manager *mm, void *mqd,
		 uint64_t queue_address, uint32_t pipe_id,
		 uint32_t queue_id);

int kfd_load_mqd_sdma(struct mqd_manager *mm, void *mqd,
		uint32_t pipe_id, uint32_t queue_id,
		struct queue_properties *p, struct mm_struct *mms);

int kfd_destroy_mqd_sdma(struct mqd_manager *mm, void *mqd,
		enum kfd_preempt_type type, unsigned int timeout,
		uint32_t pipe_id, uint32_t queue_id);

bool kfd_is_occupied_sdma(struct mqd_manager *mm, void *mqd,
		uint64_t queue_address, uint32_t pipe_id,
		uint32_t queue_id);

void kfd_get_hiq_xcc_mqd(struct kfd_node *dev,
		struct kfd_mem_obj *mqd_mem_obj, uint32_t virtual_xcc_id);

uint64_t kfd_hiq_mqd_stride(struct kfd_node *dev);
uint64_t kfd_mqd_stride(struct mqd_manager *mm,
			struct queue_properties *q);
#endif  
