

#ifndef _FUNDEV_H
#define _FUNDEV_H

#include <linux/sbitmap.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include "fun_hci.h"

struct pci_dev;
struct fun_dev;
struct fun_queue;
struct fun_cmd_ctx;
struct fun_queue_alloc_req;


enum {
	FUN_DB_QIDX_S = 0,
	FUN_DB_INTCOAL_ENTRIES_S = 16,
	FUN_DB_INTCOAL_ENTRIES_M = 0x7f,
	FUN_DB_INTCOAL_USEC_S = 23,
	FUN_DB_INTCOAL_USEC_M = 0x7f,
	FUN_DB_IRQ_S = 30,
	FUN_DB_IRQ_F = 1 << FUN_DB_IRQ_S,
	FUN_DB_IRQ_ARM_S = 31,
	FUN_DB_IRQ_ARM_F = 1U << FUN_DB_IRQ_ARM_S
};


typedef void (*fun_admin_callback_t)(struct fun_dev *fdev, void *rsp,
				     void *cb_data);


typedef void (*fun_admin_event_cb)(struct fun_dev *fdev, void *cqe);


typedef void (*fun_serv_cb)(struct fun_dev *fd);


enum {
	FUN_SERV_DISABLED,    
	FUN_SERV_FIRST_AVAIL
};


struct fun_dev {
	struct device *dev;

	void __iomem *bar;            
	u32 __iomem *dbs;             

	
	struct fun_queue *admin_q;
	struct sbitmap_queue admin_sbq;
	struct fun_cmd_ctx *cmd_ctx;
	fun_admin_event_cb adminq_cb;
	bool suppress_cmds;           

	
	unsigned int db_stride;

	
	u32 cc_reg;         
	u64 cap_reg;        

	unsigned int q_depth;    
	unsigned int max_qid;    
	unsigned int kern_end_qid; 

	unsigned int fw_handle;

	
	unsigned int num_irqs;
	unsigned int irqs_avail;
	spinlock_t irqmgr_lock;
	unsigned long *irq_map;

	
	struct work_struct service_task;
	unsigned long service_flags;
	fun_serv_cb serv_cb;
};

struct fun_dev_params {
	u8  cqe_size_log2; 
	u8  sqe_size_log2; 

	
	u16 cq_depth;
	u16 sq_depth;
	u16 rq_depth;

	u16 min_msix; 

	fun_admin_event_cb event_cb;
	fun_serv_cb serv_cb;
};


static inline u32 __iomem *fun_db_addr(const struct fun_dev *fdev,
				       unsigned int db_index)
{
	return &fdev->dbs[db_index * fdev->db_stride];
}


static inline u32 __iomem *fun_sq_db_addr(const struct fun_dev *fdev,
					  unsigned int sqid)
{
	return fun_db_addr(fdev, sqid * 2);
}

static inline u32 __iomem *fun_cq_db_addr(const struct fun_dev *fdev,
					  unsigned int cqid)
{
	return fun_db_addr(fdev, cqid * 2 + 1);
}

int fun_get_res_count(struct fun_dev *fdev, enum fun_admin_op res);
int fun_res_destroy(struct fun_dev *fdev, enum fun_admin_op res,
		    unsigned int flags, u32 id);
int fun_bind(struct fun_dev *fdev, enum fun_admin_bind_type type0,
	     unsigned int id0, enum fun_admin_bind_type type1,
	     unsigned int id1);

int fun_submit_admin_cmd(struct fun_dev *fdev, struct fun_admin_req_common *cmd,
			 fun_admin_callback_t cb, void *cb_data, bool wait_ok);
int fun_submit_admin_sync_cmd(struct fun_dev *fdev,
			      struct fun_admin_req_common *cmd, void *rsp,
			      size_t rspsize, unsigned int timeout);

int fun_dev_enable(struct fun_dev *fdev, struct pci_dev *pdev,
		   const struct fun_dev_params *areq, const char *name);
void fun_dev_disable(struct fun_dev *fdev);

int fun_reserve_irqs(struct fun_dev *fdev, unsigned int nirqs,
		     u16 *irq_indices);
void fun_release_irqs(struct fun_dev *fdev, unsigned int nirqs,
		      u16 *irq_indices);

void fun_serv_stop(struct fun_dev *fd);
void fun_serv_restart(struct fun_dev *fd);
void fun_serv_sched(struct fun_dev *fd);

#endif 
