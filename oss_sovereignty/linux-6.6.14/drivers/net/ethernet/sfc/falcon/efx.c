
 

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/ethtool.h>
#include <linux/topology.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include "net_driver.h"
#include "efx.h"
#include "nic.h"
#include "selftest.h"

#include "workarounds.h"

 

 
const unsigned int ef4_loopback_mode_max = LOOPBACK_MAX;
const char *const ef4_loopback_mode_names[] = {
	[LOOPBACK_NONE]		= "NONE",
	[LOOPBACK_DATA]		= "DATAPATH",
	[LOOPBACK_GMAC]		= "GMAC",
	[LOOPBACK_XGMII]	= "XGMII",
	[LOOPBACK_XGXS]		= "XGXS",
	[LOOPBACK_XAUI]		= "XAUI",
	[LOOPBACK_GMII]		= "GMII",
	[LOOPBACK_SGMII]	= "SGMII",
	[LOOPBACK_XGBR]		= "XGBR",
	[LOOPBACK_XFI]		= "XFI",
	[LOOPBACK_XAUI_FAR]	= "XAUI_FAR",
	[LOOPBACK_GMII_FAR]	= "GMII_FAR",
	[LOOPBACK_SGMII_FAR]	= "SGMII_FAR",
	[LOOPBACK_XFI_FAR]	= "XFI_FAR",
	[LOOPBACK_GPHY]		= "GPHY",
	[LOOPBACK_PHYXS]	= "PHYXS",
	[LOOPBACK_PCS]		= "PCS",
	[LOOPBACK_PMAPMD]	= "PMA/PMD",
	[LOOPBACK_XPORT]	= "XPORT",
	[LOOPBACK_XGMII_WS]	= "XGMII_WS",
	[LOOPBACK_XAUI_WS]	= "XAUI_WS",
	[LOOPBACK_XAUI_WS_FAR]  = "XAUI_WS_FAR",
	[LOOPBACK_XAUI_WS_NEAR] = "XAUI_WS_NEAR",
	[LOOPBACK_GMII_WS]	= "GMII_WS",
	[LOOPBACK_XFI_WS]	= "XFI_WS",
	[LOOPBACK_XFI_WS_FAR]	= "XFI_WS_FAR",
	[LOOPBACK_PHYXS_WS]	= "PHYXS_WS",
};

const unsigned int ef4_reset_type_max = RESET_TYPE_MAX;
const char *const ef4_reset_type_names[] = {
	[RESET_TYPE_INVISIBLE]          = "INVISIBLE",
	[RESET_TYPE_ALL]                = "ALL",
	[RESET_TYPE_RECOVER_OR_ALL]     = "RECOVER_OR_ALL",
	[RESET_TYPE_WORLD]              = "WORLD",
	[RESET_TYPE_RECOVER_OR_DISABLE] = "RECOVER_OR_DISABLE",
	[RESET_TYPE_DATAPATH]           = "DATAPATH",
	[RESET_TYPE_DISABLE]            = "DISABLE",
	[RESET_TYPE_TX_WATCHDOG]        = "TX_WATCHDOG",
	[RESET_TYPE_INT_ERROR]          = "INT_ERROR",
	[RESET_TYPE_RX_RECOVERY]        = "RX_RECOVERY",
	[RESET_TYPE_DMA_ERROR]          = "DMA_ERROR",
	[RESET_TYPE_TX_SKIP]            = "TX_SKIP",
};

 
static struct workqueue_struct *reset_workqueue;

 
#define BIST_WAIT_DELAY_MS	100
#define BIST_WAIT_DELAY_COUNT	100

 

 
bool ef4_separate_tx_channels;
module_param(ef4_separate_tx_channels, bool, 0444);
MODULE_PARM_DESC(ef4_separate_tx_channels,
		 "Use separate channels for TX and RX");

 
static unsigned int ef4_monitor_interval = 1 * HZ;

 
static unsigned int rx_irq_mod_usec = 60;

 
static unsigned int tx_irq_mod_usec = 150;

 
static unsigned int interrupt_mode;

 
static unsigned int rss_cpus;
module_param(rss_cpus, uint, 0444);
MODULE_PARM_DESC(rss_cpus, "Number of CPUs to use for Receive-Side Scaling");

static bool phy_flash_cfg;
module_param(phy_flash_cfg, bool, 0644);
MODULE_PARM_DESC(phy_flash_cfg, "Set PHYs into reflash mode initially");

static unsigned irq_adapt_low_thresh = 8000;
module_param(irq_adapt_low_thresh, uint, 0644);
MODULE_PARM_DESC(irq_adapt_low_thresh,
		 "Threshold score for reducing IRQ moderation");

static unsigned irq_adapt_high_thresh = 16000;
module_param(irq_adapt_high_thresh, uint, 0644);
MODULE_PARM_DESC(irq_adapt_high_thresh,
		 "Threshold score for increasing IRQ moderation");

static unsigned debug = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
			 NETIF_MSG_LINK | NETIF_MSG_IFDOWN |
			 NETIF_MSG_IFUP | NETIF_MSG_RX_ERR |
			 NETIF_MSG_TX_ERR | NETIF_MSG_HW);
module_param(debug, uint, 0);
MODULE_PARM_DESC(debug, "Bitmapped debugging message enable value");

 

static int ef4_soft_enable_interrupts(struct ef4_nic *efx);
static void ef4_soft_disable_interrupts(struct ef4_nic *efx);
static void ef4_remove_channel(struct ef4_channel *channel);
static void ef4_remove_channels(struct ef4_nic *efx);
static const struct ef4_channel_type ef4_default_channel_type;
static void ef4_remove_port(struct ef4_nic *efx);
static void ef4_init_napi_channel(struct ef4_channel *channel);
static void ef4_fini_napi(struct ef4_nic *efx);
static void ef4_fini_napi_channel(struct ef4_channel *channel);
static void ef4_fini_struct(struct ef4_nic *efx);
static void ef4_start_all(struct ef4_nic *efx);
static void ef4_stop_all(struct ef4_nic *efx);

#define EF4_ASSERT_RESET_SERIALISED(efx)		\
	do {						\
		if ((efx->state == STATE_READY) ||	\
		    (efx->state == STATE_RECOVERY) ||	\
		    (efx->state == STATE_DISABLED))	\
			ASSERT_RTNL();			\
	} while (0)

static int ef4_check_disabled(struct ef4_nic *efx)
{
	if (efx->state == STATE_DISABLED || efx->state == STATE_RECOVERY) {
		netif_err(efx, drv, efx->net_dev,
			  "device is disabled due to earlier errors\n");
		return -EIO;
	}
	return 0;
}

 

 
static int ef4_process_channel(struct ef4_channel *channel, int budget)
{
	struct ef4_tx_queue *tx_queue;
	int spent;

	if (unlikely(!channel->enabled))
		return 0;

	ef4_for_each_channel_tx_queue(tx_queue, channel) {
		tx_queue->pkts_compl = 0;
		tx_queue->bytes_compl = 0;
	}

	spent = ef4_nic_process_eventq(channel, budget);
	if (spent && ef4_channel_has_rx_queue(channel)) {
		struct ef4_rx_queue *rx_queue =
			ef4_channel_get_rx_queue(channel);

		ef4_rx_flush_packet(channel);
		ef4_fast_push_rx_descriptors(rx_queue, true);
	}

	 
	ef4_for_each_channel_tx_queue(tx_queue, channel) {
		if (tx_queue->bytes_compl) {
			netdev_tx_completed_queue(tx_queue->core_txq,
				tx_queue->pkts_compl, tx_queue->bytes_compl);
		}
	}

	return spent;
}

 
static void ef4_update_irq_mod(struct ef4_nic *efx, struct ef4_channel *channel)
{
	int step = efx->irq_mod_step_us;

	if (channel->irq_mod_score < irq_adapt_low_thresh) {
		if (channel->irq_moderation_us > step) {
			channel->irq_moderation_us -= step;
			efx->type->push_irq_moderation(channel);
		}
	} else if (channel->irq_mod_score > irq_adapt_high_thresh) {
		if (channel->irq_moderation_us <
		    efx->irq_rx_moderation_us) {
			channel->irq_moderation_us += step;
			efx->type->push_irq_moderation(channel);
		}
	}

	channel->irq_count = 0;
	channel->irq_mod_score = 0;
}

static int ef4_poll(struct napi_struct *napi, int budget)
{
	struct ef4_channel *channel =
		container_of(napi, struct ef4_channel, napi_str);
	struct ef4_nic *efx = channel->efx;
	int spent;

	netif_vdbg(efx, intr, efx->net_dev,
		   "channel %d NAPI poll executing on CPU %d\n",
		   channel->channel, raw_smp_processor_id());

	spent = ef4_process_channel(channel, budget);

	if (spent < budget) {
		if (ef4_channel_has_rx_queue(channel) &&
		    efx->irq_rx_adaptive &&
		    unlikely(++channel->irq_count == 1000)) {
			ef4_update_irq_mod(efx, channel);
		}

		ef4_filter_rfs_expire(channel);

		 
		napi_complete_done(napi, spent);
		ef4_nic_eventq_read_ack(channel);
	}

	return spent;
}

 
static int ef4_probe_eventq(struct ef4_channel *channel)
{
	struct ef4_nic *efx = channel->efx;
	unsigned long entries;

	netif_dbg(efx, probe, efx->net_dev,
		  "chan %d create event queue\n", channel->channel);

	 
	entries = roundup_pow_of_two(efx->rxq_entries + efx->txq_entries + 128);
	EF4_BUG_ON_PARANOID(entries > EF4_MAX_EVQ_SIZE);
	channel->eventq_mask = max(entries, EF4_MIN_EVQ_SIZE) - 1;

	return ef4_nic_probe_eventq(channel);
}

 
static int ef4_init_eventq(struct ef4_channel *channel)
{
	struct ef4_nic *efx = channel->efx;
	int rc;

	EF4_WARN_ON_PARANOID(channel->eventq_init);

	netif_dbg(efx, drv, efx->net_dev,
		  "chan %d init event queue\n", channel->channel);

	rc = ef4_nic_init_eventq(channel);
	if (rc == 0) {
		efx->type->push_irq_moderation(channel);
		channel->eventq_read_ptr = 0;
		channel->eventq_init = true;
	}
	return rc;
}

 
void ef4_start_eventq(struct ef4_channel *channel)
{
	netif_dbg(channel->efx, ifup, channel->efx->net_dev,
		  "chan %d start event queue\n", channel->channel);

	 
	channel->enabled = true;
	smp_wmb();

	napi_enable(&channel->napi_str);
	ef4_nic_eventq_read_ack(channel);
}

 
void ef4_stop_eventq(struct ef4_channel *channel)
{
	if (!channel->enabled)
		return;

	napi_disable(&channel->napi_str);
	channel->enabled = false;
}

