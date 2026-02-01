 
 

#ifndef EFX_EFX_H
#define EFX_EFX_H

#include <linux/indirect_call_wrapper.h>
#include "net_driver.h"
#include "filter.h"

 
void efx_siena_init_tx_queue_core_txq(struct efx_tx_queue *tx_queue);
netdev_tx_t efx_siena_hard_start_xmit(struct sk_buff *skb,
				      struct net_device *net_dev);
netdev_tx_t __efx_siena_enqueue_skb(struct efx_tx_queue *tx_queue,
				    struct sk_buff *skb);
static inline netdev_tx_t efx_enqueue_skb(struct efx_tx_queue *tx_queue, struct sk_buff *skb)
{
	return INDIRECT_CALL_1(tx_queue->efx->type->tx_enqueue,
			       __efx_siena_enqueue_skb, tx_queue, skb);
}
int efx_siena_setup_tc(struct net_device *net_dev, enum tc_setup_type type,
		       void *type_data);

 
void __efx_siena_rx_packet(struct efx_channel *channel);
void efx_siena_rx_packet(struct efx_rx_queue *rx_queue, unsigned int index,
			 unsigned int n_frags, unsigned int len, u16 flags);
static inline void efx_rx_flush_packet(struct efx_channel *channel)
{
	if (channel->rx_pkt_n_frags)
		__efx_siena_rx_packet(channel);
}

 
#define EFX_TSO_MAX_SEGS	100

 
#define EFX_RXQ_MIN_ENT		128U
#define EFX_TXQ_MIN_ENT(efx)	(2 * efx_siena_tx_max_skb_descs(efx))

 
#define EFX_TXQ_MAX_ENT(efx)	(EFX_WORKAROUND_EF10(efx) ? \
				 EFX_MAX_DMAQ_SIZE / 2 : EFX_MAX_DMAQ_SIZE)

static inline bool efx_rss_enabled(struct efx_nic *efx)
{
	return efx->rss_spread > 1;
}

 

 
static inline s32 efx_filter_insert_filter(struct efx_nic *efx,
					   struct efx_filter_spec *spec,
					   bool replace_equal)
{
	return efx->type->filter_insert(efx, spec, replace_equal);
}

 
static inline int efx_filter_remove_id_safe(struct efx_nic *efx,
					    enum efx_filter_priority priority,
					    u32 filter_id)
{
	return efx->type->filter_remove_safe(efx, priority, filter_id);
}

 
static inline int
efx_filter_get_filter_safe(struct efx_nic *efx,
			   enum efx_filter_priority priority,
			   u32 filter_id, struct efx_filter_spec *spec)
{
	return efx->type->filter_get_safe(efx, priority, filter_id, spec);
}

static inline u32 efx_filter_count_rx_used(struct efx_nic *efx,
					   enum efx_filter_priority priority)
{
	return efx->type->filter_count_rx_used(efx, priority);
}
static inline u32 efx_filter_get_rx_id_limit(struct efx_nic *efx)
{
	return efx->type->filter_get_rx_id_limit(efx);
}
static inline s32 efx_filter_get_rx_ids(struct efx_nic *efx,
					enum efx_filter_priority priority,
					u32 *buf, u32 size)
{
	return efx->type->filter_get_rx_ids(efx, priority, buf, size);
}

 
static inline bool efx_rss_active(struct efx_rss_context *ctx)
{
	return ctx->context_id != EFX_MCDI_RSS_CONTEXT_INVALID;
}

 
extern const struct ethtool_ops efx_siena_ethtool_ops;

 
unsigned int efx_siena_usecs_to_ticks(struct efx_nic *efx, unsigned int usecs);
int efx_siena_init_irq_moderation(struct efx_nic *efx, unsigned int tx_usecs,
				  unsigned int rx_usecs, bool rx_adaptive,
				  bool rx_may_override_tx);
void efx_siena_get_irq_moderation(struct efx_nic *efx, unsigned int *tx_usecs,
				  unsigned int *rx_usecs, bool *rx_adaptive);

 
void efx_siena_update_sw_stats(struct efx_nic *efx, u64 *stats);

 
#ifdef CONFIG_SFC_SIENA_MTD
int efx_siena_mtd_add(struct efx_nic *efx, struct efx_mtd_partition *parts,
		      size_t n_parts, size_t sizeof_part);
static inline int efx_mtd_probe(struct efx_nic *efx)
{
	return efx->type->mtd_probe(efx);
}
void efx_siena_mtd_rename(struct efx_nic *efx);
void efx_siena_mtd_remove(struct efx_nic *efx);
#else
static inline int efx_mtd_probe(struct efx_nic *efx) { return 0; }
static inline void efx_siena_mtd_rename(struct efx_nic *efx) {}
static inline void efx_siena_mtd_remove(struct efx_nic *efx) {}
#endif

#ifdef CONFIG_SFC_SIENA_SRIOV
static inline unsigned int efx_vf_size(struct efx_nic *efx)
{
	return 1 << efx->vi_scale;
}
#endif

static inline void efx_device_detach_sync(struct efx_nic *efx)
{
	struct net_device *dev = efx->net_dev;

	 
	netif_tx_lock_bh(dev);
	netif_device_detach(dev);
	netif_tx_unlock_bh(dev);
}

static inline void efx_device_attach_if_not_resetting(struct efx_nic *efx)
{
	if ((efx->state != STATE_DISABLED) && !efx->reset_pending)
		netif_device_attach(efx->net_dev);
}

static inline bool efx_rwsem_assert_write_locked(struct rw_semaphore *sem)
{
	if (WARN_ON(down_read_trylock(sem))) {
		up_read(sem);
		return false;
	}
	return true;
}

int efx_siena_xdp_tx_buffers(struct efx_nic *efx, int n,
			     struct xdp_frame **xdpfs, bool flush);

#endif  
