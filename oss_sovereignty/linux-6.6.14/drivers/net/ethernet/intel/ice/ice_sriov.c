
 

#include "ice.h"
#include "ice_vf_lib_private.h"
#include "ice_base.h"
#include "ice_lib.h"
#include "ice_fltr.h"
#include "ice_dcb_lib.h"
#include "ice_flow.h"
#include "ice_eswitch.h"
#include "ice_virtchnl_allowlist.h"
#include "ice_flex_pipe.h"
#include "ice_vf_vsi_vlan_ops.h"
#include "ice_vlan.h"

 
static void ice_free_vf_entries(struct ice_pf *pf)
{
	struct ice_vfs *vfs = &pf->vfs;
	struct hlist_node *tmp;
	struct ice_vf *vf;
	unsigned int bkt;

	 
	lockdep_assert_held(&vfs->table_lock);

	hash_for_each_safe(vfs->table, bkt, tmp, vf, entry) {
		hash_del_rcu(&vf->entry);
		ice_put_vf(vf);
	}
}

 
static void ice_free_vf_res(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;
	int i, last_vector_idx;

	 
	clear_bit(ICE_VF_STATE_INIT, vf->vf_states);
	ice_vf_fdir_exit(vf);
	 
	if (vf->ctrl_vsi_idx != ICE_NO_VSI)
		ice_vf_ctrl_vsi_release(vf);

	 
	if (vf->lan_vsi_idx != ICE_NO_VSI) {
		ice_vf_vsi_release(vf);
		vf->num_mac = 0;
	}

	last_vector_idx = vf->first_vector_idx + pf->vfs.num_msix_per - 1;

	 
	memset(&vf->mdd_tx_events, 0, sizeof(vf->mdd_tx_events));
	memset(&vf->mdd_rx_events, 0, sizeof(vf->mdd_rx_events));

	 
	for (i = vf->first_vector_idx; i <= last_vector_idx; i++) {
		wr32(&pf->hw, GLINT_DYN_CTL(i), GLINT_DYN_CTL_CLEARPBA_M);
		ice_flush(&pf->hw);
	}
	 
	clear_bit(ICE_VF_STATE_MC_PROMISC, vf->vf_states);
	clear_bit(ICE_VF_STATE_UC_PROMISC, vf->vf_states);
}

 
static void ice_dis_vf_mappings(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;
	struct ice_vsi *vsi;
	struct device *dev;
	int first, last, v;
	struct ice_hw *hw;

	hw = &pf->hw;
	vsi = ice_get_vf_vsi(vf);
	if (WARN_ON(!vsi))
		return;

	dev = ice_pf_to_dev(pf);
	wr32(hw, VPINT_ALLOC(vf->vf_id), 0);
	wr32(hw, VPINT_ALLOC_PCI(vf->vf_id), 0);

	first = vf->first_vector_idx;
	last = first + pf->vfs.num_msix_per - 1;
	for (v = first; v <= last; v++) {
		u32 reg;

		reg = (((1 << GLINT_VECT2FUNC_IS_PF_S) &
			GLINT_VECT2FUNC_IS_PF_M) |
		       ((hw->pf_id << GLINT_VECT2FUNC_PF_NUM_S) &
			GLINT_VECT2FUNC_PF_NUM_M));
		wr32(hw, GLINT_VECT2FUNC(v), reg);
	}

	if (vsi->tx_mapping_mode == ICE_VSI_MAP_CONTIG)
		wr32(hw, VPLAN_TX_QBASE(vf->vf_id), 0);
	else
		dev_err(dev, "Scattered mode for VF Tx queues is not yet implemented\n");

	if (vsi->rx_mapping_mode == ICE_VSI_MAP_CONTIG)
		wr32(hw, VPLAN_RX_QBASE(vf->vf_id), 0);
	else
		dev_err(dev, "Scattered mode for VF Rx queues is not yet implemented\n");
}

 
static int ice_sriov_free_msix_res(struct ice_pf *pf)
{
	if (!pf)
		return -EINVAL;

	pf->sriov_base_vector = 0;

	return 0;
}

 
void ice_free_vfs(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_vfs *vfs = &pf->vfs;
	struct ice_hw *hw = &pf->hw;
	struct ice_vf *vf;
	unsigned int bkt;

	if (!ice_has_vfs(pf))
		return;

	while (test_and_set_bit(ICE_VF_DIS, pf->state))
		usleep_range(1000, 2000);

	 
	if (!pci_vfs_assigned(pf->pdev))
		pci_disable_sriov(pf->pdev);
	else
		dev_warn(dev, "VFs are assigned - not disabling SR-IOV\n");

	mutex_lock(&vfs->table_lock);

	ice_eswitch_release(pf);

	ice_for_each_vf(pf, bkt, vf) {
		mutex_lock(&vf->cfg_lock);

		ice_dis_vf_qs(vf);

		if (test_bit(ICE_VF_STATE_INIT, vf->vf_states)) {
			 
			ice_dis_vf_mappings(vf);
			set_bit(ICE_VF_STATE_DIS, vf->vf_states);
			ice_free_vf_res(vf);
		}

		if (!pci_vfs_assigned(pf->pdev)) {
			u32 reg_idx, bit_idx;

			reg_idx = (hw->func_caps.vf_base_id + vf->vf_id) / 32;
			bit_idx = (hw->func_caps.vf_base_id + vf->vf_id) % 32;
			wr32(hw, GLGEN_VFLRSTAT(reg_idx), BIT(bit_idx));
		}

		 
		list_del(&vf->mbx_info.list_entry);

		mutex_unlock(&vf->cfg_lock);
	}

	if (ice_sriov_free_msix_res(pf))
		dev_err(dev, "Failed to free MSIX resources used by SR-IOV\n");

	vfs->num_qps_per = 0;
	ice_free_vf_entries(pf);

	mutex_unlock(&vfs->table_lock);

	clear_bit(ICE_VF_DIS, pf->state);
	clear_bit(ICE_FLAG_SRIOV_ENA, pf->flags);
}

 
static struct ice_vsi *ice_vf_vsi_setup(struct ice_vf *vf)
{
	struct ice_vsi_cfg_params params = {};
	struct ice_pf *pf = vf->pf;
	struct ice_vsi *vsi;