static void ef4_fini_eventq(struct ef4_channel *channel)
{
	if (!channel->eventq_init)
		return;

	netif_dbg(channel->efx, drv, channel->efx->net_dev,
		  "chan %d fini event queue\n", channel->channel);

	ef4_nic_fini_eventq(channel);
	channel->eventq_init = false;
}

static void ef4_remove_eventq(struct ef4_channel *channel)
{
	netif_dbg(channel->efx, drv, channel->efx->net_dev,
		  "chan %d remove event queue\n", channel->channel);

	ef4_nic_remove_eventq(channel);
}

 

 
static struct ef4_channel *
ef4_alloc_channel(struct ef4_nic *efx, int i, struct ef4_channel *old_channel)
{
	struct ef4_channel *channel;
	struct ef4_rx_queue *rx_queue;
	struct ef4_tx_queue *tx_queue;
	int j;

	channel = kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel)
		return NULL;

	channel->efx = efx;
	channel->channel = i;
	channel->type = &ef4_default_channel_type;

	for (j = 0; j < EF4_TXQ_TYPES; j++) {
		tx_queue = &channel->tx_queue[j];
		tx_queue->efx = efx;
		tx_queue->queue = i * EF4_TXQ_TYPES + j;
		tx_queue->channel = channel;
	}

	rx_queue = &channel->rx_queue;
	rx_queue->efx = efx;
	timer_setup(&rx_queue->slow_fill, ef4_rx_slow_fill, 0);

	return channel;
}

 
static struct ef4_channel *
ef4_copy_channel(const struct ef4_channel *old_channel)
{
	struct ef4_channel *channel;
	struct ef4_rx_queue *rx_queue;
	struct ef4_tx_queue *tx_queue;
	int j;

	channel = kmalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel)
		return NULL;

	*channel = *old_channel;

	channel->napi_dev = NULL;
	INIT_HLIST_NODE(&channel->napi_str.napi_hash_node);
	channel->napi_str.napi_id = 0;
	channel->napi_str.state = 0;
	memset(&channel->eventq, 0, sizeof(channel->eventq));

	for (j = 0; j < EF4_TXQ_TYPES; j++) {
		tx_queue = &channel->tx_queue[j];
		if (tx_queue->channel)
			tx_queue->channel = channel;
		tx_queue->buffer = NULL;
		memset(&tx_queue->txd, 0, sizeof(tx_queue->txd));
	}

	rx_queue = &channel->rx_queue;
	rx_queue->buffer = NULL;
	memset(&rx_queue->rxd, 0, sizeof(rx_queue->rxd));
	timer_setup(&rx_queue->slow_fill, ef4_rx_slow_fill, 0);

	return channel;
}

static int ef4_probe_channel(struct ef4_channel *channel)
{
	struct ef4_tx_queue *tx_queue;
	struct ef4_rx_queue *rx_queue;
	int rc;

	netif_dbg(channel->efx, probe, channel->efx->net_dev,
		  "creating channel %d\n", channel->channel);

	rc = channel->type->pre_probe(channel);
	if (rc)
		goto fail;

	rc = ef4_probe_eventq(channel);
	if (rc)
		goto fail;

	ef4_for_each_channel_tx_queue(tx_queue, channel) {
		rc = ef4_probe_tx_queue(tx_queue);
		if (rc)
			goto fail;
	}

	ef4_for_each_channel_rx_queue(rx_queue, channel) {
		rc = ef4_probe_rx_queue(rx_queue);
		if (rc)
			goto fail;
	}

	return 0;

fail:
	ef4_remove_channel(channel);
	return rc;
}

static void
ef4_get_channel_name(struct ef4_channel *channel, char *buf, size_t len)
{
	struct ef4_nic *efx = channel->efx;
	const char *type;
	int number;

	number = channel->channel;
	if (efx->tx_channel_offset == 0) {
		type = "";
	} else if (channel->channel < efx->tx_channel_offset) {
		type = "-rx";
	} else {
		type = "-tx";
		number -= efx->tx_channel_offset;
	}
	snprintf(buf, len, "%s%s-%d", efx->name, type, number);
}

static void ef4_set_channel_names(struct ef4_nic *efx)
{
	struct ef4_channel *channel;

	ef4_for_each_channel(channel, efx)
		channel->type->get_name(channel,
					efx->msi_context[channel->channel].name,
					sizeof(efx->msi_context[0].name));
}

static int ef4_probe_channels(struct ef4_nic *efx)
{
	struct ef4_channel *channel;
	int rc;

	 
	efx->next_buffer_table = 0;

	 
	ef4_for_each_channel_rev(channel, efx) {
		rc = ef4_probe_channel(channel);
		if (rc) {
			netif_err(efx, probe, efx->net_dev,
				  "failed to create channel %d\n",
				  channel->channel);
			goto fail;
		}
	}
	ef4_set_channel_names(efx);

	return 0;

fail:
	ef4_remove_channels(efx);
	return rc;
}

 
static void ef4_start_datapath(struct ef4_nic *efx)
{
	netdev_features_t old_features = efx->net_dev->features;
	bool old_rx_scatter = efx->rx_scatter;
	struct ef4_tx_queue *tx_queue;
	struct ef4_rx_queue *rx_queue;
	struct ef4_channel *channel;
	size_t rx_buf_len;

	 
	efx->rx_dma_len = (efx->rx_prefix_size +
			   EF4_MAX_FRAME_LEN(efx->net_dev->mtu) +
			   efx->type->rx_buffer_padding);
	rx_buf_len = (sizeof(struct ef4_rx_page_state) +
		      efx->rx_ip_align + efx->rx_dma_len);
	if (rx_buf_len <= PAGE_SIZE) {
		efx->rx_scatter = efx->type->always_rx_scatter;
		efx->rx_buffer_order = 0;
	} else if (efx->type->can_rx_scatter) {
		BUILD_BUG_ON(EF4_RX_USR_BUF_SIZE % L1_CACHE_BYTES);
		BUILD_BUG_ON(sizeof(struct ef4_rx_page_state) +
			     2 * ALIGN(NET_IP_ALIGN + EF4_RX_USR_BUF_SIZE,
				       EF4_RX_BUF_ALIGNMENT) >
			     PAGE_SIZE);
		efx->rx_scatter = true;
		efx->rx_dma_len = EF4_RX_USR_BUF_SIZE;
		efx->rx_buffer_order = 0;
	} else {
		efx->rx_scatter = false;
		efx->rx_buffer_order = get_order(rx_buf_len);
	}

	ef4_rx_config_page_split(efx);
	if (efx->rx_buffer_order)
		netif_dbg(efx, drv, efx->net_dev,
			  "RX buf len=%u; page order=%u batch=%u\n",
			  efx->rx_dma_len, efx->rx_buffer_order,
			  efx->rx_pages_per_batch);
	else
		netif_dbg(efx, drv, efx->net_dev,
			  "RX buf len=%u step=%u bpp=%u; page batch=%u\n",
			  efx->rx_dma_len, efx->rx_page_buf_step,
			  efx->rx_bufs_per_page, efx->rx_pages_per_batch);

	 
	efx->net_dev->hw_features |= efx->net_dev->features;
	efx->net_dev->hw_features &= ~efx->fixed_features;
	efx->net_dev->features |= efx->fixed_features;
	if (efx->net_dev->features != old_features)
		netdev_features_change(efx->net_dev);

	 
	if (efx->rx_scatter != old_rx_scatter)
		efx->type->filter_update_rx_scatter(efx);

	 
	efx->txq_stop_thresh = efx->txq_entries - ef4_tx_max_skb_descs(efx);
	efx->txq_wake_thresh = efx->txq_stop_thresh / 2;

	 
	ef4_for_each_channel(channel, efx) {
		ef4_for_each_channel_tx_queue(tx_queue, channel) {
			ef4_init_tx_queue(tx_queue);
			atomic_inc(&efx->active_queues);
		}

		ef4_for_each_channel_rx_queue(rx_queue, channel) {
			ef4_init_rx_queue(rx_queue);
			atomic_inc(&efx->active_queues);
			ef4_stop_eventq(channel);
			ef4_fast_push_rx_descriptors(rx_queue, false);
			ef4_start_eventq(channel);
		}

		WARN_ON(channel->rx_pkt_n_frags);
	}

	if (netif_device_present(efx->net_dev))
		netif_tx_wake_all_queues(efx->net_dev);
}

static void ef4_stop_datapath(struct ef4_nic *efx)
{
	struct ef4_channel *channel;
	struct ef4_tx_queue *tx_queue;
	struct ef4_rx_queue *rx_queue;
	int rc;

	EF4_ASSERT_RESET_SERIALISED(efx);
	BUG_ON(efx->port_enabled);

	 
	ef4_for_each_channel(channel, efx) {
		ef4_for_each_channel_rx_queue(rx_queue, channel)
			rx_queue->refill_enabled = false;
	}

	ef4_for_each_channel(channel, efx) {
		 
		if (ef4_channel_has_rx_queue(channel)) {
			ef4_stop_eventq(channel);
			ef4_start_eventq(channel);
		}
	}

	rc = efx->type->fini_dmaq(efx);
	if (rc && EF4_WORKAROUND_7803(efx)) {
		 
		netif_err(efx, drv, efx->net_dev,
			  "Resetting to recover from flush failure\n");
		ef4_schedule_reset(efx, RESET_TYPE_ALL);
	} else if (rc) {
		netif_err(efx, drv, efx->net_dev, "failed to flush queues\n");
	} else {
		netif_dbg(efx, drv, efx->net_dev,
			  "successfully flushed all queues\n");
	}

	ef4_for_each_channel(channel, efx) {
		ef4_for_each_channel_rx_queue(rx_queue, channel)
			ef4_fini_rx_queue(rx_queue);
		ef4_for_each_possible_channel_tx_queue(tx_queue, channel)
			ef4_fini_tx_queue(tx_queue);
	}
}

static void ef4_remove_channel(struct ef4_channel *channel)
{
	struct ef4_tx_queue *tx_queue;
	struct ef4_rx_queue *rx_queue;

	netif_dbg(channel->efx, drv, channel->efx->net_dev,
		  "destroy chan %d\n", channel->channel);

	ef4_for_each_channel_rx_queue(rx_queue, channel)
		ef4_remove_rx_queue(rx_queue);
	ef4_for_each_possible_channel_tx_queue(tx_queue, channel)
		ef4_remove_tx_queue(tx_queue);
	ef4_remove_eventq(channel);
	channel->type->post_remove(channel);
}

static void ef4_remove_channels(struct ef4_nic *efx)
{
	struct ef4_channel *channel;

	ef4_for_each_channel(channel, efx)
		ef4_remove_channel(channel);
}

