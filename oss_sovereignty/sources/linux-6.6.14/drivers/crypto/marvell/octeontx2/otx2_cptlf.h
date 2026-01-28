
#ifndef __OTX2_CPTLF_H
#define __OTX2_CPTLF_H

#include <linux/soc/marvell/octeontx2/asm.h>
#include <mbox.h>
#include <rvu.h>
#include "otx2_cpt_common.h"
#include "otx2_cpt_reqmgr.h"


#define OTX2_CPT_USER_REQUESTED_QLEN_MSGS 8200


#define OTX2_CPT_SIZE_DIV40 (OTX2_CPT_USER_REQUESTED_QLEN_MSGS/40)


#define OTX2_CPT_INST_QLEN_MSGS	((OTX2_CPT_SIZE_DIV40 - 1) * 40)


#define OTX2_CPT_INST_QLEN_EXTRA_BYTES  (320 * OTX2_CPT_INST_SIZE)
#define OTX2_CPT_EXTRA_SIZE_DIV40       (320/40)


#define OTX2_CPT_INST_QLEN_BYTES                                               \
		((OTX2_CPT_SIZE_DIV40 * 40 * OTX2_CPT_INST_SIZE) +             \
		OTX2_CPT_INST_QLEN_EXTRA_BYTES)


#define OTX2_CPT_INST_GRP_QLEN_BYTES                                           \
		((OTX2_CPT_SIZE_DIV40 + OTX2_CPT_EXTRA_SIZE_DIV40) * 16)


#define OTX2_CPT_Q_FC_LEN 128


#define OTX2_CPT_INST_Q_ALIGNMENT  128


#define OTX2_CPT_ALL_ENG_GRPS_MASK 0xFF


#define OTX2_CPT_MAX_LFS_NUM    64


#define OTX2_CPT_QUEUE_HI_PRIO  0x1
#define OTX2_CPT_QUEUE_LOW_PRIO 0x0

enum otx2_cptlf_state {
	OTX2_CPTLF_IN_RESET,
	OTX2_CPTLF_STARTED,
};

struct otx2_cpt_inst_queue {
	u8 *vaddr;
	u8 *real_vaddr;
	dma_addr_t dma_addr;
	dma_addr_t real_dma_addr;
	u32 size;
};

struct otx2_cptlfs_info;
struct otx2_cptlf_wqe {
	struct tasklet_struct work;
	struct otx2_cptlfs_info *lfs;
	u8 lf_num;
};

struct otx2_cptlf_info {
	struct otx2_cptlfs_info *lfs;           
	void __iomem *lmtline;                  
	void __iomem *ioreg;                    
	int msix_offset;                        
	cpumask_var_t affinity_mask;            
	u8 irq_name[OTX2_CPT_LF_MSIX_VECTORS][32];
	u8 is_irq_reg[OTX2_CPT_LF_MSIX_VECTORS];  
	u8 slot;                                

	struct otx2_cpt_inst_queue iqueue;
	struct otx2_cpt_pending_queue pqueue; 
	struct otx2_cptlf_wqe *wqe;       
};

struct cpt_hw_ops {
	void (*send_cmd)(union otx2_cpt_inst_s *cptinst, u32 insts_num,
			 struct otx2_cptlf_info *lf);
	u8 (*cpt_get_compcode)(union otx2_cpt_res_s *result);
	u8 (*cpt_get_uc_compcode)(union otx2_cpt_res_s *result);
};

struct otx2_cptlfs_info {
	
	void __iomem *reg_base;
#define LMTLINE_SIZE  128
	void __iomem *lmt_base;
	struct pci_dev *pdev;   
	struct otx2_cptlf_info lf[OTX2_CPT_MAX_LFS_NUM];
	struct otx2_mbox *mbox;
	struct cpt_hw_ops *ops;
	u8 are_lfs_attached;	
	u8 lfs_num;		
	u8 kcrypto_eng_grp_num;	
	u8 kvf_limits;          
	atomic_t state;         
	int blkaddr;            
};

static inline void otx2_cpt_free_instruction_queues(
					struct otx2_cptlfs_info *lfs)
{
	struct otx2_cpt_inst_queue *iq;
	int i;

	for (i = 0; i < lfs->lfs_num; i++) {
		iq = &lfs->lf[i].iqueue;
		if (iq->real_vaddr)
			dma_free_coherent(&lfs->pdev->dev,
					  iq->size,
					  iq->real_vaddr,
					  iq->real_dma_addr);
		iq->real_vaddr = NULL;
		iq->vaddr = NULL;
	}
}