	params.type = ICE_VSI_VF;
	params.pi = ice_vf_get_port_info(vf);
	params.vf = vf;
	params.flags = ICE_VSI_FLAG_INIT;

	vsi = ice_vsi_setup(pf, &params);

	if (!vsi) {
		dev_err(ice_pf_to_dev(pf), "Failed to create VF VSI\n");
		ice_vf_invalidate_vsi(vf);
		return NULL;
	}

	vf->lan_vsi_idx = vsi->idx;
	vf->lan_vsi_num = vsi->vsi_num;

	return vsi;
}

 
static int ice_calc_vf_first_vector_idx(struct ice_pf *pf, struct ice_vf *vf)
{
	return pf->sriov_base_vector + vf->vf_id * pf->vfs.num_msix_per;
}

 
static void ice_ena_vf_msix_mappings(struct ice_vf *vf)
{
	int device_based_first_msix, device_based_last_msix;
	int pf_based_first_msix, pf_based_last_msix, v;
	struct ice_pf *pf = vf->pf;
	int device_based_vf_id;
	struct ice_hw *hw;
	u32 reg;

	hw = &pf->hw;
	pf_based_first_msix = vf->first_vector_idx;
	pf_based_last_msix = (pf_based_first_msix + pf->vfs.num_msix_per) - 1;

	device_based_first_msix = pf_based_first_msix +
		pf->hw.func_caps.common_cap.msix_vector_first_id;
	device_based_last_msix =
		(device_based_first_msix + pf->vfs.num_msix_per) - 1;
	device_based_vf_id = vf->vf_id + hw->func_caps.vf_base_id;

	reg = (((device_based_first_msix << VPINT_ALLOC_FIRST_S) &
		VPINT_ALLOC_FIRST_M) |
	       ((device_based_last_msix << VPINT_ALLOC_LAST_S) &
		VPINT_ALLOC_LAST_M) | VPINT_ALLOC_VALID_M);
	wr32(hw, VPINT_ALLOC(vf->vf_id), reg);

	reg = (((device_based_first_msix << VPINT_ALLOC_PCI_FIRST_S)
		 & VPINT_ALLOC_PCI_FIRST_M) |
	       ((device_based_last_msix << VPINT_ALLOC_PCI_LAST_S) &
		VPINT_ALLOC_PCI_LAST_M) | VPINT_ALLOC_PCI_VALID_M);
	wr32(hw, VPINT_ALLOC_PCI(vf->vf_id), reg);

	 
	for (v = pf_based_first_msix; v <= pf_based_last_msix; v++) {
		reg = (((device_based_vf_id << GLINT_VECT2FUNC_VF_NUM_S) &
			GLINT_VECT2FUNC_VF_NUM_M) |
		       ((hw->pf_id << GLINT_VECT2FUNC_PF_NUM_S) &
			GLINT_VECT2FUNC_PF_NUM_M));
		wr32(hw, GLINT_VECT2FUNC(v), reg);
	}

	 
	wr32(hw, VPINT_MBX_CTL(device_based_vf_id), VPINT_MBX_CTL_CAUSE_ENA_M);
}

 
static void ice_ena_vf_q_mappings(struct ice_vf *vf, u16 max_txq, u16 max_rxq)
{
	struct device *dev = ice_pf_to_dev(vf->pf);
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);
	struct ice_hw *hw = &vf->pf->hw;
	u32 reg;

	if (WARN_ON(!vsi))
		return;

	 
	wr32(hw, VPLAN_TXQ_MAPENA(vf->vf_id), VPLAN_TXQ_MAPENA_TX_ENA_M);

	 
	if (vsi->tx_mapping_mode == ICE_VSI_MAP_CONTIG) {
		 
		reg = (((vsi->txq_map[0] << VPLAN_TX_QBASE_VFFIRSTQ_S) &
			VPLAN_TX_QBASE_VFFIRSTQ_M) |
		       (((max_txq - 1) << VPLAN_TX_QBASE_VFNUMQ_S) &
			VPLAN_TX_QBASE_VFNUMQ_M));
		wr32(hw, VPLAN_TX_QBASE(vf->vf_id), reg);
	} else {
		dev_err(dev, "Scattered mode for VF Tx queues is not yet implemented\n");
	}

	 
	wr32(hw, VPLAN_RXQ_MAPENA(vf->vf_id), VPLAN_RXQ_MAPENA_RX_ENA_M);

	 
	if (vsi->rx_mapping_mode == ICE_VSI_MAP_CONTIG) {
		 
		reg = (((vsi->rxq_map[0] << VPLAN_RX_QBASE_VFFIRSTQ_S) &
			VPLAN_RX_QBASE_VFFIRSTQ_M) |
		       (((max_rxq - 1) << VPLAN_RX_QBASE_VFNUMQ_S) &
			VPLAN_RX_QBASE_VFNUMQ_M));
		wr32(hw, VPLAN_RX_QBASE(vf->vf_id), reg);
	} else {
		dev_err(dev, "Scattered mode for VF Rx queues is not yet implemented\n");
	}
}

 
static void ice_ena_vf_mappings(struct ice_vf *vf)
{
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);

	if (WARN_ON(!vsi))
		return;

	ice_ena_vf_msix_mappings(vf);
	ice_ena_vf_q_mappings(vf, vsi->alloc_txq, vsi->alloc_rxq);
}

 
int ice_calc_vf_reg_idx(struct ice_vf *vf, struct ice_q_vector *q_vector)
{
	struct ice_pf *pf;

	if (!vf || !q_vector)
		return -EINVAL;

	pf = vf->pf;

	 
	return pf->sriov_base_vector + pf->vfs.num_msix_per * vf->vf_id +
		q_vector->v_idx + 1;
}

 
static int ice_sriov_set_msix_res(struct ice_pf *pf, u16 num_msix_needed)
{
	u16 total_vectors = pf->hw.func_caps.common_cap.num_msix_vectors;
	int vectors_used = ice_get_max_used_msix_vector(pf);
	int sriov_base_vector;

	sriov_base_vector = total_vectors - num_msix_needed;

	 
	if (sriov_base_vector < vectors_used)
		return -EINVAL;

	pf->sriov_base_vector = sriov_base_vector;

	return 0;
}

 
static int ice_set_per_vf_res(struct ice_pf *pf, u16 num_vfs)
{
	int vectors_used = ice_get_max_used_msix_vector(pf);
	u16 num_msix_per_vf, num_txq, num_rxq, avail_qs;
	int msix_avail_per_vf, msix_avail_for_sriov;
	struct device *dev = ice_pf_to_dev(pf);
	int err;

	lockdep_assert_held(&pf->vfs.table_lock);

	if (!num_vfs)
		return -EINVAL;

	 
	msix_avail_for_sriov = pf->hw.func_caps.common_cap.num_msix_vectors -
		vectors_used;
	msix_avail_per_vf = msix_avail_for_sriov / num_vfs;
	if (msix_avail_per_vf >= ICE_NUM_VF_MSIX_MED) {
		num_msix_per_vf = ICE_NUM_VF_MSIX_MED;
	} else if (msix_avail_per_vf >= ICE_NUM_VF_MSIX_SMALL) {
		num_msix_per_vf = ICE_NUM_VF_MSIX_SMALL;
	} else if (msix_avail_per_vf >= ICE_NUM_VF_MSIX_MULTIQ_MIN) {
		num_msix_per_vf = ICE_NUM_VF_MSIX_MULTIQ_MIN;
	} else if (msix_avail_per_vf >= ICE_MIN_INTR_PER_VF) {
		num_msix_per_vf = ICE_MIN_INTR_PER_VF;
	} else {
		dev_err(dev, "Only %d MSI-X interrupts available for SR-IOV. Not enough to support minimum of %d MSI-X interrupts per VF for %d VFs\n",
			msix_avail_for_sriov, ICE_MIN_INTR_PER_VF,
			num_vfs);
		return -ENOSPC;
	}

	num_txq = min_t(u16, num_msix_per_vf - ICE_NONQ_VECS_VF,
			ICE_MAX_RSS_QS_PER_VF);
	avail_qs = ice_get_avail_txq_count(pf) / num_vfs;
	if (!avail_qs)
		num_txq = 0;
	else if (num_txq > avail_qs)
		num_txq = rounddown_pow_of_two(avail_qs);

	num_rxq = min_t(u16, num_msix_per_vf - ICE_NONQ_VECS_VF,
			ICE_MAX_RSS_QS_PER_VF);
	avail_qs = ice_get_avail_rxq_count(pf) / num_vfs;
	if (!avail_qs)
		num_rxq = 0;
	else if (num_rxq > avail_qs)
		num_rxq = rounddown_pow_of_two(avail_qs);

	if (num_txq < ICE_MIN_QS_PER_VF || num_rxq < ICE_MIN_QS_PER_VF) {
		dev_err(dev, "Not enough queues to support minimum of %d queue pairs per VF for %d VFs\n",
			ICE_MIN_QS_PER_VF, num_vfs);
		return -ENOSPC;
	}

	err = ice_sriov_set_msix_res(pf, num_msix_per_vf * num_vfs);
	if (err) {
		dev_err(dev, "Unable to set MSI-X resources for %d VFs, err %d\n",
			num_vfs, err);
		return err;
	}

	 
	pf->vfs.num_qps_per = min_t(int, num_txq, num_rxq);
	pf->vfs.num_msix_per = num_msix_per_vf;
	dev_info(dev, "Enabling %d VFs with %d vectors and %d queues per VF\n",
		 num_vfs, pf->vfs.num_msix_per, pf->vfs.num_qps_per);

	return 0;
}

 
static int ice_init_vf_vsi_res(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;
	struct ice_vsi *vsi;
	int err;

	vf->first_vector_idx = ice_calc_vf_first_vector_idx(pf, vf);

	vsi = ice_vf_vsi_setup(vf);
	if (!vsi)
		return -ENOMEM;

	err = ice_vf_init_host_cfg(vf, vsi);
	if (err)
		goto release_vsi;

	return 0;

release_vsi:
	ice_vf_vsi_release(vf);
	return err;
}

 
static int ice_start_vfs(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;
	unsigned int bkt, it_cnt;
	struct ice_vf *vf;
	int retval;

	lockdep_assert_held(&pf->vfs.table_lock);

	it_cnt = 0;
	ice_for_each_vf(pf, bkt, vf) {
		vf->vf_ops->clear_reset_trigger(vf);

		retval = ice_init_vf_vsi_res(vf);
		if (retval) {
			dev_err(ice_pf_to_dev(pf), "Failed to initialize VSI resources for VF %d, error %d\n",
				vf->vf_id, retval);
			goto teardown;
		}

		set_bit(ICE_VF_STATE_INIT, vf->vf_states);
		ice_ena_vf_mappings(vf);
		wr32(hw, VFGEN_RSTAT(vf->vf_id), VIRTCHNL_VFR_VFACTIVE);
		it_cnt++;
	}

	ice_flush(hw);
	return 0;

teardown:
	ice_for_each_vf(pf, bkt, vf) {
		if (it_cnt == 0)
			break;

		ice_dis_vf_mappings(vf);
		ice_vf_vsi_release(vf);
		it_cnt--;
	}

	return retval;
}

 
static void ice_sriov_free_vf(struct ice_vf *vf)
{
	mutex_destroy(&vf->cfg_lock);

	kfree_rcu(vf, rcu);
}

 
static void ice_sriov_clear_reset_state(struct ice_vf *vf)
{
	struct ice_hw *hw = &vf->pf->hw;

	 
	wr32(hw, VFGEN_RSTAT(vf->vf_id), VIRTCHNL_VFR_INPROGRESS);
}

 
static void ice_sriov_clear_mbx_register(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;

	wr32(&pf->hw, VF_MBX_ARQLEN(vf->vf_id), 0);
	wr32(&pf->hw, VF_MBX_ATQLEN(vf->vf_id), 0);
}

 
static void ice_sriov_trigger_reset_register(struct ice_vf *vf, bool is_vflr)
{
	struct ice_pf *pf = vf->pf;
	u32 reg, reg_idx, bit_idx;
	unsigned int vf_abs_id, i;
	struct device *dev;
	struct ice_hw *hw;

	dev = ice_pf_to_dev(pf);
	hw = &pf->hw;
	vf_abs_id = vf->vf_id + hw->func_caps.vf_base_id;

	 
	if (!is_vflr) {
		reg = rd32(hw, VPGEN_VFRTRIG(vf->vf_id));
		reg |= VPGEN_VFRTRIG_VFSWR_M;
		wr32(hw, VPGEN_VFRTRIG(vf->vf_id), reg);
	}

	 
	reg_idx = (vf_abs_id) / 32;
	bit_idx = (vf_abs_id) % 32;
	wr32(hw, GLGEN_VFLRSTAT(reg_idx), BIT(bit_idx));
	ice_flush(hw);

	wr32(hw, PF_PCI_CIAA,
	     VF_DEVICE_STATUS | (vf_abs_id << PF_PCI_CIAA_VF_NUM_S));
	for (i = 0; i < ICE_PCI_CIAD_WAIT_COUNT; i++) {
		reg = rd32(hw, PF_PCI_CIAD);
		 
		if ((reg & VF_TRANS_PENDING_M) == 0)
			break;

		dev_err(dev, "VF %u PCI transactions stuck\n", vf->vf_id);
		udelay(ICE_PCI_CIAD_WAIT_DELAY_US);
	}
}

 
static bool ice_sriov_poll_reset_status(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;
	unsigned int i;
	u32 reg;

	for (i = 0; i < 10; i++) {
		 
		reg = rd32(&pf->hw, VPGEN_VFRSTAT(vf->vf_id));
		if (reg & VPGEN_VFRSTAT_VFRD_M)
			return true;

		 
		usleep_range(10, 20);
	}
	return false;
}

 
static void ice_sriov_clear_reset_trigger(struct ice_vf *vf)
{
	struct ice_hw *hw = &vf->pf->hw;
	u32 reg;

	reg = rd32(hw, VPGEN_VFRTRIG(vf->vf_id));
	reg &= ~VPGEN_VFRTRIG_VFSWR_M;
	wr32(hw, VPGEN_VFRTRIG(vf->vf_id), reg);
	ice_flush(hw);
}

 
static int ice_sriov_create_vsi(struct ice_vf *vf)
{
	struct ice_vsi *vsi;

	vsi = ice_vf_vsi_setup(vf);
	if (!vsi)
		return -ENOMEM;

	return 0;
}

 
static void ice_sriov_post_vsi_rebuild(struct ice_vf *vf)
{
	ice_ena_vf_mappings(vf);
	wr32(&vf->pf->hw, VFGEN_RSTAT(vf->vf_id), VIRTCHNL_VFR_VFACTIVE);
}

