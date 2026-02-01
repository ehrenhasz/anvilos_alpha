 
 

 

#ifndef _NFP_NET_H_
#define _NFP_NET_H_

#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/dim.h>
#include <linux/io-64-nonatomic-hi-lo.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <net/xdp.h>

#include "nfp_net_ctrl.h"

#define nn_pr(nn, lvl, fmt, args...)					\
	({								\
		struct nfp_net *__nn = (nn);				\
									\
		if (__nn->dp.netdev)					\
			netdev_printk(lvl, __nn->dp.netdev, fmt, ## args); \
		else							\
			dev_printk(lvl, __nn->dp.dev, "ctrl: " fmt, ## args); \
	})

#define nn_err(nn, fmt, args...)	nn_pr(nn, KERN_ERR, fmt, ## args)
#define nn_warn(nn, fmt, args...)	nn_pr(nn, KERN_WARNING, fmt, ## args)
#define nn_info(nn, fmt, args...)	nn_pr(nn, KERN_INFO, fmt, ## args)
#define nn_dbg(nn, fmt, args...)	nn_pr(nn, KERN_DEBUG, fmt, ## args)

#define nn_dp_warn(dp, fmt, args...)					\
	({								\
		struct nfp_net_dp *__dp = (dp);				\
									\
		if (unlikely(net_ratelimit())) {			\
			if (__dp->netdev)				\
				netdev_warn(__dp->netdev, fmt, ## args); \
			else						\
				dev_warn(__dp->dev, fmt, ## args);	\
		}							\
	})

 
#define NFP_NET_POLL_TIMEOUT	5

 
#define NFP_NET_STAT_POLL_IVL	msecs_to_jiffies(100)

 
#define NFP_NET_CTRL_BAR	0
#define NFP_NET_Q0_BAR		2
#define NFP_NET_Q1_BAR		4	 

 
#define NFP_NET_DEFAULT_MTU		1500U

 
#define NFP_NET_MAX_PREPEND		64

 
#define NFP_NET_NON_Q_VECTORS		2
#define NFP_NET_IRQ_LSC_IDX		0
#define NFP_NET_IRQ_EXN_IDX		1
#define NFP_NET_MIN_VNIC_IRQS		(NFP_NET_NON_Q_VECTORS + 1)

 
#define NFP_NET_MAX_TX_RINGS	64	 
#define NFP_NET_MAX_RX_RINGS	64	 
#define NFP_NET_MAX_R_VECS	(NFP_NET_MAX_TX_RINGS > NFP_NET_MAX_RX_RINGS ? \
				 NFP_NET_MAX_TX_RINGS : NFP_NET_MAX_RX_RINGS)
#define NFP_NET_MAX_IRQS	(NFP_NET_NON_Q_VECTORS + NFP_NET_MAX_R_VECS)

#define NFP_NET_TX_DESCS_DEFAULT 4096	 
#define NFP_NET_RX_DESCS_DEFAULT 4096	 

#define NFP_NET_FL_BATCH	16	 
#define NFP_NET_XDP_MAX_COMPLETE 2048	 

 
#define NFP_NET_CFG_MAC_MC_MAX	1024	 

 
#define NFP_NET_N_VXLAN_PORTS	(NFP_NET_CFG_VXLAN_SZ / sizeof(__be16))

#define NFP_NET_RX_BUF_HEADROOM	(NET_SKB_PAD + NET_IP_ALIGN)
#define NFP_NET_RX_BUF_NON_DATA	(NFP_NET_RX_BUF_HEADROOM +		\
				 SKB_DATA_ALIGN(sizeof(struct skb_shared_info)))

 
struct nfp_cpp;
struct nfp_dev_info;
struct nfp_dp_ops;
struct nfp_eth_table_port;
struct nfp_net;
struct nfp_net_r_vector;
struct nfp_port;
struct xsk_buff_pool;

struct nfp_nfd3_tx_desc;
struct nfp_nfd3_tx_buf;

struct nfp_nfdk_tx_desc;
struct nfp_nfdk_tx_buf;

 
#define D_IDX(ring, idx)	((idx) & ((ring)->cnt - 1))

 
#define nfp_desc_set_dma_addr_40b(desc, dma_addr)			\
	do {								\
		__typeof__(desc) __d = (desc);				\
		dma_addr_t __addr = (dma_addr);				\
									\
		__d->dma_addr_lo = cpu_to_le32(lower_32_bits(__addr));	\
		__d->dma_addr_hi = upper_32_bits(__addr) & 0xff;	\
	} while (0)

#define nfp_desc_set_dma_addr_48b(desc, dma_addr)			\
	do {								\
		__typeof__(desc) __d = (desc);				\
		dma_addr_t __addr = (dma_addr);				\
									\
		__d->dma_addr_hi = cpu_to_le16(upper_32_bits(__addr));	\
		__d->dma_addr_lo = cpu_to_le32(lower_32_bits(__addr));	\
	} while (0)

 
struct nfp_net_tx_ring {
	struct nfp_net_r_vector *r_vec;

	u16 idx;
	u16 data_pending;
	u8 __iomem *qcp_q;
	u64 *txrwb;

	u32 cnt;
	u32 wr_p;
	u32 rd_p;
	u32 qcp_rd_p;

	u32 wr_ptr_add;

	union {
		struct nfp_nfd3_tx_buf *txbufs;
		struct nfp_nfdk_tx_buf *ktxbufs;
	};
	union {
		struct nfp_nfd3_tx_desc *txds;
		struct nfp_nfdk_tx_desc *ktxds;
	};

	 
	int qcidx;

	dma_addr_t dma;
	size_t size;
	bool is_xdp;
} ____cacheline_aligned;

 

#define PCIE_DESC_RX_DD			BIT(7)
#define PCIE_DESC_RX_META_LEN_MASK	GENMASK(6, 0)

 
#define PCIE_DESC_RX_RSS		cpu_to_le16(BIT(15))
#define PCIE_DESC_RX_I_IP4_CSUM		cpu_to_le16(BIT(14))
#define PCIE_DESC_RX_I_IP4_CSUM_OK	cpu_to_le16(BIT(13))
#define PCIE_DESC_RX_I_TCP_CSUM		cpu_to_le16(BIT(12))
#define PCIE_DESC_RX_I_TCP_CSUM_OK	cpu_to_le16(BIT(11))
#define PCIE_DESC_RX_I_UDP_CSUM		cpu_to_le16(BIT(10))
#define PCIE_DESC_RX_I_UDP_CSUM_OK	cpu_to_le16(BIT(9))
#define PCIE_DESC_RX_DECRYPTED		cpu_to_le16(BIT(8))
#define PCIE_DESC_RX_EOP		cpu_to_le16(BIT(7))
#define PCIE_DESC_RX_IP4_CSUM		cpu_to_le16(BIT(6))
#define PCIE_DESC_RX_IP4_CSUM_OK	cpu_to_le16(BIT(5))
#define PCIE_DESC_RX_TCP_CSUM		cpu_to_le16(BIT(4))
#define PCIE_DESC_RX_TCP_CSUM_OK	cpu_to_le16(BIT(3))
#define PCIE_DESC_RX_UDP_CSUM		cpu_to_le16(BIT(2))
#define PCIE_DESC_RX_UDP_CSUM_OK	cpu_to_le16(BIT(1))
#define PCIE_DESC_RX_VLAN		cpu_to_le16(BIT(0))

#define PCIE_DESC_RX_CSUM_ALL		(PCIE_DESC_RX_IP4_CSUM |	\
					 PCIE_DESC_RX_TCP_CSUM |	\
					 PCIE_DESC_RX_UDP_CSUM |	\
					 PCIE_DESC_RX_I_IP4_CSUM |	\
					 PCIE_DESC_RX_I_TCP_CSUM |	\
					 PCIE_DESC_RX_I_UDP_CSUM)
#define PCIE_DESC_RX_CSUM_OK_SHIFT	1
#define __PCIE_DESC_RX_CSUM_ALL		le16_to_cpu(PCIE_DESC_RX_CSUM_ALL)
#define __PCIE_DESC_RX_CSUM_ALL_OK	(__PCIE_DESC_RX_CSUM_ALL >>	\
					 PCIE_DESC_RX_CSUM_OK_SHIFT)

