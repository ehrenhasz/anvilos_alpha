 
 

#ifndef HFI1_NETDEV_H
#define HFI1_NETDEV_H

#include "hfi.h"

#include <linux/netdevice.h>
#include <linux/xarray.h>

 
struct hfi1_netdev_rxq {
	struct napi_struct napi;
	struct hfi1_netdev_rx *rx;
	struct hfi1_ctxtdata *rcd;
};

 
#define HFI1_MAX_NETDEV_CTXTS   8

 
#define NUM_NETDEV_MAP_ENTRIES HFI1_MAX_NETDEV_CTXTS

 
struct hfi1_netdev_rx {
	struct net_device rx_napi;
	struct hfi1_devdata *dd;
	struct hfi1_netdev_rxq *rxq;
	int num_rx_q;
	int rmt_start;
	struct xarray dev_tbl;
	 
	atomic_t enabled;
	 
	atomic_t netdevs;
};

static inline
int hfi1_netdev_ctxt_count(struct hfi1_devdata *dd)
{
	return dd->netdev_rx->num_rx_q;
}

static inline
struct hfi1_ctxtdata *hfi1_netdev_get_ctxt(struct hfi1_devdata *dd, int ctxt)
{
	return dd->netdev_rx->rxq[ctxt].rcd;
}

static inline
int hfi1_netdev_get_free_rmt_idx(struct hfi1_devdata *dd)
{
	return dd->netdev_rx->rmt_start;
}

static inline
void hfi1_netdev_set_free_rmt_idx(struct hfi1_devdata *dd, int rmt_idx)
{
	dd->netdev_rx->rmt_start = rmt_idx;
}

u32 hfi1_num_netdev_contexts(struct hfi1_devdata *dd, u32 available_contexts,
			     struct cpumask *cpu_mask);

void hfi1_netdev_enable_queues(struct hfi1_devdata *dd);
void hfi1_netdev_disable_queues(struct hfi1_devdata *dd);
int hfi1_netdev_rx_init(struct hfi1_devdata *dd);
int hfi1_netdev_rx_destroy(struct hfi1_devdata *dd);
int hfi1_alloc_rx(struct hfi1_devdata *dd);
void hfi1_free_rx(struct hfi1_devdata *dd);
int hfi1_netdev_add_data(struct hfi1_devdata *dd, int id, void *data);
void *hfi1_netdev_remove_data(struct hfi1_devdata *dd, int id);
void *hfi1_netdev_get_data(struct hfi1_devdata *dd, int id);
void *hfi1_netdev_get_first_data(struct hfi1_devdata *dd, int *start_id);

 
int hfi1_netdev_rx_napi(struct napi_struct *napi, int budget);

#endif  