static const struct ice_vf_ops ice_sriov_vf_ops = {
	.reset_type = ICE_VF_RESET,
	.free = ice_sriov_free_vf,
	.clear_reset_state = ice_sriov_clear_reset_state,
	.clear_mbx_register = ice_sriov_clear_mbx_register,
	.trigger_reset_register = ice_sriov_trigger_reset_register,
	.poll_reset_status = ice_sriov_poll_reset_status,
	.clear_reset_trigger = ice_sriov_clear_reset_trigger,
	.irq_close = NULL,
	.create_vsi = ice_sriov_create_vsi,
	.post_vsi_rebuild = ice_sriov_post_vsi_rebuild,
};

 
static int ice_create_vf_entries(struct ice_pf *pf, u16 num_vfs)
{
	struct ice_vfs *vfs = &pf->vfs;
	struct ice_vf *vf;
	u16 vf_id;
	int err;

	lockdep_assert_held(&vfs->table_lock);

	for (vf_id = 0; vf_id < num_vfs; vf_id++) {
		vf = kzalloc(sizeof(*vf), GFP_KERNEL);
		if (!vf) {
			err = -ENOMEM;
			goto err_free_entries;
		}
		kref_init(&vf->refcnt);

		vf->pf = pf;
		vf->vf_id = vf_id;

		 
		vf->vf_ops = &ice_sriov_vf_ops;

		ice_initialize_vf_entry(vf);

		vf->vf_sw_id = pf->first_sw;

		hash_add_rcu(vfs->table, &vf->entry, vf_id);
	}

	return 0;

err_free_entries:
	ice_free_vf_entries(pf);
	return err;
}

 
static int ice_ena_vfs(struct ice_pf *pf, u16 num_vfs)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	int ret;

	 
	wr32(hw, GLINT_DYN_CTL(pf->oicr_irq.index),
	     ICE_ITR_NONE << GLINT_DYN_CTL_ITR_INDX_S);
	set_bit(ICE_OICR_INTR_DIS, pf->state);
	ice_flush(hw);

	ret = pci_enable_sriov(pf->pdev, num_vfs);
	if (ret)
		goto err_unroll_intr;

	mutex_lock(&pf->vfs.table_lock);

	ret = ice_set_per_vf_res(pf, num_vfs);
	if (ret) {
		dev_err(dev, "Not enough resources for %d VFs, err %d. Try with fewer number of VFs\n",
			num_vfs, ret);
		goto err_unroll_sriov;
	}

	ret = ice_create_vf_entries(pf, num_vfs);
	if (ret) {
		dev_err(dev, "Failed to allocate VF entries for %d VFs\n",
			num_vfs);
		goto err_unroll_sriov;
	}

	ret = ice_start_vfs(pf);
	if (ret) {
		dev_err(dev, "Failed to start %d VFs, err %d\n", num_vfs, ret);
		ret = -EAGAIN;
		goto err_unroll_vf_entries;
	}

	clear_bit(ICE_VF_DIS, pf->state);

	ret = ice_eswitch_configure(pf);
	if (ret) {
		dev_err(dev, "Failed to configure eswitch, err %d\n", ret);
		goto err_unroll_sriov;
	}

	 
	if (test_and_clear_bit(ICE_OICR_INTR_DIS, pf->state))
		ice_irq_dynamic_ena(hw, NULL, NULL);

	mutex_unlock(&pf->vfs.table_lock);

	return 0;

