#ifndef __CXGB4_SMT_H
#define __CXGB4_SMT_H
#include <linux/spinlock.h>
#include <linux/if_ether.h>
#include <linux/atomic.h>
struct adapter;
struct cpl_smt_write_rpl;
enum {
	SMT_STATE_SWITCHING,
	SMT_STATE_UNUSED,
	SMT_STATE_ERROR
};
enum {
	SMT_SIZE = 256
};
struct smt_entry {
	u16 state;
	u16 idx;
	u16 pfvf;
	u8 src_mac[ETH_ALEN];
	int refcnt;
	spinlock_t lock;	 
};
struct smt_data {
	unsigned int smt_size;
	rwlock_t lock;
	struct smt_entry smtab[];
};
struct smt_data *t4_init_smt(void);
struct smt_entry *cxgb4_smt_alloc_switching(struct net_device *dev, u8 *smac);
void cxgb4_smt_release(struct smt_entry *e);
void do_smt_write_rpl(struct adapter *p, const struct cpl_smt_write_rpl *rpl);
#endif  
