
 

#include "net_driver.h"
#include <linux/filter.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <net/gre.h>
#include "efx_common.h"
#include "efx_channels.h"
#include "efx.h"
#include "mcdi.h"
#include "selftest.h"
#include "rx_common.h"
#include "tx_common.h"
#include "nic.h"
#include "mcdi_port_common.h"
#include "io.h"
#include "mcdi_pcol.h"
#include "ef100_rep.h"

static unsigned int debug = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
			     NETIF_MSG_LINK | NETIF_MSG_IFDOWN |
			     NETIF_MSG_IFUP | NETIF_MSG_RX_ERR |
			     NETIF_MSG_TX_ERR | NETIF_MSG_HW);
module_param(debug, uint, 0);
MODULE_PARM_DESC(debug, "Bitmapped debugging message enable value");

 
static unsigned int efx_monitor_interval = 1 * HZ;

 
#define BIST_WAIT_DELAY_MS	100
#define BIST_WAIT_DELAY_COUNT	100

 
#define STATS_PERIOD_MS_DEFAULT 1000

static const unsigned int efx_reset_type_max = RESET_TYPE_MAX;
static const char *const efx_reset_type_names[] = {
	[RESET_TYPE_INVISIBLE]          = "INVISIBLE",
	[RESET_TYPE_ALL]                = "ALL",
	[RESET_TYPE_RECOVER_OR_ALL]     = "RECOVER_OR_ALL",
	[RESET_TYPE_WORLD]              = "WORLD",
	[RESET_TYPE_RECOVER_OR_DISABLE] = "RECOVER_OR_DISABLE",
	[RESET_TYPE_DATAPATH]           = "DATAPATH",
	[RESET_TYPE_MC_BIST]		= "MC_BIST",
	[RESET_TYPE_DISABLE]            = "DISABLE",
	[RESET_TYPE_TX_WATCHDOG]        = "TX_WATCHDOG",
	[RESET_TYPE_INT_ERROR]          = "INT_ERROR",
	[RESET_TYPE_DMA_ERROR]          = "DMA_ERROR",
	[RESET_TYPE_TX_SKIP]            = "TX_SKIP",
	[RESET_TYPE_MC_FAILURE]         = "MC_FAILURE",
	[RESET_TYPE_MCDI_TIMEOUT]	= "MCDI_TIMEOUT (FLR)",
};

#define RESET_TYPE(type) \
	STRING_TABLE_LOOKUP(type, efx_reset_type)

 
