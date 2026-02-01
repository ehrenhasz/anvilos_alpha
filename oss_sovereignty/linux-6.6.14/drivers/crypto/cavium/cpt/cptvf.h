 
 

#ifndef __CPTVF_H
#define __CPTVF_H

#include <linux/list.h>
#include "cpt_common.h"

 
#define CPT_CMD_QLEN 2046
#define CPT_CMD_QCHUNK_SIZE 1023

 
#define CPT_COMMAND_TIMEOUT 4
#define CPT_TIMER_THOLD	0xFFFF
#define CPT_NUM_QS_PER_VF 1
#define CPT_INST_SIZE 64
#define CPT_NEXT_CHUNK_PTR_SIZE 8

#define	CPT_VF_MSIX_VECTORS 2
#define CPT_VF_INTR_MBOX_MASK BIT(0)
#define CPT_VF_INTR_DOVF_MASK BIT(1)
#define CPT_VF_INTR_IRDE_MASK BIT(2)
#define CPT_VF_INTR_NWRP_MASK BIT(3)
#define CPT_VF_INTR_SERR_MASK BIT(4)
#define DMA_DIRECT_DIRECT 0  
#define DMA_GATHER_SCATTER 1
#define FROM_DPTR 1

 
enum cpt_vf_int_vec_e {
	CPT_VF_INT_VEC_E_MISC = 0x00,
	CPT_VF_INT_VEC_E_DONE = 0x01
};

struct command_chunk {
	u8 *head;
	dma_addr_t dma_addr;
	u32 size;  
	struct hlist_node nextchunk;
};

struct command_queue {
	spinlock_t lock;  
	u32 idx;  
	u32 nchunks;  
	struct command_chunk *qhead;	 
	struct hlist_head chead;
};

struct command_qinfo {
	u32 cmd_size;
	u32 qchunksize;  
	struct command_queue queue[CPT_NUM_QS_PER_VF];
};

struct pending_entry {
	u8 busy;  

	volatile u64 *completion_addr;  
	void *post_arg;
	void (*callback)(int, void *);  
	void *callback_arg;  
};

struct pending_queue {
	struct pending_entry *head;	 
	u32 front;  
	u32 rear;  
	atomic64_t pending_count;
	spinlock_t lock;  
};

struct pending_qinfo {
	u32 nr_queues;	 
	u32 qlen;  
	struct pending_queue queue[CPT_NUM_QS_PER_VF];
};

#define for_each_pending_queue(qinfo, q, i)	\
	for (i = 0, q = &qinfo->queue[i]; i < qinfo->nr_queues; i++, \
	     q = &qinfo->queue[i])

struct cpt_vf {
	u16 flags;  
	u8 vfid;  
	u8 vftype;  
	u8 vfgrp;  
	u8 node;  
	u8 priority;  
	struct pci_dev *pdev;  
	void __iomem *reg_base;  
	void *wqe_info;	 
	 
	cpumask_var_t affinity_mask[CPT_VF_MSIX_VECTORS];
	 
	u32 qsize;
	u32 nr_queues;
	struct command_qinfo cqinfo;  
	struct pending_qinfo pqinfo;  
	 
	bool pf_acked;
	bool pf_nacked;
};

int cptvf_send_vf_up(struct cpt_vf *cptvf);
int cptvf_send_vf_down(struct cpt_vf *cptvf);
int cptvf_send_vf_to_grp_msg(struct cpt_vf *cptvf);
int cptvf_send_vf_priority_msg(struct cpt_vf *cptvf);
int cptvf_send_vq_size_msg(struct cpt_vf *cptvf);
int cptvf_check_pf_ready(struct cpt_vf *cptvf);
void cptvf_handle_mbox_intr(struct cpt_vf *cptvf);
void cvm_crypto_exit(void);
int cvm_crypto_init(struct cpt_vf *cptvf);
void vq_post_process(struct cpt_vf *cptvf, u32 qno);
void cptvf_write_vq_doorbell(struct cpt_vf *cptvf, u32 val);
#endif  