int
ef4_realloc_channels(struct ef4_nic *efx, u32 rxq_entries, u32 txq_entries)
{
	struct ef4_channel *other_channel[EF4_MAX_CHANNELS], *channel;
	u32 old_rxq_entries, old_txq_entries;
	unsigned i, next_buffer_table = 0;
	int rc, rc2;

	rc = ef4_check_disabled(efx);
	if (rc)
		return rc;

	 
	ef4_for_each_channel(channel, efx) {
		struct ef4_rx_queue *rx_queue;
		struct ef4_tx_queue *tx_queue;

		if (channel->type->copy)
			continue;
		next_buffer_table = max(next_buffer_table,
					channel->eventq.index +
					channel->eventq.entries);
		ef4_for_each_channel_rx_queue(rx_queue, channel)
			next_buffer_table = max(next_buffer_table,
						rx_queue->rxd.index +
						rx_queue->rxd.entries);
		ef4_for_each_channel_tx_queue(tx_queue, channel)
			next_buffer_table = max(next_buffer_table,
						tx_queue->txd.index +
						tx_queue->txd.entries);
	}

	ef4_device_detach_sync(efx);
	ef4_stop_all(efx);
	ef4_soft_disable_interrupts(efx);

	 
	memset(other_channel, 0, sizeof(other_channel));
	for (i = 0; i < efx->n_channels; i++) {
		channel = efx->channel[i];
		if (channel->type->copy)
			channel = channel->type->copy(channel);
		if (!channel) {
			rc = -ENOMEM;
			goto out;
		}
		other_channel[i] = channel;
	}

	 
	old_rxq_entries = efx->rxq_entries;
	old_txq_entries = efx->txq_entries;
	efx->rxq_entries = rxq_entries;
	efx->txq_entries = txq_entries;
	for (i = 0; i < efx->n_channels; i++) {
		swap(efx->channel[i], other_channel[i]);
	}

	 
	efx->next_buffer_table = next_buffer_table;

	for (i = 0; i < efx->n_channels; i++) {
		channel = efx->channel[i];
		if (!channel->type->copy)
			continue;
		rc = ef4_probe_channel(channel);
		if (rc)
			goto rollback;
		ef4_init_napi_channel(efx->channel[i]);
	}

out:
	 
	for (i = 0; i < efx->n_channels; i++) {
		channel = other_channel[i];
		if (channel && channel->type->copy) {
			ef4_fini_napi_channel(channel);
			ef4_remove_channel(channel);
			kfree(channel);
		}
	}

	rc2 = ef4_soft_enable_interrupts(efx);
	if (rc2) {
		rc = rc ? rc : rc2;
		netif_err(efx, drv, efx->net_dev,
			  "unable to restart interrupts on channel reallocation\n");
		ef4_schedule_reset(efx, RESET_TYPE_DISABLE);
	} else {
		ef4_start_all(efx);
		netif_device_attach(efx->net_dev);
	}
	return rc;

rollback:
	 
	efx->rxq_entries = old_rxq_entries;
	efx->txq_entries = old_txq_entries;
	for (i = 0; i < efx->n_channels; i++) {
		swap(efx->channel[i], other_channel[i]);
	}
	goto out;
}

void ef4_schedule_slow_fill(struct ef4_rx_queue *rx_queue)
{
	mod_timer(&rx_queue->slow_fill, jiffies + msecs_to_jiffies(100));
}

static const struct ef4_channel_type ef4_default_channel_type = {
	.pre_probe		= ef4_channel_dummy_op_int,
	.post_remove		= ef4_channel_dummy_op_void,
	.get_name		= ef4_get_channel_name,
	.copy			= ef4_copy_channel,
	.keep_eventq		= false,
};

int ef4_channel_dummy_op_int(struct ef4_channel *channel)
{
	return 0;
}

void ef4_channel_dummy_op_void(struct ef4_channel *channel)
{
}

 

 
void ef4_link_status_changed(struct ef4_nic *efx)
{
	struct ef4_link_state *link_state = &efx->link_state;

	 
	if (!netif_running(efx->net_dev))
		return;

	if (link_state->up != netif_carrier_ok(efx->net_dev)) {
		efx->n_link_state_changes++;

		if (link_state->up)
			netif_carrier_on(efx->net_dev);
		else
			netif_carrier_off(efx->net_dev);
	}

	 
	if (link_state->up)
		netif_info(efx, link, efx->net_dev,
			   "link up at %uMbps %s-duplex (MTU %d)\n",
			   link_state->speed, link_state->fd ? "full" : "half",
			   efx->net_dev->mtu);
	else
		netif_info(efx, link, efx->net_dev, "link down\n");
}

void ef4_link_set_advertising(struct ef4_nic *efx, u32 advertising)
{
	efx->link_advertising = advertising;
	if (advertising) {
		if (advertising & ADVERTISED_Pause)
			efx->wanted_fc |= (EF4_FC_TX | EF4_FC_RX);
		else
			efx->wanted_fc &= ~(EF4_FC_TX | EF4_FC_RX);
		if (advertising & ADVERTISED_Asym_Pause)
			efx->wanted_fc ^= EF4_FC_TX;
	}
}

void ef4_link_set_wanted_fc(struct ef4_nic *efx, u8 wanted_fc)
{
	efx->wanted_fc = wanted_fc;
	if (efx->link_advertising) {
		if (wanted_fc & EF4_FC_RX)
			efx->link_advertising |= (ADVERTISED_Pause |
						  ADVERTISED_Asym_Pause);
		else
			efx->link_advertising &= ~(ADVERTISED_Pause |
						   ADVERTISED_Asym_Pause);
		if (wanted_fc & EF4_FC_TX)
			efx->link_advertising ^= ADVERTISED_Asym_Pause;
	}
}

static void ef4_fini_port(struct ef4_nic *efx);

 
void ef4_mac_reconfigure(struct ef4_nic *efx)
{
	down_read(&efx->filter_sem);
	efx->type->reconfigure_mac(efx);
	up_read(&efx->filter_sem);
}

 
int __ef4_reconfigure_port(struct ef4_nic *efx)
{
	enum ef4_phy_mode phy_mode;
	int rc;

	WARN_ON(!mutex_is_locked(&efx->mac_lock));

	 
	phy_mode = efx->phy_mode;
	if (LOOPBACK_INTERNAL(efx))
		efx->phy_mode |= PHY_MODE_TX_DISABLED;
	else
		efx->phy_mode &= ~PHY_MODE_TX_DISABLED;

	rc = efx->type->reconfigure_port(efx);

	if (rc)
		efx->phy_mode = phy_mode;

	return rc;
}

 
int ef4_reconfigure_port(struct ef4_nic *efx)
{
	int rc;

	EF4_ASSERT_RESET_SERIALISED(efx);

	mutex_lock(&efx->mac_lock);
	rc = __ef4_reconfigure_port(efx);
	mutex_unlock(&efx->mac_lock);

	return rc;
}

 
static void ef4_mac_work(struct work_struct *data)
{
	struct ef4_nic *efx = container_of(data, struct ef4_nic, mac_work);

	mutex_lock(&efx->mac_lock);
	if (efx->port_enabled)
		ef4_mac_reconfigure(efx);
	mutex_unlock(&efx->mac_lock);
}

static int ef4_probe_port(struct ef4_nic *efx)
{
	int rc;

	netif_dbg(efx, probe, efx->net_dev, "create port\n");

	if (phy_flash_cfg)
		efx->phy_mode = PHY_MODE_SPECIAL;

	 
	rc = efx->type->probe_port(efx);
	if (rc)
		return rc;

	 
	eth_hw_addr_set(efx->net_dev, efx->net_dev->perm_addr);

	return 0;
}

static int ef4_init_port(struct ef4_nic *efx)
{
	int rc;

	netif_dbg(efx, drv, efx->net_dev, "init port\n");

	mutex_lock(&efx->mac_lock);

	rc = efx->phy_op->init(efx);
	if (rc)
		goto fail1;

	efx->port_initialized = true;

	 
	ef4_mac_reconfigure(efx);

	 
	rc = efx->phy_op->reconfigure(efx);
	if (rc && rc != -EPERM)
		goto fail2;

	mutex_unlock(&efx->mac_lock);
	return 0;

fail2:
	efx->phy_op->fini(efx);
fail1:
	mutex_unlock(&efx->mac_lock);
	return rc;
}

static void ef4_start_port(struct ef4_nic *efx)
{
	netif_dbg(efx, ifup, efx->net_dev, "start port\n");
	BUG_ON(efx->port_enabled);

	mutex_lock(&efx->mac_lock);
	efx->port_enabled = true;

	 
	ef4_mac_reconfigure(efx);

	mutex_unlock(&efx->mac_lock);
}

 
static void ef4_stop_port(struct ef4_nic *efx)
{
	netif_dbg(efx, ifdown, efx->net_dev, "stop port\n");

	EF4_ASSERT_RESET_SERIALISED(efx);

	mutex_lock(&efx->mac_lock);
	efx->port_enabled = false;
	mutex_unlock(&efx->mac_lock);

	 
	netif_addr_lock_bh(efx->net_dev);
	netif_addr_unlock_bh(efx->net_dev);

	cancel_delayed_work_sync(&efx->monitor_work);
	ef4_selftest_async_cancel(efx);
	cancel_work_sync(&efx->mac_work);
}

static void ef4_fini_port(struct ef4_nic *efx)
{
	netif_dbg(efx, drv, efx->net_dev, "shut down port\n");

	if (!efx->port_initialized)
		return;

	efx->phy_op->fini(efx);
	efx->port_initialized = false;

	efx->link_state.up = false;
	ef4_link_status_changed(efx);
}

static void ef4_remove_port(struct ef4_nic *efx)
{
	netif_dbg(efx, drv, efx->net_dev, "destroying port\n");

	efx->type->remove_port(efx);
}

 

static LIST_HEAD(ef4_primary_list);
static LIST_HEAD(ef4_unassociated_list);

static bool ef4_same_controller(struct ef4_nic *left, struct ef4_nic *right)
{
	return left->type == right->type &&
		left->vpd_sn && right->vpd_sn &&
		!strcmp(left->vpd_sn, right->vpd_sn);
}

static void ef4_associate(struct ef4_nic *efx)
{
	struct ef4_nic *other, *next;

	if (efx->primary == efx) {
		 

		netif_dbg(efx, probe, efx->net_dev, "adding to primary list\n");
		list_add_tail(&efx->node, &ef4_primary_list);

		list_for_each_entry_safe(other, next, &ef4_unassociated_list,
					 node) {
			if (ef4_same_controller(efx, other)) {
				list_del(&other->node);
				netif_dbg(other, probe, other->net_dev,
					  "moving to secondary list of %s %s\n",
					  pci_name(efx->pci_dev),
					  efx->net_dev->name);
				list_add_tail(&other->node,
					      &efx->secondary_list);
				other->primary = efx;
			}
		}
	} else {
		 

		list_for_each_entry(other, &ef4_primary_list, node) {
			if (ef4_same_controller(efx, other)) {
				netif_dbg(efx, probe, efx->net_dev,
					  "adding to secondary list of %s %s\n",
					  pci_name(other->pci_dev),
					  other->net_dev->name);
				list_add_tail(&efx->node,
					      &other->secondary_list);
				efx->primary = other;
				return;
			}
		}

		netif_dbg(efx, probe, efx->net_dev,
			  "adding to unassociated list\n");
		list_add_tail(&efx->node, &ef4_unassociated_list);
	}
}