static inline int otx2_cpt_alloc_instruction_queues(
					struct otx2_cptlfs_info *lfs)
{
	struct otx2_cpt_inst_queue *iq;
	int ret = 0, i;

	if (!lfs->lfs_num)
		return -EINVAL;

	for (i = 0; i < lfs->lfs_num; i++) {
		iq = &lfs->lf[i].iqueue;
		iq->size = OTX2_CPT_INST_QLEN_BYTES +
			   OTX2_CPT_Q_FC_LEN +
			   OTX2_CPT_INST_GRP_QLEN_BYTES +
			   OTX2_CPT_INST_Q_ALIGNMENT;
		iq->real_vaddr = dma_alloc_coherent(&lfs->pdev->dev, iq->size,
					&iq->real_dma_addr, GFP_KERNEL);
		if (!iq->real_vaddr) {
			ret = -ENOMEM;
			goto error;
		}
		iq->vaddr = iq->real_vaddr + OTX2_CPT_INST_GRP_QLEN_BYTES;
		iq->dma_addr = iq->real_dma_addr + OTX2_CPT_INST_GRP_QLEN_BYTES;

		
		iq->vaddr = PTR_ALIGN(iq->vaddr, OTX2_CPT_INST_Q_ALIGNMENT);
		iq->dma_addr = PTR_ALIGN(iq->dma_addr,
					 OTX2_CPT_INST_Q_ALIGNMENT);
	}
	return 0;

error:
	otx2_cpt_free_instruction_queues(lfs);
	return ret;
}

static inline void otx2_cptlf_set_iqueues_base_addr(
					struct otx2_cptlfs_info *lfs)
{
	union otx2_cptx_lf_q_base lf_q_base;
	int slot;

	for (slot = 0; slot < lfs->lfs_num; slot++) {
		lf_q_base.u = lfs->lf[slot].iqueue.dma_addr;
		otx2_cpt_write64(lfs->reg_base, lfs->blkaddr, slot,
				 OTX2_CPT_LF_Q_BASE, lf_q_base.u);
	}
}

static inline void otx2_cptlf_do_set_iqueue_size(struct otx2_cptlf_info *lf)
{
	union otx2_cptx_lf_q_size lf_q_size = { .u = 0x0 };

	lf_q_size.s.size_div40 = OTX2_CPT_SIZE_DIV40 +
				 OTX2_CPT_EXTRA_SIZE_DIV40;
	otx2_cpt_write64(lf->lfs->reg_base, lf->lfs->blkaddr, lf->slot,
			 OTX2_CPT_LF_Q_SIZE, lf_q_size.u);
}

static inline void otx2_cptlf_set_iqueues_size(struct otx2_cptlfs_info *lfs)
{
	int slot;

	for (slot = 0; slot < lfs->lfs_num; slot++)
		otx2_cptlf_do_set_iqueue_size(&lfs->lf[slot]);
}

static inline void otx2_cptlf_do_disable_iqueue(struct otx2_cptlf_info *lf)
{
	union otx2_cptx_lf_ctl lf_ctl = { .u = 0x0 };
	union otx2_cptx_lf_inprog lf_inprog;
	u8 blkaddr = lf->lfs->blkaddr;
	int timeout = 20;

	
	otx2_cpt_write64(lf->lfs->reg_base, blkaddr, lf->slot,
			 OTX2_CPT_LF_CTL, lf_ctl.u);

	
	do {
		lf_inprog.u = otx2_cpt_read64(lf->lfs->reg_base, blkaddr,
					      lf->slot, OTX2_CPT_LF_INPROG);
		if (!lf_inprog.s.inflight)
			break;

		usleep_range(10000, 20000);
		if (timeout-- < 0) {
			dev_err(&lf->lfs->pdev->dev,
				"Error LF %d is still busy.\n", lf->slot);
			break;
		}

	} while (1);

	
	lf_inprog.s.eena = 0x0;
	otx2_cpt_write64(lf->lfs->reg_base, blkaddr, lf->slot,
			 OTX2_CPT_LF_INPROG, lf_inprog.u);
}

static inline void otx2_cptlf_disable_iqueues(struct otx2_cptlfs_info *lfs)
{
	int slot;

	for (slot = 0; slot < lfs->lfs_num; slot++)
		otx2_cptlf_do_disable_iqueue(&lfs->lf[slot]);
}

