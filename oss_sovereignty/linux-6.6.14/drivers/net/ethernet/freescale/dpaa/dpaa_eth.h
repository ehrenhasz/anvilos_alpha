 
 

#ifndef __DPAA_H
#define __DPAA_H

#include <linux/netdevice.h>
#include <linux/refcount.h>
#include <net/xdp.h>
#include <soc/fsl/qman.h>
#include <soc/fsl/bman.h>

#include "fman.h"
#include "mac.h"
#include "dpaa_eth_trace.h"

 
#define DPAA_TC_NUM		4
 
#define DPAA_TC_TXQ_NUM		NR_CPUS
 
#define DPAA_ETH_TXQ_NUM	(DPAA_TC_NUM * DPAA_TC_TXQ_NUM)

 
enum dpaa_fq_type {
	FQ_TYPE_RX_DEFAULT = 1,  
	FQ_TYPE_RX_ERROR,	 
	FQ_TYPE_RX_PCD,		 
	FQ_TYPE_TX,		 
	FQ_TYPE_TX_CONFIRM,	 
	FQ_TYPE_TX_CONF_MQ,	 
	FQ_TYPE_TX_ERROR,	 
};

struct dpaa_fq {
	struct qman_fq fq_base;
	struct list_head list;
	struct net_device *net_dev;
	bool init;
	u32 fqid;
	u32 flags;
	u16 channel;
	u8 wq;
	enum dpaa_fq_type fq_type;
	struct xdp_rxq_info xdp_rxq;
};

struct dpaa_fq_cbs {
	struct qman_fq rx_defq;
	struct qman_fq tx_defq;
	struct qman_fq rx_errq;
	struct qman_fq tx_errq;
	struct qman_fq egress_ern;
};

struct dpaa_priv;

struct dpaa_bp {
	 
	struct dpaa_priv *priv;
	 
	int __percpu *percpu_count;
	 
	size_t raw_size;
	 
	size_t size;
	 
	u16 config_count;
	u8 bpid;
	struct bman_pool *pool;
	 
	int (*seed_cb)(struct dpaa_bp *);
	 
	void (*free_buf_cb)(const struct dpaa_bp *, struct bm_buffer *);
	refcount_t refs;
};

struct dpaa_rx_errors {
	u64 dme;		 
	u64 fpe;		 
	u64 fse;		 
	u64 phe;		 
};

 
struct dpaa_ern_cnt {
	u64 cg_tdrop;		 
	u64 wred;		 
	u64 err_cond;		 
	u64 early_window;	 
	u64 late_window;	 
	u64 fq_tdrop;		 
	u64 fq_retired;		 
	u64 orp_zero;		 
};

struct dpaa_napi_portal {
	struct napi_struct napi;
	struct qman_portal *p;
	bool down;
	int xdp_act;
};

struct dpaa_percpu_priv {
	struct net_device *net_dev;
	struct dpaa_napi_portal np;
	u64 in_interrupt;
	u64 tx_confirm;
	 
	u64 tx_frag_skbuffs;
	struct rtnl_link_stats64 stats;
	struct dpaa_rx_errors rx_errors;
	struct dpaa_ern_cnt ern_cnt;
};

struct dpaa_buffer_layout {
	u16 priv_data_size;
};

 
struct dpaa_eth_swbp {
	struct sk_buff *skb;
	struct xdp_frame *xdpf;
};

struct dpaa_priv {
	struct dpaa_percpu_priv __percpu *percpu_priv;
	struct dpaa_bp *dpaa_bp;
	 
	u16 tx_headroom;
	struct net_device *net_dev;
	struct mac_device *mac_dev;
	struct device *rx_dma_dev;
	struct device *tx_dma_dev;
	struct qman_fq *egress_fqs[DPAA_ETH_TXQ_NUM];
	struct qman_fq *conf_fqs[DPAA_ETH_TXQ_NUM];

	u16 channel;
	struct list_head dpaa_fq_list;

	u8 num_tc;
	bool keygen_in_use;
	u32 msg_enable;	 

	struct {
		 
		struct qman_cgr cgr;
		 
		u32 congestion_start_jiffies;
		 
		u32 congested_jiffies;
		 
		u32 cgr_congested_count;
	} cgr_data;
	 
	bool use_ingress_cgr;
	struct qman_cgr ingress_cgr;

	struct dpaa_buffer_layout buf_layout[2];
	u16 rx_headroom;

	bool tx_tstamp;  
	bool rx_tstamp;  

	struct bpf_prog *xdp_prog;
};

 
extern const struct ethtool_ops dpaa_ethtool_ops;

 
void dpaa_eth_sysfs_remove(struct device *dev);
void dpaa_eth_sysfs_init(struct device *dev);
#endif	 
