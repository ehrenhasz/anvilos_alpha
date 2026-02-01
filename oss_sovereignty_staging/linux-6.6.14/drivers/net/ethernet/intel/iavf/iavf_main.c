
 

#include "iavf.h"
#include "iavf_prototype.h"
#include "iavf_client.h"
 
#define CREATE_TRACE_POINTS
#include "iavf_trace.h"

static int iavf_setup_all_tx_resources(struct iavf_adapter *adapter);
static int iavf_setup_all_rx_resources(struct iavf_adapter *adapter);
static int iavf_close(struct net_device *netdev);
static void iavf_init_get_resources(struct iavf_adapter *adapter);
static int iavf_check_reset_complete(struct iavf_hw *hw);

char iavf_driver_name[] = "iavf";
static const char iavf_driver_string[] =
	"Intel(R) Ethernet Adaptive Virtual Function Network Driver";

static const char iavf_copyright[] =
	"Copyright (c) 2013 - 2018 Intel Corporation.";

 
static const struct pci_device_id iavf_pci_tbl[] = {
	{PCI_VDEVICE(INTEL, IAVF_DEV_ID_VF), 0},
	{PCI_VDEVICE(INTEL, IAVF_DEV_ID_VF_HV), 0},
	{PCI_VDEVICE(INTEL, IAVF_DEV_ID_X722_VF), 0},
	{PCI_VDEVICE(INTEL, IAVF_DEV_ID_ADAPTIVE_VF), 0},
	 
	{0, }
};

MODULE_DEVICE_TABLE(pci, iavf_pci_tbl);

MODULE_ALIAS("i40evf");
MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) Ethernet Adaptive Virtual Function Network Driver");
MODULE_LICENSE("GPL v2");

static const struct net_device_ops iavf_netdev_ops;

int iavf_status_to_errno(enum iavf_status status)
{
	switch (status) {
	case IAVF_SUCCESS:
		return 0;
	case IAVF_ERR_PARAM:
	case IAVF_ERR_MAC_TYPE:
	case IAVF_ERR_INVALID_MAC_ADDR:
	case IAVF_ERR_INVALID_LINK_SETTINGS:
	case IAVF_ERR_INVALID_PD_ID:
	case IAVF_ERR_INVALID_QP_ID:
	case IAVF_ERR_INVALID_CQ_ID:
	case IAVF_ERR_INVALID_CEQ_ID:
	case IAVF_ERR_INVALID_AEQ_ID:
	case IAVF_ERR_INVALID_SIZE:
	case IAVF_ERR_INVALID_ARP_INDEX:
	case IAVF_ERR_INVALID_FPM_FUNC_ID:
	case IAVF_ERR_QP_INVALID_MSG_SIZE:
	case IAVF_ERR_INVALID_FRAG_COUNT:
	case IAVF_ERR_INVALID_ALIGNMENT:
	case IAVF_ERR_INVALID_PUSH_PAGE_INDEX:
	case IAVF_ERR_INVALID_IMM_DATA_SIZE:
	case IAVF_ERR_INVALID_VF_ID:
	case IAVF_ERR_INVALID_HMCFN_ID:
	case IAVF_ERR_INVALID_PBLE_INDEX:
	case IAVF_ERR_INVALID_SD_INDEX:
	case IAVF_ERR_INVALID_PAGE_DESC_INDEX:
	case IAVF_ERR_INVALID_SD_TYPE:
	case IAVF_ERR_INVALID_HMC_OBJ_INDEX:
	case IAVF_ERR_INVALID_HMC_OBJ_COUNT:
	case IAVF_ERR_INVALID_SRQ_ARM_LIMIT:
		return -EINVAL;
	case IAVF_ERR_NVM:
	case IAVF_ERR_NVM_CHECKSUM:
	case IAVF_ERR_PHY:
	case IAVF_ERR_CONFIG:
	case IAVF_ERR_UNKNOWN_PHY:
	case IAVF_ERR_LINK_SETUP:
	case IAVF_ERR_ADAPTER_STOPPED:
	case IAVF_ERR_PRIMARY_REQUESTS_PENDING:
	case IAVF_ERR_AUTONEG_NOT_COMPLETE:
	case IAVF_ERR_RESET_FAILED:
	case IAVF_ERR_BAD_PTR:
	case IAVF_ERR_SWFW_SYNC:
	case IAVF_ERR_QP_TOOMANY_WRS_POSTED:
	case IAVF_ERR_QUEUE_EMPTY:
	case IAVF_ERR_FLUSHED_QUEUE:
	case IAVF_ERR_OPCODE_MISMATCH:
	case IAVF_ERR_CQP_COMPL_ERROR:
	case IAVF_ERR_BACKING_PAGE_ERROR:
	case IAVF_ERR_NO_PBLCHUNKS_AVAILABLE:
	case IAVF_ERR_MEMCPY_FAILED:
	case IAVF_ERR_SRQ_ENABLED:
	case IAVF_ERR_ADMIN_QUEUE_ERROR:
	case IAVF_ERR_ADMIN_QUEUE_FULL:
	case IAVF_ERR_BAD_RDMA_CQE:
	case IAVF_ERR_NVM_BLANK_MODE:
	case IAVF_ERR_PE_DOORBELL_NOT_ENABLED:
	case IAVF_ERR_DIAG_TEST_FAILED:
	case IAVF_ERR_FIRMWARE_API_VERSION:
	case IAVF_ERR_ADMIN_QUEUE_CRITICAL_ERROR:
		return -EIO;
	case IAVF_ERR_DEVICE_NOT_SUPPORTED:
		return -ENODEV;
	case IAVF_ERR_NO_AVAILABLE_VSI:
	case IAVF_ERR_RING_FULL:
		return -ENOSPC;
	case IAVF_ERR_NO_MEMORY:
		return -ENOMEM;
	case IAVF_ERR_TIMEOUT:
	case IAVF_ERR_ADMIN_QUEUE_TIMEOUT:
		return -ETIMEDOUT;
	case IAVF_ERR_NOT_IMPLEMENTED:
	case IAVF_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	case IAVF_ERR_ADMIN_QUEUE_NO_WORK:
		return -EALREADY;
	case IAVF_ERR_NOT_READY:
		return -EBUSY;
	case IAVF_ERR_BUF_TOO_SHORT:
		return -EMSGSIZE;
	}

	return -EIO;
}

int virtchnl_status_to_errno(enum virtchnl_status_code v_status)
{
	switch (v_status) {
	case VIRTCHNL_STATUS_SUCCESS:
		return 0;
	case VIRTCHNL_STATUS_ERR_PARAM:
	case VIRTCHNL_STATUS_ERR_INVALID_VF_ID:
		return -EINVAL;
	case VIRTCHNL_STATUS_ERR_NO_MEMORY:
		return -ENOMEM;
	case VIRTCHNL_STATUS_ERR_OPCODE_MISMATCH:
	case VIRTCHNL_STATUS_ERR_CQP_COMPL_ERROR:
	case VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR:
		return -EIO;
	case VIRTCHNL_STATUS_ERR_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	}

	return -EIO;
}

 
static struct iavf_adapter *iavf_pdev_to_adapter(struct pci_dev *pdev)
{
	return netdev_priv(pci_get_drvdata(pdev));
}

 
static bool iavf_is_reset_in_progress(struct iavf_adapter *adapter)
{
	if (adapter->state == __IAVF_RESETTING ||
	    adapter->flags & (IAVF_FLAG_RESET_PENDING |
			      IAVF_FLAG_RESET_NEEDED))
		return true;

	return false;
}

 
int iavf_wait_for_reset(struct iavf_adapter *adapter)
{
	int ret = wait_event_interruptible_timeout(adapter->reset_waitqueue,
					!iavf_is_reset_in_progress(adapter),
					msecs_to_jiffies(5000));

	 
	if (ret > 0)
		return 0;
	else if (ret < 0)
		return -EINTR;
	else
		return -EBUSY;
}

 
enum iavf_status iavf_allocate_dma_mem_d(struct iavf_hw *hw,
					 struct iavf_dma_mem *mem,
					 u64 size, u32 alignment)
{
	struct iavf_adapter *adapter = (struct iavf_adapter *)hw->back;

	if (!mem)
		return IAVF_ERR_PARAM;

	mem->size = ALIGN(size, alignment);
	mem->va = dma_alloc_coherent(&adapter->pdev->dev, mem->size,
				     (dma_addr_t *)&mem->pa, GFP_KERNEL);
	if (mem->va)
		return 0;
	else
		return IAVF_ERR_NO_MEMORY;
}

 
enum iavf_status iavf_free_dma_mem(struct iavf_hw *hw, struct iavf_dma_mem *mem)
{
	struct iavf_adapter *adapter = (struct iavf_adapter *)hw->back;

	if (!mem || !mem->va)
		return IAVF_ERR_PARAM;
	dma_free_coherent(&adapter->pdev->dev, mem->size,
			  mem->va, (dma_addr_t)mem->pa);
	return 0;
}

 
enum iavf_status iavf_allocate_virt_mem(struct iavf_hw *hw,
					struct iavf_virt_mem *mem, u32 size)
{
	if (!mem)
		return IAVF_ERR_PARAM;

	mem->size = size;
	mem->va = kzalloc(size, GFP_KERNEL);

	if (mem->va)
		return 0;
	else
		return IAVF_ERR_NO_MEMORY;
}

 
void iavf_free_virt_mem(struct iavf_hw *hw, struct iavf_virt_mem *mem)
{
	kfree(mem->va);
}

 
void iavf_schedule_reset(struct iavf_adapter *adapter, u64 flags)
{
	if (!test_bit(__IAVF_IN_REMOVE_TASK, &adapter->crit_section) &&
	    !(adapter->flags &
	    (IAVF_FLAG_RESET_PENDING | IAVF_FLAG_RESET_NEEDED))) {
		adapter->flags |= flags;
		queue_work(adapter->wq, &adapter->reset_task);
	}
}

 
void iavf_schedule_aq_request(struct iavf_adapter *adapter, u64 flags)
{
	adapter->aq_required |= flags;
	mod_delayed_work(adapter->wq, &adapter->watchdog_task, 0);
}

 
static void iavf_tx_timeout(struct net_device *netdev, unsigned int txqueue)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);

	adapter->tx_timeout_count++;
	iavf_schedule_reset(adapter, IAVF_FLAG_RESET_NEEDED);
}

 
static void iavf_misc_irq_disable(struct iavf_adapter *adapter)
{
	struct iavf_hw *hw = &adapter->hw;

	if (!adapter->msix_entries)
		return;

	wr32(hw, IAVF_VFINT_DYN_CTL01, 0);

	iavf_flush(hw);

	synchronize_irq(adapter->msix_entries[0].vector);
}

 
static void iavf_misc_irq_enable(struct iavf_adapter *adapter)
{
	struct iavf_hw *hw = &adapter->hw;

	wr32(hw, IAVF_VFINT_DYN_CTL01, IAVF_VFINT_DYN_CTL01_INTENA_MASK |
				       IAVF_VFINT_DYN_CTL01_ITR_INDX_MASK);
	wr32(hw, IAVF_VFINT_ICR0_ENA1, IAVF_VFINT_ICR0_ENA1_ADMINQ_MASK);

	iavf_flush(hw);
}

 
static void iavf_irq_disable(struct iavf_adapter *adapter)
{
	int i;
	struct iavf_hw *hw = &adapter->hw;

	if (!adapter->msix_entries)
		return;

	for (i = 1; i < adapter->num_msix_vectors; i++) {
		wr32(hw, IAVF_VFINT_DYN_CTLN1(i - 1), 0);
		synchronize_irq(adapter->msix_entries[i].vector);
	}
	iavf_flush(hw);
}

 
static void iavf_irq_enable_queues(struct iavf_adapter *adapter)
{
	struct iavf_hw *hw = &adapter->hw;
	int i;

	for (i = 1; i < adapter->num_msix_vectors; i++) {
		wr32(hw, IAVF_VFINT_DYN_CTLN1(i - 1),
		     IAVF_VFINT_DYN_CTLN1_INTENA_MASK |
		     IAVF_VFINT_DYN_CTLN1_ITR_INDX_MASK);
	}
}

 
void iavf_irq_enable(struct iavf_adapter *adapter, bool flush)
{
	struct iavf_hw *hw = &adapter->hw;

	iavf_misc_irq_enable(adapter);
	iavf_irq_enable_queues(adapter);

	if (flush)
		iavf_flush(hw);
}

 
static irqreturn_t iavf_msix_aq(int irq, void *data)
{
	struct net_device *netdev = data;
	struct iavf_adapter *adapter = netdev_priv(netdev);
	struct iavf_hw *hw = &adapter->hw;

	 
	rd32(hw, IAVF_VFINT_ICR01);
	rd32(hw, IAVF_VFINT_ICR0_ENA1);

	if (adapter->state != __IAVF_REMOVE)
		 
		queue_work(adapter->wq, &adapter->adminq_task);

	return IRQ_HANDLED;
}

 
static irqreturn_t iavf_msix_clean_rings(int irq, void *data)
{
	struct iavf_q_vector *q_vector = data;

	if (!q_vector->tx.ring && !q_vector->rx.ring)
		return IRQ_HANDLED;

	napi_schedule_irqoff(&q_vector->napi);

	return IRQ_HANDLED;
}

 
static void
iavf_map_vector_to_rxq(struct iavf_adapter *adapter, int v_idx, int r_idx)
{
	struct iavf_q_vector *q_vector = &adapter->q_vectors[v_idx];
	struct iavf_ring *rx_ring = &adapter->rx_rings[r_idx];
	struct iavf_hw *hw = &adapter->hw;

	rx_ring->q_vector = q_vector;
	rx_ring->next = q_vector->rx.ring;
	rx_ring->vsi = &adapter->vsi;
	q_vector->rx.ring = rx_ring;
	q_vector->rx.count++;
	q_vector->rx.next_update = jiffies + 1;
	q_vector->rx.target_itr = ITR_TO_REG(rx_ring->itr_setting);
	q_vector->ring_mask |= BIT(r_idx);
	wr32(hw, IAVF_VFINT_ITRN1(IAVF_RX_ITR, q_vector->reg_idx),
	     q_vector->rx.current_itr >> 1);
	q_vector->rx.current_itr = q_vector->rx.target_itr;
}

 
static void
iavf_map_vector_to_txq(struct iavf_adapter *adapter, int v_idx, int t_idx)
{
	struct iavf_q_vector *q_vector = &adapter->q_vectors[v_idx];
	struct iavf_ring *tx_ring = &adapter->tx_rings[t_idx];
	struct iavf_hw *hw = &adapter->hw;

	tx_ring->q_vector = q_vector;
	tx_ring->next = q_vector->tx.ring;
	tx_ring->vsi = &adapter->vsi;
	q_vector->tx.ring = tx_ring;
	q_vector->tx.count++;
	q_vector->tx.next_update = jiffies + 1;
	q_vector->tx.target_itr = ITR_TO_REG(tx_ring->itr_setting);
	q_vector->num_ringpairs++;
	wr32(hw, IAVF_VFINT_ITRN1(IAVF_TX_ITR, q_vector->reg_idx),
	     q_vector->tx.target_itr >> 1);
	q_vector->tx.current_itr = q_vector->tx.target_itr;
}

 
static void iavf_map_rings_to_vectors(struct iavf_adapter *adapter)
{
	int rings_remaining = adapter->num_active_queues;
	int ridx = 0, vidx = 0;
	int q_vectors;

	q_vectors = adapter->num_msix_vectors - NONQ_VECS;

	for (; ridx < rings_remaining; ridx++) {
		iavf_map_vector_to_rxq(adapter, vidx, ridx);
		iavf_map_vector_to_txq(adapter, vidx, ridx);

		 
		if (++vidx >= q_vectors)
			vidx = 0;
	}

	adapter->aq_required |= IAVF_FLAG_AQ_MAP_VECTORS;
}

 
static void iavf_irq_affinity_notify(struct irq_affinity_notify *notify,
				     const cpumask_t *mask)
{
	struct iavf_q_vector *q_vector =
		container_of(notify, struct iavf_q_vector, affinity_notify);

	cpumask_copy(&q_vector->affinity_mask, mask);
}

 
static void iavf_irq_affinity_release(struct kref *ref) {}

 
static int
iavf_request_traffic_irqs(struct iavf_adapter *adapter, char *basename)
{
	unsigned int vector, q_vectors;
	unsigned int rx_int_idx = 0, tx_int_idx = 0;
	int irq_num, err;
	int cpu;

	iavf_irq_disable(adapter);
	 
	q_vectors = adapter->num_msix_vectors - NONQ_VECS;

	for (vector = 0; vector < q_vectors; vector++) {
		struct iavf_q_vector *q_vector = &adapter->q_vectors[vector];

		irq_num = adapter->msix_entries[vector + NONQ_VECS].vector;

		if (q_vector->tx.ring && q_vector->rx.ring) {
			snprintf(q_vector->name, sizeof(q_vector->name),
				 "iavf-%s-TxRx-%u", basename, rx_int_idx++);
			tx_int_idx++;
		} else if (q_vector->rx.ring) {
			snprintf(q_vector->name, sizeof(q_vector->name),
				 "iavf-%s-rx-%u", basename, rx_int_idx++);
		} else if (q_vector->tx.ring) {
			snprintf(q_vector->name, sizeof(q_vector->name),
				 "iavf-%s-tx-%u", basename, tx_int_idx++);
		} else {
			 
			continue;
		}
		err = request_irq(irq_num,
				  iavf_msix_clean_rings,
				  0,
				  q_vector->name,
				  q_vector);
		if (err) {
			dev_info(&adapter->pdev->dev,
				 "Request_irq failed, error: %d\n", err);
			goto free_queue_irqs;
		}
		 
		q_vector->affinity_notify.notify = iavf_irq_affinity_notify;
		q_vector->affinity_notify.release =
						   iavf_irq_affinity_release;
		irq_set_affinity_notifier(irq_num, &q_vector->affinity_notify);
		 
		cpu = cpumask_local_spread(q_vector->v_idx, -1);
		irq_update_affinity_hint(irq_num, get_cpu_mask(cpu));
	}

	return 0;

free_queue_irqs:
	while (vector) {
		vector--;
		irq_num = adapter->msix_entries[vector + NONQ_VECS].vector;
		irq_set_affinity_notifier(irq_num, NULL);
		irq_update_affinity_hint(irq_num, NULL);
		free_irq(irq_num, &adapter->q_vectors[vector]);
	}
	return err;
}

 
static int iavf_request_misc_irq(struct iavf_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int err;

	snprintf(adapter->misc_vector_name,
		 sizeof(adapter->misc_vector_name) - 1, "iavf-%s:mbx",
		 dev_name(&adapter->pdev->dev));
	err = request_irq(adapter->msix_entries[0].vector,
			  &iavf_msix_aq, 0,
			  adapter->misc_vector_name, netdev);
	if (err) {
		dev_err(&adapter->pdev->dev,
			"request_irq for %s failed: %d\n",
			adapter->misc_vector_name, err);
		free_irq(adapter->msix_entries[0].vector, netdev);
	}
	return err;
}

 
static void iavf_free_traffic_irqs(struct iavf_adapter *adapter)
{
	int vector, irq_num, q_vectors;

	if (!adapter->msix_entries)
		return;

	q_vectors = adapter->num_msix_vectors - NONQ_VECS;

	for (vector = 0; vector < q_vectors; vector++) {
		irq_num = adapter->msix_entries[vector + NONQ_VECS].vector;
		irq_set_affinity_notifier(irq_num, NULL);
		irq_update_affinity_hint(irq_num, NULL);
		free_irq(irq_num, &adapter->q_vectors[vector]);
	}
}

 
static void iavf_free_misc_irq(struct iavf_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	if (!adapter->msix_entries)
		return;

	free_irq(adapter->msix_entries[0].vector, netdev);
}

 
static void iavf_configure_tx(struct iavf_adapter *adapter)
{
	struct iavf_hw *hw = &adapter->hw;
	int i;

	for (i = 0; i < adapter->num_active_queues; i++)
		adapter->tx_rings[i].tail = hw->hw_addr + IAVF_QTX_TAIL1(i);
}

 
static void iavf_configure_rx(struct iavf_adapter *adapter)
{
	unsigned int rx_buf_len = IAVF_RXBUFFER_2048;
	struct iavf_hw *hw = &adapter->hw;
	int i;

	 
#if (PAGE_SIZE < 8192)
	if (!(adapter->flags & IAVF_FLAG_LEGACY_RX)) {
		struct net_device *netdev = adapter->netdev;

		 
		rx_buf_len = IAVF_RXBUFFER_3072;

		 
		if (!IAVF_2K_TOO_SMALL_WITH_PADDING &&
		    (netdev->mtu <= ETH_DATA_LEN))
			rx_buf_len = IAVF_RXBUFFER_1536 - NET_IP_ALIGN;
	}
#endif

	for (i = 0; i < adapter->num_active_queues; i++) {
		adapter->rx_rings[i].tail = hw->hw_addr + IAVF_QRX_TAIL1(i);
		adapter->rx_rings[i].rx_buf_len = rx_buf_len;

		if (adapter->flags & IAVF_FLAG_LEGACY_RX)
			clear_ring_build_skb_enabled(&adapter->rx_rings[i]);
		else
			set_ring_build_skb_enabled(&adapter->rx_rings[i]);
	}
}

 
static struct
iavf_vlan_filter *iavf_find_vlan(struct iavf_adapter *adapter,
				 struct iavf_vlan vlan)
{
	struct iavf_vlan_filter *f;