static void ef4_dissociate(struct ef4_nic *efx)
{
	struct ef4_nic *other, *next;

	list_del(&efx->node);
	efx->primary = NULL;

	list_for_each_entry_safe(other, next, &efx->secondary_list, node) {
		list_del(&other->node);
		netif_dbg(other, probe, other->net_dev,
			  "moving to unassociated list\n");
		list_add_tail(&other->node, &ef4_unassociated_list);
		other->primary = NULL;
	}
}

 
static int ef4_init_io(struct ef4_nic *efx)
{
	struct pci_dev *pci_dev = efx->pci_dev;
	dma_addr_t dma_mask = efx->type->max_dma_mask;
	unsigned int mem_map_size = efx->type->mem_map_size(efx);
	int rc, bar;

	netif_dbg(efx, probe, efx->net_dev, "initialising I/O\n");

	bar = efx->type->mem_bar;

	rc = pci_enable_device(pci_dev);
	if (rc) {
		netif_err(efx, probe, efx->net_dev,
			  "failed to enable PCI device\n");
		goto fail1;
	}

	pci_set_master(pci_dev);

	 
	while (dma_mask > 0x7fffffffUL) {
		rc = dma_set_mask_and_coherent(&pci_dev->dev, dma_mask);
		if (rc == 0)
			break;
		dma_mask >>= 1;
	}
	if (rc) {
		netif_err(efx, probe, efx->net_dev,
			  "could not find a suitable DMA mask\n");
		goto fail2;
	}
	netif_dbg(efx, probe, efx->net_dev,
		  "using DMA mask %llx\n", (unsigned long long) dma_mask);

	efx->membase_phys = pci_resource_start(efx->pci_dev, bar);
	rc = pci_request_region(pci_dev, bar, "sfc");
	if (rc) {
		netif_err(efx, probe, efx->net_dev,
			  "request for memory BAR failed\n");
		rc = -EIO;
		goto fail3;
	}
	efx->membase = ioremap(efx->membase_phys, mem_map_size);
	if (!efx->membase) {
		netif_err(efx, probe, efx->net_dev,
			  "could not map memory BAR at %llx+%x\n",
			  (unsigned long long)efx->membase_phys, mem_map_size);
		rc = -ENOMEM;
		goto fail4;
	}
	netif_dbg(efx, probe, efx->net_dev,
		  "memory BAR at %llx+%x (virtual %p)\n",
		  (unsigned long long)efx->membase_phys, mem_map_size,
		  efx->membase);

	return 0;

 fail4:
	pci_release_region(efx->pci_dev, bar);
 fail3:
	efx->membase_phys = 0;
 fail2:
	pci_disable_device(efx->pci_dev);
 fail1:
	return rc;
}

static void ef4_fini_io(struct ef4_nic *efx)
{
	int bar;

	netif_dbg(efx, drv, efx->net_dev, "shutting down I/O\n");

	if (efx->membase) {
		iounmap(efx->membase);
		efx->membase = NULL;
	}

	if (efx->membase_phys) {
		bar = efx->type->mem_bar;
		pci_release_region(efx->pci_dev, bar);
		efx->membase_phys = 0;
	}

	 
	if (!pci_vfs_assigned(efx->pci_dev))
		pci_disable_device(efx->pci_dev);
}

void ef4_set_default_rx_indir_table(struct ef4_nic *efx)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(efx->rx_indir_table); i++)
		efx->rx_indir_table[i] =
			ethtool_rxfh_indir_default(i, efx->rss_spread);
}

static unsigned int ef4_wanted_parallelism(struct ef4_nic *efx)
{
	cpumask_var_t thread_mask;
	unsigned int count;
	int cpu;

	if (rss_cpus) {
		count = rss_cpus;
	} else {
		if (unlikely(!zalloc_cpumask_var(&thread_mask, GFP_KERNEL))) {
			netif_warn(efx, probe, efx->net_dev,
				   "RSS disabled due to allocation failure\n");
			return 1;
		}

		count = 0;
		for_each_online_cpu(cpu) {
			if (!cpumask_test_cpu(cpu, thread_mask)) {
				++count;
				cpumask_or(thread_mask, thread_mask,
					   topology_sibling_cpumask(cpu));
			}
		}

		free_cpumask_var(thread_mask);
	}

	if (count > EF4_MAX_RX_QUEUES) {
		netif_cond_dbg(efx, probe, efx->net_dev, !rss_cpus, warn,
			       "Reducing number of rx queues from %u to %u.\n",
			       count, EF4_MAX_RX_QUEUES);
		count = EF4_MAX_RX_QUEUES;
	}

	return count;
}

 
static int ef4_probe_interrupts(struct ef4_nic *efx)
{
	unsigned int extra_channels = 0;
	unsigned int i, j;
	int rc;

	for (i = 0; i < EF4_MAX_EXTRA_CHANNELS; i++)
		if (efx->extra_channel_type[i])
			++extra_channels;

	if (efx->interrupt_mode == EF4_INT_MODE_MSIX) {
		struct msix_entry xentries[EF4_MAX_CHANNELS];
		unsigned int n_channels;

		n_channels = ef4_wanted_parallelism(efx);
		if (ef4_separate_tx_channels)
			n_channels *= 2;
		n_channels += extra_channels;
		n_channels = min(n_channels, efx->max_channels);

		for (i = 0; i < n_channels; i++)
			xentries[i].entry = i;
		rc = pci_enable_msix_range(efx->pci_dev,
					   xentries, 1, n_channels);
		if (rc < 0) {
			 
			efx->interrupt_mode = EF4_INT_MODE_MSI;
			netif_err(efx, drv, efx->net_dev,
				  "could not enable MSI-X\n");
		} else if (rc < n_channels) {
			netif_err(efx, drv, efx->net_dev,
				  "WARNING: Insufficient MSI-X vectors"
				  " available (%d < %u).\n", rc, n_channels);
			netif_err(efx, drv, efx->net_dev,
				  "WARNING: Performance may be reduced.\n");
			n_channels = rc;
		}

		if (rc > 0) {
			efx->n_channels = n_channels;
			if (n_channels > extra_channels)
				n_channels -= extra_channels;
			if (ef4_separate_tx_channels) {
				efx->n_tx_channels = min(max(n_channels / 2,
							     1U),
							 efx->max_tx_channels);
				efx->n_rx_channels = max(n_channels -
							 efx->n_tx_channels,
							 1U);
			} else {
				efx->n_tx_channels = min(n_channels,
							 efx->max_tx_channels);
				efx->n_rx_channels = n_channels;
			}
			for (i = 0; i < efx->n_channels; i++)
				ef4_get_channel(efx, i)->irq =
					xentries[i].vector;
		}
	}

	 
	if (efx->interrupt_mode == EF4_INT_MODE_MSI) {
		efx->n_channels = 1;
		efx->n_rx_channels = 1;
		efx->n_tx_channels = 1;
		rc = pci_enable_msi(efx->pci_dev);
		if (rc == 0) {
			ef4_get_channel(efx, 0)->irq = efx->pci_dev->irq;
		} else {
			netif_err(efx, drv, efx->net_dev,
				  "could not enable MSI\n");
			efx->interrupt_mode = EF4_INT_MODE_LEGACY;
		}
	}

	 
	if (efx->interrupt_mode == EF4_INT_MODE_LEGACY) {
		efx->n_channels = 1 + (ef4_separate_tx_channels ? 1 : 0);
		efx->n_rx_channels = 1;
		efx->n_tx_channels = 1;
		efx->legacy_irq = efx->pci_dev->irq;
	}

	 
	j = efx->n_channels;
	for (i = 0; i < EF4_MAX_EXTRA_CHANNELS; i++) {
		if (!efx->extra_channel_type[i])
			continue;
		if (efx->interrupt_mode != EF4_INT_MODE_MSIX ||
		    efx->n_channels <= extra_channels) {
			efx->extra_channel_type[i]->handle_no_channel(efx);
		} else {
			--j;
			ef4_get_channel(efx, j)->type =
				efx->extra_channel_type[i];
		}
	}

	efx->rss_spread = efx->n_rx_channels;

	return 0;
}

static int ef4_soft_enable_interrupts(struct ef4_nic *efx)
{
	struct ef4_channel *channel, *end_channel;
	int rc;

	BUG_ON(efx->state == STATE_DISABLED);

	efx->irq_soft_enabled = true;
	smp_wmb();

	ef4_for_each_channel(channel, efx) {
		if (!channel->type->keep_eventq) {
			rc = ef4_init_eventq(channel);
			if (rc)
				goto fail;
		}
		ef4_start_eventq(channel);
	}

	return 0;
fail:
	end_channel = channel;
	ef4_for_each_channel(channel, efx) {
		if (channel == end_channel)
			break;
		ef4_stop_eventq(channel);
		if (!channel->type->keep_eventq)
			ef4_fini_eventq(channel);
	}

	return rc;
}

static void ef4_soft_disable_interrupts(struct ef4_nic *efx)
{
	struct ef4_channel *channel;

	if (efx->state == STATE_DISABLED)
		return;

	efx->irq_soft_enabled = false;
	smp_wmb();

	if (efx->legacy_irq)
		synchronize_irq(efx->legacy_irq);

	ef4_for_each_channel(channel, efx) {
		if (channel->irq)
			synchronize_irq(channel->irq);

		ef4_stop_eventq(channel);
		if (!channel->type->keep_eventq)
			ef4_fini_eventq(channel);
	}
}

static int ef4_enable_interrupts(struct ef4_nic *efx)
{
	struct ef4_channel *channel, *end_channel;
	int rc;

	BUG_ON(efx->state == STATE_DISABLED);

	if (efx->eeh_disabled_legacy_irq) {
		enable_irq(efx->legacy_irq);
		efx->eeh_disabled_legacy_irq = false;
	}

	efx->type->irq_enable_master(efx);

	ef4_for_each_channel(channel, efx) {
		if (channel->type->keep_eventq) {
			rc = ef4_init_eventq(channel);
			if (rc)
				goto fail;
		}
	}

	rc = ef4_soft_enable_interrupts(efx);
	if (rc)
		goto fail;

	return 0;

fail:
	end_channel = channel;
	ef4_for_each_channel(channel, efx) {
		if (channel == end_channel)
			break;
		if (channel->type->keep_eventq)
			ef4_fini_eventq(channel);
	}

	efx->type->irq_disable_non_ev(efx);

	return rc;
}

