
 

#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/etherdevice.h>
#include <linux/phylink.h>
#include <net/ip.h>
#include <linux/if_vlan.h>

#include "../libwx/wx_type.h"
#include "../libwx/wx_lib.h"
#include "../libwx/wx_hw.h"
#include "txgbe_type.h"
#include "txgbe_hw.h"
#include "txgbe_phy.h"
#include "txgbe_ethtool.h"

char txgbe_driver_name[] = "txgbe";

 
static const struct pci_device_id txgbe_pci_tbl[] = {
	{ PCI_VDEVICE(WANGXUN, TXGBE_DEV_ID_SP1000), 0},
	{ PCI_VDEVICE(WANGXUN, TXGBE_DEV_ID_WX1820), 0},
	 
	{ .device = 0 }
};

#define DEFAULT_DEBUG_LEVEL_SHIFT 3

static void txgbe_check_minimum_link(struct wx *wx)
{
	struct pci_dev *pdev;

	pdev = wx->pdev;
	pcie_print_link_status(pdev);
}

 
static int txgbe_enumerate_functions(struct wx *wx)
{
	struct pci_dev *entry, *pdev = wx->pdev;
	int physfns = 0;

	list_for_each_entry(entry, &pdev->bus->devices, bus_list) {
		 
		if (entry->vendor != pdev->vendor ||
		    entry->device != pdev->device)
			return -EINVAL;

		physfns++;
	}

	return physfns;
}

 
static void txgbe_irq_enable(struct wx *wx, bool queues)
{
	wr32(wx, WX_PX_MISC_IEN, TXGBE_PX_MISC_IEN_MASK);

	 
	wx_intr_enable(wx, TXGBE_INTR_MISC(wx));
	if (queues)
		wx_intr_enable(wx, TXGBE_INTR_QALL(wx));
}

 
static irqreturn_t txgbe_intr(int __always_unused irq, void *data)
{
	struct wx_q_vector *q_vector;
	struct wx *wx  = data;
	struct pci_dev *pdev;
	u32 eicr;

	q_vector = wx->q_vector[0];
	pdev = wx->pdev;

	eicr = wx_misc_isb(wx, WX_ISB_VEC0);
	if (!eicr) {
		 
		if (netif_running(wx->netdev))
			txgbe_irq_enable(wx, true);
		return IRQ_NONE;         
	}
	wx->isb_mem[WX_ISB_VEC0] = 0;
	if (!(pdev->msi_enabled))
		wr32(wx, WX_PX_INTA, 1);

	wx->isb_mem[WX_ISB_MISC] = 0;
	 
	napi_schedule_irqoff(&q_vector->napi);

	 
	if (netif_running(wx->netdev))
		txgbe_irq_enable(wx, false);

	return IRQ_HANDLED;
}

 
static int txgbe_request_msix_irqs(struct wx *wx)
{
	struct net_device *netdev = wx->netdev;
	int vector, err;

	for (vector = 0; vector < wx->num_q_vectors; vector++) {
		struct wx_q_vector *q_vector = wx->q_vector[vector];
		struct msix_entry *entry = &wx->msix_entries[vector];

		if (q_vector->tx.ring && q_vector->rx.ring)
			snprintf(q_vector->name, sizeof(q_vector->name) - 1,
				 "%s-TxRx-%d", netdev->name, entry->entry);
		else
			 
			continue;

		err = request_irq(entry->vector, wx_msix_clean_rings, 0,
				  q_vector->name, q_vector);
		if (err) {
			wx_err(wx, "request_irq failed for MSIX interrupt %s Error: %d\n",
			       q_vector->name, err);
			goto free_queue_irqs;
		}
	}

	return 0;

free_queue_irqs:
	while (vector) {
		vector--;
		free_irq(wx->msix_entries[vector].vector,
			 wx->q_vector[vector]);
	}
	wx_reset_interrupt_capability(wx);
	return err;
}

 
static int txgbe_request_irq(struct wx *wx)
{
	struct net_device *netdev = wx->netdev;
	struct pci_dev *pdev = wx->pdev;
	int err;

	if (pdev->msix_enabled)
		err = txgbe_request_msix_irqs(wx);
	else if (pdev->msi_enabled)
		err = request_irq(wx->pdev->irq, &txgbe_intr, 0,
				  netdev->name, wx);
	else
		err = request_irq(wx->pdev->irq, &txgbe_intr, IRQF_SHARED,
				  netdev->name, wx);

	if (err)
		wx_err(wx, "request_irq failed, Error %d\n", err);

	return err;
}

