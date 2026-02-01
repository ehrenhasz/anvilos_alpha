 
 

#ifndef KFD_KERNEL_QUEUE_H_
#define KFD_KERNEL_QUEUE_H_

#include <linux/list.h>
#include <linux/types.h>
#include "kfd_priv.h"

 

int kq_acquire_packet_buffer(struct kernel_queue *kq,
				size_t packet_size_in_dwords,
				unsigned int **buffer_ptr);
void kq_submit_packet(struct kernel_queue *kq);
void kq_rollback_packet(struct kernel_queue *kq);


struct kernel_queue {
	 
	struct kfd_node		*dev;
	struct mqd_manager	*mqd_mgr;
	struct queue		*queue;
	uint64_t		pending_wptr64;
	uint32_t		pending_wptr;
	unsigned int		nop_packet;

	struct kfd_mem_obj	*rptr_mem;
	uint32_t		*rptr_kernel;
	uint64_t		rptr_gpu_addr;
	struct kfd_mem_obj	*wptr_mem;
	union {
		uint64_t	*wptr64_kernel;
		uint32_t	*wptr_kernel;
	};
	uint64_t		wptr_gpu_addr;
	struct kfd_mem_obj	*pq;
	uint64_t		pq_gpu_addr;
	uint32_t		*pq_kernel_addr;
	struct kfd_mem_obj	*eop_mem;
	uint64_t		eop_gpu_addr;
	uint32_t		*eop_kernel_addr;

	struct kfd_mem_obj	*fence_mem_obj;
	uint64_t		fence_gpu_addr;
	void			*fence_kernel_address;

	struct list_head	list;
};

#endif  