struct nfp_net_rx_desc {
	union {
		struct {
			__le16 dma_addr_hi;  
			u8 reserved;  
			u8 meta_len_dd;  

			__le32 dma_addr_lo;  
		} __packed fld;

		struct {
			__le16 data_len;  
			u8 reserved;
			u8 meta_len_dd;	 

			__le16 flags;	 
			__le16 vlan;	 
		} __packed rxd;

		__le32 vals[2];
	};
};

#define NFP_NET_META_FIELD_MASK GENMASK(NFP_NET_META_FIELD_SIZE - 1, 0)
#define NFP_NET_VLAN_CTAG	0
#define NFP_NET_VLAN_STAG	1

struct nfp_meta_parsed {
	u8 hash_type;
	u8 csum_type;
	u32 hash;
	u32 mark;
	u32 portid;
	__wsum csum;
	struct {
		bool stripped;
		u8 tpid;
		u16 tci;
	} vlan;

#ifdef CONFIG_NFP_NET_IPSEC
	u32 ipsec_saidx;
#endif
};

struct nfp_net_rx_hash {
	__be32 hash_type;
	__be32 hash;
};

 
struct nfp_net_rx_buf {
	void *frag;
	dma_addr_t dma_addr;
};

 
struct nfp_net_xsk_rx_buf {
	dma_addr_t dma_addr;
	struct xdp_buff *xdp;
};

 
struct nfp_net_rx_ring {
	struct nfp_net_r_vector *r_vec;