	list_for_each_entry(f, &adapter->vlan_filter_list, list) {
		if (f->vlan.vid == vlan.vid &&
		    f->vlan.tpid == vlan.tpid)
			return f;
	}

	return NULL;
}

 
static struct
iavf_vlan_filter *iavf_add_vlan(struct iavf_adapter *adapter,
				struct iavf_vlan vlan)
{
	struct iavf_vlan_filter *f = NULL;

	spin_lock_bh(&adapter->mac_vlan_list_lock);

	f = iavf_find_vlan(adapter, vlan);
	if (!f) {
		f = kzalloc(sizeof(*f), GFP_ATOMIC);
		if (!f)
			goto clearout;

		f->vlan = vlan;

		list_add_tail(&f->list, &adapter->vlan_filter_list);
		f->state = IAVF_VLAN_ADD;
		adapter->num_vlan_filters++;
		iavf_schedule_aq_request(adapter, IAVF_FLAG_AQ_ADD_VLAN_FILTER);
	}

clearout:
	spin_unlock_bh(&adapter->mac_vlan_list_lock);
	return f;
}

 
static void iavf_del_vlan(struct iavf_adapter *adapter, struct iavf_vlan vlan)
{
	struct iavf_vlan_filter *f;

	spin_lock_bh(&adapter->mac_vlan_list_lock);

	f = iavf_find_vlan(adapter, vlan);
	if (f) {
		f->state = IAVF_VLAN_REMOVE;
		iavf_schedule_aq_request(adapter, IAVF_FLAG_AQ_DEL_VLAN_FILTER);
	}

	spin_unlock_bh(&adapter->mac_vlan_list_lock);
}

 
static void iavf_restore_filters(struct iavf_adapter *adapter)
{
	struct iavf_vlan_filter *f;

	 
	spin_lock_bh(&adapter->mac_vlan_list_lock);

	list_for_each_entry(f, &adapter->vlan_filter_list, list) {
		if (f->state == IAVF_VLAN_INACTIVE)
			f->state = IAVF_VLAN_ADD;
	}

	spin_unlock_bh(&adapter->mac_vlan_list_lock);
	adapter->aq_required |= IAVF_FLAG_AQ_ADD_VLAN_FILTER;
}

 
u16 iavf_get_num_vlans_added(struct iavf_adapter *adapter)
{
	return adapter->num_vlan_filters;
}

 
static u16 iavf_get_max_vlans_allowed(struct iavf_adapter *adapter)
{
	 
	if (VLAN_ALLOWED(adapter))
		return VLAN_N_VID;
	else if (VLAN_V2_ALLOWED(adapter))
		return adapter->vlan_v2_caps.filtering.max_filters;

	return 0;
}

 
static bool iavf_max_vlans_added(struct iavf_adapter *adapter)
{
	if (iavf_get_num_vlans_added(adapter) <
	    iavf_get_max_vlans_allowed(adapter))
		return false;

	return true;
}

 
static int iavf_vlan_rx_add_vid(struct net_device *netdev,
				__always_unused __be16 proto, u16 vid)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);

	 
	if (!vid)
		return 0;

	if (!VLAN_FILTERING_ALLOWED(adapter))
		return -EIO;

	if (iavf_max_vlans_added(adapter)) {
		netdev_err(netdev, "Max allowed VLAN filters %u. Remove existing VLANs or disable filtering via Ethtool if supported.\n",
			   iavf_get_max_vlans_allowed(adapter));
		return -EIO;
	}

	if (!iavf_add_vlan(adapter, IAVF_VLAN(vid, be16_to_cpu(proto))))
		return -ENOMEM;

	return 0;
}

 
static int iavf_vlan_rx_kill_vid(struct net_device *netdev,
				 __always_unused __be16 proto, u16 vid)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);

	 
	if (!vid)
		return 0;

	iavf_del_vlan(adapter, IAVF_VLAN(vid, be16_to_cpu(proto)));
	return 0;
}

 
static struct
iavf_mac_filter *iavf_find_filter(struct iavf_adapter *adapter,
				  const u8 *macaddr)
{
	struct iavf_mac_filter *f;

	if (!macaddr)
		return NULL;

	list_for_each_entry(f, &adapter->mac_filter_list, list) {
		if (ether_addr_equal(macaddr, f->macaddr))
			return f;
	}
	return NULL;
}

 
struct iavf_mac_filter *iavf_add_filter(struct iavf_adapter *adapter,
					const u8 *macaddr)
{
	struct iavf_mac_filter *f;

	if (!macaddr)
		return NULL;

	f = iavf_find_filter(adapter, macaddr);
	if (!f) {
		f = kzalloc(sizeof(*f), GFP_ATOMIC);
		if (!f)
			return f;

		ether_addr_copy(f->macaddr, macaddr);

		list_add_tail(&f->list, &adapter->mac_filter_list);
		f->add = true;
		f->add_handled = false;
		f->is_new_mac = true;
		f->is_primary = ether_addr_equal(macaddr, adapter->hw.mac.addr);
		adapter->aq_required |= IAVF_FLAG_AQ_ADD_MAC_FILTER;
	} else {
		f->remove = false;
	}

	return f;
}

 
static int iavf_replace_primary_mac(struct iavf_adapter *adapter,
				    const u8 *new_mac)
{
	struct iavf_hw *hw = &adapter->hw;
	struct iavf_mac_filter *new_f;
	struct iavf_mac_filter *old_f;

	spin_lock_bh(&adapter->mac_vlan_list_lock);

	new_f = iavf_add_filter(adapter, new_mac);
	if (!new_f) {
		spin_unlock_bh(&adapter->mac_vlan_list_lock);
		return -ENOMEM;
	}

	old_f = iavf_find_filter(adapter, hw->mac.addr);
	if (old_f) {
		old_f->is_primary = false;
		old_f->remove = true;
		adapter->aq_required |= IAVF_FLAG_AQ_DEL_MAC_FILTER;
	}
	 
	new_f->is_primary = true;
	new_f->add = true;
	adapter->aq_required |= IAVF_FLAG_AQ_ADD_MAC_FILTER;
	ether_addr_copy(hw->mac.addr, new_mac);

	spin_unlock_bh(&adapter->mac_vlan_list_lock);

	 
	mod_delayed_work(adapter->wq, &adapter->watchdog_task, 0);
	return 0;
}

 
static bool iavf_is_mac_set_handled(struct net_device *netdev,
				    const u8 *macaddr)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);
	struct iavf_mac_filter *f;
	bool ret = false;

	spin_lock_bh(&adapter->mac_vlan_list_lock);

	f = iavf_find_filter(adapter, macaddr);

	if (!f || (!f->add && f->add_handled))
		ret = true;

	spin_unlock_bh(&adapter->mac_vlan_list_lock);

	return ret;
}

 
static int iavf_set_mac(struct net_device *netdev, void *p)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);
	struct sockaddr *addr = p;
	int ret;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	ret = iavf_replace_primary_mac(adapter, addr->sa_data);

	if (ret)
		return ret;

	ret = wait_event_interruptible_timeout(adapter->vc_waitqueue,
					       iavf_is_mac_set_handled(netdev, addr->sa_data),
					       msecs_to_jiffies(2500));

	 
	if (ret < 0)
		return ret;

	if (!ret)
		return -EAGAIN;

	if (!ether_addr_equal(netdev->dev_addr, addr->sa_data))
		return -EACCES;

	return 0;
}

 
static int iavf_addr_sync(struct net_device *netdev, const u8 *addr)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);

	if (iavf_add_filter(adapter, addr))
		return 0;
	else
		return -ENOMEM;
}

 
static int iavf_addr_unsync(struct net_device *netdev, const u8 *addr)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);
	struct iavf_mac_filter *f;

	 
	if (ether_addr_equal(addr, netdev->dev_addr))
		return 0;

	f = iavf_find_filter(adapter, addr);
	if (f) {
		f->remove = true;
		adapter->aq_required |= IAVF_FLAG_AQ_DEL_MAC_FILTER;
	}
	return 0;
}

 
bool iavf_promiscuous_mode_changed(struct iavf_adapter *adapter)
{
	return (adapter->current_netdev_promisc_flags ^ adapter->netdev->flags) &
		(IFF_PROMISC | IFF_ALLMULTI);
}

 
static void iavf_set_rx_mode(struct net_device *netdev)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);

	spin_lock_bh(&adapter->mac_vlan_list_lock);
	__dev_uc_sync(netdev, iavf_addr_sync, iavf_addr_unsync);
	__dev_mc_sync(netdev, iavf_addr_sync, iavf_addr_unsync);
	spin_unlock_bh(&adapter->mac_vlan_list_lock);

	spin_lock_bh(&adapter->current_netdev_promisc_flags_lock);
	if (iavf_promiscuous_mode_changed(adapter))
		adapter->aq_required |= IAVF_FLAG_AQ_CONFIGURE_PROMISC_MODE;
	spin_unlock_bh(&adapter->current_netdev_promisc_flags_lock);
}

 
static void iavf_napi_enable_all(struct iavf_adapter *adapter)
{
	int q_idx;
	struct iavf_q_vector *q_vector;
	int q_vectors = adapter->num_msix_vectors - NONQ_VECS;

	for (q_idx = 0; q_idx < q_vectors; q_idx++) {
		struct napi_struct *napi;

		q_vector = &adapter->q_vectors[q_idx];
		napi = &q_vector->napi;
		napi_enable(napi);
	}
}

 
static void iavf_napi_disable_all(struct iavf_adapter *adapter)
{
	int q_idx;
	struct iavf_q_vector *q_vector;
	int q_vectors = adapter->num_msix_vectors - NONQ_VECS;

	for (q_idx = 0; q_idx < q_vectors; q_idx++) {
		q_vector = &adapter->q_vectors[q_idx];
		napi_disable(&q_vector->napi);
	}
}

 
static void iavf_configure(struct iavf_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int i;

	iavf_set_rx_mode(netdev);

	iavf_configure_tx(adapter);
	iavf_configure_rx(adapter);
	adapter->aq_required |= IAVF_FLAG_AQ_CONFIGURE_QUEUES;

	for (i = 0; i < adapter->num_active_queues; i++) {
		struct iavf_ring *ring = &adapter->rx_rings[i];

		iavf_alloc_rx_buffers(ring, IAVF_DESC_UNUSED(ring));
	}
}

 
static void iavf_up_complete(struct iavf_adapter *adapter)
{
	iavf_change_state(adapter, __IAVF_RUNNING);
	clear_bit(__IAVF_VSI_DOWN, adapter->vsi.state);

	iavf_napi_enable_all(adapter);

	adapter->aq_required |= IAVF_FLAG_AQ_ENABLE_QUEUES;
	if (CLIENT_ENABLED(adapter))
		adapter->flags |= IAVF_FLAG_CLIENT_NEEDS_OPEN;
	mod_delayed_work(adapter->wq, &adapter->watchdog_task, 0);
}

 
static void iavf_clear_mac_vlan_filters(struct iavf_adapter *adapter)
{
	struct iavf_vlan_filter *vlf, *vlftmp;
	struct iavf_mac_filter *f, *ftmp;

	spin_lock_bh(&adapter->mac_vlan_list_lock);
	 
	__dev_uc_unsync(adapter->netdev, NULL);
	__dev_mc_unsync(adapter->netdev, NULL);

	 
	list_for_each_entry_safe(f, ftmp, &adapter->mac_filter_list,
				 list) {
		if (f->add) {
			list_del(&f->list);
			kfree(f);
		} else {
			f->remove = true;
		}
	}

	 
	list_for_each_entry_safe(vlf, vlftmp, &adapter->vlan_filter_list,
				 list)
		vlf->state = IAVF_VLAN_DISABLE;

	spin_unlock_bh(&adapter->mac_vlan_list_lock);
}

 
static void iavf_clear_cloud_filters(struct iavf_adapter *adapter)
{
	struct iavf_cloud_filter *cf, *cftmp;

	 
	spin_lock_bh(&adapter->cloud_filter_list_lock);
	list_for_each_entry_safe(cf, cftmp, &adapter->cloud_filter_list,
				 list) {
		if (cf->add) {
			list_del(&cf->list);
			kfree(cf);
			adapter->num_cloud_filters--;
		} else {
			cf->del = true;
		}
	}
	spin_unlock_bh(&adapter->cloud_filter_list_lock);
}

 
static void iavf_clear_fdir_filters(struct iavf_adapter *adapter)
{
	struct iavf_fdir_fltr *fdir;

	 
	spin_lock_bh(&adapter->fdir_fltr_lock);
	list_for_each_entry(fdir, &adapter->fdir_list_head, list) {
		if (fdir->state == IAVF_FDIR_FLTR_ADD_REQUEST) {
			 
			fdir->state = IAVF_FDIR_FLTR_INACTIVE;
		} else if (fdir->state == IAVF_FDIR_FLTR_ADD_PENDING ||
			 fdir->state == IAVF_FDIR_FLTR_ACTIVE) {
			 
			fdir->state = IAVF_FDIR_FLTR_DIS_REQUEST;
		}
	}
	spin_unlock_bh(&adapter->fdir_fltr_lock);
}

 
static void iavf_clear_adv_rss_conf(struct iavf_adapter *adapter)
{
	struct iavf_adv_rss *rss, *rsstmp;

	 
	spin_lock_bh(&adapter->adv_rss_lock);
	list_for_each_entry_safe(rss, rsstmp, &adapter->adv_rss_list_head,
				 list) {
		if (rss->state == IAVF_ADV_RSS_ADD_REQUEST) {
			list_del(&rss->list);
			kfree(rss);
		} else {
			rss->state = IAVF_ADV_RSS_DEL_REQUEST;
		}
	}
	spin_unlock_bh(&adapter->adv_rss_lock);
}

 
void iavf_down(struct iavf_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	if (adapter->state <= __IAVF_DOWN_PENDING)
		return;

	netif_carrier_off(netdev);
	netif_tx_disable(netdev);
	adapter->link_up = false;
	iavf_napi_disable_all(adapter);
	iavf_irq_disable(adapter);

	iavf_clear_mac_vlan_filters(adapter);
	iavf_clear_cloud_filters(adapter);
	iavf_clear_fdir_filters(adapter);
	iavf_clear_adv_rss_conf(adapter);

	if (!(adapter->flags & IAVF_FLAG_PF_COMMS_FAILED) &&
	    !(test_bit(__IAVF_IN_REMOVE_TASK, &adapter->crit_section))) {
		 
		adapter->current_op = VIRTCHNL_OP_UNKNOWN;
		 
		if (!list_empty(&adapter->mac_filter_list))
			adapter->aq_required |= IAVF_FLAG_AQ_DEL_MAC_FILTER;
		if (!list_empty(&adapter->vlan_filter_list))
			adapter->aq_required |= IAVF_FLAG_AQ_DEL_VLAN_FILTER;
		if (!list_empty(&adapter->cloud_filter_list))
			adapter->aq_required |= IAVF_FLAG_AQ_DEL_CLOUD_FILTER;
		if (!list_empty(&adapter->fdir_list_head))
			adapter->aq_required |= IAVF_FLAG_AQ_DEL_FDIR_FILTER;
		if (!list_empty(&adapter->adv_rss_list_head))
			adapter->aq_required |= IAVF_FLAG_AQ_DEL_ADV_RSS_CFG;
	}

	adapter->aq_required |= IAVF_FLAG_AQ_DISABLE_QUEUES;
	mod_delayed_work(adapter->wq, &adapter->watchdog_task, 0);
}

 
static int
iavf_acquire_msix_vectors(struct iavf_adapter *adapter, int vectors)
{
	int err, vector_threshold;

	 
	vector_threshold = MIN_MSIX_COUNT;

	 
	err = pci_enable_msix_range(adapter->pdev, adapter->msix_entries,
				    vector_threshold, vectors);
	if (err < 0) {
		dev_err(&adapter->pdev->dev, "Unable to allocate MSI-X interrupts\n");
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
		return err;
	}

	 
	adapter->num_msix_vectors = err;
	return 0;
}

 
static void iavf_free_queues(struct iavf_adapter *adapter)
{
	if (!adapter->vsi_res)
		return;
	adapter->num_active_queues = 0;
	kfree(adapter->tx_rings);
	adapter->tx_rings = NULL;
	kfree(adapter->rx_rings);
	adapter->rx_rings = NULL;
}

 
void iavf_set_queue_vlan_tag_loc(struct iavf_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_active_queues; i++) {
		struct iavf_ring *tx_ring = &adapter->tx_rings[i];
		struct iavf_ring *rx_ring = &adapter->rx_rings[i];

		 
		tx_ring->flags &=
			~(IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1 |
			  IAVF_TXR_FLAGS_VLAN_TAG_LOC_L2TAG2);
		rx_ring->flags &=
			~(IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1 |
			  IAVF_RXR_FLAGS_VLAN_TAG_LOC_L2TAG2_2);

		if (VLAN_ALLOWED(adapter)) {
			tx_ring->flags |= IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1;
			rx_ring->flags |= IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1;
		} else if (VLAN_V2_ALLOWED(adapter)) {
			struct virtchnl_vlan_supported_caps *stripping_support;
			struct virtchnl_vlan_supported_caps *insertion_support;

			stripping_support =
				&adapter->vlan_v2_caps.offloads.stripping_support;
			insertion_support =
				&adapter->vlan_v2_caps.offloads.insertion_support;

			if (stripping_support->outer) {
				if (stripping_support->outer &
				    VIRTCHNL_VLAN_TAG_LOCATION_L2TAG1)
					rx_ring->flags |=
						IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1;
				else if (stripping_support->outer &
					 VIRTCHNL_VLAN_TAG_LOCATION_L2TAG2_2)
					rx_ring->flags |=
						IAVF_RXR_FLAGS_VLAN_TAG_LOC_L2TAG2_2;
			} else if (stripping_support->inner) {
				if (stripping_support->inner &
				    VIRTCHNL_VLAN_TAG_LOCATION_L2TAG1)
					rx_ring->flags |=
						IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1;
				else if (stripping_support->inner &
					 VIRTCHNL_VLAN_TAG_LOCATION_L2TAG2_2)
					rx_ring->flags |=
						IAVF_RXR_FLAGS_VLAN_TAG_LOC_L2TAG2_2;
			}

			if (insertion_support->outer) {
				if (insertion_support->outer &
				    VIRTCHNL_VLAN_TAG_LOCATION_L2TAG1)
					tx_ring->flags |=
						IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1;
				else if (insertion_support->outer &
					 VIRTCHNL_VLAN_TAG_LOCATION_L2TAG2)
					tx_ring->flags |=
						IAVF_TXR_FLAGS_VLAN_TAG_LOC_L2TAG2;
			} else if (insertion_support->inner) {
				if (insertion_support->inner &
				    VIRTCHNL_VLAN_TAG_LOCATION_L2TAG1)
					tx_ring->flags |=
						IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1;
				else if (insertion_support->inner &
					 VIRTCHNL_VLAN_TAG_LOCATION_L2TAG2)
					tx_ring->flags |=
						IAVF_TXR_FLAGS_VLAN_TAG_LOC_L2TAG2;
			}
		}
	}
}

 
static int iavf_alloc_queues(struct iavf_adapter *adapter)
{
	int i, num_active_queues;

	 
	if (adapter->num_req_queues)
		num_active_queues = adapter->num_req_queues;
	else if ((adapter->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADQ) &&
		 adapter->num_tc)
		num_active_queues = adapter->ch_config.total_qps;
	else
		num_active_queues = min_t(int,
					  adapter->vsi_res->num_queue_pairs,
					  (int)(num_online_cpus()));


	adapter->tx_rings = kcalloc(num_active_queues,
				    sizeof(struct iavf_ring), GFP_KERNEL);
	if (!adapter->tx_rings)
		goto err_out;
	adapter->rx_rings = kcalloc(num_active_queues,
				    sizeof(struct iavf_ring), GFP_KERNEL);
	if (!adapter->rx_rings)
		goto err_out;

	for (i = 0; i < num_active_queues; i++) {
		struct iavf_ring *tx_ring;
		struct iavf_ring *rx_ring;

		tx_ring = &adapter->tx_rings[i];

		tx_ring->queue_index = i;
		tx_ring->netdev = adapter->netdev;
		tx_ring->dev = &adapter->pdev->dev;
		tx_ring->count = adapter->tx_desc_count;
		tx_ring->itr_setting = IAVF_ITR_TX_DEF;
		if (adapter->flags & IAVF_FLAG_WB_ON_ITR_CAPABLE)
			tx_ring->flags |= IAVF_TXR_FLAGS_WB_ON_ITR;

		rx_ring = &adapter->rx_rings[i];
		rx_ring->queue_index = i;
		rx_ring->netdev = adapter->netdev;
		rx_ring->dev = &adapter->pdev->dev;
		rx_ring->count = adapter->rx_desc_count;
		rx_ring->itr_setting = IAVF_ITR_RX_DEF;
	}

	adapter->num_active_queues = num_active_queues;

	iavf_set_queue_vlan_tag_loc(adapter);

	return 0;

err_out:
	iavf_free_queues(adapter);
	return -ENOMEM;
}

 
static int iavf_set_interrupt_capability(struct iavf_adapter *adapter)
{
	int vector, v_budget;
	int pairs = 0;
	int err = 0;

	if (!adapter->vsi_res) {
		err = -EIO;
		goto out;
	}
	pairs = adapter->num_active_queues;

	 
	v_budget = min_t(int, pairs + NONQ_VECS,
			 (int)adapter->vf_res->max_vectors);

	adapter->msix_entries = kcalloc(v_budget,
					sizeof(struct msix_entry), GFP_KERNEL);
	if (!adapter->msix_entries) {
		err = -ENOMEM;
		goto out;
	}

	for (vector = 0; vector < v_budget; vector++)
		adapter->msix_entries[vector].entry = vector;

	err = iavf_acquire_msix_vectors(adapter, v_budget);
	if (!err)
		iavf_schedule_finish_config(adapter);

out:
	return err;
}

 
static int iavf_config_rss_aq(struct iavf_adapter *adapter)
{
	struct iavf_aqc_get_set_rss_key_data *rss_key =
		(struct iavf_aqc_get_set_rss_key_data *)adapter->rss_key;
	struct iavf_hw *hw = &adapter->hw;
	enum iavf_status status;

	if (adapter->current_op != VIRTCHNL_OP_UNKNOWN) {
		 
		dev_err(&adapter->pdev->dev, "Cannot configure RSS, command %d pending\n",
			adapter->current_op);
		return -EBUSY;
	}

	status = iavf_aq_set_rss_key(hw, adapter->vsi.id, rss_key);
	if (status) {
		dev_err(&adapter->pdev->dev, "Cannot set RSS key, err %s aq_err %s\n",
			iavf_stat_str(hw, status),
			iavf_aq_str(hw, hw->aq.asq_last_status));
		return iavf_status_to_errno(status);

	}

	status = iavf_aq_set_rss_lut(hw, adapter->vsi.id, false,
				     adapter->rss_lut, adapter->rss_lut_size);
	if (status) {
		dev_err(&adapter->pdev->dev, "Cannot set RSS lut, err %s aq_err %s\n",
			iavf_stat_str(hw, status),
			iavf_aq_str(hw, hw->aq.asq_last_status));
		return iavf_status_to_errno(status);
	}

	return 0;

}

 
static int iavf_config_rss_reg(struct iavf_adapter *adapter)
{
	struct iavf_hw *hw = &adapter->hw;
	u32 *dw;
	u16 i;

	dw = (u32 *)adapter->rss_key;
	for (i = 0; i <= adapter->rss_key_size / 4; i++)
		wr32(hw, IAVF_VFQF_HKEY(i), dw[i]);

	dw = (u32 *)adapter->rss_lut;
	for (i = 0; i <= adapter->rss_lut_size / 4; i++)
		wr32(hw, IAVF_VFQF_HLUT(i), dw[i]);

	iavf_flush(hw);

	return 0;
}

 
int iavf_config_rss(struct iavf_adapter *adapter)
{

	if (RSS_PF(adapter)) {
		adapter->aq_required |= IAVF_FLAG_AQ_SET_RSS_LUT |
					IAVF_FLAG_AQ_SET_RSS_KEY;
		return 0;
	} else if (RSS_AQ(adapter)) {
		return iavf_config_rss_aq(adapter);
	} else {
		return iavf_config_rss_reg(adapter);
	}
}

 
static void iavf_fill_rss_lut(struct iavf_adapter *adapter)
{
	u16 i;

	for (i = 0; i < adapter->rss_lut_size; i++)
		adapter->rss_lut[i] = i % adapter->num_active_queues;
}

 
static int iavf_init_rss(struct iavf_adapter *adapter)
{
	struct iavf_hw *hw = &adapter->hw;

	if (!RSS_PF(adapter)) {
		 
		if (adapter->vf_res->vf_cap_flags &
		    VIRTCHNL_VF_OFFLOAD_RSS_PCTYPE_V2)
			adapter->hena = IAVF_DEFAULT_RSS_HENA_EXPANDED;
		else
			adapter->hena = IAVF_DEFAULT_RSS_HENA;

		wr32(hw, IAVF_VFQF_HENA(0), (u32)adapter->hena);
		wr32(hw, IAVF_VFQF_HENA(1), (u32)(adapter->hena >> 32));
	}

	iavf_fill_rss_lut(adapter);
	netdev_rss_key_fill((void *)adapter->rss_key, adapter->rss_key_size);

	return iavf_config_rss(adapter);
}

 
static int iavf_alloc_q_vectors(struct iavf_adapter *adapter)
{
	int q_idx = 0, num_q_vectors;
	struct iavf_q_vector *q_vector;

	num_q_vectors = adapter->num_msix_vectors - NONQ_VECS;
	adapter->q_vectors = kcalloc(num_q_vectors, sizeof(*q_vector),
				     GFP_KERNEL);
	if (!adapter->q_vectors)
		return -ENOMEM;

	for (q_idx = 0; q_idx < num_q_vectors; q_idx++) {
		q_vector = &adapter->q_vectors[q_idx];
		q_vector->adapter = adapter;
		q_vector->vsi = &adapter->vsi;
		q_vector->v_idx = q_idx;
		q_vector->reg_idx = q_idx;
		cpumask_copy(&q_vector->affinity_mask, cpu_possible_mask);
		netif_napi_add(adapter->netdev, &q_vector->napi,
			       iavf_napi_poll);
	}

	return 0;
}

 
static void iavf_free_q_vectors(struct iavf_adapter *adapter)
{
	int q_idx, num_q_vectors;

	if (!adapter->q_vectors)
		return;

	num_q_vectors = adapter->num_msix_vectors - NONQ_VECS;

	for (q_idx = 0; q_idx < num_q_vectors; q_idx++) {
		struct iavf_q_vector *q_vector = &adapter->q_vectors[q_idx];

		netif_napi_del(&q_vector->napi);
	}
	kfree(adapter->q_vectors);
	adapter->q_vectors = NULL;
}

 
static void iavf_reset_interrupt_capability(struct iavf_adapter *adapter)
{
	if (!adapter->msix_entries)
		return;

	pci_disable_msix(adapter->pdev);
	kfree(adapter->msix_entries);
	adapter->msix_entries = NULL;
}

 
static int iavf_init_interrupt_scheme(struct iavf_adapter *adapter)
{
	int err;

	err = iavf_alloc_queues(adapter);
	if (err) {
		dev_err(&adapter->pdev->dev,
			"Unable to allocate memory for queues\n");
		goto err_alloc_queues;
	}

	err = iavf_set_interrupt_capability(adapter);
	if (err) {
		dev_err(&adapter->pdev->dev,
			"Unable to setup interrupt capabilities\n");
		goto err_set_interrupt;
	}

	err = iavf_alloc_q_vectors(adapter);
	if (err) {
		dev_err(&adapter->pdev->dev,
			"Unable to allocate memory for queue vectors\n");
		goto err_alloc_q_vectors;
	}

	 
	if ((adapter->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADQ) &&
	    adapter->num_tc)
		dev_info(&adapter->pdev->dev, "ADq Enabled, %u TCs created",
			 adapter->num_tc);

	dev_info(&adapter->pdev->dev, "Multiqueue %s: Queue pair count = %u",
		 (adapter->num_active_queues > 1) ? "Enabled" : "Disabled",
		 adapter->num_active_queues);

	return 0;
err_alloc_q_vectors:
	iavf_reset_interrupt_capability(adapter);
err_set_interrupt:
	iavf_free_queues(adapter);
err_alloc_queues:
	return err;
}

 
static void iavf_free_rss(struct iavf_adapter *adapter)
{
	kfree(adapter->rss_key);
	adapter->rss_key = NULL;

	kfree(adapter->rss_lut);
	adapter->rss_lut = NULL;
}

 
static int iavf_reinit_interrupt_scheme(struct iavf_adapter *adapter, bool running)
{
	struct net_device *netdev = adapter->netdev;
	int err;

	if (running)
		iavf_free_traffic_irqs(adapter);
	iavf_free_misc_irq(adapter);
	iavf_reset_interrupt_capability(adapter);
	iavf_free_q_vectors(adapter);
	iavf_free_queues(adapter);

	err =  iavf_init_interrupt_scheme(adapter);
	if (err)
		goto err;

	netif_tx_stop_all_queues(netdev);

	err = iavf_request_misc_irq(adapter);
	if (err)
		goto err;

	set_bit(__IAVF_VSI_DOWN, adapter->vsi.state);

	iavf_map_rings_to_vectors(adapter);
err:
	return err;
}

 
static void iavf_finish_config(struct work_struct *work)
{
	struct iavf_adapter *adapter;
	int pairs, err;

	adapter = container_of(work, struct iavf_adapter, finish_config);

	 
	rtnl_lock();
	mutex_lock(&adapter->crit_lock);

	if ((adapter->flags & IAVF_FLAG_SETUP_NETDEV_FEATURES) &&
	    adapter->netdev_registered &&
	    !test_bit(__IAVF_IN_REMOVE_TASK, &adapter->crit_section)) {
		netdev_update_features(adapter->netdev);
		adapter->flags &= ~IAVF_FLAG_SETUP_NETDEV_FEATURES;
	}

	switch (adapter->state) {
	case __IAVF_DOWN:
		if (!adapter->netdev_registered) {
			err = register_netdevice(adapter->netdev);
			if (err) {
				dev_err(&adapter->pdev->dev, "Unable to register netdev (%d)\n",
					err);

				 
				iavf_free_rss(adapter);
				iavf_free_misc_irq(adapter);
				iavf_reset_interrupt_capability(adapter);
				iavf_change_state(adapter,
						  __IAVF_INIT_CONFIG_ADAPTER);
				goto out;
			}
			adapter->netdev_registered = true;
		}

		 
		fallthrough;
	case __IAVF_RUNNING:
		pairs = adapter->num_active_queues;
		netif_set_real_num_rx_queues(adapter->netdev, pairs);
		netif_set_real_num_tx_queues(adapter->netdev, pairs);
		break;

	default:
		break;
	}

out:
	mutex_unlock(&adapter->crit_lock);
	rtnl_unlock();
}

 
void iavf_schedule_finish_config(struct iavf_adapter *adapter)
{
	if (!test_bit(__IAVF_IN_REMOVE_TASK, &adapter->crit_section))
		queue_work(adapter->wq, &adapter->finish_config);
}

 
static int iavf_process_aq_command(struct iavf_adapter *adapter)
{
	if (adapter->aq_required & IAVF_FLAG_AQ_GET_CONFIG)
		return iavf_send_vf_config_msg(adapter);
	if (adapter->aq_required & IAVF_FLAG_AQ_GET_OFFLOAD_VLAN_V2_CAPS)
		return iavf_send_vf_offload_vlan_v2_msg(adapter);
	if (adapter->aq_required & IAVF_FLAG_AQ_DISABLE_QUEUES) {
		iavf_disable_queues(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_MAP_VECTORS) {
		iavf_map_queues(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_ADD_MAC_FILTER) {
		iavf_add_ether_addrs(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_ADD_VLAN_FILTER) {
		iavf_add_vlans(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_DEL_MAC_FILTER) {
		iavf_del_ether_addrs(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_DEL_VLAN_FILTER) {
		iavf_del_vlans(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_ENABLE_VLAN_STRIPPING) {
		iavf_enable_vlan_stripping(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_DISABLE_VLAN_STRIPPING) {
		iavf_disable_vlan_stripping(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_CONFIGURE_QUEUES) {
		iavf_configure_queues(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_ENABLE_QUEUES) {
		iavf_enable_queues(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_CONFIGURE_RSS) {
		 
		adapter->aq_required &= ~IAVF_FLAG_AQ_CONFIGURE_RSS;
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_GET_HENA) {
		iavf_get_hena(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_SET_HENA) {
		iavf_set_hena(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_SET_RSS_KEY) {
		iavf_set_rss_key(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_SET_RSS_LUT) {
		iavf_set_rss_lut(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_CONFIGURE_PROMISC_MODE) {
		iavf_set_promiscuous(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_ENABLE_CHANNELS) {
		iavf_enable_channels(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_DISABLE_CHANNELS) {
		iavf_disable_channels(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_ADD_CLOUD_FILTER) {
		iavf_add_cloud_filter(adapter);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_DEL_CLOUD_FILTER) {
		iavf_del_cloud_filter(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_DEL_CLOUD_FILTER) {
		iavf_del_cloud_filter(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_ADD_CLOUD_FILTER) {
		iavf_add_cloud_filter(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_ADD_FDIR_FILTER) {
		iavf_add_fdir_filter(adapter);
		return IAVF_SUCCESS;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_DEL_FDIR_FILTER) {
		iavf_del_fdir_filter(adapter);
		return IAVF_SUCCESS;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_ADD_ADV_RSS_CFG) {
		iavf_add_adv_rss_cfg(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_DEL_ADV_RSS_CFG) {
		iavf_del_adv_rss_cfg(adapter);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_DISABLE_CTAG_VLAN_STRIPPING) {
		iavf_disable_vlan_stripping_v2(adapter, ETH_P_8021Q);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_DISABLE_STAG_VLAN_STRIPPING) {
		iavf_disable_vlan_stripping_v2(adapter, ETH_P_8021AD);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_ENABLE_CTAG_VLAN_STRIPPING) {
		iavf_enable_vlan_stripping_v2(adapter, ETH_P_8021Q);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_ENABLE_STAG_VLAN_STRIPPING) {
		iavf_enable_vlan_stripping_v2(adapter, ETH_P_8021AD);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_DISABLE_CTAG_VLAN_INSERTION) {
		iavf_disable_vlan_insertion_v2(adapter, ETH_P_8021Q);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_DISABLE_STAG_VLAN_INSERTION) {
		iavf_disable_vlan_insertion_v2(adapter, ETH_P_8021AD);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_ENABLE_CTAG_VLAN_INSERTION) {
		iavf_enable_vlan_insertion_v2(adapter, ETH_P_8021Q);
		return 0;
	}
	if (adapter->aq_required & IAVF_FLAG_AQ_ENABLE_STAG_VLAN_INSERTION) {
		iavf_enable_vlan_insertion_v2(adapter, ETH_P_8021AD);
		return 0;
	}

	if (adapter->aq_required & IAVF_FLAG_AQ_REQUEST_STATS) {
		iavf_request_stats(adapter);
		return 0;
	}

	return -EAGAIN;
}

 
static void
iavf_set_vlan_offload_features(struct iavf_adapter *adapter,
			       netdev_features_t prev_features,
			       netdev_features_t features)
{
	bool enable_stripping = true, enable_insertion = true;
	u16 vlan_ethertype = 0;
	u64 aq_required = 0;

	 
	if (features & (NETIF_F_HW_VLAN_STAG_RX | NETIF_F_HW_VLAN_STAG_TX))
		vlan_ethertype = ETH_P_8021AD;
	else if (features & (NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_CTAG_TX))
		vlan_ethertype = ETH_P_8021Q;
	else if (prev_features & (NETIF_F_HW_VLAN_STAG_RX | NETIF_F_HW_VLAN_STAG_TX))
		vlan_ethertype = ETH_P_8021AD;
	else if (prev_features & (NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_CTAG_TX))
		vlan_ethertype = ETH_P_8021Q;
	else
		vlan_ethertype = ETH_P_8021Q;

	if (!(features & (NETIF_F_HW_VLAN_STAG_RX | NETIF_F_HW_VLAN_CTAG_RX)))
		enable_stripping = false;
	if (!(features & (NETIF_F_HW_VLAN_STAG_TX | NETIF_F_HW_VLAN_CTAG_TX)))
		enable_insertion = false;

	if (VLAN_ALLOWED(adapter)) {
		 
		if (enable_stripping)
			aq_required |= IAVF_FLAG_AQ_ENABLE_VLAN_STRIPPING;
		else
			aq_required |= IAVF_FLAG_AQ_DISABLE_VLAN_STRIPPING;

	} else if (VLAN_V2_ALLOWED(adapter)) {
		switch (vlan_ethertype) {
		case ETH_P_8021Q:
			if (enable_stripping)
				aq_required |= IAVF_FLAG_AQ_ENABLE_CTAG_VLAN_STRIPPING;
			else
				aq_required |= IAVF_FLAG_AQ_DISABLE_CTAG_VLAN_STRIPPING;

			if (enable_insertion)
				aq_required |= IAVF_FLAG_AQ_ENABLE_CTAG_VLAN_INSERTION;
			else
				aq_required |= IAVF_FLAG_AQ_DISABLE_CTAG_VLAN_INSERTION;
			break;
		case ETH_P_8021AD:
			if (enable_stripping)
				aq_required |= IAVF_FLAG_AQ_ENABLE_STAG_VLAN_STRIPPING;
			else
				aq_required |= IAVF_FLAG_AQ_DISABLE_STAG_VLAN_STRIPPING;

			if (enable_insertion)
				aq_required |= IAVF_FLAG_AQ_ENABLE_STAG_VLAN_INSERTION;
			else
				aq_required |= IAVF_FLAG_AQ_DISABLE_STAG_VLAN_INSERTION;
			break;
		}
	}

	if (aq_required) {
		adapter->aq_required |= aq_required;
		mod_delayed_work(adapter->wq, &adapter->watchdog_task, 0);
	}
}

 
static void iavf_startup(struct iavf_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	struct iavf_hw *hw = &adapter->hw;
	enum iavf_status status;
	int ret;

	WARN_ON(adapter->state != __IAVF_STARTUP);

	 
	adapter->flags &= ~IAVF_FLAG_PF_COMMS_FAILED;
	adapter->flags &= ~IAVF_FLAG_RESET_PENDING;
	status = iavf_set_mac_type(hw);
	if (status) {
		dev_err(&pdev->dev, "Failed to set MAC type (%d)\n", status);
		goto err;
	}

	ret = iavf_check_reset_complete(hw);
	if (ret) {
		dev_info(&pdev->dev, "Device is still in reset (%d), retrying\n",
			 ret);
		goto err;
	}
	hw->aq.num_arq_entries = IAVF_AQ_LEN;
	hw->aq.num_asq_entries = IAVF_AQ_LEN;
	hw->aq.arq_buf_size = IAVF_MAX_AQ_BUF_SIZE;
	hw->aq.asq_buf_size = IAVF_MAX_AQ_BUF_SIZE;

	status = iavf_init_adminq(hw);
	if (status) {
		dev_err(&pdev->dev, "Failed to init Admin Queue (%d)\n",
			status);
		goto err;
	}
	ret = iavf_send_api_ver(adapter);
	if (ret) {
		dev_err(&pdev->dev, "Unable to send to PF (%d)\n", ret);
		iavf_shutdown_adminq(hw);
		goto err;
	}
	iavf_change_state(adapter, __IAVF_INIT_VERSION_CHECK);
	return;
err:
	iavf_change_state(adapter, __IAVF_INIT_FAILED);
}

 
static void iavf_init_version_check(struct iavf_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	struct iavf_hw *hw = &adapter->hw;
	int err = -EAGAIN;

	WARN_ON(adapter->state != __IAVF_INIT_VERSION_CHECK);

	if (!iavf_asq_done(hw)) {
		dev_err(&pdev->dev, "Admin queue command never completed\n");
		iavf_shutdown_adminq(hw);
		iavf_change_state(adapter, __IAVF_STARTUP);
		goto err;
	}

	 
	err = iavf_verify_api_ver(adapter);
	if (err) {
		if (err == -EALREADY)
			err = iavf_send_api_ver(adapter);
		else
			dev_err(&pdev->dev, "Unsupported PF API version %d.%d, expected %d.%d\n",
				adapter->pf_version.major,
				adapter->pf_version.minor,
				VIRTCHNL_VERSION_MAJOR,
				VIRTCHNL_VERSION_MINOR);
		goto err;
	}
	err = iavf_send_vf_config_msg(adapter);
	if (err) {
		dev_err(&pdev->dev, "Unable to send config request (%d)\n",
			err);
		goto err;
	}
	iavf_change_state(adapter, __IAVF_INIT_GET_RESOURCES);
	return;
err:
	iavf_change_state(adapter, __IAVF_INIT_FAILED);
}

 
int iavf_parse_vf_resource_msg(struct iavf_adapter *adapter)
{
	int i, num_req_queues = adapter->num_req_queues;
	struct iavf_vsi *vsi = &adapter->vsi;

	for (i = 0; i < adapter->vf_res->num_vsis; i++) {
		if (adapter->vf_res->vsi_res[i].vsi_type == VIRTCHNL_VSI_SRIOV)
			adapter->vsi_res = &adapter->vf_res->vsi_res[i];
	}
	if (!adapter->vsi_res) {
		dev_err(&adapter->pdev->dev, "No LAN VSI found\n");
		return -ENODEV;
	}

	if (num_req_queues &&
	    num_req_queues > adapter->vsi_res->num_queue_pairs) {
		 
		dev_err(&adapter->pdev->dev,
			"Requested %d queues, but PF only gave us %d.\n",
			num_req_queues,
			adapter->vsi_res->num_queue_pairs);
		adapter->flags |= IAVF_FLAG_REINIT_MSIX_NEEDED;
		adapter->num_req_queues = adapter->vsi_res->num_queue_pairs;
		iavf_schedule_reset(adapter, IAVF_FLAG_RESET_NEEDED);

		return -EAGAIN;
	}
	adapter->num_req_queues = 0;
	adapter->vsi.id = adapter->vsi_res->vsi_id;

	adapter->vsi.back = adapter;
	adapter->vsi.base_vector = 1;
	vsi->netdev = adapter->netdev;
	vsi->qs_handle = adapter->vsi_res->qset_handle;
	if (adapter->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		adapter->rss_key_size = adapter->vf_res->rss_key_size;
		adapter->rss_lut_size = adapter->vf_res->rss_lut_size;
	} else {
		adapter->rss_key_size = IAVF_HKEY_ARRAY_SIZE;
		adapter->rss_lut_size = IAVF_HLUT_ARRAY_SIZE;
	}

	return 0;
}

 
static void iavf_init_get_resources(struct iavf_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	struct iavf_hw *hw = &adapter->hw;
	int err;

	WARN_ON(adapter->state != __IAVF_INIT_GET_RESOURCES);
	 
	if (!adapter->vf_res) {
		adapter->vf_res = kzalloc(IAVF_VIRTCHNL_VF_RESOURCE_SIZE,
					  GFP_KERNEL);
		if (!adapter->vf_res) {
			err = -ENOMEM;
			goto err;
		}
	}
	err = iavf_get_vf_config(adapter);
	if (err == -EALREADY) {
		err = iavf_send_vf_config_msg(adapter);
		goto err;
	} else if (err == -EINVAL) {
		 
		iavf_shutdown_adminq(hw);
		dev_err(&pdev->dev, "Unable to get VF config due to PF error condition, not retrying\n");
		return;
	}
	if (err) {
		dev_err(&pdev->dev, "Unable to get VF config (%d)\n", err);
		goto err_alloc;
	}

	err = iavf_parse_vf_resource_msg(adapter);
	if (err) {
		dev_err(&pdev->dev, "Failed to parse VF resource message from PF (%d)\n",
			err);
		goto err_alloc;
	}
	 
	adapter->extended_caps = IAVF_EXTENDED_CAPS;

	iavf_change_state(adapter, __IAVF_INIT_EXTENDED_CAPS);
	return;

err_alloc:
	kfree(adapter->vf_res);
	adapter->vf_res = NULL;
err:
	iavf_change_state(adapter, __IAVF_INIT_FAILED);
}

 
static void iavf_init_send_offload_vlan_v2_caps(struct iavf_adapter *adapter)
{
	int ret;

	WARN_ON(!(adapter->extended_caps & IAVF_EXTENDED_CAP_SEND_VLAN_V2));

	ret = iavf_send_vf_offload_vlan_v2_msg(adapter);
	if (ret && ret == -EOPNOTSUPP) {
		 
		adapter->extended_caps &= ~IAVF_EXTENDED_CAP_RECV_VLAN_V2;
	}

	 
	adapter->extended_caps &= ~IAVF_EXTENDED_CAP_SEND_VLAN_V2;
}

 
static void iavf_init_recv_offload_vlan_v2_caps(struct iavf_adapter *adapter)
{
	int ret;

	WARN_ON(!(adapter->extended_caps & IAVF_EXTENDED_CAP_RECV_VLAN_V2));

	memset(&adapter->vlan_v2_caps, 0, sizeof(adapter->vlan_v2_caps));

	ret = iavf_get_vf_vlan_v2_caps(adapter);
	if (ret)
		goto err;

	 
	adapter->extended_caps &= ~IAVF_EXTENDED_CAP_RECV_VLAN_V2;
	return;
err:
	 
	adapter->extended_caps |= IAVF_EXTENDED_CAP_SEND_VLAN_V2;
	iavf_change_state(adapter, __IAVF_INIT_FAILED);
}

 
static void iavf_init_process_extended_caps(struct iavf_adapter *adapter)
{
	WARN_ON(adapter->state != __IAVF_INIT_EXTENDED_CAPS);

	 
	if (adapter->extended_caps & IAVF_EXTENDED_CAP_SEND_VLAN_V2) {
		iavf_init_send_offload_vlan_v2_caps(adapter);
		return;
	} else if (adapter->extended_caps & IAVF_EXTENDED_CAP_RECV_VLAN_V2) {
		iavf_init_recv_offload_vlan_v2_caps(adapter);
		return;
	}

	 
	iavf_change_state(adapter, __IAVF_INIT_CONFIG_ADAPTER);
}

 
static void iavf_init_config_adapter(struct iavf_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	int err;

	WARN_ON(adapter->state != __IAVF_INIT_CONFIG_ADAPTER);

	if (iavf_process_config(adapter))
		goto err;

	adapter->current_op = VIRTCHNL_OP_UNKNOWN;

	adapter->flags |= IAVF_FLAG_RX_CSUM_ENABLED;

	netdev->netdev_ops = &iavf_netdev_ops;
	iavf_set_ethtool_ops(netdev);
	netdev->watchdog_timeo = 5 * HZ;

	 
	netdev->min_mtu = ETH_MIN_MTU;
	netdev->max_mtu = IAVF_MAX_RXBUFFER - IAVF_PACKET_HDR_PAD;

	if (!is_valid_ether_addr(adapter->hw.mac.addr)) {
		dev_info(&pdev->dev, "Invalid MAC address %pM, using random\n",
			 adapter->hw.mac.addr);
		eth_hw_addr_random(netdev);
		ether_addr_copy(adapter->hw.mac.addr, netdev->dev_addr);
	} else {
		eth_hw_addr_set(netdev, adapter->hw.mac.addr);
		ether_addr_copy(netdev->perm_addr, adapter->hw.mac.addr);
	}

	adapter->tx_desc_count = IAVF_DEFAULT_TXD;
	adapter->rx_desc_count = IAVF_DEFAULT_RXD;
	err = iavf_init_interrupt_scheme(adapter);
	if (err)
		goto err_sw_init;
	iavf_map_rings_to_vectors(adapter);
	if (adapter->vf_res->vf_cap_flags &
		VIRTCHNL_VF_OFFLOAD_WB_ON_ITR)
		adapter->flags |= IAVF_FLAG_WB_ON_ITR_CAPABLE;

	err = iavf_request_misc_irq(adapter);
	if (err)
		goto err_sw_init;

	netif_carrier_off(netdev);
	adapter->link_up = false;
	netif_tx_stop_all_queues(netdev);

	if (CLIENT_ALLOWED(adapter)) {
		err = iavf_lan_add_device(adapter);
		if (err)
			dev_info(&pdev->dev, "Failed to add VF to client API service list: %d\n",
				 err);
	}
	dev_info(&pdev->dev, "MAC address: %pM\n", adapter->hw.mac.addr);
	if (netdev->features & NETIF_F_GRO)
		dev_info(&pdev->dev, "GRO is enabled\n");

	iavf_change_state(adapter, __IAVF_DOWN);
	set_bit(__IAVF_VSI_DOWN, adapter->vsi.state);

	iavf_misc_irq_enable(adapter);
	wake_up(&adapter->down_waitqueue);

	adapter->rss_key = kzalloc(adapter->rss_key_size, GFP_KERNEL);
	adapter->rss_lut = kzalloc(adapter->rss_lut_size, GFP_KERNEL);
	if (!adapter->rss_key || !adapter->rss_lut) {
		err = -ENOMEM;
		goto err_mem;
	}
	if (RSS_AQ(adapter))
		adapter->aq_required |= IAVF_FLAG_AQ_CONFIGURE_RSS;
	else
		iavf_init_rss(adapter);

	if (VLAN_V2_ALLOWED(adapter))
		 
		iavf_set_vlan_offload_features(adapter, 0, netdev->features);

	iavf_schedule_finish_config(adapter);
	return;

err_mem:
	iavf_free_rss(adapter);
	iavf_free_misc_irq(adapter);
err_sw_init:
	iavf_reset_interrupt_capability(adapter);
err:
	iavf_change_state(adapter, __IAVF_INIT_FAILED);
}

 
static void iavf_watchdog_task(struct work_struct *work)
{
	struct iavf_adapter *adapter = container_of(work,
						    struct iavf_adapter,
						    watchdog_task.work);
	struct iavf_hw *hw = &adapter->hw;
	u32 reg_val;

	if (!mutex_trylock(&adapter->crit_lock)) {
		if (adapter->state == __IAVF_REMOVE)
			return;

		goto restart_watchdog;
	}

	if (adapter->flags & IAVF_FLAG_PF_COMMS_FAILED)
		iavf_change_state(adapter, __IAVF_COMM_FAILED);

	switch (adapter->state) {
	case __IAVF_STARTUP:
		iavf_startup(adapter);
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq, &adapter->watchdog_task,
				   msecs_to_jiffies(30));
		return;
	case __IAVF_INIT_VERSION_CHECK:
		iavf_init_version_check(adapter);
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq, &adapter->watchdog_task,
				   msecs_to_jiffies(30));
		return;
	case __IAVF_INIT_GET_RESOURCES:
		iavf_init_get_resources(adapter);
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq, &adapter->watchdog_task,
				   msecs_to_jiffies(1));
		return;
	case __IAVF_INIT_EXTENDED_CAPS:
		iavf_init_process_extended_caps(adapter);
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq, &adapter->watchdog_task,
				   msecs_to_jiffies(1));
		return;
	case __IAVF_INIT_CONFIG_ADAPTER:
		iavf_init_config_adapter(adapter);
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq, &adapter->watchdog_task,
				   msecs_to_jiffies(1));
		return;
	case __IAVF_INIT_FAILED:
		if (test_bit(__IAVF_IN_REMOVE_TASK,
			     &adapter->crit_section)) {
			 
			mutex_unlock(&adapter->crit_lock);
			return;
		}
		if (++adapter->aq_wait_count > IAVF_AQ_MAX_ERR) {
			dev_err(&adapter->pdev->dev,
				"Failed to communicate with PF; waiting before retry\n");
			adapter->flags |= IAVF_FLAG_PF_COMMS_FAILED;
			iavf_shutdown_adminq(hw);
			mutex_unlock(&adapter->crit_lock);
			queue_delayed_work(adapter->wq,
					   &adapter->watchdog_task, (5 * HZ));
			return;
		}
		 
		iavf_change_state(adapter, adapter->last_state);
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq, &adapter->watchdog_task, HZ);
		return;
	case __IAVF_COMM_FAILED:
		if (test_bit(__IAVF_IN_REMOVE_TASK,
			     &adapter->crit_section)) {
			 
			iavf_change_state(adapter, __IAVF_INIT_FAILED);
			adapter->flags &= ~IAVF_FLAG_PF_COMMS_FAILED;
			mutex_unlock(&adapter->crit_lock);
			return;
		}
		reg_val = rd32(hw, IAVF_VFGEN_RSTAT) &
			  IAVF_VFGEN_RSTAT_VFR_STATE_MASK;
		if (reg_val == VIRTCHNL_VFR_VFACTIVE ||
		    reg_val == VIRTCHNL_VFR_COMPLETED) {
			 
			dev_err(&adapter->pdev->dev,
				"Hardware came out of reset. Attempting reinit.\n");
			 
			iavf_change_state(adapter, __IAVF_STARTUP);
			adapter->flags &= ~IAVF_FLAG_PF_COMMS_FAILED;
		}
		adapter->aq_required = 0;
		adapter->current_op = VIRTCHNL_OP_UNKNOWN;
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq,
				   &adapter->watchdog_task,
				   msecs_to_jiffies(10));
		return;
	case __IAVF_RESETTING:
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq, &adapter->watchdog_task,
				   HZ * 2);
		return;
	case __IAVF_DOWN:
	case __IAVF_DOWN_PENDING:
	case __IAVF_TESTING:
	case __IAVF_RUNNING:
		if (adapter->current_op) {
			if (!iavf_asq_done(hw)) {
				dev_dbg(&adapter->pdev->dev,
					"Admin queue timeout\n");
				iavf_send_api_ver(adapter);
			}
		} else {
			int ret = iavf_process_aq_command(adapter);

			 
			if (ret && ret != -EOPNOTSUPP &&
			    adapter->state == __IAVF_RUNNING)
				iavf_request_stats(adapter);
		}
		if (adapter->state == __IAVF_RUNNING)
			iavf_detect_recover_hung(&adapter->vsi);
		break;
	case __IAVF_REMOVE:
	default:
		mutex_unlock(&adapter->crit_lock);
		return;
	}

	 
	reg_val = rd32(hw, IAVF_VF_ARQLEN1) & IAVF_VF_ARQLEN1_ARQENABLE_MASK;
	if (!reg_val) {
		adapter->aq_required = 0;
		adapter->current_op = VIRTCHNL_OP_UNKNOWN;
		dev_err(&adapter->pdev->dev, "Hardware reset detected\n");
		iavf_schedule_reset(adapter, IAVF_FLAG_RESET_PENDING);
		mutex_unlock(&adapter->crit_lock);
		queue_delayed_work(adapter->wq,
				   &adapter->watchdog_task, HZ * 2);
		return;
	}

	schedule_delayed_work(&adapter->client_task, msecs_to_jiffies(5));
	mutex_unlock(&adapter->crit_lock);
restart_watchdog:
	if (adapter->state >= __IAVF_DOWN)
		queue_work(adapter->wq, &adapter->adminq_task);
	if (adapter->aq_required)
		queue_delayed_work(adapter->wq, &adapter->watchdog_task,
				   msecs_to_jiffies(20));
	else
		queue_delayed_work(adapter->wq, &adapter->watchdog_task,
				   HZ * 2);
}

 
static void iavf_disable_vf(struct iavf_adapter *adapter)
{
	struct iavf_mac_filter *f, *ftmp;
	struct iavf_vlan_filter *fv, *fvtmp;
	struct iavf_cloud_filter *cf, *cftmp;

	adapter->flags |= IAVF_FLAG_PF_COMMS_FAILED;

	 
	if (adapter->state == __IAVF_RUNNING) {
		set_bit(__IAVF_VSI_DOWN, adapter->vsi.state);
		netif_carrier_off(adapter->netdev);
		netif_tx_disable(adapter->netdev);
		adapter->link_up = false;
		iavf_napi_disable_all(adapter);
		iavf_irq_disable(adapter);
		iavf_free_traffic_irqs(adapter);
		iavf_free_all_tx_resources(adapter);
		iavf_free_all_rx_resources(adapter);
	}

	spin_lock_bh(&adapter->mac_vlan_list_lock);

	 
	list_for_each_entry_safe(f, ftmp, &adapter->mac_filter_list, list) {
		list_del(&f->list);
		kfree(f);
	}

	list_for_each_entry_safe(fv, fvtmp, &adapter->vlan_filter_list, list) {
		list_del(&fv->list);
		kfree(fv);
	}
	adapter->num_vlan_filters = 0;

	spin_unlock_bh(&adapter->mac_vlan_list_lock);

	spin_lock_bh(&adapter->cloud_filter_list_lock);
	list_for_each_entry_safe(cf, cftmp, &adapter->cloud_filter_list, list) {
		list_del(&cf->list);
		kfree(cf);
		adapter->num_cloud_filters--;
	}
	spin_unlock_bh(&adapter->cloud_filter_list_lock);

	iavf_free_misc_irq(adapter);
	iavf_reset_interrupt_capability(adapter);
	iavf_free_q_vectors(adapter);
	iavf_free_queues(adapter);
	memset(adapter->vf_res, 0, IAVF_VIRTCHNL_VF_RESOURCE_SIZE);
	iavf_shutdown_adminq(&adapter->hw);
	adapter->flags &= ~IAVF_FLAG_RESET_PENDING;
	iavf_change_state(adapter, __IAVF_DOWN);
	wake_up(&adapter->down_waitqueue);
	dev_info(&adapter->pdev->dev, "Reset task did not complete, VF disabled\n");
}

 
static void iavf_reset_task(struct work_struct *work)
{
	struct iavf_adapter *adapter = container_of(work,
						      struct iavf_adapter,
						      reset_task);
	struct virtchnl_vf_resource *vfres = adapter->vf_res;
	struct net_device *netdev = adapter->netdev;
	struct iavf_hw *hw = &adapter->hw;
	struct iavf_mac_filter *f, *ftmp;
	struct iavf_cloud_filter *cf;
	enum iavf_status status;
	u32 reg_val;
	int i = 0, err;
	bool running;

	 
	if (!mutex_trylock(&adapter->crit_lock)) {
		if (adapter->state != __IAVF_REMOVE)
			queue_work(adapter->wq, &adapter->reset_task);

		return;
	}

	while (!mutex_trylock(&adapter->client_lock))
		usleep_range(500, 1000);
	if (CLIENT_ENABLED(adapter)) {
		adapter->flags &= ~(IAVF_FLAG_CLIENT_NEEDS_OPEN |
				    IAVF_FLAG_CLIENT_NEEDS_CLOSE |
				    IAVF_FLAG_CLIENT_NEEDS_L2_PARAMS |
				    IAVF_FLAG_SERVICE_CLIENT_REQUESTED);
		cancel_delayed_work_sync(&adapter->client_task);
		iavf_notify_client_close(&adapter->vsi, true);
	}
	iavf_misc_irq_disable(adapter);
	if (adapter->flags & IAVF_FLAG_RESET_NEEDED) {
		adapter->flags &= ~IAVF_FLAG_RESET_NEEDED;
		 
		iavf_shutdown_adminq(hw);
		iavf_init_adminq(hw);
		iavf_request_reset(adapter);
	}
	adapter->flags |= IAVF_FLAG_RESET_PENDING;

	 
	for (i = 0; i < IAVF_RESET_WAIT_DETECTED_COUNT; i++) {
		reg_val = rd32(hw, IAVF_VF_ARQLEN1) &
			  IAVF_VF_ARQLEN1_ARQENABLE_MASK;
		if (!reg_val)
			break;
		usleep_range(5000, 10000);
	}
	if (i == IAVF_RESET_WAIT_DETECTED_COUNT) {
		dev_info(&adapter->pdev->dev, "Never saw reset\n");
		goto continue_reset;  
	}

	 
	for (i = 0; i < IAVF_RESET_WAIT_COMPLETE_COUNT; i++) {
		 
		msleep(IAVF_RESET_WAIT_MS);

		reg_val = rd32(hw, IAVF_VFGEN_RSTAT) &
			  IAVF_VFGEN_RSTAT_VFR_STATE_MASK;
		if (reg_val == VIRTCHNL_VFR_VFACTIVE)
			break;
	}

	pci_set_master(adapter->pdev);
	pci_restore_msi_state(adapter->pdev);

	if (i == IAVF_RESET_WAIT_COMPLETE_COUNT) {
		dev_err(&adapter->pdev->dev, "Reset never finished (%x)\n",
			reg_val);
		iavf_disable_vf(adapter);
		mutex_unlock(&adapter->client_lock);
		mutex_unlock(&adapter->crit_lock);
		return;  
	}

continue_reset:
	 
	running = adapter->state == __IAVF_RUNNING;

	if (running) {
		netif_carrier_off(netdev);
		netif_tx_stop_all_queues(netdev);
		adapter->link_up = false;
		iavf_napi_disable_all(adapter);
	}
	iavf_irq_disable(adapter);

	iavf_change_state(adapter, __IAVF_RESETTING);
	adapter->flags &= ~IAVF_FLAG_RESET_PENDING;

	 
	iavf_free_all_rx_resources(adapter);
	iavf_free_all_tx_resources(adapter);

	adapter->flags |= IAVF_FLAG_QUEUES_DISABLED;
	 
	iavf_shutdown_adminq(hw);
	adapter->current_op = VIRTCHNL_OP_UNKNOWN;
	status = iavf_init_adminq(hw);
	if (status) {
		dev_info(&adapter->pdev->dev, "Failed to init adminq: %d\n",
			 status);
		goto reset_err;
	}
	adapter->aq_required = 0;

	if ((adapter->flags & IAVF_FLAG_REINIT_MSIX_NEEDED) ||
	    (adapter->flags & IAVF_FLAG_REINIT_ITR_NEEDED)) {
		err = iavf_reinit_interrupt_scheme(adapter, running);
		if (err)
			goto reset_err;
	}

	if (RSS_AQ(adapter)) {
		adapter->aq_required |= IAVF_FLAG_AQ_CONFIGURE_RSS;
	} else {
		err = iavf_init_rss(adapter);
		if (err)
			goto reset_err;
	}

	adapter->aq_required |= IAVF_FLAG_AQ_GET_CONFIG;
	 
	adapter->aq_required |= IAVF_FLAG_AQ_GET_OFFLOAD_VLAN_V2_CAPS;
	adapter->aq_required |= IAVF_FLAG_AQ_MAP_VECTORS;

	spin_lock_bh(&adapter->mac_vlan_list_lock);

	 
	list_for_each_entry_safe(f, ftmp, &adapter->mac_filter_list, list) {
		if (ether_addr_equal(f->macaddr, adapter->hw.mac.addr)) {
			list_del(&f->list);
			kfree(f);
		}
	}
	 
	list_for_each_entry(f, &adapter->mac_filter_list, list) {
		f->add = true;
	}
	spin_unlock_bh(&adapter->mac_vlan_list_lock);

	 
	spin_lock_bh(&adapter->cloud_filter_list_lock);
	if ((vfres->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADQ) &&
	    adapter->num_tc) {
		list_for_each_entry(cf, &adapter->cloud_filter_list, list) {
			cf->add = true;
		}
	}
	spin_unlock_bh(&adapter->cloud_filter_list_lock);

	adapter->aq_required |= IAVF_FLAG_AQ_ADD_MAC_FILTER;
	adapter->aq_required |= IAVF_FLAG_AQ_ADD_CLOUD_FILTER;
	iavf_misc_irq_enable(adapter);

	mod_delayed_work(adapter->wq, &adapter->watchdog_task, 2);

	 
	if (running) {
		 
		err = iavf_setup_all_tx_resources(adapter);
		if (err)
			goto reset_err;

		 
		err = iavf_setup_all_rx_resources(adapter);
		if (err)
			goto reset_err;

		if ((adapter->flags & IAVF_FLAG_REINIT_MSIX_NEEDED) ||
		    (adapter->flags & IAVF_FLAG_REINIT_ITR_NEEDED)) {
			err = iavf_request_traffic_irqs(adapter, netdev->name);
			if (err)
				goto reset_err;

			adapter->flags &= ~IAVF_FLAG_REINIT_MSIX_NEEDED;
		}

		iavf_configure(adapter);

		 
		iavf_up_complete(adapter);

		iavf_irq_enable(adapter, true);
	} else {
		iavf_change_state(adapter, __IAVF_DOWN);
		wake_up(&adapter->down_waitqueue);
	}

	adapter->flags &= ~IAVF_FLAG_REINIT_ITR_NEEDED;

	wake_up(&adapter->reset_waitqueue);
	mutex_unlock(&adapter->client_lock);
	mutex_unlock(&adapter->crit_lock);

	return;
reset_err:
	if (running) {
		set_bit(__IAVF_VSI_DOWN, adapter->vsi.state);
		iavf_free_traffic_irqs(adapter);
	}
	iavf_disable_vf(adapter);

	mutex_unlock(&adapter->client_lock);
	mutex_unlock(&adapter->crit_lock);
	dev_err(&adapter->pdev->dev, "failed to allocate resources during reinit\n");
}

 
static void iavf_adminq_task(struct work_struct *work)
{
	struct iavf_adapter *adapter =
		container_of(work, struct iavf_adapter, adminq_task);
	struct iavf_hw *hw = &adapter->hw;
	struct iavf_arq_event_info event;
	enum virtchnl_ops v_op;
	enum iavf_status ret, v_ret;
	u32 val, oldval;
	u16 pending;

	if (!mutex_trylock(&adapter->crit_lock)) {
		if (adapter->state == __IAVF_REMOVE)
			return;

		queue_work(adapter->wq, &adapter->adminq_task);
		goto out;
	}

	if (adapter->flags & IAVF_FLAG_PF_COMMS_FAILED)
		goto unlock;

	event.buf_len = IAVF_MAX_AQ_BUF_SIZE;
	event.msg_buf = kzalloc(event.buf_len, GFP_KERNEL);
	if (!event.msg_buf)
		goto unlock;

	do {
		ret = iavf_clean_arq_element(hw, &event, &pending);
		v_op = (enum virtchnl_ops)le32_to_cpu(event.desc.cookie_high);
		v_ret = (enum iavf_status)le32_to_cpu(event.desc.cookie_low);

		if (ret || !v_op)
			break;  

		iavf_virtchnl_completion(adapter, v_op, v_ret, event.msg_buf,
					 event.msg_len);
		if (pending != 0)
			memset(event.msg_buf, 0, IAVF_MAX_AQ_BUF_SIZE);
	} while (pending);

	if (iavf_is_reset_in_progress(adapter))
		goto freedom;

	 
	val = rd32(hw, hw->aq.arq.len);
	if (val == 0xdeadbeef || val == 0xffffffff)  
		goto freedom;
	oldval = val;
	if (val & IAVF_VF_ARQLEN1_ARQVFE_MASK) {
		dev_info(&adapter->pdev->dev, "ARQ VF Error detected\n");
		val &= ~IAVF_VF_ARQLEN1_ARQVFE_MASK;
	}
	if (val & IAVF_VF_ARQLEN1_ARQOVFL_MASK) {
		dev_info(&adapter->pdev->dev, "ARQ Overflow Error detected\n");
		val &= ~IAVF_VF_ARQLEN1_ARQOVFL_MASK;
	}
	if (val & IAVF_VF_ARQLEN1_ARQCRIT_MASK) {
		dev_info(&adapter->pdev->dev, "ARQ Critical Error detected\n");
		val &= ~IAVF_VF_ARQLEN1_ARQCRIT_MASK;
	}
	if (oldval != val)
		wr32(hw, hw->aq.arq.len, val);

	val = rd32(hw, hw->aq.asq.len);
	oldval = val;
	if (val & IAVF_VF_ATQLEN1_ATQVFE_MASK) {
		dev_info(&adapter->pdev->dev, "ASQ VF Error detected\n");
		val &= ~IAVF_VF_ATQLEN1_ATQVFE_MASK;
	}
	if (val & IAVF_VF_ATQLEN1_ATQOVFL_MASK) {
		dev_info(&adapter->pdev->dev, "ASQ Overflow Error detected\n");
		val &= ~IAVF_VF_ATQLEN1_ATQOVFL_MASK;
	}
	if (val & IAVF_VF_ATQLEN1_ATQCRIT_MASK) {
		dev_info(&adapter->pdev->dev, "ASQ Critical Error detected\n");
		val &= ~IAVF_VF_ATQLEN1_ATQCRIT_MASK;
	}
	if (oldval != val)
		wr32(hw, hw->aq.asq.len, val);

freedom:
	kfree(event.msg_buf);
unlock:
	mutex_unlock(&adapter->crit_lock);
out:
	 
	iavf_misc_irq_enable(adapter);
}

 
static void iavf_client_task(struct work_struct *work)
{
	struct iavf_adapter *adapter =
		container_of(work, struct iavf_adapter, client_task.work);

	 

	if (!mutex_trylock(&adapter->client_lock))
		return;

	if (adapter->flags & IAVF_FLAG_SERVICE_CLIENT_REQUESTED) {
		iavf_client_subtask(adapter);
		adapter->flags &= ~IAVF_FLAG_SERVICE_CLIENT_REQUESTED;
		goto out;
	}
	if (adapter->flags & IAVF_FLAG_CLIENT_NEEDS_L2_PARAMS) {
		iavf_notify_client_l2_params(&adapter->vsi);
		adapter->flags &= ~IAVF_FLAG_CLIENT_NEEDS_L2_PARAMS;
		goto out;
	}
	if (adapter->flags & IAVF_FLAG_CLIENT_NEEDS_CLOSE) {
		iavf_notify_client_close(&adapter->vsi, false);
		adapter->flags &= ~IAVF_FLAG_CLIENT_NEEDS_CLOSE;
		goto out;
	}
	if (adapter->flags & IAVF_FLAG_CLIENT_NEEDS_OPEN) {
		iavf_notify_client_open(&adapter->vsi);
		adapter->flags &= ~IAVF_FLAG_CLIENT_NEEDS_OPEN;
	}
out:
	mutex_unlock(&adapter->client_lock);
}

 
void iavf_free_all_tx_resources(struct iavf_adapter *adapter)
{
	int i;

	if (!adapter->tx_rings)
		return;

	for (i = 0; i < adapter->num_active_queues; i++)
		if (adapter->tx_rings[i].desc)
			iavf_free_tx_resources(&adapter->tx_rings[i]);
}

 
static int iavf_setup_all_tx_resources(struct iavf_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_active_queues; i++) {
		adapter->tx_rings[i].count = adapter->tx_desc_count;
		err = iavf_setup_tx_descriptors(&adapter->tx_rings[i]);
		if (!err)
			continue;
		dev_err(&adapter->pdev->dev,
			"Allocation for Tx Queue %u failed\n", i);
		break;
	}

	return err;
}

 
static int iavf_setup_all_rx_resources(struct iavf_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_active_queues; i++) {
		adapter->rx_rings[i].count = adapter->rx_desc_count;
		err = iavf_setup_rx_descriptors(&adapter->rx_rings[i]);
		if (!err)
			continue;
		dev_err(&adapter->pdev->dev,
			"Allocation for Rx Queue %u failed\n", i);
		break;
	}
	return err;
}

 
void iavf_free_all_rx_resources(struct iavf_adapter *adapter)
{
	int i;

	if (!adapter->rx_rings)
		return;

	for (i = 0; i < adapter->num_active_queues; i++)
		if (adapter->rx_rings[i].desc)
			iavf_free_rx_resources(&adapter->rx_rings[i]);
}

 
static int iavf_validate_tx_bandwidth(struct iavf_adapter *adapter,
				      u64 max_tx_rate)
{
	int speed = 0, ret = 0;

	if (ADV_LINK_SUPPORT(adapter)) {
		if (adapter->link_speed_mbps < U32_MAX) {
			speed = adapter->link_speed_mbps;
			goto validate_bw;
		} else {
			dev_err(&adapter->pdev->dev, "Unknown link speed\n");
			return -EINVAL;
		}
	}

	switch (adapter->link_speed) {
	case VIRTCHNL_LINK_SPEED_40GB:
		speed = SPEED_40000;
		break;
	case VIRTCHNL_LINK_SPEED_25GB:
		speed = SPEED_25000;
		break;
	case VIRTCHNL_LINK_SPEED_20GB:
		speed = SPEED_20000;
		break;
	case VIRTCHNL_LINK_SPEED_10GB:
		speed = SPEED_10000;
		break;
	case VIRTCHNL_LINK_SPEED_5GB:
		speed = SPEED_5000;
		break;
	case VIRTCHNL_LINK_SPEED_2_5GB:
		speed = SPEED_2500;
		break;
	case VIRTCHNL_LINK_SPEED_1GB:
		speed = SPEED_1000;
		break;
	case VIRTCHNL_LINK_SPEED_100MB:
		speed = SPEED_100;
		break;
	default:
		break;
	}

validate_bw:
	if (max_tx_rate > speed) {
		dev_err(&adapter->pdev->dev,
			"Invalid tx rate specified\n");
		ret = -EINVAL;
	}

	return ret;
}

 
static int iavf_validate_ch_config(struct iavf_adapter *adapter,
				   struct tc_mqprio_qopt_offload *mqprio_qopt)
{
	u64 total_max_rate = 0;
	u32 tx_rate_rem = 0;
	int i, num_qps = 0;
	u64 tx_rate = 0;
	int ret = 0;

	if (mqprio_qopt->qopt.num_tc > IAVF_MAX_TRAFFIC_CLASS ||
	    mqprio_qopt->qopt.num_tc < 1)
		return -EINVAL;

	for (i = 0; i <= mqprio_qopt->qopt.num_tc - 1; i++) {
		if (!mqprio_qopt->qopt.count[i] ||
		    mqprio_qopt->qopt.offset[i] != num_qps)
			return -EINVAL;
		if (mqprio_qopt->min_rate[i]) {
			dev_err(&adapter->pdev->dev,
				"Invalid min tx rate (greater than 0) specified for TC%d\n",
				i);
			return -EINVAL;
		}

		 
		tx_rate = div_u64(mqprio_qopt->max_rate[i],
				  IAVF_MBPS_DIVISOR);

		if (mqprio_qopt->max_rate[i] &&
		    tx_rate < IAVF_MBPS_QUANTA) {
			dev_err(&adapter->pdev->dev,
				"Invalid max tx rate for TC%d, minimum %dMbps\n",
				i, IAVF_MBPS_QUANTA);
			return -EINVAL;
		}

		(void)div_u64_rem(tx_rate, IAVF_MBPS_QUANTA, &tx_rate_rem);

		if (tx_rate_rem != 0) {
			dev_err(&adapter->pdev->dev,
				"Invalid max tx rate for TC%d, not divisible by %d\n",
				i, IAVF_MBPS_QUANTA);
			return -EINVAL;
		}

		total_max_rate += tx_rate;
		num_qps += mqprio_qopt->qopt.count[i];
	}
	if (num_qps > adapter->num_active_queues) {
		dev_err(&adapter->pdev->dev,
			"Cannot support requested number of queues\n");
		return -EINVAL;
	}

	ret = iavf_validate_tx_bandwidth(adapter, total_max_rate);
	return ret;
}

 
static void iavf_del_all_cloud_filters(struct iavf_adapter *adapter)
{
	struct iavf_cloud_filter *cf, *cftmp;

	spin_lock_bh(&adapter->cloud_filter_list_lock);
	list_for_each_entry_safe(cf, cftmp, &adapter->cloud_filter_list,
				 list) {
		list_del(&cf->list);
		kfree(cf);
		adapter->num_cloud_filters--;
	}
	spin_unlock_bh(&adapter->cloud_filter_list_lock);
}

 
static int __iavf_setup_tc(struct net_device *netdev, void *type_data)
{
	struct tc_mqprio_qopt_offload *mqprio_qopt = type_data;
	struct iavf_adapter *adapter = netdev_priv(netdev);
	struct virtchnl_vf_resource *vfres = adapter->vf_res;
	u8 num_tc = 0, total_qps = 0;
	int ret = 0, netdev_tc = 0;
	u64 max_tx_rate;
	u16 mode;
	int i;

	num_tc = mqprio_qopt->qopt.num_tc;
	mode = mqprio_qopt->mode;

	 
	if (!mqprio_qopt->qopt.hw) {
		if (adapter->ch_config.state == __IAVF_TC_RUNNING) {
			 
			netdev_reset_tc(netdev);
			adapter->num_tc = 0;
			netif_tx_stop_all_queues(netdev);
			netif_tx_disable(netdev);
			iavf_del_all_cloud_filters(adapter);
			adapter->aq_required = IAVF_FLAG_AQ_DISABLE_CHANNELS;
			total_qps = adapter->orig_num_active_queues;
			goto exit;
		} else {
			return -EINVAL;
		}
	}

	 
	if (mode == TC_MQPRIO_MODE_CHANNEL) {
		if (!(vfres->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADQ)) {
			dev_err(&adapter->pdev->dev, "ADq not supported\n");
			return -EOPNOTSUPP;
		}
		if (adapter->ch_config.state != __IAVF_TC_INVALID) {
			dev_err(&adapter->pdev->dev, "TC configuration already exists\n");
			return -EINVAL;
		}

		ret = iavf_validate_ch_config(adapter, mqprio_qopt);
		if (ret)
			return ret;
		 
		if (adapter->num_tc == num_tc)
			return 0;
		adapter->num_tc = num_tc;

		for (i = 0; i < IAVF_MAX_TRAFFIC_CLASS; i++) {
			if (i < num_tc) {
				adapter->ch_config.ch_info[i].count =
					mqprio_qopt->qopt.count[i];
				adapter->ch_config.ch_info[i].offset =
					mqprio_qopt->qopt.offset[i];
				total_qps += mqprio_qopt->qopt.count[i];
				max_tx_rate = mqprio_qopt->max_rate[i];
				 
				max_tx_rate = div_u64(max_tx_rate,
						      IAVF_MBPS_DIVISOR);
				adapter->ch_config.ch_info[i].max_tx_rate =
					max_tx_rate;
			} else {
				adapter->ch_config.ch_info[i].count = 1;
				adapter->ch_config.ch_info[i].offset = 0;
			}
		}

		 

		adapter->orig_num_active_queues = adapter->num_active_queues;

		 
		adapter->ch_config.total_qps = total_qps;

		netif_tx_stop_all_queues(netdev);
		netif_tx_disable(netdev);
		adapter->aq_required |= IAVF_FLAG_AQ_ENABLE_CHANNELS;
		netdev_reset_tc(netdev);
		 
		netdev_set_num_tc(adapter->netdev, num_tc);
		for (i = 0; i < IAVF_MAX_TRAFFIC_CLASS; i++) {
			u16 qcount = mqprio_qopt->qopt.count[i];
			u16 qoffset = mqprio_qopt->qopt.offset[i];

			if (i < num_tc)
				netdev_set_tc_queue(netdev, netdev_tc++, qcount,
						    qoffset);
		}
	}
exit:
	if (test_bit(__IAVF_IN_REMOVE_TASK, &adapter->crit_section))
		return 0;

	netif_set_real_num_rx_queues(netdev, total_qps);
	netif_set_real_num_tx_queues(netdev, total_qps);

	return ret;
}

 
static int iavf_parse_cls_flower(struct iavf_adapter *adapter,
				 struct flow_cls_offload *f,
				 struct iavf_cloud_filter *filter)
{
	struct flow_rule *rule = flow_cls_offload_flow_rule(f);
	struct flow_dissector *dissector = rule->match.dissector;
	u16 n_proto_mask = 0;
	u16 n_proto_key = 0;
	u8 field_flags = 0;
	u16 addr_type = 0;
	u16 n_proto = 0;
	int i = 0;
	struct virtchnl_filter *vf = &filter->f;

	if (dissector->used_keys &
	    ~(BIT_ULL(FLOW_DISSECTOR_KEY_CONTROL) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_BASIC) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_ETH_ADDRS) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_VLAN) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_IPV4_ADDRS) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_IPV6_ADDRS) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_PORTS) |
	      BIT_ULL(FLOW_DISSECTOR_KEY_ENC_KEYID))) {
		dev_err(&adapter->pdev->dev, "Unsupported key used: 0x%llx\n",
			dissector->used_keys);
		return -EOPNOTSUPP;
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ENC_KEYID)) {
		struct flow_match_enc_keyid match;

		flow_rule_match_enc_keyid(rule, &match);
		if (match.mask->keyid != 0)
			field_flags |= IAVF_CLOUD_FIELD_TEN_ID;
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
		n_proto = n_proto_key & n_proto_mask;
		if (n_proto != ETH_P_IP && n_proto != ETH_P_IPV6)
			return -EINVAL;
		if (n_proto == ETH_P_IPV6) {
			 
			vf->flow_type = VIRTCHNL_TCP_V6_FLOW;
		}

		if (match.key->ip_proto != IPPROTO_TCP) {
			dev_info(&adapter->pdev->dev, "Only TCP transport is supported\n");
			return -EINVAL;
		}
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ETH_ADDRS)) {
		struct flow_match_eth_addrs match;

		flow_rule_match_eth_addrs(rule, &match);

		 
		if (!is_zero_ether_addr(match.mask->dst)) {
			if (is_broadcast_ether_addr(match.mask->dst)) {
				field_flags |= IAVF_CLOUD_FIELD_OMAC;
			} else {
				dev_err(&adapter->pdev->dev, "Bad ether dest mask %pM\n",
					match.mask->dst);
				return -EINVAL;
			}
		}

		if (!is_zero_ether_addr(match.mask->src)) {
			if (is_broadcast_ether_addr(match.mask->src)) {
				field_flags |= IAVF_CLOUD_FIELD_IMAC;
			} else {
				dev_err(&adapter->pdev->dev, "Bad ether src mask %pM\n",
					match.mask->src);
				return -EINVAL;
			}
		}

		if (!is_zero_ether_addr(match.key->dst))
			if (is_valid_ether_addr(match.key->dst) ||
			    is_multicast_ether_addr(match.key->dst)) {
				 
				for (i = 0; i < ETH_ALEN; i++)
					vf->mask.tcp_spec.dst_mac[i] |= 0xff;
				ether_addr_copy(vf->data.tcp_spec.dst_mac,
						match.key->dst);
			}

		if (!is_zero_ether_addr(match.key->src))
			if (is_valid_ether_addr(match.key->src) ||
			    is_multicast_ether_addr(match.key->src)) {
				 
				for (i = 0; i < ETH_ALEN; i++)
					vf->mask.tcp_spec.src_mac[i] |= 0xff;
				ether_addr_copy(vf->data.tcp_spec.src_mac,
						match.key->src);
		}
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_VLAN)) {
		struct flow_match_vlan match;

		flow_rule_match_vlan(rule, &match);
		if (match.mask->vlan_id) {
			if (match.mask->vlan_id == VLAN_VID_MASK) {
				field_flags |= IAVF_CLOUD_FIELD_IVLAN;
			} else {
				dev_err(&adapter->pdev->dev, "Bad vlan mask %u\n",
					match.mask->vlan_id);
				return -EINVAL;
			}
		}
		vf->mask.tcp_spec.vlan_id |= cpu_to_be16(0xffff);
		vf->data.tcp_spec.vlan_id = cpu_to_be16(match.key->vlan_id);
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
				field_flags |= IAVF_CLOUD_FIELD_IIP;
			} else {
				dev_err(&adapter->pdev->dev, "Bad ip dst mask 0x%08x\n",
					be32_to_cpu(match.mask->dst));
				return -EINVAL;
			}
		}

		if (match.mask->src) {
			if (match.mask->src == cpu_to_be32(0xffffffff)) {
				field_flags |= IAVF_CLOUD_FIELD_IIP;
			} else {
				dev_err(&adapter->pdev->dev, "Bad ip src mask 0x%08x\n",
					be32_to_cpu(match.mask->src));
				return -EINVAL;
			}
		}

		if (field_flags & IAVF_CLOUD_FIELD_TEN_ID) {
			dev_info(&adapter->pdev->dev, "Tenant id not allowed for ip filter\n");
			return -EINVAL;
		}
		if (match.key->dst) {
			vf->mask.tcp_spec.dst_ip[0] |= cpu_to_be32(0xffffffff);
			vf->data.tcp_spec.dst_ip[0] = match.key->dst;
		}
		if (match.key->src) {
			vf->mask.tcp_spec.src_ip[0] |= cpu_to_be32(0xffffffff);
			vf->data.tcp_spec.src_ip[0] = match.key->src;
		}
	}

	if (addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS) {
		struct flow_match_ipv6_addrs match;

		flow_rule_match_ipv6_addrs(rule, &match);

		 
		if (ipv6_addr_any(&match.mask->dst)) {
			dev_err(&adapter->pdev->dev, "Bad ipv6 dst mask 0x%02x\n",
				IPV6_ADDR_ANY);
			return -EINVAL;
		}

		 
		if (ipv6_addr_loopback(&match.key->dst) ||
		    ipv6_addr_loopback(&match.key->src)) {
			dev_err(&adapter->pdev->dev,
				"ipv6 addr should not be loopback\n");
			return -EINVAL;
		}
		if (!ipv6_addr_any(&match.mask->dst) ||
		    !ipv6_addr_any(&match.mask->src))
			field_flags |= IAVF_CLOUD_FIELD_IIP;

		for (i = 0; i < 4; i++)
			vf->mask.tcp_spec.dst_ip[i] |= cpu_to_be32(0xffffffff);
		memcpy(&vf->data.tcp_spec.dst_ip, &match.key->dst.s6_addr32,
		       sizeof(vf->data.tcp_spec.dst_ip));
		for (i = 0; i < 4; i++)
			vf->mask.tcp_spec.src_ip[i] |= cpu_to_be32(0xffffffff);
		memcpy(&vf->data.tcp_spec.src_ip, &match.key->src.s6_addr32,
		       sizeof(vf->data.tcp_spec.src_ip));
	}
	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_PORTS)) {
		struct flow_match_ports match;

		flow_rule_match_ports(rule, &match);
		if (match.mask->src) {
			if (match.mask->src == cpu_to_be16(0xffff)) {
				field_flags |= IAVF_CLOUD_FIELD_IIP;
			} else {
				dev_err(&adapter->pdev->dev, "Bad src port mask %u\n",
					be16_to_cpu(match.mask->src));
				return -EINVAL;
			}
		}

		if (match.mask->dst) {
			if (match.mask->dst == cpu_to_be16(0xffff)) {
				field_flags |= IAVF_CLOUD_FIELD_IIP;
			} else {
				dev_err(&adapter->pdev->dev, "Bad dst port mask %u\n",
					be16_to_cpu(match.mask->dst));
				return -EINVAL;
			}
		}
		if (match.key->dst) {
			vf->mask.tcp_spec.dst_port |= cpu_to_be16(0xffff);
			vf->data.tcp_spec.dst_port = match.key->dst;
		}

		if (match.key->src) {
			vf->mask.tcp_spec.src_port |= cpu_to_be16(0xffff);
			vf->data.tcp_spec.src_port = match.key->src;
		}
	}
	vf->field_flags = field_flags;

	return 0;
}

 
static int iavf_handle_tclass(struct iavf_adapter *adapter, u32 tc,
			      struct iavf_cloud_filter *filter)
{
	if (tc == 0)
		return 0;
	if (tc < adapter->num_tc) {
		if (!filter->f.data.tcp_spec.dst_port) {
			dev_err(&adapter->pdev->dev,
				"Specify destination port to redirect to traffic class other than TC0\n");
			return -EINVAL;
		}
	}
	 
