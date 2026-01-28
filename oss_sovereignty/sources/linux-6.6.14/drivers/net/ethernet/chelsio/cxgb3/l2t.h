
#ifndef _CHELSIO_L2T_H
#define _CHELSIO_L2T_H

#include <linux/spinlock.h>
#include "t3cdev.h"
#include <linux/atomic.h>

enum {
	L2T_STATE_VALID,	
	L2T_STATE_STALE,	
	L2T_STATE_RESOLVING,	
	L2T_STATE_UNUSED	
};

struct neighbour;
struct sk_buff;


struct l2t_entry {
	u16 state;		
	u16 idx;		
	u32 addr;		
	int ifindex;		
	u16 smt_idx;		
	u16 vlan;		
	struct neighbour *neigh;	
	struct l2t_entry *first;	
	struct l2t_entry *next;	
	struct sk_buff_head arpq;	
	spinlock_t lock;
	atomic_t refcnt;	
	u8 dmac[6];		
};

struct l2t_data {
	unsigned int nentries;	
	struct l2t_entry *rover;	
	atomic_t nfree;		
	rwlock_t lock;
	struct rcu_head rcu_head;	
	struct l2t_entry l2tab[];
};

typedef void (*arp_failure_handler_func)(struct t3cdev * dev,
					 struct sk_buff * skb);


struct l2t_skb_cb {
	arp_failure_handler_func arp_failure_handler;
};

#define L2T_SKB_CB(skb) ((struct l2t_skb_cb *)(skb)->cb)

static inline void set_arp_failure_handler(struct sk_buff *skb,
					   arp_failure_handler_func hnd)
{
	L2T_SKB_CB(skb)->arp_failure_handler = hnd;
}


#define L2DATA(cdev) (rcu_dereference((cdev)->l2opt))

#define W_TCB_L2T_IX    0
#define S_TCB_L2T_IX    7
#define M_TCB_L2T_IX    0x7ffULL
#define V_TCB_L2T_IX(x) ((x) << S_TCB_L2T_IX)

void t3_l2e_free(struct l2t_data *d, struct l2t_entry *e);
void t3_l2t_update(struct t3cdev *dev, struct neighbour *neigh);
struct l2t_entry *t3_l2t_get(struct t3cdev *cdev, struct dst_entry *dst,
			     struct net_device *dev, const void *daddr);
int t3_l2t_send_slow(struct t3cdev *dev, struct sk_buff *skb,
		     struct l2t_entry *e);
void t3_l2t_send_event(struct t3cdev *dev, struct l2t_entry *e);
struct l2t_data *t3_init_l2t(unsigned int l2t_capacity);

int cxgb3_ofld_send(struct t3cdev *dev, struct sk_buff *skb);

static inline int l2t_send(struct t3cdev *dev, struct sk_buff *skb,
			   struct l2t_entry *e)
{
	if (likely(e->state == L2T_STATE_VALID))
		return cxgb3_ofld_send(dev, skb);
	return t3_l2t_send_slow(dev, skb, e);
}

static inline void l2t_release(struct t3cdev *t, struct l2t_entry *e)
{
	struct l2t_data *d;

	rcu_read_lock();
	d = L2DATA(t);

	if (atomic_dec_and_test(&e->refcnt) && d)
		t3_l2e_free(d, e);

	rcu_read_unlock();
}

static inline void l2t_hold(struct l2t_data *d, struct l2t_entry *e)
{
	if (d && atomic_add_return(1, &e->refcnt) == 1)	
		atomic_dec(&d->nfree);
}

#endif