const unsigned int efx_loopback_mode_max = LOOPBACK_MAX;
const char *const efx_loopback_mode_names[] = {
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

 
static struct workqueue_struct *reset_workqueue;

int efx_create_reset_workqueue(void)
{
	reset_workqueue = create_singlethread_workqueue("sfc_reset");
	if (!reset_workqueue) {
		printk(KERN_ERR "Failed to create reset workqueue\n");
		return -ENOMEM;
	}

	return 0;
}

void efx_queue_reset_work(struct efx_nic *efx)
{
	queue_work(reset_workqueue, &efx->reset_work);
}

void efx_flush_reset_workqueue(struct efx_nic *efx)
{
	cancel_work_sync(&efx->reset_work);
}

void efx_destroy_reset_workqueue(void)
{
	if (reset_workqueue) {
		destroy_workqueue(reset_workqueue);
		reset_workqueue = NULL;
	}
}

 
void efx_mac_reconfigure(struct efx_nic *efx, bool mtu_only)
{
	if (efx->type->reconfigure_mac) {
		down_read(&efx->filter_sem);
		efx->type->reconfigure_mac(efx, mtu_only);
		up_read(&efx->filter_sem);
	}
}

 
static void efx_mac_work(struct work_struct *data)
{
	struct efx_nic *efx = container_of(data, struct efx_nic, mac_work);

	mutex_lock(&efx->mac_lock);
	if (efx->port_enabled)
		efx_mac_reconfigure(efx, false);
	mutex_unlock(&efx->mac_lock);
}

int efx_set_mac_address(struct net_device *net_dev, void *data)
{
	struct efx_nic *efx = efx_netdev_priv(net_dev);
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
	efx_mac_reconfigure(efx, false);
	mutex_unlock(&efx->mac_lock);

	return 0;
}

 
void efx_set_rx_mode(struct net_device *net_dev)
{
	struct efx_nic *efx = efx_netdev_priv(net_dev);

	if (efx->port_enabled)
		queue_work(efx->workqueue, &efx->mac_work);
	 
}

int efx_set_features(struct net_device *net_dev, netdev_features_t data)
{
	struct efx_nic *efx = efx_netdev_priv(net_dev);
	int rc;

	 
	if (net_dev->features & ~data & NETIF_F_NTUPLE) {
		rc = efx->type->filter_clear_rx(efx, EFX_FILTER_PRI_MANUAL);
		if (rc)
			return rc;
	}

	 
	if ((net_dev->features ^ data) & (NETIF_F_HW_VLAN_CTAG_FILTER |
					  NETIF_F_RXFCS)) {
		 
		efx_set_rx_mode(net_dev);
	}

	return 0;
}

 
void efx_link_status_changed(struct efx_nic *efx)
{
	struct efx_link_state *link_state = &efx->link_state;

	 
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

unsigned int efx_xdp_max_mtu(struct efx_nic *efx)
{
	 
	int overhead = EFX_MAX_FRAME_LEN(0) + sizeof(struct efx_rx_page_state) +
		       efx->rx_prefix_size + efx->type->rx_buffer_padding +
		       efx->rx_ip_align + EFX_XDP_HEADROOM + EFX_XDP_TAILROOM;

	return PAGE_SIZE - overhead;
}

 
int efx_change_mtu(struct net_device *net_dev, int new_mtu)
{
	struct efx_nic *efx = efx_netdev_priv(net_dev);
	int rc;

	rc = efx_check_disabled(efx);
	if (rc)
		return rc;

	if (rtnl_dereference(efx->xdp_prog) &&
	    new_mtu > efx_xdp_max_mtu(efx)) {
		netif_err(efx, drv, efx->net_dev,
			  "Requested MTU of %d too big for XDP (max: %d)\n",
			  new_mtu, efx_xdp_max_mtu(efx));
		return -EINVAL;
	}

	netif_dbg(efx, drv, efx->net_dev, "changing MTU to %d\n", new_mtu);

	efx_device_detach_sync(efx);
	efx_stop_all(efx);

	mutex_lock(&efx->mac_lock);
	net_dev->mtu = new_mtu;
	efx_mac_reconfigure(efx, true);
	mutex_unlock(&efx->mac_lock);

	efx_start_all(efx);
	efx_device_attach_if_not_resetting(efx);
	return 0;
}

 

 
static void efx_monitor(struct work_struct *data)
{
	struct efx_nic *efx = container_of(data, struct efx_nic,
					   monitor_work.work);

	netif_vdbg(efx, timer, efx->net_dev,
		   "hardware monitor executing on CPU %d\n",
		   raw_smp_processor_id());
	BUG_ON(efx->type->monitor == NULL);

	 
	if (mutex_trylock(&efx->mac_lock)) {
		if (efx->port_enabled && efx->type->monitor)
			efx->type->monitor(efx);
		mutex_unlock(&efx->mac_lock);
	}

	efx_start_monitor(efx);
}

void efx_start_monitor(struct efx_nic *efx)
{
	if (efx->type->monitor)
		queue_delayed_work(efx->workqueue, &efx->monitor_work,
				   efx_monitor_interval);
}

 

 
static void efx_start_datapath(struct efx_nic *efx)
{
	netdev_features_t old_features = efx->net_dev->features;
	bool old_rx_scatter = efx->rx_scatter;
	size_t rx_buf_len;

	 
	efx->rx_dma_len = (efx->rx_prefix_size +
			   EFX_MAX_FRAME_LEN(efx->net_dev->mtu) +
			   efx->type->rx_buffer_padding);
	rx_buf_len = (sizeof(struct efx_rx_page_state)   + EFX_XDP_HEADROOM +
		      efx->rx_ip_align + efx->rx_dma_len + EFX_XDP_TAILROOM);

	if (rx_buf_len <= PAGE_SIZE) {
		efx->rx_scatter = efx->type->always_rx_scatter;
		efx->rx_buffer_order = 0;
	} else if (efx->type->can_rx_scatter) {
		BUILD_BUG_ON(EFX_RX_USR_BUF_SIZE % L1_CACHE_BYTES);
		BUILD_BUG_ON(sizeof(struct efx_rx_page_state) +
			     2 * ALIGN(NET_IP_ALIGN + EFX_RX_USR_BUF_SIZE,
				       EFX_RX_BUF_ALIGNMENT) >
			     PAGE_SIZE);
		efx->rx_scatter = true;
		efx->rx_dma_len = EFX_RX_USR_BUF_SIZE;
		efx->rx_buffer_order = 0;
	} else {
		efx->rx_scatter = false;
		efx->rx_buffer_order = get_order(rx_buf_len);
	}

	efx_rx_config_page_split(efx);
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

	 
	if ((efx->rx_scatter != old_rx_scatter) &&
	    efx->type->filter_update_rx_scatter)
		efx->type->filter_update_rx_scatter(efx);

	 
	efx->txq_stop_thresh = efx->txq_entries - efx_tx_max_skb_descs(efx);
	efx->txq_wake_thresh = efx->txq_stop_thresh / 2;

	 
	efx_start_channels(efx);

	efx_ptp_start_datapath(efx);

	if (netif_device_present(efx->net_dev))
		netif_tx_wake_all_queues(efx->net_dev);
}

static void efx_stop_datapath(struct efx_nic *efx)
{
	EFX_ASSERT_RESET_SERIALISED(efx);
	BUG_ON(efx->port_enabled);

	efx_ptp_stop_datapath(efx);

	efx_stop_channels(efx);
}

 

 
void efx_link_clear_advertising(struct efx_nic *efx)
{
	bitmap_zero(efx->link_advertising, __ETHTOOL_LINK_MODE_MASK_NBITS);
	efx->wanted_fc &= ~(EFX_FC_TX | EFX_FC_RX);
}

void efx_link_set_wanted_fc(struct efx_nic *efx, u8 wanted_fc)
{
	efx->wanted_fc = wanted_fc;
	if (efx->link_advertising[0]) {
		if (wanted_fc & EFX_FC_RX)
			efx->link_advertising[0] |= (ADVERTISED_Pause |
						     ADVERTISED_Asym_Pause);
		else
			efx->link_advertising[0] &= ~(ADVERTISED_Pause |
						      ADVERTISED_Asym_Pause);
		if (wanted_fc & EFX_FC_TX)
			efx->link_advertising[0] ^= ADVERTISED_Asym_Pause;
	}
}

static void efx_start_port(struct efx_nic *efx)
{
	netif_dbg(efx, ifup, efx->net_dev, "start port\n");
	BUG_ON(efx->port_enabled);

	mutex_lock(&efx->mac_lock);
	efx->port_enabled = true;

	 
	efx_mac_reconfigure(efx, false);

	mutex_unlock(&efx->mac_lock);
}

 
static void efx_stop_port(struct efx_nic *efx)
{
	netif_dbg(efx, ifdown, efx->net_dev, "stop port\n");

	EFX_ASSERT_RESET_SERIALISED(efx);

	mutex_lock(&efx->mac_lock);
	efx->port_enabled = false;
	mutex_unlock(&efx->mac_lock);

	 
	netif_addr_lock_bh(efx->net_dev);
	netif_addr_unlock_bh(efx->net_dev);

	cancel_delayed_work_sync(&efx->monitor_work);
	efx_selftest_async_cancel(efx);
	cancel_work_sync(&efx->mac_work);
}

 
void efx_start_all(struct efx_nic *efx)
{
	EFX_ASSERT_RESET_SERIALISED(efx);
	BUG_ON(efx->state == STATE_DISABLED);

	 
	if (efx->port_enabled || !netif_running(efx->net_dev) ||
	    efx->reset_pending)
		return;

	efx_start_port(efx);
	efx_start_datapath(efx);

	 
	efx_start_monitor(efx);

	efx_selftest_async_start(efx);

	 
	mutex_lock(&efx->mac_lock);
	if (efx_mcdi_phy_poll(efx))
		efx_link_status_changed(efx);
	mutex_unlock(&efx->mac_lock);

	if (efx->type->start_stats) {
		efx->type->start_stats(efx);
		efx->type->pull_stats(efx);
		spin_lock_bh(&efx->stats_lock);
		efx->type->update_stats(efx, NULL, NULL);
		spin_unlock_bh(&efx->stats_lock);
	}
}

 
void efx_stop_all(struct efx_nic *efx)
{
	EFX_ASSERT_RESET_SERIALISED(efx);

	 
	if (!efx->port_enabled)
		return;

	if (efx->type->update_stats) {
		 
		efx->type->pull_stats(efx);
		spin_lock_bh(&efx->stats_lock);
		efx->type->update_stats(efx, NULL, NULL);
		spin_unlock_bh(&efx->stats_lock);
		efx->type->stop_stats(efx);
	}

	efx_stop_port(efx);

	 
	WARN_ON(netif_running(efx->net_dev) &&
		netif_device_present(efx->net_dev));
	netif_tx_disable(efx->net_dev);

	efx_stop_datapath(efx);
}

 
void efx_net_stats(struct net_device *net_dev, struct rtnl_link_stats64 *stats)
{
	struct efx_nic *efx = efx_netdev_priv(net_dev);

	spin_lock_bh(&efx->stats_lock);
	efx_nic_update_stats_atomic(efx, NULL, stats);
	spin_unlock_bh(&efx->stats_lock);
}

 
int __efx_reconfigure_port(struct efx_nic *efx)
{
	enum efx_phy_mode phy_mode;
	int rc = 0;

	WARN_ON(!mutex_is_locked(&efx->mac_lock));

	 
	phy_mode = efx->phy_mode;
	if (LOOPBACK_INTERNAL(efx))
		efx->phy_mode |= PHY_MODE_TX_DISABLED;
	else
		efx->phy_mode &= ~PHY_MODE_TX_DISABLED;

	if (efx->type->reconfigure_port)
		rc = efx->type->reconfigure_port(efx);

	if (rc)
		efx->phy_mode = phy_mode;

	return rc;
}

 
int efx_reconfigure_port(struct efx_nic *efx)
{
	int rc;

	EFX_ASSERT_RESET_SERIALISED(efx);

	mutex_lock(&efx->mac_lock);
	rc = __efx_reconfigure_port(efx);
	mutex_unlock(&efx->mac_lock);

	return rc;
}

 

static void efx_wait_for_bist_end(struct efx_nic *efx)
{
	int i;

	for (i = 0; i < BIST_WAIT_DELAY_COUNT; ++i) {
		if (efx_mcdi_poll_reboot(efx))
			goto out;
		msleep(BIST_WAIT_DELAY_MS);
	}

	netif_err(efx, drv, efx->net_dev, "Warning: No MC reboot after BIST mode\n");
out:
	 
	efx->mc_bist_for_other_fn = false;
}

 
int efx_try_recovery(struct efx_nic *efx)
{
#ifdef CONFIG_EEH
	 
	struct eeh_dev *eehdev = pci_dev_to_eeh_dev(efx->pci_dev);
	if (eeh_dev_check_failure(eehdev)) {
		 
		return 1;
	}
#endif
	return 0;
}

 
void efx_reset_down(struct efx_nic *efx, enum reset_type method)
{
	EFX_ASSERT_RESET_SERIALISED(efx);

	if (method == RESET_TYPE_MCDI_TIMEOUT)
		efx->type->prepare_flr(efx);

	efx_stop_all(efx);
	efx_disable_interrupts(efx);

	mutex_lock(&efx->mac_lock);
	down_write(&efx->filter_sem);
	mutex_lock(&efx->rss_lock);
	efx->type->fini(efx);
}

 
void efx_watchdog(struct net_device *net_dev, unsigned int txqueue)
{
	struct efx_nic *efx = efx_netdev_priv(net_dev);

	netif_err(efx, tx_err, efx->net_dev,
		  "TX stuck with port_enabled=%d: resetting channels\n",
		  efx->port_enabled);

	efx_schedule_reset(efx, RESET_TYPE_TX_WATCHDOG);
}

 
int efx_reset_up(struct efx_nic *efx, enum reset_type method, bool ok)
{
	int rc;

	EFX_ASSERT_RESET_SERIALISED(efx);

	if (method == RESET_TYPE_MCDI_TIMEOUT)
		efx->type->finish_flr(efx);

	 
	rc = efx->type->init(efx);
	if (rc) {
		netif_err(efx, drv, efx->net_dev, "failed to initialise NIC\n");
		goto fail;
	}

	if (!ok)
		goto fail;

	if (efx->port_initialized && method != RESET_TYPE_INVISIBLE &&
	    method != RESET_TYPE_DATAPATH) {
		rc = efx_mcdi_port_reconfigure(efx);
		if (rc && rc != -EPERM)
			netif_err(efx, drv, efx->net_dev,
				  "could not restore PHY settings\n");
	}

	rc = efx_enable_interrupts(efx);
	if (rc)
		goto fail;

#ifdef CONFIG_SFC_SRIOV
	rc = efx->type->vswitching_restore(efx);
	if (rc)  
		netif_warn(efx, probe, efx->net_dev,
			   "failed to restore vswitching rc=%d;"
			   " VFs may not function\n", rc);
#endif

	if (efx->type->rx_restore_rss_contexts)
		efx->type->rx_restore_rss_contexts(efx);
	mutex_unlock(&efx->rss_lock);
	efx->type->filter_table_restore(efx);
	up_write(&efx->filter_sem);

	mutex_unlock(&efx->mac_lock);

	efx_start_all(efx);

	if (efx->type->udp_tnl_push_ports)
		efx->type->udp_tnl_push_ports(efx);

	return 0;

fail:
	efx->port_initialized = false;

	mutex_unlock(&efx->rss_lock);
	up_write(&efx->filter_sem);
	mutex_unlock(&efx->mac_lock);

	return rc;
}

 
int efx_reset(struct efx_nic *efx, enum reset_type method)
{
	int rc, rc2 = 0;
	bool disabled;

	netif_info(efx, drv, efx->net_dev, "resetting (%s)\n",
		   RESET_TYPE(method));

	efx_device_detach_sync(efx);
	 
	if (efx_nic_rev(efx) != EFX_REV_EF100)
		efx_reset_down(efx, method);

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
	if (efx_nic_rev(efx) != EFX_REV_EF100)
		rc2 = efx_reset_up(efx, method, !disabled);
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
		efx_device_attach_if_not_resetting(efx);
	}
	return rc;
}

 
static void efx_reset_work(struct work_struct *data)
{
	struct efx_nic *efx = container_of(data, struct efx_nic, reset_work);
	unsigned long pending;
	enum reset_type method;

	pending = READ_ONCE(efx->reset_pending);
	method = fls(pending) - 1;

	if (method == RESET_TYPE_MC_BIST)
		efx_wait_for_bist_end(efx);

	if ((method == RESET_TYPE_RECOVER_OR_DISABLE ||
	     method == RESET_TYPE_RECOVER_OR_ALL) &&
	    efx_try_recovery(efx))
		return;

	if (!pending)
		return;

	rtnl_lock();

	 
	if (efx_net_active(efx->state))
		(void)efx_reset(efx, method);

	rtnl_unlock();
}

void efx_schedule_reset(struct efx_nic *efx, enum reset_type type)
{
	enum reset_type method;

	if (efx_recovering(efx->state)) {
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
	case RESET_TYPE_MC_BIST:
	case RESET_TYPE_MCDI_TIMEOUT:
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

	 
	if (!efx_net_active(READ_ONCE(efx->state)))
		return;

	 
	efx_mcdi_mode_poll(efx);

	efx_queue_reset_work(efx);
}

 
int efx_port_dummy_op_int(struct efx_nic *efx)
{
	return 0;
}
void efx_port_dummy_op_void(struct efx_nic *efx) {}

 

 
int efx_init_struct(struct efx_nic *efx, struct pci_dev *pci_dev)
{
	int rc = -ENOMEM;

	 
	INIT_LIST_HEAD(&efx->node);
	INIT_LIST_HEAD(&efx->secondary_list);
	spin_lock_init(&efx->biu_lock);
#ifdef CONFIG_SFC_MTD
	INIT_LIST_HEAD(&efx->mtd_list);
#endif
	INIT_WORK(&efx->reset_work, efx_reset_work);
	INIT_DELAYED_WORK(&efx->monitor_work, efx_monitor);
	efx_selftest_async_init(efx);
	efx->pci_dev = pci_dev;
	efx->msg_enable = debug;
	efx->state = STATE_UNINIT;
	strscpy(efx->name, pci_name(pci_dev), sizeof(efx->name));

	efx->rx_prefix_size = efx->type->rx_prefix_size;
	efx->rx_ip_align =
		NET_IP_ALIGN ? (efx->rx_prefix_size + NET_IP_ALIGN) % 4 : 0;
	efx->rx_packet_hash_offset =
		efx->type->rx_hash_offset - efx->type->rx_prefix_size;
	efx->rx_packet_ts_offset =
		efx->type->rx_ts_offset - efx->type->rx_prefix_size;
	INIT_LIST_HEAD(&efx->rss_context.list);
	efx->rss_context.context_id = EFX_MCDI_RSS_CONTEXT_INVALID;
	mutex_init(&efx->rss_lock);
	efx->vport_id = EVB_PORT_ID_ASSIGNED;
	spin_lock_init(&efx->stats_lock);
	efx->vi_stride = EFX_DEFAULT_VI_STRIDE;
	efx->num_mac_stats = MC_CMD_MAC_NSTATS;
	BUILD_BUG_ON(MC_CMD_MAC_NSTATS - 1 != MC_CMD_MAC_GENERATION_END);
	mutex_init(&efx->mac_lock);
	init_rwsem(&efx->filter_sem);
#ifdef CONFIG_RFS_ACCEL
	mutex_init(&efx->rps_mutex);
	spin_lock_init(&efx->rps_hash_lock);
	 
	efx->rps_hash_table = kcalloc(EFX_ARFS_HASH_TABLE_SIZE,
				      sizeof(*efx->rps_hash_table), GFP_KERNEL);
#endif
	spin_lock_init(&efx->vf_reps_lock);
	INIT_LIST_HEAD(&efx->vf_reps);
	INIT_WORK(&efx->mac_work, efx_mac_work);
	init_waitqueue_head(&efx->flush_wq);

	efx->tx_queues_per_channel = 1;
	efx->rxq_entries = EFX_DEFAULT_DMAQ_SIZE;
	efx->txq_entries = EFX_DEFAULT_DMAQ_SIZE;

	efx->mem_bar = UINT_MAX;

	rc = efx_init_channels(efx);
	if (rc)
		goto fail;

	 
	snprintf(efx->workqueue_name, sizeof(efx->workqueue_name), "sfc%s",
		 pci_name(pci_dev));
	efx->workqueue = create_singlethread_workqueue(efx->workqueue_name);
	if (!efx->workqueue) {
		rc = -ENOMEM;
		goto fail;
	}

	return 0;

fail:
	efx_fini_struct(efx);
	return rc;
}

void efx_fini_struct(struct efx_nic *efx)
{
#ifdef CONFIG_RFS_ACCEL
	kfree(efx->rps_hash_table);
#endif

	efx_fini_channels(efx);

	kfree(efx->vpd_sn);

	if (efx->workqueue) {
		destroy_workqueue(efx->workqueue);
		efx->workqueue = NULL;
	}
}

 
int efx_init_io(struct efx_nic *efx, int bar, dma_addr_t dma_mask,
		unsigned int mem_map_size)
{
	struct pci_dev *pci_dev = efx->pci_dev;
	int rc;

	efx->mem_bar = UINT_MAX;
	pci_dbg(pci_dev, "initialising I/O bar=%d\n", bar);

	rc = pci_enable_device(pci_dev);
	if (rc) {
		pci_err(pci_dev, "failed to enable PCI device\n");
		goto fail1;
	}

	pci_set_master(pci_dev);

	rc = dma_set_mask_and_coherent(&pci_dev->dev, dma_mask);
	if (rc) {
		pci_err(efx->pci_dev, "could not find a suitable DMA mask\n");
		goto fail2;
	}
	pci_dbg(efx->pci_dev, "using DMA mask %llx\n", (unsigned long long)dma_mask);

	efx->membase_phys = pci_resource_start(efx->pci_dev, bar);
	if (!efx->membase_phys) {
		pci_err(efx->pci_dev,
			"ERROR: No BAR%d mapping from the BIOS. Try pci=realloc on the kernel command line\n",
			bar);
		rc = -ENODEV;
		goto fail3;
	}

	rc = pci_request_region(pci_dev, bar, "sfc");
	if (rc) {
		pci_err(efx->pci_dev,
			"request for memory BAR[%d] failed\n", bar);
		rc = -EIO;
		goto fail3;
	}
	efx->mem_bar = bar;
	efx->membase = ioremap(efx->membase_phys, mem_map_size);
	if (!efx->membase) {
		pci_err(efx->pci_dev,
			"could not map memory BAR[%d] at %llx+%x\n", bar,
			(unsigned long long)efx->membase_phys, mem_map_size);
		rc = -ENOMEM;
		goto fail4;
	}
	pci_dbg(efx->pci_dev,
		"memory BAR[%d] at %llx+%x (virtual %p)\n", bar,
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

void efx_fini_io(struct efx_nic *efx)
{
	pci_dbg(efx->pci_dev, "shutting down I/O\n");

	if (efx->membase) {
		iounmap(efx->membase);
		efx->membase = NULL;
	}

	if (efx->membase_phys) {
		pci_release_region(efx->pci_dev, efx->mem_bar);
		efx->membase_phys = 0;
		efx->mem_bar = UINT_MAX;
	}

	 
	if (!pci_vfs_assigned(efx->pci_dev))
		pci_disable_device(efx->pci_dev);
}

#ifdef CONFIG_SFC_MCDI_LOGGING
static ssize_t mcdi_logging_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct efx_nic *efx = dev_get_drvdata(dev);
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);

	return sysfs_emit(buf, "%d\n", mcdi->logging_enabled);
}

static ssize_t mcdi_logging_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct efx_nic *efx = dev_get_drvdata(dev);
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);
	bool enable = count > 0 && *buf != '0';

	mcdi->logging_enabled = enable;
	return count;
}