	filter->f.action = VIRTCHNL_ACTION_TC_REDIRECT;
	filter->f.action_meta = tc;
	return 0;
}

 
static struct iavf_cloud_filter *iavf_find_cf(struct iavf_adapter *adapter,
					      unsigned long *cookie)
{
	struct iavf_cloud_filter *filter = NULL;

	if (!cookie)
		return NULL;

	list_for_each_entry(filter, &adapter->cloud_filter_list, list) {
		if (!memcmp(cookie, &filter->cookie, sizeof(filter->cookie)))
			return filter;
	}
	return NULL;
}

 
static int iavf_configure_clsflower(struct iavf_adapter *adapter,
				    struct flow_cls_offload *cls_flower)
{
	int tc = tc_classid_to_hwtc(adapter->netdev, cls_flower->classid);
	struct iavf_cloud_filter *filter = NULL;
	int err = -EINVAL, count = 50;

	if (tc < 0) {
		dev_err(&adapter->pdev->dev, "Invalid traffic class\n");
		return -EINVAL;
	}

	filter = kzalloc(sizeof(*filter), GFP_KERNEL);
	if (!filter)
		return -ENOMEM;

	while (!mutex_trylock(&adapter->crit_lock)) {
		if (--count == 0) {
			kfree(filter);
			return err;
		}
		udelay(1);
	}