	u32 cnt;
	u32 wr_p;
	u32 rd_p;

	u32 idx;

	int fl_qcidx;
	u8 __iomem *qcp_fl;

	struct nfp_net_rx_buf *rxbufs;
	struct nfp_net_xsk_rx_buf *xsk_rxbufs;
	struct nfp_net_rx_desc *rxds;

	struct xdp_rxq_info xdp_rxq;

	dma_addr_t dma;
	size_t size;
} ____cacheline_aligned;

 
struct nfp_net_r_vector {
	struct nfp_net *nfp_net;
	union {
		struct napi_struct napi;
		struct {
			struct tasklet_struct tasklet;
			struct sk_buff_head queue;
			spinlock_t lock;
		};
	};

	struct nfp_net_tx_ring *tx_ring;
	struct nfp_net_rx_ring *rx_ring;

	u16 irq_entry;

	u16 event_ctr;
	struct dim rx_dim;
	struct dim tx_dim;

	struct u64_stats_sync rx_sync;
	u64 rx_pkts;
	u64 rx_bytes;
	u64 rx_drops;
	u64 hw_csum_rx_ok;
	u64 hw_csum_rx_inner_ok;
	u64 hw_csum_rx_complete;
	u64 hw_tls_rx;

	u64 hw_csum_rx_error;
	u64 rx_replace_buf_alloc_fail;

	struct nfp_net_tx_ring *xdp_ring;
	struct xsk_buff_pool *xsk_pool;

	struct u64_stats_sync tx_sync;
	u64 tx_pkts;
	u64 tx_bytes;

	u64 ____cacheline_aligned_in_smp hw_csum_tx;
	u64 hw_csum_tx_inner;
	u64 tx_gather;
	u64 tx_lso;
	u64 hw_tls_tx;

	u64 tls_tx_fallback;
	u64 tls_tx_no_fallback;
	u64 tx_errors;
	u64 tx_busy;

	 

	u32 irq_vector;
	irq_handler_t handler;
	char name[IFNAMSIZ + 8];
	cpumask_t affinity_mask;
} ____cacheline_aligned;

 
struct nfp_net_fw_version {
	u8 minor;
	u8 major;
	u8 class;

	 
	u8 extend;
} __packed;

static inline bool nfp_net_fw_ver_eq(struct nfp_net_fw_version *fw_ver,
				     u8 extend, u8 class, u8 major, u8 minor)
{
	return fw_ver->extend == extend &&
	       fw_ver->class == class &&
	       fw_ver->major == major &&
	       fw_ver->minor == minor;
}

struct nfp_stat_pair {
	u64 pkts;
	u64 bytes;
};

 
struct nfp_net_dp {
	struct device *dev;
	struct net_device *netdev;

	u8 is_vf:1;
	u8 chained_metadata_format:1;
	u8 ktls_tx:1;

	u8 rx_dma_dir;
	u8 rx_offset;

	u32 rx_dma_off;

	u32 ctrl;
	u32 ctrl_w1;
	u32 fl_bufsz;

	struct bpf_prog *xdp_prog;