static void txgbe_up_complete(struct wx *wx)
{
	struct net_device *netdev = wx->netdev;
	struct txgbe *txgbe;

	wx_control_hw(wx, true);
	wx_configure_vectors(wx);

	 
	smp_mb__before_atomic();
	wx_napi_enable_all(wx);

	txgbe = netdev_to_txgbe(netdev);
	phylink_start(txgbe->phylink);

	 
	rd32(wx, WX_PX_IC(0));
	rd32(wx, WX_PX_IC(1));
	rd32(wx, WX_PX_MISC_IC);
	txgbe_irq_enable(wx, true);

	 
	netif_tx_start_all_queues(netdev);
}

static void txgbe_reset(struct wx *wx)
{
	struct net_device *netdev = wx->netdev;
	u8 old_addr[ETH_ALEN];
	int err;

	err = txgbe_reset_hw(wx);
	if (err != 0)
		wx_err(wx, "Hardware Error: %d\n", err);

	wx_start_hw(wx);
	 
	memcpy(old_addr, &wx->mac_table[0].addr, netdev->addr_len);
	wx_flush_sw_mac_table(wx);
	wx_mac_set_default_filter(wx, old_addr);
}

static void txgbe_disable_device(struct wx *wx)
{
	struct net_device *netdev = wx->netdev;
	u32 i;

	wx_disable_pcie_master(wx);
	 
	wx_disable_rx(wx);

	 
	for (i = 0; i < wx->num_rx_queues; i++)
		 
		wx_disable_rx_queue(wx, wx->rx_ring[i]);

	netif_tx_stop_all_queues(netdev);
	netif_tx_disable(netdev);

	wx_irq_disable(wx);
	wx_napi_disable_all(wx);

	if (wx->bus.func < 2)
		wr32m(wx, TXGBE_MIS_PRB_CTL, TXGBE_MIS_PRB_CTL_LAN_UP(wx->bus.func), 0);
	else
		wx_err(wx, "%s: invalid bus lan id %d\n",
		       __func__, wx->bus.func);

	if (!(((wx->subsystem_device_id & WX_NCSI_MASK) == WX_NCSI_SUP) ||
	      ((wx->subsystem_device_id & WX_WOL_MASK) == WX_WOL_SUP))) {
		 
		wr32m(wx, WX_MAC_TX_CFG, WX_MAC_TX_CFG_TE, 0);
	}

	 
	for (i = 0; i < wx->num_tx_queues; i++) {
		u8 reg_idx = wx->tx_ring[i]->reg_idx;

		wr32(wx, WX_PX_TR_CFG(reg_idx), WX_PX_TR_CFG_SWFLSH);
	}

	 
	wr32m(wx, WX_TDM_CTL, WX_TDM_CTL_TE, 0);
}

