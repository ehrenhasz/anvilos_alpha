 

#ifndef __CXGB4_SCHED_H
#define __CXGB4_SCHED_H

#include <linux/spinlock.h>
#include <linux/atomic.h>

#define SCHED_CLS_NONE 0xff

#define FW_SCHED_CLS_NONE 0xffffffff

 
#define SCHED_MAX_RATE_KBPS 100000000U

enum {
	SCHED_STATE_ACTIVE,
	SCHED_STATE_UNUSED,
};

enum sched_fw_ops {
	SCHED_FW_OP_ADD,
	SCHED_FW_OP_DEL,
};

enum sched_bind_type {
	SCHED_QUEUE,
	SCHED_FLOWC,
};

struct sched_queue_entry {
	struct list_head list;
	unsigned int cntxt_id;
	struct ch_sched_queue param;
};

struct sched_flowc_entry {
	struct list_head list;
	struct ch_sched_flowc param;
};

struct sched_class {
	u8 state;
	u8 idx;
	struct ch_sched_params info;
	enum sched_bind_type bind_type;
	struct list_head entry_list;
	atomic_t refcnt;
};

struct sched_table {       
	u8 sched_size;
	struct sched_class tab[];
};

static inline bool can_sched(struct net_device *dev)
{
	struct port_info *pi = netdev2pinfo(dev);

	return !pi->sched_tbl ? false : true;
}

static inline bool valid_class_id(struct net_device *dev, u8 class_id)
{
	struct port_info *pi = netdev2pinfo(dev);

	if ((class_id > pi->sched_tbl->sched_size - 1) &&
	    (class_id != SCHED_CLS_NONE))
		return false;

	return true;
}

struct sched_class *cxgb4_sched_queue_lookup(struct net_device *dev,
					     struct ch_sched_queue *p);
int cxgb4_sched_class_bind(struct net_device *dev, void *arg,
			   enum sched_bind_type type);
int cxgb4_sched_class_unbind(struct net_device *dev, void *arg,
			     enum sched_bind_type type);

struct sched_class *cxgb4_sched_class_alloc(struct net_device *dev,
					    struct ch_sched_params *p);
void cxgb4_sched_class_free(struct net_device *dev, u8 classid);

struct sched_table *t4_init_sched(unsigned int size);
void t4_cleanup_sched(struct adapter *adap);
#endif   