	filter->cookie = cls_flower->cookie;

	 
	spin_lock_bh(&adapter->cloud_filter_list_lock);
	if (iavf_find_cf(adapter, &cls_flower->cookie)) {
		dev_err(&adapter->pdev->dev, "Failed to add TC Flower filter, it already exists\n");
		err = -EEXIST;
		goto spin_unlock;
	}
	spin_unlock_bh(&adapter->cloud_filter_list_lock);

	 
	memset(&filter->f.mask.tcp_spec, 0, sizeof(struct virtchnl_l4_spec));
	 
	filter->f.flow_type = VIRTCHNL_TCP_V4_FLOW;
	err = iavf_parse_cls_flower(adapter, cls_flower, filter);
	if (err)
		goto err;

	err = iavf_handle_tclass(adapter, tc, filter);
	if (err)
		goto err;

	 
	spin_lock_bh(&adapter->cloud_filter_list_lock);
	list_add_tail(&filter->list, &adapter->cloud_filter_list);
	adapter->num_cloud_filters++;
	filter->add = true;
	adapter->aq_required |= IAVF_FLAG_AQ_ADD_CLOUD_FILTER;
spin_unlock:
	spin_unlock_bh(&adapter->cloud_filter_list_lock);
err:
	if (err)
		kfree(filter);