err_unroll_vf_entries:
	ice_free_vf_entries(pf);
err_unroll_sriov:
	mutex_unlock(&pf->vfs.table_lock);
	pci_disable_sriov(pf->pdev);
err_unroll_intr:
	 
	ice_irq_dynamic_ena(hw, NULL, NULL);
	clear_bit(ICE_OICR_INTR_DIS, pf->state);
	return ret;
}

 
static int ice_pci_sriov_ena(struct ice_pf *pf, int num_vfs)
{
	struct device *dev = ice_pf_to_dev(pf);
	int err;

	if (!num_vfs) {
		ice_free_vfs(pf);
		return 0;
	}

	if (num_vfs > pf->vfs.num_supported) {
		dev_err(dev, "Can't enable %d VFs, max VFs supported is %d\n",
			num_vfs, pf->vfs.num_supported);
		return -EOPNOTSUPP;
	}

	dev_info(dev, "Enabling %d VFs\n", num_vfs);
	err = ice_ena_vfs(pf, num_vfs);
	if (err) {
		dev_err(dev, "Failed to enable SR-IOV: %d\n", err);
		return err;
	}

	set_bit(ICE_FLAG_SRIOV_ENA, pf->flags);
	return 0;
}

 
static int ice_check_sriov_allowed(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);

	if (!test_bit(ICE_FLAG_SRIOV_CAPABLE, pf->flags)) {
		dev_err(dev, "This device is not capable of SR-IOV\n");
		return -EOPNOTSUPP;
	}

	if (ice_is_safe_mode(pf)) {
		dev_err(dev, "SR-IOV cannot be configured - Device is in Safe Mode\n");
		return -EOPNOTSUPP;
	}

	if (!ice_pf_state_is_nominal(pf)) {
		dev_err(dev, "Cannot enable SR-IOV, device not ready\n");
		return -EBUSY;
	}

	return 0;
}

 
int ice_sriov_configure(struct pci_dev *pdev, int num_vfs)
{
	struct ice_pf *pf = pci_get_drvdata(pdev);
	struct device *dev = ice_pf_to_dev(pf);
	int err;

	err = ice_check_sriov_allowed(pf);
	if (err)
		return err;

	if (!num_vfs) {
		if (!pci_vfs_assigned(pdev)) {
			ice_free_vfs(pf);
			return 0;
		}

		dev_err(dev, "can't free VFs because some are assigned to VMs.\n");
		return -EBUSY;
	}

	err = ice_pci_sriov_ena(pf, num_vfs);
	if (err)
		return err;

	return num_vfs;
}

 
void ice_process_vflr_event(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;
	struct ice_vf *vf;
	unsigned int bkt;
	u32 reg;

	if (!test_and_clear_bit(ICE_VFLR_EVENT_PENDING, pf->state) ||
	    !ice_has_vfs(pf))
		return;

	mutex_lock(&pf->vfs.table_lock);
	ice_for_each_vf(pf, bkt, vf) {
		u32 reg_idx, bit_idx;

		reg_idx = (hw->func_caps.vf_base_id + vf->vf_id) / 32;
		bit_idx = (hw->func_caps.vf_base_id + vf->vf_id) % 32;
		 
		reg = rd32(hw, GLGEN_VFLRSTAT(reg_idx));
		if (reg & BIT(bit_idx))
			 
			ice_reset_vf(vf, ICE_VF_RESET_VFLR | ICE_VF_RESET_LOCK);
	}
	mutex_unlock(&pf->vfs.table_lock);
}

 
static struct ice_vf *ice_get_vf_from_pfq(struct ice_pf *pf, u16 pfq)
{
	struct ice_vf *vf;
	unsigned int bkt;