	struct nfp_net_tx_ring *tx_rings;
	struct nfp_net_rx_ring *rx_rings;

	u8 __iomem *ctrl_bar;

	 

	const struct nfp_dp_ops *ops;

	u64 *txrwb;
	dma_addr_t txrwb_dma;

	unsigned int txd_cnt;
	unsigned int rxd_cnt;

	unsigned int num_r_vecs;

	unsigned int num_tx_rings;
	unsigned int num_stack_tx_rings;
	unsigned int num_rx_rings;

	unsigned int mtu;

	struct xsk_buff_pool **xsk_pools;
};

 
struct nfp_net {
	struct nfp_net_dp dp;

	const struct nfp_dev_info *dev_info;
	struct nfp_net_fw_version fw_ver;

	u32 id;

	u32 cap;
	u32 cap_w1;
	u32 max_mtu;

	u8 rss_hfunc;
	u32 rss_cfg;
	u8 rss_key[NFP_NET_CFG_RSS_KEY_SZ];
	u8 rss_itbl[NFP_NET_CFG_RSS_ITBL_SZ];

	struct xdp_attachment_info xdp;
	struct xdp_attachment_info xdp_hw;

	unsigned int max_tx_rings;
	unsigned int max_rx_rings;

	int stride_tx;
	int stride_rx;

	unsigned int max_r_vecs;
	struct nfp_net_r_vector r_vecs[NFP_NET_MAX_R_VECS];
	struct msix_entry irq_entries[NFP_NET_MAX_IRQS];

	irq_handler_t lsc_handler;
	char lsc_name[IFNAMSIZ + 8];

	irq_handler_t exn_handler;
	char exn_name[IFNAMSIZ + 8];

	irq_handler_t shared_handler;
	char shared_name[IFNAMSIZ + 8];

	bool link_up;
	spinlock_t link_status_lock;

	spinlock_t reconfig_lock;
	u32 reconfig_posted;
	bool reconfig_timer_active;
	bool reconfig_sync_present;
	struct timer_list reconfig_timer;
	u32 reconfig_in_progress_update;

	struct semaphore bar_lock;

	bool rx_coalesce_adapt_on;
	bool tx_coalesce_adapt_on;
	u32 rx_coalesce_usecs;
	u32 rx_coalesce_max_frames;
	u32 tx_coalesce_usecs;
	u32 tx_coalesce_max_frames;

	u8 __iomem *qcp_cfg;

	u8 __iomem *tx_bar;
	u8 __iomem *rx_bar;

#ifdef CONFIG_NFP_NET_IPSEC
	struct xarray xa_ipsec;
#endif

	struct nfp_net_tlv_caps tlv_caps;

	unsigned int ktls_tx_conn_cnt;
	unsigned int ktls_rx_conn_cnt;

	atomic64_t ktls_conn_id_gen;

	atomic_t ktls_no_space;
	atomic_t ktls_rx_resync_req;
	atomic_t ktls_rx_resync_ign;
	atomic_t ktls_rx_resync_sent;

	struct {
		struct sk_buff_head queue;
		wait_queue_head_t wq;
		struct workqueue_struct *workq;
		struct work_struct wait_work;
		struct work_struct runq_work;
		u16 tag;
	} mbox_cmsg;

	struct dentry *debugfs_dir;

	struct list_head vnic_list;

	struct pci_dev *pdev;
	struct nfp_app *app;

	bool vnic_no_name;

	struct nfp_port *port;

	struct {
		spinlock_t lock;
		struct list_head list;
		struct work_struct work;
	} mbox_amsg;

	void *app_priv;
};

struct nfp_mbox_amsg_entry {
	struct list_head list;
	int (*cfg)(struct nfp_net *nn, struct nfp_mbox_amsg_entry *entry);
	u32 cmd;
	char msg[];
};

int nfp_net_sched_mbox_amsg_work(struct nfp_net *nn, u32 cmd, const void *data, size_t len,
				 int (*cb)(struct nfp_net *, struct nfp_mbox_amsg_entry *));

 
static inline u16 nn_readb(struct nfp_net *nn, int off)
{
	return readb(nn->dp.ctrl_bar + off);
}

static inline void nn_writeb(struct nfp_net *nn, int off, u8 val)
{
	writeb(val, nn->dp.ctrl_bar + off);
}