	mutex_unlock(&adapter->crit_lock);
	return err;
}

 
static int iavf_delete_clsflower(struct iavf_adapter *adapter,
				 struct flow_cls_offload *cls_flower)
{
	struct iavf_cloud_filter *filter = NULL;
	int err = 0;

	spin_lock_bh(&adapter->cloud_filter_list_lock);
	filter = iavf_find_cf(adapter, &cls_flower->cookie);
	if (filter) {
		filter->del = true;
		adapter->aq_required |= IAVF_FLAG_AQ_DEL_CLOUD_FILTER;
	} else {
		err = -EINVAL;
	}
	spin_unlock_bh(&adapter->cloud_filter_list_lock);

	return err;
}

 
static int iavf_setup_tc_cls_flower(struct iavf_adapter *adapter,
				    struct flow_cls_offload *cls_flower)
{
	switch (cls_flower->command) {
	case FLOW_CLS_REPLACE:
		return iavf_configure_clsflower(adapter, cls_flower);
	case FLOW_CLS_DESTROY:
		return iavf_delete_clsflower(adapter, cls_flower);
	case FLOW_CLS_STATS:
		return -EOPNOTSUPP;
	default:
		return -EOPNOTSUPP;
	}
}

 
static int iavf_setup_tc_block_cb(enum tc_setup_type type, void *type_data,
				  void *cb_priv)
{
	struct iavf_adapter *adapter = cb_priv;

	if (!tc_cls_can_offload_and_chain0(adapter->netdev, type_data))
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		return iavf_setup_tc_cls_flower(cb_priv, type_data);
	default:
		return -EOPNOTSUPP;
	}
}

