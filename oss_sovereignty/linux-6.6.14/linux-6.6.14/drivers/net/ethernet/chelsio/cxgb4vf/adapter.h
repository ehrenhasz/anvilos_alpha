#ifndef __CXGB4VF_ADAPTER_H__
#define __CXGB4VF_ADAPTER_H__
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include "../cxgb4/t4_hw.h"
enum {
	MAX_NPORTS	= 1,		 
	MAX_PORT_QSETS	= 8,		 
	MAX_ETH_QSETS	= MAX_NPORTS*MAX_PORT_QSETS,
	MSIX_FW		= 0,		 
	MSIX_IQFLINT	= 1,		 
	MSIX_EXTRAS	= 1,
	MSIX_ENTRIES	= MAX_ETH_QSETS + MSIX_EXTRAS,
	INGQ_EXTRAS	= 2,		 
	MAX_INGQ	= MAX_ETH_QSETS+INGQ_EXTRAS,
	MAX_EGRQ	= MAX_ETH_QSETS*2,
};
struct adapter;
struct sge_eth_rxq;
struct sge_rspq;
struct port_info {
	struct adapter *adapter;	 
	u32 vlan_id;			 
	u16 viid;			 
	int xact_addr_filt;		 
	u16 rss_size;			 
	u8 pidx;			 
	s8 mdio_addr;
	u8 port_type;			 
	u8 mod_type;			 
	u8 port_id;			 
	u8 nqsets;			 
	u8 first_qset;			 
	struct link_config link_cfg;	 
};
struct rx_sw_desc;
struct sge_fl {
	unsigned int avail;		 
	unsigned int pend_cred;		 
	unsigned int cidx;		 
	unsigned int pidx;		 
	unsigned long alloc_failed;	 
	unsigned long large_alloc_failed;
	unsigned long starving;		 
	unsigned int cntxt_id;		 
	unsigned int abs_id;		 
	unsigned int size;		 
	struct rx_sw_desc *sdesc;	 
	__be64 *desc;			 
	dma_addr_t addr;		 
	void __iomem *bar2_addr;	 
	unsigned int bar2_qid;		 
};
struct pkt_gl {
	struct page_frag frags[MAX_SKB_FRAGS];
	void *va;			 
	unsigned int nfrags;		 
	unsigned int tot_len;		 
};
typedef int (*rspq_handler_t)(struct sge_rspq *, const __be64 *,
			      const struct pkt_gl *);
struct sge_rspq {
	struct napi_struct napi;	 
	const __be64 *cur_desc;		 
	unsigned int cidx;		 
	u8 gen;				 
	u8 next_intr_params;		 
	int offset;			 
	unsigned int unhandled_irqs;	 
	u8 intr_params;			 
	u8 pktcnt_idx;			 
	u8 idx;				 
	u16 cntxt_id;			 
	u16 abs_id;			 
	__be64 *desc;			 
	dma_addr_t phys_addr;		 
	void __iomem *bar2_addr;	 
	unsigned int bar2_qid;		 
	unsigned int iqe_len;		 
	unsigned int size;		 
	struct adapter *adapter;	 
	struct net_device *netdev;	 
	rspq_handler_t handler;		 
};
struct sge_eth_stats {
	unsigned long pkts;		 
	unsigned long lro_pkts;		 
	unsigned long lro_merged;	 
	unsigned long rx_cso;		 
	unsigned long vlan_ex;		 
	unsigned long rx_drops;		 
};
struct sge_eth_rxq {
	struct sge_rspq rspq;		 
	struct sge_fl fl;		 
	struct sge_eth_stats stats;	 
};
struct tx_desc {
	__be64 flit[SGE_EQ_IDXSIZE/sizeof(__be64)];
};
struct tx_sw_desc;
struct sge_txq {
	unsigned int in_use;		 
	unsigned int size;		 
	unsigned int cidx;		 
	unsigned int pidx;		 
	unsigned long stops;		 
	unsigned long restarts;		 
	unsigned int cntxt_id;		 
	unsigned int abs_id;		 
	struct tx_desc *desc;		 
	struct tx_sw_desc *sdesc;	 
	struct sge_qstat *stat;		 
	dma_addr_t phys_addr;		 
	void __iomem *bar2_addr;	 
	unsigned int bar2_qid;		 
};
struct sge_eth_txq {
	struct sge_txq q;		 
	struct netdev_queue *txq;	 
	unsigned long tso;		 
	unsigned long tx_cso;		 
	unsigned long vlan_ins;		 
	unsigned long mapping_err;	 
};
struct sge {
	struct sge_eth_txq ethtxq[MAX_ETH_QSETS];
	struct sge_eth_rxq ethrxq[MAX_ETH_QSETS];
	struct sge_rspq fw_evtq ____cacheline_aligned_in_smp;
	struct sge_rspq intrq ____cacheline_aligned_in_smp;
	spinlock_t intrq_lock;
	DECLARE_BITMAP(starving_fl, MAX_EGRQ);
	struct timer_list rx_timer;
	struct timer_list tx_timer;
	u16 max_ethqsets;		 
	u16 ethqsets;			 
	u16 ethtxq_rover;		 
	u16 timer_val[SGE_NTIMERS];	 
	u8 counter_val[SGE_NCOUNTERS];	 
	u32 fl_pg_order;		 
	u32 stat_len;			 
	u32 pktshift;			 
	u32 fl_align;			 
	u32 fl_starve_thres;		 
	unsigned int egr_base;
	unsigned int ingr_base;
	void *egr_map[MAX_EGRQ];
	struct sge_rspq *ingr_map[MAX_INGQ];
};
#define EQ_IDX(s, abs_id) ((unsigned int)((abs_id) - (s)->egr_base))
#define IQ_IDX(s, abs_id) ((unsigned int)((abs_id) - (s)->ingr_base))
#define EQ_MAP(s, abs_id) ((s)->egr_map[EQ_IDX(s, abs_id)])
#define IQ_MAP(s, abs_id) ((s)->ingr_map[IQ_IDX(s, abs_id)])
#define for_each_ethrxq(sge, iter) \
	for (iter = 0; iter < (sge)->ethqsets; iter++)
