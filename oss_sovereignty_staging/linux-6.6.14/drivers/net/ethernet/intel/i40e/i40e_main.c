
 

#include <linux/etherdevice.h>
#include <linux/of_net.h>
#include <linux/pci.h>
#include <linux/bpf.h>
#include <generated/utsrelease.h>
#include <linux/crash_dump.h>

 
#include "i40e.h"
#include "i40e_diag.h"
#include "i40e_xsk.h"
#include <net/udp_tunnel.h>
#include <net/xdp_sock_drv.h>
 
#define CREATE_TRACE_POINTS
#include "i40e_trace.h"

const char i40e_driver_name[] = "i40e";
static const char i40e_driver_string[] =
			"Intel(R) Ethernet Connection XL710 Network Driver";

static const char i40e_copyright[] = "Copyright (c) 2013 - 2019 Intel Corporation.";

 
static void i40e_vsi_reinit_locked(struct i40e_vsi *vsi);
static void i40e_handle_reset_warning(struct i40e_pf *pf, bool lock_acquired);
static int i40e_add_vsi(struct i40e_vsi *vsi);
static int i40e_add_veb(struct i40e_veb *veb, struct i40e_vsi *vsi);
static int i40e_setup_pf_switch(struct i40e_pf *pf, bool reinit, bool lock_acquired);
static int i40e_setup_misc_vector(struct i40e_pf *pf);
static void i40e_determine_queue_usage(struct i40e_pf *pf);
static int i40e_setup_pf_filter_control(struct i40e_pf *pf);
static void i40e_prep_for_reset(struct i40e_pf *pf);
static void i40e_reset_and_rebuild(struct i40e_pf *pf, bool reinit,
				   bool lock_acquired);
static int i40e_reset(struct i40e_pf *pf);
static void i40e_rebuild(struct i40e_pf *pf, bool reinit, bool lock_acquired);
static int i40e_setup_misc_vector_for_recovery_mode(struct i40e_pf *pf);
static int i40e_restore_interrupt_scheme(struct i40e_pf *pf);
static bool i40e_check_recovery_mode(struct i40e_pf *pf);
static int i40e_init_recovery_mode(struct i40e_pf *pf, struct i40e_hw *hw);
static void i40e_fdir_sb_setup(struct i40e_pf *pf);
static int i40e_veb_get_bw_info(struct i40e_veb *veb);
static int i40e_get_capabilities(struct i40e_pf *pf,
				 enum i40e_admin_queue_opc list_type);
static bool i40e_is_total_port_shutdown_enabled(struct i40e_pf *pf);

 
static const struct pci_device_id i40e_pci_tbl[] = {
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_SFP_XL710), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_QEMU), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_KX_B), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_KX_C), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_QSFP_A), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_QSFP_B), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_QSFP_C), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_1G_BASE_T_BC), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_10G_BASE_T), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_10G_BASE_T4), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_10G_BASE_T_BC), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_10G_SFP), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_10G_B), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_KX_X722), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_QSFP_X722), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_SFP_X722), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_1G_BASE_T_X722), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_10G_BASE_T_X722), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_SFP_I_X722), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_SFP_X722_A), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_20G_KR2), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_20G_KR2_A), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_X710_N3000), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_XXV710_N3000), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_25G_B), 0},
	{PCI_VDEVICE(INTEL, I40E_DEV_ID_25G_SFP28), 0},
	 
	{0, }
};
MODULE_DEVICE_TABLE(pci, i40e_pci_tbl);

#define I40E_MAX_VF_COUNT 128
static int debug = -1;
module_param(debug, uint, 0);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all), Debug mask (0x8XXXXXXX)");

MODULE_AUTHOR("Intel Corporation, <e1000-devel@lists.sourceforge.net>");
MODULE_DESCRIPTION("Intel(R) Ethernet Connection XL710 Network Driver");
MODULE_LICENSE("GPL v2");

static struct workqueue_struct *i40e_wq;

static void netdev_hw_addr_refcnt(struct i40e_mac_filter *f,
				  struct net_device *netdev, int delta)
{
	struct netdev_hw_addr_list *ha_list;
	struct netdev_hw_addr *ha;

	if (!f || !netdev)
		return;

	if (is_unicast_ether_addr(f->macaddr) || is_link_local_ether_addr(f->macaddr))
		ha_list = &netdev->uc;
	else
		ha_list = &netdev->mc;

	netdev_hw_addr_list_for_each(ha, ha_list) {
		if (ether_addr_equal(ha->addr, f->macaddr)) {
			ha->refcount += delta;
			if (ha->refcount <= 0)
				ha->refcount = 1;
			break;
		}
	}
}

 
int i40e_allocate_dma_mem_d(struct i40e_hw *hw, struct i40e_dma_mem *mem,
			    u64 size, u32 alignment)
{
	struct i40e_pf *pf = (struct i40e_pf *)hw->back;

	mem->size = ALIGN(size, alignment);
	mem->va = dma_alloc_coherent(&pf->pdev->dev, mem->size, &mem->pa,
				     GFP_KERNEL);
	if (!mem->va)
		return -ENOMEM;

	return 0;
}

 
int i40e_free_dma_mem_d(struct i40e_hw *hw, struct i40e_dma_mem *mem)
{
	struct i40e_pf *pf = (struct i40e_pf *)hw->back;

	dma_free_coherent(&pf->pdev->dev, mem->size, mem->va, mem->pa);
	mem->va = NULL;
	mem->pa = 0;
	mem->size = 0;

	return 0;
}

 
int i40e_allocate_virt_mem_d(struct i40e_hw *hw, struct i40e_virt_mem *mem,
			     u32 size)
{
	mem->size = size;
	mem->va = kzalloc(size, GFP_KERNEL);

	if (!mem->va)
		return -ENOMEM;

	return 0;
}

 
int i40e_free_virt_mem_d(struct i40e_hw *hw, struct i40e_virt_mem *mem)
{
	 
	kfree(mem->va);
	mem->va = NULL;
	mem->size = 0;

	return 0;
}

 
static int i40e_get_lump(struct i40e_pf *pf, struct i40e_lump_tracking *pile,
			 u16 needed, u16 id)
{
	int ret = -ENOMEM;
	int i, j;

	if (!pile || needed == 0 || id >= I40E_PILE_VALID_BIT) {
		dev_info(&pf->pdev->dev,
			 "param err: pile=%s needed=%d id=0x%04x\n",
			 pile ? "<valid>" : "<null>", needed, id);
		return -EINVAL;
	}

	 
	if (pile == pf->qp_pile && pf->vsi[id]->type == I40E_VSI_FDIR) {
		if (pile->list[pile->num_entries - 1] & I40E_PILE_VALID_BIT) {
			dev_err(&pf->pdev->dev,
				"Cannot allocate queue %d for I40E_VSI_FDIR\n",
				pile->num_entries - 1);
			return -ENOMEM;
		}
		pile->list[pile->num_entries - 1] = id | I40E_PILE_VALID_BIT;
		return pile->num_entries - 1;
	}

	i = 0;
	while (i < pile->num_entries) {
		 
		if (pile->list[i] & I40E_PILE_VALID_BIT) {
			i++;
			continue;
		}

		 
		for (j = 0; (j < needed) && ((i+j) < pile->num_entries); j++) {
			if (pile->list[i+j] & I40E_PILE_VALID_BIT)
				break;
		}

		if (j == needed) {
			 
			for (j = 0; j < needed; j++)
				pile->list[i+j] = id | I40E_PILE_VALID_BIT;
			ret = i;
			break;
		}

		 
		i += j;
	}

	return ret;
}

 
static int i40e_put_lump(struct i40e_lump_tracking *pile, u16 index, u16 id)
{
	int valid_id = (id | I40E_PILE_VALID_BIT);
	int count = 0;
	u16 i;

	if (!pile || index >= pile->num_entries)
		return -EINVAL;

	for (i = index;
	     i < pile->num_entries && pile->list[i] == valid_id;
	     i++) {
		pile->list[i] = 0;
		count++;
	}


	return count;
}

 
struct i40e_vsi *i40e_find_vsi_from_id(struct i40e_pf *pf, u16 id)
{
	int i;

	for (i = 0; i < pf->num_alloc_vsi; i++)
		if (pf->vsi[i] && (pf->vsi[i]->id == id))
			return pf->vsi[i];

	return NULL;
}

 
void i40e_service_event_schedule(struct i40e_pf *pf)
{
	if ((!test_bit(__I40E_DOWN, pf->state) &&
	     !test_bit(__I40E_RESET_RECOVERY_PENDING, pf->state)) ||
	      test_bit(__I40E_RECOVERY_MODE, pf->state))
		queue_work(i40e_wq, &pf->service_task);
}

 
static void i40e_tx_timeout(struct net_device *netdev, unsigned int txqueue)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_ring *tx_ring = NULL;
	unsigned int i;
	u32 head, val;

	pf->tx_timeout_count++;

	 
	for (i = 0; i < vsi->num_queue_pairs; i++) {
		if (vsi->tx_rings[i] && vsi->tx_rings[i]->desc) {
			if (txqueue ==
			    vsi->tx_rings[i]->queue_index) {
				tx_ring = vsi->tx_rings[i];
				break;
			}
		}
	}

	if (time_after(jiffies, (pf->tx_timeout_last_recovery + HZ*20)))
		pf->tx_timeout_recovery_level = 1;   
	else if (time_before(jiffies,
		      (pf->tx_timeout_last_recovery + netdev->watchdog_timeo)))
		return;    

	 
	if (test_and_set_bit(__I40E_TIMEOUT_RECOVERY_PENDING, pf->state))
		return;

	if (tx_ring) {
		head = i40e_get_head(tx_ring);
		 
		if (pf->flags & I40E_FLAG_MSIX_ENABLED)
			val = rd32(&pf->hw,
			     I40E_PFINT_DYN_CTLN(tx_ring->q_vector->v_idx +
						tx_ring->vsi->base_vector - 1));
		else
			val = rd32(&pf->hw, I40E_PFINT_DYN_CTL0);

		netdev_info(netdev, "tx_timeout: VSI_seid: %d, Q %d, NTC: 0x%x, HWB: 0x%x, NTU: 0x%x, TAIL: 0x%x, INT: 0x%x\n",
			    vsi->seid, txqueue, tx_ring->next_to_clean,
			    head, tx_ring->next_to_use,
			    readl(tx_ring->tail), val);
	}

	pf->tx_timeout_last_recovery = jiffies;
	netdev_info(netdev, "tx_timeout recovery level %d, txqueue %d\n",
		    pf->tx_timeout_recovery_level, txqueue);

	switch (pf->tx_timeout_recovery_level) {
	case 1:
		set_bit(__I40E_PF_RESET_REQUESTED, pf->state);
		break;
	case 2:
		set_bit(__I40E_CORE_RESET_REQUESTED, pf->state);
		break;
	case 3:
		set_bit(__I40E_GLOBAL_RESET_REQUESTED, pf->state);
		break;
	default:
		netdev_err(netdev, "tx_timeout recovery unsuccessful, device is in non-recoverable state.\n");
		set_bit(__I40E_DOWN_REQUESTED, pf->state);
		set_bit(__I40E_VSI_DOWN_REQUESTED, vsi->state);
		break;
	}

	i40e_service_event_schedule(pf);
	pf->tx_timeout_recovery_level++;
}

 
struct rtnl_link_stats64 *i40e_get_vsi_stats_struct(struct i40e_vsi *vsi)
{
	return &vsi->net_stats;
}

 
static void i40e_get_netdev_stats_struct_tx(struct i40e_ring *ring,
					    struct rtnl_link_stats64 *stats)
{
	u64 bytes, packets;
	unsigned int start;

	do {
		start = u64_stats_fetch_begin(&ring->syncp);
		packets = ring->stats.packets;
		bytes   = ring->stats.bytes;
	} while (u64_stats_fetch_retry(&ring->syncp, start));

	stats->tx_packets += packets;
	stats->tx_bytes   += bytes;
}

 
static void i40e_get_netdev_stats_struct(struct net_device *netdev,
				  struct rtnl_link_stats64 *stats)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct rtnl_link_stats64 *vsi_stats = i40e_get_vsi_stats_struct(vsi);
	struct i40e_ring *ring;
	int i;

	if (test_bit(__I40E_VSI_DOWN, vsi->state))
		return;

	if (!vsi->tx_rings)
		return;

	rcu_read_lock();
	for (i = 0; i < vsi->num_queue_pairs; i++) {
		u64 bytes, packets;
		unsigned int start;

		ring = READ_ONCE(vsi->tx_rings[i]);
		if (!ring)
			continue;
		i40e_get_netdev_stats_struct_tx(ring, stats);

		if (i40e_enabled_xdp_vsi(vsi)) {
			ring = READ_ONCE(vsi->xdp_rings[i]);
			if (!ring)
				continue;
			i40e_get_netdev_stats_struct_tx(ring, stats);
		}

		ring = READ_ONCE(vsi->rx_rings[i]);
		if (!ring)
			continue;
		do {
			start   = u64_stats_fetch_begin(&ring->syncp);
			packets = ring->stats.packets;
			bytes   = ring->stats.bytes;
		} while (u64_stats_fetch_retry(&ring->syncp, start));

		stats->rx_packets += packets;
		stats->rx_bytes   += bytes;

	}
	rcu_read_unlock();

	 
	stats->multicast	= vsi_stats->multicast;
	stats->tx_errors	= vsi_stats->tx_errors;
	stats->tx_dropped	= vsi_stats->tx_dropped;
	stats->rx_errors	= vsi_stats->rx_errors;
	stats->rx_dropped	= vsi_stats->rx_dropped;
	stats->rx_crc_errors	= vsi_stats->rx_crc_errors;
	stats->rx_length_errors	= vsi_stats->rx_length_errors;
}

 
void i40e_vsi_reset_stats(struct i40e_vsi *vsi)
{
	struct rtnl_link_stats64 *ns;
	int i;

	if (!vsi)
		return;

	ns = i40e_get_vsi_stats_struct(vsi);
	memset(ns, 0, sizeof(*ns));
	memset(&vsi->net_stats_offsets, 0, sizeof(vsi->net_stats_offsets));
	memset(&vsi->eth_stats, 0, sizeof(vsi->eth_stats));
	memset(&vsi->eth_stats_offsets, 0, sizeof(vsi->eth_stats_offsets));
	if (vsi->rx_rings && vsi->rx_rings[0]) {
		for (i = 0; i < vsi->num_queue_pairs; i++) {
			memset(&vsi->rx_rings[i]->stats, 0,
			       sizeof(vsi->rx_rings[i]->stats));
			memset(&vsi->rx_rings[i]->rx_stats, 0,
			       sizeof(vsi->rx_rings[i]->rx_stats));
			memset(&vsi->tx_rings[i]->stats, 0,
			       sizeof(vsi->tx_rings[i]->stats));
			memset(&vsi->tx_rings[i]->tx_stats, 0,
			       sizeof(vsi->tx_rings[i]->tx_stats));
		}
	}
	vsi->stat_offsets_loaded = false;
}

 
void i40e_pf_reset_stats(struct i40e_pf *pf)
{
	int i;

	memset(&pf->stats, 0, sizeof(pf->stats));
	memset(&pf->stats_offsets, 0, sizeof(pf->stats_offsets));
	pf->stat_offsets_loaded = false;

	for (i = 0; i < I40E_MAX_VEB; i++) {
		if (pf->veb[i]) {
			memset(&pf->veb[i]->stats, 0,
			       sizeof(pf->veb[i]->stats));
			memset(&pf->veb[i]->stats_offsets, 0,
			       sizeof(pf->veb[i]->stats_offsets));
			memset(&pf->veb[i]->tc_stats, 0,
			       sizeof(pf->veb[i]->tc_stats));
			memset(&pf->veb[i]->tc_stats_offsets, 0,
			       sizeof(pf->veb[i]->tc_stats_offsets));
			pf->veb[i]->stat_offsets_loaded = false;
		}
	}
	pf->hw_csum_rx_error = 0;
}

 
static u32 i40e_compute_pci_to_hw_id(struct i40e_vsi *vsi, struct i40e_hw *hw)
{
	int pf_count = i40e_get_pf_count(hw);

	if (vsi->type == I40E_VSI_SRIOV)
		return (hw->port * BIT(7)) / pf_count + vsi->vf_id;

	return hw->port + BIT(7);
}

 
static void i40e_stat_update64(struct i40e_hw *hw, u32 hireg, u32 loreg,
			       bool offset_loaded, u64 *offset, u64 *stat)
{
	u64 new_data;

	new_data = rd64(hw, loreg);

	if (!offset_loaded || new_data < *offset)
		*offset = new_data;
	*stat = new_data - *offset;
}

 
static void i40e_stat_update48(struct i40e_hw *hw, u32 hireg, u32 loreg,
			       bool offset_loaded, u64 *offset, u64 *stat)
{
	u64 new_data;

	if (hw->device_id == I40E_DEV_ID_QEMU) {
		new_data = rd32(hw, loreg);
		new_data |= ((u64)(rd32(hw, hireg) & 0xFFFF)) << 32;
	} else {
		new_data = rd64(hw, loreg);
	}
	if (!offset_loaded)
		*offset = new_data;
	if (likely(new_data >= *offset))
		*stat = new_data - *offset;
	else
		*stat = (new_data + BIT_ULL(48)) - *offset;
	*stat &= 0xFFFFFFFFFFFFULL;
}

 
static void i40e_stat_update32(struct i40e_hw *hw, u32 reg,
			       bool offset_loaded, u64 *offset, u64 *stat)
{
	u32 new_data;

	new_data = rd32(hw, reg);
	if (!offset_loaded)
		*offset = new_data;
	if (likely(new_data >= *offset))
		*stat = (u32)(new_data - *offset);
	else
		*stat = (u32)((new_data + BIT_ULL(32)) - *offset);
}

 
static void i40e_stat_update_and_clear32(struct i40e_hw *hw, u32 reg, u64 *stat)
{
	u32 new_data = rd32(hw, reg);

	wr32(hw, reg, 1);  
	*stat += new_data;
}

 
static void
i40e_stats_update_rx_discards(struct i40e_vsi *vsi, struct i40e_hw *hw,
			      int stat_idx, bool offset_loaded,
			      struct i40e_eth_stats *stat_offset,
			      struct i40e_eth_stats *stat)
{
	u64 rx_rdpc, rx_rxerr;

	i40e_stat_update32(hw, I40E_GLV_RDPC(stat_idx), offset_loaded,
			   &stat_offset->rx_discards, &rx_rdpc);
	i40e_stat_update64(hw,
			   I40E_GL_RXERR1H(i40e_compute_pci_to_hw_id(vsi, hw)),
			   I40E_GL_RXERR1L(i40e_compute_pci_to_hw_id(vsi, hw)),
			   offset_loaded, &stat_offset->rx_discards_other,
			   &rx_rxerr);

	stat->rx_discards = rx_rdpc + rx_rxerr;
}

 
void i40e_update_eth_stats(struct i40e_vsi *vsi)
{
	int stat_idx = le16_to_cpu(vsi->info.stat_counter_idx);
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_eth_stats *oes;
	struct i40e_eth_stats *es;      

	es = &vsi->eth_stats;
	oes = &vsi->eth_stats_offsets;

	 
	i40e_stat_update32(hw, I40E_GLV_TEPC(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_errors, &es->tx_errors);
	i40e_stat_update32(hw, I40E_GLV_RDPC(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_discards, &es->rx_discards);
	i40e_stat_update32(hw, I40E_GLV_RUPP(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_unknown_protocol, &es->rx_unknown_protocol);

	i40e_stat_update48(hw, I40E_GLV_GORCH(stat_idx),
			   I40E_GLV_GORCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_bytes, &es->rx_bytes);
	i40e_stat_update48(hw, I40E_GLV_UPRCH(stat_idx),
			   I40E_GLV_UPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_unicast, &es->rx_unicast);
	i40e_stat_update48(hw, I40E_GLV_MPRCH(stat_idx),
			   I40E_GLV_MPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_multicast, &es->rx_multicast);
	i40e_stat_update48(hw, I40E_GLV_BPRCH(stat_idx),
			   I40E_GLV_BPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_broadcast, &es->rx_broadcast);

	i40e_stat_update48(hw, I40E_GLV_GOTCH(stat_idx),
			   I40E_GLV_GOTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_bytes, &es->tx_bytes);
	i40e_stat_update48(hw, I40E_GLV_UPTCH(stat_idx),
			   I40E_GLV_UPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_unicast, &es->tx_unicast);
	i40e_stat_update48(hw, I40E_GLV_MPTCH(stat_idx),
			   I40E_GLV_MPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_multicast, &es->tx_multicast);
	i40e_stat_update48(hw, I40E_GLV_BPTCH(stat_idx),
			   I40E_GLV_BPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_broadcast, &es->tx_broadcast);

	i40e_stats_update_rx_discards(vsi, hw, stat_idx,
				      vsi->stat_offsets_loaded, oes, es);

	vsi->stat_offsets_loaded = true;
}

 
void i40e_update_veb_stats(struct i40e_veb *veb)
{
	struct i40e_pf *pf = veb->pf;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_eth_stats *oes;
	struct i40e_eth_stats *es;      
	struct i40e_veb_tc_stats *veb_oes;
	struct i40e_veb_tc_stats *veb_es;
	int i, idx = 0;

	idx = veb->stats_idx;
	es = &veb->stats;
	oes = &veb->stats_offsets;
	veb_es = &veb->tc_stats;
	veb_oes = &veb->tc_stats_offsets;

	 
	i40e_stat_update32(hw, I40E_GLSW_TDPC(idx),
			   veb->stat_offsets_loaded,
			   &oes->tx_discards, &es->tx_discards);
	if (hw->revision_id > 0)
		i40e_stat_update32(hw, I40E_GLSW_RUPP(idx),
				   veb->stat_offsets_loaded,
				   &oes->rx_unknown_protocol,
				   &es->rx_unknown_protocol);
	i40e_stat_update48(hw, I40E_GLSW_GORCH(idx), I40E_GLSW_GORCL(idx),
			   veb->stat_offsets_loaded,
			   &oes->rx_bytes, &es->rx_bytes);
	i40e_stat_update48(hw, I40E_GLSW_UPRCH(idx), I40E_GLSW_UPRCL(idx),
			   veb->stat_offsets_loaded,
			   &oes->rx_unicast, &es->rx_unicast);
	i40e_stat_update48(hw, I40E_GLSW_MPRCH(idx), I40E_GLSW_MPRCL(idx),
			   veb->stat_offsets_loaded,
			   &oes->rx_multicast, &es->rx_multicast);
	i40e_stat_update48(hw, I40E_GLSW_BPRCH(idx), I40E_GLSW_BPRCL(idx),
			   veb->stat_offsets_loaded,
			   &oes->rx_broadcast, &es->rx_broadcast);

	i40e_stat_update48(hw, I40E_GLSW_GOTCH(idx), I40E_GLSW_GOTCL(idx),
			   veb->stat_offsets_loaded,
			   &oes->tx_bytes, &es->tx_bytes);
	i40e_stat_update48(hw, I40E_GLSW_UPTCH(idx), I40E_GLSW_UPTCL(idx),
			   veb->stat_offsets_loaded,
			   &oes->tx_unicast, &es->tx_unicast);
	i40e_stat_update48(hw, I40E_GLSW_MPTCH(idx), I40E_GLSW_MPTCL(idx),
			   veb->stat_offsets_loaded,
			   &oes->tx_multicast, &es->tx_multicast);
	i40e_stat_update48(hw, I40E_GLSW_BPTCH(idx), I40E_GLSW_BPTCL(idx),
			   veb->stat_offsets_loaded,
			   &oes->tx_broadcast, &es->tx_broadcast);
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		i40e_stat_update48(hw, I40E_GLVEBTC_RPCH(i, idx),
				   I40E_GLVEBTC_RPCL(i, idx),
				   veb->stat_offsets_loaded,
				   &veb_oes->tc_rx_packets[i],
				   &veb_es->tc_rx_packets[i]);
		i40e_stat_update48(hw, I40E_GLVEBTC_RBCH(i, idx),
				   I40E_GLVEBTC_RBCL(i, idx),
				   veb->stat_offsets_loaded,
				   &veb_oes->tc_rx_bytes[i],
				   &veb_es->tc_rx_bytes[i]);
		i40e_stat_update48(hw, I40E_GLVEBTC_TPCH(i, idx),
				   I40E_GLVEBTC_TPCL(i, idx),
				   veb->stat_offsets_loaded,
				   &veb_oes->tc_tx_packets[i],
				   &veb_es->tc_tx_packets[i]);
		i40e_stat_update48(hw, I40E_GLVEBTC_TBCH(i, idx),
				   I40E_GLVEBTC_TBCL(i, idx),
				   veb->stat_offsets_loaded,
				   &veb_oes->tc_tx_bytes[i],
				   &veb_es->tc_tx_bytes[i]);
	}
	veb->stat_offsets_loaded = true;
}

 
static void i40e_update_vsi_stats(struct i40e_vsi *vsi)
{
	u64 rx_page, rx_buf, rx_reuse, rx_alloc, rx_waive, rx_busy;
	struct i40e_pf *pf = vsi->back;
	struct rtnl_link_stats64 *ons;
	struct rtnl_link_stats64 *ns;    
	struct i40e_eth_stats *oes;
	struct i40e_eth_stats *es;      
	u64 tx_restart, tx_busy;
	struct i40e_ring *p;
	u64 bytes, packets;
	unsigned int start;
	u64 tx_linearize;
	u64 tx_force_wb;
	u64 tx_stopped;
	u64 rx_p, rx_b;
	u64 tx_p, tx_b;
	u16 q;

	if (test_bit(__I40E_VSI_DOWN, vsi->state) ||
	    test_bit(__I40E_CONFIG_BUSY, pf->state))
		return;

	ns = i40e_get_vsi_stats_struct(vsi);
	ons = &vsi->net_stats_offsets;
	es = &vsi->eth_stats;
	oes = &vsi->eth_stats_offsets;

	 
	rx_b = rx_p = 0;
	tx_b = tx_p = 0;
	tx_restart = tx_busy = tx_linearize = tx_force_wb = 0;
	tx_stopped = 0;
	rx_page = 0;
	rx_buf = 0;
	rx_reuse = 0;
	rx_alloc = 0;
	rx_waive = 0;
	rx_busy = 0;
	rcu_read_lock();
	for (q = 0; q < vsi->num_queue_pairs; q++) {
		 
		p = READ_ONCE(vsi->tx_rings[q]);
		if (!p)
			continue;

		do {
			start = u64_stats_fetch_begin(&p->syncp);
			packets = p->stats.packets;
			bytes = p->stats.bytes;
		} while (u64_stats_fetch_retry(&p->syncp, start));
		tx_b += bytes;
		tx_p += packets;
		tx_restart += p->tx_stats.restart_queue;
		tx_busy += p->tx_stats.tx_busy;
		tx_linearize += p->tx_stats.tx_linearize;
		tx_force_wb += p->tx_stats.tx_force_wb;
		tx_stopped += p->tx_stats.tx_stopped;

		 
		p = READ_ONCE(vsi->rx_rings[q]);
		if (!p)
			continue;

		do {
			start = u64_stats_fetch_begin(&p->syncp);
			packets = p->stats.packets;
			bytes = p->stats.bytes;
		} while (u64_stats_fetch_retry(&p->syncp, start));
		rx_b += bytes;
		rx_p += packets;
		rx_buf += p->rx_stats.alloc_buff_failed;
		rx_page += p->rx_stats.alloc_page_failed;
		rx_reuse += p->rx_stats.page_reuse_count;
		rx_alloc += p->rx_stats.page_alloc_count;
		rx_waive += p->rx_stats.page_waive_count;
		rx_busy += p->rx_stats.page_busy_count;

		if (i40e_enabled_xdp_vsi(vsi)) {
			 
			p = READ_ONCE(vsi->xdp_rings[q]);
			if (!p)
				continue;

			do {
				start = u64_stats_fetch_begin(&p->syncp);
				packets = p->stats.packets;
				bytes = p->stats.bytes;
			} while (u64_stats_fetch_retry(&p->syncp, start));
			tx_b += bytes;
			tx_p += packets;
			tx_restart += p->tx_stats.restart_queue;
			tx_busy += p->tx_stats.tx_busy;
			tx_linearize += p->tx_stats.tx_linearize;
			tx_force_wb += p->tx_stats.tx_force_wb;
		}
	}
	rcu_read_unlock();
	vsi->tx_restart = tx_restart;
	vsi->tx_busy = tx_busy;
	vsi->tx_linearize = tx_linearize;
	vsi->tx_force_wb = tx_force_wb;
	vsi->tx_stopped = tx_stopped;
	vsi->rx_page_failed = rx_page;
	vsi->rx_buf_failed = rx_buf;
	vsi->rx_page_reuse = rx_reuse;
	vsi->rx_page_alloc = rx_alloc;
	vsi->rx_page_waive = rx_waive;
	vsi->rx_page_busy = rx_busy;

	ns->rx_packets = rx_p;
	ns->rx_bytes = rx_b;
	ns->tx_packets = tx_p;
	ns->tx_bytes = tx_b;

	 
	i40e_update_eth_stats(vsi);
	ons->tx_errors = oes->tx_errors;
	ns->tx_errors = es->tx_errors;
	ons->multicast = oes->rx_multicast;
	ns->multicast = es->rx_multicast;
	ons->rx_dropped = oes->rx_discards;
	ns->rx_dropped = es->rx_discards;
	ons->tx_dropped = oes->tx_discards;
	ns->tx_dropped = es->tx_discards;

	 
	if (vsi == pf->vsi[pf->lan_vsi]) {
		ns->rx_crc_errors = pf->stats.crc_errors;
		ns->rx_errors = pf->stats.crc_errors + pf->stats.illegal_bytes;
		ns->rx_length_errors = pf->stats.rx_length_errors;
	}
}

 
static void i40e_update_pf_stats(struct i40e_pf *pf)
{
	struct i40e_hw_port_stats *osd = &pf->stats_offsets;
	struct i40e_hw_port_stats *nsd = &pf->stats;
	struct i40e_hw *hw = &pf->hw;
	u32 val;
	int i;

	i40e_stat_update48(hw, I40E_GLPRT_GORCH(hw->port),
			   I40E_GLPRT_GORCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_bytes, &nsd->eth.rx_bytes);
	i40e_stat_update48(hw, I40E_GLPRT_GOTCH(hw->port),
			   I40E_GLPRT_GOTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_bytes, &nsd->eth.tx_bytes);
	i40e_stat_update32(hw, I40E_GLPRT_RDPC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_discards,
			   &nsd->eth.rx_discards);
	i40e_stat_update48(hw, I40E_GLPRT_UPRCH(hw->port),
			   I40E_GLPRT_UPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_unicast,
			   &nsd->eth.rx_unicast);
	i40e_stat_update48(hw, I40E_GLPRT_MPRCH(hw->port),
			   I40E_GLPRT_MPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_multicast,
			   &nsd->eth.rx_multicast);
	i40e_stat_update48(hw, I40E_GLPRT_BPRCH(hw->port),
			   I40E_GLPRT_BPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_broadcast,
			   &nsd->eth.rx_broadcast);
	i40e_stat_update48(hw, I40E_GLPRT_UPTCH(hw->port),
			   I40E_GLPRT_UPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_unicast,
			   &nsd->eth.tx_unicast);
	i40e_stat_update48(hw, I40E_GLPRT_MPTCH(hw->port),
			   I40E_GLPRT_MPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_multicast,
			   &nsd->eth.tx_multicast);
	i40e_stat_update48(hw, I40E_GLPRT_BPTCH(hw->port),
			   I40E_GLPRT_BPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_broadcast,
			   &nsd->eth.tx_broadcast);

	i40e_stat_update32(hw, I40E_GLPRT_TDOLD(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_dropped_link_down,
			   &nsd->tx_dropped_link_down);

	i40e_stat_update32(hw, I40E_GLPRT_CRCERRS(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->crc_errors, &nsd->crc_errors);

	i40e_stat_update32(hw, I40E_GLPRT_ILLERRC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->illegal_bytes, &nsd->illegal_bytes);

	i40e_stat_update32(hw, I40E_GLPRT_MLFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->mac_local_faults,
			   &nsd->mac_local_faults);
	i40e_stat_update32(hw, I40E_GLPRT_MRFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->mac_remote_faults,
			   &nsd->mac_remote_faults);

	i40e_stat_update32(hw, I40E_GLPRT_RLEC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_length_errors,
			   &nsd->rx_length_errors);

	i40e_stat_update32(hw, I40E_GLPRT_LXONRXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xon_rx, &nsd->link_xon_rx);
	i40e_stat_update32(hw, I40E_GLPRT_LXONTXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xon_tx, &nsd->link_xon_tx);
	i40e_stat_update32(hw, I40E_GLPRT_LXOFFRXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xoff_rx, &nsd->link_xoff_rx);
	i40e_stat_update32(hw, I40E_GLPRT_LXOFFTXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xoff_tx, &nsd->link_xoff_tx);

	for (i = 0; i < 8; i++) {
		i40e_stat_update32(hw, I40E_GLPRT_PXOFFRXC(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xoff_rx[i],
				   &nsd->priority_xoff_rx[i]);
		i40e_stat_update32(hw, I40E_GLPRT_PXONRXC(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xon_rx[i],
				   &nsd->priority_xon_rx[i]);
		i40e_stat_update32(hw, I40E_GLPRT_PXONTXC(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xon_tx[i],
				   &nsd->priority_xon_tx[i]);
		i40e_stat_update32(hw, I40E_GLPRT_PXOFFTXC(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xoff_tx[i],
				   &nsd->priority_xoff_tx[i]);
		i40e_stat_update32(hw,
				   I40E_GLPRT_RXON2OFFCNT(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xon_2_xoff[i],
				   &nsd->priority_xon_2_xoff[i]);
	}

	i40e_stat_update48(hw, I40E_GLPRT_PRC64H(hw->port),
			   I40E_GLPRT_PRC64L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_64, &nsd->rx_size_64);
	i40e_stat_update48(hw, I40E_GLPRT_PRC127H(hw->port),
			   I40E_GLPRT_PRC127L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_127, &nsd->rx_size_127);
	i40e_stat_update48(hw, I40E_GLPRT_PRC255H(hw->port),
			   I40E_GLPRT_PRC255L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_255, &nsd->rx_size_255);
	i40e_stat_update48(hw, I40E_GLPRT_PRC511H(hw->port),
			   I40E_GLPRT_PRC511L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_511, &nsd->rx_size_511);
	i40e_stat_update48(hw, I40E_GLPRT_PRC1023H(hw->port),
			   I40E_GLPRT_PRC1023L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_1023, &nsd->rx_size_1023);
	i40e_stat_update48(hw, I40E_GLPRT_PRC1522H(hw->port),
			   I40E_GLPRT_PRC1522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_1522, &nsd->rx_size_1522);
	i40e_stat_update48(hw, I40E_GLPRT_PRC9522H(hw->port),
			   I40E_GLPRT_PRC9522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_big, &nsd->rx_size_big);

	i40e_stat_update48(hw, I40E_GLPRT_PTC64H(hw->port),
			   I40E_GLPRT_PTC64L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_64, &nsd->tx_size_64);
	i40e_stat_update48(hw, I40E_GLPRT_PTC127H(hw->port),
			   I40E_GLPRT_PTC127L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_127, &nsd->tx_size_127);
	i40e_stat_update48(hw, I40E_GLPRT_PTC255H(hw->port),
			   I40E_GLPRT_PTC255L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_255, &nsd->tx_size_255);
	i40e_stat_update48(hw, I40E_GLPRT_PTC511H(hw->port),
			   I40E_GLPRT_PTC511L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_511, &nsd->tx_size_511);
	i40e_stat_update48(hw, I40E_GLPRT_PTC1023H(hw->port),
			   I40E_GLPRT_PTC1023L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_1023, &nsd->tx_size_1023);
	i40e_stat_update48(hw, I40E_GLPRT_PTC1522H(hw->port),
			   I40E_GLPRT_PTC1522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_1522, &nsd->tx_size_1522);
	i40e_stat_update48(hw, I40E_GLPRT_PTC9522H(hw->port),
			   I40E_GLPRT_PTC9522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_big, &nsd->tx_size_big);

	i40e_stat_update32(hw, I40E_GLPRT_RUC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_undersize, &nsd->rx_undersize);
	i40e_stat_update32(hw, I40E_GLPRT_RFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_fragments, &nsd->rx_fragments);
	i40e_stat_update32(hw, I40E_GLPRT_ROC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_oversize, &nsd->rx_oversize);
	i40e_stat_update32(hw, I40E_GLPRT_RJC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_jabber, &nsd->rx_jabber);

	 
	i40e_stat_update_and_clear32(hw,
			I40E_GLQF_PCNT(I40E_FD_ATR_STAT_IDX(hw->pf_id)),
			&nsd->fd_atr_match);
	i40e_stat_update_and_clear32(hw,
			I40E_GLQF_PCNT(I40E_FD_SB_STAT_IDX(hw->pf_id)),
			&nsd->fd_sb_match);
	i40e_stat_update_and_clear32(hw,
			I40E_GLQF_PCNT(I40E_FD_ATR_TUNNEL_STAT_IDX(hw->pf_id)),
			&nsd->fd_atr_tunnel_match);

	val = rd32(hw, I40E_PRTPM_EEE_STAT);
	nsd->tx_lpi_status =
		       (val & I40E_PRTPM_EEE_STAT_TX_LPI_STATUS_MASK) >>
			I40E_PRTPM_EEE_STAT_TX_LPI_STATUS_SHIFT;
	nsd->rx_lpi_status =
		       (val & I40E_PRTPM_EEE_STAT_RX_LPI_STATUS_MASK) >>
			I40E_PRTPM_EEE_STAT_RX_LPI_STATUS_SHIFT;
	i40e_stat_update32(hw, I40E_PRTPM_TLPIC,
			   pf->stat_offsets_loaded,
			   &osd->tx_lpi_count, &nsd->tx_lpi_count);
	i40e_stat_update32(hw, I40E_PRTPM_RLPIC,
			   pf->stat_offsets_loaded,
			   &osd->rx_lpi_count, &nsd->rx_lpi_count);

	if (pf->flags & I40E_FLAG_FD_SB_ENABLED &&
	    !test_bit(__I40E_FD_SB_AUTO_DISABLED, pf->state))
		nsd->fd_sb_status = true;
	else
		nsd->fd_sb_status = false;

	if (pf->flags & I40E_FLAG_FD_ATR_ENABLED &&
	    !test_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state))
		nsd->fd_atr_status = true;
	else
		nsd->fd_atr_status = false;

	pf->stat_offsets_loaded = true;
}

 
void i40e_update_stats(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;

	if (vsi == pf->vsi[pf->lan_vsi])
		i40e_update_pf_stats(pf);

	i40e_update_vsi_stats(vsi);
}

 
int i40e_count_filters(struct i40e_vsi *vsi)
{
	struct i40e_mac_filter *f;
	struct hlist_node *h;
	int bkt;
	int cnt = 0;

	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist)
		++cnt;

	return cnt;
}

 
static struct i40e_mac_filter *i40e_find_filter(struct i40e_vsi *vsi,
						const u8 *macaddr, s16 vlan)
{
	struct i40e_mac_filter *f;
	u64 key;

	if (!vsi || !macaddr)
		return NULL;

	key = i40e_addr_to_hkey(macaddr);
	hash_for_each_possible(vsi->mac_filter_hash, f, hlist, key) {
		if ((ether_addr_equal(macaddr, f->macaddr)) &&
		    (vlan == f->vlan))
			return f;
	}
	return NULL;
}

 
struct i40e_mac_filter *i40e_find_mac(struct i40e_vsi *vsi, const u8 *macaddr)
{
	struct i40e_mac_filter *f;
	u64 key;

	if (!vsi || !macaddr)
		return NULL;

	key = i40e_addr_to_hkey(macaddr);
	hash_for_each_possible(vsi->mac_filter_hash, f, hlist, key) {
		if ((ether_addr_equal(macaddr, f->macaddr)))
			return f;
	}
	return NULL;
}

 
bool i40e_is_vsi_in_vlan(struct i40e_vsi *vsi)
{
	 
	if (vsi->info.pvid)
		return true;

	 
	return vsi->has_vlan_filter;
}

 
static int i40e_correct_mac_vlan_filters(struct i40e_vsi *vsi,
					 struct hlist_head *tmp_add_list,
					 struct hlist_head *tmp_del_list,
					 int vlan_filters)
{
	s16 pvid = le16_to_cpu(vsi->info.pvid);
	struct i40e_mac_filter *f, *add_head;
	struct i40e_new_mac_filter *new;
	struct hlist_node *h;
	int bkt, new_vlan;

	 

	 
	hlist_for_each_entry(new, tmp_add_list, hlist) {
		if (pvid && new->f->vlan != pvid)
			new->f->vlan = pvid;
		else if (vlan_filters && new->f->vlan == I40E_VLAN_ANY)
			new->f->vlan = 0;
		else if (!vlan_filters && new->f->vlan == 0)
			new->f->vlan = I40E_VLAN_ANY;
	}

	 
	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
		 
		if ((pvid && f->vlan != pvid) ||
		    (vlan_filters && f->vlan == I40E_VLAN_ANY) ||
		    (!vlan_filters && f->vlan == 0)) {
			 
			if (pvid)
				new_vlan = pvid;
			else if (vlan_filters)
				new_vlan = 0;
			else
				new_vlan = I40E_VLAN_ANY;

			 
			add_head = i40e_add_filter(vsi, f->macaddr, new_vlan);
			if (!add_head)
				return -ENOMEM;

			 
			new = kzalloc(sizeof(*new), GFP_ATOMIC);
			if (!new)
				return -ENOMEM;

			new->f = add_head;
			new->state = add_head->state;

			 
			hlist_add_head(&new->hlist, tmp_add_list);

			 
			f->state = I40E_FILTER_REMOVE;
			hash_del(&f->hlist);
			hlist_add_head(&f->hlist, tmp_del_list);
		}
	}

	vsi->has_vlan_filter = !!vlan_filters;

	return 0;
}

 
static s16 i40e_get_vf_new_vlan(struct i40e_vsi *vsi,
				struct i40e_new_mac_filter *new_mac,
				struct i40e_mac_filter *f,
				int vlan_filters,
				bool trusted)
{
	s16 pvid = le16_to_cpu(vsi->info.pvid);
	struct i40e_pf *pf = vsi->back;
	bool is_any;

	if (new_mac)
		f = new_mac->f;

	if (pvid && f->vlan != pvid)
		return pvid;

	is_any = (trusted ||
		  !(pf->flags & I40E_FLAG_VF_VLAN_PRUNING));

	if ((vlan_filters && f->vlan == I40E_VLAN_ANY) ||
	    (!is_any && !vlan_filters && f->vlan == I40E_VLAN_ANY) ||
	    (is_any && !vlan_filters && f->vlan == 0)) {
		if (is_any)
			return I40E_VLAN_ANY;
		else
			return 0;
	}

	return f->vlan;
}

 
static int i40e_correct_vf_mac_vlan_filters(struct i40e_vsi *vsi,
					    struct hlist_head *tmp_add_list,
					    struct hlist_head *tmp_del_list,
					    int vlan_filters,
					    bool trusted)
{
	struct i40e_mac_filter *f, *add_head;
	struct i40e_new_mac_filter *new_mac;
	struct hlist_node *h;
	int bkt, new_vlan;

	hlist_for_each_entry(new_mac, tmp_add_list, hlist) {
		new_mac->f->vlan = i40e_get_vf_new_vlan(vsi, new_mac, NULL,
							vlan_filters, trusted);
	}

	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
		new_vlan = i40e_get_vf_new_vlan(vsi, NULL, f, vlan_filters,
						trusted);
		if (new_vlan != f->vlan) {
			add_head = i40e_add_filter(vsi, f->macaddr, new_vlan);
			if (!add_head)
				return -ENOMEM;
			 
			new_mac = kzalloc(sizeof(*new_mac), GFP_ATOMIC);
			if (!new_mac)
				return -ENOMEM;
			new_mac->f = add_head;
			new_mac->state = add_head->state;

			 
			hlist_add_head(&new_mac->hlist, tmp_add_list);

			 
			f->state = I40E_FILTER_REMOVE;
			hash_del(&f->hlist);
			hlist_add_head(&f->hlist, tmp_del_list);
		}
	}

	vsi->has_vlan_filter = !!vlan_filters;
	return 0;
}

 
static void i40e_rm_default_mac_filter(struct i40e_vsi *vsi, u8 *macaddr)
{
	struct i40e_aqc_remove_macvlan_element_data element;
	struct i40e_pf *pf = vsi->back;

	 
	if (vsi->type != I40E_VSI_MAIN)
		return;

	memset(&element, 0, sizeof(element));
	ether_addr_copy(element.mac_addr, macaddr);
	element.vlan_tag = 0;
	 
	element.flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
	i40e_aq_remove_macvlan(&pf->hw, vsi->seid, &element, 1, NULL);

	memset(&element, 0, sizeof(element));
	ether_addr_copy(element.mac_addr, macaddr);
	element.vlan_tag = 0;
	 
	element.flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH |
			I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
	i40e_aq_remove_macvlan(&pf->hw, vsi->seid, &element, 1, NULL);
}

 
struct i40e_mac_filter *i40e_add_filter(struct i40e_vsi *vsi,
					const u8 *macaddr, s16 vlan)
{
	struct i40e_mac_filter *f;
	u64 key;

	if (!vsi || !macaddr)
		return NULL;

	f = i40e_find_filter(vsi, macaddr, vlan);
	if (!f) {
		f = kzalloc(sizeof(*f), GFP_ATOMIC);
		if (!f)
			return NULL;

		 
		if (vlan >= 0)
			vsi->has_vlan_filter = true;

		ether_addr_copy(f->macaddr, macaddr);
		f->vlan = vlan;
		f->state = I40E_FILTER_NEW;
		INIT_HLIST_NODE(&f->hlist);

		key = i40e_addr_to_hkey(macaddr);
		hash_add(vsi->mac_filter_hash, &f->hlist, key);

		vsi->flags |= I40E_VSI_FLAG_FILTER_CHANGED;
		set_bit(__I40E_MACVLAN_SYNC_PENDING, vsi->back->state);
	}

	 
	if (f->state == I40E_FILTER_REMOVE)
		f->state = I40E_FILTER_ACTIVE;

	return f;
}

 
void __i40e_del_filter(struct i40e_vsi *vsi, struct i40e_mac_filter *f)
{
	if (!f)
		return;

	 
	if ((f->state == I40E_FILTER_FAILED) ||
	    (f->state == I40E_FILTER_NEW)) {
		hash_del(&f->hlist);
		kfree(f);
	} else {
		f->state = I40E_FILTER_REMOVE;
	}

	vsi->flags |= I40E_VSI_FLAG_FILTER_CHANGED;
	set_bit(__I40E_MACVLAN_SYNC_PENDING, vsi->back->state);
}

 
void i40e_del_filter(struct i40e_vsi *vsi, const u8 *macaddr, s16 vlan)
{
	struct i40e_mac_filter *f;

	if (!vsi || !macaddr)
		return;

	f = i40e_find_filter(vsi, macaddr, vlan);
	__i40e_del_filter(vsi, f);
}

 
struct i40e_mac_filter *i40e_add_mac_filter(struct i40e_vsi *vsi,
					    const u8 *macaddr)
{
	struct i40e_mac_filter *f, *add = NULL;
	struct hlist_node *h;
	int bkt;

	if (vsi->info.pvid)
		return i40e_add_filter(vsi, macaddr,
				       le16_to_cpu(vsi->info.pvid));

	if (!i40e_is_vsi_in_vlan(vsi))
		return i40e_add_filter(vsi, macaddr, I40E_VLAN_ANY);

	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
		if (f->state == I40E_FILTER_REMOVE)
			continue;
		add = i40e_add_filter(vsi, macaddr, f->vlan);
		if (!add)
			return NULL;
	}

	return add;
}

 
int i40e_del_mac_filter(struct i40e_vsi *vsi, const u8 *macaddr)
{
	struct i40e_mac_filter *f;
	struct hlist_node *h;
	bool found = false;
	int bkt;

	lockdep_assert_held(&vsi->mac_filter_hash_lock);
	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
		if (ether_addr_equal(macaddr, f->macaddr)) {
			__i40e_del_filter(vsi, f);
			found = true;
		}
	}

	if (found)
		return 0;
	else
		return -ENOENT;
}

 
static int i40e_set_mac(struct net_device *netdev, void *p)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	if (test_bit(__I40E_DOWN, pf->state) ||
	    test_bit(__I40E_RESET_RECOVERY_PENDING, pf->state))
		return -EADDRNOTAVAIL;

	if (ether_addr_equal(hw->mac.addr, addr->sa_data))
		netdev_info(netdev, "returning to hw mac address %pM\n",
			    hw->mac.addr);
	else
		netdev_info(netdev, "set new mac address %pM\n", addr->sa_data);

	 
	spin_lock_bh(&vsi->mac_filter_hash_lock);
	i40e_del_mac_filter(vsi, netdev->dev_addr);
	eth_hw_addr_set(netdev, addr->sa_data);
	i40e_add_mac_filter(vsi, netdev->dev_addr);
	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	if (vsi->type == I40E_VSI_MAIN) {
		int ret;

		ret = i40e_aq_mac_address_write(hw, I40E_AQC_WRITE_TYPE_LAA_WOL,
						addr->sa_data, NULL);
		if (ret)
			netdev_info(netdev, "Ignoring error from firmware on LAA update, status %pe, AQ ret %s\n",
				    ERR_PTR(ret),
				    i40e_aq_str(hw, hw->aq.asq_last_status));
	}

	 
	i40e_service_event_schedule(pf);
	return 0;
}

 
static int i40e_config_rss_aq(struct i40e_vsi *vsi, const u8 *seed,
			      u8 *lut, u16 lut_size)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	int ret = 0;

	if (seed) {
		struct i40e_aqc_get_set_rss_key_data *seed_dw =
			(struct i40e_aqc_get_set_rss_key_data *)seed;
		ret = i40e_aq_set_rss_key(hw, vsi->id, seed_dw);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "Cannot set RSS key, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(hw, hw->aq.asq_last_status));
			return ret;
		}
	}
	if (lut) {
		bool pf_lut = vsi->type == I40E_VSI_MAIN;

		ret = i40e_aq_set_rss_lut(hw, vsi->id, pf_lut, lut, lut_size);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "Cannot set RSS lut, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(hw, hw->aq.asq_last_status));
			return ret;
		}
	}
	return ret;
}

 
static int i40e_vsi_config_rss(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	u8 seed[I40E_HKEY_ARRAY_SIZE];
	u8 *lut;
	int ret;

	if (!(pf->hw_features & I40E_HW_RSS_AQ_CAPABLE))
		return 0;
	if (!vsi->rss_size)
		vsi->rss_size = min_t(int, pf->alloc_rss_size,
				      vsi->num_queue_pairs);
	if (!vsi->rss_size)
		return -EINVAL;
	lut = kzalloc(vsi->rss_table_size, GFP_KERNEL);
	if (!lut)
		return -ENOMEM;

	 
	if (vsi->rss_lut_user)
		memcpy(lut, vsi->rss_lut_user, vsi->rss_table_size);
	else
		i40e_fill_rss_lut(pf, lut, vsi->rss_table_size, vsi->rss_size);
	if (vsi->rss_hkey_user)
		memcpy(seed, vsi->rss_hkey_user, I40E_HKEY_ARRAY_SIZE);
	else
		netdev_rss_key_fill((void *)seed, I40E_HKEY_ARRAY_SIZE);
	ret = i40e_config_rss_aq(vsi, seed, lut, vsi->rss_table_size);
	kfree(lut);
	return ret;
}

 
static int i40e_vsi_setup_queue_map_mqprio(struct i40e_vsi *vsi,
					   struct i40e_vsi_context *ctxt,
					   u8 enabled_tc)
{
	u16 qcount = 0, max_qcount, qmap, sections = 0;
	int i, override_q, pow, num_qps, ret;
	u8 netdev_tc = 0, offset = 0;

	if (vsi->type != I40E_VSI_MAIN)
		return -EINVAL;
	sections = I40E_AQ_VSI_PROP_QUEUE_MAP_VALID;
	sections |= I40E_AQ_VSI_PROP_SCHED_VALID;
	vsi->tc_config.numtc = vsi->mqprio_qopt.qopt.num_tc;
	vsi->tc_config.enabled_tc = enabled_tc ? enabled_tc : 1;
	num_qps = vsi->mqprio_qopt.qopt.count[0];

	 
	pow = ilog2(num_qps);
	if (!is_power_of_2(num_qps))
		pow++;
	qmap = (offset << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
		(pow << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT);

	 
	max_qcount = vsi->mqprio_qopt.qopt.count[0];
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		 
		if (vsi->tc_config.enabled_tc & BIT(i)) {
			offset = vsi->mqprio_qopt.qopt.offset[i];
			qcount = vsi->mqprio_qopt.qopt.count[i];
			if (qcount > max_qcount)
				max_qcount = qcount;
			vsi->tc_config.tc_info[i].qoffset = offset;
			vsi->tc_config.tc_info[i].qcount = qcount;
			vsi->tc_config.tc_info[i].netdev_tc = netdev_tc++;
		} else {
			 
			vsi->tc_config.tc_info[i].qoffset = 0;
			vsi->tc_config.tc_info[i].qcount = 1;
			vsi->tc_config.tc_info[i].netdev_tc = 0;
		}
	}

	 
	vsi->num_queue_pairs = offset + qcount;

	 
	ctxt->info.tc_mapping[0] = cpu_to_le16(qmap);
	ctxt->info.mapping_flags |= cpu_to_le16(I40E_AQ_VSI_QUE_MAP_CONTIG);
	ctxt->info.queue_mapping[0] = cpu_to_le16(vsi->base_queue);
	ctxt->info.valid_sections |= cpu_to_le16(sections);

	 
	vsi->rss_size = max_qcount;
	ret = i40e_vsi_config_rss(vsi);
	if (ret) {
		dev_info(&vsi->back->pdev->dev,
			 "Failed to reconfig rss for num_queues (%u)\n",
			 max_qcount);
		return ret;
	}
	vsi->reconfig_rss = true;
	dev_dbg(&vsi->back->pdev->dev,
		"Reconfigured rss with num_queues (%u)\n", max_qcount);

	 
	override_q = vsi->mqprio_qopt.qopt.count[0];
	if (override_q && override_q < vsi->num_queue_pairs) {
		vsi->cnt_q_avail = vsi->num_queue_pairs - override_q;
		vsi->next_base_queue = override_q;
	}
	return 0;
}

 
static void i40e_vsi_setup_queue_map(struct i40e_vsi *vsi,
				     struct i40e_vsi_context *ctxt,
				     u8 enabled_tc,
				     bool is_add)
{
	struct i40e_pf *pf = vsi->back;
	u16 num_tc_qps = 0;
	u16 sections = 0;
	u8 netdev_tc = 0;
	u16 numtc = 1;
	u16 qcount;
	u8 offset;
	u16 qmap;
	int i;

	sections = I40E_AQ_VSI_PROP_QUEUE_MAP_VALID;
	offset = 0;
	 
	memset(ctxt->info.queue_mapping, 0, sizeof(ctxt->info.queue_mapping));

	if (vsi->type == I40E_VSI_MAIN) {
		 
		if (vsi->req_queue_pairs > 0)
			vsi->num_queue_pairs = vsi->req_queue_pairs;
		else if (pf->flags & I40E_FLAG_MSIX_ENABLED)
			vsi->num_queue_pairs = pf->num_lan_msix;
		else
			vsi->num_queue_pairs = 1;
	}

	 
	if (vsi->type == I40E_VSI_MAIN ||
	    (vsi->type == I40E_VSI_SRIOV && vsi->num_queue_pairs != 0))
		num_tc_qps = vsi->num_queue_pairs;
	else
		num_tc_qps = vsi->alloc_queue_pairs;

	if (enabled_tc && (vsi->back->flags & I40E_FLAG_DCB_ENABLED)) {
		 
		for (i = 0, numtc = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
			if (enabled_tc & BIT(i))  
				numtc++;
		}
		if (!numtc) {
			dev_warn(&pf->pdev->dev, "DCB is enabled but no TC enabled, forcing TC0\n");
			numtc = 1;
		}
		num_tc_qps = num_tc_qps / numtc;
		num_tc_qps = min_t(int, num_tc_qps,
				   i40e_pf_get_max_q_per_tc(pf));
	}

	vsi->tc_config.numtc = numtc;
	vsi->tc_config.enabled_tc = enabled_tc ? enabled_tc : 1;

	 
	if (pf->flags & I40E_FLAG_MSIX_ENABLED)
		num_tc_qps = min_t(int, num_tc_qps, pf->num_lan_msix);

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		 
		if (vsi->tc_config.enabled_tc & BIT(i)) {
			 
			int pow, num_qps;

			switch (vsi->type) {
			case I40E_VSI_MAIN:
				if (!(pf->flags & (I40E_FLAG_FD_SB_ENABLED |
				    I40E_FLAG_FD_ATR_ENABLED)) ||
				    vsi->tc_config.enabled_tc != 1) {
					qcount = min_t(int, pf->alloc_rss_size,
						       num_tc_qps);
					break;
				}
				fallthrough;
			case I40E_VSI_FDIR:
			case I40E_VSI_SRIOV:
			case I40E_VSI_VMDQ2:
			default:
				qcount = num_tc_qps;
				WARN_ON(i != 0);
				break;
			}
			vsi->tc_config.tc_info[i].qoffset = offset;
			vsi->tc_config.tc_info[i].qcount = qcount;

			 
			num_qps = qcount;
			pow = 0;
			while (num_qps && (BIT_ULL(pow) < qcount)) {
				pow++;
				num_qps >>= 1;
			}

			vsi->tc_config.tc_info[i].netdev_tc = netdev_tc++;
			qmap =
			    (offset << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
			    (pow << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT);

			offset += qcount;
		} else {
			 
			vsi->tc_config.tc_info[i].qoffset = 0;
			vsi->tc_config.tc_info[i].qcount = 1;
			vsi->tc_config.tc_info[i].netdev_tc = 0;

			qmap = 0;
		}
		ctxt->info.tc_mapping[i] = cpu_to_le16(qmap);
	}
	 
	if ((vsi->type == I40E_VSI_MAIN && numtc != 1) ||
	    (vsi->type == I40E_VSI_SRIOV && vsi->num_queue_pairs == 0) ||
	    (vsi->type != I40E_VSI_MAIN && vsi->type != I40E_VSI_SRIOV))
		vsi->num_queue_pairs = offset;

	 
	if (is_add) {
		sections |= I40E_AQ_VSI_PROP_SCHED_VALID;

		ctxt->info.up_enable_bits = enabled_tc;
	}
	if (vsi->type == I40E_VSI_SRIOV) {
		ctxt->info.mapping_flags |=
				     cpu_to_le16(I40E_AQ_VSI_QUE_MAP_NONCONTIG);
		for (i = 0; i < vsi->num_queue_pairs; i++)
			ctxt->info.queue_mapping[i] =
					       cpu_to_le16(vsi->base_queue + i);
	} else {
		ctxt->info.mapping_flags |=
					cpu_to_le16(I40E_AQ_VSI_QUE_MAP_CONTIG);
		ctxt->info.queue_mapping[0] = cpu_to_le16(vsi->base_queue);
	}
	ctxt->info.valid_sections |= cpu_to_le16(sections);
}

 
static int i40e_addr_sync(struct net_device *netdev, const u8 *addr)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;

	if (i40e_add_mac_filter(vsi, addr))
		return 0;
	else
		return -ENOMEM;
}

 
static int i40e_addr_unsync(struct net_device *netdev, const u8 *addr)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;

	 
	if (ether_addr_equal(addr, netdev->dev_addr))
		return 0;

	i40e_del_mac_filter(vsi, addr);

	return 0;
}

 
static void i40e_set_rx_mode(struct net_device *netdev)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;

	spin_lock_bh(&vsi->mac_filter_hash_lock);

	__dev_uc_sync(netdev, i40e_addr_sync, i40e_addr_unsync);
	__dev_mc_sync(netdev, i40e_addr_sync, i40e_addr_unsync);

	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	 
	if (vsi->current_netdev_flags != vsi->netdev->flags) {
		vsi->flags |= I40E_VSI_FLAG_FILTER_CHANGED;
		set_bit(__I40E_MACVLAN_SYNC_PENDING, vsi->back->state);
	}
}

 
static void i40e_undo_del_filter_entries(struct i40e_vsi *vsi,
					 struct hlist_head *from)
{
	struct i40e_mac_filter *f;
	struct hlist_node *h;

	hlist_for_each_entry_safe(f, h, from, hlist) {
		u64 key = i40e_addr_to_hkey(f->macaddr);

		 
		hlist_del(&f->hlist);
		hash_add(vsi->mac_filter_hash, &f->hlist, key);
	}
}

 
static void i40e_undo_add_filter_entries(struct i40e_vsi *vsi,
					 struct hlist_head *from)
{
	struct i40e_new_mac_filter *new;
	struct hlist_node *h;

	hlist_for_each_entry_safe(new, h, from, hlist) {
		 
		hlist_del(&new->hlist);
		netdev_hw_addr_refcnt(new->f, vsi->netdev, -1);
		kfree(new);
	}
}

 
static
struct i40e_new_mac_filter *i40e_next_filter(struct i40e_new_mac_filter *next)
{
	hlist_for_each_entry_continue(next, hlist) {
		if (!is_broadcast_ether_addr(next->f->macaddr))
			return next;
	}

	return NULL;
}

 
static int
i40e_update_filter_state(int count,
			 struct i40e_aqc_add_macvlan_element_data *add_list,
			 struct i40e_new_mac_filter *add_head)
{
	int retval = 0;
	int i;

	for (i = 0; i < count; i++) {
		 
		if (add_list[i].match_method == I40E_AQC_MM_ERR_NO_RES) {
			add_head->state = I40E_FILTER_FAILED;
		} else {
			add_head->state = I40E_FILTER_ACTIVE;
			retval++;
		}

		add_head = i40e_next_filter(add_head);
		if (!add_head)
			break;
	}

	return retval;
}

 
static
void i40e_aqc_del_filters(struct i40e_vsi *vsi, const char *vsi_name,
			  struct i40e_aqc_remove_macvlan_element_data *list,
			  int num_del, int *retval)
{
	struct i40e_hw *hw = &vsi->back->hw;
	enum i40e_admin_queue_err aq_status;
	int aq_ret;

	aq_ret = i40e_aq_remove_macvlan_v2(hw, vsi->seid, list, num_del, NULL,
					   &aq_status);

	 
	if (aq_ret && !(aq_status == I40E_AQ_RC_ENOENT)) {
		*retval = -EIO;
		dev_info(&vsi->back->pdev->dev,
			 "ignoring delete macvlan error on %s, err %pe, aq_err %s\n",
			 vsi_name, ERR_PTR(aq_ret),
			 i40e_aq_str(hw, aq_status));
	}
}

 
static
void i40e_aqc_add_filters(struct i40e_vsi *vsi, const char *vsi_name,
			  struct i40e_aqc_add_macvlan_element_data *list,
			  struct i40e_new_mac_filter *add_head,
			  int num_add)
{
	struct i40e_hw *hw = &vsi->back->hw;
	enum i40e_admin_queue_err aq_status;
	int fcnt;

	i40e_aq_add_macvlan_v2(hw, vsi->seid, list, num_add, NULL, &aq_status);
	fcnt = i40e_update_filter_state(num_add, list, add_head);

	if (fcnt != num_add) {
		if (vsi->type == I40E_VSI_MAIN) {
			set_bit(__I40E_VSI_OVERFLOW_PROMISC, vsi->state);
			dev_warn(&vsi->back->pdev->dev,
				 "Error %s adding RX filters on %s, promiscuous mode forced on\n",
				 i40e_aq_str(hw, aq_status), vsi_name);
		} else if (vsi->type == I40E_VSI_SRIOV ||
			   vsi->type == I40E_VSI_VMDQ1 ||
			   vsi->type == I40E_VSI_VMDQ2) {
			dev_warn(&vsi->back->pdev->dev,
				 "Error %s adding RX filters on %s, please set promiscuous on manually for %s\n",
				 i40e_aq_str(hw, aq_status), vsi_name,
					     vsi_name);
		} else {
			dev_warn(&vsi->back->pdev->dev,
				 "Error %s adding RX filters on %s, incorrect VSI type: %i.\n",
				 i40e_aq_str(hw, aq_status), vsi_name,
					     vsi->type);
		}
	}
}

 
static int
i40e_aqc_broadcast_filter(struct i40e_vsi *vsi, const char *vsi_name,
			  struct i40e_mac_filter *f)
{
	bool enable = f->state == I40E_FILTER_NEW;
	struct i40e_hw *hw = &vsi->back->hw;
	int aq_ret;

	if (f->vlan == I40E_VLAN_ANY) {
		aq_ret = i40e_aq_set_vsi_broadcast(hw,
						   vsi->seid,
						   enable,
						   NULL);
	} else {
		aq_ret = i40e_aq_set_vsi_bc_promisc_on_vlan(hw,
							    vsi->seid,
							    enable,
							    f->vlan,
							    NULL);
	}

	if (aq_ret) {
		set_bit(__I40E_VSI_OVERFLOW_PROMISC, vsi->state);
		dev_warn(&vsi->back->pdev->dev,
			 "Error %s, forcing overflow promiscuous on %s\n",
			 i40e_aq_str(hw, hw->aq.asq_last_status),
			 vsi_name);
	}

	return aq_ret;
}

 
static int i40e_set_promiscuous(struct i40e_pf *pf, bool promisc)
{
	struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
	struct i40e_hw *hw = &pf->hw;
	int aq_ret;

	if (vsi->type == I40E_VSI_MAIN &&
	    pf->lan_veb != I40E_NO_VEB &&
	    !(pf->flags & I40E_FLAG_MFP_ENABLED)) {
		 
		if (promisc)
			aq_ret = i40e_aq_set_default_vsi(hw,
							 vsi->seid,
							 NULL);
		else
			aq_ret = i40e_aq_clear_default_vsi(hw,
							   vsi->seid,
							   NULL);
		if (aq_ret) {
			dev_info(&pf->pdev->dev,
				 "Set default VSI failed, err %pe, aq_err %s\n",
				 ERR_PTR(aq_ret),
				 i40e_aq_str(hw, hw->aq.asq_last_status));
		}
	} else {
		aq_ret = i40e_aq_set_vsi_unicast_promiscuous(
						  hw,
						  vsi->seid,
						  promisc, NULL,
						  true);
		if (aq_ret) {
			dev_info(&pf->pdev->dev,
				 "set unicast promisc failed, err %pe, aq_err %s\n",
				 ERR_PTR(aq_ret),
				 i40e_aq_str(hw, hw->aq.asq_last_status));
		}
		aq_ret = i40e_aq_set_vsi_multicast_promiscuous(
						  hw,
						  vsi->seid,
						  promisc, NULL);
		if (aq_ret) {
			dev_info(&pf->pdev->dev,
				 "set multicast promisc failed, err %pe, aq_err %s\n",
				 ERR_PTR(aq_ret),
				 i40e_aq_str(hw, hw->aq.asq_last_status));
		}
	}

	if (!aq_ret)
		pf->cur_promisc = promisc;

	return aq_ret;
}

 
int i40e_sync_vsi_filters(struct i40e_vsi *vsi)
{
	struct hlist_head tmp_add_list, tmp_del_list;
	struct i40e_mac_filter *f;
	struct i40e_new_mac_filter *new, *add_head = NULL;
	struct i40e_hw *hw = &vsi->back->hw;
	bool old_overflow, new_overflow;
	unsigned int failed_filters = 0;
	unsigned int vlan_filters = 0;
	char vsi_name[16] = "PF";
	int filter_list_len = 0;
	u32 changed_flags = 0;
	struct hlist_node *h;
	struct i40e_pf *pf;
	int num_add = 0;
	int num_del = 0;
	int aq_ret = 0;
	int retval = 0;
	u16 cmd_flags;
	int list_size;
	int bkt;

	 
	struct i40e_aqc_add_macvlan_element_data *add_list;
	struct i40e_aqc_remove_macvlan_element_data *del_list;

	while (test_and_set_bit(__I40E_VSI_SYNCING_FILTERS, vsi->state))
		usleep_range(1000, 2000);
	pf = vsi->back;

	old_overflow = test_bit(__I40E_VSI_OVERFLOW_PROMISC, vsi->state);

	if (vsi->netdev) {
		changed_flags = vsi->current_netdev_flags ^ vsi->netdev->flags;
		vsi->current_netdev_flags = vsi->netdev->flags;
	}

	INIT_HLIST_HEAD(&tmp_add_list);
	INIT_HLIST_HEAD(&tmp_del_list);

	if (vsi->type == I40E_VSI_SRIOV)
		snprintf(vsi_name, sizeof(vsi_name) - 1, "VF %d", vsi->vf_id);
	else if (vsi->type != I40E_VSI_MAIN)
		snprintf(vsi_name, sizeof(vsi_name) - 1, "vsi %d", vsi->seid);

	if (vsi->flags & I40E_VSI_FLAG_FILTER_CHANGED) {
		vsi->flags &= ~I40E_VSI_FLAG_FILTER_CHANGED;

		spin_lock_bh(&vsi->mac_filter_hash_lock);
		 
		hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
			if (f->state == I40E_FILTER_REMOVE) {
				 
				hash_del(&f->hlist);
				hlist_add_head(&f->hlist, &tmp_del_list);

				 
				continue;
			}
			if (f->state == I40E_FILTER_NEW) {
				 
				new = kzalloc(sizeof(*new), GFP_ATOMIC);
				if (!new)
					goto err_no_memory_locked;

				 
				new->f = f;
				new->state = f->state;

				 
				hlist_add_head(&new->hlist, &tmp_add_list);
			}

			 
			if (f->vlan > 0)
				vlan_filters++;
		}

		if (vsi->type != I40E_VSI_SRIOV)
			retval = i40e_correct_mac_vlan_filters
				(vsi, &tmp_add_list, &tmp_del_list,
				 vlan_filters);
		else if (pf->vf)
			retval = i40e_correct_vf_mac_vlan_filters
				(vsi, &tmp_add_list, &tmp_del_list,
				 vlan_filters, pf->vf[vsi->vf_id].trusted);

		hlist_for_each_entry(new, &tmp_add_list, hlist)
			netdev_hw_addr_refcnt(new->f, vsi->netdev, 1);

		if (retval)
			goto err_no_memory_locked;

		spin_unlock_bh(&vsi->mac_filter_hash_lock);
	}

	 
	if (!hlist_empty(&tmp_del_list)) {
		filter_list_len = hw->aq.asq_buf_size /
			    sizeof(struct i40e_aqc_remove_macvlan_element_data);
		list_size = filter_list_len *
			    sizeof(struct i40e_aqc_remove_macvlan_element_data);
		del_list = kzalloc(list_size, GFP_ATOMIC);
		if (!del_list)
			goto err_no_memory;

		hlist_for_each_entry_safe(f, h, &tmp_del_list, hlist) {
			cmd_flags = 0;

			 
			if (is_broadcast_ether_addr(f->macaddr)) {
				i40e_aqc_broadcast_filter(vsi, vsi_name, f);

				hlist_del(&f->hlist);
				kfree(f);
				continue;
			}

			 
			ether_addr_copy(del_list[num_del].mac_addr, f->macaddr);
			if (f->vlan == I40E_VLAN_ANY) {
				del_list[num_del].vlan_tag = 0;
				cmd_flags |= I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
			} else {
				del_list[num_del].vlan_tag =
					cpu_to_le16((u16)(f->vlan));
			}

			cmd_flags |= I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
			del_list[num_del].flags = cmd_flags;
			num_del++;

			 
			if (num_del == filter_list_len) {
				i40e_aqc_del_filters(vsi, vsi_name, del_list,
						     num_del, &retval);
				memset(del_list, 0, list_size);
				num_del = 0;
			}
			 
			hlist_del(&f->hlist);
			kfree(f);
		}

		if (num_del) {
			i40e_aqc_del_filters(vsi, vsi_name, del_list,
					     num_del, &retval);
		}

		kfree(del_list);
		del_list = NULL;
	}

	if (!hlist_empty(&tmp_add_list)) {
		 
		filter_list_len = hw->aq.asq_buf_size /
			       sizeof(struct i40e_aqc_add_macvlan_element_data);
		list_size = filter_list_len *
			       sizeof(struct i40e_aqc_add_macvlan_element_data);
		add_list = kzalloc(list_size, GFP_ATOMIC);
		if (!add_list)
			goto err_no_memory;

		num_add = 0;
		hlist_for_each_entry_safe(new, h, &tmp_add_list, hlist) {
			 
			if (is_broadcast_ether_addr(new->f->macaddr)) {
				if (i40e_aqc_broadcast_filter(vsi, vsi_name,
							      new->f))
					new->state = I40E_FILTER_FAILED;
				else
					new->state = I40E_FILTER_ACTIVE;
				continue;
			}

			 
			if (num_add == 0)
				add_head = new;
			cmd_flags = 0;
			ether_addr_copy(add_list[num_add].mac_addr,
					new->f->macaddr);
			if (new->f->vlan == I40E_VLAN_ANY) {
				add_list[num_add].vlan_tag = 0;
				cmd_flags |= I40E_AQC_MACVLAN_ADD_IGNORE_VLAN;
			} else {
				add_list[num_add].vlan_tag =
					cpu_to_le16((u16)(new->f->vlan));
			}
			add_list[num_add].queue_number = 0;
			 
			add_list[num_add].match_method = I40E_AQC_MM_ERR_NO_RES;
			cmd_flags |= I40E_AQC_MACVLAN_ADD_PERFECT_MATCH;
			add_list[num_add].flags = cpu_to_le16(cmd_flags);
			num_add++;

			 
			if (num_add == filter_list_len) {
				i40e_aqc_add_filters(vsi, vsi_name, add_list,
						     add_head, num_add);
				memset(add_list, 0, list_size);
				num_add = 0;
			}
		}
		if (num_add) {
			i40e_aqc_add_filters(vsi, vsi_name, add_list, add_head,
					     num_add);
		}
		 
		spin_lock_bh(&vsi->mac_filter_hash_lock);
		hlist_for_each_entry_safe(new, h, &tmp_add_list, hlist) {
			 
			if (new->f->state == I40E_FILTER_NEW)
				new->f->state = new->state;
			hlist_del(&new->hlist);
			netdev_hw_addr_refcnt(new->f, vsi->netdev, -1);
			kfree(new);
		}
		spin_unlock_bh(&vsi->mac_filter_hash_lock);
		kfree(add_list);
		add_list = NULL;
	}

	 
	spin_lock_bh(&vsi->mac_filter_hash_lock);
	vsi->active_filters = 0;
	hash_for_each(vsi->mac_filter_hash, bkt, f, hlist) {
		if (f->state == I40E_FILTER_ACTIVE)
			vsi->active_filters++;
		else if (f->state == I40E_FILTER_FAILED)
			failed_filters++;
	}
	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	 
	if (old_overflow && !failed_filters &&
	    vsi->active_filters < vsi->promisc_threshold) {
		dev_info(&pf->pdev->dev,
			 "filter logjam cleared on %s, leaving overflow promiscuous mode\n",
			 vsi_name);
		clear_bit(__I40E_VSI_OVERFLOW_PROMISC, vsi->state);
		vsi->promisc_threshold = 0;
	}

	 
	if (vsi->type == I40E_VSI_SRIOV && pf->vf &&
	    !pf->vf[vsi->vf_id].trusted) {
		clear_bit(__I40E_VSI_OVERFLOW_PROMISC, vsi->state);
		goto out;
	}

	new_overflow = test_bit(__I40E_VSI_OVERFLOW_PROMISC, vsi->state);

	 
	if (!old_overflow && new_overflow)
		vsi->promisc_threshold = (vsi->active_filters * 3) / 4;

	 
	if (changed_flags & IFF_ALLMULTI) {
		bool cur_multipromisc;

		cur_multipromisc = !!(vsi->current_netdev_flags & IFF_ALLMULTI);
		aq_ret = i40e_aq_set_vsi_multicast_promiscuous(&vsi->back->hw,
							       vsi->seid,
							       cur_multipromisc,
							       NULL);
		if (aq_ret) {
			retval = i40e_aq_rc_to_posix(aq_ret,
						     hw->aq.asq_last_status);
			dev_info(&pf->pdev->dev,
				 "set multi promisc failed on %s, err %pe aq_err %s\n",
				 vsi_name,
				 ERR_PTR(aq_ret),
				 i40e_aq_str(hw, hw->aq.asq_last_status));
		} else {
			dev_info(&pf->pdev->dev, "%s allmulti mode.\n",
				 cur_multipromisc ? "entering" : "leaving");
		}
	}

	if ((changed_flags & IFF_PROMISC) || old_overflow != new_overflow) {
		bool cur_promisc;

		cur_promisc = (!!(vsi->current_netdev_flags & IFF_PROMISC) ||
			       new_overflow);
		aq_ret = i40e_set_promiscuous(pf, cur_promisc);
		if (aq_ret) {
			retval = i40e_aq_rc_to_posix(aq_ret,
						     hw->aq.asq_last_status);
			dev_info(&pf->pdev->dev,
				 "Setting promiscuous %s failed on %s, err %pe aq_err %s\n",
				 cur_promisc ? "on" : "off",
				 vsi_name,
				 ERR_PTR(aq_ret),
				 i40e_aq_str(hw, hw->aq.asq_last_status));
		}
	}
out:
	 
	if (retval)
		vsi->flags |= I40E_VSI_FLAG_FILTER_CHANGED;

	clear_bit(__I40E_VSI_SYNCING_FILTERS, vsi->state);
	return retval;

err_no_memory:
	 
	spin_lock_bh(&vsi->mac_filter_hash_lock);
err_no_memory_locked:
	i40e_undo_del_filter_entries(vsi, &tmp_del_list);
	i40e_undo_add_filter_entries(vsi, &tmp_add_list);
	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	vsi->flags |= I40E_VSI_FLAG_FILTER_CHANGED;
	clear_bit(__I40E_VSI_SYNCING_FILTERS, vsi->state);
	return -ENOMEM;
}

 
static void i40e_sync_filters_subtask(struct i40e_pf *pf)
{
	int v;

	if (!pf)
		return;
	if (!test_and_clear_bit(__I40E_MACVLAN_SYNC_PENDING, pf->state))
		return;
	if (test_bit(__I40E_VF_DISABLE, pf->state)) {
		set_bit(__I40E_MACVLAN_SYNC_PENDING, pf->state);
		return;
	}

	for (v = 0; v < pf->num_alloc_vsi; v++) {
		if (pf->vsi[v] &&
		    (pf->vsi[v]->flags & I40E_VSI_FLAG_FILTER_CHANGED) &&
		    !test_bit(__I40E_VSI_RELEASING, pf->vsi[v]->state)) {
			int ret = i40e_sync_vsi_filters(pf->vsi[v]);

			if (ret) {
				 
				set_bit(__I40E_MACVLAN_SYNC_PENDING,
					pf->state);
				break;
			}
		}
	}
}

 
static u16 i40e_calculate_vsi_rx_buf_len(struct i40e_vsi *vsi)
{
	if (!vsi->netdev || (vsi->back->flags & I40E_FLAG_LEGACY_RX))
		return SKB_WITH_OVERHEAD(I40E_RXBUFFER_2048);

	return PAGE_SIZE < 8192 ? I40E_RXBUFFER_3072 : I40E_RXBUFFER_2048;
}

 
static int i40e_max_vsi_frame_size(struct i40e_vsi *vsi,
				   struct bpf_prog *xdp_prog)
{
	u16 rx_buf_len = i40e_calculate_vsi_rx_buf_len(vsi);
	u16 chain_len;

	if (xdp_prog && !xdp_prog->aux->xdp_has_frags)
		chain_len = 1;
	else
		chain_len = I40E_MAX_CHAINED_RX_BUFFERS;

	return min_t(u16, rx_buf_len * chain_len, I40E_MAX_RXBUFFER);
}

 
static int i40e_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	int frame_size;

	frame_size = i40e_max_vsi_frame_size(vsi, vsi->xdp_prog);
	if (new_mtu > frame_size - I40E_PACKET_HDR_PAD) {
		netdev_err(netdev, "Error changing mtu to %d, Max is %d\n",
			   new_mtu, frame_size - I40E_PACKET_HDR_PAD);
		return -EINVAL;
	}

	netdev_dbg(netdev, "changing MTU from %d to %d\n",
		   netdev->mtu, new_mtu);
	netdev->mtu = new_mtu;
	if (netif_running(netdev))
		i40e_vsi_reinit_locked(vsi);
	set_bit(__I40E_CLIENT_SERVICE_REQUESTED, pf->state);
	set_bit(__I40E_CLIENT_L2_CHANGE, pf->state);
	return 0;
}

 
int i40e_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_pf *pf = np->vsi->back;

	switch (cmd) {
	case SIOCGHWTSTAMP:
		return i40e_ptp_get_ts_config(pf, ifr);
	case SIOCSHWTSTAMP:
		return i40e_ptp_set_ts_config(pf, ifr);
	default:
		return -EOPNOTSUPP;
	}
}

 
void i40e_vlan_stripping_enable(struct i40e_vsi *vsi)
{
	struct i40e_vsi_context ctxt;
	int ret;

	 
	if (vsi->info.pvid)
		return;

	if ((vsi->info.valid_sections &
	     cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID)) &&
	    ((vsi->info.port_vlan_flags & I40E_AQ_VSI_PVLAN_MODE_MASK) == 0))
		return;   

	vsi->info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID);
	vsi->info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL |
				    I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;

	ctxt.seid = vsi->seid;
	ctxt.info = vsi->info;
	ret = i40e_aq_update_vsi_params(&vsi->back->hw, &ctxt, NULL);
	if (ret) {
		dev_info(&vsi->back->pdev->dev,
			 "update vlan stripping failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&vsi->back->hw,
				     vsi->back->hw.aq.asq_last_status));
	}
}

 
void i40e_vlan_stripping_disable(struct i40e_vsi *vsi)
{
	struct i40e_vsi_context ctxt;
	int ret;

	 
	if (vsi->info.pvid)
		return;

	if ((vsi->info.valid_sections &
	     cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID)) &&
	    ((vsi->info.port_vlan_flags & I40E_AQ_VSI_PVLAN_EMOD_MASK) ==
	     I40E_AQ_VSI_PVLAN_EMOD_MASK))
		return;   

	vsi->info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID);
	vsi->info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL |
				    I40E_AQ_VSI_PVLAN_EMOD_NOTHING;

	ctxt.seid = vsi->seid;
	ctxt.info = vsi->info;
	ret = i40e_aq_update_vsi_params(&vsi->back->hw, &ctxt, NULL);
	if (ret) {
		dev_info(&vsi->back->pdev->dev,
			 "update vlan stripping failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&vsi->back->hw,
				     vsi->back->hw.aq.asq_last_status));
	}
}

 
int i40e_add_vlan_all_mac(struct i40e_vsi *vsi, s16 vid)
{
	struct i40e_mac_filter *f, *add_f;
	struct hlist_node *h;
	int bkt;

	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
		 
		if (f->state == I40E_FILTER_REMOVE && f->vlan == vid) {
			f->state = I40E_FILTER_ACTIVE;
			continue;
		} else if (f->state == I40E_FILTER_REMOVE) {
			continue;
		}
		add_f = i40e_add_filter(vsi, f->macaddr, vid);
		if (!add_f) {
			dev_info(&vsi->back->pdev->dev,
				 "Could not add vlan filter %d for %pM\n",
				 vid, f->macaddr);
			return -ENOMEM;
		}
	}

	return 0;
}

 
int i40e_vsi_add_vlan(struct i40e_vsi *vsi, u16 vid)
{
	int err;

	if (vsi->info.pvid)
		return -EINVAL;

	 
	if (!vid)
		return 0;

	 
	spin_lock_bh(&vsi->mac_filter_hash_lock);
	err = i40e_add_vlan_all_mac(vsi, vid);
	spin_unlock_bh(&vsi->mac_filter_hash_lock);
	if (err)
		return err;

	 
	i40e_service_event_schedule(vsi->back);
	return 0;
}

 
void i40e_rm_vlan_all_mac(struct i40e_vsi *vsi, s16 vid)
{
	struct i40e_mac_filter *f;
	struct hlist_node *h;
	int bkt;

	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
		if (f->vlan == vid)
			__i40e_del_filter(vsi, f);
	}
}

 
void i40e_vsi_kill_vlan(struct i40e_vsi *vsi, u16 vid)
{
	if (!vid || vsi->info.pvid)
		return;

	spin_lock_bh(&vsi->mac_filter_hash_lock);
	i40e_rm_vlan_all_mac(vsi, vid);
	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	 
	i40e_service_event_schedule(vsi->back);
}

 
static int i40e_vlan_rx_add_vid(struct net_device *netdev,
				__always_unused __be16 proto, u16 vid)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	int ret = 0;

	if (vid >= VLAN_N_VID)
		return -EINVAL;

	ret = i40e_vsi_add_vlan(vsi, vid);
	if (!ret)
		set_bit(vid, vsi->active_vlans);

	return ret;
}

 
static void i40e_vlan_rx_add_vid_up(struct net_device *netdev,
				    __always_unused __be16 proto, u16 vid)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;

	if (vid >= VLAN_N_VID)
		return;
	set_bit(vid, vsi->active_vlans);
}

 
static int i40e_vlan_rx_kill_vid(struct net_device *netdev,
				 __always_unused __be16 proto, u16 vid)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;

	 
	i40e_vsi_kill_vlan(vsi, vid);

	clear_bit(vid, vsi->active_vlans);

	return 0;
}

 
static void i40e_restore_vlan(struct i40e_vsi *vsi)
{
	u16 vid;

	if (!vsi->netdev)
		return;

	if (vsi->netdev->features & NETIF_F_HW_VLAN_CTAG_RX)
		i40e_vlan_stripping_enable(vsi);
	else
		i40e_vlan_stripping_disable(vsi);

	for_each_set_bit(vid, vsi->active_vlans, VLAN_N_VID)
		i40e_vlan_rx_add_vid_up(vsi->netdev, htons(ETH_P_8021Q),
					vid);
}

 
int i40e_vsi_add_pvid(struct i40e_vsi *vsi, u16 vid)
{
	struct i40e_vsi_context ctxt;
	int ret;

	vsi->info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID);
	vsi->info.pvid = cpu_to_le16(vid);
	vsi->info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_TAGGED |
				    I40E_AQ_VSI_PVLAN_INSERT_PVID |
				    I40E_AQ_VSI_PVLAN_EMOD_STR;

	ctxt.seid = vsi->seid;
	ctxt.info = vsi->info;
	ret = i40e_aq_update_vsi_params(&vsi->back->hw, &ctxt, NULL);
	if (ret) {
		dev_info(&vsi->back->pdev->dev,
			 "add pvid failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&vsi->back->hw,
				     vsi->back->hw.aq.asq_last_status));
		return -ENOENT;
	}

	return 0;
}

 
void i40e_vsi_remove_pvid(struct i40e_vsi *vsi)
{
	vsi->info.pvid = 0;

	i40e_vlan_stripping_disable(vsi);
}

 
static int i40e_vsi_setup_tx_resources(struct i40e_vsi *vsi)
{
	int i, err = 0;

	for (i = 0; i < vsi->num_queue_pairs && !err; i++)
		err = i40e_setup_tx_descriptors(vsi->tx_rings[i]);

	if (!i40e_enabled_xdp_vsi(vsi))
		return err;

	for (i = 0; i < vsi->num_queue_pairs && !err; i++)
		err = i40e_setup_tx_descriptors(vsi->xdp_rings[i]);

	return err;
}

 
static void i40e_vsi_free_tx_resources(struct i40e_vsi *vsi)
{
	int i;

	if (vsi->tx_rings) {
		for (i = 0; i < vsi->num_queue_pairs; i++)
			if (vsi->tx_rings[i] && vsi->tx_rings[i]->desc)
				i40e_free_tx_resources(vsi->tx_rings[i]);
	}

	if (vsi->xdp_rings) {
		for (i = 0; i < vsi->num_queue_pairs; i++)
			if (vsi->xdp_rings[i] && vsi->xdp_rings[i]->desc)
				i40e_free_tx_resources(vsi->xdp_rings[i]);
	}
}

 
static int i40e_vsi_setup_rx_resources(struct i40e_vsi *vsi)
{
	int i, err = 0;

	for (i = 0; i < vsi->num_queue_pairs && !err; i++)
		err = i40e_setup_rx_descriptors(vsi->rx_rings[i]);
	return err;
}

 
static void i40e_vsi_free_rx_resources(struct i40e_vsi *vsi)
{
	int i;

	if (!vsi->rx_rings)
		return;

	for (i = 0; i < vsi->num_queue_pairs; i++)
		if (vsi->rx_rings[i] && vsi->rx_rings[i]->desc)
			i40e_free_rx_resources(vsi->rx_rings[i]);
}

 
static void i40e_config_xps_tx_ring(struct i40e_ring *ring)
{
	int cpu;

	if (!ring->q_vector || !ring->netdev || ring->ch)
		return;

	 
	if (test_and_set_bit(__I40E_TX_XPS_INIT_DONE, ring->state))
		return;

	cpu = cpumask_local_spread(ring->q_vector->v_idx, -1);
	netif_set_xps_queue(ring->netdev, get_cpu_mask(cpu),
			    ring->queue_index);
}

 
static struct xsk_buff_pool *i40e_xsk_pool(struct i40e_ring *ring)
{
	bool xdp_on = i40e_enabled_xdp_vsi(ring->vsi);
	int qid = ring->queue_index;

	if (ring_is_xdp(ring))
		qid -= ring->vsi->alloc_queue_pairs;

	if (!xdp_on || !test_bit(qid, ring->vsi->af_xdp_zc_qps))
		return NULL;

	return xsk_get_pool_from_qid(ring->vsi->netdev, qid);
}

 
static int i40e_configure_tx_ring(struct i40e_ring *ring)
{
	struct i40e_vsi *vsi = ring->vsi;
	u16 pf_q = vsi->base_queue + ring->queue_index;
	struct i40e_hw *hw = &vsi->back->hw;
	struct i40e_hmc_obj_txq tx_ctx;
	u32 qtx_ctl = 0;
	int err = 0;

	if (ring_is_xdp(ring))
		ring->xsk_pool = i40e_xsk_pool(ring);

	 
	if (vsi->back->flags & I40E_FLAG_FD_ATR_ENABLED) {
		ring->atr_sample_rate = vsi->back->atr_sample_rate;
		ring->atr_count = 0;
	} else {
		ring->atr_sample_rate = 0;
	}

	 
	i40e_config_xps_tx_ring(ring);

	 
	memset(&tx_ctx, 0, sizeof(tx_ctx));

	tx_ctx.new_context = 1;
	tx_ctx.base = (ring->dma / 128);
	tx_ctx.qlen = ring->count;
	tx_ctx.fd_ena = !!(vsi->back->flags & (I40E_FLAG_FD_SB_ENABLED |
					       I40E_FLAG_FD_ATR_ENABLED));
	tx_ctx.timesync_ena = !!(vsi->back->flags & I40E_FLAG_PTP);
	 
	if (vsi->type != I40E_VSI_FDIR)
		tx_ctx.head_wb_ena = 1;
	tx_ctx.head_wb_addr = ring->dma +
			      (ring->count * sizeof(struct i40e_tx_desc));

	 

	if (ring->ch)
		tx_ctx.rdylist =
			le16_to_cpu(ring->ch->info.qs_handle[ring->dcb_tc]);

	else
		tx_ctx.rdylist = le16_to_cpu(vsi->info.qs_handle[ring->dcb_tc]);

	tx_ctx.rdylist_act = 0;

	 
	err = i40e_clear_lan_tx_queue_context(hw, pf_q);
	if (err) {
		dev_info(&vsi->back->pdev->dev,
			 "Failed to clear LAN Tx queue context on Tx ring %d (pf_q %d), error: %d\n",
			 ring->queue_index, pf_q, err);
		return -ENOMEM;
	}

	 
	err = i40e_set_lan_tx_queue_context(hw, pf_q, &tx_ctx);
	if (err) {
		dev_info(&vsi->back->pdev->dev,
			 "Failed to set LAN Tx queue context on Tx ring %d (pf_q %d, error: %d\n",
			 ring->queue_index, pf_q, err);
		return -ENOMEM;
	}

	 
	if (ring->ch) {
		if (ring->ch->type == I40E_VSI_VMDQ2)
			qtx_ctl = I40E_QTX_CTL_VM_QUEUE;
		else
			return -EINVAL;

		qtx_ctl |= (ring->ch->vsi_number <<
			    I40E_QTX_CTL_VFVM_INDX_SHIFT) &
			    I40E_QTX_CTL_VFVM_INDX_MASK;
	} else {
		if (vsi->type == I40E_VSI_VMDQ2) {
			qtx_ctl = I40E_QTX_CTL_VM_QUEUE;
			qtx_ctl |= ((vsi->id) << I40E_QTX_CTL_VFVM_INDX_SHIFT) &
				    I40E_QTX_CTL_VFVM_INDX_MASK;
		} else {
			qtx_ctl = I40E_QTX_CTL_PF_QUEUE;
		}
	}

	qtx_ctl |= ((hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) &
		    I40E_QTX_CTL_PF_INDX_MASK);
	wr32(hw, I40E_QTX_CTL(pf_q), qtx_ctl);
	i40e_flush(hw);

	 
	ring->tail = hw->hw_addr + I40E_QTX_TAIL(pf_q);

	return 0;
}

 
static unsigned int i40e_rx_offset(struct i40e_ring *rx_ring)
{
	return ring_uses_build_skb(rx_ring) ? I40E_SKB_PAD : 0;
}

 
static int i40e_configure_rx_ring(struct i40e_ring *ring)
{
	struct i40e_vsi *vsi = ring->vsi;
	u32 chain_len = vsi->back->hw.func_caps.rx_buf_chain_len;
	u16 pf_q = vsi->base_queue + ring->queue_index;
	struct i40e_hw *hw = &vsi->back->hw;
	struct i40e_hmc_obj_rxq rx_ctx;
	int err = 0;
	bool ok;
	int ret;

	bitmap_zero(ring->state, __I40E_RING_STATE_NBITS);

	 
	memset(&rx_ctx, 0, sizeof(rx_ctx));

	if (ring->vsi->type == I40E_VSI_MAIN)
		xdp_rxq_info_unreg_mem_model(&ring->xdp_rxq);

	ring->xsk_pool = i40e_xsk_pool(ring);
	if (ring->xsk_pool) {
		ring->rx_buf_len =
		  xsk_pool_get_rx_frame_size(ring->xsk_pool);
		ret = xdp_rxq_info_reg_mem_model(&ring->xdp_rxq,
						 MEM_TYPE_XSK_BUFF_POOL,
						 NULL);
		if (ret)
			return ret;
		dev_info(&vsi->back->pdev->dev,
			 "Registered XDP mem model MEM_TYPE_XSK_BUFF_POOL on Rx ring %d\n",
			 ring->queue_index);

	} else {
		ring->rx_buf_len = vsi->rx_buf_len;
		if (ring->vsi->type == I40E_VSI_MAIN) {
			ret = xdp_rxq_info_reg_mem_model(&ring->xdp_rxq,
							 MEM_TYPE_PAGE_SHARED,
							 NULL);
			if (ret)
				return ret;
		}
	}

	xdp_init_buff(&ring->xdp, i40e_rx_pg_size(ring) / 2, &ring->xdp_rxq);

	rx_ctx.dbuff = DIV_ROUND_UP(ring->rx_buf_len,
				    BIT_ULL(I40E_RXQ_CTX_DBUFF_SHIFT));

	rx_ctx.base = (ring->dma / 128);
	rx_ctx.qlen = ring->count;

	 
	rx_ctx.dsize = 0;

	 
	rx_ctx.hsplit_0 = 0;

	rx_ctx.rxmax = min_t(u16, vsi->max_frame, chain_len * ring->rx_buf_len);
	if (hw->revision_id == 0)
		rx_ctx.lrxqthresh = 0;
	else
		rx_ctx.lrxqthresh = 1;
	rx_ctx.crcstrip = 1;
	rx_ctx.l2tsel = 1;
	 
	rx_ctx.showiv = 0;
	 
	rx_ctx.prefena = 1;

	 
	err = i40e_clear_lan_rx_queue_context(hw, pf_q);
	if (err) {
		dev_info(&vsi->back->pdev->dev,
			 "Failed to clear LAN Rx queue context on Rx ring %d (pf_q %d), error: %d\n",
			 ring->queue_index, pf_q, err);
		return -ENOMEM;
	}

	 
	err = i40e_set_lan_rx_queue_context(hw, pf_q, &rx_ctx);
	if (err) {
		dev_info(&vsi->back->pdev->dev,
			 "Failed to set LAN Rx queue context on Rx ring %d (pf_q %d), error: %d\n",
			 ring->queue_index, pf_q, err);
		return -ENOMEM;
	}

	 
	if (!vsi->netdev || (vsi->back->flags & I40E_FLAG_LEGACY_RX)) {
		if (I40E_2K_TOO_SMALL_WITH_PADDING) {
			dev_info(&vsi->back->pdev->dev,
				 "2k Rx buffer is too small to fit standard MTU and skb_shared_info\n");
			return -EOPNOTSUPP;
		}
		clear_ring_build_skb_enabled(ring);
	} else {
		set_ring_build_skb_enabled(ring);
	}

	ring->rx_offset = i40e_rx_offset(ring);

	 
	ring->tail = hw->hw_addr + I40E_QRX_TAIL(pf_q);
	writel(0, ring->tail);

	if (ring->xsk_pool) {
		xsk_pool_set_rxq_info(ring->xsk_pool, &ring->xdp_rxq);
		ok = i40e_alloc_rx_buffers_zc(ring, I40E_DESC_UNUSED(ring));
	} else {
		ok = !i40e_alloc_rx_buffers(ring, I40E_DESC_UNUSED(ring));
	}
	if (!ok) {
		 
		dev_info(&vsi->back->pdev->dev,
			 "Failed to allocate some buffers on %sRx ring %d (pf_q %d)\n",
			 ring->xsk_pool ? "AF_XDP ZC enabled " : "",
			 ring->queue_index, pf_q);
	}

	return 0;
}

 
static int i40e_vsi_configure_tx(struct i40e_vsi *vsi)
{
	int err = 0;
	u16 i;

	for (i = 0; (i < vsi->num_queue_pairs) && !err; i++)
		err = i40e_configure_tx_ring(vsi->tx_rings[i]);

	if (err || !i40e_enabled_xdp_vsi(vsi))
		return err;

	for (i = 0; (i < vsi->num_queue_pairs) && !err; i++)
		err = i40e_configure_tx_ring(vsi->xdp_rings[i]);

	return err;
}

 
static int i40e_vsi_configure_rx(struct i40e_vsi *vsi)
{
	int err = 0;
	u16 i;

	vsi->max_frame = i40e_max_vsi_frame_size(vsi, vsi->xdp_prog);
	vsi->rx_buf_len = i40e_calculate_vsi_rx_buf_len(vsi);

#if (PAGE_SIZE < 8192)
	if (vsi->netdev && !I40E_2K_TOO_SMALL_WITH_PADDING &&
	    vsi->netdev->mtu <= ETH_DATA_LEN) {
		vsi->rx_buf_len = I40E_RXBUFFER_1536 - NET_IP_ALIGN;
		vsi->max_frame = vsi->rx_buf_len;
	}
#endif

	 
	for (i = 0; i < vsi->num_queue_pairs && !err; i++)
		err = i40e_configure_rx_ring(vsi->rx_rings[i]);

	return err;
}

 
static void i40e_vsi_config_dcb_rings(struct i40e_vsi *vsi)
{
	struct i40e_ring *tx_ring, *rx_ring;
	u16 qoffset, qcount;
	int i, n;

	if (!(vsi->back->flags & I40E_FLAG_DCB_ENABLED)) {
		 
		for (i = 0; i < vsi->num_queue_pairs; i++) {
			rx_ring = vsi->rx_rings[i];
			tx_ring = vsi->tx_rings[i];
			rx_ring->dcb_tc = 0;
			tx_ring->dcb_tc = 0;
		}
		return;
	}

	for (n = 0; n < I40E_MAX_TRAFFIC_CLASS; n++) {
		if (!(vsi->tc_config.enabled_tc & BIT_ULL(n)))
			continue;

		qoffset = vsi->tc_config.tc_info[n].qoffset;
		qcount = vsi->tc_config.tc_info[n].qcount;
		for (i = qoffset; i < (qoffset + qcount); i++) {
			rx_ring = vsi->rx_rings[i];
			tx_ring = vsi->tx_rings[i];
			rx_ring->dcb_tc = n;
			tx_ring->dcb_tc = n;
		}
	}
}

 
static void i40e_set_vsi_rx_mode(struct i40e_vsi *vsi)
{
	if (vsi->netdev)
		i40e_set_rx_mode(vsi->netdev);
}

 
static void i40e_reset_fdir_filter_cnt(struct i40e_pf *pf)
{
	pf->fd_tcp4_filter_cnt = 0;
	pf->fd_udp4_filter_cnt = 0;
	pf->fd_sctp4_filter_cnt = 0;
	pf->fd_ip4_filter_cnt = 0;
	pf->fd_tcp6_filter_cnt = 0;
	pf->fd_udp6_filter_cnt = 0;
	pf->fd_sctp6_filter_cnt = 0;
	pf->fd_ip6_filter_cnt = 0;
}

 
static void i40e_fdir_filter_restore(struct i40e_vsi *vsi)
{
	struct i40e_fdir_filter *filter;
	struct i40e_pf *pf = vsi->back;
	struct hlist_node *node;

	if (!(pf->flags & I40E_FLAG_FD_SB_ENABLED))
		return;

	 
	i40e_reset_fdir_filter_cnt(pf);

	hlist_for_each_entry_safe(filter, node,
				  &pf->fdir_filter_list, fdir_node) {
		i40e_add_del_fdir(vsi, filter, true);
	}
}

 
static int i40e_vsi_configure(struct i40e_vsi *vsi)
{
	int err;

	i40e_set_vsi_rx_mode(vsi);
	i40e_restore_vlan(vsi);
	i40e_vsi_config_dcb_rings(vsi);
	err = i40e_vsi_configure_tx(vsi);
	if (!err)
		err = i40e_vsi_configure_rx(vsi);

	return err;
}

 
static void i40e_vsi_configure_msix(struct i40e_vsi *vsi)
{
	bool has_xdp = i40e_enabled_xdp_vsi(vsi);
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	u16 vector;
	int i, q;
	u32 qp;

	 
	qp = vsi->base_queue;
	vector = vsi->base_vector;
	for (i = 0; i < vsi->num_q_vectors; i++, vector++) {
		struct i40e_q_vector *q_vector = vsi->q_vectors[i];

		q_vector->rx.next_update = jiffies + 1;
		q_vector->rx.target_itr =
			ITR_TO_REG(vsi->rx_rings[i]->itr_setting);
		wr32(hw, I40E_PFINT_ITRN(I40E_RX_ITR, vector - 1),
		     q_vector->rx.target_itr >> 1);
		q_vector->rx.current_itr = q_vector->rx.target_itr;

		q_vector->tx.next_update = jiffies + 1;
		q_vector->tx.target_itr =
			ITR_TO_REG(vsi->tx_rings[i]->itr_setting);
		wr32(hw, I40E_PFINT_ITRN(I40E_TX_ITR, vector - 1),
		     q_vector->tx.target_itr >> 1);
		q_vector->tx.current_itr = q_vector->tx.target_itr;

		wr32(hw, I40E_PFINT_RATEN(vector - 1),
		     i40e_intrl_usec_to_reg(vsi->int_rate_limit));

		 
		wr32(hw, I40E_PFINT_LNKLSTN(vector - 1), qp);
		for (q = 0; q < q_vector->num_ringpairs; q++) {
			u32 nextqp = has_xdp ? qp + vsi->alloc_queue_pairs : qp;
			u32 val;

			val = I40E_QINT_RQCTL_CAUSE_ENA_MASK |
			      (I40E_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT) |
			      (vector << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
			      (nextqp << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
			      (I40E_QUEUE_TYPE_TX <<
			       I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT);

			wr32(hw, I40E_QINT_RQCTL(qp), val);

			if (has_xdp) {
				 
				val = I40E_QINT_TQCTL_CAUSE_ENA_MASK |
				      (I40E_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT) |
				      (vector << I40E_QINT_TQCTL_MSIX_INDX_SHIFT) |
				      (qp << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT) |
				      (I40E_QUEUE_TYPE_TX <<
				       I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT);

				wr32(hw, I40E_QINT_TQCTL(nextqp), val);
			}
			 
			val = I40E_QINT_TQCTL_CAUSE_ENA_MASK |
			      (I40E_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT) |
			      (vector << I40E_QINT_TQCTL_MSIX_INDX_SHIFT) |
			      ((qp + 1) << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT) |
			      (I40E_QUEUE_TYPE_RX <<
			       I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT);

			 
			if (q == (q_vector->num_ringpairs - 1))
				val |= (I40E_QUEUE_END_OF_LIST <<
					I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);

			wr32(hw, I40E_QINT_TQCTL(qp), val);
			qp++;
		}
	}

	i40e_flush(hw);
}

 
static void i40e_enable_misc_int_causes(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u32 val;

	 
	wr32(hw, I40E_PFINT_ICR0_ENA, 0);   
	rd32(hw, I40E_PFINT_ICR0);          

	val = I40E_PFINT_ICR0_ENA_ECC_ERR_MASK       |
	      I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK    |
	      I40E_PFINT_ICR0_ENA_GRST_MASK          |
	      I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK |
	      I40E_PFINT_ICR0_ENA_GPIO_MASK          |
	      I40E_PFINT_ICR0_ENA_HMC_ERR_MASK       |
	      I40E_PFINT_ICR0_ENA_VFLR_MASK          |
	      I40E_PFINT_ICR0_ENA_ADMINQ_MASK;

	if (pf->flags & I40E_FLAG_IWARP_ENABLED)
		val |= I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK;

	if (pf->flags & I40E_FLAG_PTP)
		val |= I40E_PFINT_ICR0_ENA_TIMESYNC_MASK;

	wr32(hw, I40E_PFINT_ICR0_ENA, val);

	 
	wr32(hw, I40E_PFINT_DYN_CTL0, I40E_PFINT_DYN_CTL0_SW_ITR_INDX_MASK |
					I40E_PFINT_DYN_CTL0_INTENA_MSK_MASK);

	 
	wr32(hw, I40E_PFINT_STAT_CTL0, 0);
}

 
static void i40e_configure_msi_and_legacy(struct i40e_vsi *vsi)
{
	u32 nextqp = i40e_enabled_xdp_vsi(vsi) ? vsi->alloc_queue_pairs : 0;
	struct i40e_q_vector *q_vector = vsi->q_vectors[0];
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;

	 
	q_vector->rx.next_update = jiffies + 1;
	q_vector->rx.target_itr = ITR_TO_REG(vsi->rx_rings[0]->itr_setting);
	wr32(hw, I40E_PFINT_ITR0(I40E_RX_ITR), q_vector->rx.target_itr >> 1);
	q_vector->rx.current_itr = q_vector->rx.target_itr;
	q_vector->tx.next_update = jiffies + 1;
	q_vector->tx.target_itr = ITR_TO_REG(vsi->tx_rings[0]->itr_setting);
	wr32(hw, I40E_PFINT_ITR0(I40E_TX_ITR), q_vector->tx.target_itr >> 1);
	q_vector->tx.current_itr = q_vector->tx.target_itr;

	i40e_enable_misc_int_causes(pf);

	 
	wr32(hw, I40E_PFINT_LNKLST0, 0);

	 
	wr32(hw, I40E_QINT_RQCTL(0), I40E_QINT_RQCTL_VAL(nextqp, 0, TX));

	if (i40e_enabled_xdp_vsi(vsi)) {
		 
		wr32(hw, I40E_QINT_TQCTL(nextqp),
		     I40E_QINT_TQCTL_VAL(nextqp, 0, TX));
	}

	 
	wr32(hw, I40E_QINT_TQCTL(0),
	     I40E_QINT_TQCTL_VAL(I40E_QUEUE_END_OF_LIST, 0, RX));
	i40e_flush(hw);
}

 
void i40e_irq_dynamic_disable_icr0(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;

	wr32(hw, I40E_PFINT_DYN_CTL0,
	     I40E_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);
	i40e_flush(hw);
}

 
void i40e_irq_dynamic_enable_icr0(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u32 val;

	val = I40E_PFINT_DYN_CTL0_INTENA_MASK   |
	      I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
	      (I40E_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);

	wr32(hw, I40E_PFINT_DYN_CTL0, val);
	i40e_flush(hw);
}

 
static irqreturn_t i40e_msix_clean_rings(int irq, void *data)
{
	struct i40e_q_vector *q_vector = data;

	if (!q_vector->tx.ring && !q_vector->rx.ring)
		return IRQ_HANDLED;

	napi_schedule_irqoff(&q_vector->napi);

	return IRQ_HANDLED;
}

 
static void i40e_irq_affinity_notify(struct irq_affinity_notify *notify,
				     const cpumask_t *mask)
{
	struct i40e_q_vector *q_vector =
		container_of(notify, struct i40e_q_vector, affinity_notify);

	cpumask_copy(&q_vector->affinity_mask, mask);
}

 
static void i40e_irq_affinity_release(struct kref *ref) {}

 
static int i40e_vsi_request_irq_msix(struct i40e_vsi *vsi, char *basename)
{
	int q_vectors = vsi->num_q_vectors;
	struct i40e_pf *pf = vsi->back;
	int base = vsi->base_vector;
	int rx_int_idx = 0;
	int tx_int_idx = 0;
	int vector, err;
	int irq_num;
	int cpu;

	for (vector = 0; vector < q_vectors; vector++) {
		struct i40e_q_vector *q_vector = vsi->q_vectors[vector];

		irq_num = pf->msix_entries[base + vector].vector;

		if (q_vector->tx.ring && q_vector->rx.ring) {
			snprintf(q_vector->name, sizeof(q_vector->name) - 1,
				 "%s-%s-%d", basename, "TxRx", rx_int_idx++);
			tx_int_idx++;
		} else if (q_vector->rx.ring) {
			snprintf(q_vector->name, sizeof(q_vector->name) - 1,
				 "%s-%s-%d", basename, "rx", rx_int_idx++);
		} else if (q_vector->tx.ring) {
			snprintf(q_vector->name, sizeof(q_vector->name) - 1,
				 "%s-%s-%d", basename, "tx", tx_int_idx++);
		} else {
			 
			continue;
		}
		err = request_irq(irq_num,
				  vsi->irq_handler,
				  0,
				  q_vector->name,
				  q_vector);
		if (err) {
			dev_info(&pf->pdev->dev,
				 "MSIX request_irq failed, error: %d\n", err);
			goto free_queue_irqs;
		}

		 
		q_vector->irq_num = irq_num;
		q_vector->affinity_notify.notify = i40e_irq_affinity_notify;
		q_vector->affinity_notify.release = i40e_irq_affinity_release;
		irq_set_affinity_notifier(irq_num, &q_vector->affinity_notify);
		 
		cpu = cpumask_local_spread(q_vector->v_idx, -1);
		irq_update_affinity_hint(irq_num, get_cpu_mask(cpu));
	}

	vsi->irqs_ready = true;
	return 0;

free_queue_irqs:
	while (vector) {
		vector--;
		irq_num = pf->msix_entries[base + vector].vector;
		irq_set_affinity_notifier(irq_num, NULL);
		irq_update_affinity_hint(irq_num, NULL);
		free_irq(irq_num, &vsi->q_vectors[vector]);
	}
	return err;
}

 
static void i40e_vsi_disable_irq(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	int base = vsi->base_vector;
	int i;

	 
	for (i = 0; i < vsi->num_queue_pairs; i++) {
		u32 val;

		val = rd32(hw, I40E_QINT_TQCTL(vsi->tx_rings[i]->reg_idx));
		val &= ~I40E_QINT_TQCTL_CAUSE_ENA_MASK;
		wr32(hw, I40E_QINT_TQCTL(vsi->tx_rings[i]->reg_idx), val);

		val = rd32(hw, I40E_QINT_RQCTL(vsi->rx_rings[i]->reg_idx));
		val &= ~I40E_QINT_RQCTL_CAUSE_ENA_MASK;
		wr32(hw, I40E_QINT_RQCTL(vsi->rx_rings[i]->reg_idx), val);

		if (!i40e_enabled_xdp_vsi(vsi))
			continue;
		wr32(hw, I40E_QINT_TQCTL(vsi->xdp_rings[i]->reg_idx), 0);
	}

	 
	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		for (i = vsi->base_vector;
		     i < (vsi->num_q_vectors + vsi->base_vector); i++)
			wr32(hw, I40E_PFINT_DYN_CTLN(i - 1), 0);

		i40e_flush(hw);
		for (i = 0; i < vsi->num_q_vectors; i++)
			synchronize_irq(pf->msix_entries[i + base].vector);
	} else {
		 
		wr32(hw, I40E_PFINT_ICR0_ENA, 0);
		wr32(hw, I40E_PFINT_DYN_CTL0, 0);
		i40e_flush(hw);
		synchronize_irq(pf->pdev->irq);
	}
}

 
static int i40e_vsi_enable_irq(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int i;

	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		for (i = 0; i < vsi->num_q_vectors; i++)
			i40e_irq_dynamic_enable(vsi, i);
	} else {
		i40e_irq_dynamic_enable_icr0(pf);
	}

	i40e_flush(&pf->hw);
	return 0;
}

 
static void i40e_free_misc_vector(struct i40e_pf *pf)
{
	 
	wr32(&pf->hw, I40E_PFINT_ICR0_ENA, 0);
	i40e_flush(&pf->hw);

	if (pf->flags & I40E_FLAG_MSIX_ENABLED && pf->msix_entries) {
		free_irq(pf->msix_entries[0].vector, pf);
		clear_bit(__I40E_MISC_IRQ_REQUESTED, pf->state);
	}
}

 
static irqreturn_t i40e_intr(int irq, void *data)
{
	struct i40e_pf *pf = (struct i40e_pf *)data;
	struct i40e_hw *hw = &pf->hw;
	irqreturn_t ret = IRQ_NONE;
	u32 icr0, icr0_remaining;
	u32 val, ena_mask;

	icr0 = rd32(hw, I40E_PFINT_ICR0);
	ena_mask = rd32(hw, I40E_PFINT_ICR0_ENA);

	 
	if ((icr0 & I40E_PFINT_ICR0_INTEVENT_MASK) == 0)
		goto enable_intr;

	 
	if (((icr0 & ~I40E_PFINT_ICR0_INTEVENT_MASK) == 0) ||
	    (icr0 & I40E_PFINT_ICR0_SWINT_MASK))
		pf->sw_int_count++;

	if ((pf->flags & I40E_FLAG_IWARP_ENABLED) &&
	    (icr0 & I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK)) {
		ena_mask &= ~I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK;
		dev_dbg(&pf->pdev->dev, "cleared PE_CRITERR\n");
		set_bit(__I40E_CORE_RESET_REQUESTED, pf->state);
	}

	 
	if (icr0 & I40E_PFINT_ICR0_QUEUE_0_MASK) {
		struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
		struct i40e_q_vector *q_vector = vsi->q_vectors[0];

		 
		if (!test_bit(__I40E_DOWN, pf->state))
			napi_schedule_irqoff(&q_vector->napi);
	}

	if (icr0 & I40E_PFINT_ICR0_ADMINQ_MASK) {
		ena_mask &= ~I40E_PFINT_ICR0_ENA_ADMINQ_MASK;
		set_bit(__I40E_ADMINQ_EVENT_PENDING, pf->state);
		i40e_debug(&pf->hw, I40E_DEBUG_NVM, "AdminQ event\n");
	}

	if (icr0 & I40E_PFINT_ICR0_MAL_DETECT_MASK) {
		ena_mask &= ~I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK;
		set_bit(__I40E_MDD_EVENT_PENDING, pf->state);
	}

	if (icr0 & I40E_PFINT_ICR0_VFLR_MASK) {
		 
		if (test_bit(__I40E_VF_RESETS_DISABLED, pf->state)) {
			u32 reg = rd32(hw, I40E_PFINT_ICR0_ENA);

			reg &= ~I40E_PFINT_ICR0_VFLR_MASK;
			wr32(hw, I40E_PFINT_ICR0_ENA, reg);
		} else {
			ena_mask &= ~I40E_PFINT_ICR0_ENA_VFLR_MASK;
			set_bit(__I40E_VFLR_EVENT_PENDING, pf->state);
		}
	}

	if (icr0 & I40E_PFINT_ICR0_GRST_MASK) {
		if (!test_bit(__I40E_RESET_RECOVERY_PENDING, pf->state))
			set_bit(__I40E_RESET_INTR_RECEIVED, pf->state);
		ena_mask &= ~I40E_PFINT_ICR0_ENA_GRST_MASK;
		val = rd32(hw, I40E_GLGEN_RSTAT);
		val = (val & I40E_GLGEN_RSTAT_RESET_TYPE_MASK)
		       >> I40E_GLGEN_RSTAT_RESET_TYPE_SHIFT;
		if (val == I40E_RESET_CORER) {
			pf->corer_count++;
		} else if (val == I40E_RESET_GLOBR) {
			pf->globr_count++;
		} else if (val == I40E_RESET_EMPR) {
			pf->empr_count++;
			set_bit(__I40E_EMP_RESET_INTR_RECEIVED, pf->state);
		}
	}

	if (icr0 & I40E_PFINT_ICR0_HMC_ERR_MASK) {
		icr0 &= ~I40E_PFINT_ICR0_HMC_ERR_MASK;
		dev_info(&pf->pdev->dev, "HMC error interrupt\n");
		dev_info(&pf->pdev->dev, "HMC error info 0x%x, HMC error data 0x%x\n",
			 rd32(hw, I40E_PFHMC_ERRORINFO),
			 rd32(hw, I40E_PFHMC_ERRORDATA));
	}

	if (icr0 & I40E_PFINT_ICR0_TIMESYNC_MASK) {
		u32 prttsyn_stat = rd32(hw, I40E_PRTTSYN_STAT_0);

		if (prttsyn_stat & I40E_PRTTSYN_STAT_0_EVENT0_MASK)
			schedule_work(&pf->ptp_extts0_work);

		if (prttsyn_stat & I40E_PRTTSYN_STAT_0_TXTIME_MASK)
			i40e_ptp_tx_hwtstamp(pf);

		icr0 &= ~I40E_PFINT_ICR0_ENA_TIMESYNC_MASK;
	}

	 
	icr0_remaining = icr0 & ena_mask;
	if (icr0_remaining) {
		dev_info(&pf->pdev->dev, "unhandled interrupt icr0=0x%08x\n",
			 icr0_remaining);
		if ((icr0_remaining & I40E_PFINT_ICR0_PE_CRITERR_MASK) ||
		    (icr0_remaining & I40E_PFINT_ICR0_PCI_EXCEPTION_MASK) ||
		    (icr0_remaining & I40E_PFINT_ICR0_ECC_ERR_MASK)) {
			dev_info(&pf->pdev->dev, "device will be reset\n");
			set_bit(__I40E_PF_RESET_REQUESTED, pf->state);
			i40e_service_event_schedule(pf);
		}
		ena_mask &= ~icr0_remaining;
	}
	ret = IRQ_HANDLED;

enable_intr:
	 
	wr32(hw, I40E_PFINT_ICR0_ENA, ena_mask);
	if (!test_bit(__I40E_DOWN, pf->state) ||
	    test_bit(__I40E_RECOVERY_MODE, pf->state)) {
		i40e_service_event_schedule(pf);
		i40e_irq_dynamic_enable_icr0(pf);
	}

	return ret;
}

 
static bool i40e_clean_fdir_tx_irq(struct i40e_ring *tx_ring, int budget)
{
	struct i40e_vsi *vsi = tx_ring->vsi;
	u16 i = tx_ring->next_to_clean;
	struct i40e_tx_buffer *tx_buf;
	struct i40e_tx_desc *tx_desc;

	tx_buf = &tx_ring->tx_bi[i];
	tx_desc = I40E_TX_DESC(tx_ring, i);
	i -= tx_ring->count;

	do {
		struct i40e_tx_desc *eop_desc = tx_buf->next_to_watch;

		 
		if (!eop_desc)
			break;

		 
		smp_rmb();

		 
		if (!(eop_desc->cmd_type_offset_bsz &
		      cpu_to_le64(I40E_TX_DESC_DTYPE_DESC_DONE)))
			break;

		 
		tx_buf->next_to_watch = NULL;

		tx_desc->buffer_addr = 0;
		tx_desc->cmd_type_offset_bsz = 0;
		 
		tx_buf++;
		tx_desc++;
		i++;
		if (unlikely(!i)) {
			i -= tx_ring->count;
			tx_buf = tx_ring->tx_bi;
			tx_desc = I40E_TX_DESC(tx_ring, 0);
		}
		 
		dma_unmap_single(tx_ring->dev,
				 dma_unmap_addr(tx_buf, dma),
				 dma_unmap_len(tx_buf, len),
				 DMA_TO_DEVICE);
		if (tx_buf->tx_flags & I40E_TX_FLAGS_FD_SB)
			kfree(tx_buf->raw_buf);

		tx_buf->raw_buf = NULL;
		tx_buf->tx_flags = 0;
		tx_buf->next_to_watch = NULL;
		dma_unmap_len_set(tx_buf, len, 0);
		tx_desc->buffer_addr = 0;
		tx_desc->cmd_type_offset_bsz = 0;

		 
		tx_buf++;
		tx_desc++;
		i++;
		if (unlikely(!i)) {
			i -= tx_ring->count;
			tx_buf = tx_ring->tx_bi;
			tx_desc = I40E_TX_DESC(tx_ring, 0);
		}

		 
		budget--;
	} while (likely(budget));

	i += tx_ring->count;
	tx_ring->next_to_clean = i;

	if (vsi->back->flags & I40E_FLAG_MSIX_ENABLED)
		i40e_irq_dynamic_enable(vsi, tx_ring->q_vector->v_idx);

	return budget > 0;
}

 
static irqreturn_t i40e_fdir_clean_ring(int irq, void *data)
{
	struct i40e_q_vector *q_vector = data;
	struct i40e_vsi *vsi;

	if (!q_vector->tx.ring)
		return IRQ_HANDLED;

	vsi = q_vector->tx.ring->vsi;
	i40e_clean_fdir_tx_irq(q_vector->tx.ring, vsi->work_limit);

	return IRQ_HANDLED;
}

 
static void i40e_map_vector_to_qp(struct i40e_vsi *vsi, int v_idx, int qp_idx)
{
	struct i40e_q_vector *q_vector = vsi->q_vectors[v_idx];
	struct i40e_ring *tx_ring = vsi->tx_rings[qp_idx];
	struct i40e_ring *rx_ring = vsi->rx_rings[qp_idx];

	tx_ring->q_vector = q_vector;
	tx_ring->next = q_vector->tx.ring;
	q_vector->tx.ring = tx_ring;
	q_vector->tx.count++;

	 
	if (i40e_enabled_xdp_vsi(vsi)) {
		struct i40e_ring *xdp_ring = vsi->xdp_rings[qp_idx];

		xdp_ring->q_vector = q_vector;
		xdp_ring->next = q_vector->tx.ring;
		q_vector->tx.ring = xdp_ring;
		q_vector->tx.count++;
	}

	rx_ring->q_vector = q_vector;
	rx_ring->next = q_vector->rx.ring;
	q_vector->rx.ring = rx_ring;
	q_vector->rx.count++;
}

 
static void i40e_vsi_map_rings_to_vectors(struct i40e_vsi *vsi)
{
	int qp_remaining = vsi->num_queue_pairs;
	int q_vectors = vsi->num_q_vectors;
	int num_ringpairs;
	int v_start = 0;
	int qp_idx = 0;

	 
	for (; v_start < q_vectors; v_start++) {
		struct i40e_q_vector *q_vector = vsi->q_vectors[v_start];

		num_ringpairs = DIV_ROUND_UP(qp_remaining, q_vectors - v_start);

		q_vector->num_ringpairs = num_ringpairs;
		q_vector->reg_idx = q_vector->v_idx + vsi->base_vector - 1;

		q_vector->rx.count = 0;
		q_vector->tx.count = 0;
		q_vector->rx.ring = NULL;
		q_vector->tx.ring = NULL;

		while (num_ringpairs--) {
			i40e_map_vector_to_qp(vsi, v_start, qp_idx);
			qp_idx++;
			qp_remaining--;
		}
	}
}

 
static int i40e_vsi_request_irq(struct i40e_vsi *vsi, char *basename)
{
	struct i40e_pf *pf = vsi->back;
	int err;

	if (pf->flags & I40E_FLAG_MSIX_ENABLED)
		err = i40e_vsi_request_irq_msix(vsi, basename);
	else if (pf->flags & I40E_FLAG_MSI_ENABLED)
		err = request_irq(pf->pdev->irq, i40e_intr, 0,
				  pf->int_name, pf);
	else
		err = request_irq(pf->pdev->irq, i40e_intr, IRQF_SHARED,
				  pf->int_name, pf);

	if (err)
		dev_info(&pf->pdev->dev, "request_irq failed, Error %d\n", err);

	return err;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
 
static void i40e_netpoll(struct net_device *netdev)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	int i;

	 
	if (test_bit(__I40E_VSI_DOWN, vsi->state))
		return;

	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		for (i = 0; i < vsi->num_q_vectors; i++)
			i40e_msix_clean_rings(0, vsi->q_vectors[i]);
	} else {
		i40e_intr(pf->pdev->irq, netdev);
	}
}
#endif

#define I40E_QTX_ENA_WAIT_COUNT 50

 
static int i40e_pf_txq_wait(struct i40e_pf *pf, int pf_q, bool enable)
{
	int i;
	u32 tx_reg;

	for (i = 0; i < I40E_QUEUE_WAIT_RETRY_LIMIT; i++) {
		tx_reg = rd32(&pf->hw, I40E_QTX_ENA(pf_q));
		if (enable == !!(tx_reg & I40E_QTX_ENA_QENA_STAT_MASK))
			break;

		usleep_range(10, 20);
	}
	if (i >= I40E_QUEUE_WAIT_RETRY_LIMIT)
		return -ETIMEDOUT;

	return 0;
}

 
static void i40e_control_tx_q(struct i40e_pf *pf, int pf_q, bool enable)
{
	struct i40e_hw *hw = &pf->hw;
	u32 tx_reg;
	int i;

	 
	i40e_pre_tx_queue_cfg(&pf->hw, pf_q, enable);
	if (!enable)
		usleep_range(10, 20);

	for (i = 0; i < I40E_QTX_ENA_WAIT_COUNT; i++) {
		tx_reg = rd32(hw, I40E_QTX_ENA(pf_q));
		if (((tx_reg >> I40E_QTX_ENA_QENA_REQ_SHIFT) & 1) ==
		    ((tx_reg >> I40E_QTX_ENA_QENA_STAT_SHIFT) & 1))
			break;
		usleep_range(1000, 2000);
	}

	 
	if (enable == !!(tx_reg & I40E_QTX_ENA_QENA_STAT_MASK))
		return;

	 
	if (enable) {
		wr32(hw, I40E_QTX_HEAD(pf_q), 0);
		tx_reg |= I40E_QTX_ENA_QENA_REQ_MASK;
	} else {
		tx_reg &= ~I40E_QTX_ENA_QENA_REQ_MASK;
	}

	wr32(hw, I40E_QTX_ENA(pf_q), tx_reg);
}

 
int i40e_control_wait_tx_q(int seid, struct i40e_pf *pf, int pf_q,
			   bool is_xdp, bool enable)
{
	int ret;

	i40e_control_tx_q(pf, pf_q, enable);

	 
	ret = i40e_pf_txq_wait(pf, pf_q, enable);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "VSI seid %d %sTx ring %d %sable timeout\n",
			 seid, (is_xdp ? "XDP " : ""), pf_q,
			 (enable ? "en" : "dis"));
	}

	return ret;
}

 
static int i40e_vsi_enable_tx(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int i, pf_q, ret = 0;

	pf_q = vsi->base_queue;
	for (i = 0; i < vsi->num_queue_pairs; i++, pf_q++) {
		ret = i40e_control_wait_tx_q(vsi->seid, pf,
					     pf_q,
					     false  , true);
		if (ret)
			break;

		if (!i40e_enabled_xdp_vsi(vsi))
			continue;

		ret = i40e_control_wait_tx_q(vsi->seid, pf,
					     pf_q + vsi->alloc_queue_pairs,
					     true  , true);
		if (ret)
			break;
	}
	return ret;
}

 
static int i40e_pf_rxq_wait(struct i40e_pf *pf, int pf_q, bool enable)
{
	int i;
	u32 rx_reg;

	for (i = 0; i < I40E_QUEUE_WAIT_RETRY_LIMIT; i++) {
		rx_reg = rd32(&pf->hw, I40E_QRX_ENA(pf_q));
		if (enable == !!(rx_reg & I40E_QRX_ENA_QENA_STAT_MASK))
			break;

		usleep_range(10, 20);
	}
	if (i >= I40E_QUEUE_WAIT_RETRY_LIMIT)
		return -ETIMEDOUT;

	return 0;
}

 
static void i40e_control_rx_q(struct i40e_pf *pf, int pf_q, bool enable)
{
	struct i40e_hw *hw = &pf->hw;
	u32 rx_reg;
	int i;

	for (i = 0; i < I40E_QTX_ENA_WAIT_COUNT; i++) {
		rx_reg = rd32(hw, I40E_QRX_ENA(pf_q));
		if (((rx_reg >> I40E_QRX_ENA_QENA_REQ_SHIFT) & 1) ==
		    ((rx_reg >> I40E_QRX_ENA_QENA_STAT_SHIFT) & 1))
			break;
		usleep_range(1000, 2000);
	}

	 
	if (enable == !!(rx_reg & I40E_QRX_ENA_QENA_STAT_MASK))
		return;

	 
	if (enable)
		rx_reg |= I40E_QRX_ENA_QENA_REQ_MASK;
	else
		rx_reg &= ~I40E_QRX_ENA_QENA_REQ_MASK;

	wr32(hw, I40E_QRX_ENA(pf_q), rx_reg);
}

 
int i40e_control_wait_rx_q(struct i40e_pf *pf, int pf_q, bool enable)
{
	int ret = 0;

	i40e_control_rx_q(pf, pf_q, enable);

	 
	ret = i40e_pf_rxq_wait(pf, pf_q, enable);
	if (ret)
		return ret;

	return ret;
}

 
static int i40e_vsi_enable_rx(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int i, pf_q, ret = 0;

	pf_q = vsi->base_queue;
	for (i = 0; i < vsi->num_queue_pairs; i++, pf_q++) {
		ret = i40e_control_wait_rx_q(pf, pf_q, true);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "VSI seid %d Rx ring %d enable timeout\n",
				 vsi->seid, pf_q);
			break;
		}
	}

	return ret;
}

 
int i40e_vsi_start_rings(struct i40e_vsi *vsi)
{
	int ret = 0;

	 
	ret = i40e_vsi_enable_rx(vsi);
	if (ret)
		return ret;
	ret = i40e_vsi_enable_tx(vsi);

	return ret;
}

#define I40E_DISABLE_TX_GAP_MSEC	50

 
void i40e_vsi_stop_rings(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int pf_q, err, q_end;

	 
	if (test_bit(__I40E_PORT_SUSPENDED, vsi->back->state))
		return i40e_vsi_stop_rings_no_wait(vsi);

	q_end = vsi->base_queue + vsi->num_queue_pairs;
	for (pf_q = vsi->base_queue; pf_q < q_end; pf_q++)
		i40e_pre_tx_queue_cfg(&pf->hw, (u32)pf_q, false);

	for (pf_q = vsi->base_queue; pf_q < q_end; pf_q++) {
		err = i40e_control_wait_rx_q(pf, pf_q, false);
		if (err)
			dev_info(&pf->pdev->dev,
				 "VSI seid %d Rx ring %d disable timeout\n",
				 vsi->seid, pf_q);
	}

	msleep(I40E_DISABLE_TX_GAP_MSEC);
	pf_q = vsi->base_queue;
	for (pf_q = vsi->base_queue; pf_q < q_end; pf_q++)
		wr32(&pf->hw, I40E_QTX_ENA(pf_q), 0);

	i40e_vsi_wait_queues_disabled(vsi);
}

 
void i40e_vsi_stop_rings_no_wait(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int i, pf_q;

	pf_q = vsi->base_queue;
	for (i = 0; i < vsi->num_queue_pairs; i++, pf_q++) {
		i40e_control_tx_q(pf, pf_q, false);
		i40e_control_rx_q(pf, pf_q, false);
	}
}

 
static void i40e_vsi_free_irq(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	int base = vsi->base_vector;
	u32 val, qp;
	int i;

	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		if (!vsi->q_vectors)
			return;

		if (!vsi->irqs_ready)
			return;

		vsi->irqs_ready = false;
		for (i = 0; i < vsi->num_q_vectors; i++) {
			int irq_num;
			u16 vector;

			vector = i + base;
			irq_num = pf->msix_entries[vector].vector;

			 
			if (!vsi->q_vectors[i] ||
			    !vsi->q_vectors[i]->num_ringpairs)
				continue;

			 
			irq_set_affinity_notifier(irq_num, NULL);
			 
			irq_update_affinity_hint(irq_num, NULL);
			free_irq(irq_num, vsi->q_vectors[i]);

			 
			val = rd32(hw, I40E_PFINT_LNKLSTN(vector - 1));
			qp = (val & I40E_PFINT_LNKLSTN_FIRSTQ_INDX_MASK)
				>> I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT;
			val |= I40E_QUEUE_END_OF_LIST
				<< I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT;
			wr32(hw, I40E_PFINT_LNKLSTN(vector - 1), val);

			while (qp != I40E_QUEUE_END_OF_LIST) {
				u32 next;

				val = rd32(hw, I40E_QINT_RQCTL(qp));

				val &= ~(I40E_QINT_RQCTL_MSIX_INDX_MASK  |
					 I40E_QINT_RQCTL_MSIX0_INDX_MASK |
					 I40E_QINT_RQCTL_CAUSE_ENA_MASK  |
					 I40E_QINT_RQCTL_INTEVENT_MASK);

				val |= (I40E_QINT_RQCTL_ITR_INDX_MASK |
					 I40E_QINT_RQCTL_NEXTQ_INDX_MASK);

				wr32(hw, I40E_QINT_RQCTL(qp), val);

				val = rd32(hw, I40E_QINT_TQCTL(qp));

				next = (val & I40E_QINT_TQCTL_NEXTQ_INDX_MASK)
					>> I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT;

				val &= ~(I40E_QINT_TQCTL_MSIX_INDX_MASK  |
					 I40E_QINT_TQCTL_MSIX0_INDX_MASK |
					 I40E_QINT_TQCTL_CAUSE_ENA_MASK  |
					 I40E_QINT_TQCTL_INTEVENT_MASK);

				val |= (I40E_QINT_TQCTL_ITR_INDX_MASK |
					 I40E_QINT_TQCTL_NEXTQ_INDX_MASK);

				wr32(hw, I40E_QINT_TQCTL(qp), val);
				qp = next;
			}
		}
	} else {
		free_irq(pf->pdev->irq, pf);

		val = rd32(hw, I40E_PFINT_LNKLST0);
		qp = (val & I40E_PFINT_LNKLSTN_FIRSTQ_INDX_MASK)
			>> I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT;
		val |= I40E_QUEUE_END_OF_LIST
			<< I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT;
		wr32(hw, I40E_PFINT_LNKLST0, val);

		val = rd32(hw, I40E_QINT_RQCTL(qp));
		val &= ~(I40E_QINT_RQCTL_MSIX_INDX_MASK  |
			 I40E_QINT_RQCTL_MSIX0_INDX_MASK |
			 I40E_QINT_RQCTL_CAUSE_ENA_MASK  |
			 I40E_QINT_RQCTL_INTEVENT_MASK);

		val |= (I40E_QINT_RQCTL_ITR_INDX_MASK |
			I40E_QINT_RQCTL_NEXTQ_INDX_MASK);

		wr32(hw, I40E_QINT_RQCTL(qp), val);

		val = rd32(hw, I40E_QINT_TQCTL(qp));

		val &= ~(I40E_QINT_TQCTL_MSIX_INDX_MASK  |
			 I40E_QINT_TQCTL_MSIX0_INDX_MASK |
			 I40E_QINT_TQCTL_CAUSE_ENA_MASK  |
			 I40E_QINT_TQCTL_INTEVENT_MASK);

		val |= (I40E_QINT_TQCTL_ITR_INDX_MASK |
			I40E_QINT_TQCTL_NEXTQ_INDX_MASK);

		wr32(hw, I40E_QINT_TQCTL(qp), val);
	}
}

 
static void i40e_free_q_vector(struct i40e_vsi *vsi, int v_idx)
{
	struct i40e_q_vector *q_vector = vsi->q_vectors[v_idx];
	struct i40e_ring *ring;

	if (!q_vector)
		return;

	 
	i40e_for_each_ring(ring, q_vector->tx)
		ring->q_vector = NULL;

	i40e_for_each_ring(ring, q_vector->rx)
		ring->q_vector = NULL;

	 
	if (vsi->netdev)
		netif_napi_del(&q_vector->napi);

	vsi->q_vectors[v_idx] = NULL;

	kfree_rcu(q_vector, rcu);
}

 
static void i40e_vsi_free_q_vectors(struct i40e_vsi *vsi)
{
	int v_idx;

	for (v_idx = 0; v_idx < vsi->num_q_vectors; v_idx++)
		i40e_free_q_vector(vsi, v_idx);
}

 
static void i40e_reset_interrupt_capability(struct i40e_pf *pf)
{
	 
	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		pci_disable_msix(pf->pdev);
		kfree(pf->msix_entries);
		pf->msix_entries = NULL;
		kfree(pf->irq_pile);
		pf->irq_pile = NULL;
	} else if (pf->flags & I40E_FLAG_MSI_ENABLED) {
		pci_disable_msi(pf->pdev);
	}
	pf->flags &= ~(I40E_FLAG_MSIX_ENABLED | I40E_FLAG_MSI_ENABLED);
}

 
static void i40e_clear_interrupt_scheme(struct i40e_pf *pf)
{
	int i;

	if (test_bit(__I40E_MISC_IRQ_REQUESTED, pf->state))
		i40e_free_misc_vector(pf);

	i40e_put_lump(pf->irq_pile, pf->iwarp_base_vector,
		      I40E_IWARP_IRQ_PILE_ID);

	i40e_put_lump(pf->irq_pile, 0, I40E_PILE_VALID_BIT-1);
	for (i = 0; i < pf->num_alloc_vsi; i++)
		if (pf->vsi[i])
			i40e_vsi_free_q_vectors(pf->vsi[i]);
	i40e_reset_interrupt_capability(pf);
}

 
static void i40e_napi_enable_all(struct i40e_vsi *vsi)
{
	int q_idx;

	if (!vsi->netdev)
		return;

	for (q_idx = 0; q_idx < vsi->num_q_vectors; q_idx++) {
		struct i40e_q_vector *q_vector = vsi->q_vectors[q_idx];

		if (q_vector->rx.ring || q_vector->tx.ring)
			napi_enable(&q_vector->napi);
	}
}

 
static void i40e_napi_disable_all(struct i40e_vsi *vsi)
{
	int q_idx;

	if (!vsi->netdev)
		return;

	for (q_idx = 0; q_idx < vsi->num_q_vectors; q_idx++) {
		struct i40e_q_vector *q_vector = vsi->q_vectors[q_idx];

		if (q_vector->rx.ring || q_vector->tx.ring)
			napi_disable(&q_vector->napi);
	}
}

 
static void i40e_vsi_close(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	if (!test_and_set_bit(__I40E_VSI_DOWN, vsi->state))
		i40e_down(vsi);
	i40e_vsi_free_irq(vsi);
	i40e_vsi_free_tx_resources(vsi);
	i40e_vsi_free_rx_resources(vsi);
	vsi->current_netdev_flags = 0;
	set_bit(__I40E_CLIENT_SERVICE_REQUESTED, pf->state);
	if (test_bit(__I40E_RESET_RECOVERY_PENDING, pf->state))
		set_bit(__I40E_CLIENT_RESET, pf->state);
}

 
static void i40e_quiesce_vsi(struct i40e_vsi *vsi)
{
	if (test_bit(__I40E_VSI_DOWN, vsi->state))
		return;

	set_bit(__I40E_VSI_NEEDS_RESTART, vsi->state);
	if (vsi->netdev && netif_running(vsi->netdev))
		vsi->netdev->netdev_ops->ndo_stop(vsi->netdev);
	else
		i40e_vsi_close(vsi);
}

 
static void i40e_unquiesce_vsi(struct i40e_vsi *vsi)
{
	if (!test_and_clear_bit(__I40E_VSI_NEEDS_RESTART, vsi->state))
		return;

	if (vsi->netdev && netif_running(vsi->netdev))
		vsi->netdev->netdev_ops->ndo_open(vsi->netdev);
	else
		i40e_vsi_open(vsi);    
}

 
static void i40e_pf_quiesce_all_vsi(struct i40e_pf *pf)
{
	int v;

	for (v = 0; v < pf->num_alloc_vsi; v++) {
		if (pf->vsi[v])
			i40e_quiesce_vsi(pf->vsi[v]);
	}
}

 
static void i40e_pf_unquiesce_all_vsi(struct i40e_pf *pf)
{
	int v;

	for (v = 0; v < pf->num_alloc_vsi; v++) {
		if (pf->vsi[v])
			i40e_unquiesce_vsi(pf->vsi[v]);
	}
}

 
int i40e_vsi_wait_queues_disabled(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int i, pf_q, ret;

	pf_q = vsi->base_queue;
	for (i = 0; i < vsi->num_queue_pairs; i++, pf_q++) {
		 
		ret = i40e_pf_txq_wait(pf, pf_q, false);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "VSI seid %d Tx ring %d disable timeout\n",
				 vsi->seid, pf_q);
			return ret;
		}

		if (!i40e_enabled_xdp_vsi(vsi))
			goto wait_rx;

		 
		ret = i40e_pf_txq_wait(pf, pf_q + vsi->alloc_queue_pairs,
				       false);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "VSI seid %d XDP Tx ring %d disable timeout\n",
				 vsi->seid, pf_q);
			return ret;
		}
wait_rx:
		 
		ret = i40e_pf_rxq_wait(pf, pf_q, false);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "VSI seid %d Rx ring %d disable timeout\n",
				 vsi->seid, pf_q);
			return ret;
		}
	}

	return 0;
}

#ifdef CONFIG_I40E_DCB
 
static int i40e_pf_wait_queues_disabled(struct i40e_pf *pf)
{
	int v, ret = 0;

	for (v = 0; v < pf->hw.func_caps.num_vsis; v++) {
		if (pf->vsi[v]) {
			ret = i40e_vsi_wait_queues_disabled(pf->vsi[v]);
			if (ret)
				break;
		}
	}

	return ret;
}

#endif

 
static u8 i40e_get_iscsi_tc_map(struct i40e_pf *pf)
{
	struct i40e_dcb_app_priority_table app;
	struct i40e_hw *hw = &pf->hw;
	u8 enabled_tc = 1;  
	u8 tc, i;
	 
	struct i40e_dcbx_config *dcbcfg = &hw->local_dcbx_config;

	for (i = 0; i < dcbcfg->numapps; i++) {
		app = dcbcfg->app[i];
		if (app.selector == I40E_APP_SEL_TCPIP &&
		    app.protocolid == I40E_APP_PROTOID_ISCSI) {
			tc = dcbcfg->etscfg.prioritytable[app.priority];
			enabled_tc |= BIT(tc);
			break;
		}
	}

	return enabled_tc;
}

 
static u8 i40e_dcb_get_num_tc(struct i40e_dcbx_config *dcbcfg)
{
	int i, tc_unused = 0;
	u8 num_tc = 0;
	u8 ret = 0;

	 
	for (i = 0; i < I40E_MAX_USER_PRIORITY; i++)
		num_tc |= BIT(dcbcfg->etscfg.prioritytable[i]);

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (num_tc & BIT(i)) {
			if (!tc_unused) {
				ret++;
			} else {
				pr_err("Non-contiguous TC - Disabling DCB\n");
				return 1;
			}
		} else {
			tc_unused = 1;
		}
	}

	 
	if (!ret)
		ret = 1;

	return ret;
}

 
static u8 i40e_dcb_get_enabled_tc(struct i40e_dcbx_config *dcbcfg)
{
	u8 num_tc = i40e_dcb_get_num_tc(dcbcfg);
	u8 enabled_tc = 1;
	u8 i;

	for (i = 0; i < num_tc; i++)
		enabled_tc |= BIT(i);

	return enabled_tc;
}

 
static u8 i40e_mqprio_get_enabled_tc(struct i40e_pf *pf)
{
	struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
	u8 num_tc = vsi->mqprio_qopt.qopt.num_tc;
	u8 enabled_tc = 1, i;

	for (i = 1; i < num_tc; i++)
		enabled_tc |= BIT(i);
	return enabled_tc;
}

 
static u8 i40e_pf_get_num_tc(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u8 i, enabled_tc = 1;
	u8 num_tc = 0;
	struct i40e_dcbx_config *dcbcfg = &hw->local_dcbx_config;

	if (i40e_is_tc_mqprio_enabled(pf))
		return pf->vsi[pf->lan_vsi]->mqprio_qopt.qopt.num_tc;

	 
	if (!(pf->flags & I40E_FLAG_DCB_ENABLED))
		return 1;

	 
	if (!(pf->flags & I40E_FLAG_MFP_ENABLED))
		return i40e_dcb_get_num_tc(dcbcfg);

	 
	if (pf->hw.func_caps.iscsi)
		enabled_tc =  i40e_get_iscsi_tc_map(pf);
	else
		return 1;  

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (enabled_tc & BIT(i))
			num_tc++;
	}
	return num_tc;
}

 
static u8 i40e_pf_get_tc_map(struct i40e_pf *pf)
{
	if (i40e_is_tc_mqprio_enabled(pf))
		return i40e_mqprio_get_enabled_tc(pf);

	 
	if (!(pf->flags & I40E_FLAG_DCB_ENABLED))
		return I40E_DEFAULT_TRAFFIC_CLASS;

	 
	if (!(pf->flags & I40E_FLAG_MFP_ENABLED))
		return i40e_dcb_get_enabled_tc(&pf->hw.local_dcbx_config);

	 
	if (pf->hw.func_caps.iscsi)
		return i40e_get_iscsi_tc_map(pf);
	else
		return I40E_DEFAULT_TRAFFIC_CLASS;
}

 
static int i40e_vsi_get_bw_info(struct i40e_vsi *vsi)
{
	struct i40e_aqc_query_vsi_ets_sla_config_resp bw_ets_config = {0};
	struct i40e_aqc_query_vsi_bw_config_resp bw_config = {0};
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	u32 tc_bw_max;
	int ret;
	int i;

	 
	ret = i40e_aq_query_vsi_bw_config(hw, vsi->seid, &bw_config, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't get PF vsi bw config, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		return -EINVAL;
	}

	 
	ret = i40e_aq_query_vsi_ets_sla_config(hw, vsi->seid, &bw_ets_config,
					       NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't get PF vsi ets bw config, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		return -EINVAL;
	}

	if (bw_config.tc_valid_bits != bw_ets_config.tc_valid_bits) {
		dev_info(&pf->pdev->dev,
			 "Enabled TCs mismatch from querying VSI BW info 0x%08x 0x%08x\n",
			 bw_config.tc_valid_bits,
			 bw_ets_config.tc_valid_bits);
		 
	}

	vsi->bw_limit = le16_to_cpu(bw_config.port_bw_limit);
	vsi->bw_max_quanta = bw_config.max_bw;
	tc_bw_max = le16_to_cpu(bw_ets_config.tc_bw_max[0]) |
		    (le16_to_cpu(bw_ets_config.tc_bw_max[1]) << 16);
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		vsi->bw_ets_share_credits[i] = bw_ets_config.share_credits[i];
		vsi->bw_ets_limit_credits[i] =
					le16_to_cpu(bw_ets_config.credits[i]);
		 
		vsi->bw_ets_max_quanta[i] = (u8)((tc_bw_max >> (i*4)) & 0x7);
	}

	return 0;
}

 
static int i40e_vsi_configure_bw_alloc(struct i40e_vsi *vsi, u8 enabled_tc,
				       u8 *bw_share)
{
	struct i40e_aqc_configure_vsi_tc_bw_data bw_data;
	struct i40e_pf *pf = vsi->back;
	int ret;
	int i;

	 
	if (i40e_is_tc_mqprio_enabled(pf))
		return 0;
	if (!vsi->mqprio_qopt.qopt.hw && !(pf->flags & I40E_FLAG_DCB_ENABLED)) {
		ret = i40e_set_bw_limit(vsi, vsi->seid, 0);
		if (ret)
			dev_info(&pf->pdev->dev,
				 "Failed to reset tx rate for vsi->seid %u\n",
				 vsi->seid);
		return ret;
	}
	memset(&bw_data, 0, sizeof(bw_data));
	bw_data.tc_valid_bits = enabled_tc;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		bw_data.tc_bw_credits[i] = bw_share[i];

	ret = i40e_aq_config_vsi_tc_bw(&pf->hw, vsi->seid, &bw_data, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "AQ command Config VSI BW allocation per TC failed = %d\n",
			 pf->hw.aq.asq_last_status);
		return -EINVAL;
	}

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		vsi->info.qs_handle[i] = bw_data.qs_handles[i];

	return 0;
}

 
static void i40e_vsi_config_netdev_tc(struct i40e_vsi *vsi, u8 enabled_tc)
{
	struct net_device *netdev = vsi->netdev;
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	u8 netdev_tc = 0;
	int i;
	struct i40e_dcbx_config *dcbcfg = &hw->local_dcbx_config;

	if (!netdev)
		return;

	if (!enabled_tc) {
		netdev_reset_tc(netdev);
		return;
	}

	 
	if (netdev_set_num_tc(netdev, vsi->tc_config.numtc))
		return;

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		 
		if (vsi->tc_config.enabled_tc & BIT(i))
			netdev_set_tc_queue(netdev,
					vsi->tc_config.tc_info[i].netdev_tc,
					vsi->tc_config.tc_info[i].qcount,
					vsi->tc_config.tc_info[i].qoffset);
	}

	if (i40e_is_tc_mqprio_enabled(pf))
		return;

	 
	for (i = 0; i < I40E_MAX_USER_PRIORITY; i++) {
		 
		u8 ets_tc = dcbcfg->etscfg.prioritytable[i];
		 
		netdev_tc =  vsi->tc_config.tc_info[ets_tc].netdev_tc;
		netdev_set_prio_tc_map(netdev, i, netdev_tc);
	}
}

 
static void i40e_vsi_update_queue_map(struct i40e_vsi *vsi,
				      struct i40e_vsi_context *ctxt)
{
	 
	vsi->info.mapping_flags = ctxt->info.mapping_flags;
	memcpy(&vsi->info.queue_mapping,
	       &ctxt->info.queue_mapping, sizeof(vsi->info.queue_mapping));
	memcpy(&vsi->info.tc_mapping, ctxt->info.tc_mapping,
	       sizeof(vsi->info.tc_mapping));
}

 
int i40e_update_adq_vsi_queues(struct i40e_vsi *vsi, int vsi_offset)
{
	struct i40e_vsi_context ctxt = {};
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	int ret;

	if (!vsi)
		return -EINVAL;
	pf = vsi->back;
	hw = &pf->hw;

	ctxt.seid = vsi->seid;
	ctxt.pf_num = hw->pf_id;
	ctxt.vf_num = vsi->vf_id + hw->func_caps.vf_base_id + vsi_offset;
	ctxt.uplink_seid = vsi->uplink_seid;
	ctxt.connection_type = I40E_AQ_VSI_CONN_TYPE_NORMAL;
	ctxt.flags = I40E_AQ_VSI_TYPE_VF;
	ctxt.info = vsi->info;

	i40e_vsi_setup_queue_map(vsi, &ctxt, vsi->tc_config.enabled_tc,
				 false);
	if (vsi->reconfig_rss) {
		vsi->rss_size = min_t(int, pf->alloc_rss_size,
				      vsi->num_queue_pairs);
		ret = i40e_vsi_config_rss(vsi);
		if (ret) {
			dev_info(&pf->pdev->dev, "Failed to reconfig rss for num_queues\n");
			return ret;
		}
		vsi->reconfig_rss = false;
	}

	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev, "Update vsi config failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(hw, hw->aq.asq_last_status));
		return ret;
	}
	 
	i40e_vsi_update_queue_map(vsi, &ctxt);
	vsi->info.valid_sections = 0;

	return ret;
}

 
static int i40e_vsi_config_tc(struct i40e_vsi *vsi, u8 enabled_tc)
{
	u8 bw_share[I40E_MAX_TRAFFIC_CLASS] = {0};
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vsi_context ctxt;
	int ret = 0;
	int i;

	 
	if (vsi->tc_config.enabled_tc == enabled_tc &&
	    vsi->mqprio_qopt.mode != TC_MQPRIO_MODE_CHANNEL)
		return ret;

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (enabled_tc & BIT(i))
			bw_share[i] = 1;
	}

	ret = i40e_vsi_configure_bw_alloc(vsi, enabled_tc, bw_share);
	if (ret) {
		struct i40e_aqc_query_vsi_bw_config_resp bw_config = {0};

		dev_info(&pf->pdev->dev,
			 "Failed configuring TC map %d for VSI %d\n",
			 enabled_tc, vsi->seid);
		ret = i40e_aq_query_vsi_bw_config(hw, vsi->seid,
						  &bw_config, NULL);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "Failed querying vsi bw info, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(hw, hw->aq.asq_last_status));
			goto out;
		}
		if ((bw_config.tc_valid_bits & enabled_tc) != enabled_tc) {
			u8 valid_tc = bw_config.tc_valid_bits & enabled_tc;

			if (!valid_tc)
				valid_tc = bw_config.tc_valid_bits;
			 
			valid_tc |= 1;
			dev_info(&pf->pdev->dev,
				 "Requested tc 0x%x, but FW reports 0x%x as valid. Attempting to use 0x%x.\n",
				 enabled_tc, bw_config.tc_valid_bits, valid_tc);
			enabled_tc = valid_tc;
		}

		ret = i40e_vsi_configure_bw_alloc(vsi, enabled_tc, bw_share);
		if (ret) {
			dev_err(&pf->pdev->dev,
				"Unable to  configure TC map %d for VSI %d\n",
				enabled_tc, vsi->seid);
			goto out;
		}
	}

	 
	ctxt.seid = vsi->seid;
	ctxt.pf_num = vsi->back->hw.pf_id;
	ctxt.vf_num = 0;
	ctxt.uplink_seid = vsi->uplink_seid;
	ctxt.info = vsi->info;
	if (i40e_is_tc_mqprio_enabled(pf)) {
		ret = i40e_vsi_setup_queue_map_mqprio(vsi, &ctxt, enabled_tc);
		if (ret)
			goto out;
	} else {
		i40e_vsi_setup_queue_map(vsi, &ctxt, enabled_tc, false);
	}

	 
	if (!vsi->mqprio_qopt.qopt.hw && vsi->reconfig_rss) {
		vsi->rss_size = min_t(int, vsi->back->alloc_rss_size,
				      vsi->num_queue_pairs);
		ret = i40e_vsi_config_rss(vsi);
		if (ret) {
			dev_info(&vsi->back->pdev->dev,
				 "Failed to reconfig rss for num_queues\n");
			return ret;
		}
		vsi->reconfig_rss = false;
	}
	if (vsi->back->flags & I40E_FLAG_IWARP_ENABLED) {
		ctxt.info.valid_sections |=
				cpu_to_le16(I40E_AQ_VSI_PROP_QUEUE_OPT_VALID);
		ctxt.info.queueing_opt_flags |= I40E_AQ_VSI_QUE_OPT_TCP_ENA;
	}

	 
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Update vsi tc config failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(hw, hw->aq.asq_last_status));
		goto out;
	}
	 
	i40e_vsi_update_queue_map(vsi, &ctxt);
	vsi->info.valid_sections = 0;

	 
	ret = i40e_vsi_get_bw_info(vsi);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Failed updating vsi bw info, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(hw, hw->aq.asq_last_status));
		goto out;
	}

	 
	i40e_vsi_config_netdev_tc(vsi, enabled_tc);
out:
	return ret;
}

 
static int i40e_get_link_speed(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;

	switch (pf->hw.phy.link_info.link_speed) {
	case I40E_LINK_SPEED_40GB:
		return 40000;
	case I40E_LINK_SPEED_25GB:
		return 25000;
	case I40E_LINK_SPEED_20GB:
		return 20000;
	case I40E_LINK_SPEED_10GB:
		return 10000;
	case I40E_LINK_SPEED_1GB:
		return 1000;
	default:
		return -EINVAL;
	}
}

 
static u64 i40e_bw_bytes_to_mbits(struct i40e_vsi *vsi, u64 max_tx_rate)
{
	if (max_tx_rate < I40E_BW_MBPS_DIVISOR) {
		dev_warn(&vsi->back->pdev->dev,
			 "Setting max tx rate to minimum usable value of 50Mbps.\n");
		max_tx_rate = I40E_BW_CREDIT_DIVISOR;
	} else {
		do_div(max_tx_rate, I40E_BW_MBPS_DIVISOR);
	}

	return max_tx_rate;
}

 
int i40e_set_bw_limit(struct i40e_vsi *vsi, u16 seid, u64 max_tx_rate)
{
	struct i40e_pf *pf = vsi->back;
	u64 credits = 0;
	int speed = 0;
	int ret = 0;

	speed = i40e_get_link_speed(vsi);
	if (max_tx_rate > speed) {
		dev_err(&pf->pdev->dev,
			"Invalid max tx rate %llu specified for VSI seid %d.",
			max_tx_rate, seid);
		return -EINVAL;
	}
	if (max_tx_rate && max_tx_rate < I40E_BW_CREDIT_DIVISOR) {
		dev_warn(&pf->pdev->dev,
			 "Setting max tx rate to minimum usable value of 50Mbps.\n");
		max_tx_rate = I40E_BW_CREDIT_DIVISOR;
	}

	 
	credits = max_tx_rate;
	do_div(credits, I40E_BW_CREDIT_DIVISOR);
	ret = i40e_aq_config_vsi_bw_limit(&pf->hw, seid, credits,
					  I40E_MAX_BW_INACTIVE_ACCUM, NULL);
	if (ret)
		dev_err(&pf->pdev->dev,
			"Failed set tx rate (%llu Mbps) for vsi->seid %u, err %pe aq_err %s\n",
			max_tx_rate, seid, ERR_PTR(ret),
			i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
	return ret;
}

 
static void i40e_remove_queue_channels(struct i40e_vsi *vsi)
{
	enum i40e_admin_queue_err last_aq_status;
	struct i40e_cloud_filter *cfilter;
	struct i40e_channel *ch, *ch_tmp;
	struct i40e_pf *pf = vsi->back;
	struct hlist_node *node;
	int ret, i;

	 
	vsi->current_rss_size = 0;

	 
	if (list_empty(&vsi->ch_list))
		return;

	list_for_each_entry_safe(ch, ch_tmp, &vsi->ch_list, list) {
		struct i40e_vsi *p_vsi;

		list_del(&ch->list);
		p_vsi = ch->parent_vsi;
		if (!p_vsi || !ch->initialized) {
			kfree(ch);
			continue;
		}
		 
		for (i = 0; i < ch->num_queue_pairs; i++) {
			struct i40e_ring *tx_ring, *rx_ring;
			u16 pf_q;

			pf_q = ch->base_queue + i;
			tx_ring = vsi->tx_rings[pf_q];
			tx_ring->ch = NULL;

			rx_ring = vsi->rx_rings[pf_q];
			rx_ring->ch = NULL;
		}

		 
		ret = i40e_set_bw_limit(vsi, ch->seid, 0);
		if (ret)
			dev_info(&vsi->back->pdev->dev,
				 "Failed to reset tx rate for ch->seid %u\n",
				 ch->seid);

		 
		hlist_for_each_entry_safe(cfilter, node,
					  &pf->cloud_filter_list, cloud_node) {
			if (cfilter->seid != ch->seid)
				continue;

			hash_del(&cfilter->cloud_node);
			if (cfilter->dst_port)
				ret = i40e_add_del_cloud_filter_big_buf(vsi,
									cfilter,
									false);
			else
				ret = i40e_add_del_cloud_filter(vsi, cfilter,
								false);
			last_aq_status = pf->hw.aq.asq_last_status;
			if (ret)
				dev_info(&pf->pdev->dev,
					 "Failed to delete cloud filter, err %pe aq_err %s\n",
					 ERR_PTR(ret),
					 i40e_aq_str(&pf->hw, last_aq_status));
			kfree(cfilter);
		}

		 
		ret = i40e_aq_delete_element(&vsi->back->hw, ch->seid,
					     NULL);
		if (ret)
			dev_err(&vsi->back->pdev->dev,
				"unable to remove channel (%d) for parent VSI(%d)\n",
				ch->seid, p_vsi->seid);
		kfree(ch);
	}
	INIT_LIST_HEAD(&vsi->ch_list);
}

 
static int i40e_get_max_queues_for_channel(struct i40e_vsi *vsi)
{
	struct i40e_channel *ch, *ch_tmp;
	int max = 0;

	list_for_each_entry_safe(ch, ch_tmp, &vsi->ch_list, list) {
		if (!ch->initialized)
			continue;
		if (ch->num_queue_pairs > max)
			max = ch->num_queue_pairs;
	}

	return max;
}

 
static int i40e_validate_num_queues(struct i40e_pf *pf, int num_queues,
				    struct i40e_vsi *vsi, bool *reconfig_rss)
{
	int max_ch_queues;

	if (!reconfig_rss)
		return -EINVAL;

	*reconfig_rss = false;
	if (vsi->current_rss_size) {
		if (num_queues > vsi->current_rss_size) {
			dev_dbg(&pf->pdev->dev,
				"Error: num_queues (%d) > vsi's current_size(%d)\n",
				num_queues, vsi->current_rss_size);
			return -EINVAL;
		} else if ((num_queues < vsi->current_rss_size) &&
			   (!is_power_of_2(num_queues))) {
			dev_dbg(&pf->pdev->dev,
				"Error: num_queues (%d) < vsi's current_size(%d), but not power of 2\n",
				num_queues, vsi->current_rss_size);
			return -EINVAL;
		}
	}

	if (!is_power_of_2(num_queues)) {
		 
		max_ch_queues = i40e_get_max_queues_for_channel(vsi);
		if (num_queues < max_ch_queues) {
			dev_dbg(&pf->pdev->dev,
				"Error: num_queues (%d) < max queues configured for channel(%d)\n",
				num_queues, max_ch_queues);
			return -EINVAL;
		}
		*reconfig_rss = true;
	}

	return 0;
}

 
static int i40e_vsi_reconfig_rss(struct i40e_vsi *vsi, u16 rss_size)
{
	struct i40e_pf *pf = vsi->back;
	u8 seed[I40E_HKEY_ARRAY_SIZE];
	struct i40e_hw *hw = &pf->hw;
	int local_rss_size;
	u8 *lut;
	int ret;

	if (!vsi->rss_size)
		return -EINVAL;

	if (rss_size > vsi->rss_size)
		return -EINVAL;

	local_rss_size = min_t(int, vsi->rss_size, rss_size);
	lut = kzalloc(vsi->rss_table_size, GFP_KERNEL);
	if (!lut)
		return -ENOMEM;

	 
	i40e_fill_rss_lut(pf, lut, vsi->rss_table_size, local_rss_size);

	 
	if (vsi->rss_hkey_user)
		memcpy(seed, vsi->rss_hkey_user, I40E_HKEY_ARRAY_SIZE);
	else
		netdev_rss_key_fill((void *)seed, I40E_HKEY_ARRAY_SIZE);

	ret = i40e_config_rss(vsi, seed, lut, vsi->rss_table_size);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Cannot set RSS lut, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(hw, hw->aq.asq_last_status));
		kfree(lut);
		return ret;
	}
	kfree(lut);

	 
	if (!vsi->orig_rss_size)
		vsi->orig_rss_size = vsi->rss_size;
	vsi->current_rss_size = local_rss_size;

	return ret;
}

 
static void i40e_channel_setup_queue_map(struct i40e_pf *pf,
					 struct i40e_vsi_context *ctxt,
					 struct i40e_channel *ch)
{
	u16 qcount, qmap, sections = 0;
	u8 offset = 0;
	int pow;

	sections = I40E_AQ_VSI_PROP_QUEUE_MAP_VALID;
	sections |= I40E_AQ_VSI_PROP_SCHED_VALID;

	qcount = min_t(int, ch->num_queue_pairs, pf->num_lan_msix);
	ch->num_queue_pairs = qcount;

	 
	pow = ilog2(qcount);
	if (!is_power_of_2(qcount))
		pow++;

	qmap = (offset << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
		(pow << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT);

	 
	ctxt->info.tc_mapping[0] = cpu_to_le16(qmap);

	ctxt->info.up_enable_bits = 0x1;  
	ctxt->info.mapping_flags |= cpu_to_le16(I40E_AQ_VSI_QUE_MAP_CONTIG);
	ctxt->info.queue_mapping[0] = cpu_to_le16(ch->base_queue);
	ctxt->info.valid_sections |= cpu_to_le16(sections);
}

 
static int i40e_add_channel(struct i40e_pf *pf, u16 uplink_seid,
			    struct i40e_channel *ch)
{
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vsi_context ctxt;
	u8 enabled_tc = 0x1;  
	int ret;

	if (ch->type != I40E_VSI_VMDQ2) {
		dev_info(&pf->pdev->dev,
			 "add new vsi failed, ch->type %d\n", ch->type);
		return -EINVAL;
	}

	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.pf_num = hw->pf_id;
	ctxt.vf_num = 0;
	ctxt.uplink_seid = uplink_seid;
	ctxt.connection_type = I40E_AQ_VSI_CONN_TYPE_NORMAL;
	if (ch->type == I40E_VSI_VMDQ2)
		ctxt.flags = I40E_AQ_VSI_TYPE_VMDQ2;

	if (pf->flags & I40E_FLAG_VEB_MODE_ENABLED) {
		ctxt.info.valid_sections |=
		     cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
		ctxt.info.switch_id =
		   cpu_to_le16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
	}

	 
	i40e_channel_setup_queue_map(pf, &ctxt, ch);

	 
	ret = i40e_aq_add_vsi(hw, &ctxt, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "add new vsi failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw,
				     pf->hw.aq.asq_last_status));
		return -ENOENT;
	}

	 
	ch->enabled_tc = !i40e_is_channel_macvlan(ch) && enabled_tc;
	ch->seid = ctxt.seid;
	ch->vsi_number = ctxt.vsi_number;
	ch->stat_counter_idx = le16_to_cpu(ctxt.info.stat_counter_idx);

	 
	ch->info.mapping_flags = ctxt.info.mapping_flags;
	memcpy(&ch->info.queue_mapping,
	       &ctxt.info.queue_mapping, sizeof(ctxt.info.queue_mapping));
	memcpy(&ch->info.tc_mapping, ctxt.info.tc_mapping,
	       sizeof(ctxt.info.tc_mapping));

	return 0;
}

static int i40e_channel_config_bw(struct i40e_vsi *vsi, struct i40e_channel *ch,
				  u8 *bw_share)
{
	struct i40e_aqc_configure_vsi_tc_bw_data bw_data;
	int ret;
	int i;

	memset(&bw_data, 0, sizeof(bw_data));
	bw_data.tc_valid_bits = ch->enabled_tc;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		bw_data.tc_bw_credits[i] = bw_share[i];

	ret = i40e_aq_config_vsi_tc_bw(&vsi->back->hw, ch->seid,
				       &bw_data, NULL);
	if (ret) {
		dev_info(&vsi->back->pdev->dev,
			 "Config VSI BW allocation per TC failed, aq_err: %d for new_vsi->seid %u\n",
			 vsi->back->hw.aq.asq_last_status, ch->seid);
		return -EINVAL;
	}

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		ch->info.qs_handle[i] = bw_data.qs_handles[i];

	return 0;
}

 
static int i40e_channel_config_tx_ring(struct i40e_pf *pf,
				       struct i40e_vsi *vsi,
				       struct i40e_channel *ch)
{
	u8 bw_share[I40E_MAX_TRAFFIC_CLASS] = {0};
	int ret;
	int i;

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (ch->enabled_tc & BIT(i))
			bw_share[i] = 1;
	}

	 
	ret = i40e_channel_config_bw(vsi, ch, bw_share);
	if (ret) {
		dev_info(&vsi->back->pdev->dev,
			 "Failed configuring TC map %d for channel (seid %u)\n",
			 ch->enabled_tc, ch->seid);
		return ret;
	}

	for (i = 0; i < ch->num_queue_pairs; i++) {
		struct i40e_ring *tx_ring, *rx_ring;
		u16 pf_q;

		pf_q = ch->base_queue + i;

		 
		tx_ring = vsi->tx_rings[pf_q];
		tx_ring->ch = ch;

		 
		rx_ring = vsi->rx_rings[pf_q];
		rx_ring->ch = ch;
	}

	return 0;
}

 
static inline int i40e_setup_hw_channel(struct i40e_pf *pf,
					struct i40e_vsi *vsi,
					struct i40e_channel *ch,
					u16 uplink_seid, u8 type)
{
	int ret;

	ch->initialized = false;
	ch->base_queue = vsi->next_base_queue;
	ch->type = type;

	 
	ret = i40e_add_channel(pf, uplink_seid, ch);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "failed to add_channel using uplink_seid %u\n",
			 uplink_seid);
		return ret;
	}

	 
	ch->initialized = true;

	 
	ret = i40e_channel_config_tx_ring(pf, vsi, ch);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "failed to configure TX rings for channel %u\n",
			 ch->seid);
		return ret;
	}

	 
	vsi->next_base_queue = vsi->next_base_queue + ch->num_queue_pairs;
	dev_dbg(&pf->pdev->dev,
		"Added channel: vsi_seid %u, vsi_number %u, stat_counter_idx %u, num_queue_pairs %u, pf->next_base_queue %d\n",
		ch->seid, ch->vsi_number, ch->stat_counter_idx,
		ch->num_queue_pairs,
		vsi->next_base_queue);
	return ret;
}

 
static bool i40e_setup_channel(struct i40e_pf *pf, struct i40e_vsi *vsi,
			       struct i40e_channel *ch)
{
	u8 vsi_type;
	u16 seid;
	int ret;

	if (vsi->type == I40E_VSI_MAIN) {
		vsi_type = I40E_VSI_VMDQ2;
	} else {
		dev_err(&pf->pdev->dev, "unsupported parent vsi type(%d)\n",
			vsi->type);
		return false;
	}

	 
	seid = pf->vsi[pf->lan_vsi]->uplink_seid;

	 
	ret = i40e_setup_hw_channel(pf, vsi, ch, seid, vsi_type);
	if (ret) {
		dev_err(&pf->pdev->dev, "failed to setup hw_channel\n");
		return false;
	}

	return ch->initialized ? true : false;
}

 
static int i40e_validate_and_set_switch_mode(struct i40e_vsi *vsi)
{
	u8 mode;
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	int ret;

	ret = i40e_get_capabilities(pf, i40e_aqc_opc_list_dev_capabilities);
	if (ret)
		return -EINVAL;

	if (hw->dev_caps.switch_mode) {
		 
		u32 switch_mode = hw->dev_caps.switch_mode &
				  I40E_SWITCH_MODE_MASK;
		if (switch_mode >= I40E_CLOUD_FILTER_MODE1) {
			if (switch_mode == I40E_CLOUD_FILTER_MODE2)
				return 0;
			dev_err(&pf->pdev->dev,
				"Invalid switch_mode (%d), only non-tunneled mode for cloud filter is supported\n",
				hw->dev_caps.switch_mode);
			return -EINVAL;
		}
	}

	 
	mode = I40E_AQ_SET_SWITCH_BIT7_VALID;

	 
	mode |= I40E_AQ_SET_SWITCH_L4_TYPE_TCP;

	 
	mode |= I40E_AQ_SET_SWITCH_MODE_NON_TUNNEL;

	 
	ret = i40e_aq_set_switch_config(hw, pf->last_sw_conf_flags,
					pf->last_sw_conf_valid_flags,
					mode, NULL);
	if (ret && hw->aq.asq_last_status != I40E_AQ_RC_ESRCH)
		dev_err(&pf->pdev->dev,
			"couldn't set switch config bits, err %pe aq_err %s\n",
			ERR_PTR(ret),
			i40e_aq_str(hw,
				    hw->aq.asq_last_status));

	return ret;
}

 
int i40e_create_queue_channel(struct i40e_vsi *vsi,
			      struct i40e_channel *ch)
{
	struct i40e_pf *pf = vsi->back;
	bool reconfig_rss;
	int err;

	if (!ch)
		return -EINVAL;

	if (!ch->num_queue_pairs) {
		dev_err(&pf->pdev->dev, "Invalid num_queues requested: %d\n",
			ch->num_queue_pairs);
		return -EINVAL;
	}

	 
	err = i40e_validate_num_queues(pf, ch->num_queue_pairs, vsi,
				       &reconfig_rss);
	if (err) {
		dev_info(&pf->pdev->dev, "Failed to validate num_queues (%d)\n",
			 ch->num_queue_pairs);
		return -EINVAL;
	}

	 

	if (!(pf->flags & I40E_FLAG_VEB_MODE_ENABLED)) {
		pf->flags |= I40E_FLAG_VEB_MODE_ENABLED;

		if (vsi->type == I40E_VSI_MAIN) {
			if (i40e_is_tc_mqprio_enabled(pf))
				i40e_do_reset(pf, I40E_PF_RESET_FLAG, true);
			else
				i40e_do_reset_safe(pf, I40E_PF_RESET_FLAG);
		}
		 
	}

	 
	if (!vsi->cnt_q_avail || vsi->cnt_q_avail < ch->num_queue_pairs) {
		dev_dbg(&pf->pdev->dev,
			"Error: cnt_q_avail (%u) less than num_queues %d\n",
			vsi->cnt_q_avail, ch->num_queue_pairs);
		return -EINVAL;
	}

	 
	if (reconfig_rss && (vsi->type == I40E_VSI_MAIN)) {
		err = i40e_vsi_reconfig_rss(vsi, ch->num_queue_pairs);
		if (err) {
			dev_info(&pf->pdev->dev,
				 "Error: unable to reconfig rss for num_queues (%u)\n",
				 ch->num_queue_pairs);
			return -EINVAL;
		}
	}

	if (!i40e_setup_channel(pf, vsi, ch)) {
		dev_info(&pf->pdev->dev, "Failed to setup channel\n");
		return -EINVAL;
	}

	dev_info(&pf->pdev->dev,
		 "Setup channel (id:%u) utilizing num_queues %d\n",
		 ch->seid, ch->num_queue_pairs);

	 
	if (ch->max_tx_rate) {
		u64 credits = ch->max_tx_rate;

		if (i40e_set_bw_limit(vsi, ch->seid, ch->max_tx_rate))
			return -EINVAL;

		do_div(credits, I40E_BW_CREDIT_DIVISOR);
		dev_dbg(&pf->pdev->dev,
			"Set tx rate of %llu Mbps (count of 50Mbps %llu) for vsi->seid %u\n",
			ch->max_tx_rate,
			credits,
			ch->seid);
	}

	 
	ch->parent_vsi = vsi;

	 
	vsi->cnt_q_avail -= ch->num_queue_pairs;

	return 0;
}

 
static int i40e_configure_queue_channels(struct i40e_vsi *vsi)
{
	struct i40e_channel *ch;
	u64 max_rate = 0;
	int ret = 0, i;

	 
	vsi->tc_seid_map[0] = vsi->seid;
	for (i = 1; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->tc_config.enabled_tc & BIT(i)) {
			ch = kzalloc(sizeof(*ch), GFP_KERNEL);
			if (!ch) {
				ret = -ENOMEM;
				goto err_free;
			}

			INIT_LIST_HEAD(&ch->list);
			ch->num_queue_pairs =
				vsi->tc_config.tc_info[i].qcount;
			ch->base_queue =
				vsi->tc_config.tc_info[i].qoffset;

			 
			max_rate = vsi->mqprio_qopt.max_rate[i];
			do_div(max_rate, I40E_BW_MBPS_DIVISOR);
			ch->max_tx_rate = max_rate;

			list_add_tail(&ch->list, &vsi->ch_list);

			ret = i40e_create_queue_channel(vsi, ch);
			if (ret) {
				dev_err(&vsi->back->pdev->dev,
					"Failed creating queue channel with TC%d: queues %d\n",
					i, ch->num_queue_pairs);
				goto err_free;
			}
			vsi->tc_seid_map[i] = ch->seid;
		}
	}

	 
	i40e_do_reset(vsi->back, I40E_PF_RESET_FLAG, true);
	return ret;

err_free:
	i40e_remove_queue_channels(vsi);
	return ret;
}

 
int i40e_veb_config_tc(struct i40e_veb *veb, u8 enabled_tc)
{
	struct i40e_aqc_configure_switching_comp_bw_config_data bw_data = {0};
	struct i40e_pf *pf = veb->pf;
	int ret = 0;
	int i;

	 
	if (!enabled_tc || veb->enabled_tc == enabled_tc)
		return ret;

	bw_data.tc_valid_bits = enabled_tc;
	 

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (enabled_tc & BIT(i))
			bw_data.tc_bw_share_credits[i] = 1;
	}

	ret = i40e_aq_config_switch_comp_bw_config(&pf->hw, veb->seid,
						   &bw_data, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "VEB bw config failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		goto out;
	}

	 
	ret = i40e_veb_get_bw_info(veb);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Failed getting veb bw config, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
	}

out:
	return ret;
}

#ifdef CONFIG_I40E_DCB
 
static void i40e_dcb_reconfigure(struct i40e_pf *pf)
{
	u8 tc_map = 0;
	int ret;
	u8 v;

	 
	tc_map = i40e_pf_get_tc_map(pf);
	if (tc_map == I40E_DEFAULT_TRAFFIC_CLASS)
		return;

	for (v = 0; v < I40E_MAX_VEB; v++) {
		if (!pf->veb[v])
			continue;
		ret = i40e_veb_config_tc(pf->veb[v], tc_map);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "Failed configuring TC for VEB seid=%d\n",
				 pf->veb[v]->seid);
			 
		}
	}

	 
	for (v = 0; v < pf->num_alloc_vsi; v++) {
		if (!pf->vsi[v])
			continue;

		 
		if (v == pf->lan_vsi)
			tc_map = i40e_pf_get_tc_map(pf);
		else
			tc_map = I40E_DEFAULT_TRAFFIC_CLASS;

		ret = i40e_vsi_config_tc(pf->vsi[v], tc_map);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "Failed configuring TC for VSI seid=%d\n",
				 pf->vsi[v]->seid);
			 
		} else {
			 
			i40e_vsi_map_rings_to_vectors(pf->vsi[v]);
			if (pf->vsi[v]->netdev)
				i40e_dcbnl_set_all(pf->vsi[v]);
		}
	}
}

 
static int i40e_resume_port_tx(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	int ret;

	ret = i40e_aq_resume_port_tx(hw, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Resume Port Tx failed, err %pe aq_err %s\n",
			  ERR_PTR(ret),
			  i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		 
		set_bit(__I40E_PF_RESET_REQUESTED, pf->state);
		i40e_service_event_schedule(pf);
	}

	return ret;
}

 
static int i40e_suspend_port_tx(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	int ret;

	ret = i40e_aq_suspend_port_tx(hw, pf->mac_seid, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Suspend Port Tx failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		 
		set_bit(__I40E_PF_RESET_REQUESTED, pf->state);
		i40e_service_event_schedule(pf);
	}

	return ret;
}

 
static int i40e_hw_set_dcb_config(struct i40e_pf *pf,
				  struct i40e_dcbx_config *new_cfg)
{
	struct i40e_dcbx_config *old_cfg = &pf->hw.local_dcbx_config;
	int ret;

	 
	if (!memcmp(&new_cfg, &old_cfg, sizeof(new_cfg))) {
		dev_dbg(&pf->pdev->dev, "No Change in DCB Config required.\n");
		return 0;
	}

	 
	i40e_pf_quiesce_all_vsi(pf);

	 
	*old_cfg = *new_cfg;
	old_cfg->etsrec = old_cfg->etscfg;
	ret = i40e_set_dcb_config(&pf->hw);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Set DCB Config failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		goto out;
	}

	 
	i40e_dcb_reconfigure(pf);
out:
	 
	if (!test_bit(__I40E_RESET_RECOVERY_PENDING, pf->state)) {
		 
		ret = i40e_resume_port_tx(pf);
		 
		if (ret)
			goto err;
		i40e_pf_unquiesce_all_vsi(pf);
	}
err:
	return ret;
}

 
int i40e_hw_dcb_config(struct i40e_pf *pf, struct i40e_dcbx_config *new_cfg)
{
	struct i40e_aqc_configure_switching_comp_ets_data ets_data;
	u8 prio_type[I40E_MAX_TRAFFIC_CLASS] = {0};
	u32 mfs_tc[I40E_MAX_TRAFFIC_CLASS];
	struct i40e_dcbx_config *old_cfg;
	u8 mode[I40E_MAX_TRAFFIC_CLASS];
	struct i40e_rx_pb_config pb_cfg;
	struct i40e_hw *hw = &pf->hw;
	u8 num_ports = hw->num_ports;
	bool need_reconfig;
	int ret = -EINVAL;
	u8 lltc_map = 0;
	u8 tc_map = 0;
	u8 new_numtc;
	u8 i;

	dev_dbg(&pf->pdev->dev, "Configuring DCB registers directly\n");
	 

	new_numtc = i40e_dcb_get_num_tc(new_cfg);

	memset(&ets_data, 0, sizeof(ets_data));
	for (i = 0; i < new_numtc; i++) {
		tc_map |= BIT(i);
		switch (new_cfg->etscfg.tsatable[i]) {
		case I40E_IEEE_TSA_ETS:
			prio_type[i] = I40E_DCB_PRIO_TYPE_ETS;
			ets_data.tc_bw_share_credits[i] =
					new_cfg->etscfg.tcbwtable[i];
			break;
		case I40E_IEEE_TSA_STRICT:
			prio_type[i] = I40E_DCB_PRIO_TYPE_STRICT;
			lltc_map |= BIT(i);
			ets_data.tc_bw_share_credits[i] =
					I40E_DCB_STRICT_PRIO_CREDITS;
			break;
		default:
			 
			need_reconfig = false;
			goto out;
		}
	}

	old_cfg = &hw->local_dcbx_config;
	 
	need_reconfig = i40e_dcb_need_reconfig(pf, old_cfg, new_cfg);

	 
	if (need_reconfig) {
		 
		if (new_numtc > 1)
			pf->flags |= I40E_FLAG_DCB_ENABLED;
		else
			pf->flags &= ~I40E_FLAG_DCB_ENABLED;

		set_bit(__I40E_PORT_SUSPENDED, pf->state);
		 
		i40e_pf_quiesce_all_vsi(pf);
		ret = i40e_suspend_port_tx(pf);
		if (ret)
			goto err;
	}

	 
	ets_data.tc_valid_bits = tc_map;
	ets_data.tc_strict_priority_flags = lltc_map;
	ret = i40e_aq_config_switch_comp_ets
		(hw, pf->mac_seid, &ets_data,
		 i40e_aqc_opc_modify_switching_comp_ets, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Modify Port ETS failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		goto out;
	}

	 
	memset(&mode, I40E_DCB_ARB_MODE_ROUND_ROBIN, sizeof(mode));
	i40e_dcb_hw_set_num_tc(hw, new_numtc);
	i40e_dcb_hw_rx_fifo_config(hw, I40E_DCB_ARB_MODE_ROUND_ROBIN,
				   I40E_DCB_ARB_MODE_STRICT_PRIORITY,
				   I40E_DCB_DEFAULT_MAX_EXPONENT,
				   lltc_map);
	i40e_dcb_hw_rx_cmd_monitor_config(hw, new_numtc, num_ports);
	i40e_dcb_hw_rx_ets_bw_config(hw, new_cfg->etscfg.tcbwtable, mode,
				     prio_type);
	i40e_dcb_hw_pfc_config(hw, new_cfg->pfc.pfcenable,
			       new_cfg->etscfg.prioritytable);
	i40e_dcb_hw_rx_up2tc_config(hw, new_cfg->etscfg.prioritytable);

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		mfs_tc[i] = pf->vsi[pf->lan_vsi]->netdev->mtu;
		mfs_tc[i] += I40E_PACKET_HDR_PAD;
	}

	i40e_dcb_hw_calculate_pool_sizes(hw, num_ports,
					 false, new_cfg->pfc.pfcenable,
					 mfs_tc, &pb_cfg);
	i40e_dcb_hw_rx_pb_config(hw, &pf->pb_cfg, &pb_cfg);

	 
	pf->pb_cfg = pb_cfg;

	 
	ret = i40e_aq_dcb_updated(&pf->hw, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "DCB Updated failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		goto out;
	}

	 
	*old_cfg = *new_cfg;

	 
	i40e_dcb_reconfigure(pf);
out:
	 
	if (need_reconfig) {
		ret = i40e_resume_port_tx(pf);

		clear_bit(__I40E_PORT_SUSPENDED, pf->state);
		 
		if (ret)
			goto err;

		 
		ret = i40e_pf_wait_queues_disabled(pf);
		if (ret) {
			 
			set_bit(__I40E_PF_RESET_REQUESTED, pf->state);
			i40e_service_event_schedule(pf);
			goto err;
		} else {
			i40e_pf_unquiesce_all_vsi(pf);
			set_bit(__I40E_CLIENT_SERVICE_REQUESTED, pf->state);
			set_bit(__I40E_CLIENT_L2_CHANGE, pf->state);
		}
		 
		if (pf->hw_features & I40E_HW_USE_SET_LLDP_MIB)
			ret = i40e_hw_set_dcb_config(pf, new_cfg);
	}

err:
	return ret;
}

 
int i40e_dcb_sw_default_config(struct i40e_pf *pf)
{
	struct i40e_dcbx_config *dcb_cfg = &pf->hw.local_dcbx_config;
	struct i40e_aqc_configure_switching_comp_ets_data ets_data;
	struct i40e_hw *hw = &pf->hw;
	int err;

	if (pf->hw_features & I40E_HW_USE_SET_LLDP_MIB) {
		 
		memset(&pf->tmp_cfg, 0, sizeof(struct i40e_dcbx_config));
		pf->tmp_cfg.etscfg.willing = I40E_IEEE_DEFAULT_ETS_WILLING;
		pf->tmp_cfg.etscfg.maxtcs = 0;
		pf->tmp_cfg.etscfg.tcbwtable[0] = I40E_IEEE_DEFAULT_ETS_TCBW;
		pf->tmp_cfg.etscfg.tsatable[0] = I40E_IEEE_TSA_ETS;
		pf->tmp_cfg.pfc.willing = I40E_IEEE_DEFAULT_PFC_WILLING;
		pf->tmp_cfg.pfc.pfccap = I40E_MAX_TRAFFIC_CLASS;
		 
		pf->tmp_cfg.numapps = I40E_IEEE_DEFAULT_NUM_APPS;
		pf->tmp_cfg.app[0].selector = I40E_APP_SEL_ETHTYPE;
		pf->tmp_cfg.app[0].priority = I40E_IEEE_DEFAULT_APP_PRIO;
		pf->tmp_cfg.app[0].protocolid = I40E_APP_PROTOID_FCOE;

		return i40e_hw_set_dcb_config(pf, &pf->tmp_cfg);
	}

	memset(&ets_data, 0, sizeof(ets_data));
	ets_data.tc_valid_bits = I40E_DEFAULT_TRAFFIC_CLASS;  
	ets_data.tc_strict_priority_flags = 0;  
	ets_data.tc_bw_share_credits[0] = I40E_IEEE_DEFAULT_ETS_TCBW;  

	 
	err = i40e_aq_config_switch_comp_ets
		(hw, pf->mac_seid, &ets_data,
		 i40e_aqc_opc_enable_switching_comp_ets, NULL);
	if (err) {
		dev_info(&pf->pdev->dev,
			 "Enable Port ETS failed, err %pe aq_err %s\n",
			 ERR_PTR(err),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		err = -ENOENT;
		goto out;
	}

	 
	dcb_cfg->etscfg.willing = I40E_IEEE_DEFAULT_ETS_WILLING;
	dcb_cfg->etscfg.cbs = 0;
	dcb_cfg->etscfg.maxtcs = I40E_MAX_TRAFFIC_CLASS;
	dcb_cfg->etscfg.tcbwtable[0] = I40E_IEEE_DEFAULT_ETS_TCBW;

out:
	return err;
}

 
static int i40e_init_pf_dcb(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	int err;

	 
	if (pf->hw_features & I40E_HW_NO_DCB_SUPPORT) {
		dev_info(&pf->pdev->dev, "DCB is not supported.\n");
		err = -EOPNOTSUPP;
		goto out;
	}
	if (pf->flags & I40E_FLAG_DISABLE_FW_LLDP) {
		dev_info(&pf->pdev->dev, "FW LLDP is disabled, attempting SW DCB\n");
		err = i40e_dcb_sw_default_config(pf);
		if (err) {
			dev_info(&pf->pdev->dev, "Could not initialize SW DCB\n");
			goto out;
		}
		dev_info(&pf->pdev->dev, "SW DCB initialization succeeded.\n");
		pf->dcbx_cap = DCB_CAP_DCBX_HOST |
			       DCB_CAP_DCBX_VER_IEEE;
		 
		pf->flags |= I40E_FLAG_DCB_CAPABLE;
		pf->flags &= ~I40E_FLAG_DCB_ENABLED;
		goto out;
	}
	err = i40e_init_dcb(hw, true);
	if (!err) {
		 
		if ((!hw->func_caps.dcb) ||
		    (hw->dcbx_status == I40E_DCBX_STATUS_DISABLED)) {
			dev_info(&pf->pdev->dev,
				 "DCBX offload is not supported or is disabled for this PF.\n");
		} else {
			 
			pf->dcbx_cap = DCB_CAP_DCBX_LLD_MANAGED |
				       DCB_CAP_DCBX_VER_IEEE;

			pf->flags |= I40E_FLAG_DCB_CAPABLE;
			 
			if (i40e_dcb_get_num_tc(&hw->local_dcbx_config) > 1)
				pf->flags |= I40E_FLAG_DCB_ENABLED;
			else
				pf->flags &= ~I40E_FLAG_DCB_ENABLED;
			dev_dbg(&pf->pdev->dev,
				"DCBX offload is supported for this PF.\n");
		}
	} else if (pf->hw.aq.asq_last_status == I40E_AQ_RC_EPERM) {
		dev_info(&pf->pdev->dev, "FW LLDP disabled for this PF.\n");
		pf->flags |= I40E_FLAG_DISABLE_FW_LLDP;
	} else {
		dev_info(&pf->pdev->dev,
			 "Query for DCB configuration failed, err %pe aq_err %s\n",
			 ERR_PTR(err),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
	}

out:
	return err;
}
#endif  

 
void i40e_print_link_message(struct i40e_vsi *vsi, bool isup)
{
	enum i40e_aq_link_speed new_speed;
	struct i40e_pf *pf = vsi->back;
	char *speed = "Unknown";
	char *fc = "Unknown";
	char *fec = "";
	char *req_fec = "";
	char *an = "";

	if (isup)
		new_speed = pf->hw.phy.link_info.link_speed;
	else
		new_speed = I40E_LINK_SPEED_UNKNOWN;

	if ((vsi->current_isup == isup) && (vsi->current_speed == new_speed))
		return;
	vsi->current_isup = isup;
	vsi->current_speed = new_speed;
	if (!isup) {
		netdev_info(vsi->netdev, "NIC Link is Down\n");
		return;
	}

	 
	if (pf->hw.func_caps.npar_enable &&
	    (pf->hw.phy.link_info.link_speed == I40E_LINK_SPEED_1GB ||
	     pf->hw.phy.link_info.link_speed == I40E_LINK_SPEED_100MB))
		netdev_warn(vsi->netdev,
			    "The partition detected link speed that is less than 10Gbps\n");

	switch (pf->hw.phy.link_info.link_speed) {
	case I40E_LINK_SPEED_40GB:
		speed = "40 G";
		break;
	case I40E_LINK_SPEED_20GB:
		speed = "20 G";
		break;
	case I40E_LINK_SPEED_25GB:
		speed = "25 G";
		break;
	case I40E_LINK_SPEED_10GB:
		speed = "10 G";
		break;
	case I40E_LINK_SPEED_5GB:
		speed = "5 G";
		break;
	case I40E_LINK_SPEED_2_5GB:
		speed = "2.5 G";
		break;
	case I40E_LINK_SPEED_1GB:
		speed = "1000 M";
		break;
	case I40E_LINK_SPEED_100MB:
		speed = "100 M";
		break;
	default:
		break;
	}

	switch (pf->hw.fc.current_mode) {
	case I40E_FC_FULL:
		fc = "RX/TX";
		break;
	case I40E_FC_TX_PAUSE:
		fc = "TX";
		break;
	case I40E_FC_RX_PAUSE:
		fc = "RX";
		break;
	default:
		fc = "None";
		break;
	}

	if (pf->hw.phy.link_info.link_speed == I40E_LINK_SPEED_25GB) {
		req_fec = "None";
		fec = "None";
		an = "False";

		if (pf->hw.phy.link_info.an_info & I40E_AQ_AN_COMPLETED)
			an = "True";

		if (pf->hw.phy.link_info.fec_info &
		    I40E_AQ_CONFIG_FEC_KR_ENA)
			fec = "CL74 FC-FEC/BASE-R";
		else if (pf->hw.phy.link_info.fec_info &
			 I40E_AQ_CONFIG_FEC_RS_ENA)
			fec = "CL108 RS-FEC";

		 
		if (vsi->back->hw.phy.link_info.req_fec_info &
		    (I40E_AQ_REQUEST_FEC_KR | I40E_AQ_REQUEST_FEC_RS)) {
			if (vsi->back->hw.phy.link_info.req_fec_info &
			    I40E_AQ_REQUEST_FEC_RS)
				req_fec = "CL108 RS-FEC";
			else
				req_fec = "CL74 FC-FEC/BASE-R";
		}
		netdev_info(vsi->netdev,
			    "NIC Link is Up, %sbps Full Duplex, Requested FEC: %s, Negotiated FEC: %s, Autoneg: %s, Flow Control: %s\n",
			    speed, req_fec, fec, an, fc);
	} else if (pf->hw.device_id == I40E_DEV_ID_KX_X722) {
		req_fec = "None";
		fec = "None";
		an = "False";

		if (pf->hw.phy.link_info.an_info & I40E_AQ_AN_COMPLETED)
			an = "True";

		if (pf->hw.phy.link_info.fec_info &
		    I40E_AQ_CONFIG_FEC_KR_ENA)
			fec = "CL74 FC-FEC/BASE-R";

		if (pf->hw.phy.link_info.req_fec_info &
		    I40E_AQ_REQUEST_FEC_KR)
			req_fec = "CL74 FC-FEC/BASE-R";

		netdev_info(vsi->netdev,
			    "NIC Link is Up, %sbps Full Duplex, Requested FEC: %s, Negotiated FEC: %s, Autoneg: %s, Flow Control: %s\n",
			    speed, req_fec, fec, an, fc);
	} else {
		netdev_info(vsi->netdev,
			    "NIC Link is Up, %sbps Full Duplex, Flow Control: %s\n",
			    speed, fc);
	}

}

 
static int i40e_up_complete(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int err;

	if (pf->flags & I40E_FLAG_MSIX_ENABLED)
		i40e_vsi_configure_msix(vsi);
	else
		i40e_configure_msi_and_legacy(vsi);

	 
	err = i40e_vsi_start_rings(vsi);
	if (err)
		return err;

	clear_bit(__I40E_VSI_DOWN, vsi->state);
	i40e_napi_enable_all(vsi);
	i40e_vsi_enable_irq(vsi);

	if ((pf->hw.phy.link_info.link_info & I40E_AQ_LINK_UP) &&
	    (vsi->netdev)) {
		i40e_print_link_message(vsi, true);
		netif_tx_start_all_queues(vsi->netdev);
		netif_carrier_on(vsi->netdev);
	}

	 
	if (vsi->type == I40E_VSI_FDIR) {
		 
		pf->fd_add_err = 0;
		pf->fd_atr_cnt = 0;
		i40e_fdir_filter_restore(vsi);
	}

	 
	set_bit(__I40E_CLIENT_SERVICE_REQUESTED, pf->state);
	i40e_service_event_schedule(pf);

	return 0;
}

 
static void i40e_vsi_reinit_locked(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;

	while (test_and_set_bit(__I40E_CONFIG_BUSY, pf->state))
		usleep_range(1000, 2000);
	i40e_down(vsi);

	i40e_up(vsi);
	clear_bit(__I40E_CONFIG_BUSY, pf->state);
}

 
static int i40e_force_link_state(struct i40e_pf *pf, bool is_up)
{
	struct i40e_aq_get_phy_abilities_resp abilities;
	struct i40e_aq_set_phy_config config = {0};
	bool non_zero_phy_type = is_up;
	struct i40e_hw *hw = &pf->hw;
	u64 mask;
	u8 speed;
	int err;

	 
	err = i40e_aq_get_phy_capabilities(hw, false, true, &abilities,
					   NULL);
	if (err) {
		dev_err(&pf->pdev->dev,
			"failed to get phy cap., ret =  %pe last_status =  %s\n",
			ERR_PTR(err),
			i40e_aq_str(hw, hw->aq.asq_last_status));
		return err;
	}
	speed = abilities.link_speed;

	 
	err = i40e_aq_get_phy_capabilities(hw, false, false, &abilities,
					   NULL);
	if (err) {
		dev_err(&pf->pdev->dev,
			"failed to get phy cap., ret =  %pe last_status =  %s\n",
			ERR_PTR(err),
			i40e_aq_str(hw, hw->aq.asq_last_status));
		return err;
	}

	 
	if (pf->flags & I40E_FLAG_TOTAL_PORT_SHUTDOWN_ENABLED)
		non_zero_phy_type = true;
	else if (is_up && abilities.phy_type != 0 && abilities.link_speed != 0)
		return 0;

	 
	mask = I40E_PHY_TYPES_BITMASK;
	config.phy_type =
		non_zero_phy_type ? cpu_to_le32((u32)(mask & 0xffffffff)) : 0;
	config.phy_type_ext =
		non_zero_phy_type ? (u8)((mask >> 32) & 0xff) : 0;
	 
	config.abilities = abilities.abilities;
	if (pf->flags & I40E_FLAG_TOTAL_PORT_SHUTDOWN_ENABLED) {
		if (is_up)
			config.abilities |= I40E_AQ_PHY_ENABLE_LINK;
		else
			config.abilities &= ~(I40E_AQ_PHY_ENABLE_LINK);
	}
	if (abilities.link_speed != 0)
		config.link_speed = abilities.link_speed;
	else
		config.link_speed = speed;
	config.eee_capability = abilities.eee_capability;
	config.eeer = abilities.eeer_val;
	config.low_power_ctrl = abilities.d3_lpan;
	config.fec_config = abilities.fec_cfg_curr_mod_ext_info &
			    I40E_AQ_PHY_FEC_CONFIG_MASK;
	err = i40e_aq_set_phy_config(hw, &config, NULL);

	if (err) {
		dev_err(&pf->pdev->dev,
			"set phy config ret =  %pe last_status =  %s\n",
			ERR_PTR(err),
			i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		return err;
	}

	 
	err = i40e_update_link_info(hw);
	if (err) {
		 
		msleep(1000);
		i40e_update_link_info(hw);
	}

	i40e_aq_set_link_restart_an(hw, is_up, NULL);

	return 0;
}

 
int i40e_up(struct i40e_vsi *vsi)
{
	int err;

	if (vsi->type == I40E_VSI_MAIN &&
	    (vsi->back->flags & I40E_FLAG_LINK_DOWN_ON_CLOSE_ENABLED ||
	     vsi->back->flags & I40E_FLAG_TOTAL_PORT_SHUTDOWN_ENABLED))
		i40e_force_link_state(vsi->back, true);

	err = i40e_vsi_configure(vsi);
	if (!err)
		err = i40e_up_complete(vsi);

	return err;
}

 
void i40e_down(struct i40e_vsi *vsi)
{
	int i;

	 
	if (vsi->netdev) {
		netif_carrier_off(vsi->netdev);
		netif_tx_disable(vsi->netdev);
	}
	i40e_vsi_disable_irq(vsi);
	i40e_vsi_stop_rings(vsi);
	if (vsi->type == I40E_VSI_MAIN &&
	   (vsi->back->flags & I40E_FLAG_LINK_DOWN_ON_CLOSE_ENABLED ||
	    vsi->back->flags & I40E_FLAG_TOTAL_PORT_SHUTDOWN_ENABLED))
		i40e_force_link_state(vsi->back, false);
	i40e_napi_disable_all(vsi);

	for (i = 0; i < vsi->num_queue_pairs; i++) {
		i40e_clean_tx_ring(vsi->tx_rings[i]);
		if (i40e_enabled_xdp_vsi(vsi)) {
			 
			synchronize_rcu();
			i40e_clean_tx_ring(vsi->xdp_rings[i]);
		}
		i40e_clean_rx_ring(vsi->rx_rings[i]);
	}

}

 
static int i40e_validate_mqprio_qopt(struct i40e_vsi *vsi,
				     struct tc_mqprio_qopt_offload *mqprio_qopt)
{
	u64 sum_max_rate = 0;
	u64 max_rate = 0;
	int i;

	if (mqprio_qopt->qopt.offset[0] != 0 ||
	    mqprio_qopt->qopt.num_tc < 1 ||
	    mqprio_qopt->qopt.num_tc > I40E_MAX_TRAFFIC_CLASS)
		return -EINVAL;
	for (i = 0; ; i++) {
		if (!mqprio_qopt->qopt.count[i])
			return -EINVAL;
		if (mqprio_qopt->min_rate[i]) {
			dev_err(&vsi->back->pdev->dev,
				"Invalid min tx rate (greater than 0) specified\n");
			return -EINVAL;
		}
		max_rate = mqprio_qopt->max_rate[i];
		do_div(max_rate, I40E_BW_MBPS_DIVISOR);
		sum_max_rate += max_rate;

		if (i >= mqprio_qopt->qopt.num_tc - 1)
			break;
		if (mqprio_qopt->qopt.offset[i + 1] !=
		    (mqprio_qopt->qopt.offset[i] + mqprio_qopt->qopt.count[i]))
			return -EINVAL;
	}
	if (vsi->num_queue_pairs <
	    (mqprio_qopt->qopt.offset[i] + mqprio_qopt->qopt.count[i])) {
		dev_err(&vsi->back->pdev->dev,
			"Failed to create traffic channel, insufficient number of queues.\n");
		return -EINVAL;
	}
	if (sum_max_rate > i40e_get_link_speed(vsi)) {
		dev_err(&vsi->back->pdev->dev,
			"Invalid max tx rate specified\n");
		return -EINVAL;
	}
	return 0;
}

 
static void i40e_vsi_set_default_tc_config(struct i40e_vsi *vsi)
{
	u16 qcount;
	int i;

	 
	vsi->tc_config.numtc = 1;
	vsi->tc_config.enabled_tc = 1;
	qcount = min_t(int, vsi->alloc_queue_pairs,
		       i40e_pf_get_max_q_per_tc(vsi->back));
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		 
		vsi->tc_config.tc_info[i].qoffset = 0;
		if (i == 0)
			vsi->tc_config.tc_info[i].qcount = qcount;
		else
			vsi->tc_config.tc_info[i].qcount = 1;
		vsi->tc_config.tc_info[i].netdev_tc = 0;
	}
}

 
static int i40e_del_macvlan_filter(struct i40e_hw *hw, u16 seid,
				   const u8 *macaddr, int *aq_err)
{
	struct i40e_aqc_remove_macvlan_element_data element;
	int status;

	memset(&element, 0, sizeof(element));
	ether_addr_copy(element.mac_addr, macaddr);
	element.vlan_tag = 0;
	element.flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
	status = i40e_aq_remove_macvlan(hw, seid, &element, 1, NULL);
	*aq_err = hw->aq.asq_last_status;

	return status;
}

 
static int i40e_add_macvlan_filter(struct i40e_hw *hw, u16 seid,
				   const u8 *macaddr, int *aq_err)
{
	struct i40e_aqc_add_macvlan_element_data element;
	u16 cmd_flags = 0;
	int status;

	ether_addr_copy(element.mac_addr, macaddr);
	element.vlan_tag = 0;
	element.queue_number = 0;
	element.match_method = I40E_AQC_MM_ERR_NO_RES;
	cmd_flags |= I40E_AQC_MACVLAN_ADD_PERFECT_MATCH;
	element.flags = cpu_to_le16(cmd_flags);
	status = i40e_aq_add_macvlan(hw, seid, &element, 1, NULL);
	*aq_err = hw->aq.asq_last_status;

	return status;
}

 
static void i40e_reset_ch_rings(struct i40e_vsi *vsi, struct i40e_channel *ch)
{
	struct i40e_ring *tx_ring, *rx_ring;
	u16 pf_q;
	int i;

	for (i = 0; i < ch->num_queue_pairs; i++) {
		pf_q = ch->base_queue + i;
		tx_ring = vsi->tx_rings[pf_q];
		tx_ring->ch = NULL;
		rx_ring = vsi->rx_rings[pf_q];
		rx_ring->ch = NULL;
	}
}

 
static void i40e_free_macvlan_channels(struct i40e_vsi *vsi)
{
	struct i40e_channel *ch, *ch_tmp;
	int ret;

	if (list_empty(&vsi->macvlan_list))
		return;

	list_for_each_entry_safe(ch, ch_tmp, &vsi->macvlan_list, list) {
		struct i40e_vsi *parent_vsi;

		if (i40e_is_channel_macvlan(ch)) {
			i40e_reset_ch_rings(vsi, ch);
			clear_bit(ch->fwd->bit_no, vsi->fwd_bitmask);
			netdev_unbind_sb_channel(vsi->netdev, ch->fwd->netdev);
			netdev_set_sb_channel(ch->fwd->netdev, 0);
			kfree(ch->fwd);
			ch->fwd = NULL;
		}

		list_del(&ch->list);
		parent_vsi = ch->parent_vsi;
		if (!parent_vsi || !ch->initialized) {
			kfree(ch);
			continue;
		}

		 
		ret = i40e_aq_delete_element(&vsi->back->hw, ch->seid,
					     NULL);
		if (ret)
			dev_err(&vsi->back->pdev->dev,
				"unable to remove channel (%d) for parent VSI(%d)\n",
				ch->seid, parent_vsi->seid);
		kfree(ch);
	}
	vsi->macvlan_cnt = 0;
}

 
static int i40e_fwd_ring_up(struct i40e_vsi *vsi, struct net_device *vdev,
			    struct i40e_fwd_adapter *fwd)
{
	struct i40e_channel *ch = NULL, *ch_tmp, *iter;
	int ret = 0, num_tc = 1,  i, aq_err;
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;

	 
	list_for_each_entry_safe(iter, ch_tmp, &vsi->macvlan_list, list) {
		if (!i40e_is_channel_macvlan(iter)) {
			iter->fwd = fwd;
			 
			for (i = 0; i < num_tc; i++)
				netdev_bind_sb_channel_queue(vsi->netdev, vdev,
							     i,
							     iter->num_queue_pairs,
							     iter->base_queue);
			for (i = 0; i < iter->num_queue_pairs; i++) {
				struct i40e_ring *tx_ring, *rx_ring;
				u16 pf_q;

				pf_q = iter->base_queue + i;

				 
				tx_ring = vsi->tx_rings[pf_q];
				tx_ring->ch = iter;

				 
				rx_ring = vsi->rx_rings[pf_q];
				rx_ring->ch = iter;
			}
			ch = iter;
			break;
		}
	}

	if (!ch)
		return -EINVAL;

	 
	wmb();

	 
	ret = i40e_add_macvlan_filter(hw, ch->seid, vdev->dev_addr, &aq_err);
	if (ret) {
		 
		macvlan_release_l2fw_offload(vdev);
		for (i = 0; i < ch->num_queue_pairs; i++) {
			struct i40e_ring *rx_ring;
			u16 pf_q;

			pf_q = ch->base_queue + i;
			rx_ring = vsi->rx_rings[pf_q];
			rx_ring->netdev = NULL;
		}
		dev_info(&pf->pdev->dev,
			 "Error adding mac filter on macvlan err %pe, aq_err %s\n",
			  ERR_PTR(ret),
			  i40e_aq_str(hw, aq_err));
		netdev_err(vdev, "L2fwd offload disabled to L2 filter error\n");
	}

	return ret;
}

 
static int i40e_setup_macvlans(struct i40e_vsi *vsi, u16 macvlan_cnt, u16 qcnt,
			       struct net_device *vdev)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vsi_context ctxt;
	u16 sections, qmap, num_qps;
	struct i40e_channel *ch;
	int i, pow, ret = 0;
	u8 offset = 0;

	if (vsi->type != I40E_VSI_MAIN || !macvlan_cnt)
		return -EINVAL;

	num_qps = vsi->num_queue_pairs - (macvlan_cnt * qcnt);

	 
	pow = fls(roundup_pow_of_two(num_qps) - 1);

	qmap = (offset << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
		(pow << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT);

	 
	sections = I40E_AQ_VSI_PROP_QUEUE_MAP_VALID;
	sections |= I40E_AQ_VSI_PROP_SCHED_VALID;
	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.seid = vsi->seid;
	ctxt.pf_num = vsi->back->hw.pf_id;
	ctxt.vf_num = 0;
	ctxt.uplink_seid = vsi->uplink_seid;
	ctxt.info = vsi->info;
	ctxt.info.tc_mapping[0] = cpu_to_le16(qmap);
	ctxt.info.mapping_flags |= cpu_to_le16(I40E_AQ_VSI_QUE_MAP_CONTIG);
	ctxt.info.queue_mapping[0] = cpu_to_le16(vsi->base_queue);
	ctxt.info.valid_sections |= cpu_to_le16(sections);

	 
	vsi->rss_size = max_t(u16, num_qps, qcnt);
	ret = i40e_vsi_config_rss(vsi);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Failed to reconfig RSS for num_queues (%u)\n",
			 vsi->rss_size);
		return ret;
	}
	vsi->reconfig_rss = true;
	dev_dbg(&vsi->back->pdev->dev,
		"Reconfigured RSS with num_queues (%u)\n", vsi->rss_size);
	vsi->next_base_queue = num_qps;
	vsi->cnt_q_avail = vsi->num_queue_pairs - num_qps;

	 
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Update vsi tc config failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(hw, hw->aq.asq_last_status));
		return ret;
	}
	 
	i40e_vsi_update_queue_map(vsi, &ctxt);
	vsi->info.valid_sections = 0;

	 
	INIT_LIST_HEAD(&vsi->macvlan_list);
	for (i = 0; i < macvlan_cnt; i++) {
		ch = kzalloc(sizeof(*ch), GFP_KERNEL);
		if (!ch) {
			ret = -ENOMEM;
			goto err_free;
		}
		INIT_LIST_HEAD(&ch->list);
		ch->num_queue_pairs = qcnt;
		if (!i40e_setup_channel(pf, vsi, ch)) {
			ret = -EINVAL;
			kfree(ch);
			goto err_free;
		}
		ch->parent_vsi = vsi;
		vsi->cnt_q_avail -= ch->num_queue_pairs;
		vsi->macvlan_cnt++;
		list_add_tail(&ch->list, &vsi->macvlan_list);
	}

	return ret;

err_free:
	dev_info(&pf->pdev->dev, "Failed to setup macvlans\n");
	i40e_free_macvlan_channels(vsi);

	return ret;
}

 
static void *i40e_fwd_add(struct net_device *netdev, struct net_device *vdev)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	u16 q_per_macvlan = 0, macvlan_cnt = 0, vectors;
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_fwd_adapter *fwd;
	int avail_macvlan, ret;

	if ((pf->flags & I40E_FLAG_DCB_ENABLED)) {
		netdev_info(netdev, "Macvlans are not supported when DCB is enabled\n");
		return ERR_PTR(-EINVAL);
	}
	if (i40e_is_tc_mqprio_enabled(pf)) {
		netdev_info(netdev, "Macvlans are not supported when HW TC offload is on\n");
		return ERR_PTR(-EINVAL);
	}
	if (pf->num_lan_msix < I40E_MIN_MACVLAN_VECTORS) {
		netdev_info(netdev, "Not enough vectors available to support macvlans\n");
		return ERR_PTR(-EINVAL);
	}

	 
	if (netif_is_multiqueue(vdev))
		return ERR_PTR(-ERANGE);

	if (!vsi->macvlan_cnt) {
		 
		set_bit(0, vsi->fwd_bitmask);

		 
		vectors = pf->num_lan_msix;
		if (vectors <= I40E_MAX_MACVLANS && vectors > 64) {
			 
			q_per_macvlan = 4;
			macvlan_cnt = (vectors - 32) / 4;
		} else if (vectors <= 64 && vectors > 32) {
			 
			q_per_macvlan = 2;
			macvlan_cnt = (vectors - 16) / 2;
		} else if (vectors <= 32 && vectors > 16) {
			 
			q_per_macvlan = 1;
			macvlan_cnt = vectors - 16;
		} else if (vectors <= 16 && vectors > 8) {
			 
			q_per_macvlan = 1;
			macvlan_cnt = vectors - 8;
		} else {
			 
			q_per_macvlan = 1;
			macvlan_cnt = vectors - 1;
		}

		if (macvlan_cnt == 0)
			return ERR_PTR(-EBUSY);

		 
		i40e_quiesce_vsi(vsi);

		 
		ret = i40e_setup_macvlans(vsi, macvlan_cnt, q_per_macvlan,
					  vdev);
		if (ret)
			return ERR_PTR(ret);

		 
		i40e_unquiesce_vsi(vsi);
	}
	avail_macvlan = find_first_zero_bit(vsi->fwd_bitmask,
					    vsi->macvlan_cnt);
	if (avail_macvlan >= I40E_MAX_MACVLANS)
		return ERR_PTR(-EBUSY);

	 
	fwd = kzalloc(sizeof(*fwd), GFP_KERNEL);
	if (!fwd)
		return ERR_PTR(-ENOMEM);

	set_bit(avail_macvlan, vsi->fwd_bitmask);
	fwd->bit_no = avail_macvlan;
	netdev_set_sb_channel(vdev, avail_macvlan);
	fwd->netdev = vdev;

	if (!netif_running(netdev))
		return fwd;

	 
	ret = i40e_fwd_ring_up(vsi, vdev, fwd);
	if (ret) {
		 
		netdev_unbind_sb_channel(netdev, vdev);
		netdev_set_sb_channel(vdev, 0);

		kfree(fwd);
		return ERR_PTR(-EINVAL);
	}

	return fwd;
}

 
static void i40e_del_all_macvlans(struct i40e_vsi *vsi)
{
	struct i40e_channel *ch, *ch_tmp;
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	int aq_err, ret = 0;

	if (list_empty(&vsi->macvlan_list))
		return;

	list_for_each_entry_safe(ch, ch_tmp, &vsi->macvlan_list, list) {
		if (i40e_is_channel_macvlan(ch)) {
			ret = i40e_del_macvlan_filter(hw, ch->seid,
						      i40e_channel_mac(ch),
						      &aq_err);
			if (!ret) {
				 
				i40e_reset_ch_rings(vsi, ch);
				clear_bit(ch->fwd->bit_no, vsi->fwd_bitmask);
				netdev_unbind_sb_channel(vsi->netdev,
							 ch->fwd->netdev);
				netdev_set_sb_channel(ch->fwd->netdev, 0);
				kfree(ch->fwd);
				ch->fwd = NULL;
			}
		}
	}
}

 
static void i40e_fwd_del(struct net_device *netdev, void *vdev)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_fwd_adapter *fwd = vdev;
	struct i40e_channel *ch, *ch_tmp;
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	int aq_err, ret = 0;

	 
	list_for_each_entry_safe(ch, ch_tmp, &vsi->macvlan_list, list) {
		if (i40e_is_channel_macvlan(ch) &&
		    ether_addr_equal(i40e_channel_mac(ch),
				     fwd->netdev->dev_addr)) {
			ret = i40e_del_macvlan_filter(hw, ch->seid,
						      i40e_channel_mac(ch),
						      &aq_err);
			if (!ret) {
				 
				i40e_reset_ch_rings(vsi, ch);
				clear_bit(ch->fwd->bit_no, vsi->fwd_bitmask);
				netdev_unbind_sb_channel(netdev, fwd->netdev);
				netdev_set_sb_channel(fwd->netdev, 0);
				kfree(ch->fwd);
				ch->fwd = NULL;
			} else {
				dev_info(&pf->pdev->dev,
					 "Error deleting mac filter on macvlan err %pe, aq_err %s\n",
					  ERR_PTR(ret),
					  i40e_aq_str(hw, aq_err));
			}
			break;
		}
	}
}

 
static int i40e_setup_tc(struct net_device *netdev, void *type_data)
{
	struct tc_mqprio_qopt_offload *mqprio_qopt = type_data;
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	u8 enabled_tc = 0, num_tc, hw;
	bool need_reset = false;
	int old_queue_pairs;
	int ret = -EINVAL;
	u16 mode;
	int i;

	old_queue_pairs = vsi->num_queue_pairs;
	num_tc = mqprio_qopt->qopt.num_tc;
	hw = mqprio_qopt->qopt.hw;
	mode = mqprio_qopt->mode;
	if (!hw) {
		pf->flags &= ~I40E_FLAG_TC_MQPRIO;
		memcpy(&vsi->mqprio_qopt, mqprio_qopt, sizeof(*mqprio_qopt));
		goto config_tc;
	}

	 
	if (pf->flags & I40E_FLAG_MFP_ENABLED) {
		netdev_info(netdev,
			    "Configuring TC not supported in MFP mode\n");
		return ret;
	}
	switch (mode) {
	case TC_MQPRIO_MODE_DCB:
		pf->flags &= ~I40E_FLAG_TC_MQPRIO;

		 
		if (!(pf->flags & I40E_FLAG_DCB_ENABLED)) {
			netdev_info(netdev,
				    "DCB is not enabled for adapter\n");
			return ret;
		}

		 
		if (num_tc > i40e_pf_get_num_tc(pf)) {
			netdev_info(netdev,
				    "TC count greater than enabled on link for adapter\n");
			return ret;
		}
		break;
	case TC_MQPRIO_MODE_CHANNEL:
		if (pf->flags & I40E_FLAG_DCB_ENABLED) {
			netdev_info(netdev,
				    "Full offload of TC Mqprio options is not supported when DCB is enabled\n");
			return ret;
		}
		if (!(pf->flags & I40E_FLAG_MSIX_ENABLED))
			return ret;
		ret = i40e_validate_mqprio_qopt(vsi, mqprio_qopt);
		if (ret)
			return ret;
		memcpy(&vsi->mqprio_qopt, mqprio_qopt,
		       sizeof(*mqprio_qopt));
		pf->flags |= I40E_FLAG_TC_MQPRIO;
		pf->flags &= ~I40E_FLAG_DCB_ENABLED;
		break;
	default:
		return -EINVAL;
	}

config_tc:
	 
	for (i = 0; i < num_tc; i++)
		enabled_tc |= BIT(i);

	 
	if (enabled_tc == vsi->tc_config.enabled_tc &&
	    mode != TC_MQPRIO_MODE_CHANNEL)
		return 0;

	 
	i40e_quiesce_vsi(vsi);

	if (!hw && !i40e_is_tc_mqprio_enabled(pf))
		i40e_remove_queue_channels(vsi);

	 
	ret = i40e_vsi_config_tc(vsi, enabled_tc);
	if (ret) {
		netdev_info(netdev, "Failed configuring TC for VSI seid=%d\n",
			    vsi->seid);
		need_reset = true;
		goto exit;
	} else if (enabled_tc &&
		   (!is_power_of_2(vsi->tc_config.tc_info[0].qcount))) {
		netdev_info(netdev,
			    "Failed to create channel. Override queues (%u) not power of 2\n",
			    vsi->tc_config.tc_info[0].qcount);
		ret = -EINVAL;
		need_reset = true;
		goto exit;
	}

	dev_info(&vsi->back->pdev->dev,
		 "Setup channel (id:%u) utilizing num_queues %d\n",
		 vsi->seid, vsi->tc_config.tc_info[0].qcount);

	if (i40e_is_tc_mqprio_enabled(pf)) {
		if (vsi->mqprio_qopt.max_rate[0]) {
			u64 max_tx_rate = i40e_bw_bytes_to_mbits(vsi,
						  vsi->mqprio_qopt.max_rate[0]);

			ret = i40e_set_bw_limit(vsi, vsi->seid, max_tx_rate);
			if (!ret) {
				u64 credits = max_tx_rate;

				do_div(credits, I40E_BW_CREDIT_DIVISOR);
				dev_dbg(&vsi->back->pdev->dev,
					"Set tx rate of %llu Mbps (count of 50Mbps %llu) for vsi->seid %u\n",
					max_tx_rate,
					credits,
					vsi->seid);
			} else {
				need_reset = true;
				goto exit;
			}
		}
		ret = i40e_configure_queue_channels(vsi);
		if (ret) {
			vsi->num_queue_pairs = old_queue_pairs;
			netdev_info(netdev,
				    "Failed configuring queue channels\n");
			need_reset = true;
			goto exit;
		}
	}

exit:
	 
	if (need_reset) {
		i40e_vsi_set_default_tc_config(vsi);
		need_reset = false;
	}

	 
	i40e_unquiesce_vsi(vsi);
	return ret;
}

 
static inline void
i40e_set_cld_element(struct i40e_cloud_filter *filter,
		     struct i40e_aqc_cloud_filters_element_data *cld)
{
	u32 ipa;
	int i;

	memset(cld, 0, sizeof(*cld));
	ether_addr_copy(cld->outer_mac, filter->dst_mac);
	ether_addr_copy(cld->inner_mac, filter->src_mac);

	if (filter->n_proto != ETH_P_IP && filter->n_proto != ETH_P_IPV6)
		return;

	if (filter->n_proto == ETH_P_IPV6) {
#define IPV6_MAX_INDEX	(ARRAY_SIZE(filter->dst_ipv6) - 1)
		for (i = 0; i < ARRAY_SIZE(filter->dst_ipv6); i++) {
			ipa = be32_to_cpu(filter->dst_ipv6[IPV6_MAX_INDEX - i]);

			*(__le32 *)&cld->ipaddr.raw_v6.data[i * 2] = cpu_to_le32(ipa);
		}
	} else {
		ipa = be32_to_cpu(filter->dst_ipv4);

		memcpy(&cld->ipaddr.v4.data, &ipa, sizeof(ipa));
	}

	cld->inner_vlan = cpu_to_le16(ntohs(filter->vlan_id));

	 
	if (filter->tenant_id)
		return;
}

 
int i40e_add_del_cloud_filter(struct i40e_vsi *vsi,
			      struct i40e_cloud_filter *filter, bool add)
{
	struct i40e_aqc_cloud_filters_element_data cld_filter;
	struct i40e_pf *pf = vsi->back;
	int ret;
	static const u16 flag_table[128] = {
		[I40E_CLOUD_FILTER_FLAGS_OMAC]  =
			I40E_AQC_ADD_CLOUD_FILTER_OMAC,
		[I40E_CLOUD_FILTER_FLAGS_IMAC]  =
			I40E_AQC_ADD_CLOUD_FILTER_IMAC,
		[I40E_CLOUD_FILTER_FLAGS_IMAC_IVLAN]  =
			I40E_AQC_ADD_CLOUD_FILTER_IMAC_IVLAN,
		[I40E_CLOUD_FILTER_FLAGS_IMAC_TEN_ID] =
			I40E_AQC_ADD_CLOUD_FILTER_IMAC_TEN_ID,
		[I40E_CLOUD_FILTER_FLAGS_OMAC_TEN_ID_IMAC] =
			I40E_AQC_ADD_CLOUD_FILTER_OMAC_TEN_ID_IMAC,
		[I40E_CLOUD_FILTER_FLAGS_IMAC_IVLAN_TEN_ID] =
			I40E_AQC_ADD_CLOUD_FILTER_IMAC_IVLAN_TEN_ID,
		[I40E_CLOUD_FILTER_FLAGS_IIP] =
			I40E_AQC_ADD_CLOUD_FILTER_IIP,
	};

	if (filter->flags >= ARRAY_SIZE(flag_table))
		return -EIO;

	memset(&cld_filter, 0, sizeof(cld_filter));

	 
	i40e_set_cld_element(filter, &cld_filter);

	if (filter->tunnel_type != I40E_CLOUD_TNL_TYPE_NONE)
		cld_filter.flags = cpu_to_le16(filter->tunnel_type <<
					     I40E_AQC_ADD_CLOUD_TNL_TYPE_SHIFT);

	if (filter->n_proto == ETH_P_IPV6)
		cld_filter.flags |= cpu_to_le16(flag_table[filter->flags] |
						I40E_AQC_ADD_CLOUD_FLAGS_IPV6);
	else
		cld_filter.flags |= cpu_to_le16(flag_table[filter->flags] |
						I40E_AQC_ADD_CLOUD_FLAGS_IPV4);

	if (add)
		ret = i40e_aq_add_cloud_filters(&pf->hw, filter->seid,
						&cld_filter, 1);
	else
		ret = i40e_aq_rem_cloud_filters(&pf->hw, filter->seid,
						&cld_filter, 1);
	if (ret)
		dev_dbg(&pf->pdev->dev,
			"Failed to %s cloud filter using l4 port %u, err %d aq_err %d\n",
			add ? "add" : "delete", filter->dst_port, ret,
			pf->hw.aq.asq_last_status);
	else
		dev_info(&pf->pdev->dev,
			 "%s cloud filter for VSI: %d\n",
			 add ? "Added" : "Deleted", filter->seid);
	return ret;
}

 
int i40e_add_del_cloud_filter_big_buf(struct i40e_vsi *vsi,
				      struct i40e_cloud_filter *filter,
				      bool add)
{
	struct i40e_aqc_cloud_filters_element_bb cld_filter;
	struct i40e_pf *pf = vsi->back;
	int ret;

	 
	if ((is_valid_ether_addr(filter->dst_mac) &&
	     is_valid_ether_addr(filter->src_mac)) ||
	    (is_multicast_ether_addr(filter->dst_mac) &&
	     is_multicast_ether_addr(filter->src_mac)))
		return -EOPNOTSUPP;

	 
	if (!filter->dst_port || filter->ip_proto == IPPROTO_UDP)
		return -EOPNOTSUPP;

	 
	if (filter->src_port ||
	    (filter->src_ipv4 && filter->n_proto != ETH_P_IPV6) ||
	    !ipv6_addr_any(&filter->ip.v6.src_ip6))
		return -EOPNOTSUPP;

	memset(&cld_filter, 0, sizeof(cld_filter));

	 
	i40e_set_cld_element(filter, &cld_filter.element);

	if (is_valid_ether_addr(filter->dst_mac) ||
	    is_valid_ether_addr(filter->src_mac) ||
	    is_multicast_ether_addr(filter->dst_mac) ||
	    is_multicast_ether_addr(filter->src_mac)) {
		 
		if (filter->dst_ipv4)
			return -EOPNOTSUPP;

		 
		cld_filter.element.flags =
			cpu_to_le16(I40E_AQC_ADD_CLOUD_FILTER_MAC_PORT);

		if (filter->vlan_id) {
			cld_filter.element.flags =
			cpu_to_le16(I40E_AQC_ADD_CLOUD_FILTER_MAC_VLAN_PORT);
		}

	} else if ((filter->dst_ipv4 && filter->n_proto != ETH_P_IPV6) ||
		   !ipv6_addr_any(&filter->ip.v6.dst_ip6)) {
		cld_filter.element.flags =
				cpu_to_le16(I40E_AQC_ADD_CLOUD_FILTER_IP_PORT);
		if (filter->n_proto == ETH_P_IPV6)
			cld_filter.element.flags |=
				cpu_to_le16(I40E_AQC_ADD_CLOUD_FLAGS_IPV6);
		else
			cld_filter.element.flags |=
				cpu_to_le16(I40E_AQC_ADD_CLOUD_FLAGS_IPV4);
	} else {
		dev_err(&pf->pdev->dev,
			"either mac or ip has to be valid for cloud filter\n");
		return -EINVAL;
	}

	 
	cld_filter.general_fields[I40E_AQC_ADD_CLOUD_FV_FLU_0X16_WORD0] =
						be16_to_cpu(filter->dst_port);

	if (add) {
		 
		ret = i40e_validate_and_set_switch_mode(vsi);
		if (ret) {
			dev_err(&pf->pdev->dev,
				"failed to set switch mode, ret %d\n",
				ret);
			return ret;
		}

		ret = i40e_aq_add_cloud_filters_bb(&pf->hw, filter->seid,
						   &cld_filter, 1);
	} else {
		ret = i40e_aq_rem_cloud_filters_bb(&pf->hw, filter->seid,
						   &cld_filter, 1);
	}

	if (ret)
		dev_dbg(&pf->pdev->dev,
			"Failed to %s cloud filter(big buffer) err %d aq_err %d\n",
			add ? "add" : "delete", ret, pf->hw.aq.asq_last_status);
	else
		dev_info(&pf->pdev->dev,
			 "%s cloud filter for VSI: %d, L4 port: %d\n",
			 add ? "add" : "delete", filter->seid,
			 ntohs(filter->dst_port));
	return ret;
}

 
static int i40e_parse_cls_flower(struct i40e_vsi *vsi,
				 struct flow_cls_offload *f,
				 struct i40e_cloud_filter *filter)
{
	struct flow_rule *rule = flow_cls_offload_flow_rule(f);
	struct flow_dissector *dissector = rule->match.dissector;
	u16 n_proto_mask = 0, n_proto_key = 0, addr_type = 0;
	struct i40e_pf *pf = vsi->back;
	u8 field_flags = 0;

	if (dissector->used_keys &
	    ~(BIT_ULL(FLOW_DISSECTOR_KEY_CONTROL) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_BASIC) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_ETH_ADDRS) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_VLAN) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_IPV4_ADDRS) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_IPV6_ADDRS) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_PORTS) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_ENC_KEYID))) {
		dev_err(&pf->pdev->dev, "Unsupported key used: 0x%llx\n",
			dissector->used_keys);
		return -EOPNOTSUPP;
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ENC_KEYID)) {
		struct flow_match_enc_keyid match;

		flow_rule_match_enc_keyid(rule, &match);
		if (match.mask->keyid != 0)
			field_flags |= I40E_CLOUD_FIELD_TEN_ID;

		filter->tenant_id = be32_to_cpu(match.key->keyid);
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_BASIC)) {
		struct flow_match_basic match;

		flow_rule_match_basic(rule, &match);
		n_proto_key = ntohs(match.key->n_proto);
		n_proto_mask = ntohs(match.mask->n_proto);

		if (n_proto_key == ETH_P_ALL) {
			n_proto_key = 0;
			n_proto_mask = 0;
		}
		filter->n_proto = n_proto_key & n_proto_mask;
		filter->ip_proto = match.key->ip_proto;
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ETH_ADDRS)) {
		struct flow_match_eth_addrs match;

		flow_rule_match_eth_addrs(rule, &match);

		 
		if (!is_zero_ether_addr(match.mask->dst)) {
			if (is_broadcast_ether_addr(match.mask->dst)) {
				field_flags |= I40E_CLOUD_FIELD_OMAC;
			} else {
				dev_err(&pf->pdev->dev, "Bad ether dest mask %pM\n",
					match.mask->dst);
				return -EIO;
			}
		}

		if (!is_zero_ether_addr(match.mask->src)) {
			if (is_broadcast_ether_addr(match.mask->src)) {
				field_flags |= I40E_CLOUD_FIELD_IMAC;
			} else {
				dev_err(&pf->pdev->dev, "Bad ether src mask %pM\n",
					match.mask->src);
				return -EIO;
			}
		}
		ether_addr_copy(filter->dst_mac, match.key->dst);
		ether_addr_copy(filter->src_mac, match.key->src);
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_VLAN)) {
		struct flow_match_vlan match;

		flow_rule_match_vlan(rule, &match);
		if (match.mask->vlan_id) {
			if (match.mask->vlan_id == VLAN_VID_MASK) {
				field_flags |= I40E_CLOUD_FIELD_IVLAN;

			} else {
				dev_err(&pf->pdev->dev, "Bad vlan mask 0x%04x\n",
					match.mask->vlan_id);
				return -EIO;
			}
		}

		filter->vlan_id = cpu_to_be16(match.key->vlan_id);
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_CONTROL)) {
		struct flow_match_control match;

		flow_rule_match_control(rule, &match);
		addr_type = match.key->addr_type;
	}

	if (addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS) {
		struct flow_match_ipv4_addrs match;

		flow_rule_match_ipv4_addrs(rule, &match);
		if (match.mask->dst) {
			if (match.mask->dst == cpu_to_be32(0xffffffff)) {
				field_flags |= I40E_CLOUD_FIELD_IIP;
			} else {
				dev_err(&pf->pdev->dev, "Bad ip dst mask %pI4b\n",
					&match.mask->dst);
				return -EIO;
			}
		}

		if (match.mask->src) {
			if (match.mask->src == cpu_to_be32(0xffffffff)) {
				field_flags |= I40E_CLOUD_FIELD_IIP;
			} else {
				dev_err(&pf->pdev->dev, "Bad ip src mask %pI4b\n",
					&match.mask->src);
				return -EIO;
			}
		}

		if (field_flags & I40E_CLOUD_FIELD_TEN_ID) {
			dev_err(&pf->pdev->dev, "Tenant id not allowed for ip filter\n");
			return -EIO;
		}
		filter->dst_ipv4 = match.key->dst;
		filter->src_ipv4 = match.key->src;
	}

	if (addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS) {
		struct flow_match_ipv6_addrs match;

		flow_rule_match_ipv6_addrs(rule, &match);

		 
		if (ipv6_addr_loopback(&match.key->dst) ||
		    ipv6_addr_loopback(&match.key->src)) {
			dev_err(&pf->pdev->dev,
				"Bad ipv6, addr is LOOPBACK\n");
			return -EIO;
		}
		if (!ipv6_addr_any(&match.mask->dst) ||
		    !ipv6_addr_any(&match.mask->src))
			field_flags |= I40E_CLOUD_FIELD_IIP;

		memcpy(&filter->src_ipv6, &match.key->src.s6_addr32,
		       sizeof(filter->src_ipv6));
		memcpy(&filter->dst_ipv6, &match.key->dst.s6_addr32,
		       sizeof(filter->dst_ipv6));
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_PORTS)) {
		struct flow_match_ports match;

		flow_rule_match_ports(rule, &match);
		if (match.mask->src) {
			if (match.mask->src == cpu_to_be16(0xffff)) {
				field_flags |= I40E_CLOUD_FIELD_IIP;
			} else {
				dev_err(&pf->pdev->dev, "Bad src port mask 0x%04x\n",
					be16_to_cpu(match.mask->src));
				return -EIO;
			}
		}

		if (match.mask->dst) {
			if (match.mask->dst == cpu_to_be16(0xffff)) {
				field_flags |= I40E_CLOUD_FIELD_IIP;
			} else {
				dev_err(&pf->pdev->dev, "Bad dst port mask 0x%04x\n",
					be16_to_cpu(match.mask->dst));
				return -EIO;
			}
		}

		filter->dst_port = match.key->dst;
		filter->src_port = match.key->src;

		switch (filter->ip_proto) {
		case IPPROTO_TCP:
		case IPPROTO_UDP:
			break;
		default:
			dev_err(&pf->pdev->dev,
				"Only UDP and TCP transport are supported\n");
			return -EINVAL;
		}
	}
	filter->flags = field_flags;
	return 0;
}

 
static int i40e_handle_tclass(struct i40e_vsi *vsi, u32 tc,
			      struct i40e_cloud_filter *filter)
{
	struct i40e_channel *ch, *ch_tmp;

	 
	if (tc == 0) {
		filter->seid = vsi->seid;
		return 0;
	} else if (vsi->tc_config.enabled_tc & BIT(tc)) {
		if (!filter->dst_port) {
			dev_err(&vsi->back->pdev->dev,
				"Specify destination port to direct to traffic class that is not default\n");
			return -EINVAL;
		}
		if (list_empty(&vsi->ch_list))
			return -EINVAL;
		list_for_each_entry_safe(ch, ch_tmp, &vsi->ch_list,
					 list) {
			if (ch->seid == vsi->tc_seid_map[tc])
				filter->seid = ch->seid;
		}
		return 0;
	}
	dev_err(&vsi->back->pdev->dev, "TC is not enabled\n");
	return -EINVAL;
}

 
static int i40e_configure_clsflower(struct i40e_vsi *vsi,
				    struct flow_cls_offload *cls_flower)
{
	int tc = tc_classid_to_hwtc(vsi->netdev, cls_flower->classid);
	struct i40e_cloud_filter *filter = NULL;
	struct i40e_pf *pf = vsi->back;
	int err = 0;

	if (tc < 0) {
		dev_err(&vsi->back->pdev->dev, "Invalid traffic class\n");
		return -EOPNOTSUPP;
	}

	if (!tc) {
		dev_err(&pf->pdev->dev, "Unable to add filter because of invalid destination");
		return -EINVAL;
	}

	if (test_bit(__I40E_RESET_RECOVERY_PENDING, pf->state) ||
	    test_bit(__I40E_RESET_INTR_RECEIVED, pf->state))
		return -EBUSY;

	if (pf->fdir_pf_active_filters ||
	    (!hlist_empty(&pf->fdir_filter_list))) {
		dev_err(&vsi->back->pdev->dev,
			"Flow Director Sideband filters exists, turn ntuple off to configure cloud filters\n");
		return -EINVAL;
	}

	if (vsi->back->flags & I40E_FLAG_FD_SB_ENABLED) {
		dev_err(&vsi->back->pdev->dev,
			"Disable Flow Director Sideband, configuring Cloud filters via tc-flower\n");
		vsi->back->flags &= ~I40E_FLAG_FD_SB_ENABLED;
		vsi->back->flags |= I40E_FLAG_FD_SB_TO_CLOUD_FILTER;
	}

	filter = kzalloc(sizeof(*filter), GFP_KERNEL);
	if (!filter)
		return -ENOMEM;

	filter->cookie = cls_flower->cookie;

	err = i40e_parse_cls_flower(vsi, cls_flower, filter);
	if (err < 0)
		goto err;

	err = i40e_handle_tclass(vsi, tc, filter);
	if (err < 0)
		goto err;

	 
	if (filter->dst_port)
		err = i40e_add_del_cloud_filter_big_buf(vsi, filter, true);
	else
		err = i40e_add_del_cloud_filter(vsi, filter, true);

	if (err) {
		dev_err(&pf->pdev->dev, "Failed to add cloud filter, err %d\n",
			err);
		goto err;
	}

	 
	INIT_HLIST_NODE(&filter->cloud_node);

	hlist_add_head(&filter->cloud_node, &pf->cloud_filter_list);

	pf->num_cloud_filters++;

	return err;
err:
	kfree(filter);
	return err;
}

 
static struct i40e_cloud_filter *i40e_find_cloud_filter(struct i40e_vsi *vsi,
							unsigned long *cookie)
{
	struct i40e_cloud_filter *filter = NULL;
	struct hlist_node *node2;

	hlist_for_each_entry_safe(filter, node2,
				  &vsi->back->cloud_filter_list, cloud_node)
		if (!memcmp(cookie, &filter->cookie, sizeof(filter->cookie)))
			return filter;
	return NULL;
}

 
static int i40e_delete_clsflower(struct i40e_vsi *vsi,
				 struct flow_cls_offload *cls_flower)
{
	struct i40e_cloud_filter *filter = NULL;
	struct i40e_pf *pf = vsi->back;
	int err = 0;

	filter = i40e_find_cloud_filter(vsi, &cls_flower->cookie);

	if (!filter)
		return -EINVAL;

	hash_del(&filter->cloud_node);

	if (filter->dst_port)
		err = i40e_add_del_cloud_filter_big_buf(vsi, filter, false);
	else
		err = i40e_add_del_cloud_filter(vsi, filter, false);

	kfree(filter);
	if (err) {
		dev_err(&pf->pdev->dev,
			"Failed to delete cloud filter, err %pe\n",
			ERR_PTR(err));
		return i40e_aq_rc_to_posix(err, pf->hw.aq.asq_last_status);
	}

	pf->num_cloud_filters--;
	if (!pf->num_cloud_filters)
		if ((pf->flags & I40E_FLAG_FD_SB_TO_CLOUD_FILTER) &&
		    !(pf->flags & I40E_FLAG_FD_SB_INACTIVE)) {
			pf->flags |= I40E_FLAG_FD_SB_ENABLED;
			pf->flags &= ~I40E_FLAG_FD_SB_TO_CLOUD_FILTER;
			pf->flags &= ~I40E_FLAG_FD_SB_INACTIVE;
		}
	return 0;
}

 
static int i40e_setup_tc_cls_flower(struct i40e_netdev_priv *np,
				    struct flow_cls_offload *cls_flower)
{
	struct i40e_vsi *vsi = np->vsi;

	switch (cls_flower->command) {
	case FLOW_CLS_REPLACE:
		return i40e_configure_clsflower(vsi, cls_flower);
	case FLOW_CLS_DESTROY:
		return i40e_delete_clsflower(vsi, cls_flower);
	case FLOW_CLS_STATS:
		return -EOPNOTSUPP;
	default:
		return -EOPNOTSUPP;
	}
}

static int i40e_setup_tc_block_cb(enum tc_setup_type type, void *type_data,
				  void *cb_priv)
{
	struct i40e_netdev_priv *np = cb_priv;

	if (!tc_cls_can_offload_and_chain0(np->vsi->netdev, type_data))
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		return i40e_setup_tc_cls_flower(np, type_data);

	default:
		return -EOPNOTSUPP;
	}
}

static LIST_HEAD(i40e_block_cb_list);

static int __i40e_setup_tc(struct net_device *netdev, enum tc_setup_type type,
			   void *type_data)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);

	switch (type) {
	case TC_SETUP_QDISC_MQPRIO:
		return i40e_setup_tc(netdev, type_data);
	case TC_SETUP_BLOCK:
		return flow_block_cb_setup_simple(type_data,
						  &i40e_block_cb_list,
						  i40e_setup_tc_block_cb,
						  np, np, true);
	default:
		return -EOPNOTSUPP;
	}
}

 
int i40e_open(struct net_device *netdev)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	int err;

	 
	if (test_bit(__I40E_TESTING, pf->state) ||
	    test_bit(__I40E_BAD_EEPROM, pf->state))
		return -EBUSY;

	netif_carrier_off(netdev);

	if (i40e_force_link_state(pf, true))
		return -EAGAIN;

	err = i40e_vsi_open(vsi);
	if (err)
		return err;

	 
	wr32(&pf->hw, I40E_GLLAN_TSOMSK_F, be32_to_cpu(TCP_FLAG_PSH |
						       TCP_FLAG_FIN) >> 16);
	wr32(&pf->hw, I40E_GLLAN_TSOMSK_M, be32_to_cpu(TCP_FLAG_PSH |
						       TCP_FLAG_FIN |
						       TCP_FLAG_CWR) >> 16);
	wr32(&pf->hw, I40E_GLLAN_TSOMSK_L, be32_to_cpu(TCP_FLAG_CWR) >> 16);
	udp_tunnel_get_rx_info(netdev);

	return 0;
}

 
static int i40e_netif_set_realnum_tx_rx_queues(struct i40e_vsi *vsi)
{
	int ret;

	ret = netif_set_real_num_rx_queues(vsi->netdev,
					   vsi->num_queue_pairs);
	if (ret)
		return ret;

	return netif_set_real_num_tx_queues(vsi->netdev,
					    vsi->num_queue_pairs);
}

 
int i40e_vsi_open(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	char int_name[I40E_INT_NAME_STR_LEN];
	int err;

	 
	err = i40e_vsi_setup_tx_resources(vsi);
	if (err)
		goto err_setup_tx;
	err = i40e_vsi_setup_rx_resources(vsi);
	if (err)
		goto err_setup_rx;

	err = i40e_vsi_configure(vsi);
	if (err)
		goto err_setup_rx;

	if (vsi->netdev) {
		snprintf(int_name, sizeof(int_name) - 1, "%s-%s",
			 dev_driver_string(&pf->pdev->dev), vsi->netdev->name);
		err = i40e_vsi_request_irq(vsi, int_name);
		if (err)
			goto err_setup_rx;

		 
		err = i40e_netif_set_realnum_tx_rx_queues(vsi);
		if (err)
			goto err_set_queues;

	} else if (vsi->type == I40E_VSI_FDIR) {
		snprintf(int_name, sizeof(int_name) - 1, "%s-%s:fdir",
			 dev_driver_string(&pf->pdev->dev),
			 dev_name(&pf->pdev->dev));
		err = i40e_vsi_request_irq(vsi, int_name);
		if (err)
			goto err_setup_rx;

	} else {
		err = -EINVAL;
		goto err_setup_rx;
	}

	err = i40e_up_complete(vsi);
	if (err)
		goto err_up_complete;

	return 0;

err_up_complete:
	i40e_down(vsi);
err_set_queues:
	i40e_vsi_free_irq(vsi);
err_setup_rx:
	i40e_vsi_free_rx_resources(vsi);
err_setup_tx:
	i40e_vsi_free_tx_resources(vsi);
	if (vsi == pf->vsi[pf->lan_vsi])
		i40e_do_reset(pf, I40E_PF_RESET_FLAG, true);

	return err;
}

 
static void i40e_fdir_filter_exit(struct i40e_pf *pf)
{
	struct i40e_fdir_filter *filter;
	struct i40e_flex_pit *pit_entry, *tmp;
	struct hlist_node *node2;

	hlist_for_each_entry_safe(filter, node2,
				  &pf->fdir_filter_list, fdir_node) {
		hlist_del(&filter->fdir_node);
		kfree(filter);
	}

	list_for_each_entry_safe(pit_entry, tmp, &pf->l3_flex_pit_list, list) {
		list_del(&pit_entry->list);
		kfree(pit_entry);
	}
	INIT_LIST_HEAD(&pf->l3_flex_pit_list);

	list_for_each_entry_safe(pit_entry, tmp, &pf->l4_flex_pit_list, list) {
		list_del(&pit_entry->list);
		kfree(pit_entry);
	}
	INIT_LIST_HEAD(&pf->l4_flex_pit_list);

	pf->fdir_pf_active_filters = 0;
	i40e_reset_fdir_filter_cnt(pf);

	 
	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV4_TCP,
				I40E_L3_SRC_MASK | I40E_L3_DST_MASK |
				I40E_L4_SRC_MASK | I40E_L4_DST_MASK);

	 
	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV6_TCP,
				I40E_L3_V6_SRC_MASK | I40E_L3_V6_DST_MASK |
				I40E_L4_SRC_MASK | I40E_L4_DST_MASK);

	 
	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV4_UDP,
				I40E_L3_SRC_MASK | I40E_L3_DST_MASK |
				I40E_L4_SRC_MASK | I40E_L4_DST_MASK);

	 
	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV6_UDP,
				I40E_L3_V6_SRC_MASK | I40E_L3_V6_DST_MASK |
				I40E_L4_SRC_MASK | I40E_L4_DST_MASK);

	 
	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV4_SCTP,
				I40E_L3_SRC_MASK | I40E_L3_DST_MASK |
				I40E_L4_SRC_MASK | I40E_L4_DST_MASK);

	 
	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV6_SCTP,
				I40E_L3_V6_SRC_MASK | I40E_L3_V6_DST_MASK |
				I40E_L4_SRC_MASK | I40E_L4_DST_MASK);

	 
	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV4_OTHER,
				I40E_L3_SRC_MASK | I40E_L3_DST_MASK);

	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_FRAG_IPV4,
				I40E_L3_SRC_MASK | I40E_L3_DST_MASK);

	 
	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV6_OTHER,
				I40E_L3_SRC_MASK | I40E_L3_DST_MASK);

	i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_FRAG_IPV6,
				I40E_L3_SRC_MASK | I40E_L3_DST_MASK);
}

 
static void i40e_cloud_filter_exit(struct i40e_pf *pf)
{
	struct i40e_cloud_filter *cfilter;
	struct hlist_node *node;

	hlist_for_each_entry_safe(cfilter, node,
				  &pf->cloud_filter_list, cloud_node) {
		hlist_del(&cfilter->cloud_node);
		kfree(cfilter);
	}
	pf->num_cloud_filters = 0;

	if ((pf->flags & I40E_FLAG_FD_SB_TO_CLOUD_FILTER) &&
	    !(pf->flags & I40E_FLAG_FD_SB_INACTIVE)) {
		pf->flags |= I40E_FLAG_FD_SB_ENABLED;
		pf->flags &= ~I40E_FLAG_FD_SB_TO_CLOUD_FILTER;
		pf->flags &= ~I40E_FLAG_FD_SB_INACTIVE;
	}
}

 
int i40e_close(struct net_device *netdev)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;

	i40e_vsi_close(vsi);

	return 0;
}

 
void i40e_do_reset(struct i40e_pf *pf, u32 reset_flags, bool lock_acquired)
{
	u32 val;

	 
	if (reset_flags & BIT_ULL(__I40E_GLOBAL_RESET_REQUESTED)) {

		 
		dev_dbg(&pf->pdev->dev, "GlobalR requested\n");
		val = rd32(&pf->hw, I40E_GLGEN_RTRIG);
		val |= I40E_GLGEN_RTRIG_GLOBR_MASK;
		wr32(&pf->hw, I40E_GLGEN_RTRIG, val);

	} else if (reset_flags & BIT_ULL(__I40E_CORE_RESET_REQUESTED)) {

		 
		dev_dbg(&pf->pdev->dev, "CoreR requested\n");
		val = rd32(&pf->hw, I40E_GLGEN_RTRIG);
		val |= I40E_GLGEN_RTRIG_CORER_MASK;
		wr32(&pf->hw, I40E_GLGEN_RTRIG, val);
		i40e_flush(&pf->hw);

	} else if (reset_flags & I40E_PF_RESET_FLAG) {

		 
		dev_dbg(&pf->pdev->dev, "PFR requested\n");
		i40e_handle_reset_warning(pf, lock_acquired);

	} else if (reset_flags & I40E_PF_RESET_AND_REBUILD_FLAG) {
		 
		i40e_prep_for_reset(pf);
		i40e_reset_and_rebuild(pf, true, lock_acquired);
		dev_info(&pf->pdev->dev,
			 pf->flags & I40E_FLAG_DISABLE_FW_LLDP ?
			 "FW LLDP is disabled\n" :
			 "FW LLDP is enabled\n");

	} else if (reset_flags & BIT_ULL(__I40E_REINIT_REQUESTED)) {
		int v;

		 
		dev_info(&pf->pdev->dev,
			 "VSI reinit requested\n");
		for (v = 0; v < pf->num_alloc_vsi; v++) {
			struct i40e_vsi *vsi = pf->vsi[v];

			if (vsi != NULL &&
			    test_and_clear_bit(__I40E_VSI_REINIT_REQUESTED,
					       vsi->state))
				i40e_vsi_reinit_locked(pf->vsi[v]);
		}
	} else if (reset_flags & BIT_ULL(__I40E_DOWN_REQUESTED)) {
		int v;

		 
		dev_info(&pf->pdev->dev, "VSI down requested\n");
		for (v = 0; v < pf->num_alloc_vsi; v++) {
			struct i40e_vsi *vsi = pf->vsi[v];

			if (vsi != NULL &&
			    test_and_clear_bit(__I40E_VSI_DOWN_REQUESTED,
					       vsi->state)) {
				set_bit(__I40E_VSI_DOWN, vsi->state);
				i40e_down(vsi);
			}
		}
	} else {
		dev_info(&pf->pdev->dev,
			 "bad reset request 0x%08x\n", reset_flags);
	}
}

#ifdef CONFIG_I40E_DCB
 
bool i40e_dcb_need_reconfig(struct i40e_pf *pf,
			    struct i40e_dcbx_config *old_cfg,
			    struct i40e_dcbx_config *new_cfg)
{
	bool need_reconfig = false;

	 
	if (memcmp(&new_cfg->etscfg,
		   &old_cfg->etscfg,
		   sizeof(new_cfg->etscfg))) {
		 
		if (memcmp(&new_cfg->etscfg.prioritytable,
			   &old_cfg->etscfg.prioritytable,
			   sizeof(new_cfg->etscfg.prioritytable))) {
			need_reconfig = true;
			dev_dbg(&pf->pdev->dev, "ETS UP2TC changed.\n");
		}

		if (memcmp(&new_cfg->etscfg.tcbwtable,
			   &old_cfg->etscfg.tcbwtable,
			   sizeof(new_cfg->etscfg.tcbwtable)))
			dev_dbg(&pf->pdev->dev, "ETS TC BW Table changed.\n");

		if (memcmp(&new_cfg->etscfg.tsatable,
			   &old_cfg->etscfg.tsatable,
			   sizeof(new_cfg->etscfg.tsatable)))
			dev_dbg(&pf->pdev->dev, "ETS TSA Table changed.\n");
	}

	 
	if (memcmp(&new_cfg->pfc,
		   &old_cfg->pfc,
		   sizeof(new_cfg->pfc))) {
		need_reconfig = true;
		dev_dbg(&pf->pdev->dev, "PFC config change detected.\n");
	}

	 
	if (memcmp(&new_cfg->app,
		   &old_cfg->app,
		   sizeof(new_cfg->app))) {
		need_reconfig = true;
		dev_dbg(&pf->pdev->dev, "APP Table change detected.\n");
	}

	dev_dbg(&pf->pdev->dev, "dcb need_reconfig=%d\n", need_reconfig);
	return need_reconfig;
}

 
static int i40e_handle_lldp_event(struct i40e_pf *pf,
				  struct i40e_arq_event_info *e)
{
	struct i40e_aqc_lldp_get_mib *mib =
		(struct i40e_aqc_lldp_get_mib *)&e->desc.params.raw;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_dcbx_config tmp_dcbx_cfg;
	bool need_reconfig = false;
	int ret = 0;
	u8 type;

	 
	if (I40E_IS_X710TL_DEVICE(hw->device_id) &&
	    (hw->phy.link_info.link_speed &
	     ~(I40E_LINK_SPEED_2_5GB | I40E_LINK_SPEED_5GB)) &&
	     !(pf->flags & I40E_FLAG_DCB_CAPABLE))
		 
		pf->flags |= I40E_FLAG_DCB_CAPABLE;

	 
	if (!(pf->flags & I40E_FLAG_DCB_CAPABLE))
		return ret;

	 
	type = ((mib->type >> I40E_AQ_LLDP_BRIDGE_TYPE_SHIFT)
		& I40E_AQ_LLDP_BRIDGE_TYPE_MASK);
	dev_dbg(&pf->pdev->dev, "LLDP event mib bridge type 0x%x\n", type);
	if (type != I40E_AQ_LLDP_BRIDGE_TYPE_NEAREST_BRIDGE)
		return ret;

	 
	type = mib->type & I40E_AQ_LLDP_MIB_TYPE_MASK;
	dev_dbg(&pf->pdev->dev,
		"LLDP event mib type %s\n", type ? "remote" : "local");
	if (type == I40E_AQ_LLDP_MIB_REMOTE) {
		 
		ret = i40e_aq_get_dcb_config(hw, I40E_AQ_LLDP_MIB_REMOTE,
				I40E_AQ_LLDP_BRIDGE_TYPE_NEAREST_BRIDGE,
				&hw->remote_dcbx_config);
		goto exit;
	}

	 
	tmp_dcbx_cfg = hw->local_dcbx_config;

	 
	memset(&hw->local_dcbx_config, 0, sizeof(hw->local_dcbx_config));
	 
	ret = i40e_get_dcb_config(&pf->hw);
	if (ret) {
		 
		if (I40E_IS_X710TL_DEVICE(hw->device_id) &&
		    (hw->phy.link_info.link_speed &
		     (I40E_LINK_SPEED_2_5GB | I40E_LINK_SPEED_5GB))) {
			dev_warn(&pf->pdev->dev,
				 "DCB is not supported for X710-T*L 2.5/5G speeds\n");
			pf->flags &= ~I40E_FLAG_DCB_CAPABLE;
		} else {
			dev_info(&pf->pdev->dev,
				 "Failed querying DCB configuration data from firmware, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
		}
		goto exit;
	}

	 
	if (!memcmp(&tmp_dcbx_cfg, &hw->local_dcbx_config,
		    sizeof(tmp_dcbx_cfg))) {
		dev_dbg(&pf->pdev->dev, "No change detected in DCBX configuration.\n");
		goto exit;
	}

	need_reconfig = i40e_dcb_need_reconfig(pf, &tmp_dcbx_cfg,
					       &hw->local_dcbx_config);

	i40e_dcbnl_flush_apps(pf, &tmp_dcbx_cfg, &hw->local_dcbx_config);

	if (!need_reconfig)
		goto exit;

	 
	if (i40e_dcb_get_num_tc(&hw->local_dcbx_config) > 1)
		pf->flags |= I40E_FLAG_DCB_ENABLED;
	else
		pf->flags &= ~I40E_FLAG_DCB_ENABLED;

	set_bit(__I40E_PORT_SUSPENDED, pf->state);
	 
	i40e_pf_quiesce_all_vsi(pf);

	 
	i40e_dcb_reconfigure(pf);

	ret = i40e_resume_port_tx(pf);

	clear_bit(__I40E_PORT_SUSPENDED, pf->state);
	 
	if (ret)
		goto exit;

	 
	ret = i40e_pf_wait_queues_disabled(pf);
	if (ret) {
		 
		set_bit(__I40E_PF_RESET_REQUESTED, pf->state);
		i40e_service_event_schedule(pf);
	} else {
		i40e_pf_unquiesce_all_vsi(pf);
		set_bit(__I40E_CLIENT_SERVICE_REQUESTED, pf->state);
		set_bit(__I40E_CLIENT_L2_CHANGE, pf->state);
	}

exit:
	return ret;
}
#endif  

 
void i40e_do_reset_safe(struct i40e_pf *pf, u32 reset_flags)
{
	rtnl_lock();
	i40e_do_reset(pf, reset_flags, true);
	rtnl_unlock();
}

 
static void i40e_handle_lan_overflow_event(struct i40e_pf *pf,
					   struct i40e_arq_event_info *e)
{
	struct i40e_aqc_lan_overflow *data =
		(struct i40e_aqc_lan_overflow *)&e->desc.params.raw;
	u32 queue = le32_to_cpu(data->prtdcb_rupto);
	u32 qtx_ctl = le32_to_cpu(data->otx_ctl);
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vf *vf;
	u16 vf_id;

	dev_dbg(&pf->pdev->dev, "overflow Rx Queue Number = %d QTX_CTL=0x%08x\n",
		queue, qtx_ctl);

	 
	if (((qtx_ctl & I40E_QTX_CTL_PFVF_Q_MASK)
	    >> I40E_QTX_CTL_PFVF_Q_SHIFT) == I40E_QTX_CTL_VF_QUEUE) {
		vf_id = (u16)((qtx_ctl & I40E_QTX_CTL_VFVM_INDX_MASK)
			 >> I40E_QTX_CTL_VFVM_INDX_SHIFT);
		vf_id -= hw->func_caps.vf_base_id;
		vf = &pf->vf[vf_id];
		i40e_vc_notify_vf_reset(vf);
		 
		msleep(20);
		i40e_reset_vf(vf, false);
	}
}

 
u32 i40e_get_cur_guaranteed_fd_count(struct i40e_pf *pf)
{
	u32 val, fcnt_prog;

	val = rd32(&pf->hw, I40E_PFQF_FDSTAT);
	fcnt_prog = (val & I40E_PFQF_FDSTAT_GUARANT_CNT_MASK);
	return fcnt_prog;
}

 
u32 i40e_get_current_fd_count(struct i40e_pf *pf)
{
	u32 val, fcnt_prog;

	val = rd32(&pf->hw, I40E_PFQF_FDSTAT);
	fcnt_prog = (val & I40E_PFQF_FDSTAT_GUARANT_CNT_MASK) +
		    ((val & I40E_PFQF_FDSTAT_BEST_CNT_MASK) >>
		      I40E_PFQF_FDSTAT_BEST_CNT_SHIFT);
	return fcnt_prog;
}

 
u32 i40e_get_global_fd_count(struct i40e_pf *pf)
{
	u32 val, fcnt_prog;

	val = rd32(&pf->hw, I40E_GLQF_FDCNT_0);
	fcnt_prog = (val & I40E_GLQF_FDCNT_0_GUARANT_CNT_MASK) +
		    ((val & I40E_GLQF_FDCNT_0_BESTCNT_MASK) >>
		     I40E_GLQF_FDCNT_0_BESTCNT_SHIFT);
	return fcnt_prog;
}

 
static void i40e_reenable_fdir_sb(struct i40e_pf *pf)
{
	if (test_and_clear_bit(__I40E_FD_SB_AUTO_DISABLED, pf->state))
		if ((pf->flags & I40E_FLAG_FD_SB_ENABLED) &&
		    (I40E_DEBUG_FD & pf->hw.debug_mask))
			dev_info(&pf->pdev->dev, "FD Sideband/ntuple is being enabled since we have space in the table now\n");
}

 
static void i40e_reenable_fdir_atr(struct i40e_pf *pf)
{
	if (test_and_clear_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state)) {
		 
		i40e_write_fd_input_set(pf, I40E_FILTER_PCTYPE_NONF_IPV4_TCP,
					I40E_L3_SRC_MASK | I40E_L3_DST_MASK |
					I40E_L4_SRC_MASK | I40E_L4_DST_MASK);

		if ((pf->flags & I40E_FLAG_FD_ATR_ENABLED) &&
		    (I40E_DEBUG_FD & pf->hw.debug_mask))
			dev_info(&pf->pdev->dev, "ATR is being enabled since we have space in the table and there are no conflicting ntuple rules\n");
	}
}

 
static void i40e_delete_invalid_filter(struct i40e_pf *pf,
				       struct i40e_fdir_filter *filter)
{
	 
	pf->fdir_pf_active_filters--;
	pf->fd_inv = 0;

	switch (filter->flow_type) {
	case TCP_V4_FLOW:
		pf->fd_tcp4_filter_cnt--;
		break;
	case UDP_V4_FLOW:
		pf->fd_udp4_filter_cnt--;
		break;
	case SCTP_V4_FLOW:
		pf->fd_sctp4_filter_cnt--;
		break;
	case TCP_V6_FLOW:
		pf->fd_tcp6_filter_cnt--;
		break;
	case UDP_V6_FLOW:
		pf->fd_udp6_filter_cnt--;
		break;
	case SCTP_V6_FLOW:
		pf->fd_udp6_filter_cnt--;
		break;
	case IP_USER_FLOW:
		switch (filter->ipl4_proto) {
		case IPPROTO_TCP:
			pf->fd_tcp4_filter_cnt--;
			break;
		case IPPROTO_UDP:
			pf->fd_udp4_filter_cnt--;
			break;
		case IPPROTO_SCTP:
			pf->fd_sctp4_filter_cnt--;
			break;
		case IPPROTO_IP:
			pf->fd_ip4_filter_cnt--;
			break;
		}
		break;
	case IPV6_USER_FLOW:
		switch (filter->ipl4_proto) {
		case IPPROTO_TCP:
			pf->fd_tcp6_filter_cnt--;
			break;
		case IPPROTO_UDP:
			pf->fd_udp6_filter_cnt--;
			break;
		case IPPROTO_SCTP:
			pf->fd_sctp6_filter_cnt--;
			break;
		case IPPROTO_IP:
			pf->fd_ip6_filter_cnt--;
			break;
		}
		break;
	}

	 
	hlist_del(&filter->fdir_node);
	kfree(filter);
}

 
void i40e_fdir_check_and_reenable(struct i40e_pf *pf)
{
	struct i40e_fdir_filter *filter;
	u32 fcnt_prog, fcnt_avail;
	struct hlist_node *node;

	if (test_bit(__I40E_FD_FLUSH_REQUESTED, pf->state))
		return;

	 
	fcnt_prog = i40e_get_global_fd_count(pf);
	fcnt_avail = pf->fdir_pf_filter_count;
	if ((fcnt_prog < (fcnt_avail - I40E_FDIR_BUFFER_HEAD_ROOM)) ||
	    (pf->fd_add_err == 0) ||
	    (i40e_get_current_atr_cnt(pf) < pf->fd_atr_cnt))
		i40e_reenable_fdir_sb(pf);

	 
	if ((fcnt_prog < (fcnt_avail - I40E_FDIR_BUFFER_HEAD_ROOM_FOR_ATR)) &&
	    pf->fd_tcp4_filter_cnt == 0 && pf->fd_tcp6_filter_cnt == 0)
		i40e_reenable_fdir_atr(pf);

	 
	if (pf->fd_inv > 0) {
		hlist_for_each_entry_safe(filter, node,
					  &pf->fdir_filter_list, fdir_node)
			if (filter->fd_id == pf->fd_inv)
				i40e_delete_invalid_filter(pf, filter);
	}
}

#define I40E_MIN_FD_FLUSH_INTERVAL 10
#define I40E_MIN_FD_FLUSH_SB_ATR_UNSTABLE 30
 
static void i40e_fdir_flush_and_replay(struct i40e_pf *pf)
{
	unsigned long min_flush_time;
	int flush_wait_retry = 50;
	bool disable_atr = false;
	int fd_room;
	int reg;

	if (!time_after(jiffies, pf->fd_flush_timestamp +
				 (I40E_MIN_FD_FLUSH_INTERVAL * HZ)))
		return;

	 
	min_flush_time = pf->fd_flush_timestamp +
			 (I40E_MIN_FD_FLUSH_SB_ATR_UNSTABLE * HZ);
	fd_room = pf->fdir_pf_filter_count - pf->fdir_pf_active_filters;

	if (!(time_after(jiffies, min_flush_time)) &&
	    (fd_room < I40E_FDIR_BUFFER_HEAD_ROOM_FOR_ATR)) {
		if (I40E_DEBUG_FD & pf->hw.debug_mask)
			dev_info(&pf->pdev->dev, "ATR disabled, not enough FD filter space.\n");
		disable_atr = true;
	}

	pf->fd_flush_timestamp = jiffies;
	set_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state);
	 
	wr32(&pf->hw, I40E_PFQF_CTL_1,
	     I40E_PFQF_CTL_1_CLEARFDTABLE_MASK);
	i40e_flush(&pf->hw);
	pf->fd_flush_cnt++;
	pf->fd_add_err = 0;
	do {
		 
		usleep_range(5000, 6000);
		reg = rd32(&pf->hw, I40E_PFQF_CTL_1);
		if (!(reg & I40E_PFQF_CTL_1_CLEARFDTABLE_MASK))
			break;
	} while (flush_wait_retry--);
	if (reg & I40E_PFQF_CTL_1_CLEARFDTABLE_MASK) {
		dev_warn(&pf->pdev->dev, "FD table did not flush, needs more time\n");
	} else {
		 
		i40e_fdir_filter_restore(pf->vsi[pf->lan_vsi]);
		if (!disable_atr && !pf->fd_tcp4_filter_cnt)
			clear_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state);
		clear_bit(__I40E_FD_FLUSH_REQUESTED, pf->state);
		if (I40E_DEBUG_FD & pf->hw.debug_mask)
			dev_info(&pf->pdev->dev, "FD Filter table flushed and FD-SB replayed.\n");
	}
}

 
u32 i40e_get_current_atr_cnt(struct i40e_pf *pf)
{
	return i40e_get_current_fd_count(pf) - pf->fdir_pf_active_filters;
}

 
static void i40e_fdir_reinit_subtask(struct i40e_pf *pf)
{

	 
	if (test_bit(__I40E_DOWN, pf->state))
		return;

	if (test_bit(__I40E_FD_FLUSH_REQUESTED, pf->state))
		i40e_fdir_flush_and_replay(pf);

	i40e_fdir_check_and_reenable(pf);

}

 
static void i40e_vsi_link_event(struct i40e_vsi *vsi, bool link_up)
{
	if (!vsi || test_bit(__I40E_VSI_DOWN, vsi->state))
		return;

	switch (vsi->type) {
	case I40E_VSI_MAIN:
		if (!vsi->netdev || !vsi->netdev_registered)
			break;

		if (link_up) {
			netif_carrier_on(vsi->netdev);
			netif_tx_wake_all_queues(vsi->netdev);
		} else {
			netif_carrier_off(vsi->netdev);
			netif_tx_stop_all_queues(vsi->netdev);
		}
		break;

	case I40E_VSI_SRIOV:
	case I40E_VSI_VMDQ2:
	case I40E_VSI_CTRL:
	case I40E_VSI_IWARP:
	case I40E_VSI_MIRROR:
	default:
		 
		break;
	}
}

 
static void i40e_veb_link_event(struct i40e_veb *veb, bool link_up)
{
	struct i40e_pf *pf;
	int i;

	if (!veb || !veb->pf)
		return;
	pf = veb->pf;

	 
	for (i = 0; i < I40E_MAX_VEB; i++)
		if (pf->veb[i] && (pf->veb[i]->uplink_seid == veb->seid))
			i40e_veb_link_event(pf->veb[i], link_up);

	 
	for (i = 0; i < pf->num_alloc_vsi; i++)
		if (pf->vsi[i] && (pf->vsi[i]->uplink_seid == veb->seid))
			i40e_vsi_link_event(pf->vsi[i], link_up);
}

 
static void i40e_link_event(struct i40e_pf *pf)
{
	struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
	u8 new_link_speed, old_link_speed;
	bool new_link, old_link;
	int status;
#ifdef CONFIG_I40E_DCB
	int err;
#endif  

	 
	pf->hw.phy.get_link_info = true;
	old_link = (pf->hw.phy.link_info_old.link_info & I40E_AQ_LINK_UP);
	status = i40e_get_link_status(&pf->hw, &new_link);

	 
	if (status == 0) {
		clear_bit(__I40E_TEMP_LINK_POLLING, pf->state);
	} else {
		 
		set_bit(__I40E_TEMP_LINK_POLLING, pf->state);
		dev_dbg(&pf->pdev->dev, "couldn't get link state, status: %d\n",
			status);
		return;
	}

	old_link_speed = pf->hw.phy.link_info_old.link_speed;
	new_link_speed = pf->hw.phy.link_info.link_speed;

	if (new_link == old_link &&
	    new_link_speed == old_link_speed &&
	    (test_bit(__I40E_VSI_DOWN, vsi->state) ||
	     new_link == netif_carrier_ok(vsi->netdev)))
		return;

	i40e_print_link_message(vsi, new_link);

	 
	if (pf->lan_veb < I40E_MAX_VEB && pf->veb[pf->lan_veb])
		i40e_veb_link_event(pf->veb[pf->lan_veb], new_link);
	else
		i40e_vsi_link_event(vsi, new_link);

	if (pf->vf)
		i40e_vc_notify_link_state(pf);

	if (pf->flags & I40E_FLAG_PTP)
		i40e_ptp_set_increment(pf);
#ifdef CONFIG_I40E_DCB
	if (new_link == old_link)
		return;
	 
	if (pf->dcbx_cap & DCB_CAP_DCBX_LLD_MANAGED)
		return;

	 
	if (!new_link) {
		dev_dbg(&pf->pdev->dev, "Reconfig DCB to single TC as result of Link Down\n");
		memset(&pf->tmp_cfg, 0, sizeof(pf->tmp_cfg));
		err = i40e_dcb_sw_default_config(pf);
		if (err) {
			pf->flags &= ~(I40E_FLAG_DCB_CAPABLE |
				       I40E_FLAG_DCB_ENABLED);
		} else {
			pf->dcbx_cap = DCB_CAP_DCBX_HOST |
				       DCB_CAP_DCBX_VER_IEEE;
			pf->flags |= I40E_FLAG_DCB_CAPABLE;
			pf->flags &= ~I40E_FLAG_DCB_ENABLED;
		}
	}
#endif  
}

 
static void i40e_watchdog_subtask(struct i40e_pf *pf)
{
	int i;

	 
	if (test_bit(__I40E_DOWN, pf->state) ||
	    test_bit(__I40E_CONFIG_BUSY, pf->state))
		return;

	 
	if (time_before(jiffies, (pf->service_timer_previous +
				  pf->service_timer_period)))
		return;
	pf->service_timer_previous = jiffies;

	if ((pf->flags & I40E_FLAG_LINK_POLLING_ENABLED) ||
	    test_bit(__I40E_TEMP_LINK_POLLING, pf->state))
		i40e_link_event(pf);

	 
	for (i = 0; i < pf->num_alloc_vsi; i++)
		if (pf->vsi[i] && pf->vsi[i]->netdev)
			i40e_update_stats(pf->vsi[i]);

	if (pf->flags & I40E_FLAG_VEB_STATS_ENABLED) {
		 
		for (i = 0; i < I40E_MAX_VEB; i++)
			if (pf->veb[i])
				i40e_update_veb_stats(pf->veb[i]);
	}

	i40e_ptp_rx_hang(pf);
	i40e_ptp_tx_hang(pf);
}

 
static void i40e_reset_subtask(struct i40e_pf *pf)
{
	u32 reset_flags = 0;

	if (test_bit(__I40E_REINIT_REQUESTED, pf->state)) {
		reset_flags |= BIT(__I40E_REINIT_REQUESTED);
		clear_bit(__I40E_REINIT_REQUESTED, pf->state);
	}
	if (test_bit(__I40E_PF_RESET_REQUESTED, pf->state)) {
		reset_flags |= BIT(__I40E_PF_RESET_REQUESTED);
		clear_bit(__I40E_PF_RESET_REQUESTED, pf->state);
	}
	if (test_bit(__I40E_CORE_RESET_REQUESTED, pf->state)) {
		reset_flags |= BIT(__I40E_CORE_RESET_REQUESTED);
		clear_bit(__I40E_CORE_RESET_REQUESTED, pf->state);
	}
	if (test_bit(__I40E_GLOBAL_RESET_REQUESTED, pf->state)) {
		reset_flags |= BIT(__I40E_GLOBAL_RESET_REQUESTED);
		clear_bit(__I40E_GLOBAL_RESET_REQUESTED, pf->state);
	}
	if (test_bit(__I40E_DOWN_REQUESTED, pf->state)) {
		reset_flags |= BIT(__I40E_DOWN_REQUESTED);
		clear_bit(__I40E_DOWN_REQUESTED, pf->state);
	}

	 
	if (test_bit(__I40E_RESET_INTR_RECEIVED, pf->state)) {
		i40e_prep_for_reset(pf);
		i40e_reset(pf);
		i40e_rebuild(pf, false, false);
	}

	 
	if (reset_flags &&
	    !test_bit(__I40E_DOWN, pf->state) &&
	    !test_bit(__I40E_CONFIG_BUSY, pf->state)) {
		i40e_do_reset(pf, reset_flags, false);
	}
}

 
static void i40e_handle_link_event(struct i40e_pf *pf,
				   struct i40e_arq_event_info *e)
{
	struct i40e_aqc_get_link_status *status =
		(struct i40e_aqc_get_link_status *)&e->desc.params.raw;

	 
	i40e_link_event(pf);

	 
	if (status->phy_type == I40E_PHY_TYPE_NOT_SUPPORTED_HIGH_TEMP) {
		dev_err(&pf->pdev->dev,
			"Rx/Tx is disabled on this device because the module does not meet thermal requirements.\n");
		dev_err(&pf->pdev->dev,
			"Refer to the Intel(R) Ethernet Adapters and Devices User Guide for a list of supported modules.\n");
	} else {
		 
		if ((status->link_info & I40E_AQ_MEDIA_AVAILABLE) &&
		    (!(status->an_info & I40E_AQ_QUALIFIED_MODULE)) &&
		    (!(status->link_info & I40E_AQ_LINK_UP)) &&
		    (!(pf->flags & I40E_FLAG_LINK_DOWN_ON_CLOSE_ENABLED))) {
			dev_err(&pf->pdev->dev,
				"Rx/Tx is disabled on this device because an unsupported SFP module type was detected.\n");
			dev_err(&pf->pdev->dev,
				"Refer to the Intel(R) Ethernet Adapters and Devices User Guide for a list of supported modules.\n");
		}
	}
}

 
static void i40e_clean_adminq_subtask(struct i40e_pf *pf)
{
	struct i40e_arq_event_info event;
	struct i40e_hw *hw = &pf->hw;
	u16 pending, i = 0;
	u16 opcode;
	u32 oldval;
	int ret;
	u32 val;

	 
	if (test_bit(__I40E_RESET_FAILED, pf->state))
		return;

	 
	val = rd32(&pf->hw, pf->hw.aq.arq.len);
	oldval = val;
	if (val & I40E_PF_ARQLEN_ARQVFE_MASK) {
		if (hw->debug_mask & I40E_DEBUG_AQ)
			dev_info(&pf->pdev->dev, "ARQ VF Error detected\n");
		val &= ~I40E_PF_ARQLEN_ARQVFE_MASK;
	}
	if (val & I40E_PF_ARQLEN_ARQOVFL_MASK) {
		if (hw->debug_mask & I40E_DEBUG_AQ)
			dev_info(&pf->pdev->dev, "ARQ Overflow Error detected\n");
		val &= ~I40E_PF_ARQLEN_ARQOVFL_MASK;
		pf->arq_overflows++;
	}
	if (val & I40E_PF_ARQLEN_ARQCRIT_MASK) {
		if (hw->debug_mask & I40E_DEBUG_AQ)
			dev_info(&pf->pdev->dev, "ARQ Critical Error detected\n");
		val &= ~I40E_PF_ARQLEN_ARQCRIT_MASK;
	}
	if (oldval != val)
		wr32(&pf->hw, pf->hw.aq.arq.len, val);

	val = rd32(&pf->hw, pf->hw.aq.asq.len);
	oldval = val;
	if (val & I40E_PF_ATQLEN_ATQVFE_MASK) {
		if (pf->hw.debug_mask & I40E_DEBUG_AQ)
			dev_info(&pf->pdev->dev, "ASQ VF Error detected\n");
		val &= ~I40E_PF_ATQLEN_ATQVFE_MASK;
	}
	if (val & I40E_PF_ATQLEN_ATQOVFL_MASK) {
		if (pf->hw.debug_mask & I40E_DEBUG_AQ)
			dev_info(&pf->pdev->dev, "ASQ Overflow Error detected\n");
		val &= ~I40E_PF_ATQLEN_ATQOVFL_MASK;
	}
	if (val & I40E_PF_ATQLEN_ATQCRIT_MASK) {
		if (pf->hw.debug_mask & I40E_DEBUG_AQ)
			dev_info(&pf->pdev->dev, "ASQ Critical Error detected\n");
		val &= ~I40E_PF_ATQLEN_ATQCRIT_MASK;
	}
	if (oldval != val)
		wr32(&pf->hw, pf->hw.aq.asq.len, val);

	event.buf_len = I40E_MAX_AQ_BUF_SIZE;
	event.msg_buf = kzalloc(event.buf_len, GFP_KERNEL);
	if (!event.msg_buf)
		return;

	do {
		ret = i40e_clean_arq_element(hw, &event, &pending);
		if (ret == -EALREADY)
			break;
		else if (ret) {
			dev_info(&pf->pdev->dev, "ARQ event error %d\n", ret);
			break;
		}

		opcode = le16_to_cpu(event.desc.opcode);
		switch (opcode) {

		case i40e_aqc_opc_get_link_status:
			rtnl_lock();
			i40e_handle_link_event(pf, &event);
			rtnl_unlock();
			break;
		case i40e_aqc_opc_send_msg_to_pf:
			ret = i40e_vc_process_vf_msg(pf,
					le16_to_cpu(event.desc.retval),
					le32_to_cpu(event.desc.cookie_high),
					le32_to_cpu(event.desc.cookie_low),
					event.msg_buf,
					event.msg_len);
			break;
		case i40e_aqc_opc_lldp_update_mib:
			dev_dbg(&pf->pdev->dev, "ARQ: Update LLDP MIB event received\n");
#ifdef CONFIG_I40E_DCB
			rtnl_lock();
			i40e_handle_lldp_event(pf, &event);
			rtnl_unlock();
#endif  
			break;
		case i40e_aqc_opc_event_lan_overflow:
			dev_dbg(&pf->pdev->dev, "ARQ LAN queue overflow event received\n");
			i40e_handle_lan_overflow_event(pf, &event);
			break;
		case i40e_aqc_opc_send_msg_to_peer:
			dev_info(&pf->pdev->dev, "ARQ: Msg from other pf\n");
			break;
		case i40e_aqc_opc_nvm_erase:
		case i40e_aqc_opc_nvm_update:
		case i40e_aqc_opc_oem_post_update:
			i40e_debug(&pf->hw, I40E_DEBUG_NVM,
				   "ARQ NVM operation 0x%04x completed\n",
				   opcode);
			break;
		default:
			dev_info(&pf->pdev->dev,
				 "ARQ: Unknown event 0x%04x ignored\n",
				 opcode);
			break;
		}
	} while (i++ < pf->adminq_work_limit);

	if (i < pf->adminq_work_limit)
		clear_bit(__I40E_ADMINQ_EVENT_PENDING, pf->state);

	 
	val = rd32(hw, I40E_PFINT_ICR0_ENA);
	val |=  I40E_PFINT_ICR0_ENA_ADMINQ_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, val);
	i40e_flush(hw);

	kfree(event.msg_buf);
}

 
static void i40e_verify_eeprom(struct i40e_pf *pf)
{
	int err;

	err = i40e_diag_eeprom_test(&pf->hw);
	if (err) {
		 
		err = i40e_diag_eeprom_test(&pf->hw);
		if (err) {
			dev_info(&pf->pdev->dev, "eeprom check failed (%d), Tx/Rx traffic disabled\n",
				 err);
			set_bit(__I40E_BAD_EEPROM, pf->state);
		}
	}

	if (!err && test_bit(__I40E_BAD_EEPROM, pf->state)) {
		dev_info(&pf->pdev->dev, "eeprom check passed, Tx/Rx traffic enabled\n");
		clear_bit(__I40E_BAD_EEPROM, pf->state);
	}
}

 
static void i40e_enable_pf_switch_lb(struct i40e_pf *pf)
{
	struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
	struct i40e_vsi_context ctxt;
	int ret;

	ctxt.seid = pf->main_vsi_seid;
	ctxt.pf_num = pf->hw.pf_id;
	ctxt.vf_num = 0;
	ret = i40e_aq_get_vsi_params(&pf->hw, &ctxt, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't get PF vsi config, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		return;
	}
	ctxt.flags = I40E_AQ_VSI_TYPE_PF;
	ctxt.info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
	ctxt.info.switch_id |= cpu_to_le16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);

	ret = i40e_aq_update_vsi_params(&vsi->back->hw, &ctxt, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "update vsi switch failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
	}
}

 
static void i40e_disable_pf_switch_lb(struct i40e_pf *pf)
{
	struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
	struct i40e_vsi_context ctxt;
	int ret;

	ctxt.seid = pf->main_vsi_seid;
	ctxt.pf_num = pf->hw.pf_id;
	ctxt.vf_num = 0;
	ret = i40e_aq_get_vsi_params(&pf->hw, &ctxt, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't get PF vsi config, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		return;
	}
	ctxt.flags = I40E_AQ_VSI_TYPE_PF;
	ctxt.info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
	ctxt.info.switch_id &= ~cpu_to_le16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);

	ret = i40e_aq_update_vsi_params(&vsi->back->hw, &ctxt, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "update vsi switch failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
	}
}

 
static void i40e_config_bridge_mode(struct i40e_veb *veb)
{
	struct i40e_pf *pf = veb->pf;

	if (pf->hw.debug_mask & I40E_DEBUG_LAN)
		dev_info(&pf->pdev->dev, "enabling bridge mode: %s\n",
			 veb->bridge_mode == BRIDGE_MODE_VEPA ? "VEPA" : "VEB");
	if (veb->bridge_mode & BRIDGE_MODE_VEPA)
		i40e_disable_pf_switch_lb(pf);
	else
		i40e_enable_pf_switch_lb(pf);
}

 
static int i40e_reconstitute_veb(struct i40e_veb *veb)
{
	struct i40e_vsi *ctl_vsi = NULL;
	struct i40e_pf *pf = veb->pf;
	int v, veb_idx;
	int ret;

	 
	for (v = 0; v < pf->num_alloc_vsi && !ctl_vsi; v++) {
		if (pf->vsi[v] &&
		    pf->vsi[v]->veb_idx == veb->idx &&
		    pf->vsi[v]->flags & I40E_VSI_FLAG_VEB_OWNER) {
			ctl_vsi = pf->vsi[v];
			break;
		}
	}
	if (!ctl_vsi) {
		dev_info(&pf->pdev->dev,
			 "missing owner VSI for veb_idx %d\n", veb->idx);
		ret = -ENOENT;
		goto end_reconstitute;
	}
	if (ctl_vsi != pf->vsi[pf->lan_vsi])
		ctl_vsi->uplink_seid = pf->vsi[pf->lan_vsi]->uplink_seid;
	ret = i40e_add_vsi(ctl_vsi);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "rebuild of veb_idx %d owner VSI failed: %d\n",
			 veb->idx, ret);
		goto end_reconstitute;
	}
	i40e_vsi_reset_stats(ctl_vsi);

	 
	ret = i40e_add_veb(veb, ctl_vsi);
	if (ret)
		goto end_reconstitute;

	if (pf->flags & I40E_FLAG_VEB_MODE_ENABLED)
		veb->bridge_mode = BRIDGE_MODE_VEB;
	else
		veb->bridge_mode = BRIDGE_MODE_VEPA;
	i40e_config_bridge_mode(veb);

	 
	for (v = 0; v < pf->num_alloc_vsi; v++) {
		if (!pf->vsi[v] || pf->vsi[v] == ctl_vsi)
			continue;

		if (pf->vsi[v]->veb_idx == veb->idx) {
			struct i40e_vsi *vsi = pf->vsi[v];

			vsi->uplink_seid = veb->seid;
			ret = i40e_add_vsi(vsi);
			if (ret) {
				dev_info(&pf->pdev->dev,
					 "rebuild of vsi_idx %d failed: %d\n",
					 v, ret);
				goto end_reconstitute;
			}
			i40e_vsi_reset_stats(vsi);
		}
	}

	 
	for (veb_idx = 0; veb_idx < I40E_MAX_VEB; veb_idx++) {
		if (pf->veb[veb_idx] && pf->veb[veb_idx]->veb_idx == veb->idx) {
			pf->veb[veb_idx]->uplink_seid = veb->seid;
			ret = i40e_reconstitute_veb(pf->veb[veb_idx]);
			if (ret)
				break;
		}
	}

end_reconstitute:
	return ret;
}

 
static int i40e_get_capabilities(struct i40e_pf *pf,
				 enum i40e_admin_queue_opc list_type)
{
	struct i40e_aqc_list_capabilities_element_resp *cap_buf;
	u16 data_size;
	int buf_len;
	int err;

	buf_len = 40 * sizeof(struct i40e_aqc_list_capabilities_element_resp);
	do {
		cap_buf = kzalloc(buf_len, GFP_KERNEL);
		if (!cap_buf)
			return -ENOMEM;

		 
		err = i40e_aq_discover_capabilities(&pf->hw, cap_buf, buf_len,
						    &data_size, list_type,
						    NULL);
		 
		kfree(cap_buf);

		if (pf->hw.aq.asq_last_status == I40E_AQ_RC_ENOMEM) {
			 
			buf_len = data_size;
		} else if (pf->hw.aq.asq_last_status != I40E_AQ_RC_OK || err) {
			dev_info(&pf->pdev->dev,
				 "capability discovery failed, err %pe aq_err %s\n",
				 ERR_PTR(err),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
			return -ENODEV;
		}
	} while (err);

	if (pf->hw.debug_mask & I40E_DEBUG_USER) {
		if (list_type == i40e_aqc_opc_list_func_capabilities) {
			dev_info(&pf->pdev->dev,
				 "pf=%d, num_vfs=%d, msix_pf=%d, msix_vf=%d, fd_g=%d, fd_b=%d, pf_max_q=%d num_vsi=%d\n",
				 pf->hw.pf_id, pf->hw.func_caps.num_vfs,
				 pf->hw.func_caps.num_msix_vectors,
				 pf->hw.func_caps.num_msix_vectors_vf,
				 pf->hw.func_caps.fd_filters_guaranteed,
				 pf->hw.func_caps.fd_filters_best_effort,
				 pf->hw.func_caps.num_tx_qp,
				 pf->hw.func_caps.num_vsis);
		} else if (list_type == i40e_aqc_opc_list_dev_capabilities) {
			dev_info(&pf->pdev->dev,
				 "switch_mode=0x%04x, function_valid=0x%08x\n",
				 pf->hw.dev_caps.switch_mode,
				 pf->hw.dev_caps.valid_functions);
			dev_info(&pf->pdev->dev,
				 "SR-IOV=%d, num_vfs for all function=%u\n",
				 pf->hw.dev_caps.sr_iov_1_1,
				 pf->hw.dev_caps.num_vfs);
			dev_info(&pf->pdev->dev,
				 "num_vsis=%u, num_rx:%u, num_tx=%u\n",
				 pf->hw.dev_caps.num_vsis,
				 pf->hw.dev_caps.num_rx_qp,
				 pf->hw.dev_caps.num_tx_qp);
		}
	}
	if (list_type == i40e_aqc_opc_list_func_capabilities) {
#define DEF_NUM_VSI (1 + (pf->hw.func_caps.fcoe ? 1 : 0) \
		       + pf->hw.func_caps.num_vfs)
		if (pf->hw.revision_id == 0 &&
		    pf->hw.func_caps.num_vsis < DEF_NUM_VSI) {
			dev_info(&pf->pdev->dev,
				 "got num_vsis %d, setting num_vsis to %d\n",
				 pf->hw.func_caps.num_vsis, DEF_NUM_VSI);
			pf->hw.func_caps.num_vsis = DEF_NUM_VSI;
		}
	}
	return 0;
}

static int i40e_vsi_clear(struct i40e_vsi *vsi);

 
static void i40e_fdir_sb_setup(struct i40e_pf *pf)
{
	struct i40e_vsi *vsi;

	 
	if (!rd32(&pf->hw, I40E_GLQF_HKEY(0))) {
		static const u32 hkey[] = {
			0xe640d33f, 0xcdfe98ab, 0x73fa7161, 0x0d7a7d36,
			0xeacb7d61, 0xaa4f05b6, 0x9c5c89ed, 0xfc425ddb,
			0xa4654832, 0xfc7461d4, 0x8f827619, 0xf5c63c21,
			0x95b3a76d};
		int i;

		for (i = 0; i <= I40E_GLQF_HKEY_MAX_INDEX; i++)
			wr32(&pf->hw, I40E_GLQF_HKEY(i), hkey[i]);
	}

	if (!(pf->flags & I40E_FLAG_FD_SB_ENABLED))
		return;

	 
	vsi = i40e_find_vsi_by_type(pf, I40E_VSI_FDIR);

	 
	if (!vsi) {
		vsi = i40e_vsi_setup(pf, I40E_VSI_FDIR,
				     pf->vsi[pf->lan_vsi]->seid, 0);
		if (!vsi) {
			dev_info(&pf->pdev->dev, "Couldn't create FDir VSI\n");
			pf->flags &= ~I40E_FLAG_FD_SB_ENABLED;
			pf->flags |= I40E_FLAG_FD_SB_INACTIVE;
			return;
		}
	}

	i40e_vsi_setup_irqhandler(vsi, i40e_fdir_clean_ring);
}

 
static void i40e_fdir_teardown(struct i40e_pf *pf)
{
	struct i40e_vsi *vsi;

	i40e_fdir_filter_exit(pf);
	vsi = i40e_find_vsi_by_type(pf, I40E_VSI_FDIR);
	if (vsi)
		i40e_vsi_release(vsi);
}

 
static int i40e_rebuild_cloud_filters(struct i40e_vsi *vsi, u16 seid)
{
	struct i40e_cloud_filter *cfilter;
	struct i40e_pf *pf = vsi->back;
	struct hlist_node *node;
	int ret;

	 
	hlist_for_each_entry_safe(cfilter, node, &pf->cloud_filter_list,
				  cloud_node) {
		if (cfilter->seid != seid)
			continue;

		if (cfilter->dst_port)
			ret = i40e_add_del_cloud_filter_big_buf(vsi, cfilter,
								true);
		else
			ret = i40e_add_del_cloud_filter(vsi, cfilter, true);

		if (ret) {
			dev_dbg(&pf->pdev->dev,
				"Failed to rebuild cloud filter, err %pe aq_err %s\n",
				ERR_PTR(ret),
				i40e_aq_str(&pf->hw,
					    pf->hw.aq.asq_last_status));
			return ret;
		}
	}
	return 0;
}

 
static int i40e_rebuild_channels(struct i40e_vsi *vsi)
{
	struct i40e_channel *ch, *ch_tmp;
	int ret;

	if (list_empty(&vsi->ch_list))
		return 0;

	list_for_each_entry_safe(ch, ch_tmp, &vsi->ch_list, list) {
		if (!ch->initialized)
			break;
		 
		ret = i40e_add_channel(vsi->back, vsi->uplink_seid, ch);
		if (ret) {
			dev_info(&vsi->back->pdev->dev,
				 "failed to rebuild channels using uplink_seid %u\n",
				 vsi->uplink_seid);
			return ret;
		}
		 
		ret = i40e_channel_config_tx_ring(vsi->back, vsi, ch);
		if (ret) {
			dev_info(&vsi->back->pdev->dev,
				 "failed to configure TX rings for channel %u\n",
				 ch->seid);
			return ret;
		}
		 
		vsi->next_base_queue = vsi->next_base_queue +
							ch->num_queue_pairs;
		if (ch->max_tx_rate) {
			u64 credits = ch->max_tx_rate;

			if (i40e_set_bw_limit(vsi, ch->seid,
					      ch->max_tx_rate))
				return -EINVAL;

			do_div(credits, I40E_BW_CREDIT_DIVISOR);
			dev_dbg(&vsi->back->pdev->dev,
				"Set tx rate of %llu Mbps (count of 50Mbps %llu) for vsi->seid %u\n",
				ch->max_tx_rate,
				credits,
				ch->seid);
		}
		ret = i40e_rebuild_cloud_filters(vsi, ch->seid);
		if (ret) {
			dev_dbg(&vsi->back->pdev->dev,
				"Failed to rebuild cloud filters for channel VSI %u\n",
				ch->seid);
			return ret;
		}
	}
	return 0;
}

 
static void i40e_clean_xps_state(struct i40e_vsi *vsi)
{
	int i;

	if (vsi->tx_rings)
		for (i = 0; i < vsi->num_queue_pairs; i++)
			if (vsi->tx_rings[i])
				clear_bit(__I40E_TX_XPS_INIT_DONE,
					  vsi->tx_rings[i]->state);
}

 
static void i40e_prep_for_reset(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	int ret = 0;
	u32 v;

	clear_bit(__I40E_RESET_INTR_RECEIVED, pf->state);
	if (test_and_set_bit(__I40E_RESET_RECOVERY_PENDING, pf->state))
		return;
	if (i40e_check_asq_alive(&pf->hw))
		i40e_vc_notify_reset(pf);

	dev_dbg(&pf->pdev->dev, "Tearing down internal switch for reset\n");

	 
	i40e_pf_quiesce_all_vsi(pf);

	for (v = 0; v < pf->num_alloc_vsi; v++) {
		if (pf->vsi[v]) {
			i40e_clean_xps_state(pf->vsi[v]);
			pf->vsi[v]->seid = 0;
		}
	}

	i40e_shutdown_adminq(&pf->hw);

	 
	if (hw->hmc.hmc_obj) {
		ret = i40e_shutdown_lan_hmc(hw);
		if (ret)
			dev_warn(&pf->pdev->dev,
				 "shutdown_lan_hmc failed: %d\n", ret);
	}

	 
	i40e_ptp_save_hw_time(pf);
}

 
static void i40e_send_version(struct i40e_pf *pf)
{
	struct i40e_driver_version dv;

	dv.major_version = 0xff;
	dv.minor_version = 0xff;
	dv.build_version = 0xff;
	dv.subbuild_version = 0;
	strscpy(dv.driver_string, UTS_RELEASE, sizeof(dv.driver_string));
	i40e_aq_send_driver_version(&pf->hw, &dv, NULL);
}

 
static void i40e_get_oem_version(struct i40e_hw *hw)
{
	u16 block_offset = 0xffff;
	u16 block_length = 0;
	u16 capabilities = 0;
	u16 gen_snap = 0;
	u16 release = 0;

#define I40E_SR_NVM_OEM_VERSION_PTR		0x1B
#define I40E_NVM_OEM_LENGTH_OFFSET		0x00
#define I40E_NVM_OEM_CAPABILITIES_OFFSET	0x01
#define I40E_NVM_OEM_GEN_OFFSET			0x02
#define I40E_NVM_OEM_RELEASE_OFFSET		0x03
#define I40E_NVM_OEM_CAPABILITIES_MASK		0x000F
#define I40E_NVM_OEM_LENGTH			3

	 
	i40e_read_nvm_word(hw, I40E_SR_NVM_OEM_VERSION_PTR, &block_offset);
	if (block_offset == 0xffff)
		return;

	 
	i40e_read_nvm_word(hw, block_offset + I40E_NVM_OEM_LENGTH_OFFSET,
			   &block_length);
	if (block_length < I40E_NVM_OEM_LENGTH)
		return;

	 
	i40e_read_nvm_word(hw, block_offset + I40E_NVM_OEM_CAPABILITIES_OFFSET,
			   &capabilities);
	if ((capabilities & I40E_NVM_OEM_CAPABILITIES_MASK) != 0)
		return;

	i40e_read_nvm_word(hw, block_offset + I40E_NVM_OEM_GEN_OFFSET,
			   &gen_snap);
	i40e_read_nvm_word(hw, block_offset + I40E_NVM_OEM_RELEASE_OFFSET,
			   &release);
	hw->nvm.oem_ver = (gen_snap << I40E_OEM_SNAP_SHIFT) | release;
	hw->nvm.eetrack = I40E_OEM_EETRACK_ID;
}

 
static int i40e_reset(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	int ret;

	ret = i40e_pf_reset(hw);
	if (ret) {
		dev_info(&pf->pdev->dev, "PF reset failed, %d\n", ret);
		set_bit(__I40E_RESET_FAILED, pf->state);
		clear_bit(__I40E_RESET_RECOVERY_PENDING, pf->state);
	} else {
		pf->pfr_count++;
	}
	return ret;
}

 
static void i40e_rebuild(struct i40e_pf *pf, bool reinit, bool lock_acquired)
{
	const bool is_recovery_mode_reported = i40e_check_recovery_mode(pf);
	struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
	struct i40e_hw *hw = &pf->hw;
	int ret;
	u32 val;
	int v;

	if (test_bit(__I40E_EMP_RESET_INTR_RECEIVED, pf->state) &&
	    is_recovery_mode_reported)
		i40e_set_ethtool_ops(pf->vsi[pf->lan_vsi]->netdev);

	if (test_bit(__I40E_DOWN, pf->state) &&
	    !test_bit(__I40E_RECOVERY_MODE, pf->state))
		goto clear_recovery;
	dev_dbg(&pf->pdev->dev, "Rebuilding internal switch\n");

	 
	ret = i40e_init_adminq(&pf->hw);
	if (ret) {
		dev_info(&pf->pdev->dev, "Rebuild AdminQ failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		goto clear_recovery;
	}
	i40e_get_oem_version(&pf->hw);

	if (test_and_clear_bit(__I40E_EMP_RESET_INTR_RECEIVED, pf->state)) {
		 
		mdelay(1000);
	}

	 
	if (test_and_clear_bit(__I40E_EMP_RESET_INTR_RECEIVED, pf->state))
		i40e_verify_eeprom(pf);

	 
	if (test_bit(__I40E_RECOVERY_MODE, pf->state)) {
		if (i40e_get_capabilities(pf,
					  i40e_aqc_opc_list_func_capabilities))
			goto end_unlock;

		if (is_recovery_mode_reported) {
			 
			if (i40e_setup_misc_vector_for_recovery_mode(pf))
				goto end_unlock;
		} else {
			if (!lock_acquired)
				rtnl_lock();
			 
			free_irq(pf->pdev->irq, pf);
			i40e_clear_interrupt_scheme(pf);
			if (i40e_restore_interrupt_scheme(pf))
				goto end_unlock;
		}

		 
		i40e_send_version(pf);

		 
		goto end_unlock;
	}

	i40e_clear_pxe_mode(hw);
	ret = i40e_get_capabilities(pf, i40e_aqc_opc_list_func_capabilities);
	if (ret)
		goto end_core_reset;

	ret = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
				hw->func_caps.num_rx_qp, 0, 0);
	if (ret) {
		dev_info(&pf->pdev->dev, "init_lan_hmc failed: %d\n", ret);
		goto end_core_reset;
	}
	ret = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (ret) {
		dev_info(&pf->pdev->dev, "configure_lan_hmc failed: %d\n", ret);
		goto end_core_reset;
	}

#ifdef CONFIG_I40E_DCB
	 
	if (i40e_is_tc_mqprio_enabled(pf)) {
		i40e_aq_set_dcb_parameters(hw, false, NULL);
	} else {
		if (I40E_IS_X710TL_DEVICE(hw->device_id) &&
		    (hw->phy.link_info.link_speed &
		     (I40E_LINK_SPEED_2_5GB | I40E_LINK_SPEED_5GB))) {
			i40e_aq_set_dcb_parameters(hw, false, NULL);
			dev_warn(&pf->pdev->dev,
				 "DCB is not supported for X710-T*L 2.5/5G speeds\n");
			pf->flags &= ~I40E_FLAG_DCB_CAPABLE;
		} else {
			i40e_aq_set_dcb_parameters(hw, true, NULL);
			ret = i40e_init_pf_dcb(pf);
			if (ret) {
				dev_info(&pf->pdev->dev, "DCB init failed %d, disabled\n",
					 ret);
				pf->flags &= ~I40E_FLAG_DCB_CAPABLE;
				 
			}
		}
	}

#endif  
	if (!lock_acquired)
		rtnl_lock();
	ret = i40e_setup_pf_switch(pf, reinit, true);
	if (ret)
		goto end_unlock;

	 
	ret = i40e_aq_set_phy_int_mask(&pf->hw,
				       ~(I40E_AQ_EVENT_LINK_UPDOWN |
					 I40E_AQ_EVENT_MEDIA_NA |
					 I40E_AQ_EVENT_MODULE_QUAL_FAIL), NULL);
	if (ret)
		dev_info(&pf->pdev->dev, "set phy mask fail, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));

	 
	if (vsi->uplink_seid != pf->mac_seid) {
		dev_dbg(&pf->pdev->dev, "attempting to rebuild switch\n");
		 
		for (v = 0; v < I40E_MAX_VEB; v++) {
			if (!pf->veb[v])
				continue;

			if (pf->veb[v]->uplink_seid == pf->mac_seid ||
			    pf->veb[v]->uplink_seid == 0) {
				ret = i40e_reconstitute_veb(pf->veb[v]);

				if (!ret)
					continue;

				 
				if (pf->veb[v]->uplink_seid == pf->mac_seid) {
					dev_info(&pf->pdev->dev,
						 "rebuild of switch failed: %d, will try to set up simple PF connection\n",
						 ret);
					vsi->uplink_seid = pf->mac_seid;
					break;
				} else if (pf->veb[v]->uplink_seid == 0) {
					dev_info(&pf->pdev->dev,
						 "rebuild of orphan VEB failed: %d\n",
						 ret);
				}
			}
		}
	}

	if (vsi->uplink_seid == pf->mac_seid) {
		dev_dbg(&pf->pdev->dev, "attempting to rebuild PF VSI\n");
		 
		ret = i40e_add_vsi(vsi);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "rebuild of Main VSI failed: %d\n", ret);
			goto end_unlock;
		}
	}

	if (vsi->mqprio_qopt.max_rate[0]) {
		u64 max_tx_rate = i40e_bw_bytes_to_mbits(vsi,
						  vsi->mqprio_qopt.max_rate[0]);
		u64 credits = 0;

		ret = i40e_set_bw_limit(vsi, vsi->seid, max_tx_rate);
		if (ret)
			goto end_unlock;

		credits = max_tx_rate;
		do_div(credits, I40E_BW_CREDIT_DIVISOR);
		dev_dbg(&vsi->back->pdev->dev,
			"Set tx rate of %llu Mbps (count of 50Mbps %llu) for vsi->seid %u\n",
			max_tx_rate,
			credits,
			vsi->seid);
	}

	ret = i40e_rebuild_cloud_filters(vsi, vsi->seid);
	if (ret)
		goto end_unlock;

	 
	ret = i40e_rebuild_channels(vsi);
	if (ret)
		goto end_unlock;

	 
#define I40E_REG_MSS          0x000E64DC
#define I40E_REG_MSS_MIN_MASK 0x3FF0000
#define I40E_64BYTE_MSS       0x400000
	val = rd32(hw, I40E_REG_MSS);
	if ((val & I40E_REG_MSS_MIN_MASK) > I40E_64BYTE_MSS) {
		val &= ~I40E_REG_MSS_MIN_MASK;
		val |= I40E_64BYTE_MSS;
		wr32(hw, I40E_REG_MSS, val);
	}

	if (pf->hw_features & I40E_HW_RESTART_AUTONEG) {
		msleep(75);
		ret = i40e_aq_set_link_restart_an(&pf->hw, true, NULL);
		if (ret)
			dev_info(&pf->pdev->dev, "link restart failed, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
	}
	 
	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		ret = i40e_setup_misc_vector(pf);
		if (ret)
			goto end_unlock;
	}

	 
	i40e_add_filter_to_drop_tx_flow_control_frames(&pf->hw,
						       pf->main_vsi_seid);

	 
	i40e_pf_unquiesce_all_vsi(pf);

	 
	if (!lock_acquired)
		rtnl_unlock();

	 
	ret = i40e_set_promiscuous(pf, pf->cur_promisc);
	if (ret)
		dev_warn(&pf->pdev->dev,
			 "Failed to restore promiscuous setting: %s, err %pe aq_err %s\n",
			 pf->cur_promisc ? "on" : "off",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));

	i40e_reset_all_vfs(pf, true);

	 
	i40e_send_version(pf);

	 
	goto end_core_reset;

end_unlock:
	if (!lock_acquired)
		rtnl_unlock();
end_core_reset:
	clear_bit(__I40E_RESET_FAILED, pf->state);
clear_recovery:
	clear_bit(__I40E_RESET_RECOVERY_PENDING, pf->state);
	clear_bit(__I40E_TIMEOUT_RECOVERY_PENDING, pf->state);
}

 
static void i40e_reset_and_rebuild(struct i40e_pf *pf, bool reinit,
				   bool lock_acquired)
{
	int ret;

	if (test_bit(__I40E_IN_REMOVE, pf->state))
		return;
	 
	ret = i40e_reset(pf);
	if (!ret)
		i40e_rebuild(pf, reinit, lock_acquired);
}

 
static void i40e_handle_reset_warning(struct i40e_pf *pf, bool lock_acquired)
{
	i40e_prep_for_reset(pf);
	i40e_reset_and_rebuild(pf, false, lock_acquired);
}

 
static void i40e_handle_mdd_event(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	bool mdd_detected = false;
	struct i40e_vf *vf;
	u32 reg;
	int i;

	if (!test_bit(__I40E_MDD_EVENT_PENDING, pf->state))
		return;

	 
	reg = rd32(hw, I40E_GL_MDET_TX);
	if (reg & I40E_GL_MDET_TX_VALID_MASK) {
		u8 pf_num = (reg & I40E_GL_MDET_TX_PF_NUM_MASK) >>
				I40E_GL_MDET_TX_PF_NUM_SHIFT;
		u16 vf_num = (reg & I40E_GL_MDET_TX_VF_NUM_MASK) >>
				I40E_GL_MDET_TX_VF_NUM_SHIFT;
		u8 event = (reg & I40E_GL_MDET_TX_EVENT_MASK) >>
				I40E_GL_MDET_TX_EVENT_SHIFT;
		u16 queue = ((reg & I40E_GL_MDET_TX_QUEUE_MASK) >>
				I40E_GL_MDET_TX_QUEUE_SHIFT) -
				pf->hw.func_caps.base_queue;
		if (netif_msg_tx_err(pf))
			dev_info(&pf->pdev->dev, "Malicious Driver Detection event 0x%02x on TX queue %d PF number 0x%02x VF number 0x%02x\n",
				 event, queue, pf_num, vf_num);
		wr32(hw, I40E_GL_MDET_TX, 0xffffffff);
		mdd_detected = true;
	}
	reg = rd32(hw, I40E_GL_MDET_RX);
	if (reg & I40E_GL_MDET_RX_VALID_MASK) {
		u8 func = (reg & I40E_GL_MDET_RX_FUNCTION_MASK) >>
				I40E_GL_MDET_RX_FUNCTION_SHIFT;
		u8 event = (reg & I40E_GL_MDET_RX_EVENT_MASK) >>
				I40E_GL_MDET_RX_EVENT_SHIFT;
		u16 queue = ((reg & I40E_GL_MDET_RX_QUEUE_MASK) >>
				I40E_GL_MDET_RX_QUEUE_SHIFT) -
				pf->hw.func_caps.base_queue;
		if (netif_msg_rx_err(pf))
			dev_info(&pf->pdev->dev, "Malicious Driver Detection event 0x%02x on RX queue %d of function 0x%02x\n",
				 event, queue, func);
		wr32(hw, I40E_GL_MDET_RX, 0xffffffff);
		mdd_detected = true;
	}

	if (mdd_detected) {
		reg = rd32(hw, I40E_PF_MDET_TX);
		if (reg & I40E_PF_MDET_TX_VALID_MASK) {
			wr32(hw, I40E_PF_MDET_TX, 0xFFFF);
			dev_dbg(&pf->pdev->dev, "TX driver issue detected on PF\n");
		}
		reg = rd32(hw, I40E_PF_MDET_RX);
		if (reg & I40E_PF_MDET_RX_VALID_MASK) {
			wr32(hw, I40E_PF_MDET_RX, 0xFFFF);
			dev_dbg(&pf->pdev->dev, "RX driver issue detected on PF\n");
		}
	}

	 
	for (i = 0; i < pf->num_alloc_vfs && mdd_detected; i++) {
		vf = &(pf->vf[i]);
		reg = rd32(hw, I40E_VP_MDET_TX(i));
		if (reg & I40E_VP_MDET_TX_VALID_MASK) {
			wr32(hw, I40E_VP_MDET_TX(i), 0xFFFF);
			vf->num_mdd_events++;
			dev_info(&pf->pdev->dev, "TX driver issue detected on VF %d\n",
				 i);
			dev_info(&pf->pdev->dev,
				 "Use PF Control I/F to re-enable the VF\n");
			set_bit(I40E_VF_STATE_DISABLED, &vf->vf_states);
		}

		reg = rd32(hw, I40E_VP_MDET_RX(i));
		if (reg & I40E_VP_MDET_RX_VALID_MASK) {
			wr32(hw, I40E_VP_MDET_RX(i), 0xFFFF);
			vf->num_mdd_events++;
			dev_info(&pf->pdev->dev, "RX driver issue detected on VF %d\n",
				 i);
			dev_info(&pf->pdev->dev,
				 "Use PF Control I/F to re-enable the VF\n");
			set_bit(I40E_VF_STATE_DISABLED, &vf->vf_states);
		}
	}

	 
	clear_bit(__I40E_MDD_EVENT_PENDING, pf->state);
	reg = rd32(hw, I40E_PFINT_ICR0_ENA);
	reg |=  I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);
	i40e_flush(hw);
}

 
static void i40e_service_task(struct work_struct *work)
{
	struct i40e_pf *pf = container_of(work,
					  struct i40e_pf,
					  service_task);
	unsigned long start_time = jiffies;

	 
	if (test_bit(__I40E_RESET_RECOVERY_PENDING, pf->state) ||
	    test_bit(__I40E_SUSPENDED, pf->state))
		return;

	if (test_and_set_bit(__I40E_SERVICE_SCHED, pf->state))
		return;

	if (!test_bit(__I40E_RECOVERY_MODE, pf->state)) {
		i40e_detect_recover_hung(pf->vsi[pf->lan_vsi]);
		i40e_sync_filters_subtask(pf);
		i40e_reset_subtask(pf);
		i40e_handle_mdd_event(pf);
		i40e_vc_process_vflr_event(pf);
		i40e_watchdog_subtask(pf);
		i40e_fdir_reinit_subtask(pf);
		if (test_and_clear_bit(__I40E_CLIENT_RESET, pf->state)) {
			 
			i40e_notify_client_of_netdev_close(pf->vsi[pf->lan_vsi],
							   true);
		} else {
			i40e_client_subtask(pf);
			if (test_and_clear_bit(__I40E_CLIENT_L2_CHANGE,
					       pf->state))
				i40e_notify_client_of_l2_param_changes(
								pf->vsi[pf->lan_vsi]);
		}
		i40e_sync_filters_subtask(pf);
	} else {
		i40e_reset_subtask(pf);
	}

	i40e_clean_adminq_subtask(pf);

	 
	smp_mb__before_atomic();
	clear_bit(__I40E_SERVICE_SCHED, pf->state);

	 
	if (time_after(jiffies, (start_time + pf->service_timer_period)) ||
	    test_bit(__I40E_ADMINQ_EVENT_PENDING, pf->state)		 ||
	    test_bit(__I40E_MDD_EVENT_PENDING, pf->state)		 ||
	    test_bit(__I40E_VFLR_EVENT_PENDING, pf->state))
		i40e_service_event_schedule(pf);
}

 
static void i40e_service_timer(struct timer_list *t)
{
	struct i40e_pf *pf = from_timer(pf, t, service_timer);

	mod_timer(&pf->service_timer,
		  round_jiffies(jiffies + pf->service_timer_period));
	i40e_service_event_schedule(pf);
}

 
static int i40e_set_num_rings_in_vsi(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;

	switch (vsi->type) {
	case I40E_VSI_MAIN:
		vsi->alloc_queue_pairs = pf->num_lan_qps;
		if (!vsi->num_tx_desc)
			vsi->num_tx_desc = ALIGN(I40E_DEFAULT_NUM_DESCRIPTORS,
						 I40E_REQ_DESCRIPTOR_MULTIPLE);
		if (!vsi->num_rx_desc)
			vsi->num_rx_desc = ALIGN(I40E_DEFAULT_NUM_DESCRIPTORS,
						 I40E_REQ_DESCRIPTOR_MULTIPLE);
		if (pf->flags & I40E_FLAG_MSIX_ENABLED)
			vsi->num_q_vectors = pf->num_lan_msix;
		else
			vsi->num_q_vectors = 1;

		break;

	case I40E_VSI_FDIR:
		vsi->alloc_queue_pairs = 1;
		vsi->num_tx_desc = ALIGN(I40E_FDIR_RING_COUNT,
					 I40E_REQ_DESCRIPTOR_MULTIPLE);
		vsi->num_rx_desc = ALIGN(I40E_FDIR_RING_COUNT,
					 I40E_REQ_DESCRIPTOR_MULTIPLE);
		vsi->num_q_vectors = pf->num_fdsb_msix;
		break;

	case I40E_VSI_VMDQ2:
		vsi->alloc_queue_pairs = pf->num_vmdq_qps;
		if (!vsi->num_tx_desc)
			vsi->num_tx_desc = ALIGN(I40E_DEFAULT_NUM_DESCRIPTORS,
						 I40E_REQ_DESCRIPTOR_MULTIPLE);
		if (!vsi->num_rx_desc)
			vsi->num_rx_desc = ALIGN(I40E_DEFAULT_NUM_DESCRIPTORS,
						 I40E_REQ_DESCRIPTOR_MULTIPLE);
		vsi->num_q_vectors = pf->num_vmdq_msix;
		break;

	case I40E_VSI_SRIOV:
		vsi->alloc_queue_pairs = pf->num_vf_qps;
		if (!vsi->num_tx_desc)
			vsi->num_tx_desc = ALIGN(I40E_DEFAULT_NUM_DESCRIPTORS,
						 I40E_REQ_DESCRIPTOR_MULTIPLE);
		if (!vsi->num_rx_desc)
			vsi->num_rx_desc = ALIGN(I40E_DEFAULT_NUM_DESCRIPTORS,
						 I40E_REQ_DESCRIPTOR_MULTIPLE);
		break;

	default:
		WARN_ON(1);
		return -ENODATA;
	}

	if (is_kdump_kernel()) {
		vsi->num_tx_desc = I40E_MIN_NUM_DESCRIPTORS;
		vsi->num_rx_desc = I40E_MIN_NUM_DESCRIPTORS;
	}

	return 0;
}

 
static int i40e_vsi_alloc_arrays(struct i40e_vsi *vsi, bool alloc_qvectors)
{
	struct i40e_ring **next_rings;
	int size;
	int ret = 0;

	 
	size = sizeof(struct i40e_ring *) * vsi->alloc_queue_pairs *
	       (i40e_enabled_xdp_vsi(vsi) ? 3 : 2);
	vsi->tx_rings = kzalloc(size, GFP_KERNEL);
	if (!vsi->tx_rings)
		return -ENOMEM;
	next_rings = vsi->tx_rings + vsi->alloc_queue_pairs;
	if (i40e_enabled_xdp_vsi(vsi)) {
		vsi->xdp_rings = next_rings;
		next_rings += vsi->alloc_queue_pairs;
	}
	vsi->rx_rings = next_rings;

	if (alloc_qvectors) {
		 
		size = sizeof(struct i40e_q_vector *) * vsi->num_q_vectors;
		vsi->q_vectors = kzalloc(size, GFP_KERNEL);
		if (!vsi->q_vectors) {
			ret = -ENOMEM;
			goto err_vectors;
		}
	}
	return ret;

err_vectors:
	kfree(vsi->tx_rings);
	return ret;
}

 
static int i40e_vsi_mem_alloc(struct i40e_pf *pf, enum i40e_vsi_type type)
{
	int ret = -ENODEV;
	struct i40e_vsi *vsi;
	int vsi_idx;
	int i;

	 
	mutex_lock(&pf->switch_mutex);

	 
	i = pf->next_vsi;
	while (i < pf->num_alloc_vsi && pf->vsi[i])
		i++;
	if (i >= pf->num_alloc_vsi) {
		i = 0;
		while (i < pf->next_vsi && pf->vsi[i])
			i++;
	}

	if (i < pf->num_alloc_vsi && !pf->vsi[i]) {
		vsi_idx = i;              
	} else {
		ret = -ENODEV;
		goto unlock_pf;   
	}
	pf->next_vsi = ++i;

	vsi = kzalloc(sizeof(*vsi), GFP_KERNEL);
	if (!vsi) {
		ret = -ENOMEM;
		goto unlock_pf;
	}
	vsi->type = type;
	vsi->back = pf;
	set_bit(__I40E_VSI_DOWN, vsi->state);
	vsi->flags = 0;
	vsi->idx = vsi_idx;
	vsi->int_rate_limit = 0;
	vsi->rss_table_size = (vsi->type == I40E_VSI_MAIN) ?
				pf->rss_table_size : 64;
	vsi->netdev_registered = false;
	vsi->work_limit = I40E_DEFAULT_IRQ_WORK;
	hash_init(vsi->mac_filter_hash);
	vsi->irqs_ready = false;

	if (type == I40E_VSI_MAIN) {
		vsi->af_xdp_zc_qps = bitmap_zalloc(pf->num_lan_qps, GFP_KERNEL);
		if (!vsi->af_xdp_zc_qps)
			goto err_rings;
	}

	ret = i40e_set_num_rings_in_vsi(vsi);
	if (ret)
		goto err_rings;

	ret = i40e_vsi_alloc_arrays(vsi, true);
	if (ret)
		goto err_rings;

	 
	i40e_vsi_setup_irqhandler(vsi, i40e_msix_clean_rings);

	 
	spin_lock_init(&vsi->mac_filter_hash_lock);
	pf->vsi[vsi_idx] = vsi;
	ret = vsi_idx;
	goto unlock_pf;

err_rings:
	bitmap_free(vsi->af_xdp_zc_qps);
	pf->next_vsi = i - 1;
	kfree(vsi);
unlock_pf:
	mutex_unlock(&pf->switch_mutex);
	return ret;
}

 
static void i40e_vsi_free_arrays(struct i40e_vsi *vsi, bool free_qvectors)
{
	 
	if (free_qvectors) {
		kfree(vsi->q_vectors);
		vsi->q_vectors = NULL;
	}
	kfree(vsi->tx_rings);
	vsi->tx_rings = NULL;
	vsi->rx_rings = NULL;
	vsi->xdp_rings = NULL;
}

 
static void i40e_clear_rss_config_user(struct i40e_vsi *vsi)
{
	if (!vsi)
		return;

	kfree(vsi->rss_hkey_user);
	vsi->rss_hkey_user = NULL;

	kfree(vsi->rss_lut_user);
	vsi->rss_lut_user = NULL;
}

 
static int i40e_vsi_clear(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf;

	if (!vsi)
		return 0;

	if (!vsi->back)
		goto free_vsi;
	pf = vsi->back;

	mutex_lock(&pf->switch_mutex);
	if (!pf->vsi[vsi->idx]) {
		dev_err(&pf->pdev->dev, "pf->vsi[%d] is NULL, just free vsi[%d](type %d)\n",
			vsi->idx, vsi->idx, vsi->type);
		goto unlock_vsi;
	}

	if (pf->vsi[vsi->idx] != vsi) {
		dev_err(&pf->pdev->dev,
			"pf->vsi[%d](type %d) != vsi[%d](type %d): no free!\n",
			pf->vsi[vsi->idx]->idx,
			pf->vsi[vsi->idx]->type,
			vsi->idx, vsi->type);
		goto unlock_vsi;
	}

	 
	i40e_put_lump(pf->qp_pile, vsi->base_queue, vsi->idx);
	i40e_put_lump(pf->irq_pile, vsi->base_vector, vsi->idx);

	bitmap_free(vsi->af_xdp_zc_qps);
	i40e_vsi_free_arrays(vsi, true);
	i40e_clear_rss_config_user(vsi);

	pf->vsi[vsi->idx] = NULL;
	if (vsi->idx < pf->next_vsi)
		pf->next_vsi = vsi->idx;

unlock_vsi:
	mutex_unlock(&pf->switch_mutex);
free_vsi:
	kfree(vsi);

	return 0;
}

 
static void i40e_vsi_clear_rings(struct i40e_vsi *vsi)
{
	int i;

	if (vsi->tx_rings && vsi->tx_rings[0]) {
		for (i = 0; i < vsi->alloc_queue_pairs; i++) {
			kfree_rcu(vsi->tx_rings[i], rcu);
			WRITE_ONCE(vsi->tx_rings[i], NULL);
			WRITE_ONCE(vsi->rx_rings[i], NULL);
			if (vsi->xdp_rings)
				WRITE_ONCE(vsi->xdp_rings[i], NULL);
		}
	}
}

 
static int i40e_alloc_rings(struct i40e_vsi *vsi)
{
	int i, qpv = i40e_enabled_xdp_vsi(vsi) ? 3 : 2;
	struct i40e_pf *pf = vsi->back;
	struct i40e_ring *ring;

	 
	for (i = 0; i < vsi->alloc_queue_pairs; i++) {
		 
		ring = kcalloc(qpv, sizeof(struct i40e_ring), GFP_KERNEL);
		if (!ring)
			goto err_out;

		ring->queue_index = i;
		ring->reg_idx = vsi->base_queue + i;
		ring->ring_active = false;
		ring->vsi = vsi;
		ring->netdev = vsi->netdev;
		ring->dev = &pf->pdev->dev;
		ring->count = vsi->num_tx_desc;
		ring->size = 0;
		ring->dcb_tc = 0;
		if (vsi->back->hw_features & I40E_HW_WB_ON_ITR_CAPABLE)
			ring->flags = I40E_TXR_FLAGS_WB_ON_ITR;
		ring->itr_setting = pf->tx_itr_default;
		WRITE_ONCE(vsi->tx_rings[i], ring++);

		if (!i40e_enabled_xdp_vsi(vsi))
			goto setup_rx;

		ring->queue_index = vsi->alloc_queue_pairs + i;
		ring->reg_idx = vsi->base_queue + ring->queue_index;
		ring->ring_active = false;
		ring->vsi = vsi;
		ring->netdev = NULL;
		ring->dev = &pf->pdev->dev;
		ring->count = vsi->num_tx_desc;
		ring->size = 0;
		ring->dcb_tc = 0;
		if (vsi->back->hw_features & I40E_HW_WB_ON_ITR_CAPABLE)
			ring->flags = I40E_TXR_FLAGS_WB_ON_ITR;
		set_ring_xdp(ring);
		ring->itr_setting = pf->tx_itr_default;
		WRITE_ONCE(vsi->xdp_rings[i], ring++);

setup_rx:
		ring->queue_index = i;
		ring->reg_idx = vsi->base_queue + i;
		ring->ring_active = false;
		ring->vsi = vsi;
		ring->netdev = vsi->netdev;
		ring->dev = &pf->pdev->dev;
		ring->count = vsi->num_rx_desc;
		ring->size = 0;
		ring->dcb_tc = 0;
		ring->itr_setting = pf->rx_itr_default;
		WRITE_ONCE(vsi->rx_rings[i], ring);
	}

	return 0;

err_out:
	i40e_vsi_clear_rings(vsi);
	return -ENOMEM;
}

 
static int i40e_reserve_msix_vectors(struct i40e_pf *pf, int vectors)
{
	vectors = pci_enable_msix_range(pf->pdev, pf->msix_entries,
					I40E_MIN_MSIX, vectors);
	if (vectors < 0) {
		dev_info(&pf->pdev->dev,
			 "MSI-X vector reservation failed: %d\n", vectors);
		vectors = 0;
	}

	return vectors;
}

 
static int i40e_init_msix(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	int cpus, extra_vectors;
	int vectors_left;
	int v_budget, i;
	int v_actual;
	int iwarp_requested = 0;

	if (!(pf->flags & I40E_FLAG_MSIX_ENABLED))
		return -ENODEV;

	 
	vectors_left = hw->func_caps.num_msix_vectors;
	v_budget = 0;

	 
	if (vectors_left) {
		v_budget++;
		vectors_left--;
	}

	 
	cpus = num_online_cpus();
	pf->num_lan_msix = min_t(int, cpus, vectors_left / 2);
	vectors_left -= pf->num_lan_msix;

	 
	if (pf->flags & I40E_FLAG_FD_SB_ENABLED) {
		if (vectors_left) {
			pf->num_fdsb_msix = 1;
			v_budget++;
			vectors_left--;
		} else {
			pf->num_fdsb_msix = 0;
		}
	}

	 
	if (pf->flags & I40E_FLAG_IWARP_ENABLED) {
		iwarp_requested = pf->num_iwarp_msix;

		if (!vectors_left)
			pf->num_iwarp_msix = 0;
		else if (vectors_left < pf->num_iwarp_msix)
			pf->num_iwarp_msix = 1;
		v_budget += pf->num_iwarp_msix;
		vectors_left -= pf->num_iwarp_msix;
	}

	 
	if (pf->flags & I40E_FLAG_VMDQ_ENABLED) {
		if (!vectors_left) {
			pf->num_vmdq_msix = 0;
			pf->num_vmdq_qps = 0;
		} else {
			int vmdq_vecs_wanted =
				pf->num_vmdq_vsis * pf->num_vmdq_qps;
			int vmdq_vecs =
				min_t(int, vectors_left, vmdq_vecs_wanted);

			 
			if (vectors_left < vmdq_vecs_wanted) {
				pf->num_vmdq_qps = 1;
				vmdq_vecs_wanted = pf->num_vmdq_vsis;
				vmdq_vecs = min_t(int,
						  vectors_left,
						  vmdq_vecs_wanted);
			}
			pf->num_vmdq_msix = pf->num_vmdq_qps;

			v_budget += vmdq_vecs;
			vectors_left -= vmdq_vecs;
		}
	}

	 
	extra_vectors = min_t(int, cpus - pf->num_lan_msix, vectors_left);
	pf->num_lan_msix += extra_vectors;
	vectors_left -= extra_vectors;

	WARN(vectors_left < 0,
	     "Calculation of remaining vectors underflowed. This is an accounting bug when determining total MSI-X vectors.\n");

	v_budget += pf->num_lan_msix;
	pf->msix_entries = kcalloc(v_budget, sizeof(struct msix_entry),
				   GFP_KERNEL);
	if (!pf->msix_entries)
		return -ENOMEM;

	for (i = 0; i < v_budget; i++)
		pf->msix_entries[i].entry = i;
	v_actual = i40e_reserve_msix_vectors(pf, v_budget);

	if (v_actual < I40E_MIN_MSIX) {
		pf->flags &= ~I40E_FLAG_MSIX_ENABLED;
		kfree(pf->msix_entries);
		pf->msix_entries = NULL;
		pci_disable_msix(pf->pdev);
		return -ENODEV;

	} else if (v_actual == I40E_MIN_MSIX) {
		 
		pf->num_vmdq_vsis = 0;
		pf->num_vmdq_qps = 0;
		pf->num_lan_qps = 1;
		pf->num_lan_msix = 1;

	} else if (v_actual != v_budget) {
		 
		int vec;

		dev_info(&pf->pdev->dev,
			 "MSI-X vector limit reached with %d, wanted %d, attempting to redistribute vectors\n",
			 v_actual, v_budget);
		 
		vec = v_actual - 1;

		 
		pf->num_vmdq_msix = 1;     
		pf->num_vmdq_vsis = 1;
		pf->num_vmdq_qps = 1;

		 
		switch (vec) {
		case 2:
			pf->num_lan_msix = 1;
			break;
		case 3:
			if (pf->flags & I40E_FLAG_IWARP_ENABLED) {
				pf->num_lan_msix = 1;
				pf->num_iwarp_msix = 1;
			} else {
				pf->num_lan_msix = 2;
			}
			break;
		default:
			if (pf->flags & I40E_FLAG_IWARP_ENABLED) {
				pf->num_iwarp_msix = min_t(int, (vec / 3),
						 iwarp_requested);
				pf->num_vmdq_vsis = min_t(int, (vec / 3),
						  I40E_DEFAULT_NUM_VMDQ_VSI);
			} else {
				pf->num_vmdq_vsis = min_t(int, (vec / 2),
						  I40E_DEFAULT_NUM_VMDQ_VSI);
			}
			if (pf->flags & I40E_FLAG_FD_SB_ENABLED) {
				pf->num_fdsb_msix = 1;
				vec--;
			}
			pf->num_lan_msix = min_t(int,
			       (vec - (pf->num_iwarp_msix + pf->num_vmdq_vsis)),
							      pf->num_lan_msix);
			pf->num_lan_qps = pf->num_lan_msix;
			break;
		}
	}

	if ((pf->flags & I40E_FLAG_FD_SB_ENABLED) &&
	    (pf->num_fdsb_msix == 0)) {
		dev_info(&pf->pdev->dev, "Sideband Flowdir disabled, not enough MSI-X vectors\n");
		pf->flags &= ~I40E_FLAG_FD_SB_ENABLED;
		pf->flags |= I40E_FLAG_FD_SB_INACTIVE;
	}
	if ((pf->flags & I40E_FLAG_VMDQ_ENABLED) &&
	    (pf->num_vmdq_msix == 0)) {
		dev_info(&pf->pdev->dev, "VMDq disabled, not enough MSI-X vectors\n");
		pf->flags &= ~I40E_FLAG_VMDQ_ENABLED;
	}

	if ((pf->flags & I40E_FLAG_IWARP_ENABLED) &&
	    (pf->num_iwarp_msix == 0)) {
		dev_info(&pf->pdev->dev, "IWARP disabled, not enough MSI-X vectors\n");
		pf->flags &= ~I40E_FLAG_IWARP_ENABLED;
	}
	i40e_debug(&pf->hw, I40E_DEBUG_INIT,
		   "MSI-X vector distribution: PF %d, VMDq %d, FDSB %d, iWARP %d\n",
		   pf->num_lan_msix,
		   pf->num_vmdq_msix * pf->num_vmdq_vsis,
		   pf->num_fdsb_msix,
		   pf->num_iwarp_msix);

	return v_actual;
}

 
static int i40e_vsi_alloc_q_vector(struct i40e_vsi *vsi, int v_idx)
{
	struct i40e_q_vector *q_vector;

	 
	q_vector = kzalloc(sizeof(struct i40e_q_vector), GFP_KERNEL);
	if (!q_vector)
		return -ENOMEM;

	q_vector->vsi = vsi;
	q_vector->v_idx = v_idx;
	cpumask_copy(&q_vector->affinity_mask, cpu_possible_mask);

	if (vsi->netdev)
		netif_napi_add(vsi->netdev, &q_vector->napi, i40e_napi_poll);

	 
	vsi->q_vectors[v_idx] = q_vector;

	return 0;
}

 
static int i40e_vsi_alloc_q_vectors(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int err, v_idx, num_q_vectors;

	 
	if (pf->flags & I40E_FLAG_MSIX_ENABLED)
		num_q_vectors = vsi->num_q_vectors;
	else if (vsi == pf->vsi[pf->lan_vsi])
		num_q_vectors = 1;
	else
		return -EINVAL;

	for (v_idx = 0; v_idx < num_q_vectors; v_idx++) {
		err = i40e_vsi_alloc_q_vector(vsi, v_idx);
		if (err)
			goto err_out;
	}

	return 0;

err_out:
	while (v_idx--)
		i40e_free_q_vector(vsi, v_idx);

	return err;
}

 
static int i40e_init_interrupt_scheme(struct i40e_pf *pf)
{
	int vectors = 0;
	ssize_t size;

	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		vectors = i40e_init_msix(pf);
		if (vectors < 0) {
			pf->flags &= ~(I40E_FLAG_MSIX_ENABLED	|
				       I40E_FLAG_IWARP_ENABLED	|
				       I40E_FLAG_RSS_ENABLED	|
				       I40E_FLAG_DCB_CAPABLE	|
				       I40E_FLAG_DCB_ENABLED	|
				       I40E_FLAG_SRIOV_ENABLED	|
				       I40E_FLAG_FD_SB_ENABLED	|
				       I40E_FLAG_FD_ATR_ENABLED	|
				       I40E_FLAG_VMDQ_ENABLED);
			pf->flags |= I40E_FLAG_FD_SB_INACTIVE;

			 
			i40e_determine_queue_usage(pf);
		}
	}

	if (!(pf->flags & I40E_FLAG_MSIX_ENABLED) &&
	    (pf->flags & I40E_FLAG_MSI_ENABLED)) {
		dev_info(&pf->pdev->dev, "MSI-X not available, trying MSI\n");
		vectors = pci_enable_msi(pf->pdev);
		if (vectors < 0) {
			dev_info(&pf->pdev->dev, "MSI init failed - %d\n",
				 vectors);
			pf->flags &= ~I40E_FLAG_MSI_ENABLED;
		}
		vectors = 1;   
	}

	if (!(pf->flags & (I40E_FLAG_MSIX_ENABLED | I40E_FLAG_MSI_ENABLED)))
		dev_info(&pf->pdev->dev, "MSI-X and MSI not available, falling back to Legacy IRQ\n");

	 
	size = sizeof(struct i40e_lump_tracking) + (sizeof(u16) * vectors);
	pf->irq_pile = kzalloc(size, GFP_KERNEL);
	if (!pf->irq_pile)
		return -ENOMEM;

	pf->irq_pile->num_entries = vectors;

	 
	(void)i40e_get_lump(pf, pf->irq_pile, 1, I40E_PILE_VALID_BIT - 1);

	return 0;
}

 
static int i40e_restore_interrupt_scheme(struct i40e_pf *pf)
{
	int err, i;

	 
	pf->flags |= (I40E_FLAG_MSIX_ENABLED | I40E_FLAG_MSI_ENABLED);

	err = i40e_init_interrupt_scheme(pf);
	if (err)
		return err;

	 
	for (i = 0; i < pf->num_alloc_vsi; i++) {
		if (pf->vsi[i]) {
			err = i40e_vsi_alloc_q_vectors(pf->vsi[i]);
			if (err)
				goto err_unwind;
			i40e_vsi_map_rings_to_vectors(pf->vsi[i]);
		}
	}

	err = i40e_setup_misc_vector(pf);
	if (err)
		goto err_unwind;

	if (pf->flags & I40E_FLAG_IWARP_ENABLED)
		i40e_client_update_msix_info(pf);

	return 0;

err_unwind:
	while (i--) {
		if (pf->vsi[i])
			i40e_vsi_free_q_vectors(pf->vsi[i]);
	}

	return err;
}

 
static int i40e_setup_misc_vector_for_recovery_mode(struct i40e_pf *pf)
{
	int err;

	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		err = i40e_setup_misc_vector(pf);

		if (err) {
			dev_info(&pf->pdev->dev,
				 "MSI-X misc vector request failed, error %d\n",
				 err);
			return err;
		}
	} else {
		u32 flags = pf->flags & I40E_FLAG_MSI_ENABLED ? 0 : IRQF_SHARED;

		err = request_irq(pf->pdev->irq, i40e_intr, flags,
				  pf->int_name, pf);

		if (err) {
			dev_info(&pf->pdev->dev,
				 "MSI/legacy misc vector request failed, error %d\n",
				 err);
			return err;
		}
		i40e_enable_misc_int_causes(pf);
		i40e_irq_dynamic_enable_icr0(pf);
	}

	return 0;
}

 
static int i40e_setup_misc_vector(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	int err = 0;

	 
	if (!test_and_set_bit(__I40E_MISC_IRQ_REQUESTED, pf->state)) {
		err = request_irq(pf->msix_entries[0].vector,
				  i40e_intr, 0, pf->int_name, pf);
		if (err) {
			clear_bit(__I40E_MISC_IRQ_REQUESTED, pf->state);
			dev_info(&pf->pdev->dev,
				 "request_irq for %s failed: %d\n",
				 pf->int_name, err);
			return -EFAULT;
		}
	}

	i40e_enable_misc_int_causes(pf);

	 
	wr32(hw, I40E_PFINT_LNKLST0, I40E_QUEUE_END_OF_LIST);
	wr32(hw, I40E_PFINT_ITR0(I40E_RX_ITR), I40E_ITR_8K >> 1);

	i40e_flush(hw);

	i40e_irq_dynamic_enable_icr0(pf);

	return err;
}

 
static int i40e_get_rss_aq(struct i40e_vsi *vsi, const u8 *seed,
			   u8 *lut, u16 lut_size)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	int ret = 0;

	if (seed) {
		ret = i40e_aq_get_rss_key(hw, vsi->id,
			(struct i40e_aqc_get_set_rss_key_data *)seed);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "Cannot get RSS key, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
			return ret;
		}
	}

	if (lut) {
		bool pf_lut = vsi->type == I40E_VSI_MAIN;

		ret = i40e_aq_get_rss_lut(hw, vsi->id, pf_lut, lut, lut_size);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "Cannot get RSS lut, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
			return ret;
		}
	}

	return ret;
}

 
static int i40e_config_rss_reg(struct i40e_vsi *vsi, const u8 *seed,
			       const u8 *lut, u16 lut_size)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	u16 vf_id = vsi->vf_id;
	u8 i;

	 
	if (seed) {
		u32 *seed_dw = (u32 *)seed;

		if (vsi->type == I40E_VSI_MAIN) {
			for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++)
				wr32(hw, I40E_PFQF_HKEY(i), seed_dw[i]);
		} else if (vsi->type == I40E_VSI_SRIOV) {
			for (i = 0; i <= I40E_VFQF_HKEY1_MAX_INDEX; i++)
				wr32(hw, I40E_VFQF_HKEY1(i, vf_id), seed_dw[i]);
		} else {
			dev_err(&pf->pdev->dev, "Cannot set RSS seed - invalid VSI type\n");
		}
	}

	if (lut) {
		u32 *lut_dw = (u32 *)lut;

		if (vsi->type == I40E_VSI_MAIN) {
			if (lut_size != I40E_HLUT_ARRAY_SIZE)
				return -EINVAL;
			for (i = 0; i <= I40E_PFQF_HLUT_MAX_INDEX; i++)
				wr32(hw, I40E_PFQF_HLUT(i), lut_dw[i]);
		} else if (vsi->type == I40E_VSI_SRIOV) {
			if (lut_size != I40E_VF_HLUT_ARRAY_SIZE)
				return -EINVAL;
			for (i = 0; i <= I40E_VFQF_HLUT_MAX_INDEX; i++)
				wr32(hw, I40E_VFQF_HLUT1(i, vf_id), lut_dw[i]);
		} else {
			dev_err(&pf->pdev->dev, "Cannot set RSS LUT - invalid VSI type\n");
		}
	}
	i40e_flush(hw);

	return 0;
}

 
static int i40e_get_rss_reg(struct i40e_vsi *vsi, u8 *seed,
			    u8 *lut, u16 lut_size)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	u16 i;

	if (seed) {
		u32 *seed_dw = (u32 *)seed;

		for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++)
			seed_dw[i] = i40e_read_rx_ctl(hw, I40E_PFQF_HKEY(i));
	}
	if (lut) {
		u32 *lut_dw = (u32 *)lut;

		if (lut_size != I40E_HLUT_ARRAY_SIZE)
			return -EINVAL;
		for (i = 0; i <= I40E_PFQF_HLUT_MAX_INDEX; i++)
			lut_dw[i] = rd32(hw, I40E_PFQF_HLUT(i));
	}

	return 0;
}

 
int i40e_config_rss(struct i40e_vsi *vsi, u8 *seed, u8 *lut, u16 lut_size)
{
	struct i40e_pf *pf = vsi->back;

	if (pf->hw_features & I40E_HW_RSS_AQ_CAPABLE)
		return i40e_config_rss_aq(vsi, seed, lut, lut_size);
	else
		return i40e_config_rss_reg(vsi, seed, lut, lut_size);
}

 
int i40e_get_rss(struct i40e_vsi *vsi, u8 *seed, u8 *lut, u16 lut_size)
{
	struct i40e_pf *pf = vsi->back;

	if (pf->hw_features & I40E_HW_RSS_AQ_CAPABLE)
		return i40e_get_rss_aq(vsi, seed, lut, lut_size);
	else
		return i40e_get_rss_reg(vsi, seed, lut, lut_size);
}

 
void i40e_fill_rss_lut(struct i40e_pf *pf, u8 *lut,
		       u16 rss_table_size, u16 rss_size)
{
	u16 i;

	for (i = 0; i < rss_table_size; i++)
		lut[i] = i % rss_size;
}

 
static int i40e_pf_config_rss(struct i40e_pf *pf)
{
	struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
	u8 seed[I40E_HKEY_ARRAY_SIZE];
	u8 *lut;
	struct i40e_hw *hw = &pf->hw;
	u32 reg_val;
	u64 hena;
	int ret;

	 
	hena = (u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0)) |
		((u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1)) << 32);
	hena |= i40e_pf_get_default_rss_hena(pf);

	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(0), (u32)hena);
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(1), (u32)(hena >> 32));

	 
	reg_val = i40e_read_rx_ctl(hw, I40E_PFQF_CTL_0);
	reg_val = (pf->rss_table_size == 512) ?
			(reg_val | I40E_PFQF_CTL_0_HASHLUTSIZE_512) :
			(reg_val & ~I40E_PFQF_CTL_0_HASHLUTSIZE_512);
	i40e_write_rx_ctl(hw, I40E_PFQF_CTL_0, reg_val);

	 
	if (!vsi->rss_size) {
		u16 qcount;
		 
		qcount = vsi->num_queue_pairs /
			 (vsi->tc_config.numtc ? vsi->tc_config.numtc : 1);
		vsi->rss_size = min_t(int, pf->alloc_rss_size, qcount);
	}
	if (!vsi->rss_size)
		return -EINVAL;

	lut = kzalloc(vsi->rss_table_size, GFP_KERNEL);
	if (!lut)
		return -ENOMEM;

	 
	if (vsi->rss_lut_user)
		memcpy(lut, vsi->rss_lut_user, vsi->rss_table_size);
	else
		i40e_fill_rss_lut(pf, lut, vsi->rss_table_size, vsi->rss_size);

	 
	if (vsi->rss_hkey_user)
		memcpy(seed, vsi->rss_hkey_user, I40E_HKEY_ARRAY_SIZE);
	else
		netdev_rss_key_fill((void *)seed, I40E_HKEY_ARRAY_SIZE);
	ret = i40e_config_rss(vsi, seed, lut, vsi->rss_table_size);
	kfree(lut);

	return ret;
}

 
int i40e_reconfig_rss_queues(struct i40e_pf *pf, int queue_count)
{
	struct i40e_vsi *vsi = pf->vsi[pf->lan_vsi];
	int new_rss_size;

	if (!(pf->flags & I40E_FLAG_RSS_ENABLED))
		return 0;

	queue_count = min_t(int, queue_count, num_online_cpus());
	new_rss_size = min_t(int, queue_count, pf->rss_size_max);

	if (queue_count != vsi->num_queue_pairs) {
		u16 qcount;

		vsi->req_queue_pairs = queue_count;
		i40e_prep_for_reset(pf);
		if (test_bit(__I40E_IN_REMOVE, pf->state))
			return pf->alloc_rss_size;

		pf->alloc_rss_size = new_rss_size;

		i40e_reset_and_rebuild(pf, true, true);

		 
		if (queue_count < vsi->rss_size) {
			i40e_clear_rss_config_user(vsi);
			dev_dbg(&pf->pdev->dev,
				"discard user configured hash keys and lut\n");
		}

		 
		qcount = vsi->num_queue_pairs / vsi->tc_config.numtc;
		vsi->rss_size = min_t(int, pf->alloc_rss_size, qcount);

		i40e_pf_config_rss(pf);
	}
	dev_info(&pf->pdev->dev, "User requested queue count/HW max RSS count:  %d/%d\n",
		 vsi->req_queue_pairs, pf->rss_size_max);
	return pf->alloc_rss_size;
}

 
int i40e_get_partition_bw_setting(struct i40e_pf *pf)
{
	bool min_valid, max_valid;
	u32 max_bw, min_bw;
	int status;

	status = i40e_read_bw_from_alt_ram(&pf->hw, &max_bw, &min_bw,
					   &min_valid, &max_valid);

	if (!status) {
		if (min_valid)
			pf->min_bw = min_bw;
		if (max_valid)
			pf->max_bw = max_bw;
	}

	return status;
}

 
int i40e_set_partition_bw_setting(struct i40e_pf *pf)
{
	struct i40e_aqc_configure_partition_bw_data bw_data;
	int status;

	memset(&bw_data, 0, sizeof(bw_data));

	 
	bw_data.pf_valid_bits = cpu_to_le16(BIT(pf->hw.pf_id));
	bw_data.max_bw[pf->hw.pf_id] = pf->max_bw & I40E_ALT_BW_VALUE_MASK;
	bw_data.min_bw[pf->hw.pf_id] = pf->min_bw & I40E_ALT_BW_VALUE_MASK;

	 
	status = i40e_aq_configure_partition_bw(&pf->hw, &bw_data, NULL);

	return status;
}

 
int i40e_commit_partition_bw_setting(struct i40e_pf *pf)
{
	 
	enum i40e_admin_queue_err last_aq_status;
	u16 nvm_word;
	int ret;

	if (pf->hw.partition_id != 1) {
		dev_info(&pf->pdev->dev,
			 "Commit BW only works on partition 1! This is partition %d",
			 pf->hw.partition_id);
		ret = -EOPNOTSUPP;
		goto bw_commit_out;
	}

	 
	ret = i40e_acquire_nvm(&pf->hw, I40E_RESOURCE_READ);
	last_aq_status = pf->hw.aq.asq_last_status;
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Cannot acquire NVM for read access, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, last_aq_status));
		goto bw_commit_out;
	}

	 
	ret = i40e_aq_read_nvm(&pf->hw,
			       I40E_SR_NVM_CONTROL_WORD,
			       0x10, sizeof(nvm_word), &nvm_word,
			       false, NULL);
	 
	last_aq_status = pf->hw.aq.asq_last_status;
	i40e_release_nvm(&pf->hw);
	if (ret) {
		dev_info(&pf->pdev->dev, "NVM read error, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, last_aq_status));
		goto bw_commit_out;
	}

	 
	msleep(50);

	 
	ret = i40e_acquire_nvm(&pf->hw, I40E_RESOURCE_WRITE);
	last_aq_status = pf->hw.aq.asq_last_status;
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "Cannot acquire NVM for write access, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, last_aq_status));
		goto bw_commit_out;
	}
	 
	ret = i40e_aq_update_nvm(&pf->hw,
				 I40E_SR_NVM_CONTROL_WORD,
				 0x10, sizeof(nvm_word),
				 &nvm_word, true, 0, NULL);
	 
	last_aq_status = pf->hw.aq.asq_last_status;
	i40e_release_nvm(&pf->hw);
	if (ret)
		dev_info(&pf->pdev->dev,
			 "BW settings NOT SAVED, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, last_aq_status));
bw_commit_out:

	return ret;
}

 
static bool i40e_is_total_port_shutdown_enabled(struct i40e_pf *pf)
{
#define I40E_TOTAL_PORT_SHUTDOWN_ENABLED	BIT(4)
#define I40E_FEATURES_ENABLE_PTR		0x2A
#define I40E_CURRENT_SETTING_PTR		0x2B
#define I40E_LINK_BEHAVIOR_WORD_OFFSET		0x2D
#define I40E_LINK_BEHAVIOR_WORD_LENGTH		0x1
#define I40E_LINK_BEHAVIOR_OS_FORCED_ENABLED	BIT(0)
#define I40E_LINK_BEHAVIOR_PORT_BIT_LENGTH	4
	u16 sr_emp_sr_settings_ptr = 0;
	u16 features_enable = 0;
	u16 link_behavior = 0;
	int read_status = 0;
	bool ret = false;

	read_status = i40e_read_nvm_word(&pf->hw,
					 I40E_SR_EMP_SR_SETTINGS_PTR,
					 &sr_emp_sr_settings_ptr);
	if (read_status)
		goto err_nvm;
	read_status = i40e_read_nvm_word(&pf->hw,
					 sr_emp_sr_settings_ptr +
					 I40E_FEATURES_ENABLE_PTR,
					 &features_enable);
	if (read_status)
		goto err_nvm;
	if (I40E_TOTAL_PORT_SHUTDOWN_ENABLED & features_enable) {
		read_status = i40e_read_nvm_module_data(&pf->hw,
							I40E_SR_EMP_SR_SETTINGS_PTR,
							I40E_CURRENT_SETTING_PTR,
							I40E_LINK_BEHAVIOR_WORD_OFFSET,
							I40E_LINK_BEHAVIOR_WORD_LENGTH,
							&link_behavior);
		if (read_status)
			goto err_nvm;
		link_behavior >>= (pf->hw.port * I40E_LINK_BEHAVIOR_PORT_BIT_LENGTH);
		ret = I40E_LINK_BEHAVIOR_OS_FORCED_ENABLED & link_behavior;
	}
	return ret;

err_nvm:
	dev_warn(&pf->pdev->dev,
		 "total-port-shutdown feature is off due to read nvm error: %pe\n",
		 ERR_PTR(read_status));
	return ret;
}

 
static int i40e_sw_init(struct i40e_pf *pf)
{
	int err = 0;
	int size;
	u16 pow;

	 
	pf->flags = I40E_FLAG_RX_CSUM_ENABLED |
		    I40E_FLAG_MSI_ENABLED     |
		    I40E_FLAG_MSIX_ENABLED;

	 
	pf->rx_itr_default = I40E_ITR_RX_DEF;
	pf->tx_itr_default = I40E_ITR_TX_DEF;

	 
	pf->rss_size_max = BIT(pf->hw.func_caps.rss_table_entry_width);
	pf->alloc_rss_size = 1;
	pf->rss_table_size = pf->hw.func_caps.rss_table_size;
	pf->rss_size_max = min_t(int, pf->rss_size_max,
				 pf->hw.func_caps.num_tx_qp);

	 
	pow = roundup_pow_of_two(num_online_cpus());
	pf->rss_size_max = min_t(int, pf->rss_size_max, pow);

	if (pf->hw.func_caps.rss) {
		pf->flags |= I40E_FLAG_RSS_ENABLED;
		pf->alloc_rss_size = min_t(int, pf->rss_size_max,
					   num_online_cpus());
	}

	 
	if (pf->hw.func_caps.npar_enable || pf->hw.func_caps.flex10_enable) {
		pf->flags |= I40E_FLAG_MFP_ENABLED;
		dev_info(&pf->pdev->dev, "MFP mode Enabled\n");
		if (i40e_get_partition_bw_setting(pf)) {
			dev_warn(&pf->pdev->dev,
				 "Could not get partition bw settings\n");
		} else {
			dev_info(&pf->pdev->dev,
				 "Partition BW Min = %8.8x, Max = %8.8x\n",
				 pf->min_bw, pf->max_bw);

			 
			i40e_set_partition_bw_setting(pf);
		}
	}

	if ((pf->hw.func_caps.fd_filters_guaranteed > 0) ||
	    (pf->hw.func_caps.fd_filters_best_effort > 0)) {
		pf->flags |= I40E_FLAG_FD_ATR_ENABLED;
		pf->atr_sample_rate = I40E_DEFAULT_ATR_SAMPLE_RATE;
		if (pf->flags & I40E_FLAG_MFP_ENABLED &&
		    pf->hw.num_partitions > 1)
			dev_info(&pf->pdev->dev,
				 "Flow Director Sideband mode Disabled in MFP mode\n");
		else
			pf->flags |= I40E_FLAG_FD_SB_ENABLED;
		pf->fdir_pf_filter_count =
				 pf->hw.func_caps.fd_filters_guaranteed;
		pf->hw.fdir_shared_filter_count =
				 pf->hw.func_caps.fd_filters_best_effort;
	}

	if (pf->hw.mac.type == I40E_MAC_X722) {
		pf->hw_features |= (I40E_HW_RSS_AQ_CAPABLE |
				    I40E_HW_128_QP_RSS_CAPABLE |
				    I40E_HW_ATR_EVICT_CAPABLE |
				    I40E_HW_WB_ON_ITR_CAPABLE |
				    I40E_HW_MULTIPLE_TCP_UDP_RSS_PCTYPE |
				    I40E_HW_NO_PCI_LINK_CHECK |
				    I40E_HW_USE_SET_LLDP_MIB |
				    I40E_HW_GENEVE_OFFLOAD_CAPABLE |
				    I40E_HW_PTP_L4_CAPABLE |
				    I40E_HW_WOL_MC_MAGIC_PKT_WAKE |
				    I40E_HW_OUTER_UDP_CSUM_CAPABLE);

#define I40E_FDEVICT_PCTYPE_DEFAULT 0xc03
		if (rd32(&pf->hw, I40E_GLQF_FDEVICTENA(1)) !=
		    I40E_FDEVICT_PCTYPE_DEFAULT) {
			dev_warn(&pf->pdev->dev,
				 "FD EVICT PCTYPES are not right, disable FD HW EVICT\n");
			pf->hw_features &= ~I40E_HW_ATR_EVICT_CAPABLE;
		}
	} else if ((pf->hw.aq.api_maj_ver > 1) ||
		   ((pf->hw.aq.api_maj_ver == 1) &&
		    (pf->hw.aq.api_min_ver > 4))) {
		 
		pf->hw_features |= I40E_HW_GENEVE_OFFLOAD_CAPABLE;
	}

	 
	if (pf->hw_features & I40E_HW_ATR_EVICT_CAPABLE)
		pf->flags |= I40E_FLAG_HW_ATR_EVICT_ENABLED;

	if ((pf->hw.mac.type == I40E_MAC_XL710) &&
	    (((pf->hw.aq.fw_maj_ver == 4) && (pf->hw.aq.fw_min_ver < 33)) ||
	    (pf->hw.aq.fw_maj_ver < 4))) {
		pf->hw_features |= I40E_HW_RESTART_AUTONEG;
		 
		pf->hw_features |= I40E_HW_NO_DCB_SUPPORT;
	}

	 
	if ((pf->hw.mac.type == I40E_MAC_XL710) &&
	    (((pf->hw.aq.fw_maj_ver == 4) && (pf->hw.aq.fw_min_ver < 3)) ||
	    (pf->hw.aq.fw_maj_ver < 4)))
		pf->hw_features |= I40E_HW_STOP_FW_LLDP;

	 
	if ((pf->hw.mac.type == I40E_MAC_XL710) &&
	    (((pf->hw.aq.fw_maj_ver == 4) && (pf->hw.aq.fw_min_ver >= 40)) ||
	    (pf->hw.aq.fw_maj_ver >= 5)))
		pf->hw_features |= I40E_HW_USE_SET_LLDP_MIB;

	 
	if (pf->hw.mac.type == I40E_MAC_XL710 &&
	    pf->hw.aq.fw_maj_ver >= 6)
		pf->hw_features |= I40E_HW_PTP_L4_CAPABLE;

	if (pf->hw.func_caps.vmdq && num_online_cpus() != 1) {
		pf->num_vmdq_vsis = I40E_DEFAULT_NUM_VMDQ_VSI;
		pf->flags |= I40E_FLAG_VMDQ_ENABLED;
		pf->num_vmdq_qps = i40e_default_queues_per_vmdq(pf);
	}

	if (pf->hw.func_caps.iwarp && num_online_cpus() != 1) {
		pf->flags |= I40E_FLAG_IWARP_ENABLED;
		 
		pf->num_iwarp_msix = (int)num_online_cpus() + 1;
	}
	 
	if (pf->hw.mac.type == I40E_MAC_XL710 &&
	    pf->hw.func_caps.npar_enable &&
	    (pf->hw.flags & I40E_HW_FLAG_FW_LLDP_STOPPABLE))
		pf->hw.flags &= ~I40E_HW_FLAG_FW_LLDP_STOPPABLE;

#ifdef CONFIG_PCI_IOV
	if (pf->hw.func_caps.num_vfs && pf->hw.partition_id == 1) {
		pf->num_vf_qps = I40E_DEFAULT_QUEUES_PER_VF;
		pf->flags |= I40E_FLAG_SRIOV_ENABLED;
		pf->num_req_vfs = min_t(int,
					pf->hw.func_caps.num_vfs,
					I40E_MAX_VF_COUNT);
	}
#endif  
	pf->eeprom_version = 0xDEAD;
	pf->lan_veb = I40E_NO_VEB;
	pf->lan_vsi = I40E_NO_VSI;

	 
	pf->flags &= ~I40E_FLAG_VEB_STATS_ENABLED;

	 
	size = sizeof(struct i40e_lump_tracking)
		+ (sizeof(u16) * pf->hw.func_caps.num_tx_qp);
	pf->qp_pile = kzalloc(size, GFP_KERNEL);
	if (!pf->qp_pile) {
		err = -ENOMEM;
		goto sw_init_done;
	}
	pf->qp_pile->num_entries = pf->hw.func_caps.num_tx_qp;

	pf->tx_timeout_recovery_level = 1;

	if (pf->hw.mac.type != I40E_MAC_X722 &&
	    i40e_is_total_port_shutdown_enabled(pf)) {
		 
		pf->flags |= (I40E_FLAG_TOTAL_PORT_SHUTDOWN_ENABLED |
			      I40E_FLAG_LINK_DOWN_ON_CLOSE_ENABLED);
		dev_info(&pf->pdev->dev,
			 "total-port-shutdown was enabled, link-down-on-close is forced on\n");
	}
	mutex_init(&pf->switch_mutex);

sw_init_done:
	return err;
}

 
bool i40e_set_ntuple(struct i40e_pf *pf, netdev_features_t features)
{
	bool need_reset = false;

	 
	if (features & NETIF_F_NTUPLE) {
		 
		if (!(pf->flags & I40E_FLAG_FD_SB_ENABLED))
			need_reset = true;
		 
		if (pf->num_fdsb_msix > 0 && !pf->num_cloud_filters) {
			pf->flags |= I40E_FLAG_FD_SB_ENABLED;
			pf->flags &= ~I40E_FLAG_FD_SB_INACTIVE;
		}
	} else {
		 
		if (pf->flags & I40E_FLAG_FD_SB_ENABLED) {
			need_reset = true;
			i40e_fdir_filter_exit(pf);
		}
		pf->flags &= ~I40E_FLAG_FD_SB_ENABLED;
		clear_bit(__I40E_FD_SB_AUTO_DISABLED, pf->state);
		pf->flags |= I40E_FLAG_FD_SB_INACTIVE;

		 
		pf->fd_add_err = 0;
		pf->fd_atr_cnt = 0;
		 
		if (test_and_clear_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state))
			if ((pf->flags & I40E_FLAG_FD_ATR_ENABLED) &&
			    (I40E_DEBUG_FD & pf->hw.debug_mask))
				dev_info(&pf->pdev->dev, "ATR re-enabled.\n");
	}
	return need_reset;
}

 
static void i40e_clear_rss_lut(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	u16 vf_id = vsi->vf_id;
	u8 i;

	if (vsi->type == I40E_VSI_MAIN) {
		for (i = 0; i <= I40E_PFQF_HLUT_MAX_INDEX; i++)
			wr32(hw, I40E_PFQF_HLUT(i), 0);
	} else if (vsi->type == I40E_VSI_SRIOV) {
		for (i = 0; i <= I40E_VFQF_HLUT_MAX_INDEX; i++)
			i40e_write_rx_ctl(hw, I40E_VFQF_HLUT1(i, vf_id), 0);
	} else {
		dev_err(&pf->pdev->dev, "Cannot set RSS LUT - invalid VSI type\n");
	}
}

 
static int i40e_set_loopback(struct i40e_vsi *vsi, bool ena)
{
	bool if_running = netif_running(vsi->netdev) &&
			  !test_and_set_bit(__I40E_VSI_DOWN, vsi->state);
	int ret;

	if (if_running)
		i40e_down(vsi);

	ret = i40e_aq_set_mac_loopback(&vsi->back->hw, ena, NULL);
	if (ret)
		netdev_err(vsi->netdev, "Failed to toggle loopback state\n");
	if (if_running)
		i40e_up(vsi);

	return ret;
}

 
static int i40e_set_features(struct net_device *netdev,
			     netdev_features_t features)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	bool need_reset;

	if (features & NETIF_F_RXHASH && !(netdev->features & NETIF_F_RXHASH))
		i40e_pf_config_rss(pf);
	else if (!(features & NETIF_F_RXHASH) &&
		 netdev->features & NETIF_F_RXHASH)
		i40e_clear_rss_lut(vsi);

	if (features & NETIF_F_HW_VLAN_CTAG_RX)
		i40e_vlan_stripping_enable(vsi);
	else
		i40e_vlan_stripping_disable(vsi);

	if (!(features & NETIF_F_HW_TC) &&
	    (netdev->features & NETIF_F_HW_TC) && pf->num_cloud_filters) {
		dev_err(&pf->pdev->dev,
			"Offloaded tc filters active, can't turn hw_tc_offload off");
		return -EINVAL;
	}

	if (!(features & NETIF_F_HW_L2FW_DOFFLOAD) && vsi->macvlan_cnt)
		i40e_del_all_macvlans(vsi);

	need_reset = i40e_set_ntuple(pf, features);

	if (need_reset)
		i40e_do_reset(pf, I40E_PF_RESET_FLAG, true);

	if ((features ^ netdev->features) & NETIF_F_LOOPBACK)
		return i40e_set_loopback(vsi, !!(features & NETIF_F_LOOPBACK));

	return 0;
}

static int i40e_udp_tunnel_set_port(struct net_device *netdev,
				    unsigned int table, unsigned int idx,
				    struct udp_tunnel_info *ti)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_hw *hw = &np->vsi->back->hw;
	u8 type, filter_index;
	int ret;

	type = ti->type == UDP_TUNNEL_TYPE_VXLAN ? I40E_AQC_TUNNEL_TYPE_VXLAN :
						   I40E_AQC_TUNNEL_TYPE_NGE;

	ret = i40e_aq_add_udp_tunnel(hw, ntohs(ti->port), type, &filter_index,
				     NULL);
	if (ret) {
		netdev_info(netdev, "add UDP port failed, err %pe aq_err %s\n",
			    ERR_PTR(ret),
			    i40e_aq_str(hw, hw->aq.asq_last_status));
		return -EIO;
	}

	udp_tunnel_nic_set_port_priv(netdev, table, idx, filter_index);
	return 0;
}

static int i40e_udp_tunnel_unset_port(struct net_device *netdev,
				      unsigned int table, unsigned int idx,
				      struct udp_tunnel_info *ti)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_hw *hw = &np->vsi->back->hw;
	int ret;

	ret = i40e_aq_del_udp_tunnel(hw, ti->hw_priv, NULL);
	if (ret) {
		netdev_info(netdev, "delete UDP port failed, err %pe aq_err %s\n",
			    ERR_PTR(ret),
			    i40e_aq_str(hw, hw->aq.asq_last_status));
		return -EIO;
	}

	return 0;
}

static int i40e_get_phys_port_id(struct net_device *netdev,
				 struct netdev_phys_item_id *ppid)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_pf *pf = np->vsi->back;
	struct i40e_hw *hw = &pf->hw;

	if (!(pf->hw_features & I40E_HW_PORT_ID_VALID))
		return -EOPNOTSUPP;

	ppid->id_len = min_t(int, sizeof(hw->mac.port_addr), sizeof(ppid->id));
	memcpy(ppid->id, hw->mac.port_addr, ppid->id_len);

	return 0;
}

 
static int i40e_ndo_fdb_add(struct ndmsg *ndm, struct nlattr *tb[],
			    struct net_device *dev,
			    const unsigned char *addr, u16 vid,
			    u16 flags,
			    struct netlink_ext_ack *extack)
{
	struct i40e_netdev_priv *np = netdev_priv(dev);
	struct i40e_pf *pf = np->vsi->back;
	int err = 0;

	if (!(pf->flags & I40E_FLAG_SRIOV_ENABLED))
		return -EOPNOTSUPP;

	if (vid) {
		pr_info("%s: vlans aren't supported yet for dev_uc|mc_add()\n", dev->name);
		return -EINVAL;
	}

	 
	if (ndm->ndm_state && !(ndm->ndm_state & NUD_PERMANENT)) {
		netdev_info(dev, "FDB only supports static addresses\n");
		return -EINVAL;
	}

	if (is_unicast_ether_addr(addr) || is_link_local_ether_addr(addr))
		err = dev_uc_add_excl(dev, addr);
	else if (is_multicast_ether_addr(addr))
		err = dev_mc_add_excl(dev, addr);
	else
		err = -EINVAL;

	 
	if (err == -EEXIST && !(flags & NLM_F_EXCL))
		err = 0;

	return err;
}

 
static int i40e_ndo_bridge_setlink(struct net_device *dev,
				   struct nlmsghdr *nlh,
				   u16 flags,
				   struct netlink_ext_ack *extack)
{
	struct i40e_netdev_priv *np = netdev_priv(dev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_veb *veb = NULL;
	struct nlattr *attr, *br_spec;
	int i, rem;

	 
	if (vsi->seid != pf->vsi[pf->lan_vsi]->seid)
		return -EOPNOTSUPP;

	 
	for (i = 0; i < I40E_MAX_VEB && !veb; i++) {
		if (pf->veb[i] && pf->veb[i]->seid == vsi->uplink_seid)
			veb = pf->veb[i];
	}

	br_spec = nlmsg_find_attr(nlh, sizeof(struct ifinfomsg), IFLA_AF_SPEC);
	if (!br_spec)
		return -EINVAL;

	nla_for_each_nested(attr, br_spec, rem) {
		__u16 mode;

		if (nla_type(attr) != IFLA_BRIDGE_MODE)
			continue;

		mode = nla_get_u16(attr);
		if ((mode != BRIDGE_MODE_VEPA) &&
		    (mode != BRIDGE_MODE_VEB))
			return -EINVAL;

		 
		if (!veb) {
			veb = i40e_veb_setup(pf, 0, vsi->uplink_seid, vsi->seid,
					     vsi->tc_config.enabled_tc);
			if (veb) {
				veb->bridge_mode = mode;
				i40e_config_bridge_mode(veb);
			} else {
				 
				return -ENOENT;
			}
			break;
		} else if (mode != veb->bridge_mode) {
			 
			veb->bridge_mode = mode;
			 
			if (mode == BRIDGE_MODE_VEB)
				pf->flags |= I40E_FLAG_VEB_MODE_ENABLED;
			else
				pf->flags &= ~I40E_FLAG_VEB_MODE_ENABLED;
			i40e_do_reset(pf, I40E_PF_RESET_FLAG, true);
			break;
		}
	}

	return 0;
}

 
static int i40e_ndo_bridge_getlink(struct sk_buff *skb, u32 pid, u32 seq,
				   struct net_device *dev,
				   u32 __always_unused filter_mask,
				   int nlflags)
{
	struct i40e_netdev_priv *np = netdev_priv(dev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_veb *veb = NULL;
	int i;

	 
	if (vsi->seid != pf->vsi[pf->lan_vsi]->seid)
		return -EOPNOTSUPP;

	 
	for (i = 0; i < I40E_MAX_VEB && !veb; i++) {
		if (pf->veb[i] && pf->veb[i]->seid == vsi->uplink_seid)
			veb = pf->veb[i];
	}

	if (!veb)
		return 0;

	return ndo_dflt_bridge_getlink(skb, pid, seq, dev, veb->bridge_mode,
				       0, 0, nlflags, filter_mask, NULL);
}

 
static netdev_features_t i40e_features_check(struct sk_buff *skb,
					     struct net_device *dev,
					     netdev_features_t features)
{
	size_t len;

	 
	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return features;

	 
	if (skb_is_gso(skb) && (skb_shinfo(skb)->gso_size < 64))
		features &= ~NETIF_F_GSO_MASK;

	 
	len = skb_network_header(skb) - skb->data;
	if (len & ~(63 * 2))
		goto out_err;

	 
	len = skb_transport_header(skb) - skb_network_header(skb);
	if (len & ~(127 * 4))
		goto out_err;

	if (skb->encapsulation) {
		 
		len = skb_inner_network_header(skb) - skb_transport_header(skb);
		if (len & ~(127 * 2))
			goto out_err;

		 
		len = skb_inner_transport_header(skb) -
		      skb_inner_network_header(skb);
		if (len & ~(127 * 4))
			goto out_err;
	}

	 

	return features;
out_err:
	return features & ~(NETIF_F_CSUM_MASK | NETIF_F_GSO_MASK);
}

 
static int i40e_xdp_setup(struct i40e_vsi *vsi, struct bpf_prog *prog,
			  struct netlink_ext_ack *extack)
{
	int frame_size = i40e_max_vsi_frame_size(vsi, prog);
	struct i40e_pf *pf = vsi->back;
	struct bpf_prog *old_prog;
	bool need_reset;
	int i;

	 
	if (vsi->netdev->mtu > frame_size - I40E_PACKET_HDR_PAD) {
		NL_SET_ERR_MSG_MOD(extack, "MTU too large for linear frames and XDP prog does not support frags");
		return -EINVAL;
	}

	 
	need_reset = (i40e_enabled_xdp_vsi(vsi) != !!prog);

	if (need_reset)
		i40e_prep_for_reset(pf);

	 
	if (test_bit(__I40E_IN_REMOVE, pf->state))
		return -EINVAL;

	old_prog = xchg(&vsi->xdp_prog, prog);

	if (need_reset) {
		if (!prog) {
			xdp_features_clear_redirect_target(vsi->netdev);
			 
			synchronize_rcu();
		}
		i40e_reset_and_rebuild(pf, true, true);
	}

	if (!i40e_enabled_xdp_vsi(vsi) && prog) {
		if (i40e_realloc_rx_bi_zc(vsi, true))
			return -ENOMEM;
	} else if (i40e_enabled_xdp_vsi(vsi) && !prog) {
		if (i40e_realloc_rx_bi_zc(vsi, false))
			return -ENOMEM;
	}

	for (i = 0; i < vsi->num_queue_pairs; i++)
		WRITE_ONCE(vsi->rx_rings[i]->xdp_prog, vsi->xdp_prog);

	if (old_prog)
		bpf_prog_put(old_prog);

	 
	if (need_reset && prog) {
		for (i = 0; i < vsi->num_queue_pairs; i++)
			if (vsi->xdp_rings[i]->xsk_pool)
				(void)i40e_xsk_wakeup(vsi->netdev, i,
						      XDP_WAKEUP_RX);
		xdp_features_set_redirect_target(vsi->netdev, true);
	}

	return 0;
}

 
static int i40e_enter_busy_conf(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	int timeout = 50;

	while (test_and_set_bit(__I40E_CONFIG_BUSY, pf->state)) {
		timeout--;
		if (!timeout)
			return -EBUSY;
		usleep_range(1000, 2000);
	}

	return 0;
}

 
static void i40e_exit_busy_conf(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;

	clear_bit(__I40E_CONFIG_BUSY, pf->state);
}

 
static void i40e_queue_pair_reset_stats(struct i40e_vsi *vsi, int queue_pair)
{
	memset(&vsi->rx_rings[queue_pair]->rx_stats, 0,
	       sizeof(vsi->rx_rings[queue_pair]->rx_stats));
	memset(&vsi->tx_rings[queue_pair]->stats, 0,
	       sizeof(vsi->tx_rings[queue_pair]->stats));
	if (i40e_enabled_xdp_vsi(vsi)) {
		memset(&vsi->xdp_rings[queue_pair]->stats, 0,
		       sizeof(vsi->xdp_rings[queue_pair]->stats));
	}
}

 
static void i40e_queue_pair_clean_rings(struct i40e_vsi *vsi, int queue_pair)
{
	i40e_clean_tx_ring(vsi->tx_rings[queue_pair]);
	if (i40e_enabled_xdp_vsi(vsi)) {
		 
		synchronize_rcu();
		i40e_clean_tx_ring(vsi->xdp_rings[queue_pair]);
	}
	i40e_clean_rx_ring(vsi->rx_rings[queue_pair]);
}

 
static void i40e_queue_pair_toggle_napi(struct i40e_vsi *vsi, int queue_pair,
					bool enable)
{
	struct i40e_ring *rxr = vsi->rx_rings[queue_pair];
	struct i40e_q_vector *q_vector = rxr->q_vector;

	if (!vsi->netdev)
		return;

	 
	if (q_vector->rx.ring || q_vector->tx.ring) {
		if (enable)
			napi_enable(&q_vector->napi);
		else
			napi_disable(&q_vector->napi);
	}
}

 
static int i40e_queue_pair_toggle_rings(struct i40e_vsi *vsi, int queue_pair,
					bool enable)
{
	struct i40e_pf *pf = vsi->back;
	int pf_q, ret = 0;

	pf_q = vsi->base_queue + queue_pair;
	ret = i40e_control_wait_tx_q(vsi->seid, pf, pf_q,
				     false  , enable);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "VSI seid %d Tx ring %d %sable timeout\n",
			 vsi->seid, pf_q, (enable ? "en" : "dis"));
		return ret;
	}

	i40e_control_rx_q(pf, pf_q, enable);
	ret = i40e_pf_rxq_wait(pf, pf_q, enable);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "VSI seid %d Rx ring %d %sable timeout\n",
			 vsi->seid, pf_q, (enable ? "en" : "dis"));
		return ret;
	}

	 
	if (!enable)
		mdelay(50);

	if (!i40e_enabled_xdp_vsi(vsi))
		return ret;

	ret = i40e_control_wait_tx_q(vsi->seid, pf,
				     pf_q + vsi->alloc_queue_pairs,
				     true  , enable);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "VSI seid %d XDP Tx ring %d %sable timeout\n",
			 vsi->seid, pf_q, (enable ? "en" : "dis"));
	}

	return ret;
}

 
static void i40e_queue_pair_enable_irq(struct i40e_vsi *vsi, int queue_pair)
{
	struct i40e_ring *rxr = vsi->rx_rings[queue_pair];
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;

	 
	if (pf->flags & I40E_FLAG_MSIX_ENABLED)
		i40e_irq_dynamic_enable(vsi, rxr->q_vector->v_idx);
	else
		i40e_irq_dynamic_enable_icr0(pf);

	i40e_flush(hw);
}

 
static void i40e_queue_pair_disable_irq(struct i40e_vsi *vsi, int queue_pair)
{
	struct i40e_ring *rxr = vsi->rx_rings[queue_pair];
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;

	 
	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		u32 intpf = vsi->base_vector + rxr->q_vector->v_idx;

		wr32(hw, I40E_PFINT_DYN_CTLN(intpf - 1), 0);
		i40e_flush(hw);
		synchronize_irq(pf->msix_entries[intpf].vector);
	} else {
		 
		wr32(hw, I40E_PFINT_ICR0_ENA, 0);
		wr32(hw, I40E_PFINT_DYN_CTL0, 0);
		i40e_flush(hw);
		synchronize_irq(pf->pdev->irq);
	}
}

 
int i40e_queue_pair_disable(struct i40e_vsi *vsi, int queue_pair)
{
	int err;

	err = i40e_enter_busy_conf(vsi);
	if (err)
		return err;

	i40e_queue_pair_disable_irq(vsi, queue_pair);
	err = i40e_queue_pair_toggle_rings(vsi, queue_pair, false  );
	i40e_clean_rx_ring(vsi->rx_rings[queue_pair]);
	i40e_queue_pair_toggle_napi(vsi, queue_pair, false  );
	i40e_queue_pair_clean_rings(vsi, queue_pair);
	i40e_queue_pair_reset_stats(vsi, queue_pair);

	return err;
}

 
int i40e_queue_pair_enable(struct i40e_vsi *vsi, int queue_pair)
{
	int err;

	err = i40e_configure_tx_ring(vsi->tx_rings[queue_pair]);
	if (err)
		return err;

	if (i40e_enabled_xdp_vsi(vsi)) {
		err = i40e_configure_tx_ring(vsi->xdp_rings[queue_pair]);
		if (err)
			return err;
	}

	err = i40e_configure_rx_ring(vsi->rx_rings[queue_pair]);
	if (err)
		return err;

	err = i40e_queue_pair_toggle_rings(vsi, queue_pair, true  );
	i40e_queue_pair_toggle_napi(vsi, queue_pair, true  );
	i40e_queue_pair_enable_irq(vsi, queue_pair);

	i40e_exit_busy_conf(vsi);

	return err;
}

 
static int i40e_xdp(struct net_device *dev,
		    struct netdev_bpf *xdp)
{
	struct i40e_netdev_priv *np = netdev_priv(dev);
	struct i40e_vsi *vsi = np->vsi;

	if (vsi->type != I40E_VSI_MAIN)
		return -EINVAL;

	switch (xdp->command) {
	case XDP_SETUP_PROG:
		return i40e_xdp_setup(vsi, xdp->prog, xdp->extack);
	case XDP_SETUP_XSK_POOL:
		return i40e_xsk_pool_setup(vsi, xdp->xsk.pool,
					   xdp->xsk.queue_id);
	default:
		return -EINVAL;
	}
}

static const struct net_device_ops i40e_netdev_ops = {
	.ndo_open		= i40e_open,
	.ndo_stop		= i40e_close,
	.ndo_start_xmit		= i40e_lan_xmit_frame,
	.ndo_get_stats64	= i40e_get_netdev_stats_struct,
	.ndo_set_rx_mode	= i40e_set_rx_mode,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= i40e_set_mac,
	.ndo_change_mtu		= i40e_change_mtu,
	.ndo_eth_ioctl		= i40e_ioctl,
	.ndo_tx_timeout		= i40e_tx_timeout,
	.ndo_vlan_rx_add_vid	= i40e_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= i40e_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= i40e_netpoll,
#endif
	.ndo_setup_tc		= __i40e_setup_tc,
	.ndo_select_queue	= i40e_lan_select_queue,
	.ndo_set_features	= i40e_set_features,
	.ndo_set_vf_mac		= i40e_ndo_set_vf_mac,
	.ndo_set_vf_vlan	= i40e_ndo_set_vf_port_vlan,
	.ndo_get_vf_stats	= i40e_get_vf_stats,
	.ndo_set_vf_rate	= i40e_ndo_set_vf_bw,
	.ndo_get_vf_config	= i40e_ndo_get_vf_config,
	.ndo_set_vf_link_state	= i40e_ndo_set_vf_link_state,
	.ndo_set_vf_spoofchk	= i40e_ndo_set_vf_spoofchk,
	.ndo_set_vf_trust	= i40e_ndo_set_vf_trust,
	.ndo_get_phys_port_id	= i40e_get_phys_port_id,
	.ndo_fdb_add		= i40e_ndo_fdb_add,
	.ndo_features_check	= i40e_features_check,
	.ndo_bridge_getlink	= i40e_ndo_bridge_getlink,
	.ndo_bridge_setlink	= i40e_ndo_bridge_setlink,
	.ndo_bpf		= i40e_xdp,
	.ndo_xdp_xmit		= i40e_xdp_xmit,
	.ndo_xsk_wakeup	        = i40e_xsk_wakeup,
	.ndo_dfwd_add_station	= i40e_fwd_add,
	.ndo_dfwd_del_station	= i40e_fwd_del,
};

 
static int i40e_config_netdev(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_netdev_priv *np;
	struct net_device *netdev;
	u8 broadcast[ETH_ALEN];
	u8 mac_addr[ETH_ALEN];
	int etherdev_size;
	netdev_features_t hw_enc_features;
	netdev_features_t hw_features;

	etherdev_size = sizeof(struct i40e_netdev_priv);
	netdev = alloc_etherdev_mq(etherdev_size, vsi->alloc_queue_pairs);
	if (!netdev)
		return -ENOMEM;

	vsi->netdev = netdev;
	np = netdev_priv(netdev);
	np->vsi = vsi;

	hw_enc_features = NETIF_F_SG			|
			  NETIF_F_HW_CSUM		|
			  NETIF_F_HIGHDMA		|
			  NETIF_F_SOFT_FEATURES		|
			  NETIF_F_TSO			|
			  NETIF_F_TSO_ECN		|
			  NETIF_F_TSO6			|
			  NETIF_F_GSO_GRE		|
			  NETIF_F_GSO_GRE_CSUM		|
			  NETIF_F_GSO_PARTIAL		|
			  NETIF_F_GSO_IPXIP4		|
			  NETIF_F_GSO_IPXIP6		|
			  NETIF_F_GSO_UDP_TUNNEL	|
			  NETIF_F_GSO_UDP_TUNNEL_CSUM	|
			  NETIF_F_GSO_UDP_L4		|
			  NETIF_F_SCTP_CRC		|
			  NETIF_F_RXHASH		|
			  NETIF_F_RXCSUM		|
			  0;

	if (!(pf->hw_features & I40E_HW_OUTER_UDP_CSUM_CAPABLE))
		netdev->gso_partial_features |= NETIF_F_GSO_UDP_TUNNEL_CSUM;

	netdev->udp_tunnel_nic_info = &pf->udp_tunnel_nic;

	netdev->gso_partial_features |= NETIF_F_GSO_GRE_CSUM;

	netdev->hw_enc_features |= hw_enc_features;

	 
	netdev->vlan_features |= hw_enc_features | NETIF_F_TSO_MANGLEID;

#define I40E_GSO_PARTIAL_FEATURES (NETIF_F_GSO_GRE |		\
				   NETIF_F_GSO_GRE_CSUM |	\
				   NETIF_F_GSO_IPXIP4 |		\
				   NETIF_F_GSO_IPXIP6 |		\
				   NETIF_F_GSO_UDP_TUNNEL |	\
				   NETIF_F_GSO_UDP_TUNNEL_CSUM)

	netdev->gso_partial_features = I40E_GSO_PARTIAL_FEATURES;
	netdev->features |= NETIF_F_GSO_PARTIAL |
			    I40E_GSO_PARTIAL_FEATURES;

	netdev->mpls_features |= NETIF_F_SG;
	netdev->mpls_features |= NETIF_F_HW_CSUM;
	netdev->mpls_features |= NETIF_F_TSO;
	netdev->mpls_features |= NETIF_F_TSO6;
	netdev->mpls_features |= I40E_GSO_PARTIAL_FEATURES;

	 
	netdev->hw_features |= NETIF_F_HW_L2FW_DOFFLOAD;

	hw_features = hw_enc_features		|
		      NETIF_F_HW_VLAN_CTAG_TX	|
		      NETIF_F_HW_VLAN_CTAG_RX;

	if (!(pf->flags & I40E_FLAG_MFP_ENABLED))
		hw_features |= NETIF_F_NTUPLE | NETIF_F_HW_TC;

	netdev->hw_features |= hw_features | NETIF_F_LOOPBACK;

	netdev->features |= hw_features | NETIF_F_HW_VLAN_CTAG_FILTER;
	netdev->hw_enc_features |= NETIF_F_TSO_MANGLEID;

	netdev->features &= ~NETIF_F_HW_TC;

	if (vsi->type == I40E_VSI_MAIN) {
		SET_NETDEV_DEV(netdev, &pf->pdev->dev);
		ether_addr_copy(mac_addr, hw->mac.perm_addr);
		 
		i40e_rm_default_mac_filter(vsi, mac_addr);
		spin_lock_bh(&vsi->mac_filter_hash_lock);
		i40e_add_mac_filter(vsi, mac_addr);
		spin_unlock_bh(&vsi->mac_filter_hash_lock);

		netdev->xdp_features = NETDEV_XDP_ACT_BASIC |
				       NETDEV_XDP_ACT_REDIRECT |
				       NETDEV_XDP_ACT_XSK_ZEROCOPY |
				       NETDEV_XDP_ACT_RX_SG;
		netdev->xdp_zc_max_segs = I40E_MAX_BUFFER_TXD;
	} else {
		 
		snprintf(netdev->name, IFNAMSIZ, "%.*sv%%d",
			 IFNAMSIZ - 4,
			 pf->vsi[pf->lan_vsi]->netdev->name);
		eth_random_addr(mac_addr);

		spin_lock_bh(&vsi->mac_filter_hash_lock);
		i40e_add_mac_filter(vsi, mac_addr);
		spin_unlock_bh(&vsi->mac_filter_hash_lock);
	}

	 
	eth_broadcast_addr(broadcast);
	spin_lock_bh(&vsi->mac_filter_hash_lock);
	i40e_add_mac_filter(vsi, broadcast);
	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	eth_hw_addr_set(netdev, mac_addr);
	ether_addr_copy(netdev->perm_addr, mac_addr);

	 
	netdev->neigh_priv_len = sizeof(u32) * 4;

	netdev->priv_flags |= IFF_UNICAST_FLT;
	netdev->priv_flags |= IFF_SUPP_NOFCS;
	 
	i40e_vsi_config_netdev_tc(vsi, vsi->tc_config.enabled_tc);

	netdev->netdev_ops = &i40e_netdev_ops;
	netdev->watchdog_timeo = 5 * HZ;
	i40e_set_ethtool_ops(netdev);

	 
	netdev->min_mtu = ETH_MIN_MTU;
	netdev->max_mtu = I40E_MAX_RXBUFFER - I40E_PACKET_HDR_PAD;

	return 0;
}

 
static void i40e_vsi_delete(struct i40e_vsi *vsi)
{
	 
	if (vsi == vsi->back->vsi[vsi->back->lan_vsi])
		return;

	i40e_aq_delete_element(&vsi->back->hw, vsi->seid, NULL);
}

 
int i40e_is_vsi_uplink_mode_veb(struct i40e_vsi *vsi)
{
	struct i40e_veb *veb;
	struct i40e_pf *pf = vsi->back;

	 
	if (vsi->veb_idx >= I40E_MAX_VEB)
		return 1;

	veb = pf->veb[vsi->veb_idx];
	if (!veb) {
		dev_info(&pf->pdev->dev,
			 "There is no veb associated with the bridge\n");
		return -ENOENT;
	}

	 
	if (veb->bridge_mode & BRIDGE_MODE_VEPA) {
		return 0;
	} else {
		 
		return 1;
	}

	 
	return 0;
}

 
static int i40e_add_vsi(struct i40e_vsi *vsi)
{
	int ret = -ENODEV;
	struct i40e_pf *pf = vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vsi_context ctxt;
	struct i40e_mac_filter *f;
	struct hlist_node *h;
	int bkt;

	u8 enabled_tc = 0x1;  
	int f_count = 0;

	memset(&ctxt, 0, sizeof(ctxt));
	switch (vsi->type) {
	case I40E_VSI_MAIN:
		 
		ctxt.seid = pf->main_vsi_seid;
		ctxt.pf_num = pf->hw.pf_id;
		ctxt.vf_num = 0;
		ret = i40e_aq_get_vsi_params(&pf->hw, &ctxt, NULL);
		ctxt.flags = I40E_AQ_VSI_TYPE_PF;
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "couldn't get PF vsi config, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
			return -ENOENT;
		}
		vsi->info = ctxt.info;
		vsi->info.valid_sections = 0;

		vsi->seid = ctxt.seid;
		vsi->id = ctxt.vsi_number;

		enabled_tc = i40e_pf_get_tc_map(pf);

		 
		if (pf->flags & I40E_FLAG_SOURCE_PRUNING_DISABLED) {
			memset(&ctxt, 0, sizeof(ctxt));
			ctxt.seid = pf->main_vsi_seid;
			ctxt.pf_num = pf->hw.pf_id;
			ctxt.vf_num = 0;
			ctxt.info.valid_sections |=
				     cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
			ctxt.info.switch_id =
				   cpu_to_le16(I40E_AQ_VSI_SW_ID_FLAG_LOCAL_LB);
			ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
			if (ret) {
				dev_info(&pf->pdev->dev,
					 "update vsi failed, err %d aq_err %s\n",
					 ret,
					 i40e_aq_str(&pf->hw,
						     pf->hw.aq.asq_last_status));
				ret = -ENOENT;
				goto err;
			}
		}

		 
		if ((pf->flags & I40E_FLAG_MFP_ENABLED) &&
		    !(pf->hw.func_caps.iscsi)) {  
			memset(&ctxt, 0, sizeof(ctxt));
			ctxt.seid = pf->main_vsi_seid;
			ctxt.pf_num = pf->hw.pf_id;
			ctxt.vf_num = 0;
			i40e_vsi_setup_queue_map(vsi, &ctxt, enabled_tc, false);
			ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
			if (ret) {
				dev_info(&pf->pdev->dev,
					 "update vsi failed, err %pe aq_err %s\n",
					 ERR_PTR(ret),
					 i40e_aq_str(&pf->hw,
						    pf->hw.aq.asq_last_status));
				ret = -ENOENT;
				goto err;
			}
			 
			i40e_vsi_update_queue_map(vsi, &ctxt);
			vsi->info.valid_sections = 0;
		} else {
			 
			ret = i40e_vsi_config_tc(vsi, enabled_tc);
			if (ret) {
				 
				dev_info(&pf->pdev->dev,
					 "failed to configure TCs for main VSI tc_map 0x%08x, err %pe aq_err %s\n",
					 enabled_tc,
					 ERR_PTR(ret),
					 i40e_aq_str(&pf->hw,
						    pf->hw.aq.asq_last_status));
			}
		}
		break;

	case I40E_VSI_FDIR:
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = 0;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = I40E_AQ_VSI_CONN_TYPE_NORMAL;
		ctxt.flags = I40E_AQ_VSI_TYPE_PF;
		if ((pf->flags & I40E_FLAG_VEB_MODE_ENABLED) &&
		    (i40e_is_vsi_uplink_mode_veb(vsi))) {
			ctxt.info.valid_sections |=
			     cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
			ctxt.info.switch_id =
			   cpu_to_le16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
		}
		i40e_vsi_setup_queue_map(vsi, &ctxt, enabled_tc, true);
		break;

	case I40E_VSI_VMDQ2:
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = 0;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = I40E_AQ_VSI_CONN_TYPE_NORMAL;
		ctxt.flags = I40E_AQ_VSI_TYPE_VMDQ2;

		 
		if (i40e_is_vsi_uplink_mode_veb(vsi)) {
			ctxt.info.valid_sections |=
				cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
			ctxt.info.switch_id =
				cpu_to_le16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
		}

		 
		i40e_vsi_setup_queue_map(vsi, &ctxt, enabled_tc, true);
		break;

	case I40E_VSI_SRIOV:
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = vsi->vf_id + hw->func_caps.vf_base_id;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = I40E_AQ_VSI_CONN_TYPE_NORMAL;
		ctxt.flags = I40E_AQ_VSI_TYPE_VF;

		 
		if (i40e_is_vsi_uplink_mode_veb(vsi)) {
			ctxt.info.valid_sections |=
				cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
			ctxt.info.switch_id =
				cpu_to_le16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
		}

		if (vsi->back->flags & I40E_FLAG_IWARP_ENABLED) {
			ctxt.info.valid_sections |=
				cpu_to_le16(I40E_AQ_VSI_PROP_QUEUE_OPT_VALID);
			ctxt.info.queueing_opt_flags |=
				(I40E_AQ_VSI_QUE_OPT_TCP_ENA |
				 I40E_AQ_VSI_QUE_OPT_RSS_LUT_VSI);
		}

		ctxt.info.valid_sections |= cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID);
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_ALL;
		if (pf->vf[vsi->vf_id].spoofchk) {
			ctxt.info.valid_sections |=
				cpu_to_le16(I40E_AQ_VSI_PROP_SECURITY_VALID);
			ctxt.info.sec_flags |=
				(I40E_AQ_VSI_SEC_FLAG_ENABLE_VLAN_CHK |
				 I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK);
		}
		 
		i40e_vsi_setup_queue_map(vsi, &ctxt, enabled_tc, true);
		break;

	case I40E_VSI_IWARP:
		 
		break;

	default:
		return -ENODEV;
	}

	if (vsi->type != I40E_VSI_MAIN) {
		ret = i40e_aq_add_vsi(hw, &ctxt, NULL);
		if (ret) {
			dev_info(&vsi->back->pdev->dev,
				 "add vsi failed, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
			ret = -ENOENT;
			goto err;
		}
		vsi->info = ctxt.info;
		vsi->info.valid_sections = 0;
		vsi->seid = ctxt.seid;
		vsi->id = ctxt.vsi_number;
	}

	spin_lock_bh(&vsi->mac_filter_hash_lock);
	vsi->active_filters = 0;
	 
	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
		f->state = I40E_FILTER_NEW;
		f_count++;
	}
	spin_unlock_bh(&vsi->mac_filter_hash_lock);
	clear_bit(__I40E_VSI_OVERFLOW_PROMISC, vsi->state);

	if (f_count) {
		vsi->flags |= I40E_VSI_FLAG_FILTER_CHANGED;
		set_bit(__I40E_MACVLAN_SYNC_PENDING, pf->state);
	}

	 
	ret = i40e_vsi_get_bw_info(vsi);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't get vsi bw info, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		 
		ret = 0;
	}

err:
	return ret;
}

 
int i40e_vsi_release(struct i40e_vsi *vsi)
{
	struct i40e_mac_filter *f;
	struct hlist_node *h;
	struct i40e_veb *veb = NULL;
	struct i40e_pf *pf;
	u16 uplink_seid;
	int i, n, bkt;

	pf = vsi->back;

	 
	if (vsi->flags & I40E_VSI_FLAG_VEB_OWNER) {
		dev_info(&pf->pdev->dev, "VSI %d has existing VEB %d\n",
			 vsi->seid, vsi->uplink_seid);
		return -ENODEV;
	}
	if (vsi == pf->vsi[pf->lan_vsi] &&
	    !test_bit(__I40E_DOWN, pf->state)) {
		dev_info(&pf->pdev->dev, "Can't remove PF VSI\n");
		return -ENODEV;
	}
	set_bit(__I40E_VSI_RELEASING, vsi->state);
	uplink_seid = vsi->uplink_seid;
	if (vsi->type != I40E_VSI_SRIOV) {
		if (vsi->netdev_registered) {
			vsi->netdev_registered = false;
			if (vsi->netdev) {
				 
				unregister_netdev(vsi->netdev);
			}
		} else {
			i40e_vsi_close(vsi);
		}
		i40e_vsi_disable_irq(vsi);
	}

	spin_lock_bh(&vsi->mac_filter_hash_lock);

	 
	if (vsi->netdev) {
		__dev_uc_unsync(vsi->netdev, NULL);
		__dev_mc_unsync(vsi->netdev, NULL);
	}

	 
	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist)
		__i40e_del_filter(vsi, f);

	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	i40e_sync_vsi_filters(vsi);

	i40e_vsi_delete(vsi);
	i40e_vsi_free_q_vectors(vsi);
	if (vsi->netdev) {
		free_netdev(vsi->netdev);
		vsi->netdev = NULL;
	}
	i40e_vsi_clear_rings(vsi);
	i40e_vsi_clear(vsi);

	 
	for (n = 0, i = 0; i < pf->num_alloc_vsi; i++) {
		if (pf->vsi[i] &&
		    pf->vsi[i]->uplink_seid == uplink_seid &&
		    (pf->vsi[i]->flags & I40E_VSI_FLAG_VEB_OWNER) == 0) {
			n++;       
		}
	}
	for (i = 0; i < I40E_MAX_VEB; i++) {
		if (!pf->veb[i])
			continue;
		if (pf->veb[i]->uplink_seid == uplink_seid)
			n++;      
		if (pf->veb[i]->seid == uplink_seid)
			veb = pf->veb[i];
	}
	if (n == 0 && veb && veb->uplink_seid != 0)
		i40e_veb_release(veb);

	return 0;
}

 
static int i40e_vsi_setup_vectors(struct i40e_vsi *vsi)
{
	int ret = -ENOENT;
	struct i40e_pf *pf = vsi->back;

	if (vsi->q_vectors[0]) {
		dev_info(&pf->pdev->dev, "VSI %d has existing q_vectors\n",
			 vsi->seid);
		return -EEXIST;
	}

	if (vsi->base_vector) {
		dev_info(&pf->pdev->dev, "VSI %d has non-zero base vector %d\n",
			 vsi->seid, vsi->base_vector);
		return -EEXIST;
	}

	ret = i40e_vsi_alloc_q_vectors(vsi);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "failed to allocate %d q_vector for VSI %d, ret=%d\n",
			 vsi->num_q_vectors, vsi->seid, ret);
		vsi->num_q_vectors = 0;
		goto vector_setup_out;
	}

	 
	if (!(pf->flags & I40E_FLAG_MSIX_ENABLED))
		return ret;
	if (vsi->num_q_vectors)
		vsi->base_vector = i40e_get_lump(pf, pf->irq_pile,
						 vsi->num_q_vectors, vsi->idx);
	if (vsi->base_vector < 0) {
		dev_info(&pf->pdev->dev,
			 "failed to get tracking for %d vectors for VSI %d, err=%d\n",
			 vsi->num_q_vectors, vsi->seid, vsi->base_vector);
		i40e_vsi_free_q_vectors(vsi);
		ret = -ENOENT;
		goto vector_setup_out;
	}

vector_setup_out:
	return ret;
}

 
static struct i40e_vsi *i40e_vsi_reinit_setup(struct i40e_vsi *vsi)
{
	u16 alloc_queue_pairs;
	struct i40e_pf *pf;
	u8 enabled_tc;
	int ret;

	if (!vsi)
		return NULL;

	pf = vsi->back;

	i40e_put_lump(pf->qp_pile, vsi->base_queue, vsi->idx);
	i40e_vsi_clear_rings(vsi);

	i40e_vsi_free_arrays(vsi, false);
	i40e_set_num_rings_in_vsi(vsi);
	ret = i40e_vsi_alloc_arrays(vsi, false);
	if (ret)
		goto err_vsi;

	alloc_queue_pairs = vsi->alloc_queue_pairs *
			    (i40e_enabled_xdp_vsi(vsi) ? 2 : 1);

	ret = i40e_get_lump(pf, pf->qp_pile, alloc_queue_pairs, vsi->idx);
	if (ret < 0) {
		dev_info(&pf->pdev->dev,
			 "failed to get tracking for %d queues for VSI %d err %d\n",
			 alloc_queue_pairs, vsi->seid, ret);
		goto err_vsi;
	}
	vsi->base_queue = ret;

	 
	enabled_tc = pf->vsi[pf->lan_vsi]->tc_config.enabled_tc;
	pf->vsi[pf->lan_vsi]->tc_config.enabled_tc = 0;
	pf->vsi[pf->lan_vsi]->seid = pf->main_vsi_seid;
	i40e_vsi_config_tc(pf->vsi[pf->lan_vsi], enabled_tc);
	if (vsi->type == I40E_VSI_MAIN)
		i40e_rm_default_mac_filter(vsi, pf->hw.mac.perm_addr);

	 
	ret = i40e_alloc_rings(vsi);
	if (ret)
		goto err_rings;

	 
	i40e_vsi_map_rings_to_vectors(vsi);
	return vsi;

err_rings:
	i40e_vsi_free_q_vectors(vsi);
	if (vsi->netdev_registered) {
		vsi->netdev_registered = false;
		unregister_netdev(vsi->netdev);
		free_netdev(vsi->netdev);
		vsi->netdev = NULL;
	}
	i40e_aq_delete_element(&pf->hw, vsi->seid, NULL);
err_vsi:
	i40e_vsi_clear(vsi);
	return NULL;
}

 
struct i40e_vsi *i40e_vsi_setup(struct i40e_pf *pf, u8 type,
				u16 uplink_seid, u32 param1)
{
	struct i40e_vsi *vsi = NULL;
	struct i40e_veb *veb = NULL;
	u16 alloc_queue_pairs;
	int ret, i;
	int v_idx;

	 
	for (i = 0; i < I40E_MAX_VEB; i++) {
		if (pf->veb[i] && pf->veb[i]->seid == uplink_seid) {
			veb = pf->veb[i];
			break;
		}
	}

	if (!veb && uplink_seid != pf->mac_seid) {

		for (i = 0; i < pf->num_alloc_vsi; i++) {
			if (pf->vsi[i] && pf->vsi[i]->seid == uplink_seid) {
				vsi = pf->vsi[i];
				break;
			}
		}
		if (!vsi) {
			dev_info(&pf->pdev->dev, "no such uplink_seid %d\n",
				 uplink_seid);
			return NULL;
		}

		if (vsi->uplink_seid == pf->mac_seid)
			veb = i40e_veb_setup(pf, 0, pf->mac_seid, vsi->seid,
					     vsi->tc_config.enabled_tc);
		else if ((vsi->flags & I40E_VSI_FLAG_VEB_OWNER) == 0)
			veb = i40e_veb_setup(pf, 0, vsi->uplink_seid, vsi->seid,
					     vsi->tc_config.enabled_tc);
		if (veb) {
			if (vsi->seid != pf->vsi[pf->lan_vsi]->seid) {
				dev_info(&vsi->back->pdev->dev,
					 "New VSI creation error, uplink seid of LAN VSI expected.\n");
				return NULL;
			}
			 
			if (!(pf->flags & I40E_FLAG_VEB_MODE_ENABLED)) {
				veb->bridge_mode = BRIDGE_MODE_VEPA;
				pf->flags &= ~I40E_FLAG_VEB_MODE_ENABLED;
			}
			i40e_config_bridge_mode(veb);
		}
		for (i = 0; i < I40E_MAX_VEB && !veb; i++) {
			if (pf->veb[i] && pf->veb[i]->seid == vsi->uplink_seid)
				veb = pf->veb[i];
		}
		if (!veb) {
			dev_info(&pf->pdev->dev, "couldn't add VEB\n");
			return NULL;
		}

		vsi->flags |= I40E_VSI_FLAG_VEB_OWNER;
		uplink_seid = veb->seid;
	}

	 
	v_idx = i40e_vsi_mem_alloc(pf, type);
	if (v_idx < 0)
		goto err_alloc;
	vsi = pf->vsi[v_idx];
	if (!vsi)
		goto err_alloc;
	vsi->type = type;
	vsi->veb_idx = (veb ? veb->idx : I40E_NO_VEB);

	if (type == I40E_VSI_MAIN)
		pf->lan_vsi = v_idx;
	else if (type == I40E_VSI_SRIOV)
		vsi->vf_id = param1;
	 
	alloc_queue_pairs = vsi->alloc_queue_pairs *
			    (i40e_enabled_xdp_vsi(vsi) ? 2 : 1);

	ret = i40e_get_lump(pf, pf->qp_pile, alloc_queue_pairs, vsi->idx);
	if (ret < 0) {
		dev_info(&pf->pdev->dev,
			 "failed to get tracking for %d queues for VSI %d err=%d\n",
			 alloc_queue_pairs, vsi->seid, ret);
		goto err_vsi;
	}
	vsi->base_queue = ret;

	 
	vsi->uplink_seid = uplink_seid;
	ret = i40e_add_vsi(vsi);
	if (ret)
		goto err_vsi;

	switch (vsi->type) {
	 
	case I40E_VSI_MAIN:
	case I40E_VSI_VMDQ2:
		ret = i40e_config_netdev(vsi);
		if (ret)
			goto err_netdev;
		ret = i40e_netif_set_realnum_tx_rx_queues(vsi);
		if (ret)
			goto err_netdev;
		ret = register_netdev(vsi->netdev);
		if (ret)
			goto err_netdev;
		vsi->netdev_registered = true;
		netif_carrier_off(vsi->netdev);
#ifdef CONFIG_I40E_DCB
		 
		i40e_dcbnl_setup(vsi);
#endif  
		fallthrough;
	case I40E_VSI_FDIR:
		 
		ret = i40e_vsi_setup_vectors(vsi);
		if (ret)
			goto err_msix;

		ret = i40e_alloc_rings(vsi);
		if (ret)
			goto err_rings;

		 
		i40e_vsi_map_rings_to_vectors(vsi);

		i40e_vsi_reset_stats(vsi);
		break;
	default:
		 
		break;
	}

	if ((pf->hw_features & I40E_HW_RSS_AQ_CAPABLE) &&
	    (vsi->type == I40E_VSI_VMDQ2)) {
		ret = i40e_vsi_config_rss(vsi);
	}
	return vsi;

err_rings:
	i40e_vsi_free_q_vectors(vsi);
err_msix:
	if (vsi->netdev_registered) {
		vsi->netdev_registered = false;
		unregister_netdev(vsi->netdev);
		free_netdev(vsi->netdev);
		vsi->netdev = NULL;
	}
err_netdev:
	i40e_aq_delete_element(&pf->hw, vsi->seid, NULL);
err_vsi:
	i40e_vsi_clear(vsi);
err_alloc:
	return NULL;
}

 
static int i40e_veb_get_bw_info(struct i40e_veb *veb)
{
	struct i40e_aqc_query_switching_comp_ets_config_resp ets_data;
	struct i40e_aqc_query_switching_comp_bw_config_resp bw_data;
	struct i40e_pf *pf = veb->pf;
	struct i40e_hw *hw = &pf->hw;
	u32 tc_bw_max;
	int ret = 0;
	int i;

	ret = i40e_aq_query_switch_comp_bw_config(hw, veb->seid,
						  &bw_data, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "query veb bw config failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, hw->aq.asq_last_status));
		goto out;
	}

	ret = i40e_aq_query_switch_comp_ets_config(hw, veb->seid,
						   &ets_data, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "query veb bw ets config failed, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, hw->aq.asq_last_status));
		goto out;
	}

	veb->bw_limit = le16_to_cpu(ets_data.port_bw_limit);
	veb->bw_max_quanta = ets_data.tc_bw_max;
	veb->is_abs_credits = bw_data.absolute_credits_enable;
	veb->enabled_tc = ets_data.tc_valid_bits;
	tc_bw_max = le16_to_cpu(bw_data.tc_bw_max[0]) |
		    (le16_to_cpu(bw_data.tc_bw_max[1]) << 16);
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		veb->bw_tc_share_credits[i] = bw_data.tc_bw_share_credits[i];
		veb->bw_tc_limit_credits[i] =
					le16_to_cpu(bw_data.tc_bw_limits[i]);
		veb->bw_tc_max_quanta[i] = ((tc_bw_max >> (i*4)) & 0x7);
	}

out:
	return ret;
}

 
static int i40e_veb_mem_alloc(struct i40e_pf *pf)
{
	int ret = -ENOENT;
	struct i40e_veb *veb;
	int i;

	 
	mutex_lock(&pf->switch_mutex);

	 
	i = 0;
	while ((i < I40E_MAX_VEB) && (pf->veb[i] != NULL))
		i++;
	if (i >= I40E_MAX_VEB) {
		ret = -ENOMEM;
		goto err_alloc_veb;   
	}

	veb = kzalloc(sizeof(*veb), GFP_KERNEL);
	if (!veb) {
		ret = -ENOMEM;
		goto err_alloc_veb;
	}
	veb->pf = pf;
	veb->idx = i;
	veb->enabled_tc = 1;

	pf->veb[i] = veb;
	ret = i;
err_alloc_veb:
	mutex_unlock(&pf->switch_mutex);
	return ret;
}

 
static void i40e_switch_branch_release(struct i40e_veb *branch)
{
	struct i40e_pf *pf = branch->pf;
	u16 branch_seid = branch->seid;
	u16 veb_idx = branch->idx;
	int i;

	 
	for (i = 0; i < I40E_MAX_VEB; i++) {
		if (!pf->veb[i])
			continue;
		if (pf->veb[i]->uplink_seid == branch->seid)
			i40e_switch_branch_release(pf->veb[i]);
	}

	 
	for (i = 0; i < pf->num_alloc_vsi; i++) {
		if (!pf->vsi[i])
			continue;
		if (pf->vsi[i]->uplink_seid == branch_seid &&
		   (pf->vsi[i]->flags & I40E_VSI_FLAG_VEB_OWNER) == 0) {
			i40e_vsi_release(pf->vsi[i]);
		}
	}

	 
	if (pf->veb[veb_idx])
		i40e_veb_release(pf->veb[veb_idx]);
}

 
static void i40e_veb_clear(struct i40e_veb *veb)
{
	if (!veb)
		return;

	if (veb->pf) {
		struct i40e_pf *pf = veb->pf;

		mutex_lock(&pf->switch_mutex);
		if (pf->veb[veb->idx] == veb)
			pf->veb[veb->idx] = NULL;
		mutex_unlock(&pf->switch_mutex);
	}

	kfree(veb);
}

 
void i40e_veb_release(struct i40e_veb *veb)
{
	struct i40e_vsi *vsi = NULL;
	struct i40e_pf *pf;
	int i, n = 0;

	pf = veb->pf;

	 
	for (i = 0; i < pf->num_alloc_vsi; i++) {
		if (pf->vsi[i] && pf->vsi[i]->uplink_seid == veb->seid) {
			n++;
			vsi = pf->vsi[i];
		}
	}
	if (n != 1) {
		dev_info(&pf->pdev->dev,
			 "can't remove VEB %d with %d VSIs left\n",
			 veb->seid, n);
		return;
	}

	 
	vsi->flags &= ~I40E_VSI_FLAG_VEB_OWNER;
	if (veb->uplink_seid) {
		vsi->uplink_seid = veb->uplink_seid;
		if (veb->uplink_seid == pf->mac_seid)
			vsi->veb_idx = I40E_NO_VEB;
		else
			vsi->veb_idx = veb->veb_idx;
	} else {
		 
		vsi->uplink_seid = pf->vsi[pf->lan_vsi]->uplink_seid;
		vsi->veb_idx = pf->vsi[pf->lan_vsi]->veb_idx;
	}

	i40e_aq_delete_element(&pf->hw, veb->seid, NULL);
	i40e_veb_clear(veb);
}

 
static int i40e_add_veb(struct i40e_veb *veb, struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = veb->pf;
	bool enable_stats = !!(pf->flags & I40E_FLAG_VEB_STATS_ENABLED);
	int ret;

	ret = i40e_aq_add_veb(&pf->hw, veb->uplink_seid, vsi->seid,
			      veb->enabled_tc, false,
			      &veb->seid, enable_stats, NULL);

	 
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't add VEB, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		return -EPERM;
	}

	 
	ret = i40e_aq_get_veb_parameters(&pf->hw, veb->seid, NULL, NULL,
					 &veb->stats_idx, NULL, NULL, NULL);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't get VEB statistics idx, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		return -EPERM;
	}
	ret = i40e_veb_get_bw_info(veb);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't get VEB bw info, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		i40e_aq_delete_element(&pf->hw, veb->seid, NULL);
		return -ENOENT;
	}

	vsi->uplink_seid = veb->seid;
	vsi->veb_idx = veb->idx;
	vsi->flags |= I40E_VSI_FLAG_VEB_OWNER;

	return 0;
}

 
struct i40e_veb *i40e_veb_setup(struct i40e_pf *pf, u16 flags,
				u16 uplink_seid, u16 vsi_seid,
				u8 enabled_tc)
{
	struct i40e_veb *veb, *uplink_veb = NULL;
	int vsi_idx, veb_idx;
	int ret;

	 
	if ((uplink_seid == 0 || vsi_seid == 0) &&
	    (uplink_seid + vsi_seid != 0)) {
		dev_info(&pf->pdev->dev,
			 "one, not both seid's are 0: uplink=%d vsi=%d\n",
			 uplink_seid, vsi_seid);
		return NULL;
	}

	 
	for (vsi_idx = 0; vsi_idx < pf->num_alloc_vsi; vsi_idx++)
		if (pf->vsi[vsi_idx] && pf->vsi[vsi_idx]->seid == vsi_seid)
			break;
	if (vsi_idx == pf->num_alloc_vsi && vsi_seid != 0) {
		dev_info(&pf->pdev->dev, "vsi seid %d not found\n",
			 vsi_seid);
		return NULL;
	}

	if (uplink_seid && uplink_seid != pf->mac_seid) {
		for (veb_idx = 0; veb_idx < I40E_MAX_VEB; veb_idx++) {
			if (pf->veb[veb_idx] &&
			    pf->veb[veb_idx]->seid == uplink_seid) {
				uplink_veb = pf->veb[veb_idx];
				break;
			}
		}
		if (!uplink_veb) {
			dev_info(&pf->pdev->dev,
				 "uplink seid %d not found\n", uplink_seid);
			return NULL;
		}
	}

	 
	veb_idx = i40e_veb_mem_alloc(pf);
	if (veb_idx < 0)
		goto err_alloc;
	veb = pf->veb[veb_idx];
	veb->flags = flags;
	veb->uplink_seid = uplink_seid;
	veb->veb_idx = (uplink_veb ? uplink_veb->idx : I40E_NO_VEB);
	veb->enabled_tc = (enabled_tc ? enabled_tc : 0x1);

	 
	ret = i40e_add_veb(veb, pf->vsi[vsi_idx]);
	if (ret)
		goto err_veb;
	if (vsi_idx == pf->lan_vsi)
		pf->lan_veb = veb->idx;

	return veb;

err_veb:
	i40e_veb_clear(veb);
err_alloc:
	return NULL;
}

 
static void i40e_setup_pf_switch_element(struct i40e_pf *pf,
				struct i40e_aqc_switch_config_element_resp *ele,
				u16 num_reported, bool printconfig)
{
	u16 downlink_seid = le16_to_cpu(ele->downlink_seid);
	u16 uplink_seid = le16_to_cpu(ele->uplink_seid);
	u8 element_type = ele->element_type;
	u16 seid = le16_to_cpu(ele->seid);

	if (printconfig)
		dev_info(&pf->pdev->dev,
			 "type=%d seid=%d uplink=%d downlink=%d\n",
			 element_type, seid, uplink_seid, downlink_seid);

	switch (element_type) {
	case I40E_SWITCH_ELEMENT_TYPE_MAC:
		pf->mac_seid = seid;
		break;
	case I40E_SWITCH_ELEMENT_TYPE_VEB:
		 
		if (uplink_seid != pf->mac_seid)
			break;
		if (pf->lan_veb >= I40E_MAX_VEB) {
			int v;

			 
			for (v = 0; v < I40E_MAX_VEB; v++) {
				if (pf->veb[v] && (pf->veb[v]->seid == seid)) {
					pf->lan_veb = v;
					break;
				}
			}
			if (pf->lan_veb >= I40E_MAX_VEB) {
				v = i40e_veb_mem_alloc(pf);
				if (v < 0)
					break;
				pf->lan_veb = v;
			}
		}
		if (pf->lan_veb >= I40E_MAX_VEB)
			break;

		pf->veb[pf->lan_veb]->seid = seid;
		pf->veb[pf->lan_veb]->uplink_seid = pf->mac_seid;
		pf->veb[pf->lan_veb]->pf = pf;
		pf->veb[pf->lan_veb]->veb_idx = I40E_NO_VEB;
		break;
	case I40E_SWITCH_ELEMENT_TYPE_VSI:
		if (num_reported != 1)
			break;
		 
		pf->mac_seid = uplink_seid;
		pf->pf_seid = downlink_seid;
		pf->main_vsi_seid = seid;
		if (printconfig)
			dev_info(&pf->pdev->dev,
				 "pf_seid=%d main_vsi_seid=%d\n",
				 pf->pf_seid, pf->main_vsi_seid);
		break;
	case I40E_SWITCH_ELEMENT_TYPE_PF:
	case I40E_SWITCH_ELEMENT_TYPE_VF:
	case I40E_SWITCH_ELEMENT_TYPE_EMP:
	case I40E_SWITCH_ELEMENT_TYPE_BMC:
	case I40E_SWITCH_ELEMENT_TYPE_PE:
	case I40E_SWITCH_ELEMENT_TYPE_PA:
		 
		break;
	default:
		dev_info(&pf->pdev->dev, "unknown element type=%d seid=%d\n",
			 element_type, seid);
		break;
	}
}

 
int i40e_fetch_switch_configuration(struct i40e_pf *pf, bool printconfig)
{
	struct i40e_aqc_get_switch_config_resp *sw_config;
	u16 next_seid = 0;
	int ret = 0;
	u8 *aq_buf;
	int i;

	aq_buf = kzalloc(I40E_AQ_LARGE_BUF, GFP_KERNEL);
	if (!aq_buf)
		return -ENOMEM;

	sw_config = (struct i40e_aqc_get_switch_config_resp *)aq_buf;
	do {
		u16 num_reported, num_total;

		ret = i40e_aq_get_switch_config(&pf->hw, sw_config,
						I40E_AQ_LARGE_BUF,
						&next_seid, NULL);
		if (ret) {
			dev_info(&pf->pdev->dev,
				 "get switch config failed err %d aq_err %s\n",
				 ret,
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
			kfree(aq_buf);
			return -ENOENT;
		}

		num_reported = le16_to_cpu(sw_config->header.num_reported);
		num_total = le16_to_cpu(sw_config->header.num_total);

		if (printconfig)
			dev_info(&pf->pdev->dev,
				 "header: %d reported %d total\n",
				 num_reported, num_total);

		for (i = 0; i < num_reported; i++) {
			struct i40e_aqc_switch_config_element_resp *ele =
				&sw_config->element[i];

			i40e_setup_pf_switch_element(pf, ele, num_reported,
						     printconfig);
		}
	} while (next_seid != 0);

	kfree(aq_buf);
	return ret;
}

 
static int i40e_setup_pf_switch(struct i40e_pf *pf, bool reinit, bool lock_acquired)
{
	u16 flags = 0;
	int ret;

	 
	ret = i40e_fetch_switch_configuration(pf, false);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "couldn't fetch switch config, err %pe aq_err %s\n",
			 ERR_PTR(ret),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		return ret;
	}
	i40e_pf_reset_stats(pf);

	 

	if ((pf->hw.pf_id == 0) &&
	    !(pf->flags & I40E_FLAG_TRUE_PROMISC_SUPPORT)) {
		flags = I40E_AQ_SET_SWITCH_CFG_PROMISC;
		pf->last_sw_conf_flags = flags;
	}

	if (pf->hw.pf_id == 0) {
		u16 valid_flags;

		valid_flags = I40E_AQ_SET_SWITCH_CFG_PROMISC;
		ret = i40e_aq_set_switch_config(&pf->hw, flags, valid_flags, 0,
						NULL);
		if (ret && pf->hw.aq.asq_last_status != I40E_AQ_RC_ESRCH) {
			dev_info(&pf->pdev->dev,
				 "couldn't set switch config bits, err %pe aq_err %s\n",
				 ERR_PTR(ret),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
			 
		}
		pf->last_sw_conf_valid_flags = valid_flags;
	}

	 
	if (pf->lan_vsi == I40E_NO_VSI || reinit) {
		struct i40e_vsi *vsi = NULL;
		u16 uplink_seid;

		 
		if (pf->lan_veb < I40E_MAX_VEB && pf->veb[pf->lan_veb])
			uplink_seid = pf->veb[pf->lan_veb]->seid;
		else
			uplink_seid = pf->mac_seid;
		if (pf->lan_vsi == I40E_NO_VSI)
			vsi = i40e_vsi_setup(pf, I40E_VSI_MAIN, uplink_seid, 0);
		else if (reinit)
			vsi = i40e_vsi_reinit_setup(pf->vsi[pf->lan_vsi]);
		if (!vsi) {
			dev_info(&pf->pdev->dev, "setup of MAIN VSI failed\n");
			i40e_cloud_filter_exit(pf);
			i40e_fdir_teardown(pf);
			return -EAGAIN;
		}
	} else {
		 
		u8 enabled_tc = pf->vsi[pf->lan_vsi]->tc_config.enabled_tc;

		pf->vsi[pf->lan_vsi]->tc_config.enabled_tc = 0;
		pf->vsi[pf->lan_vsi]->seid = pf->main_vsi_seid;
		i40e_vsi_config_tc(pf->vsi[pf->lan_vsi], enabled_tc);
	}
	i40e_vlan_stripping_disable(pf->vsi[pf->lan_vsi]);

	i40e_fdir_sb_setup(pf);

	 
	ret = i40e_setup_pf_filter_control(pf);
	if (ret) {
		dev_info(&pf->pdev->dev, "setup_pf_filter_control failed: %d\n",
			 ret);
		 
	}

	 
	if ((pf->flags & I40E_FLAG_RSS_ENABLED))
		i40e_pf_config_rss(pf);

	 
	i40e_link_event(pf);

	 
	pf->fc_autoneg_status = ((pf->hw.phy.link_info.an_info &
				  I40E_AQ_AN_COMPLETED) ? true : false);

	i40e_ptp_init(pf);

	if (!lock_acquired)
		rtnl_lock();

	 
	udp_tunnel_nic_reset_ntf(pf->vsi[pf->lan_vsi]->netdev);

	if (!lock_acquired)
		rtnl_unlock();

	return ret;
}

 
static void i40e_determine_queue_usage(struct i40e_pf *pf)
{
	int queues_left;
	int q_max;

	pf->num_lan_qps = 0;

	 
	queues_left = pf->hw.func_caps.num_tx_qp;

	if ((queues_left == 1) ||
	    !(pf->flags & I40E_FLAG_MSIX_ENABLED)) {
		 
		queues_left = 0;
		pf->alloc_rss_size = pf->num_lan_qps = 1;

		 
		pf->flags &= ~(I40E_FLAG_RSS_ENABLED	|
			       I40E_FLAG_IWARP_ENABLED	|
			       I40E_FLAG_FD_SB_ENABLED	|
			       I40E_FLAG_FD_ATR_ENABLED	|
			       I40E_FLAG_DCB_CAPABLE	|
			       I40E_FLAG_DCB_ENABLED	|
			       I40E_FLAG_SRIOV_ENABLED	|
			       I40E_FLAG_VMDQ_ENABLED);
		pf->flags |= I40E_FLAG_FD_SB_INACTIVE;
	} else if (!(pf->flags & (I40E_FLAG_RSS_ENABLED |
				  I40E_FLAG_FD_SB_ENABLED |
				  I40E_FLAG_FD_ATR_ENABLED |
				  I40E_FLAG_DCB_CAPABLE))) {
		 
		pf->alloc_rss_size = pf->num_lan_qps = 1;
		queues_left -= pf->num_lan_qps;

		pf->flags &= ~(I40E_FLAG_RSS_ENABLED	|
			       I40E_FLAG_IWARP_ENABLED	|
			       I40E_FLAG_FD_SB_ENABLED	|
			       I40E_FLAG_FD_ATR_ENABLED	|
			       I40E_FLAG_DCB_ENABLED	|
			       I40E_FLAG_VMDQ_ENABLED);
		pf->flags |= I40E_FLAG_FD_SB_INACTIVE;
	} else {
		 
		if ((pf->flags & I40E_FLAG_DCB_CAPABLE) &&
		    (queues_left < I40E_MAX_TRAFFIC_CLASS)) {
			pf->flags &= ~(I40E_FLAG_DCB_CAPABLE |
					I40E_FLAG_DCB_ENABLED);
			dev_info(&pf->pdev->dev, "not enough queues for DCB. DCB is disabled.\n");
		}

		 
		q_max = max_t(int, pf->rss_size_max, num_online_cpus());
		q_max = min_t(int, q_max, pf->hw.func_caps.num_tx_qp);
		q_max = min_t(int, q_max, pf->hw.func_caps.num_msix_vectors);
		pf->num_lan_qps = q_max;

		queues_left -= pf->num_lan_qps;
	}

	if (pf->flags & I40E_FLAG_FD_SB_ENABLED) {
		if (queues_left > 1) {
			queues_left -= 1;  
		} else {
			pf->flags &= ~I40E_FLAG_FD_SB_ENABLED;
			pf->flags |= I40E_FLAG_FD_SB_INACTIVE;
			dev_info(&pf->pdev->dev, "not enough queues for Flow Director. Flow Director feature is disabled\n");
		}
	}

	if ((pf->flags & I40E_FLAG_SRIOV_ENABLED) &&
	    pf->num_vf_qps && pf->num_req_vfs && queues_left) {
		pf->num_req_vfs = min_t(int, pf->num_req_vfs,
					(queues_left / pf->num_vf_qps));
		queues_left -= (pf->num_req_vfs * pf->num_vf_qps);
	}

	if ((pf->flags & I40E_FLAG_VMDQ_ENABLED) &&
	    pf->num_vmdq_vsis && pf->num_vmdq_qps && queues_left) {
		pf->num_vmdq_vsis = min_t(int, pf->num_vmdq_vsis,
					  (queues_left / pf->num_vmdq_qps));
		queues_left -= (pf->num_vmdq_vsis * pf->num_vmdq_qps);
	}

	pf->queues_left = queues_left;
	dev_dbg(&pf->pdev->dev,
		"qs_avail=%d FD SB=%d lan_qs=%d lan_tc0=%d vf=%d*%d vmdq=%d*%d, remaining=%d\n",
		pf->hw.func_caps.num_tx_qp,
		!!(pf->flags & I40E_FLAG_FD_SB_ENABLED),
		pf->num_lan_qps, pf->alloc_rss_size, pf->num_req_vfs,
		pf->num_vf_qps, pf->num_vmdq_vsis, pf->num_vmdq_qps,
		queues_left);
}

 
static int i40e_setup_pf_filter_control(struct i40e_pf *pf)
{
	struct i40e_filter_control_settings *settings = &pf->filter_settings;

	settings->hash_lut_size = I40E_HASH_LUT_SIZE_128;

	 
	if (pf->flags & (I40E_FLAG_FD_SB_ENABLED | I40E_FLAG_FD_ATR_ENABLED))
		settings->enable_fdir = true;

	 
	settings->enable_ethtype = true;
	settings->enable_macvlan = true;

	if (i40e_set_filter_control(&pf->hw, settings))
		return -ENOENT;

	return 0;
}

#define INFO_STRING_LEN 255
#define REMAIN(__x) (INFO_STRING_LEN - (__x))
static void i40e_print_features(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	char *buf;
	int i;

	buf = kmalloc(INFO_STRING_LEN, GFP_KERNEL);
	if (!buf)
		return;

	i = snprintf(buf, INFO_STRING_LEN, "Features: PF-id[%d]", hw->pf_id);
#ifdef CONFIG_PCI_IOV
	i += scnprintf(&buf[i], REMAIN(i), " VFs: %d", pf->num_req_vfs);
#endif
	i += scnprintf(&buf[i], REMAIN(i), " VSIs: %d QP: %d",
		      pf->hw.func_caps.num_vsis,
		      pf->vsi[pf->lan_vsi]->num_queue_pairs);
	if (pf->flags & I40E_FLAG_RSS_ENABLED)
		i += scnprintf(&buf[i], REMAIN(i), " RSS");
	if (pf->flags & I40E_FLAG_FD_ATR_ENABLED)
		i += scnprintf(&buf[i], REMAIN(i), " FD_ATR");
	if (pf->flags & I40E_FLAG_FD_SB_ENABLED) {
		i += scnprintf(&buf[i], REMAIN(i), " FD_SB");
		i += scnprintf(&buf[i], REMAIN(i), " NTUPLE");
	}
	if (pf->flags & I40E_FLAG_DCB_CAPABLE)
		i += scnprintf(&buf[i], REMAIN(i), " DCB");
	i += scnprintf(&buf[i], REMAIN(i), " VxLAN");
	i += scnprintf(&buf[i], REMAIN(i), " Geneve");
	if (pf->flags & I40E_FLAG_PTP)
		i += scnprintf(&buf[i], REMAIN(i), " PTP");
	if (pf->flags & I40E_FLAG_VEB_MODE_ENABLED)
		i += scnprintf(&buf[i], REMAIN(i), " VEB");
	else
		i += scnprintf(&buf[i], REMAIN(i), " VEPA");

	dev_info(&pf->pdev->dev, "%s\n", buf);
	kfree(buf);
	WARN_ON(i > INFO_STRING_LEN);
}

 
static void i40e_get_platform_mac_addr(struct pci_dev *pdev, struct i40e_pf *pf)
{
	if (eth_platform_get_mac_address(&pdev->dev, pf->hw.mac.addr))
		i40e_get_mac_addr(&pf->hw, pf->hw.mac.addr);
}

 
void i40e_set_fec_in_flags(u8 fec_cfg, u32 *flags)
{
	if (fec_cfg & I40E_AQ_SET_FEC_AUTO)
		*flags |= I40E_FLAG_RS_FEC | I40E_FLAG_BASE_R_FEC;
	if ((fec_cfg & I40E_AQ_SET_FEC_REQUEST_RS) ||
	    (fec_cfg & I40E_AQ_SET_FEC_ABILITY_RS)) {
		*flags |= I40E_FLAG_RS_FEC;
		*flags &= ~I40E_FLAG_BASE_R_FEC;
	}
	if ((fec_cfg & I40E_AQ_SET_FEC_REQUEST_KR) ||
	    (fec_cfg & I40E_AQ_SET_FEC_ABILITY_KR)) {
		*flags |= I40E_FLAG_BASE_R_FEC;
		*flags &= ~I40E_FLAG_RS_FEC;
	}
	if (fec_cfg == 0)
		*flags &= ~(I40E_FLAG_RS_FEC | I40E_FLAG_BASE_R_FEC);
}

 
static bool i40e_check_recovery_mode(struct i40e_pf *pf)
{
	u32 val = rd32(&pf->hw, I40E_GL_FWSTS);

	if (val & I40E_GL_FWSTS_FWS1B_MASK) {
		dev_crit(&pf->pdev->dev, "Firmware recovery mode detected. Limiting functionality.\n");
		dev_crit(&pf->pdev->dev, "Refer to the Intel(R) Ethernet Adapters and Devices User Guide for details on firmware recovery mode.\n");
		set_bit(__I40E_RECOVERY_MODE, pf->state);

		return true;
	}
	if (test_bit(__I40E_RECOVERY_MODE, pf->state))
		dev_info(&pf->pdev->dev, "Please do Power-On Reset to initialize adapter in normal mode with full functionality.\n");

	return false;
}

 
static int i40e_pf_loop_reset(struct i40e_pf *pf)
{
	 
	const unsigned long time_end = jiffies + 10 * HZ;
	struct i40e_hw *hw = &pf->hw;
	int ret;

	ret = i40e_pf_reset(hw);
	while (ret != 0 && time_before(jiffies, time_end)) {
		usleep_range(10000, 20000);
		ret = i40e_pf_reset(hw);
	}

	if (ret == 0)
		pf->pfr_count++;
	else
		dev_info(&pf->pdev->dev, "PF reset failed: %d\n", ret);

	return ret;
}

 
static bool i40e_check_fw_empr(struct i40e_pf *pf)
{
	const u32 fw_sts = rd32(&pf->hw, I40E_GL_FWSTS) &
			   I40E_GL_FWSTS_FWS1B_MASK;
	return (fw_sts > I40E_GL_FWSTS_FWS1B_EMPR_0) &&
	       (fw_sts <= I40E_GL_FWSTS_FWS1B_EMPR_10);
}

 
static int i40e_handle_resets(struct i40e_pf *pf)
{
	const int pfr = i40e_pf_loop_reset(pf);
	const bool is_empr = i40e_check_fw_empr(pf);

	if (is_empr || pfr != 0)
		dev_crit(&pf->pdev->dev, "Entering recovery mode due to repeated FW resets. This may take several minutes. Refer to the Intel(R) Ethernet Adapters and Devices User Guide.\n");

	return is_empr ? -EIO : pfr;
}

 
static int i40e_init_recovery_mode(struct i40e_pf *pf, struct i40e_hw *hw)
{
	struct i40e_vsi *vsi;
	int err;
	int v_idx;

	pci_set_drvdata(pf->pdev, pf);
	pci_save_state(pf->pdev);

	 
	timer_setup(&pf->service_timer, i40e_service_timer, 0);
	pf->service_timer_period = HZ;

	INIT_WORK(&pf->service_task, i40e_service_task);
	clear_bit(__I40E_SERVICE_SCHED, pf->state);

	err = i40e_init_interrupt_scheme(pf);
	if (err)
		goto err_switch_setup;

	 
	if (pf->hw.func_caps.num_vsis < I40E_MIN_VSI_ALLOC)
		pf->num_alloc_vsi = I40E_MIN_VSI_ALLOC;
	else
		pf->num_alloc_vsi = pf->hw.func_caps.num_vsis;

	 
	pf->vsi = kcalloc(pf->num_alloc_vsi, sizeof(struct i40e_vsi *),
			  GFP_KERNEL);
	if (!pf->vsi) {
		err = -ENOMEM;
		goto err_switch_setup;
	}

	 
	v_idx = i40e_vsi_mem_alloc(pf, I40E_VSI_MAIN);
	if (v_idx < 0) {
		err = v_idx;
		goto err_switch_setup;
	}
	pf->lan_vsi = v_idx;
	vsi = pf->vsi[v_idx];
	if (!vsi) {
		err = -EFAULT;
		goto err_switch_setup;
	}
	vsi->alloc_queue_pairs = 1;
	err = i40e_config_netdev(vsi);
	if (err)
		goto err_switch_setup;
	err = register_netdev(vsi->netdev);
	if (err)
		goto err_switch_setup;
	vsi->netdev_registered = true;
	i40e_dbg_pf_init(pf);

	err = i40e_setup_misc_vector_for_recovery_mode(pf);
	if (err)
		goto err_switch_setup;

	 
	i40e_send_version(pf);

	 
	mod_timer(&pf->service_timer,
		  round_jiffies(jiffies + pf->service_timer_period));

	return 0;

err_switch_setup:
	i40e_reset_interrupt_capability(pf);
	timer_shutdown_sync(&pf->service_timer);
	i40e_shutdown_adminq(hw);
	iounmap(hw->hw_addr);
	pci_release_mem_regions(pf->pdev);
	pci_disable_device(pf->pdev);
	kfree(pf);

	return err;
}

 
static inline void i40e_set_subsystem_device_id(struct i40e_hw *hw)
{
	struct pci_dev *pdev = ((struct i40e_pf *)hw->back)->pdev;

	hw->subsystem_device_id = pdev->subsystem_device ?
		pdev->subsystem_device :
		(ushort)(rd32(hw, I40E_PFPCI_SUBSYSID) & USHRT_MAX);
}

 
static int i40e_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct i40e_aq_get_phy_abilities_resp abilities;
#ifdef CONFIG_I40E_DCB
	enum i40e_get_fw_lldp_status_resp lldp_status;
#endif  
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	static u16 pfs_found;
	u16 wol_nvm_bits;
	u16 link_status;
#ifdef CONFIG_I40E_DCB
	int status;
#endif  
	int err;
	u32 val;
	u32 i;

	err = pci_enable_device_mem(pdev);
	if (err)
		return err;

	 
	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (err) {
		dev_err(&pdev->dev,
			"DMA configuration failed: 0x%x\n", err);
		goto err_dma;
	}

	 
	err = pci_request_mem_regions(pdev, i40e_driver_name);
	if (err) {
		dev_info(&pdev->dev,
			 "pci_request_selected_regions failed %d\n", err);
		goto err_pci_reg;
	}

	pci_set_master(pdev);

	 
	pf = kzalloc(sizeof(*pf), GFP_KERNEL);
	if (!pf) {
		err = -ENOMEM;
		goto err_pf_alloc;
	}
	pf->next_vsi = 0;
	pf->pdev = pdev;
	set_bit(__I40E_DOWN, pf->state);

	hw = &pf->hw;
	hw->back = pf;

	pf->ioremap_len = min_t(int, pci_resource_len(pdev, 0),
				I40E_MAX_CSR_SPACE);
	 
	if (pf->ioremap_len < I40E_GLGEN_STAT_CLEAR) {
		dev_err(&pdev->dev, "Cannot map registers, bar size 0x%X too small, aborting\n",
			pf->ioremap_len);
		err = -ENOMEM;
		goto err_ioremap;
	}
	hw->hw_addr = ioremap(pci_resource_start(pdev, 0), pf->ioremap_len);
	if (!hw->hw_addr) {
		err = -EIO;
		dev_info(&pdev->dev, "ioremap(0x%04x, 0x%04x) failed: 0x%x\n",
			 (unsigned int)pci_resource_start(pdev, 0),
			 pf->ioremap_len, err);
		goto err_ioremap;
	}
	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	pci_read_config_byte(pdev, PCI_REVISION_ID, &hw->revision_id);
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	i40e_set_subsystem_device_id(hw);
	hw->bus.device = PCI_SLOT(pdev->devfn);
	hw->bus.func = PCI_FUNC(pdev->devfn);
	hw->bus.bus_id = pdev->bus->number;
	pf->instance = pfs_found;

	 
	hw->switch_tag = 0xffff;
	hw->first_tag = ETH_P_8021AD;
	hw->second_tag = ETH_P_8021Q;

	INIT_LIST_HEAD(&pf->l3_flex_pit_list);
	INIT_LIST_HEAD(&pf->l4_flex_pit_list);
	INIT_LIST_HEAD(&pf->ddp_old_prof);

	 
	mutex_init(&hw->aq.asq_mutex);
	mutex_init(&hw->aq.arq_mutex);

	pf->msg_enable = netif_msg_init(debug,
					NETIF_MSG_DRV |
					NETIF_MSG_PROBE |
					NETIF_MSG_LINK);
	if (debug < -1)
		pf->hw.debug_mask = debug;

	 
	if (hw->revision_id == 0 &&
	    (rd32(hw, I40E_GLLAN_RCTL_0) & I40E_GLLAN_RCTL_0_PXE_MODE_MASK)) {
		wr32(hw, I40E_GLGEN_RTRIG, I40E_GLGEN_RTRIG_CORER_MASK);
		i40e_flush(hw);
		msleep(200);
		pf->corer_count++;

		i40e_clear_pxe_mode(hw);
	}

	 
	i40e_clear_hw(hw);

	err = i40e_set_mac_type(hw);
	if (err) {
		dev_warn(&pdev->dev, "unidentified MAC or BLANK NVM: %d\n",
			 err);
		goto err_pf_reset;
	}

	err = i40e_handle_resets(pf);
	if (err)
		goto err_pf_reset;

	i40e_check_recovery_mode(pf);

	if (is_kdump_kernel()) {
		hw->aq.num_arq_entries = I40E_MIN_ARQ_LEN;
		hw->aq.num_asq_entries = I40E_MIN_ASQ_LEN;
	} else {
		hw->aq.num_arq_entries = I40E_AQ_LEN;
		hw->aq.num_asq_entries = I40E_AQ_LEN;
	}
	hw->aq.arq_buf_size = I40E_MAX_AQ_BUF_SIZE;
	hw->aq.asq_buf_size = I40E_MAX_AQ_BUF_SIZE;
	pf->adminq_work_limit = I40E_AQ_WORK_LIMIT;

	snprintf(pf->int_name, sizeof(pf->int_name) - 1,
		 "%s-%s:misc",
		 dev_driver_string(&pf->pdev->dev), dev_name(&pdev->dev));

	err = i40e_init_shared_code(hw);
	if (err) {
		dev_warn(&pdev->dev, "unidentified MAC or BLANK NVM: %d\n",
			 err);
		goto err_pf_reset;
	}

	 
	pf->hw.fc.requested_mode = I40E_FC_NONE;

	err = i40e_init_adminq(hw);
	if (err) {
		if (err == -EIO)
			dev_info(&pdev->dev,
				 "The driver for the device stopped because the NVM image v%u.%u is newer than expected v%u.%u. You must install the most recent version of the network driver.\n",
				 hw->aq.api_maj_ver,
				 hw->aq.api_min_ver,
				 I40E_FW_API_VERSION_MAJOR,
				 I40E_FW_MINOR_VERSION(hw));
		else
			dev_info(&pdev->dev,
				 "The driver for the device stopped because the device firmware failed to init. Try updating your NVM image.\n");

		goto err_pf_reset;
	}
	i40e_get_oem_version(hw);

	 
	dev_info(&pdev->dev, "fw %d.%d.%05d api %d.%d nvm %s [%04x:%04x] [%04x:%04x]\n",
		 hw->aq.fw_maj_ver, hw->aq.fw_min_ver, hw->aq.fw_build,
		 hw->aq.api_maj_ver, hw->aq.api_min_ver,
		 i40e_nvm_version_str(hw), hw->vendor_id, hw->device_id,
		 hw->subsystem_vendor_id, hw->subsystem_device_id);

	if (hw->aq.api_maj_ver == I40E_FW_API_VERSION_MAJOR &&
	    hw->aq.api_min_ver > I40E_FW_MINOR_VERSION(hw))
		dev_dbg(&pdev->dev,
			"The driver for the device detected a newer version of the NVM image v%u.%u than v%u.%u.\n",
			 hw->aq.api_maj_ver,
			 hw->aq.api_min_ver,
			 I40E_FW_API_VERSION_MAJOR,
			 I40E_FW_MINOR_VERSION(hw));
	else if (hw->aq.api_maj_ver == 1 && hw->aq.api_min_ver < 4)
		dev_info(&pdev->dev,
			 "The driver for the device detected an older version of the NVM image v%u.%u than expected v%u.%u. Please update the NVM image.\n",
			 hw->aq.api_maj_ver,
			 hw->aq.api_min_ver,
			 I40E_FW_API_VERSION_MAJOR,
			 I40E_FW_MINOR_VERSION(hw));

	i40e_verify_eeprom(pf);

	 
	if (hw->revision_id < 1)
		dev_warn(&pdev->dev, "This device is a pre-production adapter/LOM. Please be aware there may be issues with your hardware. If you are experiencing problems please contact your Intel or hardware representative who provided you with this hardware.\n");

	i40e_clear_pxe_mode(hw);

	err = i40e_get_capabilities(pf, i40e_aqc_opc_list_func_capabilities);
	if (err)
		goto err_adminq_setup;

	err = i40e_sw_init(pf);
	if (err) {
		dev_info(&pdev->dev, "sw_init failed: %d\n", err);
		goto err_sw_init;
	}

	if (test_bit(__I40E_RECOVERY_MODE, pf->state))
		return i40e_init_recovery_mode(pf, hw);

	err = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
				hw->func_caps.num_rx_qp, 0, 0);
	if (err) {
		dev_info(&pdev->dev, "init_lan_hmc failed: %d\n", err);
		goto err_init_lan_hmc;
	}

	err = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (err) {
		dev_info(&pdev->dev, "configure_lan_hmc failed: %d\n", err);
		err = -ENOENT;
		goto err_configure_lan_hmc;
	}

	 
	if (pf->hw_features & I40E_HW_STOP_FW_LLDP) {
		dev_info(&pdev->dev, "Stopping firmware LLDP agent.\n");
		i40e_aq_stop_lldp(hw, true, false, NULL);
	}

	 
	i40e_get_platform_mac_addr(pdev, pf);

	if (!is_valid_ether_addr(hw->mac.addr)) {
		dev_info(&pdev->dev, "invalid MAC address %pM\n", hw->mac.addr);
		err = -EIO;
		goto err_mac_addr;
	}
	dev_info(&pdev->dev, "MAC address: %pM\n", hw->mac.addr);
	ether_addr_copy(hw->mac.perm_addr, hw->mac.addr);
	i40e_get_port_mac_addr(hw, hw->mac.port_addr);
	if (is_valid_ether_addr(hw->mac.port_addr))
		pf->hw_features |= I40E_HW_PORT_ID_VALID;

	i40e_ptp_alloc_pins(pf);
	pci_set_drvdata(pdev, pf);
	pci_save_state(pdev);

#ifdef CONFIG_I40E_DCB
	status = i40e_get_fw_lldp_status(&pf->hw, &lldp_status);
	(!status &&
	 lldp_status == I40E_GET_FW_LLDP_STATUS_ENABLED) ?
		(pf->flags &= ~I40E_FLAG_DISABLE_FW_LLDP) :
		(pf->flags |= I40E_FLAG_DISABLE_FW_LLDP);
	dev_info(&pdev->dev,
		 (pf->flags & I40E_FLAG_DISABLE_FW_LLDP) ?
			"FW LLDP is disabled\n" :
			"FW LLDP is enabled\n");

	 
	i40e_aq_set_dcb_parameters(hw, true, NULL);

	err = i40e_init_pf_dcb(pf);
	if (err) {
		dev_info(&pdev->dev, "DCB init failed %d, disabled\n", err);
		pf->flags &= ~(I40E_FLAG_DCB_CAPABLE | I40E_FLAG_DCB_ENABLED);
		 
	}
#endif  

	 
	timer_setup(&pf->service_timer, i40e_service_timer, 0);
	pf->service_timer_period = HZ;

	INIT_WORK(&pf->service_task, i40e_service_task);
	clear_bit(__I40E_SERVICE_SCHED, pf->state);

	 
	i40e_read_nvm_word(hw, I40E_SR_NVM_WAKE_ON_LAN, &wol_nvm_bits);
	if (BIT (hw->port) & wol_nvm_bits || hw->partition_id != 1)
		pf->wol_en = false;
	else
		pf->wol_en = true;
	device_set_wakeup_enable(&pf->pdev->dev, pf->wol_en);

	 
	i40e_determine_queue_usage(pf);
	err = i40e_init_interrupt_scheme(pf);
	if (err)
		goto err_switch_setup;

	 
	if (is_kdump_kernel())
		pf->num_lan_msix = 1;

	pf->udp_tunnel_nic.set_port = i40e_udp_tunnel_set_port;
	pf->udp_tunnel_nic.unset_port = i40e_udp_tunnel_unset_port;
	pf->udp_tunnel_nic.flags = UDP_TUNNEL_NIC_INFO_MAY_SLEEP;
	pf->udp_tunnel_nic.shared = &pf->udp_tunnel_shared;
	pf->udp_tunnel_nic.tables[0].n_entries = I40E_MAX_PF_UDP_OFFLOAD_PORTS;
	pf->udp_tunnel_nic.tables[0].tunnel_types = UDP_TUNNEL_TYPE_VXLAN |
						    UDP_TUNNEL_TYPE_GENEVE;

	 
	if (pf->hw.func_caps.num_vsis < I40E_MIN_VSI_ALLOC)
		pf->num_alloc_vsi = I40E_MIN_VSI_ALLOC;
	else
		pf->num_alloc_vsi = pf->hw.func_caps.num_vsis;
	if (pf->num_alloc_vsi > UDP_TUNNEL_NIC_MAX_SHARING_DEVICES) {
		dev_warn(&pf->pdev->dev,
			 "limiting the VSI count due to UDP tunnel limitation %d > %d\n",
			 pf->num_alloc_vsi, UDP_TUNNEL_NIC_MAX_SHARING_DEVICES);
		pf->num_alloc_vsi = UDP_TUNNEL_NIC_MAX_SHARING_DEVICES;
	}

	 
	pf->vsi = kcalloc(pf->num_alloc_vsi, sizeof(struct i40e_vsi *),
			  GFP_KERNEL);
	if (!pf->vsi) {
		err = -ENOMEM;
		goto err_switch_setup;
	}

#ifdef CONFIG_PCI_IOV
	 
	if ((pf->flags & I40E_FLAG_SRIOV_ENABLED) &&
	    (pf->flags & I40E_FLAG_MSIX_ENABLED) &&
	    !test_bit(__I40E_BAD_EEPROM, pf->state)) {
		if (pci_num_vf(pdev))
			pf->flags |= I40E_FLAG_VEB_MODE_ENABLED;
	}
#endif
	err = i40e_setup_pf_switch(pf, false, false);
	if (err) {
		dev_info(&pdev->dev, "setup_pf_switch failed: %d\n", err);
		goto err_vsis;
	}
	INIT_LIST_HEAD(&pf->vsi[pf->lan_vsi]->ch_list);

	 
	for (i = 0; i < pf->num_alloc_vsi; i++) {
		if (pf->vsi[i] && pf->vsi[i]->type == I40E_VSI_FDIR) {
			i40e_vsi_open(pf->vsi[i]);
			break;
		}
	}

	 
	err = i40e_aq_set_phy_int_mask(&pf->hw,
				       ~(I40E_AQ_EVENT_LINK_UPDOWN |
					 I40E_AQ_EVENT_MEDIA_NA |
					 I40E_AQ_EVENT_MODULE_QUAL_FAIL), NULL);
	if (err)
		dev_info(&pf->pdev->dev, "set phy mask fail, err %pe aq_err %s\n",
			 ERR_PTR(err),
			 i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));

	 
	val = rd32(hw, I40E_REG_MSS);
	if ((val & I40E_REG_MSS_MIN_MASK) > I40E_64BYTE_MSS) {
		val &= ~I40E_REG_MSS_MIN_MASK;
		val |= I40E_64BYTE_MSS;
		wr32(hw, I40E_REG_MSS, val);
	}

	if (pf->hw_features & I40E_HW_RESTART_AUTONEG) {
		msleep(75);
		err = i40e_aq_set_link_restart_an(&pf->hw, true, NULL);
		if (err)
			dev_info(&pf->pdev->dev, "link restart failed, err %pe aq_err %s\n",
				 ERR_PTR(err),
				 i40e_aq_str(&pf->hw,
					     pf->hw.aq.asq_last_status));
	}
	 
	clear_bit(__I40E_DOWN, pf->state);

	 
	if (pf->flags & I40E_FLAG_MSIX_ENABLED) {
		err = i40e_setup_misc_vector(pf);
		if (err) {
			dev_info(&pdev->dev,
				 "setup of misc vector failed: %d\n", err);
			i40e_cloud_filter_exit(pf);
			i40e_fdir_teardown(pf);
			goto err_vsis;
		}
	}

#ifdef CONFIG_PCI_IOV
	 
	if ((pf->flags & I40E_FLAG_SRIOV_ENABLED) &&
	    (pf->flags & I40E_FLAG_MSIX_ENABLED) &&
	    !test_bit(__I40E_BAD_EEPROM, pf->state)) {
		 
		val = rd32(hw, I40E_PFGEN_PORTMDIO_NUM);
		val &= ~I40E_PFGEN_PORTMDIO_NUM_VFLINK_STAT_ENA_MASK;
		wr32(hw, I40E_PFGEN_PORTMDIO_NUM, val);
		i40e_flush(hw);

		if (pci_num_vf(pdev)) {
			dev_info(&pdev->dev,
				 "Active VFs found, allocating resources.\n");
			err = i40e_alloc_vfs(pf, pci_num_vf(pdev));
			if (err)
				dev_info(&pdev->dev,
					 "Error %d allocating resources for existing VFs\n",
					 err);
		}
	}
#endif  

	if (pf->flags & I40E_FLAG_IWARP_ENABLED) {
		pf->iwarp_base_vector = i40e_get_lump(pf, pf->irq_pile,
						      pf->num_iwarp_msix,
						      I40E_IWARP_IRQ_PILE_ID);
		if (pf->iwarp_base_vector < 0) {
			dev_info(&pdev->dev,
				 "failed to get tracking for %d vectors for IWARP err=%d\n",
				 pf->num_iwarp_msix, pf->iwarp_base_vector);
			pf->flags &= ~I40E_FLAG_IWARP_ENABLED;
		}
	}

	i40e_dbg_pf_init(pf);

	 
	i40e_send_version(pf);

	 
	mod_timer(&pf->service_timer,
		  round_jiffies(jiffies + pf->service_timer_period));

	 
	if (pf->flags & I40E_FLAG_IWARP_ENABLED) {
		err = i40e_lan_add_device(pf);
		if (err)
			dev_info(&pdev->dev, "Failed to add PF to client API service list: %d\n",
				 err);
	}

#define PCI_SPEED_SIZE 8
#define PCI_WIDTH_SIZE 8
	 
	if (!(pf->hw_features & I40E_HW_NO_PCI_LINK_CHECK)) {
		char speed[PCI_SPEED_SIZE] = "Unknown";
		char width[PCI_WIDTH_SIZE] = "Unknown";

		 
		pcie_capability_read_word(pf->pdev, PCI_EXP_LNKSTA,
					  &link_status);

		i40e_set_pci_config_data(hw, link_status);

		switch (hw->bus.speed) {
		case i40e_bus_speed_8000:
			strscpy(speed, "8.0", PCI_SPEED_SIZE); break;
		case i40e_bus_speed_5000:
			strscpy(speed, "5.0", PCI_SPEED_SIZE); break;
		case i40e_bus_speed_2500:
			strscpy(speed, "2.5", PCI_SPEED_SIZE); break;
		default:
			break;
		}
		switch (hw->bus.width) {
		case i40e_bus_width_pcie_x8:
			strscpy(width, "8", PCI_WIDTH_SIZE); break;
		case i40e_bus_width_pcie_x4:
			strscpy(width, "4", PCI_WIDTH_SIZE); break;
		case i40e_bus_width_pcie_x2:
			strscpy(width, "2", PCI_WIDTH_SIZE); break;
		case i40e_bus_width_pcie_x1:
			strscpy(width, "1", PCI_WIDTH_SIZE); break;
		default:
			break;
		}

		dev_info(&pdev->dev, "PCI-Express: Speed %sGT/s Width x%s\n",
			 speed, width);

		if (hw->bus.width < i40e_bus_width_pcie_x8 ||
		    hw->bus.speed < i40e_bus_speed_8000) {
			dev_warn(&pdev->dev, "PCI-Express bandwidth available for this device may be insufficient for optimal performance.\n");
			dev_warn(&pdev->dev, "Please move the device to a different PCI-e link with more lanes and/or higher transfer rate.\n");
		}
	}

	 
	err = i40e_aq_get_phy_capabilities(hw, false, false, &abilities, NULL);
	if (err)
		dev_dbg(&pf->pdev->dev, "get requested speeds ret =  %pe last_status =  %s\n",
			ERR_PTR(err),
			i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
	pf->hw.phy.link_info.requested_speeds = abilities.link_speed;

	 
	i40e_set_fec_in_flags(abilities.fec_cfg_curr_mod_ext_info, &pf->flags);

	 
	err = i40e_aq_get_phy_capabilities(hw, false, true, &abilities, NULL);
	if (err)
		dev_dbg(&pf->pdev->dev, "get supported phy types ret =  %pe last_status =  %s\n",
			ERR_PTR(err),
			i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));

	 
#define MAX_FRAME_SIZE_DEFAULT 0x2600
	val = (rd32(&pf->hw, I40E_PRTGL_SAH) &
	       I40E_PRTGL_SAH_MFS_MASK) >> I40E_PRTGL_SAH_MFS_SHIFT;
	if (val < MAX_FRAME_SIZE_DEFAULT)
		dev_warn(&pdev->dev, "MFS for port %x has been set below the default: %x\n",
			 pf->hw.port, val);

	 
	i40e_add_filter_to_drop_tx_flow_control_frames(&pf->hw,
						       pf->main_vsi_seid);

	if ((pf->hw.device_id == I40E_DEV_ID_10G_BASE_T) ||
		(pf->hw.device_id == I40E_DEV_ID_10G_BASE_T4))
		pf->hw_features |= I40E_HW_PHY_CONTROLS_LEDS;
	if (pf->hw.device_id == I40E_DEV_ID_SFP_I_X722)
		pf->hw_features |= I40E_HW_HAVE_CRT_RETIMER;
	 
	i40e_print_features(pf);

	return 0;

	 
err_vsis:
	set_bit(__I40E_DOWN, pf->state);
	i40e_clear_interrupt_scheme(pf);
	kfree(pf->vsi);
err_switch_setup:
	i40e_reset_interrupt_capability(pf);
	timer_shutdown_sync(&pf->service_timer);
err_mac_addr:
err_configure_lan_hmc:
	(void)i40e_shutdown_lan_hmc(hw);
err_init_lan_hmc:
	kfree(pf->qp_pile);
err_sw_init:
err_adminq_setup:
err_pf_reset:
	iounmap(hw->hw_addr);
err_ioremap:
	kfree(pf);
err_pf_alloc:
	pci_release_mem_regions(pdev);
err_pci_reg:
err_dma:
	pci_disable_device(pdev);
	return err;
}

 
static void i40e_remove(struct pci_dev *pdev)
{
	struct i40e_pf *pf = pci_get_drvdata(pdev);
	struct i40e_hw *hw = &pf->hw;
	int ret_code;
	int i;

	i40e_dbg_pf_exit(pf);

	i40e_ptp_stop(pf);

	 
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(0), 0);
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(1), 0);

	 
	while (test_and_set_bit(__I40E_RESET_RECOVERY_PENDING, pf->state))
		usleep_range(1000, 2000);
	set_bit(__I40E_IN_REMOVE, pf->state);

	if (pf->flags & I40E_FLAG_SRIOV_ENABLED) {
		set_bit(__I40E_VF_RESETS_DISABLED, pf->state);
		i40e_free_vfs(pf);
		pf->flags &= ~I40E_FLAG_SRIOV_ENABLED;
	}
	 
	set_bit(__I40E_SUSPENDED, pf->state);
	set_bit(__I40E_DOWN, pf->state);
	if (pf->service_timer.function)
		timer_shutdown_sync(&pf->service_timer);
	if (pf->service_task.func)
		cancel_work_sync(&pf->service_task);

	if (test_bit(__I40E_RECOVERY_MODE, pf->state)) {
		struct i40e_vsi *vsi = pf->vsi[0];

		 
		unregister_netdev(vsi->netdev);
		free_netdev(vsi->netdev);

		goto unmap;
	}

	 
	i40e_notify_client_of_netdev_close(pf->vsi[pf->lan_vsi], false);

	i40e_fdir_teardown(pf);

	 
	for (i = 0; i < I40E_MAX_VEB; i++) {
		if (!pf->veb[i])
			continue;

		if (pf->veb[i]->uplink_seid == pf->mac_seid ||
		    pf->veb[i]->uplink_seid == 0)
			i40e_switch_branch_release(pf->veb[i]);
	}

	 
	for (i = pf->num_alloc_vsi; i--;)
		if (pf->vsi[i]) {
			i40e_vsi_close(pf->vsi[i]);
			i40e_vsi_release(pf->vsi[i]);
			pf->vsi[i] = NULL;
		}

	i40e_cloud_filter_exit(pf);

	 
	if (pf->flags & I40E_FLAG_IWARP_ENABLED) {
		ret_code = i40e_lan_del_device(pf);
		if (ret_code)
			dev_warn(&pdev->dev, "Failed to delete client device: %d\n",
				 ret_code);
	}

	 
	if (hw->hmc.hmc_obj) {
		ret_code = i40e_shutdown_lan_hmc(hw);
		if (ret_code)
			dev_warn(&pdev->dev,
				 "Failed to destroy the HMC resources: %d\n",
				 ret_code);
	}

unmap:
	 
	if (test_bit(__I40E_RECOVERY_MODE, pf->state) &&
	    !(pf->flags & I40E_FLAG_MSIX_ENABLED))
		free_irq(pf->pdev->irq, pf);

	 
	i40e_shutdown_adminq(hw);

	 
	mutex_destroy(&hw->aq.arq_mutex);
	mutex_destroy(&hw->aq.asq_mutex);

	 
	rtnl_lock();
	i40e_clear_interrupt_scheme(pf);
	for (i = 0; i < pf->num_alloc_vsi; i++) {
		if (pf->vsi[i]) {
			if (!test_bit(__I40E_RECOVERY_MODE, pf->state))
				i40e_vsi_clear_rings(pf->vsi[i]);
			i40e_vsi_clear(pf->vsi[i]);
			pf->vsi[i] = NULL;
		}
	}
	rtnl_unlock();

	for (i = 0; i < I40E_MAX_VEB; i++) {
		kfree(pf->veb[i]);
		pf->veb[i] = NULL;
	}

	kfree(pf->qp_pile);
	kfree(pf->vsi);

	iounmap(hw->hw_addr);
	kfree(pf);
	pci_release_mem_regions(pdev);

	pci_disable_device(pdev);
}

 
static pci_ers_result_t i40e_pci_error_detected(struct pci_dev *pdev,
						pci_channel_state_t error)
{
	struct i40e_pf *pf = pci_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s: error %d\n", __func__, error);

	if (!pf) {
		dev_info(&pdev->dev,
			 "Cannot recover - error happened during device probe\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	 
	if (!test_bit(__I40E_SUSPENDED, pf->state))
		i40e_prep_for_reset(pf);

	 
	return PCI_ERS_RESULT_NEED_RESET;
}

 
static pci_ers_result_t i40e_pci_error_slot_reset(struct pci_dev *pdev)
{
	struct i40e_pf *pf = pci_get_drvdata(pdev);
	pci_ers_result_t result;
	u32 reg;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (pci_enable_device_mem(pdev)) {
		dev_info(&pdev->dev,
			 "Cannot re-enable PCI device after reset.\n");
		result = PCI_ERS_RESULT_DISCONNECT;
	} else {
		pci_set_master(pdev);
		pci_restore_state(pdev);
		pci_save_state(pdev);
		pci_wake_from_d3(pdev, false);

		reg = rd32(&pf->hw, I40E_GLGEN_RTRIG);
		if (reg == 0)
			result = PCI_ERS_RESULT_RECOVERED;
		else
			result = PCI_ERS_RESULT_DISCONNECT;
	}

	return result;
}

 
static void i40e_pci_error_reset_prepare(struct pci_dev *pdev)
{
	struct i40e_pf *pf = pci_get_drvdata(pdev);

	i40e_prep_for_reset(pf);
}

 
static void i40e_pci_error_reset_done(struct pci_dev *pdev)
{
	struct i40e_pf *pf = pci_get_drvdata(pdev);

	if (test_bit(__I40E_IN_REMOVE, pf->state))
		return;

	i40e_reset_and_rebuild(pf, false, false);
#ifdef CONFIG_PCI_IOV
	i40e_restore_all_vfs_msi_state(pdev);
#endif  
}

 
static void i40e_pci_error_resume(struct pci_dev *pdev)
{
	struct i40e_pf *pf = pci_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (test_bit(__I40E_SUSPENDED, pf->state))
		return;

	i40e_handle_reset_warning(pf, false);
}

 
static void i40e_enable_mc_magic_wake(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u8 mac_addr[6];
	u16 flags = 0;
	int ret;

	 
	if (pf->vsi[pf->lan_vsi] && pf->vsi[pf->lan_vsi]->netdev) {
		ether_addr_copy(mac_addr,
				pf->vsi[pf->lan_vsi]->netdev->dev_addr);
	} else {
		dev_err(&pf->pdev->dev,
			"Failed to retrieve MAC address; using default\n");
		ether_addr_copy(mac_addr, hw->mac.addr);
	}

	 
	flags = I40E_AQC_WRITE_TYPE_LAA_WOL;

	if (hw->func_caps.flex10_enable && hw->partition_id != 1)
		flags = I40E_AQC_WRITE_TYPE_LAA_ONLY;

	ret = i40e_aq_mac_address_write(hw, flags, mac_addr, NULL);
	if (ret) {
		dev_err(&pf->pdev->dev,
			"Failed to update MAC address registers; cannot enable Multicast Magic packet wake up");
		return;
	}

	flags = I40E_AQC_MC_MAG_EN
			| I40E_AQC_WOL_PRESERVE_ON_PFR
			| I40E_AQC_WRITE_TYPE_UPDATE_MC_MAG;
	ret = i40e_aq_mac_address_write(hw, flags, mac_addr, NULL);
	if (ret)
		dev_err(&pf->pdev->dev,
			"Failed to enable Multicast Magic Packet wake up\n");
}

 
static void i40e_shutdown(struct pci_dev *pdev)
{
	struct i40e_pf *pf = pci_get_drvdata(pdev);
	struct i40e_hw *hw = &pf->hw;

	set_bit(__I40E_SUSPENDED, pf->state);
	set_bit(__I40E_DOWN, pf->state);

	del_timer_sync(&pf->service_timer);
	cancel_work_sync(&pf->service_task);
	i40e_cloud_filter_exit(pf);
	i40e_fdir_teardown(pf);

	 
	i40e_notify_client_of_netdev_close(pf->vsi[pf->lan_vsi], false);

	if (pf->wol_en && (pf->hw_features & I40E_HW_WOL_MC_MAGIC_PKT_WAKE))
		i40e_enable_mc_magic_wake(pf);

	i40e_prep_for_reset(pf);

	wr32(hw, I40E_PFPM_APM,
	     (pf->wol_en ? I40E_PFPM_APM_APME_MASK : 0));
	wr32(hw, I40E_PFPM_WUFC,
	     (pf->wol_en ? I40E_PFPM_WUFC_MAG_MASK : 0));

	 
	if (test_bit(__I40E_RECOVERY_MODE, pf->state) &&
	    !(pf->flags & I40E_FLAG_MSIX_ENABLED))
		free_irq(pf->pdev->irq, pf);

	 
	rtnl_lock();
	i40e_clear_interrupt_scheme(pf);
	rtnl_unlock();

	if (system_state == SYSTEM_POWER_OFF) {
		pci_wake_from_d3(pdev, pf->wol_en);
		pci_set_power_state(pdev, PCI_D3hot);
	}
}

 
static int __maybe_unused i40e_suspend(struct device *dev)
{
	struct i40e_pf *pf = dev_get_drvdata(dev);
	struct i40e_hw *hw = &pf->hw;

	 
	if (test_and_set_bit(__I40E_SUSPENDED, pf->state))
		return 0;

	set_bit(__I40E_DOWN, pf->state);

	 
	del_timer_sync(&pf->service_timer);
	cancel_work_sync(&pf->service_task);

	 
	i40e_notify_client_of_netdev_close(pf->vsi[pf->lan_vsi], false);

	if (pf->wol_en && (pf->hw_features & I40E_HW_WOL_MC_MAGIC_PKT_WAKE))
		i40e_enable_mc_magic_wake(pf);

	 
	rtnl_lock();

	i40e_prep_for_reset(pf);

	wr32(hw, I40E_PFPM_APM, (pf->wol_en ? I40E_PFPM_APM_APME_MASK : 0));
	wr32(hw, I40E_PFPM_WUFC, (pf->wol_en ? I40E_PFPM_WUFC_MAG_MASK : 0));

	 
	i40e_clear_interrupt_scheme(pf);

	rtnl_unlock();

	return 0;
}

 
static int __maybe_unused i40e_resume(struct device *dev)
{
	struct i40e_pf *pf = dev_get_drvdata(dev);
	int err;

	 
	if (!test_bit(__I40E_SUSPENDED, pf->state))
		return 0;

	 
	rtnl_lock();

	 
	err = i40e_restore_interrupt_scheme(pf);
	if (err) {
		dev_err(dev, "Cannot restore interrupt scheme: %d\n",
			err);
	}

	clear_bit(__I40E_DOWN, pf->state);
	i40e_reset_and_rebuild(pf, false, true);

	rtnl_unlock();

	 
	clear_bit(__I40E_SUSPENDED, pf->state);

	 
	mod_timer(&pf->service_timer,
		  round_jiffies(jiffies + pf->service_timer_period));

	return 0;
}

static const struct pci_error_handlers i40e_err_handler = {
	.error_detected = i40e_pci_error_detected,
	.slot_reset = i40e_pci_error_slot_reset,
	.reset_prepare = i40e_pci_error_reset_prepare,
	.reset_done = i40e_pci_error_reset_done,
	.resume = i40e_pci_error_resume,
};

static SIMPLE_DEV_PM_OPS(i40e_pm_ops, i40e_suspend, i40e_resume);

static struct pci_driver i40e_driver = {
	.name     = i40e_driver_name,
	.id_table = i40e_pci_tbl,
	.probe    = i40e_probe,
	.remove   = i40e_remove,
	.driver   = {
		.pm = &i40e_pm_ops,
	},
	.shutdown = i40e_shutdown,
	.err_handler = &i40e_err_handler,
	.sriov_configure = i40e_pci_sriov_configure,
};

 
static int __init i40e_init_module(void)
{
	int err;

	pr_info("%s: %s\n", i40e_driver_name, i40e_driver_string);
	pr_info("%s: %s\n", i40e_driver_name, i40e_copyright);

	 
	i40e_wq = alloc_workqueue("%s", WQ_MEM_RECLAIM, 0, i40e_driver_name);
	if (!i40e_wq) {
		pr_err("%s: Failed to create workqueue\n", i40e_driver_name);
		return -ENOMEM;
	}

	i40e_dbg_init();
	err = pci_register_driver(&i40e_driver);
	if (err) {
		destroy_workqueue(i40e_wq);
		i40e_dbg_exit();
		return err;
	}

	return 0;
}
module_init(i40e_init_module);

 
static void __exit i40e_exit_module(void)
{
	pci_unregister_driver(&i40e_driver);
	destroy_workqueue(i40e_wq);
	ida_destroy(&i40e_client_ida);
	i40e_dbg_exit();
}
module_exit(i40e_exit_module);