static inline void otx2_cptlf_set_iqueue_enq(struct otx2_cptlf_info *lf,
					     bool enable)
{
	u8 blkaddr = lf->lfs->blkaddr;
	union otx2_cptx_lf_ctl lf_ctl;

	lf_ctl.u = otx2_cpt_read64(lf->lfs->reg_base, blkaddr, lf->slot,
				   OTX2_CPT_LF_CTL);

	
	lf_ctl.s.ena = enable ? 0x1 : 0x0;
	otx2_cpt_write64(lf->lfs->reg_base, blkaddr, lf->slot,
			 OTX2_CPT_LF_CTL, lf_ctl.u);
}

static inline void otx2_cptlf_enable_iqueue_enq(struct otx2_cptlf_info *lf)
{
	otx2_cptlf_set_iqueue_enq(lf, true);
}

static inline void otx2_cptlf_set_iqueue_exec(struct otx2_cptlf_info *lf,
					      bool enable)
{
	union otx2_cptx_lf_inprog lf_inprog;
	u8 blkaddr = lf->lfs->blkaddr;

	lf_inprog.u = otx2_cpt_read64(lf->lfs->reg_base, blkaddr, lf->slot,
				      OTX2_CPT_LF_INPROG);

	
	lf_inprog.s.eena = enable ? 0x1 : 0x0;
	otx2_cpt_write64(lf->lfs->reg_base, blkaddr, lf->slot,
			 OTX2_CPT_LF_INPROG, lf_inprog.u);
}

static inline void otx2_cptlf_enable_iqueue_exec(struct otx2_cptlf_info *lf)
{
	otx2_cptlf_set_iqueue_exec(lf, true);
}

static inline void otx2_cptlf_disable_iqueue_exec(struct otx2_cptlf_info *lf)
{
	otx2_cptlf_set_iqueue_exec(lf, false);
}

static inline void otx2_cptlf_enable_iqueues(struct otx2_cptlfs_info *lfs)
{
	int slot;

	for (slot = 0; slot < lfs->lfs_num; slot++) {
		otx2_cptlf_enable_iqueue_exec(&lfs->lf[slot]);
		otx2_cptlf_enable_iqueue_enq(&lfs->lf[slot]);
	}
}

static inline void otx2_cpt_fill_inst(union otx2_cpt_inst_s *cptinst,
				      struct otx2_cpt_iq_command *iq_cmd,
				      u64 comp_baddr)
{
	cptinst->u[0] = 0x0;
	cptinst->s.doneint = true;
	cptinst->s.res_addr = comp_baddr;
	cptinst->u[2] = 0x0;
	cptinst->u[3] = 0x0;
	cptinst->s.ei0 = iq_cmd->cmd.u;
	cptinst->s.ei1 = iq_cmd->dptr;
	cptinst->s.ei2 = iq_cmd->rptr;
	cptinst->s.ei3 = iq_cmd->cptr.u;
}


static inline void otx2_cpt_send_cmd(union otx2_cpt_inst_s *cptinst,
				     u32 insts_num, struct otx2_cptlf_info *lf)
{
	void __iomem *lmtline = lf->lmtline;
	long ret;

	
	dma_wmb();

	do {
		
		memcpy_toio(lmtline, cptinst, insts_num * OTX2_CPT_INST_SIZE);

		
		ret = otx2_lmt_flush(lf->ioreg);

	} while (!ret);
}

static inline bool otx2_cptlf_started(struct otx2_cptlfs_info *lfs)
{
	return atomic_read(&lfs->state) == OTX2_CPTLF_STARTED;
}

static inline void otx2_cptlf_set_dev_info(struct otx2_cptlfs_info *lfs,
					   struct pci_dev *pdev,
					   void __iomem *reg_base,
					   struct otx2_mbox *mbox,
					   int blkaddr)
{
	lfs->pdev = pdev;
	lfs->reg_base = reg_base;
	lfs->mbox = mbox;
	lfs->blkaddr = blkaddr;
}

int otx2_cptlf_init(struct otx2_cptlfs_info *lfs, u8 eng_grp_msk, int pri,
		    int lfs_num);
void otx2_cptlf_shutdown(struct otx2_cptlfs_info *lfs);
int otx2_cptlf_register_interrupts(struct otx2_cptlfs_info *lfs);
void otx2_cptlf_unregister_interrupts(struct otx2_cptlfs_info *lfs);
void otx2_cptlf_free_irqs_affinity(struct otx2_cptlfs_info *lfs);
int otx2_cptlf_set_irqs_affinity(struct otx2_cptlfs_info *lfs);

#endif 