struct hash_mac_addr {
	struct list_head list;
	u8 addr[ETH_ALEN];
	unsigned int iface_mac;
};
struct mbox_list {
	struct list_head list;
};
struct adapter {
	void __iomem *regs;
	void __iomem *bar2;
	struct pci_dev *pdev;
	struct device *pdev_dev;
	unsigned long registered_device_map;
	unsigned long open_device_map;
	unsigned long flags;
	struct adapter_params params;
	struct {
		unsigned short vec;
		char desc[22];
	} msix_info[MSIX_ENTRIES];
	struct sge sge;
	struct net_device *port[MAX_NPORTS];
	const char *name;
	unsigned int msg_enable;
	struct dentry *debugfs_root;
	spinlock_t stats_lock;
	spinlock_t mbox_lock;
	struct mbox_list mlist;
#define T4VF_OS_LOG_MBOX_CMDS 256
	struct mbox_cmd_log *mbox_log;
	struct list_head mac_hlist;
};
enum {  
	CXGB4VF_FULL_INIT_DONE			= (1UL << 0),
	CXGB4VF_USING_MSI			= (1UL << 1),
	CXGB4VF_USING_MSIX			= (1UL << 2),
	CXGB4VF_QUEUES_BOUND			= (1UL << 3),
	CXGB4VF_ROOT_NO_RELAXED_ORDERING	= (1UL << 4),
	CXGB4VF_FW_OK				= (1UL << 5),
};
static inline u32 t4_read_reg(struct adapter *adapter, u32 reg_addr)
{
	return readl(adapter->regs + reg_addr);
}
static inline void t4_write_reg(struct adapter *adapter, u32 reg_addr, u32 val)
{
	writel(val, adapter->regs + reg_addr);
}
#ifndef readq
static inline u64 readq(const volatile void __iomem *addr)
{
	return readl(addr) + ((u64)readl(addr + 4) << 32);
}
static inline void writeq(u64 val, volatile void __iomem *addr)
{
	writel(val, addr);
	writel(val >> 32, addr + 4);
}
#endif
static inline u64 t4_read_reg64(struct adapter *adapter, u32 reg_addr)
{
	return readq(adapter->regs + reg_addr);
}
static inline void t4_write_reg64(struct adapter *adapter, u32 reg_addr,
				  u64 val)
{
	writeq(val, adapter->regs + reg_addr);
}
static inline const char *port_name(struct adapter *adapter, int pidx)
{
	return adapter->port[pidx]->name;
}
static inline void t4_os_set_hw_addr(struct adapter *adapter, int pidx,
				     u8 hw_addr[])
{
	eth_hw_addr_set(adapter->port[pidx], hw_addr);
}
static inline struct port_info *netdev2pinfo(const struct net_device *dev)
{
	return netdev_priv(dev);
}
static inline struct port_info *adap2pinfo(struct adapter *adapter, int pidx)
{
	return netdev_priv(adapter->port[pidx]);
}
static inline struct adapter *netdev2adap(const struct net_device *dev)
{
	return netdev2pinfo(dev)->adapter;
}
void t4vf_os_link_changed(struct adapter *, int, int);
void t4vf_os_portmod_changed(struct adapter *, int);
int t4vf_sge_alloc_rxq(struct adapter *, struct sge_rspq *, bool,
		       struct net_device *, int,
		       struct sge_fl *, rspq_handler_t);
int t4vf_sge_alloc_eth_txq(struct adapter *, struct sge_eth_txq *,
			   struct net_device *, struct netdev_queue *,
			   unsigned int);
void t4vf_free_sge_resources(struct adapter *);
netdev_tx_t t4vf_eth_xmit(struct sk_buff *, struct net_device *);
int t4vf_ethrx_handler(struct sge_rspq *, const __be64 *,
		       const struct pkt_gl *);
irq_handler_t t4vf_intr_handler(struct adapter *);
irqreturn_t t4vf_sge_intr_msix(int, void *);
int t4vf_sge_init(struct adapter *);
void t4vf_sge_start(struct adapter *);
void t4vf_sge_stop(struct adapter *);
#endif  
