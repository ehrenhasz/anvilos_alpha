#ifndef __CSIO_INIT_H__
#define __CSIO_INIT_H__
#include <linux/pci.h>
#include <linux/if_ether.h>
#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include "csio_scsi.h"
#include "csio_lnode.h"
#include "csio_rnode.h"
#include "csio_hw.h"
#define CSIO_DRV_AUTHOR			"Chelsio Communications"
#define CSIO_DRV_DESC			"Chelsio FCoE driver"
#define CSIO_DRV_VERSION		"1.0.0-ko"
extern struct fc_function_template csio_fc_transport_funcs;
extern struct fc_function_template csio_fc_transport_vport_funcs;
void csio_fchost_attr_init(struct csio_lnode *);
void csio_scsi_intx_handler(struct csio_hw *, void *, uint32_t,
			       struct csio_fl_dma_buf *, void *);
void csio_fwevt_intx_handler(struct csio_hw *, void *, uint32_t,
				struct csio_fl_dma_buf *, void *);
void csio_lnodes_block_request(struct csio_hw *);
void csio_lnodes_unblock_request(struct csio_hw *);
void csio_lnodes_block_by_port(struct csio_hw *, uint8_t);
void csio_lnodes_unblock_by_port(struct csio_hw *, uint8_t);
struct csio_lnode *csio_shost_init(struct csio_hw *, struct device *, bool,
					struct csio_lnode *);
void csio_shost_exit(struct csio_lnode *);
void csio_lnodes_exit(struct csio_hw *, bool);
void csio_add_debugfs_mem(struct csio_hw *, const char *,
		unsigned int, unsigned int);
static inline struct Scsi_Host *
csio_ln_to_shost(struct csio_lnode *ln)
{
	return container_of((void *)ln, struct Scsi_Host, hostdata[0]);
}
static inline struct csio_ioreq *
csio_get_scsi_ioreq_lock(struct csio_hw *hw, struct csio_scsim *scsim)
{
	struct csio_ioreq *ioreq;
	unsigned long flags;
	spin_lock_irqsave(&scsim->freelist_lock, flags);
	ioreq = csio_get_scsi_ioreq(scsim);
	spin_unlock_irqrestore(&scsim->freelist_lock, flags);
	return ioreq;
}
static inline void
csio_put_scsi_ioreq_lock(struct csio_hw *hw, struct csio_scsim *scsim,
			 struct csio_ioreq *ioreq)
{
	unsigned long flags;
	spin_lock_irqsave(&scsim->freelist_lock, flags);
	csio_put_scsi_ioreq(scsim, ioreq);
	spin_unlock_irqrestore(&scsim->freelist_lock, flags);
}
static inline void
csio_put_scsi_ioreq_list_lock(struct csio_hw *hw, struct csio_scsim *scsim,
			      struct list_head *reqlist, int n)
{
	unsigned long flags;
	spin_lock_irqsave(&scsim->freelist_lock, flags);
	csio_put_scsi_ioreq_list(scsim, reqlist, n);
	spin_unlock_irqrestore(&scsim->freelist_lock, flags);
}
static inline void
csio_put_scsi_ddp_list_lock(struct csio_hw *hw, struct csio_scsim *scsim,
			      struct list_head *reqlist, int n)
{
	unsigned long flags;
	spin_lock_irqsave(&hw->lock, flags);
	csio_put_scsi_ddp_list(scsim, reqlist, n);
	spin_unlock_irqrestore(&hw->lock, flags);
}
#endif  