static DEVICE_ATTR_RW(mcdi_logging);

void efx_init_mcdi_logging(struct efx_nic *efx)
{
	int rc = device_create_file(&efx->pci_dev->dev, &dev_attr_mcdi_logging);

	if (rc) {
		netif_warn(efx, drv, efx->net_dev,
			   "failed to init net dev attributes\n");
	}
}

void efx_fini_mcdi_logging(struct efx_nic *efx)
{
	device_remove_file(&efx->pci_dev->dev, &dev_attr_mcdi_logging);
}
#endif

 
static pci_ers_result_t efx_io_error_detected(struct pci_dev *pdev,
					      pci_channel_state_t state)
{
	pci_ers_result_t status = PCI_ERS_RESULT_RECOVERED;
	struct efx_nic *efx = pci_get_drvdata(pdev);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	rtnl_lock();

	if (efx->state != STATE_DISABLED) {
		efx->state = efx_recover(efx->state);
		efx->reset_pending = 0;

		efx_device_detach_sync(efx);

		if (efx_net_active(efx->state)) {
			efx_stop_all(efx);
			efx_disable_interrupts(efx);
		}

		status = PCI_ERS_RESULT_NEED_RESET;
	} else {
		 
		status = PCI_ERS_RESULT_RECOVERED;
	}

	rtnl_unlock();

	pci_disable_device(pdev);

	return status;
}

 
static pci_ers_result_t efx_io_slot_reset(struct pci_dev *pdev)
{
	struct efx_nic *efx = pci_get_drvdata(pdev);
	pci_ers_result_t status = PCI_ERS_RESULT_RECOVERED;

	if (pci_enable_device(pdev)) {
		netif_err(efx, hw, efx->net_dev,
			  "Cannot re-enable PCI device after reset.\n");
		status =  PCI_ERS_RESULT_DISCONNECT;
	}

	return status;
}

 
static void efx_io_resume(struct pci_dev *pdev)
{
	struct efx_nic *efx = pci_get_drvdata(pdev);
	int rc;

	rtnl_lock();

	if (efx->state == STATE_DISABLED)
		goto out;

	rc = efx_reset(efx, RESET_TYPE_ALL);
	if (rc) {
		netif_err(efx, hw, efx->net_dev,
			  "efx_reset failed after PCI error (%d)\n", rc);
	} else {
		efx->state = efx_recovered(efx->state);
		netif_dbg(efx, hw, efx->net_dev,
			  "Done resetting and resuming IO after PCI error.\n");
	}

out:
	rtnl_unlock();
}

 
const struct pci_error_handlers efx_err_handlers = {
	.error_detected = efx_io_error_detected,
	.slot_reset	= efx_io_slot_reset,
	.resume		= efx_io_resume,
};

 
static bool efx_can_encap_offloads(struct efx_nic *efx, struct sk_buff *skb)
{
	struct gre_base_hdr *greh;
	__be16 dst_port;
	u8 ipproto;

	 
	if (WARN_ON_ONCE(!efx->type->udp_tnl_has_port))
		return false;

	 
	switch (skb->protocol) {
	case htons(ETH_P_IP):
		ipproto = ip_hdr(skb)->protocol;
		break;
	case htons(ETH_P_IPV6):
		 
		ipproto = ipv6_hdr(skb)->nexthdr;
		break;
	default:
		 
		return false;
	}
	switch (ipproto) {
	case IPPROTO_GRE:
		 
		if (skb->inner_protocol_type != ENCAP_TYPE_ETHER)
			return false;
		if (ntohs(skb->inner_protocol) != ETH_P_TEB)
			return false;
		if (skb_inner_mac_header(skb) - skb_transport_header(skb) != 8)
			return false;
		greh = (struct gre_base_hdr *)skb_transport_header(skb);
		return !(greh->flags & (GRE_CSUM | GRE_SEQ));
	case IPPROTO_UDP:
		 
		dst_port = udp_hdr(skb)->dest;
		return efx->type->udp_tnl_has_port(efx, dst_port);
	default:
		return false;
	}
}