static void txgbe_down(struct wx *wx)
{
	struct txgbe *txgbe = netdev_to_txgbe(wx->netdev);

	txgbe_disable_device(wx);
	txgbe_reset(wx);
	phylink_stop(txgbe->phylink);

	wx_clean_all_tx_rings(wx);
	wx_clean_all_rx_rings(wx);
}

 
static void txgbe_init_type_code(struct wx *wx)
{
	u8 device_type = wx->subsystem_device_id & 0xF0;

	switch (wx->device_id) {
	case TXGBE_DEV_ID_SP1000:
	case TXGBE_DEV_ID_WX1820:
		wx->mac.type = wx_mac_sp;
		break;
	default:
		wx->mac.type = wx_mac_unknown;
		break;
	}

	switch (device_type) {
	case TXGBE_ID_SFP:
		wx->media_type = sp_media_fiber;
		break;
	case TXGBE_ID_XAUI:
	case TXGBE_ID_SGMII:
		wx->media_type = sp_media_copper;
		break;
	case TXGBE_ID_KR_KX_KX4:
	case TXGBE_ID_MAC_XAUI:
	case TXGBE_ID_MAC_SGMII:
		wx->media_type = sp_media_backplane;
		break;
	case TXGBE_ID_SFI_XAUI:
		if (wx->bus.func == 0)
			wx->media_type = sp_media_fiber;
		else
			wx->media_type = sp_media_copper;
		break;
	default:
		wx->media_type = sp_media_unknown;
		break;
	}
}

 
static int txgbe_sw_init(struct wx *wx)
{
	u16 msix_count = 0;
	int err;

	wx->mac.num_rar_entries = TXGBE_SP_RAR_ENTRIES;
	wx->mac.max_tx_queues = TXGBE_SP_MAX_TX_QUEUES;
	wx->mac.max_rx_queues = TXGBE_SP_MAX_RX_QUEUES;
	wx->mac.mcft_size = TXGBE_SP_MC_TBL_SIZE;
	wx->mac.vft_size = TXGBE_SP_VFT_TBL_SIZE;
	wx->mac.rx_pb_size = TXGBE_SP_RX_PB_SIZE;
	wx->mac.tx_pb_size = TXGBE_SP_TDB_PB_SZ;

	 
	err = wx_sw_init(wx);
	if (err < 0)
		return err;

	txgbe_init_type_code(wx);

	 
	wx->max_q_vectors = TXGBE_MAX_MSIX_VECTORS;
	err = wx_get_pcie_msix_counts(wx, &msix_count, TXGBE_MAX_MSIX_VECTORS);
	if (err)
		wx_err(wx, "Do not support MSI-X\n");
	wx->mac.max_msix_vectors = msix_count;

	 
	wx->rx_itr_setting = 1;
	wx->tx_itr_setting = 1;

	 
	wx->tx_ring_count = TXGBE_DEFAULT_TXD;
	wx->rx_ring_count = TXGBE_DEFAULT_RXD;

	 
	wx->tx_work_limit = TXGBE_DEFAULT_TX_WORK;
	wx->rx_work_limit = TXGBE_DEFAULT_RX_WORK;

	return 0;
}

 
static int txgbe_open(struct net_device *netdev)
{
	struct wx *wx = netdev_priv(netdev);
	int err;

	err = wx_setup_resources(wx);
	if (err)
		goto err_reset;

	wx_configure(wx);

	err = txgbe_request_irq(wx);
	if (err)
		goto err_free_isb;

	 
	err = netif_set_real_num_tx_queues(netdev, wx->num_tx_queues);
	if (err)
		goto err_free_irq;

	err = netif_set_real_num_rx_queues(netdev, wx->num_rx_queues);
	if (err)
		goto err_free_irq;

	txgbe_up_complete(wx);

	return 0;

err_free_irq:
	wx_free_irq(wx);
err_free_isb:
	wx_free_isb_resources(wx);
err_reset:
	txgbe_reset(wx);

	return err;
}

 
static void txgbe_close_suspend(struct wx *wx)
{
	txgbe_disable_device(wx);
	wx_free_resources(wx);
}

 
static int txgbe_close(struct net_device *netdev)
{
	struct wx *wx = netdev_priv(netdev);

	txgbe_down(wx);
	wx_free_irq(wx);
	wx_free_resources(wx);
	wx_control_hw(wx, false);

	return 0;
}

static void txgbe_dev_shutdown(struct pci_dev *pdev)
{
	struct wx *wx = pci_get_drvdata(pdev);
	struct net_device *netdev;

	netdev = wx->netdev;
	netif_device_detach(netdev);

	rtnl_lock();
	if (netif_running(netdev))
		txgbe_close_suspend(wx);
	rtnl_unlock();

	wx_control_hw(wx, false);

	pci_disable_device(pdev);
}

static void txgbe_shutdown(struct pci_dev *pdev)
{
	txgbe_dev_shutdown(pdev);

	if (system_state == SYSTEM_POWER_OFF) {
		pci_wake_from_d3(pdev, false);
		pci_set_power_state(pdev, PCI_D3hot);
	}
}