static LIST_HEAD(iavf_block_cb_list);

 
static int iavf_setup_tc(struct net_device *netdev, enum tc_setup_type type,
			 void *type_data)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);

	switch (type) {
	case TC_SETUP_QDISC_MQPRIO:
		return __iavf_setup_tc(netdev, type_data);
	case TC_SETUP_BLOCK:
		return flow_block_cb_setup_simple(type_data,
						  &iavf_block_cb_list,
						  iavf_setup_tc_block_cb,
						  adapter, adapter, true);
	default:
		return -EOPNOTSUPP;
	}
}

 
static void iavf_restore_fdir_filters(struct iavf_adapter *adapter)
{
	struct iavf_fdir_fltr *f;

	spin_lock_bh(&adapter->fdir_fltr_lock);
	list_for_each_entry(f, &adapter->fdir_list_head, list) {
		if (f->state == IAVF_FDIR_FLTR_DIS_REQUEST) {
			 
			f->state = IAVF_FDIR_FLTR_ACTIVE;
		} else if (f->state == IAVF_FDIR_FLTR_DIS_PENDING ||
			   f->state == IAVF_FDIR_FLTR_INACTIVE) {
			 
			f->state = IAVF_FDIR_FLTR_ADD_REQUEST;
			adapter->aq_required |= IAVF_FLAG_AQ_ADD_FDIR_FILTER;
		}
	}
	spin_unlock_bh(&adapter->fdir_fltr_lock);
}

 
static int iavf_open(struct net_device *netdev)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);
	int err;

	if (adapter->flags & IAVF_FLAG_PF_COMMS_FAILED) {
		dev_err(&adapter->pdev->dev, "Unable to open device due to PF driver failure.\n");
		return -EIO;
	}

	while (!mutex_trylock(&adapter->crit_lock)) {
		 
		if (adapter->state == __IAVF_INIT_CONFIG_ADAPTER)
			return -EBUSY;

		usleep_range(500, 1000);
	}

	if (adapter->state != __IAVF_DOWN) {
		err = -EBUSY;
		goto err_unlock;
	}

	if (adapter->state == __IAVF_RUNNING &&
	    !test_bit(__IAVF_VSI_DOWN, adapter->vsi.state)) {
		dev_dbg(&adapter->pdev->dev, "VF is already open.\n");
		err = 0;
		goto err_unlock;
	}

	 
	err = iavf_setup_all_tx_resources(adapter);
	if (err)
		goto err_setup_tx;

	 
	err = iavf_setup_all_rx_resources(adapter);
	if (err)
		goto err_setup_rx;

	 
	err = iavf_request_traffic_irqs(adapter, netdev->name);
	if (err)
		goto err_req_irq;

	spin_lock_bh(&adapter->mac_vlan_list_lock);

	iavf_add_filter(adapter, adapter->hw.mac.addr);

	spin_unlock_bh(&adapter->mac_vlan_list_lock);

	 
	iavf_restore_filters(adapter);
	iavf_restore_fdir_filters(adapter);

	iavf_configure(adapter);

	iavf_up_complete(adapter);

	iavf_irq_enable(adapter, true);

	mutex_unlock(&adapter->crit_lock);

	return 0;