	rcu_read_lock();
	ice_for_each_vf_rcu(pf, bkt, vf) {
		struct ice_vsi *vsi;
		u16 rxq_idx;

		vsi = ice_get_vf_vsi(vf);
		if (!vsi)
			continue;

		ice_for_each_rxq(vsi, rxq_idx)
			if (vsi->rxq_map[rxq_idx] == pfq) {
				struct ice_vf *found;

				if (kref_get_unless_zero(&vf->refcnt))
					found = vf;
				else
					found = NULL;
				rcu_read_unlock();
				return found;
			}
	}
	rcu_read_unlock();

	return NULL;
}

 
static u32 ice_globalq_to_pfq(struct ice_pf *pf, u32 globalq)
{
	return globalq - pf->hw.func_caps.common_cap.rxq_first_id;
}

 
void
ice_vf_lan_overflow_event(struct ice_pf *pf, struct ice_rq_event_info *event)
{
	u32 gldcb_rtctq, queue;
	struct ice_vf *vf;

	gldcb_rtctq = le32_to_cpu(event->desc.params.lan_overflow.prtdcb_ruptq);
	dev_dbg(ice_pf_to_dev(pf), "GLDCB_RTCTQ: 0x%08x\n", gldcb_rtctq);

	 
	queue = (gldcb_rtctq & GLDCB_RTCTQ_RXQNUM_M) >>
		GLDCB_RTCTQ_RXQNUM_S;

	vf = ice_get_vf_from_pfq(pf, ice_globalq_to_pfq(pf, queue));
	if (!vf)
		return;

	ice_reset_vf(vf, ICE_VF_RESET_NOTIFY | ICE_VF_RESET_LOCK);
	ice_put_vf(vf);
}

 
int ice_set_vf_spoofchk(struct net_device *netdev, int vf_id, bool ena)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_pf *pf = np->vsi->back;
	struct ice_vsi *vf_vsi;
	struct device *dev;
	struct ice_vf *vf;
	int ret;

	dev = ice_pf_to_dev(pf);

	vf = ice_get_vf_by_id(pf, vf_id);
	if (!vf)
		return -EINVAL;

	ret = ice_check_vf_ready_for_cfg(vf);
	if (ret)
		goto out_put_vf;

	vf_vsi = ice_get_vf_vsi(vf);
	if (!vf_vsi) {
		netdev_err(netdev, "VSI %d for VF %d is null\n",
			   vf->lan_vsi_idx, vf->vf_id);
		ret = -EINVAL;
		goto out_put_vf;
	}

	if (vf_vsi->type != ICE_VSI_VF) {
		netdev_err(netdev, "Type %d of VSI %d for VF %d is no ICE_VSI_VF\n",
			   vf_vsi->type, vf_vsi->vsi_num, vf->vf_id);
		ret = -ENODEV;
		goto out_put_vf;
	}

	if (ena == vf->spoofchk) {
		dev_dbg(dev, "VF spoofchk already %s\n", ena ? "ON" : "OFF");
		ret = 0;
		goto out_put_vf;
	}

	ret = ice_vsi_apply_spoofchk(vf_vsi, ena);
	if (ret)
		dev_err(dev, "Failed to set spoofchk %s for VF %d VSI %d\n error %d\n",
			ena ? "ON" : "OFF", vf->vf_id, vf_vsi->vsi_num, ret);
	else
		vf->spoofchk = ena;