static void ef4_disable_interrupts(struct ef4_nic *efx)
{
	struct ef4_channel *channel;

	ef4_soft_disable_interrupts(efx);

	ef4_for_each_channel(channel, efx) {
		if (channel->type->keep_eventq)
			ef4_fini_eventq(channel);
	}

	efx->type->irq_disable_non_ev(efx);
}

static void ef4_remove_interrupts(struct ef4_nic *efx)
{
	struct ef4_channel *channel;

	 
	ef4_for_each_channel(channel, efx)
		channel->irq = 0;
	pci_disable_msi(efx->pci_dev);
	pci_disable_msix(efx->pci_dev);

	 
	efx->legacy_irq = 0;
}

static void ef4_set_channels(struct ef4_nic *efx)
{
	struct ef4_channel *channel;
	struct ef4_tx_queue *tx_queue;

	efx->tx_channel_offset =
		ef4_separate_tx_channels ?
		efx->n_channels - efx->n_tx_channels : 0;

	 
	ef4_for_each_channel(channel, efx) {
		if (channel->channel < efx->n_rx_channels)
			channel->rx_queue.core_index = channel->channel;
		else
			channel->rx_queue.core_index = -1;

		ef4_for_each_channel_tx_queue(tx_queue, channel)
			tx_queue->queue -= (efx->tx_channel_offset *
					    EF4_TXQ_TYPES);
	}
}

static int ef4_probe_nic(struct ef4_nic *efx)
{
	int rc;

	netif_dbg(efx, probe, efx->net_dev, "creating NIC\n");

	 
	rc = efx->type->probe(efx);
	if (rc)
		return rc;

	do {
		if (!efx->max_channels || !efx->max_tx_channels) {
			netif_err(efx, drv, efx->net_dev,
				  "Insufficient resources to allocate"
				  " any channels\n");
			rc = -ENOSPC;
			goto fail1;
		}

		 
		rc = ef4_probe_interrupts(efx);
		if (rc)
			goto fail1;

		ef4_set_channels(efx);

		 
		rc = efx->type->dimension_resources(efx);
		if (rc != 0 && rc != -EAGAIN)
			goto fail2;

		if (rc == -EAGAIN)
			 
			ef4_remove_interrupts(efx);

	} while (rc == -EAGAIN);

	if (efx->n_channels > 1)
		netdev_rss_key_fill(&efx->rx_hash_key,
				    sizeof(efx->rx_hash_key));
	ef4_set_default_rx_indir_table(efx);

	netif_set_real_num_tx_queues(efx->net_dev, efx->n_tx_channels);
	netif_set_real_num_rx_queues(efx->net_dev, efx->n_rx_channels);

	 
	efx->irq_mod_step_us = DIV_ROUND_UP(efx->timer_quantum_ns, 1000);
	ef4_init_irq_moderation(efx, tx_irq_mod_usec, rx_irq_mod_usec, true,
				true);

	return 0;

fail2:
	ef4_remove_interrupts(efx);
fail1:
	efx->type->remove(efx);
	return rc;
}

static void ef4_remove_nic(struct ef4_nic *efx)
{
	netif_dbg(efx, drv, efx->net_dev, "destroying NIC\n");

	ef4_remove_interrupts(efx);
	efx->type->remove(efx);
}

static int ef4_probe_filters(struct ef4_nic *efx)
{
	int rc;

	spin_lock_init(&efx->filter_lock);
	init_rwsem(&efx->filter_sem);
	mutex_lock(&efx->mac_lock);
	down_write(&efx->filter_sem);
	rc = efx->type->filter_table_probe(efx);
	if (rc)
		goto out_unlock;

#ifdef CONFIG_RFS_ACCEL
	if (efx->type->offload_features & NETIF_F_NTUPLE) {
		struct ef4_channel *channel;
		int i, success = 1;

		ef4_for_each_channel(channel, efx) {
			channel->rps_flow_id =
				kcalloc(efx->type->max_rx_ip_filters,
					sizeof(*channel->rps_flow_id),
					GFP_KERNEL);
			if (!channel->rps_flow_id)
				success = 0;
			else
				for (i = 0;
				     i < efx->type->max_rx_ip_filters;
				     ++i)
					channel->rps_flow_id[i] =
						RPS_FLOW_ID_INVALID;
		}

		if (!success) {
			ef4_for_each_channel(channel, efx)
				kfree(channel->rps_flow_id);
			efx->type->filter_table_remove(efx);
			rc = -ENOMEM;
			goto out_unlock;
		}

		efx->rps_expire_index = efx->rps_expire_channel = 0;
	}
#endif
out_unlock:
	up_write(&efx->filter_sem);
	mutex_unlock(&efx->mac_lock);
	return rc;
}

static void ef4_remove_filters(struct ef4_nic *efx)
{
#ifdef CONFIG_RFS_ACCEL
	struct ef4_channel *channel;

	ef4_for_each_channel(channel, efx)
		kfree(channel->rps_flow_id);
#endif
	down_write(&efx->filter_sem);
	efx->type->filter_table_remove(efx);
	up_write(&efx->filter_sem);
}

static void ef4_restore_filters(struct ef4_nic *efx)
{
	down_read(&efx->filter_sem);
	efx->type->filter_table_restore(efx);
	up_read(&efx->filter_sem);
}

 

static int ef4_probe_all(struct ef4_nic *efx)
{
	int rc;

	rc = ef4_probe_nic(efx);
	if (rc) {
		netif_err(efx, probe, efx->net_dev, "failed to create NIC\n");
		goto fail1;
	}

	rc = ef4_probe_port(efx);
	if (rc) {
		netif_err(efx, probe, efx->net_dev, "failed to create port\n");
		goto fail2;
	}

	BUILD_BUG_ON(EF4_DEFAULT_DMAQ_SIZE < EF4_RXQ_MIN_ENT);
	if (WARN_ON(EF4_DEFAULT_DMAQ_SIZE < EF4_TXQ_MIN_ENT(efx))) {
		rc = -EINVAL;
		goto fail3;
	}
	efx->rxq_entries = efx->txq_entries = EF4_DEFAULT_DMAQ_SIZE;

	rc = ef4_probe_filters(efx);
	if (rc) {
		netif_err(efx, probe, efx->net_dev,
			  "failed to create filter tables\n");
		goto fail4;
	}

	rc = ef4_probe_channels(efx);
	if (rc)
		goto fail5;

	return 0;

 fail5:
	ef4_remove_filters(efx);
 fail4:
 fail3:
	ef4_remove_port(efx);
 fail2:
	ef4_remove_nic(efx);
 fail1:
	return rc;
}

 
static void ef4_start_all(struct ef4_nic *efx)
{
	EF4_ASSERT_RESET_SERIALISED(efx);
	BUG_ON(efx->state == STATE_DISABLED);

	 
	if (efx->port_enabled || !netif_running(efx->net_dev) ||
	    efx->reset_pending)
		return;

	ef4_start_port(efx);
	ef4_start_datapath(efx);

	 
	if (efx->type->monitor != NULL)
		queue_delayed_work(efx->workqueue, &efx->monitor_work,
				   ef4_monitor_interval);

	efx->type->start_stats(efx);
	efx->type->pull_stats(efx);
	spin_lock_bh(&efx->stats_lock);
	efx->type->update_stats(efx, NULL, NULL);
	spin_unlock_bh(&efx->stats_lock);
}

 
static void ef4_stop_all(struct ef4_nic *efx)
{
	EF4_ASSERT_RESET_SERIALISED(efx);

	 
	if (!efx->port_enabled)
		return;

	 
	efx->type->pull_stats(efx);
	spin_lock_bh(&efx->stats_lock);
	efx->type->update_stats(efx, NULL, NULL);
	spin_unlock_bh(&efx->stats_lock);
	efx->type->stop_stats(efx);
	ef4_stop_port(efx);

	 
	WARN_ON(netif_running(efx->net_dev) &&
		netif_device_present(efx->net_dev));
	netif_tx_disable(efx->net_dev);

	ef4_stop_datapath(efx);
}

static void ef4_remove_all(struct ef4_nic *efx)
{
	ef4_remove_channels(efx);
	ef4_remove_filters(efx);
	ef4_remove_port(efx);
	ef4_remove_nic(efx);
}

 
unsigned int ef4_usecs_to_ticks(struct ef4_nic *efx, unsigned int usecs)
{
	if (usecs == 0)
		return 0;
	if (usecs * 1000 < efx->timer_quantum_ns)
		return 1;  
	return usecs * 1000 / efx->timer_quantum_ns;
}

unsigned int ef4_ticks_to_usecs(struct ef4_nic *efx, unsigned int ticks)
{
	 
	return DIV_ROUND_UP(ticks * efx->timer_quantum_ns, 1000);
}

 
int ef4_init_irq_moderation(struct ef4_nic *efx, unsigned int tx_usecs,
			    unsigned int rx_usecs, bool rx_adaptive,
			    bool rx_may_override_tx)
{
	struct ef4_channel *channel;
	unsigned int timer_max_us;

	EF4_ASSERT_RESET_SERIALISED(efx);

	timer_max_us = efx->timer_max_ns / 1000;

	if (tx_usecs > timer_max_us || rx_usecs > timer_max_us)
		return -EINVAL;

	if (tx_usecs != rx_usecs && efx->tx_channel_offset == 0 &&
	    !rx_may_override_tx) {
		netif_err(efx, drv, efx->net_dev, "Channels are shared. "
			  "RX and TX IRQ moderation must be equal\n");
		return -EINVAL;
	}

	efx->irq_rx_adaptive = rx_adaptive;
	efx->irq_rx_moderation_us = rx_usecs;
	ef4_for_each_channel(channel, efx) {
		if (ef4_channel_has_rx_queue(channel))
			channel->irq_moderation_us = rx_usecs;
		else if (ef4_channel_has_tx_queues(channel))
			channel->irq_moderation_us = tx_usecs;
	}

	return 0;
}