err_req_irq:
	iavf_down(adapter);
	iavf_free_traffic_irqs(adapter);
err_setup_rx:
	iavf_free_all_rx_resources(adapter);
err_setup_tx:
	iavf_free_all_tx_resources(adapter);
err_unlock:
	mutex_unlock(&adapter->crit_lock);

	return err;
}

 
static int iavf_close(struct net_device *netdev)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);
	u64 aq_to_restore;
	int status;

	mutex_lock(&adapter->crit_lock);

	if (adapter->state <= __IAVF_DOWN_PENDING) {
		mutex_unlock(&adapter->crit_lock);
		return 0;
	}

	set_bit(__IAVF_VSI_DOWN, adapter->vsi.state);
	if (CLIENT_ENABLED(adapter))
		adapter->flags |= IAVF_FLAG_CLIENT_NEEDS_CLOSE;
	 
	aq_to_restore = adapter->aq_required;
	adapter->aq_required &= IAVF_FLAG_AQ_GET_CONFIG;

	 
	aq_to_restore &= ~(IAVF_FLAG_AQ_GET_CONFIG		|
			   IAVF_FLAG_AQ_ENABLE_QUEUES		|
			   IAVF_FLAG_AQ_CONFIGURE_QUEUES	|
			   IAVF_FLAG_AQ_ADD_VLAN_FILTER		|
			   IAVF_FLAG_AQ_ADD_MAC_FILTER		|
			   IAVF_FLAG_AQ_ADD_CLOUD_FILTER	|
			   IAVF_FLAG_AQ_ADD_FDIR_FILTER		|
			   IAVF_FLAG_AQ_ADD_ADV_RSS_CFG);

	iavf_down(adapter);
	iavf_change_state(adapter, __IAVF_DOWN_PENDING);
	iavf_free_traffic_irqs(adapter);

	mutex_unlock(&adapter->crit_lock);

	 

	status = wait_event_timeout(adapter->down_waitqueue,
				    adapter->state == __IAVF_DOWN,
				    msecs_to_jiffies(500));
	if (!status)
		netdev_warn(netdev, "Device resources not yet released\n");

	mutex_lock(&adapter->crit_lock);
	adapter->aq_required |= aq_to_restore;
	mutex_unlock(&adapter->crit_lock);
	return 0;
}

 
static int iavf_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);
	int ret = 0;

	netdev_dbg(netdev, "changing MTU from %d to %d\n",
		   netdev->mtu, new_mtu);
	netdev->mtu = new_mtu;
	if (CLIENT_ENABLED(adapter)) {
		iavf_notify_client_l2_params(&adapter->vsi);
		adapter->flags |= IAVF_FLAG_SERVICE_CLIENT_REQUESTED;
	}

	if (netif_running(netdev)) {
		iavf_schedule_reset(adapter, IAVF_FLAG_RESET_NEEDED);
		ret = iavf_wait_for_reset(adapter);
		if (ret < 0)
			netdev_warn(netdev, "MTU change interrupted waiting for reset");
		else if (ret)
			netdev_warn(netdev, "MTU change timed out waiting for reset");
	}

	return ret;
}

 
static void iavf_disable_fdir(struct iavf_adapter *adapter)
{
	struct iavf_fdir_fltr *fdir, *fdirtmp;
	bool del_filters = false;

	adapter->flags &= ~IAVF_FLAG_FDIR_ENABLED;

	 
	spin_lock_bh(&adapter->fdir_fltr_lock);
	list_for_each_entry_safe(fdir, fdirtmp, &adapter->fdir_list_head,
				 list) {
		if (fdir->state == IAVF_FDIR_FLTR_ADD_REQUEST ||
		    fdir->state == IAVF_FDIR_FLTR_INACTIVE) {
			 
			list_del(&fdir->list);
			kfree(fdir);
			adapter->fdir_active_fltr--;
		} else if (fdir->state == IAVF_FDIR_FLTR_ADD_PENDING ||
			   fdir->state == IAVF_FDIR_FLTR_DIS_REQUEST ||
			   fdir->state == IAVF_FDIR_FLTR_ACTIVE) {
			 
			fdir->state = IAVF_FDIR_FLTR_DEL_REQUEST;
			del_filters = true;
		} else if (fdir->state == IAVF_FDIR_FLTR_DIS_PENDING) {
			 
			fdir->state = IAVF_FDIR_FLTR_DEL_PENDING;
		}
	}
	spin_unlock_bh(&adapter->fdir_fltr_lock);

	if (del_filters) {
		adapter->aq_required |= IAVF_FLAG_AQ_DEL_FDIR_FILTER;
		mod_delayed_work(adapter->wq, &adapter->watchdog_task, 0);
	}
}

#define NETIF_VLAN_OFFLOAD_FEATURES	(NETIF_F_HW_VLAN_CTAG_RX | \
					 NETIF_F_HW_VLAN_CTAG_TX | \
					 NETIF_F_HW_VLAN_STAG_RX | \
					 NETIF_F_HW_VLAN_STAG_TX)

 