out_put_vf:
	ice_put_vf(vf);
	return ret;
}

 
int
ice_get_vf_cfg(struct net_device *netdev, int vf_id, struct ifla_vf_info *ivi)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	struct ice_vf *vf;
	int ret;

	vf = ice_get_vf_by_id(pf, vf_id);
	if (!vf)
		return -EINVAL;

	ret = ice_check_vf_ready_for_cfg(vf);
	if (ret)
		goto out_put_vf;

	ivi->vf = vf_id;
	ether_addr_copy(ivi->mac, vf->hw_lan_addr);

	 
	ivi->vlan = ice_vf_get_port_vlan_id(vf);
	ivi->qos = ice_vf_get_port_vlan_prio(vf);
	if (ice_vf_is_port_vlan_ena(vf))
		ivi->vlan_proto = cpu_to_be16(ice_vf_get_port_vlan_tpid(vf));

	ivi->trusted = vf->trusted;
	ivi->spoofchk = vf->spoofchk;
	if (!vf->link_forced)
		ivi->linkstate = IFLA_VF_LINK_STATE_AUTO;
	else if (vf->link_up)
		ivi->linkstate = IFLA_VF_LINK_STATE_ENABLE;
	else
		ivi->linkstate = IFLA_VF_LINK_STATE_DISABLE;
	ivi->max_tx_rate = vf->max_tx_rate;
	ivi->min_tx_rate = vf->min_tx_rate;

out_put_vf:
	ice_put_vf(vf);
	return ret;
}

 
int ice_set_vf_mac(struct net_device *netdev, int vf_id, u8 *mac)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	struct ice_vf *vf;
	int ret;

	if (is_multicast_ether_addr(mac)) {
		netdev_err(netdev, "%pM not a valid unicast address\n", mac);
		return -EINVAL;
	}

	vf = ice_get_vf_by_id(pf, vf_id);
	if (!vf)
		return -EINVAL;

	 
	if (ether_addr_equal(vf->dev_lan_addr, mac) &&
	    ether_addr_equal(vf->hw_lan_addr, mac)) {
		ret = 0;
		goto out_put_vf;
	}

	ret = ice_check_vf_ready_for_cfg(vf);
	if (ret)
		goto out_put_vf;

	mutex_lock(&vf->cfg_lock);

	 
	ether_addr_copy(vf->dev_lan_addr, mac);
	ether_addr_copy(vf->hw_lan_addr, mac);
	if (is_zero_ether_addr(mac)) {
		 
		vf->pf_set_mac = false;
		netdev_info(netdev, "Removing MAC on VF %d. VF driver will be reinitialized\n",
			    vf->vf_id);
	} else {
		 
		vf->pf_set_mac = true;
		netdev_info(netdev, "Setting MAC %pM on VF %d. VF driver will be reinitialized\n",
			    mac, vf_id);
	}

	ice_reset_vf(vf, ICE_VF_RESET_NOTIFY);
	mutex_unlock(&vf->cfg_lock);