void ef4_get_irq_moderation(struct ef4_nic *efx, unsigned int *tx_usecs,
			    unsigned int *rx_usecs, bool *rx_adaptive)
{
	*rx_adaptive = efx->irq_rx_adaptive;
	*rx_usecs = efx->irq_rx_moderation_us;

	 
	if (efx->tx_channel_offset == 0) {
		*tx_usecs = *rx_usecs;
	} else {
		struct ef4_channel *tx_channel;

		tx_channel = efx->channel[efx->tx_channel_offset];
		*tx_usecs = tx_channel->irq_moderation_us;
	}
}

 

 
static void ef4_monitor(struct work_struct *data)
{
	struct ef4_nic *efx = container_of(data, struct ef4_nic,
					   monitor_work.work);

	netif_vdbg(efx, timer, efx->net_dev,
		   "hardware monitor executing on CPU %d\n",
		   raw_smp_processor_id());
	BUG_ON(efx->type->monitor == NULL);

	 
	if (mutex_trylock(&efx->mac_lock)) {
		if (efx->port_enabled)
			efx->type->monitor(efx);
		mutex_unlock(&efx->mac_lock);
	}

	queue_delayed_work(efx->workqueue, &efx->monitor_work,
			   ef4_monitor_interval);
}

 

 
static int ef4_ioctl(struct net_device *net_dev, struct ifreq *ifr, int cmd)
{
	struct ef4_nic *efx = netdev_priv(net_dev);
	struct mii_ioctl_data *data = if_mii(ifr);

	 
	if ((cmd == SIOCGMIIREG || cmd == SIOCSMIIREG) &&
	    (data->phy_id & 0xfc00) == 0x0400)
		data->phy_id ^= MDIO_PHY_ID_C45 | 0x0400;

	return mdio_mii_ioctl(&efx->mdio, data, cmd);
}

 

static void ef4_init_napi_channel(struct ef4_channel *channel)
{
	struct ef4_nic *efx = channel->efx;

	channel->napi_dev = efx->net_dev;
	netif_napi_add(channel->napi_dev, &channel->napi_str, ef4_poll);
}

static void ef4_init_napi(struct ef4_nic *efx)
{
	struct ef4_channel *channel;

	ef4_for_each_channel(channel, efx)
		ef4_init_napi_channel(channel);
}

static void ef4_fini_napi_channel(struct ef4_channel *channel)
{
	if (channel->napi_dev)
		netif_napi_del(&channel->napi_str);

	channel->napi_dev = NULL;
}

static void ef4_fini_napi(struct ef4_nic *efx)
{
	struct ef4_channel *channel;

	ef4_for_each_channel(channel, efx)
		ef4_fini_napi_channel(channel);
}

 

 
int ef4_net_open(struct net_device *net_dev)
{
	struct ef4_nic *efx = netdev_priv(net_dev);
	int rc;

	netif_dbg(efx, ifup, efx->net_dev, "opening device on CPU %d\n",
		  raw_smp_processor_id());

	rc = ef4_check_disabled(efx);
	if (rc)
		return rc;
	if (efx->phy_mode & PHY_MODE_SPECIAL)
		return -EBUSY;

	 
	ef4_link_status_changed(efx);

	ef4_start_all(efx);
	ef4_selftest_async_start(efx);
	return 0;
}

 
int ef4_net_stop(struct net_device *net_dev)
{
	struct ef4_nic *efx = netdev_priv(net_dev);

	netif_dbg(efx, ifdown, efx->net_dev, "closing on CPU %d\n",
		  raw_smp_processor_id());

	 
	ef4_stop_all(efx);

	return 0;
}

 
static void ef4_net_stats(struct net_device *net_dev,
			  struct rtnl_link_stats64 *stats)
{
	struct ef4_nic *efx = netdev_priv(net_dev);

	spin_lock_bh(&efx->stats_lock);
	efx->type->update_stats(efx, NULL, stats);
	spin_unlock_bh(&efx->stats_lock);
}

 
static void ef4_watchdog(struct net_device *net_dev, unsigned int txqueue)
{
	struct ef4_nic *efx = netdev_priv(net_dev);

	netif_err(efx, tx_err, efx->net_dev,
		  "TX stuck with port_enabled=%d: resetting channels\n",
		  efx->port_enabled);

	ef4_schedule_reset(efx, RESET_TYPE_TX_WATCHDOG);
}


 
static int ef4_change_mtu(struct net_device *net_dev, int new_mtu)
{
	struct ef4_nic *efx = netdev_priv(net_dev);
	int rc;

	rc = ef4_check_disabled(efx);
	if (rc)
		return rc;

	netif_dbg(efx, drv, efx->net_dev, "changing MTU to %d\n", new_mtu);

	ef4_device_detach_sync(efx);
	ef4_stop_all(efx);

	mutex_lock(&efx->mac_lock);
	net_dev->mtu = new_mtu;
	ef4_mac_reconfigure(efx);
	mutex_unlock(&efx->mac_lock);

	ef4_start_all(efx);
	netif_device_attach(efx->net_dev);
	return 0;
}

static int ef4_set_mac_address(struct net_device *net_dev, void *data)
{
	struct ef4_nic *efx = netdev_priv(net_dev);
	struct sockaddr *addr = data;
	u8 *new_addr = addr->sa_data;
	u8 old_addr[6];
	int rc;

	if (!is_valid_ether_addr(new_addr)) {
		netif_err(efx, drv, efx->net_dev,
			  "invalid ethernet MAC address requested: %pM\n",
			  new_addr);
		return -EADDRNOTAVAIL;
	}

	 
	ether_addr_copy(old_addr, net_dev->dev_addr);
	eth_hw_addr_set(net_dev, new_addr);
	if (efx->type->set_mac_address) {
		rc = efx->type->set_mac_address(efx);
		if (rc) {
			eth_hw_addr_set(net_dev, old_addr);
			return rc;
		}
	}

	 
	mutex_lock(&efx->mac_lock);
	ef4_mac_reconfigure(efx);
	mutex_unlock(&efx->mac_lock);

	return 0;
}

 
static void ef4_set_rx_mode(struct net_device *net_dev)
{
	struct ef4_nic *efx = netdev_priv(net_dev);

	if (efx->port_enabled)
		queue_work(efx->workqueue, &efx->mac_work);
	 
}

static int ef4_set_features(struct net_device *net_dev, netdev_features_t data)
{
	struct ef4_nic *efx = netdev_priv(net_dev);
	int rc;

	 
	if (net_dev->features & ~data & NETIF_F_NTUPLE) {
		rc = efx->type->filter_clear_rx(efx, EF4_FILTER_PRI_MANUAL);
		if (rc)
			return rc;
	}

	 
	if ((net_dev->features ^ data) & NETIF_F_HW_VLAN_CTAG_FILTER) {
		 
		ef4_set_rx_mode(net_dev);
	}

	return 0;
}

static const struct net_device_ops ef4_netdev_ops = {
	.ndo_open		= ef4_net_open,
	.ndo_stop		= ef4_net_stop,
	.ndo_get_stats64	= ef4_net_stats,
	.ndo_tx_timeout		= ef4_watchdog,
	.ndo_start_xmit		= ef4_hard_start_xmit,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_eth_ioctl		= ef4_ioctl,
	.ndo_change_mtu		= ef4_change_mtu,
	.ndo_set_mac_address	= ef4_set_mac_address,
	.ndo_set_rx_mode	= ef4_set_rx_mode,
	.ndo_set_features	= ef4_set_features,
	.ndo_setup_tc		= ef4_setup_tc,
#ifdef CONFIG_RFS_ACCEL
	.ndo_rx_flow_steer	= ef4_filter_rfs,
#endif
};

static void ef4_update_name(struct ef4_nic *efx)
{
	strcpy(efx->name, efx->net_dev->name);
	ef4_mtd_rename(efx);
	ef4_set_channel_names(efx);
}

static int ef4_netdev_event(struct notifier_block *this,
			    unsigned long event, void *ptr)
{
	struct net_device *net_dev = netdev_notifier_info_to_dev(ptr);

	if ((net_dev->netdev_ops == &ef4_netdev_ops) &&
	    event == NETDEV_CHANGENAME)
		ef4_update_name(netdev_priv(net_dev));

	return NOTIFY_DONE;
}

static struct notifier_block ef4_netdev_notifier = {
	.notifier_call = ef4_netdev_event,
};

static ssize_t
phy_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ef4_nic *efx = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", efx->phy_type);
}
static DEVICE_ATTR_RO(phy_type);

static int ef4_register_netdev(struct ef4_nic *efx)
{
	struct net_device *net_dev = efx->net_dev;
	struct ef4_channel *channel;
	int rc;

	net_dev->watchdog_timeo = 5 * HZ;
	net_dev->irq = efx->pci_dev->irq;
	net_dev->netdev_ops = &ef4_netdev_ops;
	net_dev->ethtool_ops = &ef4_ethtool_ops;
	netif_set_tso_max_segs(net_dev, EF4_TSO_MAX_SEGS);
	net_dev->min_mtu = EF4_MIN_MTU;
	net_dev->max_mtu = EF4_MAX_MTU;

	rtnl_lock();

	 
	efx->state = STATE_READY;
	smp_mb();  
	if (efx->reset_pending) {
		netif_err(efx, probe, efx->net_dev,
			  "aborting probe due to scheduled reset\n");
		rc = -EIO;
		goto fail_locked;
	}

	rc = dev_alloc_name(net_dev, net_dev->name);
	if (rc < 0)
		goto fail_locked;
	ef4_update_name(efx);

	 
	netif_carrier_off(net_dev);

	rc = register_netdevice(net_dev);
	if (rc)
		goto fail_locked;

	ef4_for_each_channel(channel, efx) {
		struct ef4_tx_queue *tx_queue;
		ef4_for_each_channel_tx_queue(tx_queue, channel)
			ef4_init_tx_queue_core_txq(tx_queue);
	}

	ef4_associate(efx);

	rtnl_unlock();

	rc = device_create_file(&efx->pci_dev->dev, &dev_attr_phy_type);
	if (rc) {
		netif_err(efx, drv, efx->net_dev,
			  "failed to init net dev attributes\n");
		goto fail_registered;
	}
	return 0;

fail_registered:
	rtnl_lock();
	ef4_dissociate(efx);
	unregister_netdevice(net_dev);
fail_locked:
	efx->state = STATE_UNINIT;
	rtnl_unlock();
	netif_err(efx, drv, efx->net_dev, "could not register net dev\n");
	return rc;
}

