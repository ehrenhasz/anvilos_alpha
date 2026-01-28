#ifndef HFI1_IPOIB_H
#define HFI1_IPOIB_H
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/atomic.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/if_infiniband.h>
#include "hfi.h"
#include "iowait.h"
#include "netdev.h"
#include <rdma/ib_verbs.h>
#define HFI1_IPOIB_ENTROPY_SHIFT   24
#define HFI1_IPOIB_TXREQ_NAME_LEN   32
#define HFI1_IPOIB_PSEUDO_LEN 20
#define HFI1_IPOIB_ENCAP_LEN 4
struct hfi1_ipoib_dev_priv;
union hfi1_ipoib_flow {
	u16 as_int;
	struct {
		u8 tx_queue;
		u8 sc5;
	} __attribute__((__packed__));
};
struct ipoib_txreq {
	struct sdma_txreq           txreq;
	struct hfi1_sdma_header     *sdma_hdr;
	int                         sdma_status;
	int                         complete;
	struct hfi1_ipoib_dev_priv *priv;
	struct hfi1_ipoib_txq      *txq;
	struct sk_buff             *skb;
};
struct hfi1_ipoib_circ_buf {
	void *items;
	u32 max_items;
	u32 shift;
	u64 ____cacheline_aligned_in_smp sent_txreqs;
	u32 avail;
	u32 tail;
	atomic_t stops;
	atomic_t ring_full;
	atomic_t no_desc;
	u64 ____cacheline_aligned_in_smp complete_txreqs;
	u32 head;
};
struct hfi1_ipoib_txq {
	struct napi_struct napi;
	struct hfi1_ipoib_dev_priv *priv;
	struct sdma_engine *sde;
	struct list_head tx_list;
	union hfi1_ipoib_flow flow;
	u8 q_idx;
	bool pkts_sent;
	struct iowait wait;
	struct hfi1_ipoib_circ_buf ____cacheline_aligned_in_smp tx_ring;
};
struct hfi1_ipoib_dev_priv {
	struct hfi1_devdata *dd;
	struct net_device   *netdev;
	struct ib_device    *device;
	struct hfi1_ipoib_txq *txqs;
	const struct net_device_ops *netdev_ops;
	struct rvt_qp *qp;
	u32 qkey;
	u16 pkey;
	u16 pkey_index;
	u8 port_num;
};
struct hfi1_ipoib_rdma_netdev {
	struct rdma_netdev rn;   
	struct hfi1_ipoib_dev_priv dev_priv;
};
static inline struct hfi1_ipoib_dev_priv *
hfi1_ipoib_priv(const struct net_device *dev)
{
	return &((struct hfi1_ipoib_rdma_netdev *)netdev_priv(dev))->dev_priv;
}
int hfi1_ipoib_send(struct net_device *dev,
		    struct sk_buff *skb,
		    struct ib_ah *address,
		    u32 dqpn);
int hfi1_ipoib_txreq_init(struct hfi1_ipoib_dev_priv *priv);
void hfi1_ipoib_txreq_deinit(struct hfi1_ipoib_dev_priv *priv);
int hfi1_ipoib_rxq_init(struct net_device *dev);
void hfi1_ipoib_rxq_deinit(struct net_device *dev);
void hfi1_ipoib_napi_tx_enable(struct net_device *dev);
void hfi1_ipoib_napi_tx_disable(struct net_device *dev);
struct sk_buff *hfi1_ipoib_prepare_skb(struct hfi1_netdev_rxq *rxq,
				       int size, void *data);
int hfi1_ipoib_rn_get_params(struct ib_device *device,
			     u32 port_num,
			     enum rdma_netdev_t type,
			     struct rdma_netdev_alloc_params *params);
void hfi1_ipoib_tx_timeout(struct net_device *dev, unsigned int q);
#endif  