static inline u16 nn_readw(struct nfp_net *nn, int off)
{
	return readw(nn->dp.ctrl_bar + off);
}

static inline void nn_writew(struct nfp_net *nn, int off, u16 val)
{
	writew(val, nn->dp.ctrl_bar + off);
}

static inline u32 nn_readl(struct nfp_net *nn, int off)
{
	return readl(nn->dp.ctrl_bar + off);
}

static inline void nn_writel(struct nfp_net *nn, int off, u32 val)
{
	writel(val, nn->dp.ctrl_bar + off);
}

static inline u64 nn_readq(struct nfp_net *nn, int off)
{
	return readq(nn->dp.ctrl_bar + off);
}

static inline void nn_writeq(struct nfp_net *nn, int off, u64 val)
{
	writeq(val, nn->dp.ctrl_bar + off);
}

 
static inline void nn_pci_flush(struct nfp_net *nn)
{
	nn_readl(nn, NFP_NET_CFG_VERSION);
}

 
#define NFP_QCP_QUEUE_ADDR_SZ			0x800
#define NFP_QCP_QUEUE_OFF(_x)			((_x) * NFP_QCP_QUEUE_ADDR_SZ)
#define NFP_QCP_QUEUE_ADD_RPTR			0x0000
#define NFP_QCP_QUEUE_ADD_WPTR			0x0004
#define NFP_QCP_QUEUE_STS_LO			0x0008
#define NFP_QCP_QUEUE_STS_LO_READPTR_mask	0x3ffff
#define NFP_QCP_QUEUE_STS_HI			0x000c
#define NFP_QCP_QUEUE_STS_HI_WRITEPTR_mask	0x3ffff

 
enum nfp_qcp_ptr {
	NFP_QCP_READ_PTR = 0,
	NFP_QCP_WRITE_PTR
};

 
static inline void nfp_qcp_rd_ptr_add(u8 __iomem *q, u32 val)
{
	writel(val, q + NFP_QCP_QUEUE_ADD_RPTR);
}

 
static inline void nfp_qcp_wr_ptr_add(u8 __iomem *q, u32 val)
{
	writel(val, q + NFP_QCP_QUEUE_ADD_WPTR);
}

static inline u32 _nfp_qcp_read(u8 __iomem *q, enum nfp_qcp_ptr ptr)
{
	u32 off;
	u32 val;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_STS_LO;
	else
		off = NFP_QCP_QUEUE_STS_HI;

	val = readl(q + off);

	if (ptr == NFP_QCP_READ_PTR)
		return val & NFP_QCP_QUEUE_STS_LO_READPTR_mask;
	else
		return val & NFP_QCP_QUEUE_STS_HI_WRITEPTR_mask;
}

 
static inline u32 nfp_qcp_rd_ptr_read(u8 __iomem *q)
{
	return _nfp_qcp_read(q, NFP_QCP_READ_PTR);
}

 
static inline u32 nfp_qcp_wr_ptr_read(u8 __iomem *q)
{
	return _nfp_qcp_read(q, NFP_QCP_WRITE_PTR);
}

u32 nfp_qcp_queue_offset(const struct nfp_dev_info *dev_info, u16 queue);

static inline bool nfp_net_is_data_vnic(struct nfp_net *nn)
{
	WARN_ON_ONCE(!nn->dp.netdev && nn->port);
	return !!nn->dp.netdev;
}

static inline bool nfp_net_running(struct nfp_net *nn)
{
	return nn->dp.ctrl & NFP_NET_CFG_CTRL_ENABLE;
}

static inline const char *nfp_net_name(struct nfp_net *nn)
{
	return nn->dp.netdev ? nn->dp.netdev->name : "ctrl";
}

static inline void nfp_ctrl_lock(struct nfp_net *nn)
	__acquires(&nn->r_vecs[0].lock)
{
	spin_lock_bh(&nn->r_vecs[0].lock);
}

static inline void nfp_ctrl_unlock(struct nfp_net *nn)
	__releases(&nn->r_vecs[0].lock)
{
	spin_unlock_bh(&nn->r_vecs[0].lock);
}

static inline void nn_ctrl_bar_lock(struct nfp_net *nn)
{
	down(&nn->bar_lock);
}

static inline bool nn_ctrl_bar_trylock(struct nfp_net *nn)
{
	return !down_trylock(&nn->bar_lock);
}