netdev_features_t efx_features_check(struct sk_buff *skb, struct net_device *dev,
				     netdev_features_t features)
{
	struct efx_nic *efx = efx_netdev_priv(dev);

	if (skb->encapsulation) {
		if (features & NETIF_F_GSO_MASK)
			 
			if (skb_inner_transport_offset(skb) >
			    EFX_TSO2_MAX_HDRLEN)
				features &= ~(NETIF_F_GSO_MASK);
		if (features & (NETIF_F_GSO_MASK | NETIF_F_CSUM_MASK))
			if (!efx_can_encap_offloads(efx, skb))
				features &= ~(NETIF_F_GSO_MASK |
					      NETIF_F_CSUM_MASK);
	}
	return features;
}

int efx_get_phys_port_id(struct net_device *net_dev,
			 struct netdev_phys_item_id *ppid)
{
	struct efx_nic *efx = efx_netdev_priv(net_dev);

	if (efx->type->get_phys_port_id)
		return efx->type->get_phys_port_id(efx, ppid);
	else
		return -EOPNOTSUPP;
}

int efx_get_phys_port_name(struct net_device *net_dev, char *name, size_t len)
{
	struct efx_nic *efx = efx_netdev_priv(net_dev);

	if (snprintf(name, len, "p%u", efx->port_num) >= len)
		return -EINVAL;
	return 0;
}

void efx_detach_reps(struct efx_nic *efx)
{
	struct net_device *rep_dev;
	struct efx_rep *efv;

	ASSERT_RTNL();
	netif_dbg(efx, drv, efx->net_dev, "Detaching VF representors\n");
	list_for_each_entry(efv, &efx->vf_reps, list) {
		rep_dev = efv->net_dev;
		if (!rep_dev)
			continue;
		netif_carrier_off(rep_dev);
		 
		netif_tx_lock_bh(rep_dev);
		netif_tx_stop_all_queues(rep_dev);
		netif_tx_unlock_bh(rep_dev);
	}
}

void efx_attach_reps(struct efx_nic *efx)
{
	struct net_device *rep_dev;
	struct efx_rep *efv;

	ASSERT_RTNL();
	netif_dbg(efx, drv, efx->net_dev, "Attaching VF representors\n");
	list_for_each_entry(efv, &efx->vf_reps, list) {
		rep_dev = efv->net_dev;
		if (!rep_dev)
			continue;
		netif_tx_wake_all_queues(rep_dev);
		netif_carrier_on(rep_dev);
	}
}
