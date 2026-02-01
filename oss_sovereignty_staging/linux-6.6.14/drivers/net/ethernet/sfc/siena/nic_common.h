 
 

#ifndef EFX_NIC_COMMON_H
#define EFX_NIC_COMMON_H

#include "net_driver.h"
#include "efx_common.h"
#include "mcdi.h"
#include "ptp.h"

enum {
	 
	EFX_REV_SIENA_A0 = 3,
	EFX_REV_HUNT_A0 = 4,
	EFX_REV_EF100 = 5,
};

static inline int efx_nic_rev(struct efx_nic *efx)
{
	return efx->type->revision;
}

 
static inline efx_qword_t *efx_event(struct efx_channel *channel,
				     unsigned int index)
{
	return ((efx_qword_t *) (channel->eventq.buf.addr)) +
		(index & channel->eventq_mask);
}

 
static inline int efx_event_present(efx_qword_t *event)
{
	return !(EFX_DWORD_IS_ALL_ONES(event->dword[0]) |
		  EFX_DWORD_IS_ALL_ONES(event->dword[1]));
}

 
static inline efx_qword_t *
efx_tx_desc(struct efx_tx_queue *tx_queue, unsigned int index)
{
	return ((efx_qword_t *) (tx_queue->txd.buf.addr)) + index;
}

 
static inline bool efx_nic_tx_is_empty(struct efx_tx_queue *tx_queue, unsigned int write_count)
{
	unsigned int empty_read_count = READ_ONCE(tx_queue->empty_read_count);

	if (empty_read_count == 0)
		return false;

	return ((empty_read_count ^ write_count) & ~EFX_EMPTY_COUNT_VALID) == 0;
}

 
static inline bool efx_nic_may_push_tx_desc(struct efx_tx_queue *tx_queue,
					    unsigned int write_count)
{
	bool was_empty = efx_nic_tx_is_empty(tx_queue, write_count);

	tx_queue->empty_read_count = 0;
	return was_empty && tx_queue->write_count - write_count == 1;
}

 
static inline efx_qword_t *
efx_rx_desc(struct efx_rx_queue *rx_queue, unsigned int index)
{
	return ((efx_qword_t *) (rx_queue->rxd.buf.addr)) + index;
}

 
#define EFX_PAGE_SIZE	4096
 
#define EFX_BUF_SIZE	EFX_PAGE_SIZE

 
enum {
	GENERIC_STAT_rx_noskb_drops,
	GENERIC_STAT_rx_nodesc_trunc,
	GENERIC_STAT_COUNT
};

#define EFX_GENERIC_SW_STAT(ext_name)				\
	[GENERIC_STAT_ ## ext_name] = { #ext_name, 0, 0 }

 
static inline int efx_nic_probe_tx(struct efx_tx_queue *tx_queue)
{
	return tx_queue->efx->type->tx_probe(tx_queue);
}
static inline void efx_nic_init_tx(struct efx_tx_queue *tx_queue)
{
	tx_queue->efx->type->tx_init(tx_queue);
}
static inline void efx_nic_remove_tx(struct efx_tx_queue *tx_queue)
{
	if (tx_queue->efx->type->tx_remove)
		tx_queue->efx->type->tx_remove(tx_queue);
}
static inline void efx_nic_push_buffers(struct efx_tx_queue *tx_queue)
{
	tx_queue->efx->type->tx_write(tx_queue);
}

 
static inline int efx_nic_probe_rx(struct efx_rx_queue *rx_queue)
{
	return rx_queue->efx->type->rx_probe(rx_queue);
}
static inline void efx_nic_init_rx(struct efx_rx_queue *rx_queue)
{
	rx_queue->efx->type->rx_init(rx_queue);
}
static inline void efx_nic_remove_rx(struct efx_rx_queue *rx_queue)
{
	rx_queue->efx->type->rx_remove(rx_queue);
}
static inline void efx_nic_notify_rx_desc(struct efx_rx_queue *rx_queue)
{
	rx_queue->efx->type->rx_write(rx_queue);
}
static inline void efx_nic_generate_fill_event(struct efx_rx_queue *rx_queue)
{
	rx_queue->efx->type->rx_defer_refill(rx_queue);
}

 
static inline int efx_nic_probe_eventq(struct efx_channel *channel)
{
	return channel->efx->type->ev_probe(channel);
}
static inline int efx_nic_init_eventq(struct efx_channel *channel)
{
	return channel->efx->type->ev_init(channel);
}
static inline void efx_nic_fini_eventq(struct efx_channel *channel)
{
	channel->efx->type->ev_fini(channel);
}
static inline void efx_nic_remove_eventq(struct efx_channel *channel)
{
	channel->efx->type->ev_remove(channel);
}
static inline int
efx_nic_process_eventq(struct efx_channel *channel, int quota)
{
	return channel->efx->type->ev_process(channel, quota);
}
static inline void efx_nic_eventq_read_ack(struct efx_channel *channel)
{
	channel->efx->type->ev_read_ack(channel);
}

void efx_siena_event_test_start(struct efx_channel *channel);

bool efx_siena_event_present(struct efx_channel *channel);

static inline void efx_sensor_event(struct efx_nic *efx, efx_qword_t *ev)
{
	if (efx->type->sensor_event)
		efx->type->sensor_event(efx, ev);
}

static inline unsigned int efx_rx_recycle_ring_size(const struct efx_nic *efx)
{
	return efx->type->rx_recycle_ring_size(efx);
}

 
static inline void efx_update_diff_stat(u64 *stat, u64 diff)
{
	if ((s64)(diff - *stat) > 0)
		*stat = diff;
}

 
int efx_siena_init_interrupt(struct efx_nic *efx);
int efx_siena_irq_test_start(struct efx_nic *efx);
void efx_siena_fini_interrupt(struct efx_nic *efx);

static inline int efx_nic_event_test_irq_cpu(struct efx_channel *channel)
{
	return READ_ONCE(channel->event_test_cpu);
}
static inline int efx_nic_irq_test_irq_cpu(struct efx_nic *efx)
{
	return READ_ONCE(efx->last_irq_cpu);
}

 
int efx_siena_alloc_buffer(struct efx_nic *efx, struct efx_buffer *buffer,
			   unsigned int len, gfp_t gfp_flags);
void efx_siena_free_buffer(struct efx_nic *efx, struct efx_buffer *buffer);

size_t efx_siena_get_regs_len(struct efx_nic *efx);
void efx_siena_get_regs(struct efx_nic *efx, void *buf);

#define EFX_MC_STATS_GENERATION_INVALID ((__force __le64)(-1))

size_t efx_siena_describe_stats(const struct efx_hw_stat_desc *desc, size_t count,
				const unsigned long *mask, u8 *names);
void efx_siena_update_stats(const struct efx_hw_stat_desc *desc, size_t count,
			    const unsigned long *mask, u64 *stats,
			    const void *dma_buf, bool accumulate);
void efx_siena_fix_nodesc_drop_stat(struct efx_nic *efx, u64 *stat);

#define EFX_MAX_FLUSH_TIME 5000

#endif  
