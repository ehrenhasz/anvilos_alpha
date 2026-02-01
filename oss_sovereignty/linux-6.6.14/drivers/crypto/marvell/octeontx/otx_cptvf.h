 

#ifndef __OTX_CPTVF_H
#define __OTX_CPTVF_H

#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include "otx_cpt_common.h"
#include "otx_cptvf_reqmgr.h"

 
#define OTX_CPT_FLAG_DEVICE_READY  BIT(1)
#define otx_cpt_device_ready(cpt)  ((cpt)->flags & OTX_CPT_FLAG_DEVICE_READY)
 
#define OTX_CPT_CMD_QLEN	(4*2046)
#define OTX_CPT_CMD_QCHUNK_SIZE	1023
#define OTX_CPT_NUM_QS_PER_VF	1

struct otx_cpt_cmd_chunk {
	u8 *head;
	dma_addr_t dma_addr;
	u32 size;  
	struct list_head nextchunk;
};

struct otx_cpt_cmd_queue {
	u32 idx;	 
	u32 num_chunks;	 
	struct otx_cpt_cmd_chunk *qhead; 
	struct otx_cpt_cmd_chunk *base;
	struct list_head chead;
};

struct otx_cpt_cmd_qinfo {
	u32 qchunksize;  
	struct otx_cpt_cmd_queue queue[OTX_CPT_NUM_QS_PER_VF];
};

struct otx_cpt_pending_qinfo {
	u32 num_queues;	 
	struct otx_cpt_pending_queue queue[OTX_CPT_NUM_QS_PER_VF];
};

#define for_each_pending_queue(qinfo, q, i)	\
		for (i = 0, q = &qinfo->queue[i]; i < qinfo->num_queues; i++, \
		     q = &qinfo->queue[i])

struct otx_cptvf_wqe {
	struct tasklet_struct twork;
	struct otx_cptvf *cptvf;
};

struct otx_cptvf_wqe_info {
	struct otx_cptvf_wqe vq_wqe[OTX_CPT_NUM_QS_PER_VF];
};

struct otx_cptvf {
	u16 flags;	 
	u8 vfid;	 
	u8 num_vfs;	 
	u8 vftype;	 
	u8 vfgrp;	 
	u8 node;	 
	u8 priority;	 
	struct pci_dev *pdev;	 
	void __iomem *reg_base;	 
	void *wqe_info;		 
	 
	cpumask_var_t affinity_mask[OTX_CPT_VF_MSIX_VECTORS];
	 
	u32 qsize;
	u32 num_queues;
	struct otx_cpt_cmd_qinfo cqinfo;  
	struct otx_cpt_pending_qinfo pqinfo;  
	 
	bool pf_acked;
	bool pf_nacked;
};

int otx_cptvf_send_vf_up(struct otx_cptvf *cptvf);
int otx_cptvf_send_vf_down(struct otx_cptvf *cptvf);
int otx_cptvf_send_vf_to_grp_msg(struct otx_cptvf *cptvf, int group);
int otx_cptvf_send_vf_priority_msg(struct otx_cptvf *cptvf);
int otx_cptvf_send_vq_size_msg(struct otx_cptvf *cptvf);
int otx_cptvf_check_pf_ready(struct otx_cptvf *cptvf);
void otx_cptvf_handle_mbox_intr(struct otx_cptvf *cptvf);
void otx_cptvf_write_vq_doorbell(struct otx_cptvf *cptvf, u32 val);

#endif  
