#ifndef __CXGB4_L2T_H
#define __CXGB4_L2T_H
#include <linux/spinlock.h>
#include <linux/if_ether.h>
#include <linux/atomic.h>
#define VLAN_NONE 0xfff
enum { L2T_SIZE = 4096 };      
enum {
	L2T_STATE_VALID,       
	L2T_STATE_STALE,       
	L2T_STATE_RESOLVING,   
	L2T_STATE_SYNC_WRITE,  
	L2T_STATE_NOARP,       
	L2T_STATE_SWITCHING,   
	L2T_STATE_UNUSED       
};
struct adapter;
struct l2t_data;
struct neighbour;
struct net_device;
struct file_operations;
struct cpl_l2t_write_rpl;
struct l2t_entry {
	u16 state;                   
	u16 idx;                     
	u32 addr[4];                 
	int ifindex;                 
	struct neighbour *neigh;     
	struct l2t_entry *first;     
	struct l2t_entry *next;      
	struct sk_buff_head arpq;    
	spinlock_t lock;
	atomic_t refcnt;             
	u16 hash;                    
	u16 vlan;                    
	u8 v6;                       
	u8 lport;                    
	u8 dmac[ETH_ALEN];           
};
typedef void (*arp_err_handler_t)(void *handle, struct sk_buff *skb);
struct l2t_skb_cb {
	void *handle;
	arp_err_handler_t arp_err_handler;
};
#define L2T_SKB_CB(skb) ((struct l2t_skb_cb *)(skb)->cb)
static inline void t4_set_arp_err_handler(struct sk_buff *skb, void *handle,
					  arp_err_handler_t handler)
{
	L2T_SKB_CB(skb)->handle = handle;
	L2T_SKB_CB(skb)->arp_err_handler = handler;
}
void cxgb4_l2t_release(struct l2t_entry *e);
int cxgb4_l2t_send(struct net_device *dev, struct sk_buff *skb,
		   struct l2t_entry *e);
struct l2t_entry *cxgb4_l2t_get(struct l2t_data *d, struct neighbour *neigh,
				const struct net_device *physdev,
				unsigned int priority);
u64 cxgb4_select_ntuple(struct net_device *dev,
			const struct l2t_entry *l2t);
struct l2t_entry *cxgb4_l2t_alloc_switching(struct net_device *dev, u16 vlan,
					    u8 port, u8 *dmac);
void t4_l2t_update(struct adapter *adap, struct neighbour *neigh);
struct l2t_entry *t4_l2t_alloc_switching(struct adapter *adap, u16 vlan,
					 u8 port, u8 *dmac);
struct l2t_data *t4_init_l2t(unsigned int l2t_start, unsigned int l2t_end);
void do_l2t_write_rpl(struct adapter *p, const struct cpl_l2t_write_rpl *rpl);
bool cxgb4_check_l2t_valid(struct l2t_entry *e);
extern const struct file_operations t4_l2t_fops;
#endif   