static void ef4_unregister_netdev(struct ef4_nic *efx)
{
	if (!efx->net_dev)
		return;

	BUG_ON(netdev_priv(efx->net_dev) != efx);

	if (ef4_dev_registered(efx)) {
		strscpy(efx->name, pci_name(efx->pci_dev), sizeof(efx->name));
		device_remove_file(&efx->pci_dev->dev, &dev_attr_phy_type);
		unregister_netdev(efx->net_dev);
	}
}

 

 
void ef4_reset_down(struct ef4_nic *efx, enum reset_type method)
{
	EF4_ASSERT_RESET_SERIALISED(efx);

	ef4_stop_all(efx);
	ef4_disable_interrupts(efx);

	mutex_lock(&efx->mac_lock);
	if (efx->port_initialized && method != RESET_TYPE_INVISIBLE &&
	    method != RESET_TYPE_DATAPATH)
		efx->phy_op->fini(efx);
	efx->type->fini(efx);
}

 
int ef4_reset_up(struct ef4_nic *efx, enum reset_type method, bool ok)
{
	int rc;

	EF4_ASSERT_RESET_SERIALISED(efx);

	 
	rc = efx->type->init(efx);
	if (rc) {
		netif_err(efx, drv, efx->net_dev, "failed to initialise NIC\n");
		goto fail;
	}

	if (!ok)
		goto fail;

	if (efx->port_initialized && method != RESET_TYPE_INVISIBLE &&
	    method != RESET_TYPE_DATAPATH) {
		rc = efx->phy_op->init(efx);
		if (rc)
			goto fail;
		rc = efx->phy_op->reconfigure(efx);
		if (rc && rc != -EPERM)
			netif_err(efx, drv, efx->net_dev,
				  "could not restore PHY settings\n");
	}

	rc = ef4_enable_interrupts(efx);
	if (rc)
		goto fail;

	down_read(&efx->filter_sem);
	ef4_restore_filters(efx);
	up_read(&efx->filter_sem);

	mutex_unlock(&efx->mac_lock);

	ef4_start_all(efx);

	return 0;

fail:
	efx->port_initialized = false;

	mutex_unlock(&efx->mac_lock);

	return rc;
}

 
int ef4_reset(struct ef4_nic *efx, enum reset_type method)
{
	int rc, rc2;
	bool disabled;

	netif_info(efx, drv, efx->net_dev, "resetting (%s)\n",
		   RESET_TYPE(method));

	ef4_device_detach_sync(efx);
	ef4_reset_down(efx, method);

	rc = efx->type->reset(efx, method);
	if (rc) {
		netif_err(efx, drv, efx->net_dev, "failed to reset hardware\n");
		goto out;
	}

	 
	if (method < RESET_TYPE_MAX_METHOD)
		efx->reset_pending &= -(1 << (method + 1));
	else  
		__clear_bit(method, &efx->reset_pending);

	 
	pci_set_master(efx->pci_dev);

out:
	 
	disabled = rc ||
		method == RESET_TYPE_DISABLE ||
		method == RESET_TYPE_RECOVER_OR_DISABLE;
	rc2 = ef4_reset_up(efx, method, !disabled);
	if (rc2) {
		disabled = true;
		if (!rc)
			rc = rc2;
	}

	if (disabled) {
		dev_close(efx->net_dev);
		netif_err(efx, drv, efx->net_dev, "has been disabled\n");
		efx->state = STATE_DISABLED;
	} else {
		netif_dbg(efx, drv, efx->net_dev, "reset complete\n");
		netif_device_attach(efx->net_dev);
	}
	return rc;
}

 
int ef4_try_recovery(struct ef4_nic *efx)
{
#ifdef CONFIG_EEH
	 
	struct eeh_dev *eehdev = pci_dev_to_eeh_dev(efx->pci_dev);
	if (eeh_dev_check_failure(eehdev)) {
		 
		return 1;
	}
#endif
	return 0;
}

 
static void ef4_reset_work(struct work_struct *data)
{
	struct ef4_nic *efx = container_of(data, struct ef4_nic, reset_work);
	unsigned long pending;
	enum reset_type method;

	pending = READ_ONCE(efx->reset_pending);
	method = fls(pending) - 1;

	if ((method == RESET_TYPE_RECOVER_OR_DISABLE ||
	     method == RESET_TYPE_RECOVER_OR_ALL) &&
	    ef4_try_recovery(efx))
		return;

	if (!pending)
		return;

	rtnl_lock();

	 
	if (efx->state == STATE_READY)
		(void)ef4_reset(efx, method);

	rtnl_unlock();
}

void ef4_schedule_reset(struct ef4_nic *efx, enum reset_type type)
{
	enum reset_type method;

	if (efx->state == STATE_RECOVERY) {
		netif_dbg(efx, drv, efx->net_dev,
			  "recovering: skip scheduling %s reset\n",
			  RESET_TYPE(type));
		return;
	}

	switch (type) {
	case RESET_TYPE_INVISIBLE:
	case RESET_TYPE_ALL:
	case RESET_TYPE_RECOVER_OR_ALL:
	case RESET_TYPE_WORLD:
	case RESET_TYPE_DISABLE:
	case RESET_TYPE_RECOVER_OR_DISABLE:
	case RESET_TYPE_DATAPATH:
		method = type;
		netif_dbg(efx, drv, efx->net_dev, "scheduling %s reset\n",
			  RESET_TYPE(method));
		break;
	default:
		method = efx->type->map_reset_reason(type);
		netif_dbg(efx, drv, efx->net_dev,
			  "scheduling %s reset for %s\n",
			  RESET_TYPE(method), RESET_TYPE(type));
		break;
	}

	set_bit(method, &efx->reset_pending);
	smp_mb();  

	 
	if (READ_ONCE(efx->state) != STATE_READY)
		return;

	queue_work(reset_workqueue, &efx->reset_work);
}

 

 
static const struct pci_device_id ef4_pci_table[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_SOLARFLARE,
		    PCI_DEVICE_ID_SOLARFLARE_SFC4000A_0),
	 .driver_data = (unsigned long) &falcon_a1_nic_type},
	{PCI_DEVICE(PCI_VENDOR_ID_SOLARFLARE,
		    PCI_DEVICE_ID_SOLARFLARE_SFC4000B),
	 .driver_data = (unsigned long) &falcon_b0_nic_type},
	{0}			 
};

 
int ef4_port_dummy_op_int(struct ef4_nic *efx)
{
	return 0;
}
void ef4_port_dummy_op_void(struct ef4_nic *efx) {}

static bool ef4_port_dummy_op_poll(struct ef4_nic *efx)
{
	return false;
}

static const struct ef4_phy_operations ef4_dummy_phy_operations = {
	.init		 = ef4_port_dummy_op_int,
	.reconfigure	 = ef4_port_dummy_op_int,
	.poll		 = ef4_port_dummy_op_poll,
	.fini		 = ef4_port_dummy_op_void,
};

 

 
static int ef4_init_struct(struct ef4_nic *efx,
			   struct pci_dev *pci_dev, struct net_device *net_dev)
{
	int i;

	 
	INIT_LIST_HEAD(&efx->node);
	INIT_LIST_HEAD(&efx->secondary_list);
	spin_lock_init(&efx->biu_lock);
#ifdef CONFIG_SFC_FALCON_MTD
	INIT_LIST_HEAD(&efx->mtd_list);
#endif
	INIT_WORK(&efx->reset_work, ef4_reset_work);
	INIT_DELAYED_WORK(&efx->monitor_work, ef4_monitor);
	INIT_DELAYED_WORK(&efx->selftest_work, ef4_selftest_async_work);
	efx->pci_dev = pci_dev;
	efx->msg_enable = debug;
	efx->state = STATE_UNINIT;
	strscpy(efx->name, pci_name(pci_dev), sizeof(efx->name));

	efx->net_dev = net_dev;
	efx->rx_prefix_size = efx->type->rx_prefix_size;
	efx->rx_ip_align =
		NET_IP_ALIGN ? (efx->rx_prefix_size + NET_IP_ALIGN) % 4 : 0;
	efx->rx_packet_hash_offset =
		efx->type->rx_hash_offset - efx->type->rx_prefix_size;
	efx->rx_packet_ts_offset =
		efx->type->rx_ts_offset - efx->type->rx_prefix_size;
	spin_lock_init(&efx->stats_lock);
	mutex_init(&efx->mac_lock);
	efx->phy_op = &ef4_dummy_phy_operations;
	efx->mdio.dev = net_dev;
	INIT_WORK(&efx->mac_work, ef4_mac_work);
	init_waitqueue_head(&efx->flush_wq);

	for (i = 0; i < EF4_MAX_CHANNELS; i++) {
		efx->channel[i] = ef4_alloc_channel(efx, i, NULL);
		if (!efx->channel[i])
			goto fail;
		efx->msi_context[i].efx = efx;
		efx->msi_context[i].index = i;
	}

	 
	efx->interrupt_mode = max(efx->type->max_interrupt_mode,
				  interrupt_mode);

	 
	snprintf(efx->workqueue_name, sizeof(efx->workqueue_name), "sfc%s",
		 pci_name(pci_dev));
	efx->workqueue = create_singlethread_workqueue(efx->workqueue_name);
	if (!efx->workqueue)
		goto fail;

	return 0;

fail:
	ef4_fini_struct(efx);
	return -ENOMEM;
}

static void ef4_fini_struct(struct ef4_nic *efx)
{
	int i;

	for (i = 0; i < EF4_MAX_CHANNELS; i++)
		kfree(efx->channel[i]);

	kfree(efx->vpd_sn);

	if (efx->workqueue) {
		destroy_workqueue(efx->workqueue);
		efx->workqueue = NULL;
	}
}

