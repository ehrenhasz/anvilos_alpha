 
 

#ifndef _HFI1_VNIC_H
#define _HFI1_VNIC_H
#include <rdma/opa_vnic.h>
#include "hfi.h"
#include "sdma.h"

#define HFI1_VNIC_MAX_TXQ     16
#define HFI1_VNIC_MAX_PAD     12

 
#define HFI1_VNIC_L4_HDR_OFFSET  OPA_VNIC_L2_HDR_LEN

#define HFI1_VNIC_GET_L4_HDR(data)   \
	(*((u16 *)((u8 *)(data) + HFI1_VNIC_L4_HDR_OFFSET)))

#define HFI1_VNIC_GET_VESWID(data)   \
	(HFI1_VNIC_GET_L4_HDR(data) & 0xFFF)

 
#define HFI1_VNIC_SC_OFFSET_LOW 6
#define HFI1_VNIC_SC_OFFSET_HI  7
#define HFI1_VNIC_SC_SHIFT      4

#define HFI1_VNIC_MAX_QUEUE 16
#define HFI1_NUM_VNIC_CTXT 8

 
struct hfi1_vnic_sdma {
	struct hfi1_devdata *dd;
	struct sdma_engine  *sde;
	struct hfi1_vnic_vport_info *vinfo;
	struct iowait wait;
	struct sdma_txreq stx;
	unsigned int state;
	u8 q_idx;
	bool pkts_sent;
};

 
struct hfi1_vnic_rx_queue {
	u8                           idx;
	struct hfi1_vnic_vport_info *vinfo;
	struct net_device           *netdev;
	struct napi_struct           napi;
};

 
struct hfi1_vnic_vport_info {
	struct hfi1_devdata *dd;
	struct net_device   *netdev;
	unsigned long        flags;

	 
	struct mutex         lock;

	u8  num_tx_q;
	u8  num_rx_q;
	u16 vesw_id;
	struct hfi1_vnic_rx_queue rxq[HFI1_NUM_VNIC_CTXT];

	struct opa_vnic_stats  stats[HFI1_VNIC_MAX_QUEUE];
	struct hfi1_vnic_sdma  sdma[HFI1_VNIC_MAX_TXQ];
};

#define v_dbg(format, arg...) \
	netdev_dbg(vinfo->netdev, format, ## arg)
#define v_err(format, arg...) \
	netdev_err(vinfo->netdev, format, ## arg)
#define v_info(format, arg...) \
	netdev_info(vinfo->netdev, format, ## arg)

 
void hfi1_vnic_setup(struct hfi1_devdata *dd);
int hfi1_vnic_txreq_init(struct hfi1_devdata *dd);
void hfi1_vnic_txreq_deinit(struct hfi1_devdata *dd);

void hfi1_vnic_bypass_rcv(struct hfi1_packet *packet);
void hfi1_vnic_sdma_init(struct hfi1_vnic_vport_info *vinfo);
bool hfi1_vnic_sdma_write_avail(struct hfi1_vnic_vport_info *vinfo,
				u8 q_idx);

 
struct net_device *hfi1_vnic_alloc_rn(struct ib_device *device,
				      u32 port_num,
				      enum rdma_netdev_t type,
				      const char *name,
				      unsigned char name_assign_type,
				      void (*setup)(struct net_device *));
int hfi1_vnic_send_dma(struct hfi1_devdata *dd, u8 q_idx,
		       struct hfi1_vnic_vport_info *vinfo,
		       struct sk_buff *skb, u64 pbc, u8 plen);

#endif  