static inline void nn_ctrl_bar_unlock(struct nfp_net *nn)
{
	up(&nn->bar_lock);
}

 
extern const char nfp_driver_version[];

extern const struct net_device_ops nfp_nfd3_netdev_ops;
extern const struct net_device_ops nfp_nfdk_netdev_ops;

static inline bool nfp_netdev_is_nfp_net(struct net_device *netdev)
{
	return netdev->netdev_ops == &nfp_nfd3_netdev_ops ||
	       netdev->netdev_ops == &nfp_nfdk_netdev_ops;
}

static inline int nfp_net_coalesce_para_check(u32 usecs, u32 pkts)
{
	if ((usecs >= ((1 << 16) - 1)) || (pkts >= ((1 << 16) - 1)))
		return -EINVAL;

	return 0;
}

 
void nfp_net_get_fw_version(struct nfp_net_fw_version *fw_ver,
			    void __iomem *ctrl_bar);

struct nfp_net *
nfp_net_alloc(struct pci_dev *pdev, const struct nfp_dev_info *dev_info,
	      void __iomem *ctrl_bar, bool needs_netdev,
	      unsigned int max_tx_rings, unsigned int max_rx_rings);
void nfp_net_free(struct nfp_net *nn);

int nfp_net_init(struct nfp_net *nn);
void nfp_net_clean(struct nfp_net *nn);

int nfp_ctrl_open(struct nfp_net *nn);
void nfp_ctrl_close(struct nfp_net *nn);

void nfp_net_set_ethtool_ops(struct net_device *netdev);
void nfp_net_info(struct nfp_net *nn);
int __nfp_net_reconfig(struct nfp_net *nn, u32 update);
int nfp_net_reconfig(struct nfp_net *nn, u32 update);
unsigned int nfp_net_rss_key_sz(struct nfp_net *nn);
void nfp_net_rss_write_itbl(struct nfp_net *nn);
void nfp_net_rss_write_key(struct nfp_net *nn);
void nfp_net_coalesce_write_cfg(struct nfp_net *nn);
int nfp_net_mbox_lock(struct nfp_net *nn, unsigned int data_size);
int nfp_net_mbox_reconfig(struct nfp_net *nn, u32 mbox_cmd);
int nfp_net_mbox_reconfig_and_unlock(struct nfp_net *nn, u32 mbox_cmd);
void nfp_net_mbox_reconfig_post(struct nfp_net *nn, u32 update);
int nfp_net_mbox_reconfig_wait_posted(struct nfp_net *nn);

unsigned int
nfp_net_irqs_alloc(struct pci_dev *pdev, struct msix_entry *irq_entries,
		   unsigned int min_irqs, unsigned int want_irqs);
void nfp_net_irqs_disable(struct pci_dev *pdev);
void
nfp_net_irqs_assign(struct nfp_net *nn, struct msix_entry *irq_entries,
		    unsigned int n);
struct sk_buff *
nfp_net_tls_tx(struct nfp_net_dp *dp, struct nfp_net_r_vector *r_vec,
	       struct sk_buff *skb, u64 *tls_handle, int *nr_frags);
void nfp_net_tls_tx_undo(struct sk_buff *skb, u64 tls_handle);

struct nfp_net_dp *nfp_net_clone_dp(struct nfp_net *nn);
int nfp_net_ring_reconfig(struct nfp_net *nn, struct nfp_net_dp *new,
			  struct netlink_ext_ack *extack);

#ifdef CONFIG_NFP_DEBUG
void nfp_net_debugfs_create(void);
void nfp_net_debugfs_destroy(void);
struct dentry *nfp_net_debugfs_device_add(struct pci_dev *pdev);
void nfp_net_debugfs_vnic_add(struct nfp_net *nn, struct dentry *ddir);
void nfp_net_debugfs_dir_clean(struct dentry **dir);
#else
static inline void nfp_net_debugfs_create(void)
{
}

static inline void nfp_net_debugfs_destroy(void)
{
}

static inline struct dentry *nfp_net_debugfs_device_add(struct pci_dev *pdev)
{
	return NULL;
}

static inline void
nfp_net_debugfs_vnic_add(struct nfp_net *nn, struct dentry *ddir)
{
}

static inline void nfp_net_debugfs_dir_clean(struct dentry **dir)
{
}
#endif  

#endif  