static const struct net_device_ops txgbe_netdev_ops = {
	.ndo_open               = txgbe_open,
	.ndo_stop               = txgbe_close,
	.ndo_change_mtu         = wx_change_mtu,
	.ndo_start_xmit         = wx_xmit_frame,
	.ndo_set_rx_mode        = wx_set_rx_mode,
	.ndo_set_features       = wx_set_features,
	.ndo_validate_addr      = eth_validate_addr,
	.ndo_set_mac_address    = wx_set_mac,
	.ndo_get_stats64        = wx_get_stats64,
	.ndo_vlan_rx_add_vid    = wx_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid   = wx_vlan_rx_kill_vid,
};

 
static int txgbe_probe(struct pci_dev *pdev,
		       const struct pci_device_id __always_unused *ent)
{
	struct net_device *netdev;
	int err, expected_gts;
	struct wx *wx = NULL;
	struct txgbe *txgbe;

	u16 eeprom_verh = 0, eeprom_verl = 0, offset = 0;
	u16 eeprom_cfg_blkh = 0, eeprom_cfg_blkl = 0;
	u16 build = 0, major = 0, patch = 0;
	u8 part_str[TXGBE_PBANUM_LENGTH];
	u32 etrack_id = 0;

	err = pci_enable_device_mem(pdev);
	if (err)
		return err;

	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (err) {
		dev_err(&pdev->dev,
			"No usable DMA configuration, aborting\n");
		goto err_pci_disable_dev;
	}

	err = pci_request_selected_regions(pdev,
					   pci_select_bars(pdev, IORESOURCE_MEM),
					   txgbe_driver_name);
	if (err) {
		dev_err(&pdev->dev,
			"pci_request_selected_regions failed 0x%x\n", err);
		goto err_pci_disable_dev;
	}

	pci_set_master(pdev);

	netdev = devm_alloc_etherdev_mqs(&pdev->dev,
					 sizeof(struct wx),
					 TXGBE_MAX_TX_QUEUES,
					 TXGBE_MAX_RX_QUEUES);
	if (!netdev) {
		err = -ENOMEM;
		goto err_pci_release_regions;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

	wx = netdev_priv(netdev);
	wx->netdev = netdev;
	wx->pdev = pdev;

	wx->msg_enable = (1 << DEFAULT_DEBUG_LEVEL_SHIFT) - 1;

	wx->hw_addr = devm_ioremap(&pdev->dev,
				   pci_resource_start(pdev, 0),
				   pci_resource_len(pdev, 0));
	if (!wx->hw_addr) {
		err = -EIO;
		goto err_pci_release_regions;
	}

	wx->driver_name = txgbe_driver_name;
	txgbe_set_ethtool_ops(netdev);
	netdev->netdev_ops = &txgbe_netdev_ops;

	 
	err = txgbe_sw_init(wx);
	if (err)
		goto err_free_mac_table;

	 
	err = wx_check_flash_load(wx, TXGBE_SPI_ILDR_STATUS_PERST);
	if (err)
		goto err_free_mac_table;
	err = wx_check_flash_load(wx, TXGBE_SPI_ILDR_STATUS_PWRRST);
	if (err)
		goto err_free_mac_table;

	err = wx_mng_present(wx);
	if (err) {
		dev_err(&pdev->dev, "Management capability is not present\n");
		goto err_free_mac_table;
	}

	err = txgbe_reset_hw(wx);
	if (err) {
		dev_err(&pdev->dev, "HW Init failed: %d\n", err);
		goto err_free_mac_table;
	}

	netdev->features = NETIF_F_SG |
			   NETIF_F_TSO |
			   NETIF_F_TSO6 |
			   NETIF_F_RXHASH |
			   NETIF_F_RXCSUM |
			   NETIF_F_HW_CSUM;

	netdev->gso_partial_features =  NETIF_F_GSO_ENCAP_ALL;
	netdev->features |= netdev->gso_partial_features;
	netdev->features |= NETIF_F_SCTP_CRC;
	netdev->vlan_features |= netdev->features | NETIF_F_TSO_MANGLEID;
	netdev->hw_enc_features |= netdev->vlan_features;
	netdev->features |= NETIF_F_VLAN_FEATURES;
	 
	netdev->hw_features |= netdev->features | NETIF_F_RXALL;
	netdev->hw_features |= NETIF_F_NTUPLE | NETIF_F_HW_TC;
	netdev->features |= NETIF_F_HIGHDMA;
	netdev->hw_features |= NETIF_F_GRO;
	netdev->features |= NETIF_F_GRO;

	netdev->priv_flags |= IFF_UNICAST_FLT;
	netdev->priv_flags |= IFF_SUPP_NOFCS;

	netdev->min_mtu = ETH_MIN_MTU;
	netdev->max_mtu = WX_MAX_JUMBO_FRAME_SIZE -
			  (ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN);

	 
	err = txgbe_validate_eeprom_checksum(wx, NULL);
	if (err != 0) {
		dev_err(&pdev->dev, "The EEPROM Checksum Is Not Valid\n");
		wr32(wx, WX_MIS_RST, WX_MIS_RST_SW_RST);
		err = -EIO;
		goto err_free_mac_table;
	}

	eth_hw_addr_set(netdev, wx->mac.perm_addr);
	wx_mac_set_default_filter(wx, wx->mac.perm_addr);

	err = wx_init_interrupt_scheme(wx);
	if (err)
		goto err_free_mac_table;

	 
	wx_read_ee_hostif(wx,
			  wx->eeprom.sw_region_offset + TXGBE_EEPROM_VERSION_H,
			  &eeprom_verh);
	wx_read_ee_hostif(wx,
			  wx->eeprom.sw_region_offset + TXGBE_EEPROM_VERSION_L,
			  &eeprom_verl);
	etrack_id = (eeprom_verh << 16) | eeprom_verl;

	wx_read_ee_hostif(wx,
			  wx->eeprom.sw_region_offset + TXGBE_ISCSI_BOOT_CONFIG,
			  &offset);

	 
	if (!(offset == 0x0) && !(offset == 0xffff)) {
		wx_read_ee_hostif(wx, offset + 0x84, &eeprom_cfg_blkh);
		wx_read_ee_hostif(wx, offset + 0x83, &eeprom_cfg_blkl);

		 
		if (eeprom_cfg_blkl && eeprom_cfg_blkh) {
			major = eeprom_cfg_blkl >> 8;
			build = (eeprom_cfg_blkl << 8) | (eeprom_cfg_blkh >> 8);
			patch = eeprom_cfg_blkh & 0x00ff;

			snprintf(wx->eeprom_id, sizeof(wx->eeprom_id),
				 "0x%08x, %d.%d.%d", etrack_id, major, build,
				 patch);
		} else {
			snprintf(wx->eeprom_id, sizeof(wx->eeprom_id),
				 "0x%08x", etrack_id);
		}
	} else {
		snprintf(wx->eeprom_id, sizeof(wx->eeprom_id),
			 "0x%08x", etrack_id);
	}

	if (etrack_id < 0x20010)
		dev_warn(&pdev->dev, "Please upgrade the firmware to 0x20010 or above.\n");

	txgbe = devm_kzalloc(&pdev->dev, sizeof(*txgbe), GFP_KERNEL);
	if (!txgbe) {
		err = -ENOMEM;
		goto err_release_hw;
	}

	txgbe->wx = wx;
	wx->priv = txgbe;

	err = txgbe_init_phy(txgbe);
	if (err)
		goto err_release_hw;

	err = register_netdev(netdev);
	if (err)
		goto err_remove_phy;

	pci_set_drvdata(pdev, wx);

	netif_tx_stop_all_queues(netdev);

	 
	expected_gts = txgbe_enumerate_functions(wx) * 10;

	 
	if (expected_gts > 0)
		txgbe_check_minimum_link(wx);
	else
		dev_warn(&pdev->dev, "Failed to enumerate PF devices.\n");

	 
	err = txgbe_read_pba_string(wx, part_str, TXGBE_PBANUM_LENGTH);
	if (err)
		strncpy(part_str, "Unknown", TXGBE_PBANUM_LENGTH);

	netif_info(wx, probe, netdev, "%pM\n", netdev->dev_addr);

	return 0;

err_remove_phy:
	txgbe_remove_phy(txgbe);
err_release_hw:
	wx_clear_interrupt_scheme(wx);
	wx_control_hw(wx, false);
err_free_mac_table:
	kfree(wx->mac_table);
err_pci_release_regions:
	pci_release_selected_regions(pdev,
				     pci_select_bars(pdev, IORESOURCE_MEM));
err_pci_disable_dev:
	pci_disable_device(pdev);
	return err;
}

 
static void txgbe_remove(struct pci_dev *pdev)
{
	struct wx *wx = pci_get_drvdata(pdev);
	struct txgbe *txgbe = wx->priv;
	struct net_device *netdev;

	netdev = wx->netdev;
	unregister_netdev(netdev);

	txgbe_remove_phy(txgbe);

	pci_release_selected_regions(pdev,
				     pci_select_bars(pdev, IORESOURCE_MEM));

	kfree(wx->mac_table);
	wx_clear_interrupt_scheme(wx);

	pci_disable_device(pdev);
}

static struct pci_driver txgbe_driver = {
	.name     = txgbe_driver_name,
	.id_table = txgbe_pci_tbl,
	.probe    = txgbe_probe,
	.remove   = txgbe_remove,
	.shutdown = txgbe_shutdown,
};

module_pci_driver(txgbe_driver);

MODULE_DEVICE_TABLE(pci, txgbe_pci_tbl);
MODULE_AUTHOR("Beijing WangXun Technology Co., Ltd, <software@trustnetic.com>");
MODULE_DESCRIPTION("WangXun(R) 10 Gigabit PCI Express Network Driver");
MODULE_LICENSE("GPL");