out_put_vf:
	ice_put_vf(vf);
	return ret;
}

 
int ice_set_vf_trust(struct net_device *netdev, int vf_id, bool trusted)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	struct ice_vf *vf;
	int ret;

	vf = ice_get_vf_by_id(pf, vf_id);
	if (!vf)
		return -EINVAL;

	if (ice_is_eswitch_mode_switchdev(pf)) {
		dev_info(ice_pf_to_dev(pf), "Trusted VF is forbidden in switchdev mode\n");
		return -EOPNOTSUPP;
	}

	ret = ice_check_vf_ready_for_cfg(vf);
	if (ret)
		goto out_put_vf;

	 
	if (trusted == vf->trusted) {
		ret = 0;
		goto out_put_vf;
	}

	mutex_lock(&vf->cfg_lock);

	vf->trusted = trusted;
	ice_reset_vf(vf, ICE_VF_RESET_NOTIFY);
	dev_info(ice_pf_to_dev(pf), "VF %u is now %strusted\n",
		 vf_id, trusted ? "" : "un");

	mutex_unlock(&vf->cfg_lock);

out_put_vf:
	ice_put_vf(vf);
	return ret;
}

 
int ice_set_vf_link_state(struct net_device *netdev, int vf_id, int link_state)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	struct ice_vf *vf;
	int ret;

	vf = ice_get_vf_by_id(pf, vf_id);
	if (!vf)
		return -EINVAL;

	ret = ice_check_vf_ready_for_cfg(vf);
	if (ret)
		goto out_put_vf;

	switch (link_state) {
	case IFLA_VF_LINK_STATE_AUTO:
		vf->link_forced = false;
		break;
	case IFLA_VF_LINK_STATE_ENABLE:
		vf->link_forced = true;
		vf->link_up = true;
		break;
	case IFLA_VF_LINK_STATE_DISABLE:
		vf->link_forced = true;
		vf->link_up = false;
		break;
	default:
		ret = -EINVAL;
		goto out_put_vf;
	}

	ice_vc_notify_vf_link_state(vf);

out_put_vf:
	ice_put_vf(vf);
	return ret;
}

 
static int ice_calc_all_vfs_min_tx_rate(struct ice_pf *pf)
{
	struct ice_vf *vf;
	unsigned int bkt;
	int rate = 0;

	rcu_read_lock();
	ice_for_each_vf_rcu(pf, bkt, vf)
		rate += vf->min_tx_rate;
	rcu_read_unlock();

	return rate;
}

 
static bool
ice_min_tx_rate_oversubscribed(struct ice_vf *vf, int min_tx_rate)
{
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);
	int all_vfs_min_tx_rate;
	int link_speed_mbps;

	if (WARN_ON(!vsi))
		return false;

	link_speed_mbps = ice_get_link_speed_mbps(vsi);
	all_vfs_min_tx_rate = ice_calc_all_vfs_min_tx_rate(vf->pf);

	 
	all_vfs_min_tx_rate -= vf->min_tx_rate;

	if (all_vfs_min_tx_rate + min_tx_rate > link_speed_mbps) {
		dev_err(ice_pf_to_dev(vf->pf), "min_tx_rate of %d Mbps on VF %u would cause oversubscription of %d Mbps based on the current link speed %d Mbps\n",
			min_tx_rate, vf->vf_id,
			all_vfs_min_tx_rate + min_tx_rate - link_speed_mbps,
			link_speed_mbps);
		return true;
	}

	return false;
}

 
int
ice_set_vf_bw(struct net_device *netdev, int vf_id, int min_tx_rate,
	      int max_tx_rate)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	struct ice_vsi *vsi;
	struct device *dev;
	struct ice_vf *vf;
	int ret;

	dev = ice_pf_to_dev(pf);

	vf = ice_get_vf_by_id(pf, vf_id);
	if (!vf)
		return -EINVAL;

	ret = ice_check_vf_ready_for_cfg(vf);
	if (ret)
		goto out_put_vf;

	vsi = ice_get_vf_vsi(vf);
	if (!vsi) {
		ret = -EINVAL;
		goto out_put_vf;
	}

	if (min_tx_rate && ice_is_dcb_active(pf)) {
		dev_err(dev, "DCB on PF is currently enabled. VF min Tx rate limiting not allowed on this PF.\n");
		ret = -EOPNOTSUPP;
		goto out_put_vf;
	}

	if (ice_min_tx_rate_oversubscribed(vf, min_tx_rate)) {
		ret = -EINVAL;
		goto out_put_vf;
	}

	if (vf->min_tx_rate != (unsigned int)min_tx_rate) {
		ret = ice_set_min_bw_limit(vsi, (u64)min_tx_rate * 1000);
		if (ret) {
			dev_err(dev, "Unable to set min-tx-rate for VF %d\n",
				vf->vf_id);
			goto out_put_vf;
		}

		vf->min_tx_rate = min_tx_rate;
	}

	if (vf->max_tx_rate != (unsigned int)max_tx_rate) {
		ret = ice_set_max_bw_limit(vsi, (u64)max_tx_rate * 1000);
		if (ret) {
			dev_err(dev, "Unable to set max-tx-rate for VF %d\n",
				vf->vf_id);
			goto out_put_vf;
		}

		vf->max_tx_rate = max_tx_rate;
	}

out_put_vf:
	ice_put_vf(vf);
	return ret;
}

 
int ice_get_vf_stats(struct net_device *netdev, int vf_id,
		     struct ifla_vf_stats *vf_stats)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	struct ice_eth_stats *stats;
	struct ice_vsi *vsi;
	struct ice_vf *vf;
	int ret;

	vf = ice_get_vf_by_id(pf, vf_id);
	if (!vf)
		return -EINVAL;

	ret = ice_check_vf_ready_for_cfg(vf);
	if (ret)
		goto out_put_vf;

	vsi = ice_get_vf_vsi(vf);
	if (!vsi) {
		ret = -EINVAL;
		goto out_put_vf;
	}

	ice_update_eth_stats(vsi);
	stats = &vsi->eth_stats;

	memset(vf_stats, 0, sizeof(*vf_stats));

	vf_stats->rx_packets = stats->rx_unicast + stats->rx_broadcast +
		stats->rx_multicast;
	vf_stats->tx_packets = stats->tx_unicast + stats->tx_broadcast +
		stats->tx_multicast;
	vf_stats->rx_bytes   = stats->rx_bytes;
	vf_stats->tx_bytes   = stats->tx_bytes;
	vf_stats->broadcast  = stats->rx_broadcast;
	vf_stats->multicast  = stats->rx_multicast;
	vf_stats->rx_dropped = stats->rx_discards;
	vf_stats->tx_dropped = stats->tx_discards;