void ef4_update_sw_stats(struct ef4_nic *efx, u64 *stats)
{
	u64 n_rx_nodesc_trunc = 0;
	struct ef4_channel *channel;

	ef4_for_each_channel(channel, efx)
		n_rx_nodesc_trunc += channel->n_rx_nodesc_trunc;
	stats[GENERIC_STAT_rx_nodesc_trunc] = n_rx_nodesc_trunc;
	stats[GENERIC_STAT_rx_noskb_drops] = atomic_read(&efx->n_rx_noskb_drops);
}

 

 
static void ef4_pci_remove_main(struct ef4_nic *efx)
{
	 
	BUG_ON(efx->state == STATE_READY);
	cancel_work_sync(&efx->reset_work);

	ef4_disable_interrupts(efx);
	ef4_nic_fini_interrupt(efx);
	ef4_fini_port(efx);
	efx->type->fini(efx);
	ef4_fini_napi(efx);
	ef4_remove_all(efx);
}

 
static void ef4_pci_remove(struct pci_dev *pci_dev)
{
	struct ef4_nic *efx;

	efx = pci_get_drvdata(pci_dev);
	if (!efx)
		return;

	 
	rtnl_lock();
	ef4_dissociate(efx);
	dev_close(efx->net_dev);
	ef4_disable_interrupts(efx);
	efx->state = STATE_UNINIT;
	rtnl_unlock();

	ef4_unregister_netdev(efx);

	ef4_mtd_remove(efx);

	ef4_pci_remove_main(efx);

	ef4_fini_io(efx);
	netif_dbg(efx, drv, efx->net_dev, "shutdown successful\n");

	ef4_fini_struct(efx);
	free_netdev(efx->net_dev);
};

 
static void ef4_probe_vpd_strings(struct ef4_nic *efx)
{
	struct pci_dev *dev = efx->pci_dev;
	unsigned int vpd_size, kw_len;
	u8 *vpd_data;
	int start;

	vpd_data = pci_vpd_alloc(dev, &vpd_size);
	if (IS_ERR(vpd_data)) {
		pci_warn(dev, "Unable to read VPD\n");
		return;
	}

	start = pci_vpd_find_ro_info_keyword(vpd_data, vpd_size,
					     PCI_VPD_RO_KEYWORD_PARTNO, &kw_len);
	if (start < 0)
		pci_warn(dev, "Part number not found or incomplete\n");
	else
		pci_info(dev, "Part Number : %.*s\n", kw_len, vpd_data + start);

	start = pci_vpd_find_ro_info_keyword(vpd_data, vpd_size,
					     PCI_VPD_RO_KEYWORD_SERIALNO, &kw_len);
	if (start < 0)
		pci_warn(dev, "Serial number not found or incomplete\n");
	else
		efx->vpd_sn = kmemdup_nul(vpd_data + start, kw_len, GFP_KERNEL);

	kfree(vpd_data);
}


 
static int ef4_pci_probe_main(struct ef4_nic *efx)
{
	int rc;

	 
	rc = ef4_probe_all(efx);
	if (rc)
		goto fail1;

	ef4_init_napi(efx);

	rc = efx->type->init(efx);
	if (rc) {
		netif_err(efx, probe, efx->net_dev,
			  "failed to initialise NIC\n");
		goto fail3;
	}

	rc = ef4_init_port(efx);
	if (rc) {
		netif_err(efx, probe, efx->net_dev,
			  "failed to initialise port\n");
		goto fail4;
	}

	rc = ef4_nic_init_interrupt(efx);
	if (rc)
		goto fail5;
	rc = ef4_enable_interrupts(efx);
	if (rc)
		goto fail6;

	return 0;

 fail6:
	ef4_nic_fini_interrupt(efx);
 fail5:
	ef4_fini_port(efx);
 fail4:
	efx->type->fini(efx);
 fail3:
	ef4_fini_napi(efx);
	ef4_remove_all(efx);
 fail1:
	return rc;
}

 
static int ef4_pci_probe(struct pci_dev *pci_dev,
			 const struct pci_device_id *entry)
{
	struct net_device *net_dev;
	struct ef4_nic *efx;
	int rc;

	 
	net_dev = alloc_etherdev_mqs(sizeof(*efx), EF4_MAX_CORE_TX_QUEUES,
				     EF4_MAX_RX_QUEUES);
	if (!net_dev)
		return -ENOMEM;
	efx = netdev_priv(net_dev);
	efx->type = (const struct ef4_nic_type *) entry->driver_data;
	efx->fixed_features |= NETIF_F_HIGHDMA;

	pci_set_drvdata(pci_dev, efx);
	SET_NETDEV_DEV(net_dev, &pci_dev->dev);
	rc = ef4_init_struct(efx, pci_dev, net_dev);
	if (rc)
		goto fail1;

	netif_info(efx, probe, efx->net_dev,
		   "Solarflare NIC detected\n");

	ef4_probe_vpd_strings(efx);

	 
	rc = ef4_init_io(efx);
	if (rc)
		goto fail2;

	rc = ef4_pci_probe_main(efx);
	if (rc)
		goto fail3;

	net_dev->features |= (efx->type->offload_features | NETIF_F_SG |
			      NETIF_F_RXCSUM);
	 
	net_dev->vlan_features |= (NETIF_F_HW_CSUM | NETIF_F_SG |
				   NETIF_F_HIGHDMA | NETIF_F_RXCSUM);

	net_dev->hw_features = net_dev->features & ~efx->fixed_features;

	 
	net_dev->features &= ~NETIF_F_HW_VLAN_CTAG_FILTER;
	net_dev->features |= efx->fixed_features;

	rc = ef4_register_netdev(efx);
	if (rc)
		goto fail4;

	netif_dbg(efx, probe, efx->net_dev, "initialisation successful\n");

	 
	rtnl_lock();
	rc = ef4_mtd_probe(efx);
	rtnl_unlock();
	if (rc && rc != -EPERM)
		netif_warn(efx, probe, efx->net_dev,
			   "failed to create MTDs (%d)\n", rc);

	return 0;

 fail4:
	ef4_pci_remove_main(efx);
 fail3:
	ef4_fini_io(efx);
 fail2:
	ef4_fini_struct(efx);
 fail1:
	WARN_ON(rc > 0);
	netif_dbg(efx, drv, efx->net_dev, "initialisation failed. rc=%d\n", rc);
	free_netdev(net_dev);
	return rc;
}

static int ef4_pm_freeze(struct device *dev)
{
	struct ef4_nic *efx = dev_get_drvdata(dev);

	rtnl_lock();

	if (efx->state != STATE_DISABLED) {
		efx->state = STATE_UNINIT;

		ef4_device_detach_sync(efx);

		ef4_stop_all(efx);
		ef4_disable_interrupts(efx);
	}

	rtnl_unlock();

	return 0;
}

static int ef4_pm_thaw(struct device *dev)
{
	int rc;
	struct ef4_nic *efx = dev_get_drvdata(dev);

	rtnl_lock();

	if (efx->state != STATE_DISABLED) {
		rc = ef4_enable_interrupts(efx);
		if (rc)
			goto fail;

		mutex_lock(&efx->mac_lock);
		efx->phy_op->reconfigure(efx);
		mutex_unlock(&efx->mac_lock);

		ef4_start_all(efx);

		netif_device_attach(efx->net_dev);

		efx->state = STATE_READY;

		efx->type->resume_wol(efx);
	}

	rtnl_unlock();

	 
	queue_work(reset_workqueue, &efx->reset_work);

	return 0;

fail:
	rtnl_unlock();

	return rc;
}

static int ef4_pm_poweroff(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct ef4_nic *efx = pci_get_drvdata(pci_dev);

	efx->type->fini(efx);

	efx->reset_pending = 0;

	pci_save_state(pci_dev);
	return pci_set_power_state(pci_dev, PCI_D3hot);
}

 
static int ef4_pm_resume(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct ef4_nic *efx = pci_get_drvdata(pci_dev);
	int rc;

	rc = pci_set_power_state(pci_dev, PCI_D0);
	if (rc)
		return rc;
	pci_restore_state(pci_dev);
	rc = pci_enable_device(pci_dev);
	if (rc)
		return rc;
	pci_set_master(efx->pci_dev);
	rc = efx->type->reset(efx, RESET_TYPE_ALL);
	if (rc)
		return rc;
	rc = efx->type->init(efx);
	if (rc)
		return rc;
	rc = ef4_pm_thaw(dev);
	return rc;
}

static int ef4_pm_suspend(struct device *dev)
{
	int rc;

	ef4_pm_freeze(dev);
	rc = ef4_pm_poweroff(dev);
	if (rc)
		ef4_pm_resume(dev);
	return rc;
}

static const struct dev_pm_ops ef4_pm_ops = {
	.suspend	= ef4_pm_suspend,
	.resume		= ef4_pm_resume,
	.freeze		= ef4_pm_freeze,
	.thaw		= ef4_pm_thaw,
	.poweroff	= ef4_pm_poweroff,
	.restore	= ef4_pm_resume,
};

 
static pci_ers_result_t ef4_io_error_detected(struct pci_dev *pdev,
					      pci_channel_state_t state)
{
	pci_ers_result_t status = PCI_ERS_RESULT_RECOVERED;
	struct ef4_nic *efx = pci_get_drvdata(pdev);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	rtnl_lock();

	if (efx->state != STATE_DISABLED) {
		efx->state = STATE_RECOVERY;
		efx->reset_pending = 0;

		ef4_device_detach_sync(efx);

		ef4_stop_all(efx);
		ef4_disable_interrupts(efx);

		status = PCI_ERS_RESULT_NEED_RESET;
	} else {
		 
		status = PCI_ERS_RESULT_RECOVERED;
	}

	rtnl_unlock();

	pci_disable_device(pdev);

	return status;
}

 
static pci_ers_result_t ef4_io_slot_reset(struct pci_dev *pdev)
{
	struct ef4_nic *efx = pci_get_drvdata(pdev);
	pci_ers_result_t status = PCI_ERS_RESULT_RECOVERED;

	if (pci_enable_device(pdev)) {
		netif_err(efx, hw, efx->net_dev,
			  "Cannot re-enable PCI device after reset.\n");
		status =  PCI_ERS_RESULT_DISCONNECT;
	}

	return status;
}

 
static void ef4_io_resume(struct pci_dev *pdev)
{
	struct ef4_nic *efx = pci_get_drvdata(pdev);
	int rc;

	rtnl_lock();

	if (efx->state == STATE_DISABLED)
		goto out;

	rc = ef4_reset(efx, RESET_TYPE_ALL);
	if (rc) {
		netif_err(efx, hw, efx->net_dev,
			  "ef4_reset failed after PCI error (%d)\n", rc);
	} else {
		efx->state = STATE_READY;
		netif_dbg(efx, hw, efx->net_dev,
			  "Done resetting and resuming IO after PCI error.\n");
	}

out:
	rtnl_unlock();
}

 
static const struct pci_error_handlers ef4_err_handlers = {
	.error_detected = ef4_io_error_detected,
	.slot_reset	= ef4_io_slot_reset,
	.resume		= ef4_io_resume,
};

static struct pci_driver ef4_pci_driver = {
	.name		= KBUILD_MODNAME,
	.id_table	= ef4_pci_table,
	.probe		= ef4_pci_probe,
	.remove		= ef4_pci_remove,
	.driver.pm	= &ef4_pm_ops,
	.err_handler	= &ef4_err_handlers,
};

 

module_param(interrupt_mode, uint, 0444);
MODULE_PARM_DESC(interrupt_mode,
		 "Interrupt mode (0=>MSIX 1=>MSI 2=>legacy)");

static int __init ef4_init_module(void)
{
	int rc;

	printk(KERN_INFO "Solarflare Falcon driver v" EF4_DRIVER_VERSION "\n");

	rc = register_netdevice_notifier(&ef4_netdev_notifier);
	if (rc)
		goto err_notifier;

	reset_workqueue = create_singlethread_workqueue("sfc_reset");
	if (!reset_workqueue) {
		rc = -ENOMEM;
		goto err_reset;
	}

	rc = pci_register_driver(&ef4_pci_driver);
	if (rc < 0)
		goto err_pci;

	return 0;

 err_pci:
	destroy_workqueue(reset_workqueue);
 err_reset:
	unregister_netdevice_notifier(&ef4_netdev_notifier);
 err_notifier:
	return rc;
}

static void __exit ef4_exit_module(void)
{
	printk(KERN_INFO "Solarflare Falcon driver unloading\n");

	pci_unregister_driver(&ef4_pci_driver);
	destroy_workqueue(reset_workqueue);
	unregister_netdevice_notifier(&ef4_netdev_notifier);

}

module_init(ef4_init_module);
module_exit(ef4_exit_module);

MODULE_AUTHOR("Solarflare Communications and "
	      "Michael Brown <mbrown@fensystems.co.uk>");
MODULE_DESCRIPTION("Solarflare Falcon network driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, ef4_pci_table);
MODULE_VERSION(EF4_DRIVER_VERSION);