static int iavf_set_features(struct net_device *netdev,
			     netdev_features_t features)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);

	 
	if ((netdev->features & NETIF_VLAN_OFFLOAD_FEATURES) ^
	    (features & NETIF_VLAN_OFFLOAD_FEATURES))
		iavf_set_vlan_offload_features(adapter, netdev->features,
					       features);

	if ((netdev->features & NETIF_F_NTUPLE) ^ (features & NETIF_F_NTUPLE)) {
		if (features & NETIF_F_NTUPLE)
			adapter->flags |= IAVF_FLAG_FDIR_ENABLED;
		else
			iavf_disable_fdir(adapter);
	}

	return 0;
}

 
static netdev_features_t iavf_features_check(struct sk_buff *skb,
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

 
static netdev_features_t
iavf_get_netdev_vlan_hw_features(struct iavf_adapter *adapter)
{
	netdev_features_t hw_features = 0;

	if (!adapter->vf_res || !adapter->vf_res->vf_cap_flags)
		return hw_features;

	 
	if (VLAN_ALLOWED(adapter)) {
		hw_features |= (NETIF_F_HW_VLAN_CTAG_TX |
				NETIF_F_HW_VLAN_CTAG_RX);
	} else if (VLAN_V2_ALLOWED(adapter)) {
		struct virtchnl_vlan_caps *vlan_v2_caps =
			&adapter->vlan_v2_caps;
		struct virtchnl_vlan_supported_caps *stripping_support =
			&vlan_v2_caps->offloads.stripping_support;
		struct virtchnl_vlan_supported_caps *insertion_support =
			&vlan_v2_caps->offloads.insertion_support;

		if (stripping_support->outer != VIRTCHNL_VLAN_UNSUPPORTED &&
		    stripping_support->outer & VIRTCHNL_VLAN_TOGGLE) {
			if (stripping_support->outer &
			    VIRTCHNL_VLAN_ETHERTYPE_8100)
				hw_features |= NETIF_F_HW_VLAN_CTAG_RX;
			if (stripping_support->outer &
			    VIRTCHNL_VLAN_ETHERTYPE_88A8)
				hw_features |= NETIF_F_HW_VLAN_STAG_RX;
		} else if (stripping_support->inner !=
			   VIRTCHNL_VLAN_UNSUPPORTED &&
			   stripping_support->inner & VIRTCHNL_VLAN_TOGGLE) {
			if (stripping_support->inner &
			    VIRTCHNL_VLAN_ETHERTYPE_8100)
				hw_features |= NETIF_F_HW_VLAN_CTAG_RX;
		}

		if (insertion_support->outer != VIRTCHNL_VLAN_UNSUPPORTED &&
		    insertion_support->outer & VIRTCHNL_VLAN_TOGGLE) {
			if (insertion_support->outer &
			    VIRTCHNL_VLAN_ETHERTYPE_8100)
				hw_features |= NETIF_F_HW_VLAN_CTAG_TX;
			if (insertion_support->outer &
			    VIRTCHNL_VLAN_ETHERTYPE_88A8)
				hw_features |= NETIF_F_HW_VLAN_STAG_TX;
		} else if (insertion_support->inner &&
			   insertion_support->inner & VIRTCHNL_VLAN_TOGGLE) {
			if (insertion_support->inner &
			    VIRTCHNL_VLAN_ETHERTYPE_8100)
				hw_features |= NETIF_F_HW_VLAN_CTAG_TX;
		}
	}

	return hw_features;
}

 
static netdev_features_t
iavf_get_netdev_vlan_features(struct iavf_adapter *adapter)
{
	netdev_features_t features = 0;

	if (!adapter->vf_res || !adapter->vf_res->vf_cap_flags)
		return features;

	if (VLAN_ALLOWED(adapter)) {
		features |= NETIF_F_HW_VLAN_CTAG_FILTER |
			NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_CTAG_TX;
	} else if (VLAN_V2_ALLOWED(adapter)) {
		struct virtchnl_vlan_caps *vlan_v2_caps =
			&adapter->vlan_v2_caps;
		struct virtchnl_vlan_supported_caps *filtering_support =
			&vlan_v2_caps->filtering.filtering_support;
		struct virtchnl_vlan_supported_caps *stripping_support =
			&vlan_v2_caps->offloads.stripping_support;
		struct virtchnl_vlan_supported_caps *insertion_support =
			&vlan_v2_caps->offloads.insertion_support;
		u32 ethertype_init;

		 
		ethertype_init = vlan_v2_caps->offloads.ethertype_init;
		if (stripping_support->outer != VIRTCHNL_VLAN_UNSUPPORTED) {
			if (stripping_support->outer &
			    VIRTCHNL_VLAN_ETHERTYPE_8100 &&
			    ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_8100)
				features |= NETIF_F_HW_VLAN_CTAG_RX;
			else if (stripping_support->outer &
				 VIRTCHNL_VLAN_ETHERTYPE_88A8 &&
				 ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_88A8)
				features |= NETIF_F_HW_VLAN_STAG_RX;
		} else if (stripping_support->inner !=
			   VIRTCHNL_VLAN_UNSUPPORTED) {
			if (stripping_support->inner &
			    VIRTCHNL_VLAN_ETHERTYPE_8100 &&
			    ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_8100)
				features |= NETIF_F_HW_VLAN_CTAG_RX;
		}

		 
		if (insertion_support->outer != VIRTCHNL_VLAN_UNSUPPORTED) {
			if (insertion_support->outer &
			    VIRTCHNL_VLAN_ETHERTYPE_8100 &&
			    ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_8100)
				features |= NETIF_F_HW_VLAN_CTAG_TX;
			else if (insertion_support->outer &
				 VIRTCHNL_VLAN_ETHERTYPE_88A8 &&
				 ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_88A8)
				features |= NETIF_F_HW_VLAN_STAG_TX;
		} else if (insertion_support->inner !=
			   VIRTCHNL_VLAN_UNSUPPORTED) {
			if (insertion_support->inner &
			    VIRTCHNL_VLAN_ETHERTYPE_8100 &&
			    ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_8100)
				features |= NETIF_F_HW_VLAN_CTAG_TX;
		}

		 
		ethertype_init = vlan_v2_caps->filtering.ethertype_init;
		if (filtering_support->outer != VIRTCHNL_VLAN_UNSUPPORTED) {
			if (filtering_support->outer &
			    VIRTCHNL_VLAN_ETHERTYPE_8100 &&
			    ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_8100)
				features |= NETIF_F_HW_VLAN_CTAG_FILTER;
			if (filtering_support->outer &
			    VIRTCHNL_VLAN_ETHERTYPE_88A8 &&
			    ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_88A8)
				features |= NETIF_F_HW_VLAN_STAG_FILTER;
		} else if (filtering_support->inner !=
			   VIRTCHNL_VLAN_UNSUPPORTED) {
			if (filtering_support->inner &
			    VIRTCHNL_VLAN_ETHERTYPE_8100 &&
			    ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_8100)
				features |= NETIF_F_HW_VLAN_CTAG_FILTER;
			if (filtering_support->inner &
			    VIRTCHNL_VLAN_ETHERTYPE_88A8 &&
			    ethertype_init & VIRTCHNL_VLAN_ETHERTYPE_88A8)
				features |= NETIF_F_HW_VLAN_STAG_FILTER;
		}
	}

	return features;
}

#define IAVF_NETDEV_VLAN_FEATURE_ALLOWED(requested, allowed, feature_bit) \
	(!(((requested) & (feature_bit)) && \
	   !((allowed) & (feature_bit))))

 
static netdev_features_t
iavf_fix_netdev_vlan_features(struct iavf_adapter *adapter,
			      netdev_features_t requested_features)
{
	netdev_features_t allowed_features;

	allowed_features = iavf_get_netdev_vlan_hw_features(adapter) |
		iavf_get_netdev_vlan_features(adapter);

	if (!IAVF_NETDEV_VLAN_FEATURE_ALLOWED(requested_features,
					      allowed_features,
					      NETIF_F_HW_VLAN_CTAG_TX))
		requested_features &= ~NETIF_F_HW_VLAN_CTAG_TX;

	if (!IAVF_NETDEV_VLAN_FEATURE_ALLOWED(requested_features,
					      allowed_features,
					      NETIF_F_HW_VLAN_CTAG_RX))
		requested_features &= ~NETIF_F_HW_VLAN_CTAG_RX;

	if (!IAVF_NETDEV_VLAN_FEATURE_ALLOWED(requested_features,
					      allowed_features,
					      NETIF_F_HW_VLAN_STAG_TX))
		requested_features &= ~NETIF_F_HW_VLAN_STAG_TX;
	if (!IAVF_NETDEV_VLAN_FEATURE_ALLOWED(requested_features,
					      allowed_features,
					      NETIF_F_HW_VLAN_STAG_RX))
		requested_features &= ~NETIF_F_HW_VLAN_STAG_RX;

	if (!IAVF_NETDEV_VLAN_FEATURE_ALLOWED(requested_features,
					      allowed_features,
					      NETIF_F_HW_VLAN_CTAG_FILTER))
		requested_features &= ~NETIF_F_HW_VLAN_CTAG_FILTER;

	if (!IAVF_NETDEV_VLAN_FEATURE_ALLOWED(requested_features,
					      allowed_features,
					      NETIF_F_HW_VLAN_STAG_FILTER))
		requested_features &= ~NETIF_F_HW_VLAN_STAG_FILTER;

	if ((requested_features &
	     (NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_CTAG_TX)) &&
	    (requested_features &
	     (NETIF_F_HW_VLAN_STAG_RX | NETIF_F_HW_VLAN_STAG_TX)) &&
	    adapter->vlan_v2_caps.offloads.ethertype_match ==
	    VIRTCHNL_ETHERTYPE_STRIPPING_MATCHES_INSERTION) {
		netdev_warn(adapter->netdev, "cannot support CTAG and STAG VLAN stripping and/or insertion simultaneously since CTAG and STAG offloads are mutually exclusive, clearing STAG offload settings\n");
		requested_features &= ~(NETIF_F_HW_VLAN_STAG_RX |
					NETIF_F_HW_VLAN_STAG_TX);
	}

	return requested_features;
}

 
static netdev_features_t iavf_fix_features(struct net_device *netdev,
					   netdev_features_t features)
{
	struct iavf_adapter *adapter = netdev_priv(netdev);

	if (!FDIR_FLTR_SUPPORT(adapter))
		features &= ~NETIF_F_NTUPLE;

	return iavf_fix_netdev_vlan_features(adapter, features);
}

static const struct net_device_ops iavf_netdev_ops = {
	.ndo_open		= iavf_open,
	.ndo_stop		= iavf_close,
	.ndo_start_xmit		= iavf_xmit_frame,
	.ndo_set_rx_mode	= iavf_set_rx_mode,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= iavf_set_mac,
	.ndo_change_mtu		= iavf_change_mtu,
	.ndo_tx_timeout		= iavf_tx_timeout,
	.ndo_vlan_rx_add_vid	= iavf_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= iavf_vlan_rx_kill_vid,
	.ndo_features_check	= iavf_features_check,
	.ndo_fix_features	= iavf_fix_features,
	.ndo_set_features	= iavf_set_features,
	.ndo_setup_tc		= iavf_setup_tc,
};

 
static int iavf_check_reset_complete(struct iavf_hw *hw)
{
	u32 rstat;
	int i;

	for (i = 0; i < IAVF_RESET_WAIT_COMPLETE_COUNT; i++) {
		rstat = rd32(hw, IAVF_VFGEN_RSTAT) &
			     IAVF_VFGEN_RSTAT_VFR_STATE_MASK;
		if ((rstat == VIRTCHNL_VFR_VFACTIVE) ||
		    (rstat == VIRTCHNL_VFR_COMPLETED))
			return 0;
		usleep_range(10, 20);
	}
	return -EBUSY;
}

 
int iavf_process_config(struct iavf_adapter *adapter)
{
	struct virtchnl_vf_resource *vfres = adapter->vf_res;
	netdev_features_t hw_vlan_features, vlan_features;
	struct net_device *netdev = adapter->netdev;
	netdev_features_t hw_enc_features;
	netdev_features_t hw_features;

	hw_enc_features = NETIF_F_SG			|
			  NETIF_F_IP_CSUM		|
			  NETIF_F_IPV6_CSUM		|
			  NETIF_F_HIGHDMA		|
			  NETIF_F_SOFT_FEATURES	|
			  NETIF_F_TSO			|
			  NETIF_F_TSO_ECN		|
			  NETIF_F_TSO6			|
			  NETIF_F_SCTP_CRC		|
			  NETIF_F_RXHASH		|
			  NETIF_F_RXCSUM		|
			  0;

	 
	if (vfres->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ENCAP) {
		hw_enc_features |= NETIF_F_GSO_UDP_TUNNEL	|
				   NETIF_F_GSO_GRE		|
				   NETIF_F_GSO_GRE_CSUM		|
				   NETIF_F_GSO_IPXIP4		|
				   NETIF_F_GSO_IPXIP6		|
				   NETIF_F_GSO_UDP_TUNNEL_CSUM	|
				   NETIF_F_GSO_PARTIAL		|
				   0;

		if (!(vfres->vf_cap_flags &
		      VIRTCHNL_VF_OFFLOAD_ENCAP_CSUM))
			netdev->gso_partial_features |=
				NETIF_F_GSO_UDP_TUNNEL_CSUM;

		netdev->gso_partial_features |= NETIF_F_GSO_GRE_CSUM;
		netdev->hw_enc_features |= NETIF_F_TSO_MANGLEID;
		netdev->hw_enc_features |= hw_enc_features;
	}
	 
	netdev->vlan_features |= hw_enc_features | NETIF_F_TSO_MANGLEID;

	 
	hw_features = hw_enc_features;

	 
	hw_vlan_features = iavf_get_netdev_vlan_hw_features(adapter);

	 
	if (vfres->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADQ)
		hw_features |= NETIF_F_HW_TC;
	if (vfres->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_USO)
		hw_features |= NETIF_F_GSO_UDP_L4;

	netdev->hw_features |= hw_features | hw_vlan_features;
	vlan_features = iavf_get_netdev_vlan_features(adapter);

	netdev->features |= hw_features | vlan_features;

	if (vfres->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN)
		netdev->features |= NETIF_F_HW_VLAN_CTAG_FILTER;

	if (FDIR_FLTR_SUPPORT(adapter)) {
		netdev->hw_features |= NETIF_F_NTUPLE;
		netdev->features |= NETIF_F_NTUPLE;
		adapter->flags |= IAVF_FLAG_FDIR_ENABLED;
	}

	netdev->priv_flags |= IFF_UNICAST_FLT;

	 
	if (netdev->wanted_features) {
		if (!(netdev->wanted_features & NETIF_F_TSO) ||
		    netdev->mtu < 576)
			netdev->features &= ~NETIF_F_TSO;
		if (!(netdev->wanted_features & NETIF_F_TSO6) ||
		    netdev->mtu < 576)
			netdev->features &= ~NETIF_F_TSO6;
		if (!(netdev->wanted_features & NETIF_F_TSO_ECN))
			netdev->features &= ~NETIF_F_TSO_ECN;
		if (!(netdev->wanted_features & NETIF_F_GRO))
			netdev->features &= ~NETIF_F_GRO;
		if (!(netdev->wanted_features & NETIF_F_GSO))
			netdev->features &= ~NETIF_F_GSO;
	}

	return 0;
}

 
static int iavf_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct iavf_adapter *adapter = NULL;
	struct iavf_hw *hw = NULL;
	int err;

	err = pci_enable_device(pdev);
	if (err)
		return err;

	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (err) {
		dev_err(&pdev->dev,
			"DMA configuration failed: 0x%x\n", err);
		goto err_dma;
	}

	err = pci_request_regions(pdev, iavf_driver_name);
	if (err) {
		dev_err(&pdev->dev,
			"pci_request_regions failed 0x%x\n", err);
		goto err_pci_reg;
	}

	pci_set_master(pdev);

	netdev = alloc_etherdev_mq(sizeof(struct iavf_adapter),
				   IAVF_MAX_REQ_QUEUES);
	if (!netdev) {
		err = -ENOMEM;
		goto err_alloc_etherdev;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

	pci_set_drvdata(pdev, netdev);
	adapter = netdev_priv(netdev);

	adapter->netdev = netdev;
	adapter->pdev = pdev;

	hw = &adapter->hw;
	hw->back = adapter;

	adapter->wq = alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM,
					      iavf_driver_name);
	if (!adapter->wq) {
		err = -ENOMEM;
		goto err_alloc_wq;
	}

	adapter->msg_enable = BIT(DEFAULT_DEBUG_LEVEL_SHIFT) - 1;
	iavf_change_state(adapter, __IAVF_STARTUP);

	 
	pci_save_state(pdev);

	hw->hw_addr = ioremap(pci_resource_start(pdev, 0),
			      pci_resource_len(pdev, 0));
	if (!hw->hw_addr) {
		err = -EIO;
		goto err_ioremap;
	}
	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	pci_read_config_byte(pdev, PCI_REVISION_ID, &hw->revision_id);
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_device_id = pdev->subsystem_device;
	hw->bus.device = PCI_SLOT(pdev->devfn);
	hw->bus.func = PCI_FUNC(pdev->devfn);
	hw->bus.bus_id = pdev->bus->number;

	 
	mutex_init(&adapter->crit_lock);
	mutex_init(&adapter->client_lock);
	mutex_init(&hw->aq.asq_mutex);
	mutex_init(&hw->aq.arq_mutex);

	spin_lock_init(&adapter->mac_vlan_list_lock);
	spin_lock_init(&adapter->cloud_filter_list_lock);
	spin_lock_init(&adapter->fdir_fltr_lock);
	spin_lock_init(&adapter->adv_rss_lock);
	spin_lock_init(&adapter->current_netdev_promisc_flags_lock);

	INIT_LIST_HEAD(&adapter->mac_filter_list);
	INIT_LIST_HEAD(&adapter->vlan_filter_list);
	INIT_LIST_HEAD(&adapter->cloud_filter_list);
	INIT_LIST_HEAD(&adapter->fdir_list_head);
	INIT_LIST_HEAD(&adapter->adv_rss_list_head);

	INIT_WORK(&adapter->reset_task, iavf_reset_task);
	INIT_WORK(&adapter->adminq_task, iavf_adminq_task);
	INIT_WORK(&adapter->finish_config, iavf_finish_config);
	INIT_DELAYED_WORK(&adapter->watchdog_task, iavf_watchdog_task);
	INIT_DELAYED_WORK(&adapter->client_task, iavf_client_task);

	 
	init_waitqueue_head(&adapter->down_waitqueue);

	 
	init_waitqueue_head(&adapter->reset_waitqueue);

	 
	init_waitqueue_head(&adapter->vc_waitqueue);

	queue_delayed_work(adapter->wq, &adapter->watchdog_task,
			   msecs_to_jiffies(5 * (pdev->devfn & 0x07)));
	 
	return 0;

err_ioremap:
	destroy_workqueue(adapter->wq);
err_alloc_wq:
	free_netdev(netdev);
err_alloc_etherdev:
	pci_release_regions(pdev);
err_pci_reg:
err_dma:
	pci_disable_device(pdev);
	return err;
}

 
static int __maybe_unused iavf_suspend(struct device *dev_d)
{
	struct net_device *netdev = dev_get_drvdata(dev_d);
	struct iavf_adapter *adapter = netdev_priv(netdev);

	netif_device_detach(netdev);

	while (!mutex_trylock(&adapter->crit_lock))
		usleep_range(500, 1000);

	if (netif_running(netdev)) {
		rtnl_lock();
		iavf_down(adapter);
		rtnl_unlock();
	}
	iavf_free_misc_irq(adapter);
	iavf_reset_interrupt_capability(adapter);

	mutex_unlock(&adapter->crit_lock);

	return 0;
}

 
static int __maybe_unused iavf_resume(struct device *dev_d)
{
	struct pci_dev *pdev = to_pci_dev(dev_d);
	struct iavf_adapter *adapter;
	u32 err;

	adapter = iavf_pdev_to_adapter(pdev);

	pci_set_master(pdev);

	rtnl_lock();
	err = iavf_set_interrupt_capability(adapter);
	if (err) {
		rtnl_unlock();
		dev_err(&pdev->dev, "Cannot enable MSI-X interrupts.\n");
		return err;
	}
	err = iavf_request_misc_irq(adapter);
	rtnl_unlock();
	if (err) {
		dev_err(&pdev->dev, "Cannot get interrupt vector.\n");
		return err;
	}

	queue_work(adapter->wq, &adapter->reset_task);

	netif_device_attach(adapter->netdev);

	return err;
}

 
static void iavf_remove(struct pci_dev *pdev)
{
	struct iavf_fdir_fltr *fdir, *fdirtmp;
	struct iavf_vlan_filter *vlf, *vlftmp;
	struct iavf_cloud_filter *cf, *cftmp;
	struct iavf_adv_rss *rss, *rsstmp;
	struct iavf_mac_filter *f, *ftmp;
	struct iavf_adapter *adapter;
	struct net_device *netdev;
	struct iavf_hw *hw;
	int err;

	 
	netdev = pci_get_drvdata(pdev);
	if (!netdev)
		return;

	adapter = iavf_pdev_to_adapter(pdev);
	hw = &adapter->hw;

	if (test_and_set_bit(__IAVF_IN_REMOVE_TASK, &adapter->crit_section))
		return;

	 
	while (1) {
		mutex_lock(&adapter->crit_lock);
		if (adapter->state == __IAVF_RUNNING ||
		    adapter->state == __IAVF_DOWN ||
		    adapter->state == __IAVF_INIT_FAILED) {
			mutex_unlock(&adapter->crit_lock);
			break;
		}
		 
		if (adapter->state == __IAVF_REMOVE) {
			mutex_unlock(&adapter->crit_lock);
			return;
		}

		mutex_unlock(&adapter->crit_lock);
		usleep_range(500, 1000);
	}
	cancel_delayed_work_sync(&adapter->watchdog_task);
	cancel_work_sync(&adapter->finish_config);

	rtnl_lock();
	if (adapter->netdev_registered) {
		unregister_netdevice(netdev);
		adapter->netdev_registered = false;
	}
	rtnl_unlock();

	if (CLIENT_ALLOWED(adapter)) {
		err = iavf_lan_del_device(adapter);
		if (err)
			dev_warn(&pdev->dev, "Failed to delete client device: %d\n",
				 err);
	}

	mutex_lock(&adapter->crit_lock);
	dev_info(&adapter->pdev->dev, "Removing device\n");
	iavf_change_state(adapter, __IAVF_REMOVE);

	iavf_request_reset(adapter);
	msleep(50);
	 
	if (!iavf_asq_done(hw)) {
		iavf_request_reset(adapter);
		msleep(50);
	}

	iavf_misc_irq_disable(adapter);
	 
	cancel_work_sync(&adapter->reset_task);
	cancel_delayed_work_sync(&adapter->watchdog_task);
	cancel_work_sync(&adapter->adminq_task);
	cancel_delayed_work_sync(&adapter->client_task);

	adapter->aq_required = 0;
	adapter->flags &= ~IAVF_FLAG_REINIT_ITR_NEEDED;

	iavf_free_all_tx_resources(adapter);
	iavf_free_all_rx_resources(adapter);
	iavf_free_misc_irq(adapter);

	iavf_reset_interrupt_capability(adapter);
	iavf_free_q_vectors(adapter);

	iavf_free_rss(adapter);

	if (hw->aq.asq.count)
		iavf_shutdown_adminq(hw);

	 
	mutex_destroy(&hw->aq.arq_mutex);
	mutex_destroy(&hw->aq.asq_mutex);
	mutex_destroy(&adapter->client_lock);
	mutex_unlock(&adapter->crit_lock);
	mutex_destroy(&adapter->crit_lock);

	iounmap(hw->hw_addr);
	pci_release_regions(pdev);
	iavf_free_queues(adapter);
	kfree(adapter->vf_res);
	spin_lock_bh(&adapter->mac_vlan_list_lock);
	 
	list_for_each_entry_safe(f, ftmp, &adapter->mac_filter_list, list) {
		list_del(&f->list);
		kfree(f);
	}
	list_for_each_entry_safe(vlf, vlftmp, &adapter->vlan_filter_list,
				 list) {
		list_del(&vlf->list);
		kfree(vlf);
	}

	spin_unlock_bh(&adapter->mac_vlan_list_lock);

	spin_lock_bh(&adapter->cloud_filter_list_lock);
	list_for_each_entry_safe(cf, cftmp, &adapter->cloud_filter_list, list) {
		list_del(&cf->list);
		kfree(cf);
	}
	spin_unlock_bh(&adapter->cloud_filter_list_lock);

	spin_lock_bh(&adapter->fdir_fltr_lock);
	list_for_each_entry_safe(fdir, fdirtmp, &adapter->fdir_list_head, list) {
		list_del(&fdir->list);
		kfree(fdir);
	}
	spin_unlock_bh(&adapter->fdir_fltr_lock);

	spin_lock_bh(&adapter->adv_rss_lock);
	list_for_each_entry_safe(rss, rsstmp, &adapter->adv_rss_list_head,
				 list) {
		list_del(&rss->list);
		kfree(rss);
	}
	spin_unlock_bh(&adapter->adv_rss_lock);

	destroy_workqueue(adapter->wq);

	pci_set_drvdata(pdev, NULL);

	free_netdev(netdev);

	pci_disable_device(pdev);
}

 
static void iavf_shutdown(struct pci_dev *pdev)
{
	iavf_remove(pdev);

	if (system_state == SYSTEM_POWER_OFF)
		pci_set_power_state(pdev, PCI_D3hot);
}

static SIMPLE_DEV_PM_OPS(iavf_pm_ops, iavf_suspend, iavf_resume);

static struct pci_driver iavf_driver = {
	.name      = iavf_driver_name,
	.id_table  = iavf_pci_tbl,
	.probe     = iavf_probe,
	.remove    = iavf_remove,
	.driver.pm = &iavf_pm_ops,
	.shutdown  = iavf_shutdown,
};

 
static int __init iavf_init_module(void)
{
	pr_info("iavf: %s\n", iavf_driver_string);

	pr_info("%s\n", iavf_copyright);

	return pci_register_driver(&iavf_driver);
}

module_init(iavf_init_module);

 
static void __exit iavf_exit_module(void)
{
	pci_unregister_driver(&iavf_driver);
}

module_exit(iavf_exit_module);

 