out_put_vf:
	ice_put_vf(vf);
	return ret;
}

 
static bool
ice_is_supported_port_vlan_proto(struct ice_hw *hw, u16 vlan_proto)
{
	bool is_supported = false;

	switch (vlan_proto) {
	case ETH_P_8021Q:
		is_supported = true;
		break;
	case ETH_P_8021AD:
		if (ice_is_dvm_ena(hw))
			is_supported = true;
		break;
	}

	return is_supported;
}

 
int
ice_set_vf_port_vlan(struct net_device *netdev, int vf_id, u16 vlan_id, u8 qos,
		     __be16 vlan_proto)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	u16 local_vlan_proto = ntohs(vlan_proto);
	struct device *dev;
	struct ice_vf *vf;
	int ret;

	dev = ice_pf_to_dev(pf);

	if (vlan_id >= VLAN_N_VID || qos > 7) {
		dev_err(dev, "Invalid Port VLAN parameters for VF %d, ID %d, QoS %d\n",
			vf_id, vlan_id, qos);
		return -EINVAL;
	}

	if (!ice_is_supported_port_vlan_proto(&pf->hw, local_vlan_proto)) {
		dev_err(dev, "VF VLAN protocol 0x%04x is not supported\n",
			local_vlan_proto);
		return -EPROTONOSUPPORT;
	}

	vf = ice_get_vf_by_id(pf, vf_id);
	if (!vf)
		return -EINVAL;

	ret = ice_check_vf_ready_for_cfg(vf);
	if (ret)
		goto out_put_vf;

	if (ice_vf_get_port_vlan_prio(vf) == qos &&
	    ice_vf_get_port_vlan_tpid(vf) == local_vlan_proto &&
	    ice_vf_get_port_vlan_id(vf) == vlan_id) {
		 
		dev_dbg(dev, "Duplicate port VLAN %u, QoS %u, TPID 0x%04x request\n",
			vlan_id, qos, local_vlan_proto);
		ret = 0;
		goto out_put_vf;
	}

	mutex_lock(&vf->cfg_lock);

	vf->port_vlan_info = ICE_VLAN(local_vlan_proto, vlan_id, qos);
	if (ice_vf_is_port_vlan_ena(vf))
		dev_info(dev, "Setting VLAN %u, QoS %u, TPID 0x%04x on VF %d\n",
			 vlan_id, qos, local_vlan_proto, vf_id);
	else
		dev_info(dev, "Clearing port VLAN on VF %d\n", vf_id);

	ice_reset_vf(vf, ICE_VF_RESET_NOTIFY);
	mutex_unlock(&vf->cfg_lock);

out_put_vf:
	ice_put_vf(vf);
	return ret;
}

 
void ice_print_vf_rx_mdd_event(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;
	struct device *dev;

	dev = ice_pf_to_dev(pf);

	dev_info(dev, "%d Rx Malicious Driver Detection events detected on PF %d VF %d MAC %pM. mdd-auto-reset-vfs=%s\n",
		 vf->mdd_rx_events.count, pf->hw.pf_id, vf->vf_id,
		 vf->dev_lan_addr,
		 test_bit(ICE_FLAG_MDD_AUTO_RESET_VF, pf->flags)
			  ? "on" : "off");
}

 
void ice_print_vfs_mdd_events(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	struct ice_vf *vf;
	unsigned int bkt;

	 
	if (!test_and_clear_bit(ICE_MDD_VF_PRINT_PENDING, pf->state))
		return;

	 
	if (time_is_after_jiffies(pf->vfs.last_printed_mdd_jiffies + HZ * 1))
		return;

	pf->vfs.last_printed_mdd_jiffies = jiffies;

	mutex_lock(&pf->vfs.table_lock);
	ice_for_each_vf(pf, bkt, vf) {
		 
		if (vf->mdd_rx_events.count != vf->mdd_rx_events.last_printed) {
			vf->mdd_rx_events.last_printed =
							vf->mdd_rx_events.count;
			ice_print_vf_rx_mdd_event(vf);
		}

		 
		if (vf->mdd_tx_events.count != vf->mdd_tx_events.last_printed) {
			vf->mdd_tx_events.last_printed =
							vf->mdd_tx_events.count;

			dev_info(dev, "%d Tx Malicious Driver Detection events detected on PF %d VF %d MAC %pM.\n",
				 vf->mdd_tx_events.count, hw->pf_id, vf->vf_id,
				 vf->dev_lan_addr);
		}
	}
	mutex_unlock(&pf->vfs.table_lock);
}

 
void ice_restore_all_vfs_msi_state(struct pci_dev *pdev)
{
	u16 vf_id;
	int pos;

	if (!pci_num_vf(pdev))
		return;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_SRIOV);
	if (pos) {
		struct pci_dev *vfdev;

		pci_read_config_word(pdev, pos + PCI_SRIOV_VF_DID,
				     &vf_id);
		vfdev = pci_get_device(pdev->vendor, vf_id, NULL);
		while (vfdev) {
			if (vfdev->is_virtfn && vfdev->physfn == pdev)
				pci_restore_msi_state(vfdev);
			vfdev = pci_get_device(pdev->vendor, vf_id,
					       vfdev);
		}
	}
}
