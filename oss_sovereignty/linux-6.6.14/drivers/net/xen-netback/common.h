#ifndef __XEN_NETBACK__COMMON_H__
#define __XEN_NETBACK__COMMON_H__
#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/io.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <xen/interface/io/netif.h>
#include <xen/interface/grant_table.h>
#include <xen/grant_table.h>
#include <xen/xenbus.h>
#include <xen/page.h>
#include <linux/debugfs.h>
typedef unsigned int pending_ring_idx_t;
struct pending_tx_info {
	struct xen_netif_tx_request req;  
	unsigned int extra_count;
	struct ubuf_info_msgzc callback_struct;
};
#define XEN_NETIF_TX_RING_SIZE __CONST_RING_SIZE(xen_netif_tx, XEN_PAGE_SIZE)
#define XEN_NETIF_RX_RING_SIZE __CONST_RING_SIZE(xen_netif_rx, XEN_PAGE_SIZE)
struct xenvif_rx_meta {
	int id;
	int size;
	int gso_type;
	int gso_size;
};
#define GSO_BIT(type) \
	(1 << XEN_NETIF_GSO_TYPE_ ## type)
#define INVALID_PENDING_IDX 0xFFFF
#define MAX_PENDING_REQS XEN_NETIF_TX_RING_SIZE
#define MAX_XEN_SKB_FRAGS (65536 / XEN_PAGE_SIZE + 1)
#define NETBACK_INVALID_HANDLE -1
#define XEN_NETBK_LEGACY_SLOTS_MAX XEN_NETIF_NR_SLOTS_MIN
#define QUEUE_NAME_SIZE (IFNAMSIZ + 5)
#define IRQ_NAME_SIZE (QUEUE_NAME_SIZE + 3)
struct xenvif;
struct xenvif_stats {
	u64 rx_bytes;
	u64 rx_packets;
	u64 tx_bytes;
	u64 tx_packets;
	unsigned long rx_gso_checksum_fixup;
	unsigned long tx_zerocopy_sent;
	unsigned long tx_zerocopy_success;
	unsigned long tx_zerocopy_fail;
	unsigned long tx_frag_overflow;
};
#define COPY_BATCH_SIZE 64
struct xenvif_copy_state {
	struct gnttab_copy op[COPY_BATCH_SIZE];
	RING_IDX idx[COPY_BATCH_SIZE];
	unsigned int num;
	struct sk_buff_head *completed;
};
struct xenvif_queue {  
	unsigned int id;  
	char name[QUEUE_NAME_SIZE];  
	struct xenvif *vif;  
	atomic_t eoi_pending;
#define NETBK_RX_EOI		0x01
#define NETBK_TX_EOI		0x02
#define NETBK_COMMON_EOI	0x04
	struct napi_struct napi;
	unsigned int tx_irq;
	char tx_irq_name[IRQ_NAME_SIZE];  
	struct xen_netif_tx_back_ring tx;
	struct sk_buff_head tx_queue;
	struct page *mmap_pages[MAX_PENDING_REQS];
	pending_ring_idx_t pending_prod;
	pending_ring_idx_t pending_cons;
	u16 pending_ring[MAX_PENDING_REQS];
	struct pending_tx_info pending_tx_info[MAX_PENDING_REQS];
	grant_handle_t grant_tx_handle[MAX_PENDING_REQS];
	struct gnttab_copy tx_copy_ops[2 * MAX_PENDING_REQS];
	struct gnttab_map_grant_ref tx_map_ops[MAX_PENDING_REQS];
	struct gnttab_unmap_grant_ref tx_unmap_ops[MAX_PENDING_REQS];
	struct page *pages_to_map[MAX_PENDING_REQS];
	struct page *pages_to_unmap[MAX_PENDING_REQS];
	spinlock_t callback_lock;
	spinlock_t response_lock;
	pending_ring_idx_t dealloc_prod;
	pending_ring_idx_t dealloc_cons;
	u16 dealloc_ring[MAX_PENDING_REQS];
	struct task_struct *dealloc_task;
	wait_queue_head_t dealloc_wq;
	atomic_t inflight_packets;
	struct task_struct *task;
	wait_queue_head_t wq;
	unsigned int rx_irq;
	char rx_irq_name[IRQ_NAME_SIZE];  
	struct xen_netif_rx_back_ring rx;
	struct sk_buff_head rx_queue;
	unsigned int rx_queue_max;
	unsigned int rx_queue_len;
	unsigned long last_rx_time;
	unsigned int rx_slots_needed;
	bool stalled;
	struct xenvif_copy_state rx_copy;
	unsigned long   credit_bytes;
	unsigned long   credit_usec;
	unsigned long   remaining_credit;
	struct timer_list credit_timeout;
	u64 credit_window_start;
	bool rate_limited;
	struct xenvif_stats stats;
};
enum state_bit_shift {
	VIF_STATUS_CONNECTED,
};
struct xenvif_mcast_addr {
	struct list_head entry;
	struct rcu_head rcu;
	u8 addr[6];
};
#define XEN_NETBK_MCAST_MAX 64
#define XEN_NETBK_MAX_HASH_KEY_SIZE 40
#define XEN_NETBK_MAX_HASH_MAPPING_SIZE 128
#define XEN_NETBK_HASH_TAG_SIZE 40
struct xenvif_hash_cache_entry {
	struct list_head link;
	struct rcu_head rcu;
	u8 tag[XEN_NETBK_HASH_TAG_SIZE];
	unsigned int len;
	u32 val;
	int seq;
};
struct xenvif_hash_cache {
	spinlock_t lock;
	struct list_head list;
	unsigned int count;
	atomic_t seq;
};
struct xenvif_hash {
	unsigned int alg;
	u32 flags;
	bool mapping_sel;
	u8 key[XEN_NETBK_MAX_HASH_KEY_SIZE];
	u32 mapping[2][XEN_NETBK_MAX_HASH_MAPPING_SIZE];
	unsigned int size;
	struct xenvif_hash_cache cache;
};
struct backend_info {
	struct xenbus_device *dev;
	struct xenvif *vif;
	enum xenbus_state state;
	enum xenbus_state frontend_state;
	struct xenbus_watch hotplug_status_watch;
	u8 have_hotplug_status_watch:1;
	const char *hotplug_script;
};
struct xenvif {
	domid_t          domid;
	unsigned int     handle;
	u8               fe_dev_addr[6];
	struct list_head fe_mcast_addr;
	unsigned int     fe_mcast_count;
	int gso_mask;
	u8 can_sg:1;
	u8 ip_csum:1;
	u8 ipv6_csum:1;
	u8 multicast_control:1;
	u16 xdp_headroom;
	bool disabled;
	unsigned long status;
	unsigned long drain_timeout;
	unsigned long stall_timeout;
	struct xenvif_queue *queues;
	unsigned int num_queues;  
	unsigned int stalled_queues;
	struct xenvif_hash hash;
	struct xenbus_watch credit_watch;
	struct xenbus_watch mcast_ctrl_watch;
	struct backend_info *be;
	spinlock_t lock;
#ifdef CONFIG_DEBUG_FS
	struct dentry *xenvif_dbg_root;
#endif
	struct xen_netif_ctrl_back_ring ctrl;
	unsigned int ctrl_irq;
	struct net_device *dev;
};
struct xenvif_rx_cb {
	unsigned long expires;
	int meta_slots_used;
};
#define XENVIF_RX_CB(skb) ((struct xenvif_rx_cb *)(skb)->cb)
static inline struct xenbus_device *xenvif_to_xenbus_device(struct xenvif *vif)
{
	return to_xenbus_device(vif->dev->dev.parent);
}
void xenvif_tx_credit_callback(struct timer_list *t);
struct xenvif *xenvif_alloc(struct device *parent,
			    domid_t domid,
			    unsigned int handle);
int xenvif_init_queue(struct xenvif_queue *queue);
void xenvif_deinit_queue(struct xenvif_queue *queue);
int xenvif_connect_data(struct xenvif_queue *queue,
			unsigned long tx_ring_ref,
			unsigned long rx_ring_ref,
			unsigned int tx_evtchn,
			unsigned int rx_evtchn);
void xenvif_disconnect_data(struct xenvif *vif);
int xenvif_connect_ctrl(struct xenvif *vif, grant_ref_t ring_ref,
			unsigned int evtchn);
void xenvif_disconnect_ctrl(struct xenvif *vif);
void xenvif_free(struct xenvif *vif);
int xenvif_xenbus_init(void);
void xenvif_xenbus_fini(void);
void xenvif_unmap_frontend_data_rings(struct xenvif_queue *queue);
int xenvif_map_frontend_data_rings(struct xenvif_queue *queue,
				   grant_ref_t tx_ring_ref,
				   grant_ref_t rx_ring_ref);
void xenvif_napi_schedule_or_enable_events(struct xenvif_queue *queue);
void xenvif_carrier_off(struct xenvif *vif);
int xenvif_tx_action(struct xenvif_queue *queue, int budget);
int xenvif_kthread_guest_rx(void *data);
void xenvif_kick_thread(struct xenvif_queue *queue);
int xenvif_dealloc_kthread(void *data);
irqreturn_t xenvif_ctrl_irq_fn(int irq, void *data);
bool xenvif_have_rx_work(struct xenvif_queue *queue, bool test_kthread);
bool xenvif_rx_queue_tail(struct xenvif_queue *queue, struct sk_buff *skb);
void xenvif_carrier_on(struct xenvif *vif);
void xenvif_zerocopy_callback(struct sk_buff *skb, struct ubuf_info *ubuf,
			      bool zerocopy_success);
static inline pending_ring_idx_t nr_pending_reqs(struct xenvif_queue *queue)
{
	return MAX_PENDING_REQS -
		queue->pending_prod + queue->pending_cons;
}
irqreturn_t xenvif_interrupt(int irq, void *dev_id);
extern bool separate_tx_rx_irq;
extern bool provides_xdp_headroom;
extern unsigned int rx_drain_timeout_msecs;
extern unsigned int rx_stall_timeout_msecs;
extern unsigned int xenvif_max_queues;
extern unsigned int xenvif_hash_cache_size;
#ifdef CONFIG_DEBUG_FS
extern struct dentry *xen_netback_dbg_root;
#endif
void xenvif_skb_zerocopy_prepare(struct xenvif_queue *queue,
				 struct sk_buff *skb);
void xenvif_skb_zerocopy_complete(struct xenvif_queue *queue);
bool xenvif_mcast_match(struct xenvif *vif, const u8 *addr);
void xenvif_mcast_addr_list_free(struct xenvif *vif);
void xenvif_init_hash(struct xenvif *vif);
void xenvif_deinit_hash(struct xenvif *vif);
u32 xenvif_set_hash_alg(struct xenvif *vif, u32 alg);
u32 xenvif_get_hash_flags(struct xenvif *vif, u32 *flags);
u32 xenvif_set_hash_flags(struct xenvif *vif, u32 flags);
u32 xenvif_set_hash_key(struct xenvif *vif, u32 gref, u32 len);
u32 xenvif_set_hash_mapping_size(struct xenvif *vif, u32 size);
u32 xenvif_set_hash_mapping(struct xenvif *vif, u32 gref, u32 len,
			    u32 off);
void xenvif_set_skb_hash(struct xenvif *vif, struct sk_buff *skb);
#ifdef CONFIG_DEBUG_FS
void xenvif_dump_hash_info(struct xenvif *vif, struct seq_file *m);
#endif
#endif  
