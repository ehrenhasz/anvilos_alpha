
 

#include "ice.h"
#include "ice_base.h"
#include "ice_flow.h"
#include "ice_lib.h"
#include "ice_fltr.h"
#include "ice_dcb_lib.h"
#include "ice_devlink.h"
#include "ice_vsi_vlan_ops.h"

 
const char *ice_vsi_type_str(enum ice_vsi_type vsi_type)
{
	switch (vsi_type) {
	case ICE_VSI_PF:
		return "ICE_VSI_PF";
	case ICE_VSI_VF:
		return "ICE_VSI_VF";
	case ICE_VSI_CTRL:
		return "ICE_VSI_CTRL";
	case ICE_VSI_CHNL:
		return "ICE_VSI_CHNL";
	case ICE_VSI_LB:
		return "ICE_VSI_LB";
	case ICE_VSI_SWITCHDEV_CTRL:
		return "ICE_VSI_SWITCHDEV_CTRL";
	default:
		return "unknown";
	}
}

 
static int ice_vsi_ctrl_all_rx_rings(struct ice_vsi *vsi, bool ena)
{
	int ret = 0;
	u16 i;

	ice_for_each_rxq(vsi, i)
		ice_vsi_ctrl_one_rx_ring(vsi, ena, i, false);

	ice_flush(&vsi->back->hw);

	ice_for_each_rxq(vsi, i) {
		ret = ice_vsi_wait_one_rx_ring(vsi, ena, i);
		if (ret)
			break;
	}

	return ret;
}

 
static int ice_vsi_alloc_arrays(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;

	dev = ice_pf_to_dev(pf);
	if (vsi->type == ICE_VSI_CHNL)
		return 0;

	 
	vsi->tx_rings = devm_kcalloc(dev, vsi->alloc_txq,
				     sizeof(*vsi->tx_rings), GFP_KERNEL);
	if (!vsi->tx_rings)
		return -ENOMEM;

	vsi->rx_rings = devm_kcalloc(dev, vsi->alloc_rxq,
				     sizeof(*vsi->rx_rings), GFP_KERNEL);
	if (!vsi->rx_rings)
		goto err_rings;

	 
	vsi->txq_map = devm_kcalloc(dev, (vsi->alloc_txq + num_possible_cpus()),
				    sizeof(*vsi->txq_map), GFP_KERNEL);

	if (!vsi->txq_map)
		goto err_txq_map;

	vsi->rxq_map = devm_kcalloc(dev, vsi->alloc_rxq,
				    sizeof(*vsi->rxq_map), GFP_KERNEL);
	if (!vsi->rxq_map)
		goto err_rxq_map;

	 
	if (vsi->type == ICE_VSI_LB)
		return 0;

	 
	vsi->q_vectors = devm_kcalloc(dev, vsi->num_q_vectors,
				      sizeof(*vsi->q_vectors), GFP_KERNEL);
	if (!vsi->q_vectors)
		goto err_vectors;

	vsi->af_xdp_zc_qps = bitmap_zalloc(max_t(int, vsi->alloc_txq, vsi->alloc_rxq), GFP_KERNEL);
	if (!vsi->af_xdp_zc_qps)
		goto err_zc_qps;

	return 0;

err_zc_qps:
	devm_kfree(dev, vsi->q_vectors);
err_vectors:
	devm_kfree(dev, vsi->rxq_map);
err_rxq_map:
	devm_kfree(dev, vsi->txq_map);
err_txq_map:
	devm_kfree(dev, vsi->rx_rings);
err_rings:
	devm_kfree(dev, vsi->tx_rings);
	return -ENOMEM;
}

 
static void ice_vsi_set_num_desc(struct ice_vsi *vsi)
{
	switch (vsi->type) {
	case ICE_VSI_PF:
	case ICE_VSI_SWITCHDEV_CTRL:
	case ICE_VSI_CTRL:
	case ICE_VSI_LB:
		 
		if (!vsi->num_rx_desc)
			vsi->num_rx_desc = ICE_DFLT_NUM_RX_DESC;
		if (!vsi->num_tx_desc)
			vsi->num_tx_desc = ICE_DFLT_NUM_TX_DESC;
		break;
	default:
		dev_dbg(ice_pf_to_dev(vsi->back), "Not setting number of Tx/Rx descriptors for VSI type %d\n",
			vsi->type);
		break;
	}
}

 
static void ice_vsi_set_num_qs(struct ice_vsi *vsi)
{
	enum ice_vsi_type vsi_type = vsi->type;
	struct ice_pf *pf = vsi->back;
	struct ice_vf *vf = vsi->vf;

	if (WARN_ON(vsi_type == ICE_VSI_VF && !vf))
		return;

	switch (vsi_type) {
	case ICE_VSI_PF:
		if (vsi->req_txq) {
			vsi->alloc_txq = vsi->req_txq;
			vsi->num_txq = vsi->req_txq;
		} else {
			vsi->alloc_txq = min3(pf->num_lan_msix,
					      ice_get_avail_txq_count(pf),
					      (u16)num_online_cpus());
		}

		pf->num_lan_tx = vsi->alloc_txq;

		 
		if (!test_bit(ICE_FLAG_RSS_ENA, pf->flags)) {
			vsi->alloc_rxq = 1;
		} else {
			if (vsi->req_rxq) {
				vsi->alloc_rxq = vsi->req_rxq;
				vsi->num_rxq = vsi->req_rxq;
			} else {
				vsi->alloc_rxq = min3(pf->num_lan_msix,
						      ice_get_avail_rxq_count(pf),
						      (u16)num_online_cpus());
			}
		}

		pf->num_lan_rx = vsi->alloc_rxq;

		vsi->num_q_vectors = min_t(int, pf->num_lan_msix,
					   max_t(int, vsi->alloc_rxq,
						 vsi->alloc_txq));
		break;
	case ICE_VSI_SWITCHDEV_CTRL:
		 
		vsi->alloc_txq = ice_get_num_vfs(pf);
		vsi->alloc_rxq = vsi->alloc_txq;
		vsi->num_q_vectors = 1;
		break;
	case ICE_VSI_VF:
		if (vf->num_req_qs)
			vf->num_vf_qs = vf->num_req_qs;
		vsi->alloc_txq = vf->num_vf_qs;
		vsi->alloc_rxq = vf->num_vf_qs;
		 
		vsi->num_q_vectors = pf->vfs.num_msix_per - ICE_NONQ_VECS_VF;
		break;
	case ICE_VSI_CTRL:
		vsi->alloc_txq = 1;
		vsi->alloc_rxq = 1;
		vsi->num_q_vectors = 1;
		break;
	case ICE_VSI_CHNL:
		vsi->alloc_txq = 0;
		vsi->alloc_rxq = 0;
		break;
	case ICE_VSI_LB:
		vsi->alloc_txq = 1;
		vsi->alloc_rxq = 1;
		break;
	default:
		dev_warn(ice_pf_to_dev(pf), "Unknown VSI type %d\n", vsi_type);
		break;
	}

	ice_vsi_set_num_desc(vsi);
}

 
static int ice_get_free_slot(void *array, int size, int curr)
{
	int **tmp_array = (int **)array;
	int next;

	if (curr < (size - 1) && !tmp_array[curr + 1]) {
		next = curr + 1;
	} else {
		int i = 0;

		while ((i < size) && (tmp_array[i]))
			i++;
		if (i == size)
			next = ICE_NO_VSI;
		else
			next = i;
	}
	return next;
}

 
static void ice_vsi_delete_from_hw(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct ice_vsi_ctx *ctxt;
	int status;

	ice_fltr_remove_all(vsi);
	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return;

	if (vsi->type == ICE_VSI_VF)
		ctxt->vf_num = vsi->vf->vf_id;
	ctxt->vsi_num = vsi->vsi_num;

	memcpy(&ctxt->info, &vsi->info, sizeof(ctxt->info));

	status = ice_free_vsi(&pf->hw, vsi->idx, ctxt, false, NULL);
	if (status)
		dev_err(ice_pf_to_dev(pf), "Failed to delete VSI %i in FW - error: %d\n",
			vsi->vsi_num, status);

	kfree(ctxt);
}

 
static void ice_vsi_free_arrays(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;

	dev = ice_pf_to_dev(pf);

	bitmap_free(vsi->af_xdp_zc_qps);
	vsi->af_xdp_zc_qps = NULL;
	 
	devm_kfree(dev, vsi->q_vectors);
	vsi->q_vectors = NULL;
	devm_kfree(dev, vsi->tx_rings);
	vsi->tx_rings = NULL;
	devm_kfree(dev, vsi->rx_rings);
	vsi->rx_rings = NULL;
	devm_kfree(dev, vsi->txq_map);
	vsi->txq_map = NULL;
	devm_kfree(dev, vsi->rxq_map);
	vsi->rxq_map = NULL;
}

 
static void ice_vsi_free_stats(struct ice_vsi *vsi)
{
	struct ice_vsi_stats *vsi_stat;
	struct ice_pf *pf = vsi->back;
	int i;

	if (vsi->type == ICE_VSI_CHNL)
		return;
	if (!pf->vsi_stats)
		return;

	vsi_stat = pf->vsi_stats[vsi->idx];
	if (!vsi_stat)
		return;

	ice_for_each_alloc_txq(vsi, i) {
		if (vsi_stat->tx_ring_stats[i]) {
			kfree_rcu(vsi_stat->tx_ring_stats[i], rcu);
			WRITE_ONCE(vsi_stat->tx_ring_stats[i], NULL);
		}
	}

	ice_for_each_alloc_rxq(vsi, i) {
		if (vsi_stat->rx_ring_stats[i]) {
			kfree_rcu(vsi_stat->rx_ring_stats[i], rcu);
			WRITE_ONCE(vsi_stat->rx_ring_stats[i], NULL);
		}
	}

	kfree(vsi_stat->tx_ring_stats);
	kfree(vsi_stat->rx_ring_stats);
	kfree(vsi_stat);
	pf->vsi_stats[vsi->idx] = NULL;
}

 
static int ice_vsi_alloc_ring_stats(struct ice_vsi *vsi)
{
	struct ice_ring_stats **tx_ring_stats;
	struct ice_ring_stats **rx_ring_stats;
	struct ice_vsi_stats *vsi_stats;
	struct ice_pf *pf = vsi->back;
	u16 i;

	vsi_stats = pf->vsi_stats[vsi->idx];
	tx_ring_stats = vsi_stats->tx_ring_stats;
	rx_ring_stats = vsi_stats->rx_ring_stats;

	 
	ice_for_each_alloc_txq(vsi, i) {
		struct ice_ring_stats *ring_stats;
		struct ice_tx_ring *ring;

		ring = vsi->tx_rings[i];
		ring_stats = tx_ring_stats[i];

		if (!ring_stats) {
			ring_stats = kzalloc(sizeof(*ring_stats), GFP_KERNEL);
			if (!ring_stats)
				goto err_out;

			WRITE_ONCE(tx_ring_stats[i], ring_stats);
		}

		ring->ring_stats = ring_stats;
	}

	 
	ice_for_each_alloc_rxq(vsi, i) {
		struct ice_ring_stats *ring_stats;
		struct ice_rx_ring *ring;

		ring = vsi->rx_rings[i];
		ring_stats = rx_ring_stats[i];

		if (!ring_stats) {
			ring_stats = kzalloc(sizeof(*ring_stats), GFP_KERNEL);
			if (!ring_stats)
				goto err_out;

			WRITE_ONCE(rx_ring_stats[i], ring_stats);
		}

		ring->ring_stats = ring_stats;
	}

	return 0;

err_out:
	ice_vsi_free_stats(vsi);
	return -ENOMEM;
}

 
static void ice_vsi_free(struct ice_vsi *vsi)
{
	struct ice_pf *pf = NULL;
	struct device *dev;

	if (!vsi || !vsi->back)
		return;

	pf = vsi->back;
	dev = ice_pf_to_dev(pf);

	if (!pf->vsi[vsi->idx] || pf->vsi[vsi->idx] != vsi) {
		dev_dbg(dev, "vsi does not exist at pf->vsi[%d]\n", vsi->idx);
		return;
	}

	mutex_lock(&pf->sw_mutex);
	 

	pf->vsi[vsi->idx] = NULL;
	pf->next_vsi = vsi->idx;

	ice_vsi_free_stats(vsi);
	ice_vsi_free_arrays(vsi);
	mutex_unlock(&pf->sw_mutex);
	devm_kfree(dev, vsi);
}

void ice_vsi_delete(struct ice_vsi *vsi)
{
	ice_vsi_delete_from_hw(vsi);
	ice_vsi_free(vsi);
}

 
static irqreturn_t ice_msix_clean_ctrl_vsi(int __always_unused irq, void *data)
{
	struct ice_q_vector *q_vector = (struct ice_q_vector *)data;

	if (!q_vector->tx.tx_ring)
		return IRQ_HANDLED;

#define FDIR_RX_DESC_CLEAN_BUDGET 64
	ice_clean_rx_irq(q_vector->rx.rx_ring, FDIR_RX_DESC_CLEAN_BUDGET);
	ice_clean_ctrl_tx_irq(q_vector->tx.tx_ring);

	return IRQ_HANDLED;
}

 
static irqreturn_t ice_msix_clean_rings(int __always_unused irq, void *data)
{
	struct ice_q_vector *q_vector = (struct ice_q_vector *)data;

	if (!q_vector->tx.tx_ring && !q_vector->rx.rx_ring)
		return IRQ_HANDLED;

	q_vector->total_events++;

	napi_schedule(&q_vector->napi);

	return IRQ_HANDLED;
}

static irqreturn_t ice_eswitch_msix_clean_rings(int __always_unused irq, void *data)
{
	struct ice_q_vector *q_vector = (struct ice_q_vector *)data;
	struct ice_pf *pf = q_vector->vsi->back;
	struct ice_vf *vf;
	unsigned int bkt;

	if (!q_vector->tx.tx_ring && !q_vector->rx.rx_ring)
		return IRQ_HANDLED;

	rcu_read_lock();
	ice_for_each_vf_rcu(pf, bkt, vf)
		napi_schedule(&vf->repr->q_vector->napi);
	rcu_read_unlock();

	return IRQ_HANDLED;
}

 
static int ice_vsi_alloc_stat_arrays(struct ice_vsi *vsi)
{
	struct ice_vsi_stats *vsi_stat;
	struct ice_pf *pf = vsi->back;

	if (vsi->type == ICE_VSI_CHNL)
		return 0;
	if (!pf->vsi_stats)
		return -ENOENT;

	if (pf->vsi_stats[vsi->idx])
	 
		return 0;

	vsi_stat = kzalloc(sizeof(*vsi_stat), GFP_KERNEL);
	if (!vsi_stat)
		return -ENOMEM;

	vsi_stat->tx_ring_stats =
		kcalloc(vsi->alloc_txq, sizeof(*vsi_stat->tx_ring_stats),
			GFP_KERNEL);
	if (!vsi_stat->tx_ring_stats)
		goto err_alloc_tx;

	vsi_stat->rx_ring_stats =
		kcalloc(vsi->alloc_rxq, sizeof(*vsi_stat->rx_ring_stats),
			GFP_KERNEL);
	if (!vsi_stat->rx_ring_stats)
		goto err_alloc_rx;

	pf->vsi_stats[vsi->idx] = vsi_stat;

	return 0;

err_alloc_rx:
	kfree(vsi_stat->rx_ring_stats);
err_alloc_tx:
	kfree(vsi_stat->tx_ring_stats);
	kfree(vsi_stat);
	pf->vsi_stats[vsi->idx] = NULL;
	return -ENOMEM;
}

 
static int
ice_vsi_alloc_def(struct ice_vsi *vsi, struct ice_channel *ch)
{
	if (vsi->type != ICE_VSI_CHNL) {
		ice_vsi_set_num_qs(vsi);
		if (ice_vsi_alloc_arrays(vsi))
			return -ENOMEM;
	}

	switch (vsi->type) {
	case ICE_VSI_SWITCHDEV_CTRL:
		 
		vsi->irq_handler = ice_eswitch_msix_clean_rings;
		break;
	case ICE_VSI_PF:
		 
		vsi->irq_handler = ice_msix_clean_rings;
		break;
	case ICE_VSI_CTRL:
		 
		vsi->irq_handler = ice_msix_clean_ctrl_vsi;
		break;
	case ICE_VSI_CHNL:
		if (!ch)
			return -EINVAL;

		vsi->num_rxq = ch->num_rxq;
		vsi->num_txq = ch->num_txq;
		vsi->next_base_q = ch->base_q;
		break;
	case ICE_VSI_VF:
	case ICE_VSI_LB:
		break;
	default:
		ice_vsi_free_arrays(vsi);
		return -EINVAL;
	}

	return 0;
}

 
static struct ice_vsi *ice_vsi_alloc(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_vsi *vsi = NULL;

	 
	mutex_lock(&pf->sw_mutex);

	 
	if (pf->next_vsi == ICE_NO_VSI) {
		dev_dbg(dev, "out of VSI slots!\n");
		goto unlock_pf;
	}

	vsi = devm_kzalloc(dev, sizeof(*vsi), GFP_KERNEL);
	if (!vsi)
		goto unlock_pf;

	vsi->back = pf;
	set_bit(ICE_VSI_DOWN, vsi->state);

	 
	vsi->idx = pf->next_vsi;
	pf->vsi[pf->next_vsi] = vsi;

	 
	pf->next_vsi = ice_get_free_slot(pf->vsi, pf->num_alloc_vsi,
					 pf->next_vsi);

unlock_pf:
	mutex_unlock(&pf->sw_mutex);
	return vsi;
}

 
static int ice_alloc_fd_res(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	u32 g_val, b_val;

	 
	if (!test_bit(ICE_FLAG_FD_ENA, pf->flags))
		return -EPERM;

	if (!(vsi->type == ICE_VSI_PF || vsi->type == ICE_VSI_VF ||
	      vsi->type == ICE_VSI_CHNL))
		return -EPERM;

	 
	g_val = pf->hw.func_caps.fd_fltr_guar;
	if (!g_val)
		return -EPERM;

	 
	b_val = pf->hw.func_caps.fd_fltr_best_effort;
	if (!b_val)
		return -EPERM;

	 
#define ICE_PF_VSI_GFLTR	64

	 
	if (vsi->type == ICE_VSI_PF) {
		vsi->num_gfltr = g_val;
		 
		if (test_bit(ICE_FLAG_TC_MQPRIO, pf->flags)) {
			if (g_val < ICE_PF_VSI_GFLTR)
				return -EPERM;
			 
			vsi->num_gfltr = ICE_PF_VSI_GFLTR;
		}

		 
		vsi->num_bfltr = b_val;
	} else if (vsi->type == ICE_VSI_VF) {
		vsi->num_gfltr = 0;

		 
		vsi->num_bfltr = b_val;
	} else {
		struct ice_vsi *main_vsi;
		int numtc;

		main_vsi = ice_get_main_vsi(pf);
		if (!main_vsi)
			return -EPERM;

		if (!main_vsi->all_numtc)
			return -EINVAL;

		 
		numtc = main_vsi->all_numtc - ICE_CHNL_START_TC;

		 
		if (numtc < ICE_CHNL_START_TC)
			return -EPERM;

		g_val -= ICE_PF_VSI_GFLTR;
		 
		vsi->num_gfltr = g_val / numtc;

		 
		vsi->num_bfltr = b_val;
	}

	return 0;
}

 
static int ice_vsi_get_qs(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct ice_qs_cfg tx_qs_cfg = {
		.qs_mutex = &pf->avail_q_mutex,
		.pf_map = pf->avail_txqs,
		.pf_map_size = pf->max_pf_txqs,
		.q_count = vsi->alloc_txq,
		.scatter_count = ICE_MAX_SCATTER_TXQS,
		.vsi_map = vsi->txq_map,
		.vsi_map_offset = 0,
		.mapping_mode = ICE_VSI_MAP_CONTIG
	};
	struct ice_qs_cfg rx_qs_cfg = {
		.qs_mutex = &pf->avail_q_mutex,
		.pf_map = pf->avail_rxqs,
		.pf_map_size = pf->max_pf_rxqs,
		.q_count = vsi->alloc_rxq,
		.scatter_count = ICE_MAX_SCATTER_RXQS,
		.vsi_map = vsi->rxq_map,
		.vsi_map_offset = 0,
		.mapping_mode = ICE_VSI_MAP_CONTIG
	};
	int ret;

	if (vsi->type == ICE_VSI_CHNL)
		return 0;

	ret = __ice_vsi_get_qs(&tx_qs_cfg);
	if (ret)
		return ret;
	vsi->tx_mapping_mode = tx_qs_cfg.mapping_mode;

	ret = __ice_vsi_get_qs(&rx_qs_cfg);
	if (ret)
		return ret;
	vsi->rx_mapping_mode = rx_qs_cfg.mapping_mode;

	return 0;
}

 
static void ice_vsi_put_qs(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	int i;

	mutex_lock(&pf->avail_q_mutex);

	ice_for_each_alloc_txq(vsi, i) {
		clear_bit(vsi->txq_map[i], pf->avail_txqs);
		vsi->txq_map[i] = ICE_INVAL_Q_INDEX;
	}

	ice_for_each_alloc_rxq(vsi, i) {
		clear_bit(vsi->rxq_map[i], pf->avail_rxqs);
		vsi->rxq_map[i] = ICE_INVAL_Q_INDEX;
	}

	mutex_unlock(&pf->avail_q_mutex);
}

 
bool ice_is_safe_mode(struct ice_pf *pf)
{
	return !test_bit(ICE_FLAG_ADV_FEATURES, pf->flags);
}

 
bool ice_is_rdma_ena(struct ice_pf *pf)
{
	return test_bit(ICE_FLAG_RDMA_ENA, pf->flags);
}

 
static void ice_vsi_clean_rss_flow_fld(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	int status;

	if (ice_is_safe_mode(pf))
		return;

	status = ice_rem_vsi_rss_cfg(&pf->hw, vsi->idx);
	if (status)
		dev_dbg(ice_pf_to_dev(pf), "ice_rem_vsi_rss_cfg failed for vsi = %d, error = %d\n",
			vsi->vsi_num, status);
}

 
static void ice_rss_clean(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;

	dev = ice_pf_to_dev(pf);

	devm_kfree(dev, vsi->rss_hkey_user);
	devm_kfree(dev, vsi->rss_lut_user);

	ice_vsi_clean_rss_flow_fld(vsi);
	 
	if (!ice_is_safe_mode(pf))
		ice_rem_vsi_rss_list(&pf->hw, vsi->idx);
}

 
static void ice_vsi_set_rss_params(struct ice_vsi *vsi)
{
	struct ice_hw_common_caps *cap;
	struct ice_pf *pf = vsi->back;
	u16 max_rss_size;

	if (!test_bit(ICE_FLAG_RSS_ENA, pf->flags)) {
		vsi->rss_size = 1;
		return;
	}

	cap = &pf->hw.func_caps.common_cap;
	max_rss_size = BIT(cap->rss_table_entry_width);
	switch (vsi->type) {
	case ICE_VSI_CHNL:
	case ICE_VSI_PF:
		 
		vsi->rss_table_size = (u16)cap->rss_table_size;
		if (vsi->type == ICE_VSI_CHNL)
			vsi->rss_size = min_t(u16, vsi->num_rxq, max_rss_size);
		else
			vsi->rss_size = min_t(u16, num_online_cpus(),
					      max_rss_size);
		vsi->rss_lut_type = ICE_LUT_PF;
		break;
	case ICE_VSI_SWITCHDEV_CTRL:
		vsi->rss_table_size = ICE_LUT_VSI_SIZE;
		vsi->rss_size = min_t(u16, num_online_cpus(), max_rss_size);
		vsi->rss_lut_type = ICE_LUT_VSI;
		break;
	case ICE_VSI_VF:
		 
		vsi->rss_table_size = ICE_LUT_VSI_SIZE;
		vsi->rss_size = ICE_MAX_RSS_QS_PER_VF;
		vsi->rss_lut_type = ICE_LUT_VSI;
		break;
	case ICE_VSI_LB:
		break;
	default:
		dev_dbg(ice_pf_to_dev(pf), "Unsupported VSI type %s\n",
			ice_vsi_type_str(vsi->type));
		break;
	}
}

 
static void ice_set_dflt_vsi_ctx(struct ice_hw *hw, struct ice_vsi_ctx *ctxt)
{
	u32 table = 0;

	memset(&ctxt->info, 0, sizeof(ctxt->info));
	 
	ctxt->alloc_from_pool = true;
	 
	ctxt->info.sw_flags = ICE_AQ_VSI_SW_FLAG_SRC_PRUNE;
	 
	ctxt->info.sw_flags2 = ICE_AQ_VSI_SW_FLAG_LAN_ENA;
	 
	ctxt->info.inner_vlan_flags = ((ICE_AQ_VSI_INNER_VLAN_TX_MODE_ALL &
				  ICE_AQ_VSI_INNER_VLAN_TX_MODE_M) >>
				 ICE_AQ_VSI_INNER_VLAN_TX_MODE_S);
	 
	if (ice_is_dvm_ena(hw)) {
		ctxt->info.inner_vlan_flags |=
			ICE_AQ_VSI_INNER_VLAN_EMODE_NOTHING;
		ctxt->info.outer_vlan_flags =
			(ICE_AQ_VSI_OUTER_VLAN_TX_MODE_ALL <<
			 ICE_AQ_VSI_OUTER_VLAN_TX_MODE_S) &
			ICE_AQ_VSI_OUTER_VLAN_TX_MODE_M;
		ctxt->info.outer_vlan_flags |=
			(ICE_AQ_VSI_OUTER_TAG_VLAN_8100 <<
			 ICE_AQ_VSI_OUTER_TAG_TYPE_S) &
			ICE_AQ_VSI_OUTER_TAG_TYPE_M;
		ctxt->info.outer_vlan_flags |=
			FIELD_PREP(ICE_AQ_VSI_OUTER_VLAN_EMODE_M,
				   ICE_AQ_VSI_OUTER_VLAN_EMODE_NOTHING);
	}
	 
	table |= ICE_UP_TABLE_TRANSLATE(0, 0);
	table |= ICE_UP_TABLE_TRANSLATE(1, 1);
	table |= ICE_UP_TABLE_TRANSLATE(2, 2);
	table |= ICE_UP_TABLE_TRANSLATE(3, 3);
	table |= ICE_UP_TABLE_TRANSLATE(4, 4);
	table |= ICE_UP_TABLE_TRANSLATE(5, 5);
	table |= ICE_UP_TABLE_TRANSLATE(6, 6);
	table |= ICE_UP_TABLE_TRANSLATE(7, 7);
	ctxt->info.ingress_table = cpu_to_le32(table);
	ctxt->info.egress_table = cpu_to_le32(table);
	 
	ctxt->info.outer_up_table = cpu_to_le32(table);
	 
}

 
static int ice_vsi_setup_q_map(struct ice_vsi *vsi, struct ice_vsi_ctx *ctxt)
{
	u16 offset = 0, qmap = 0, tx_count = 0, rx_count = 0, pow = 0;
	u16 num_txq_per_tc, num_rxq_per_tc;
	u16 qcount_tx = vsi->alloc_txq;
	u16 qcount_rx = vsi->alloc_rxq;
	u8 netdev_tc = 0;
	int i;

	if (!vsi->tc_cfg.numtc) {
		 
		vsi->tc_cfg.numtc = 1;
		vsi->tc_cfg.ena_tc = 1;
	}

	num_rxq_per_tc = min_t(u16, qcount_rx / vsi->tc_cfg.numtc, ICE_MAX_RXQS_PER_TC);
	if (!num_rxq_per_tc)
		num_rxq_per_tc = 1;
	num_txq_per_tc = qcount_tx / vsi->tc_cfg.numtc;
	if (!num_txq_per_tc)
		num_txq_per_tc = 1;

	 
	pow = (u16)order_base_2(num_rxq_per_tc);

	 
	ice_for_each_traffic_class(i) {
		if (!(vsi->tc_cfg.ena_tc & BIT(i))) {
			 
			vsi->tc_cfg.tc_info[i].qoffset = 0;
			vsi->tc_cfg.tc_info[i].qcount_rx = 1;
			vsi->tc_cfg.tc_info[i].qcount_tx = 1;
			vsi->tc_cfg.tc_info[i].netdev_tc = 0;
			ctxt->info.tc_mapping[i] = 0;
			continue;
		}

		 
		vsi->tc_cfg.tc_info[i].qoffset = offset;
		vsi->tc_cfg.tc_info[i].qcount_rx = num_rxq_per_tc;
		vsi->tc_cfg.tc_info[i].qcount_tx = num_txq_per_tc;
		vsi->tc_cfg.tc_info[i].netdev_tc = netdev_tc++;

		qmap = ((offset << ICE_AQ_VSI_TC_Q_OFFSET_S) &
			ICE_AQ_VSI_TC_Q_OFFSET_M) |
			((pow << ICE_AQ_VSI_TC_Q_NUM_S) &
			 ICE_AQ_VSI_TC_Q_NUM_M);
		offset += num_rxq_per_tc;
		tx_count += num_txq_per_tc;
		ctxt->info.tc_mapping[i] = cpu_to_le16(qmap);
	}

	 
	if (offset)
		rx_count = offset;
	else
		rx_count = num_rxq_per_tc;

	if (rx_count > vsi->alloc_rxq) {
		dev_err(ice_pf_to_dev(vsi->back), "Trying to use more Rx queues (%u), than were allocated (%u)!\n",
			rx_count, vsi->alloc_rxq);
		return -EINVAL;
	}

	if (tx_count > vsi->alloc_txq) {
		dev_err(ice_pf_to_dev(vsi->back), "Trying to use more Tx queues (%u), than were allocated (%u)!\n",
			tx_count, vsi->alloc_txq);
		return -EINVAL;
	}

	vsi->num_txq = tx_count;
	vsi->num_rxq = rx_count;

	if (vsi->type == ICE_VSI_VF && vsi->num_txq != vsi->num_rxq) {
		dev_dbg(ice_pf_to_dev(vsi->back), "VF VSI should have same number of Tx and Rx queues. Hence making them equal\n");
		 
		vsi->num_txq = vsi->num_rxq;
	}

	 
	ctxt->info.mapping_flags |= cpu_to_le16(ICE_AQ_VSI_Q_MAP_CONTIG);
	 
	ctxt->info.q_mapping[0] = cpu_to_le16(vsi->rxq_map[0]);
	ctxt->info.q_mapping[1] = cpu_to_le16(vsi->num_rxq);

	return 0;
}

 
static void ice_set_fd_vsi_ctx(struct ice_vsi_ctx *ctxt, struct ice_vsi *vsi)
{
	u8 dflt_q_group, dflt_q_prio;
	u16 dflt_q, report_q, val;

	if (vsi->type != ICE_VSI_PF && vsi->type != ICE_VSI_CTRL &&
	    vsi->type != ICE_VSI_VF && vsi->type != ICE_VSI_CHNL)
		return;

	val = ICE_AQ_VSI_PROP_FLOW_DIR_VALID;
	ctxt->info.valid_sections |= cpu_to_le16(val);
	dflt_q = 0;
	dflt_q_group = 0;
	report_q = 0;
	dflt_q_prio = 0;

	 
	val = ICE_AQ_VSI_FD_ENABLE | ICE_AQ_VSI_FD_PROG_ENABLE;
	ctxt->info.fd_options = cpu_to_le16(val);
	 
	ctxt->info.max_fd_fltr_dedicated =
			cpu_to_le16(vsi->num_gfltr);
	 
	ctxt->info.max_fd_fltr_shared =
			cpu_to_le16(vsi->num_bfltr);
	 
	val = ((dflt_q << ICE_AQ_VSI_FD_DEF_Q_S) &
	       ICE_AQ_VSI_FD_DEF_Q_M);
	 
	val |= ((dflt_q_group << ICE_AQ_VSI_FD_DEF_GRP_S) &
		ICE_AQ_VSI_FD_DEF_GRP_M);
	ctxt->info.fd_def_q = cpu_to_le16(val);
	 
	val = ((report_q << ICE_AQ_VSI_FD_REPORT_Q_S) &
	       ICE_AQ_VSI_FD_REPORT_Q_M);
	 
	val |= ((dflt_q_prio << ICE_AQ_VSI_FD_DEF_PRIORITY_S) &
		ICE_AQ_VSI_FD_DEF_PRIORITY_M);
	ctxt->info.fd_report_opt = cpu_to_le16(val);
}

 
static void ice_set_rss_vsi_ctx(struct ice_vsi_ctx *ctxt, struct ice_vsi *vsi)
{
	u8 lut_type, hash_type;
	struct device *dev;
	struct ice_pf *pf;

	pf = vsi->back;
	dev = ice_pf_to_dev(pf);

	switch (vsi->type) {
	case ICE_VSI_CHNL:
	case ICE_VSI_PF:
		 
		lut_type = ICE_AQ_VSI_Q_OPT_RSS_LUT_PF;
		hash_type = ICE_AQ_VSI_Q_OPT_RSS_TPLZ;
		break;
	case ICE_VSI_VF:
		 
		lut_type = ICE_AQ_VSI_Q_OPT_RSS_LUT_VSI;
		hash_type = ICE_AQ_VSI_Q_OPT_RSS_TPLZ;
		break;
	default:
		dev_dbg(dev, "Unsupported VSI type %s\n",
			ice_vsi_type_str(vsi->type));
		return;
	}

	ctxt->info.q_opt_rss = ((lut_type << ICE_AQ_VSI_Q_OPT_RSS_LUT_S) &
				ICE_AQ_VSI_Q_OPT_RSS_LUT_M) |
				(hash_type & ICE_AQ_VSI_Q_OPT_RSS_HASH_M);
}

static void
ice_chnl_vsi_setup_q_map(struct ice_vsi *vsi, struct ice_vsi_ctx *ctxt)
{
	struct ice_pf *pf = vsi->back;
	u16 qcount, qmap;
	u8 offset = 0;
	int pow;

	qcount = min_t(int, vsi->num_rxq, pf->num_lan_msix);

	pow = order_base_2(qcount);
	qmap = ((offset << ICE_AQ_VSI_TC_Q_OFFSET_S) &
		 ICE_AQ_VSI_TC_Q_OFFSET_M) |
		 ((pow << ICE_AQ_VSI_TC_Q_NUM_S) &
		   ICE_AQ_VSI_TC_Q_NUM_M);

	ctxt->info.tc_mapping[0] = cpu_to_le16(qmap);
	ctxt->info.mapping_flags |= cpu_to_le16(ICE_AQ_VSI_Q_MAP_CONTIG);
	ctxt->info.q_mapping[0] = cpu_to_le16(vsi->next_base_q);
	ctxt->info.q_mapping[1] = cpu_to_le16(qcount);
}

 
static bool ice_vsi_is_vlan_pruning_ena(struct ice_vsi *vsi)
{
	return vsi->info.sw_flags2 & ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;
}

 
static int ice_vsi_init(struct ice_vsi *vsi, u32 vsi_flags)
{
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	struct ice_vsi_ctx *ctxt;
	struct device *dev;
	int ret = 0;

	dev = ice_pf_to_dev(pf);
	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	switch (vsi->type) {
	case ICE_VSI_CTRL:
	case ICE_VSI_LB:
	case ICE_VSI_PF:
		ctxt->flags = ICE_AQ_VSI_TYPE_PF;
		break;
	case ICE_VSI_SWITCHDEV_CTRL:
	case ICE_VSI_CHNL:
		ctxt->flags = ICE_AQ_VSI_TYPE_VMDQ2;
		break;
	case ICE_VSI_VF:
		ctxt->flags = ICE_AQ_VSI_TYPE_VF;
		 
		ctxt->vf_num = vsi->vf->vf_id + hw->func_caps.vf_base_id;
		break;
	default:
		ret = -ENODEV;
		goto out;
	}

	 
	if (vsi->type == ICE_VSI_CHNL) {
		struct ice_vsi *main_vsi;

		main_vsi = ice_get_main_vsi(pf);
		if (main_vsi && ice_vsi_is_vlan_pruning_ena(main_vsi))
			ctxt->info.sw_flags2 |=
				ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;
		else
			ctxt->info.sw_flags2 &=
				~ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;
	}

	ice_set_dflt_vsi_ctx(hw, ctxt);
	if (test_bit(ICE_FLAG_FD_ENA, pf->flags))
		ice_set_fd_vsi_ctx(ctxt, vsi);
	 
	if (vsi->vsw->bridge_mode == BRIDGE_MODE_VEB)
		ctxt->info.sw_flags |= ICE_AQ_VSI_SW_FLAG_ALLOW_LB;

	 
	if (test_bit(ICE_FLAG_RSS_ENA, pf->flags) &&
	    vsi->type != ICE_VSI_CTRL) {
		ice_set_rss_vsi_ctx(ctxt, vsi);
		 
		if (!(vsi_flags & ICE_VSI_FLAG_INIT))
			ctxt->info.valid_sections |=
				cpu_to_le16(ICE_AQ_VSI_PROP_Q_OPT_VALID);
	}

	ctxt->info.sw_id = vsi->port_info->sw_id;
	if (vsi->type == ICE_VSI_CHNL) {
		ice_chnl_vsi_setup_q_map(vsi, ctxt);
	} else {
		ret = ice_vsi_setup_q_map(vsi, ctxt);
		if (ret)
			goto out;

		if (!(vsi_flags & ICE_VSI_FLAG_INIT))
			 
			 
			ctxt->info.valid_sections |=
				cpu_to_le16(ICE_AQ_VSI_PROP_RXQ_MAP_VALID);
	}

	 
	if (vsi->type == ICE_VSI_PF) {
		ctxt->info.sec_flags |= ICE_AQ_VSI_SEC_FLAG_ALLOW_DEST_OVRD;
		ctxt->info.valid_sections |=
			cpu_to_le16(ICE_AQ_VSI_PROP_SECURITY_VALID);
	}

	if (vsi_flags & ICE_VSI_FLAG_INIT) {
		ret = ice_add_vsi(hw, vsi->idx, ctxt, NULL);
		if (ret) {
			dev_err(dev, "Add VSI failed, err %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		ret = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
		if (ret) {
			dev_err(dev, "Update VSI failed, err %d\n", ret);
			ret = -EIO;
			goto out;
		}
	}

	 
	vsi->info = ctxt->info;

	 
	vsi->vsi_num = ctxt->vsi_num;

out:
	kfree(ctxt);
	return ret;
}

 
static void ice_vsi_clear_rings(struct ice_vsi *vsi)
{
	int i;

	 
	if (vsi->q_vectors) {
		ice_for_each_q_vector(vsi, i) {
			struct ice_q_vector *q_vector = vsi->q_vectors[i];

			if (q_vector) {
				q_vector->tx.tx_ring = NULL;
				q_vector->rx.rx_ring = NULL;
			}
		}
	}

	if (vsi->tx_rings) {
		ice_for_each_alloc_txq(vsi, i) {
			if (vsi->tx_rings[i]) {
				kfree_rcu(vsi->tx_rings[i], rcu);
				WRITE_ONCE(vsi->tx_rings[i], NULL);
			}
		}
	}
	if (vsi->rx_rings) {
		ice_for_each_alloc_rxq(vsi, i) {
			if (vsi->rx_rings[i]) {
				kfree_rcu(vsi->rx_rings[i], rcu);
				WRITE_ONCE(vsi->rx_rings[i], NULL);
			}
		}
	}
}

 
static int ice_vsi_alloc_rings(struct ice_vsi *vsi)
{
	bool dvm_ena = ice_is_dvm_ena(&vsi->back->hw);
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	u16 i;

	dev = ice_pf_to_dev(pf);
	 
	ice_for_each_alloc_txq(vsi, i) {
		struct ice_tx_ring *ring;

		 
		ring = kzalloc(sizeof(*ring), GFP_KERNEL);

		if (!ring)
			goto err_out;

		ring->q_index = i;
		ring->reg_idx = vsi->txq_map[i];
		ring->vsi = vsi;
		ring->tx_tstamps = &pf->ptp.port.tx;
		ring->dev = dev;
		ring->count = vsi->num_tx_desc;
		ring->txq_teid = ICE_INVAL_TEID;
		if (dvm_ena)
			ring->flags |= ICE_TX_FLAGS_RING_VLAN_L2TAG2;
		else
			ring->flags |= ICE_TX_FLAGS_RING_VLAN_L2TAG1;
		WRITE_ONCE(vsi->tx_rings[i], ring);
	}

	 
	ice_for_each_alloc_rxq(vsi, i) {
		struct ice_rx_ring *ring;

		 
		ring = kzalloc(sizeof(*ring), GFP_KERNEL);
		if (!ring)
			goto err_out;

		ring->q_index = i;
		ring->reg_idx = vsi->rxq_map[i];
		ring->vsi = vsi;
		ring->netdev = vsi->netdev;
		ring->dev = dev;
		ring->count = vsi->num_rx_desc;
		ring->cached_phctime = pf->ptp.cached_phc_time;
		WRITE_ONCE(vsi->rx_rings[i], ring);
	}

	return 0;

err_out:
	ice_vsi_clear_rings(vsi);
	return -ENOMEM;
}

 
void ice_vsi_manage_rss_lut(struct ice_vsi *vsi, bool ena)
{
	u8 *lut;

	lut = kzalloc(vsi->rss_table_size, GFP_KERNEL);
	if (!lut)
		return;

	if (ena) {
		if (vsi->rss_lut_user)
			memcpy(lut, vsi->rss_lut_user, vsi->rss_table_size);
		else
			ice_fill_rss_lut(lut, vsi->rss_table_size,
					 vsi->rss_size);
	}

	ice_set_rss_lut(vsi, lut, vsi->rss_table_size);
	kfree(lut);
}

 
void ice_vsi_cfg_crc_strip(struct ice_vsi *vsi, bool disable)
{
	int i;

	ice_for_each_rxq(vsi, i)
		if (disable)
			vsi->rx_rings[i]->flags |= ICE_RX_FLAGS_CRC_STRIP_DIS;
		else
			vsi->rx_rings[i]->flags &= ~ICE_RX_FLAGS_CRC_STRIP_DIS;
}

 
int ice_vsi_cfg_rss_lut_key(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	u8 *lut, *key;
	int err;

	dev = ice_pf_to_dev(pf);
	if (vsi->type == ICE_VSI_PF && vsi->ch_rss_size &&
	    (test_bit(ICE_FLAG_TC_MQPRIO, pf->flags))) {
		vsi->rss_size = min_t(u16, vsi->rss_size, vsi->ch_rss_size);
	} else {
		vsi->rss_size = min_t(u16, vsi->rss_size, vsi->num_rxq);

		 
		if (vsi->orig_rss_size && vsi->rss_size < vsi->orig_rss_size &&
		    vsi->orig_rss_size <= vsi->num_rxq) {
			vsi->rss_size = vsi->orig_rss_size;
			 
			vsi->orig_rss_size = 0;
		}
	}

	lut = kzalloc(vsi->rss_table_size, GFP_KERNEL);
	if (!lut)
		return -ENOMEM;

	if (vsi->rss_lut_user)
		memcpy(lut, vsi->rss_lut_user, vsi->rss_table_size);
	else
		ice_fill_rss_lut(lut, vsi->rss_table_size, vsi->rss_size);

	err = ice_set_rss_lut(vsi, lut, vsi->rss_table_size);
	if (err) {
		dev_err(dev, "set_rss_lut failed, error %d\n", err);
		goto ice_vsi_cfg_rss_exit;
	}

	key = kzalloc(ICE_GET_SET_RSS_KEY_EXTEND_KEY_SIZE, GFP_KERNEL);
	if (!key) {
		err = -ENOMEM;
		goto ice_vsi_cfg_rss_exit;
	}

	if (vsi->rss_hkey_user)
		memcpy(key, vsi->rss_hkey_user, ICE_GET_SET_RSS_KEY_EXTEND_KEY_SIZE);
	else
		netdev_rss_key_fill((void *)key, ICE_GET_SET_RSS_KEY_EXTEND_KEY_SIZE);

	err = ice_set_rss_key(vsi, key);
	if (err)
		dev_err(dev, "set_rss_key failed, error %d\n", err);

	kfree(key);
ice_vsi_cfg_rss_exit:
	kfree(lut);
	return err;
}

 
static void ice_vsi_set_vf_rss_flow_fld(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	int status;

	dev = ice_pf_to_dev(pf);
	if (ice_is_safe_mode(pf)) {
		dev_dbg(dev, "Advanced RSS disabled. Package download failed, vsi num = %d\n",
			vsi->vsi_num);
		return;
	}

	status = ice_add_avf_rss_cfg(&pf->hw, vsi->idx, ICE_DEFAULT_RSS_HENA);
	if (status)
		dev_dbg(dev, "ice_add_avf_rss_cfg failed for vsi = %d, error = %d\n",
			vsi->vsi_num, status);
}

 
static void ice_vsi_set_rss_flow_fld(struct ice_vsi *vsi)
{
	u16 vsi_handle = vsi->idx, vsi_num = vsi->vsi_num;
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	struct device *dev;
	int status;

	dev = ice_pf_to_dev(pf);
	if (ice_is_safe_mode(pf)) {
		dev_dbg(dev, "Advanced RSS disabled. Package download failed, vsi num = %d\n",
			vsi_num);
		return;
	}
	 
	status = ice_add_rss_cfg(hw, vsi_handle, ICE_FLOW_HASH_IPV4,
				 ICE_FLOW_SEG_HDR_IPV4);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for ipv4 flow, vsi = %d, error = %d\n",
			vsi_num, status);

	 
	status = ice_add_rss_cfg(hw, vsi_handle, ICE_FLOW_HASH_IPV6,
				 ICE_FLOW_SEG_HDR_IPV6);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for ipv6 flow, vsi = %d, error = %d\n",
			vsi_num, status);

	 
	status = ice_add_rss_cfg(hw, vsi_handle, ICE_HASH_TCP_IPV4,
				 ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_IPV4);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for tcp4 flow, vsi = %d, error = %d\n",
			vsi_num, status);

	 
	status = ice_add_rss_cfg(hw, vsi_handle, ICE_HASH_UDP_IPV4,
				 ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_IPV4);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for udp4 flow, vsi = %d, error = %d\n",
			vsi_num, status);

	 
	status = ice_add_rss_cfg(hw, vsi_handle, ICE_FLOW_HASH_IPV4,
				 ICE_FLOW_SEG_HDR_SCTP | ICE_FLOW_SEG_HDR_IPV4);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for sctp4 flow, vsi = %d, error = %d\n",
			vsi_num, status);

	 
	status = ice_add_rss_cfg(hw, vsi_handle, ICE_HASH_TCP_IPV6,
				 ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_IPV6);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for tcp6 flow, vsi = %d, error = %d\n",
			vsi_num, status);

	 
	status = ice_add_rss_cfg(hw, vsi_handle, ICE_HASH_UDP_IPV6,
				 ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_IPV6);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for udp6 flow, vsi = %d, error = %d\n",
			vsi_num, status);

	 
	status = ice_add_rss_cfg(hw, vsi_handle, ICE_FLOW_HASH_IPV6,
				 ICE_FLOW_SEG_HDR_SCTP | ICE_FLOW_SEG_HDR_IPV6);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for sctp6 flow, vsi = %d, error = %d\n",
			vsi_num, status);

	status = ice_add_rss_cfg(hw, vsi_handle, ICE_FLOW_HASH_ESP_SPI,
				 ICE_FLOW_SEG_HDR_ESP);
	if (status)
		dev_dbg(dev, "ice_add_rss_cfg failed for esp/spi flow, vsi = %d, error = %d\n",
			vsi_num, status);
}

 
static void ice_vsi_cfg_frame_size(struct ice_vsi *vsi)
{
	if (!vsi->netdev || test_bit(ICE_FLAG_LEGACY_RX, vsi->back->flags)) {
		vsi->max_frame = ICE_MAX_FRAME_LEGACY_RX;
		vsi->rx_buf_len = ICE_RXBUF_1664;
#if (PAGE_SIZE < 8192)
	} else if (!ICE_2K_TOO_SMALL_WITH_PADDING &&
		   (vsi->netdev->mtu <= ETH_DATA_LEN)) {
		vsi->max_frame = ICE_RXBUF_1536 - NET_IP_ALIGN;
		vsi->rx_buf_len = ICE_RXBUF_1536 - NET_IP_ALIGN;
#endif
	} else {
		vsi->max_frame = ICE_AQ_SET_MAC_FRAME_SIZE_MAX;
		vsi->rx_buf_len = ICE_RXBUF_3072;
	}
}

 
bool ice_pf_state_is_nominal(struct ice_pf *pf)
{
	DECLARE_BITMAP(check_bits, ICE_STATE_NBITS) = { 0 };

	if (!pf)
		return false;

	bitmap_set(check_bits, 0, ICE_STATE_NOMINAL_CHECK_BITS);
	if (bitmap_intersects(pf->state, check_bits, ICE_STATE_NBITS))
		return false;

	return true;
}

 
void ice_update_eth_stats(struct ice_vsi *vsi)
{
	struct ice_eth_stats *prev_es, *cur_es;
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_pf *pf = vsi->back;
	u16 vsi_num = vsi->vsi_num;     

	prev_es = &vsi->eth_stats_prev;
	cur_es = &vsi->eth_stats;

	if (ice_is_reset_in_progress(pf->state))
		vsi->stat_offsets_loaded = false;

	ice_stat_update40(hw, GLV_GORCL(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->rx_bytes, &cur_es->rx_bytes);

	ice_stat_update40(hw, GLV_UPRCL(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->rx_unicast, &cur_es->rx_unicast);

	ice_stat_update40(hw, GLV_MPRCL(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->rx_multicast, &cur_es->rx_multicast);

	ice_stat_update40(hw, GLV_BPRCL(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->rx_broadcast, &cur_es->rx_broadcast);

	ice_stat_update32(hw, GLV_RDPC(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->rx_discards, &cur_es->rx_discards);

	ice_stat_update40(hw, GLV_GOTCL(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->tx_bytes, &cur_es->tx_bytes);

	ice_stat_update40(hw, GLV_UPTCL(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->tx_unicast, &cur_es->tx_unicast);

	ice_stat_update40(hw, GLV_MPTCL(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->tx_multicast, &cur_es->tx_multicast);

	ice_stat_update40(hw, GLV_BPTCL(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->tx_broadcast, &cur_es->tx_broadcast);

	ice_stat_update32(hw, GLV_TEPC(vsi_num), vsi->stat_offsets_loaded,
			  &prev_es->tx_errors, &cur_es->tx_errors);

	vsi->stat_offsets_loaded = true;
}

 
void
ice_write_qrxflxp_cntxt(struct ice_hw *hw, u16 pf_q, u32 rxdid, u32 prio,
			bool ena_ts)
{
	int regval = rd32(hw, QRXFLXP_CNTXT(pf_q));

	 
	regval &= ~(QRXFLXP_CNTXT_RXDID_IDX_M |
		    QRXFLXP_CNTXT_RXDID_PRIO_M |
		    QRXFLXP_CNTXT_TS_M);

	regval |= (rxdid << QRXFLXP_CNTXT_RXDID_IDX_S) &
		QRXFLXP_CNTXT_RXDID_IDX_M;

	regval |= (prio << QRXFLXP_CNTXT_RXDID_PRIO_S) &
		QRXFLXP_CNTXT_RXDID_PRIO_M;

	if (ena_ts)
		 
		regval |= QRXFLXP_CNTXT_TS_M;

	wr32(hw, QRXFLXP_CNTXT(pf_q), regval);
}

int ice_vsi_cfg_single_rxq(struct ice_vsi *vsi, u16 q_idx)
{
	if (q_idx >= vsi->num_rxq)
		return -EINVAL;

	return ice_vsi_cfg_rxq(vsi->rx_rings[q_idx]);
}

int ice_vsi_cfg_single_txq(struct ice_vsi *vsi, struct ice_tx_ring **tx_rings, u16 q_idx)
{
	struct ice_aqc_add_tx_qgrp *qg_buf;
	int err;

	if (q_idx >= vsi->alloc_txq || !tx_rings || !tx_rings[q_idx])
		return -EINVAL;

	qg_buf = kzalloc(struct_size(qg_buf, txqs, 1), GFP_KERNEL);
	if (!qg_buf)
		return -ENOMEM;

	qg_buf->num_txqs = 1;

	err = ice_vsi_cfg_txq(vsi, tx_rings[q_idx], qg_buf);
	kfree(qg_buf);
	return err;
}

 
int ice_vsi_cfg_rxqs(struct ice_vsi *vsi)
{
	u16 i;

	if (vsi->type == ICE_VSI_VF)
		goto setup_rings;

	ice_vsi_cfg_frame_size(vsi);
setup_rings:
	 
	ice_for_each_rxq(vsi, i) {
		int err = ice_vsi_cfg_rxq(vsi->rx_rings[i]);

		if (err)
			return err;
	}

	return 0;
}

 
static int
ice_vsi_cfg_txqs(struct ice_vsi *vsi, struct ice_tx_ring **rings, u16 count)
{
	struct ice_aqc_add_tx_qgrp *qg_buf;
	u16 q_idx = 0;
	int err = 0;

	qg_buf = kzalloc(struct_size(qg_buf, txqs, 1), GFP_KERNEL);
	if (!qg_buf)
		return -ENOMEM;

	qg_buf->num_txqs = 1;

	for (q_idx = 0; q_idx < count; q_idx++) {
		err = ice_vsi_cfg_txq(vsi, rings[q_idx], qg_buf);
		if (err)
			goto err_cfg_txqs;
	}

err_cfg_txqs:
	kfree(qg_buf);
	return err;
}

 
int ice_vsi_cfg_lan_txqs(struct ice_vsi *vsi)
{
	return ice_vsi_cfg_txqs(vsi, vsi->tx_rings, vsi->num_txq);
}

 
int ice_vsi_cfg_xdp_txqs(struct ice_vsi *vsi)
{
	int ret;
	int i;

	ret = ice_vsi_cfg_txqs(vsi, vsi->xdp_rings, vsi->num_xdp_txq);
	if (ret)
		return ret;

	ice_for_each_rxq(vsi, i)
		ice_tx_xsk_pool(vsi, i);

	return 0;
}

 
static u32 ice_intrl_usec_to_reg(u8 intrl, u8 gran)
{
	u32 val = intrl / gran;

	if (val)
		return val | GLINT_RATE_INTRL_ENA_M;
	return 0;
}

 
void ice_write_intrl(struct ice_q_vector *q_vector, u8 intrl)
{
	struct ice_hw *hw = &q_vector->vsi->back->hw;

	wr32(hw, GLINT_RATE(q_vector->reg_idx),
	     ice_intrl_usec_to_reg(intrl, ICE_INTRL_GRAN_ABOVE_25));
}

static struct ice_q_vector *ice_pull_qvec_from_rc(struct ice_ring_container *rc)
{
	switch (rc->type) {
	case ICE_RX_CONTAINER:
		if (rc->rx_ring)
			return rc->rx_ring->q_vector;
		break;
	case ICE_TX_CONTAINER:
		if (rc->tx_ring)
			return rc->tx_ring->q_vector;
		break;
	default:
		break;
	}

	return NULL;
}

 
static void __ice_write_itr(struct ice_q_vector *q_vector,
			    struct ice_ring_container *rc, u16 itr)
{
	struct ice_hw *hw = &q_vector->vsi->back->hw;

	wr32(hw, GLINT_ITR(rc->itr_idx, q_vector->reg_idx),
	     ITR_REG_ALIGN(itr) >> ICE_ITR_GRAN_S);
}

 
void ice_write_itr(struct ice_ring_container *rc, u16 itr)
{
	struct ice_q_vector *q_vector;

	q_vector = ice_pull_qvec_from_rc(rc);
	if (!q_vector)
		return;

	__ice_write_itr(q_vector, rc, itr);
}

 
void ice_set_q_vector_intrl(struct ice_q_vector *q_vector)
{
	if (ITR_IS_DYNAMIC(&q_vector->tx) || ITR_IS_DYNAMIC(&q_vector->rx)) {
		 
		ice_write_intrl(q_vector, 4);
	} else {
		ice_write_intrl(q_vector, q_vector->intrl);
	}
}

 
void ice_vsi_cfg_msix(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	u16 txq = 0, rxq = 0;
	int i, q;

	ice_for_each_q_vector(vsi, i) {
		struct ice_q_vector *q_vector = vsi->q_vectors[i];
		u16 reg_idx = q_vector->reg_idx;

		ice_cfg_itr(hw, q_vector);

		 
		for (q = 0; q < q_vector->num_ring_tx; q++) {
			ice_cfg_txq_interrupt(vsi, txq, reg_idx,
					      q_vector->tx.itr_idx);
			txq++;
		}

		for (q = 0; q < q_vector->num_ring_rx; q++) {
			ice_cfg_rxq_interrupt(vsi, rxq, reg_idx,
					      q_vector->rx.itr_idx);
			rxq++;
		}
	}
}

 
int ice_vsi_start_all_rx_rings(struct ice_vsi *vsi)
{
	return ice_vsi_ctrl_all_rx_rings(vsi, true);
}

 
int ice_vsi_stop_all_rx_rings(struct ice_vsi *vsi)
{
	return ice_vsi_ctrl_all_rx_rings(vsi, false);
}

 
static int
ice_vsi_stop_tx_rings(struct ice_vsi *vsi, enum ice_disq_rst_src rst_src,
		      u16 rel_vmvf_num, struct ice_tx_ring **rings, u16 count)
{
	u16 q_idx;

	if (vsi->num_txq > ICE_LAN_TXQ_MAX_QDIS)
		return -EINVAL;

	for (q_idx = 0; q_idx < count; q_idx++) {
		struct ice_txq_meta txq_meta = { };
		int status;

		if (!rings || !rings[q_idx])
			return -EINVAL;

		ice_fill_txq_meta(vsi, rings[q_idx], &txq_meta);
		status = ice_vsi_stop_tx_ring(vsi, rst_src, rel_vmvf_num,
					      rings[q_idx], &txq_meta);

		if (status)
			return status;
	}

	return 0;
}

 
int
ice_vsi_stop_lan_tx_rings(struct ice_vsi *vsi, enum ice_disq_rst_src rst_src,
			  u16 rel_vmvf_num)
{
	return ice_vsi_stop_tx_rings(vsi, rst_src, rel_vmvf_num, vsi->tx_rings, vsi->num_txq);
}

 
int ice_vsi_stop_xdp_tx_rings(struct ice_vsi *vsi)
{
	return ice_vsi_stop_tx_rings(vsi, ICE_NO_RESET, 0, vsi->xdp_rings, vsi->num_xdp_txq);
}

 
bool ice_vsi_is_rx_queue_active(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	int i;

	ice_for_each_rxq(vsi, i) {
		u32 rx_reg;
		int pf_q;

		pf_q = vsi->rxq_map[i];
		rx_reg = rd32(hw, QRX_CTRL(pf_q));
		if (rx_reg & QRX_CTRL_QENA_STAT_M)
			return true;
	}

	return false;
}

static void ice_vsi_set_tc_cfg(struct ice_vsi *vsi)
{
	if (!test_bit(ICE_FLAG_DCB_ENA, vsi->back->flags)) {
		vsi->tc_cfg.ena_tc = ICE_DFLT_TRAFFIC_CLASS;
		vsi->tc_cfg.numtc = 1;
		return;
	}

	 
	ice_vsi_set_dcb_tc_cfg(vsi);
}

 
void ice_cfg_sw_lldp(struct ice_vsi *vsi, bool tx, bool create)
{
	int (*eth_fltr)(struct ice_vsi *v, u16 type, u16 flag,
			enum ice_sw_fwd_act_type act);
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	int status;

	dev = ice_pf_to_dev(pf);
	eth_fltr = create ? ice_fltr_add_eth : ice_fltr_remove_eth;

	if (tx) {
		status = eth_fltr(vsi, ETH_P_LLDP, ICE_FLTR_TX,
				  ICE_DROP_PACKET);
	} else {
		if (ice_fw_supports_lldp_fltr_ctrl(&pf->hw)) {
			status = ice_lldp_fltr_add_remove(&pf->hw, vsi->vsi_num,
							  create);
		} else {
			status = eth_fltr(vsi, ETH_P_LLDP, ICE_FLTR_RX,
					  ICE_FWD_TO_VSI);
		}
	}

	if (status)
		dev_dbg(dev, "Fail %s %s LLDP rule on VSI %i error: %d\n",
			create ? "adding" : "removing", tx ? "TX" : "RX",
			vsi->vsi_num, status);
}

 
static void ice_set_agg_vsi(struct ice_vsi *vsi)
{
	struct device *dev = ice_pf_to_dev(vsi->back);
	struct ice_agg_node *agg_node_iter = NULL;
	u32 agg_id = ICE_INVALID_AGG_NODE_ID;
	struct ice_agg_node *agg_node = NULL;
	int node_offset, max_agg_nodes = 0;
	struct ice_port_info *port_info;
	struct ice_pf *pf = vsi->back;
	u32 agg_node_id_start = 0;
	int status;

	 
	port_info = pf->hw.port_info;
	if (!port_info)
		return;

	switch (vsi->type) {
	case ICE_VSI_CTRL:
	case ICE_VSI_CHNL:
	case ICE_VSI_LB:
	case ICE_VSI_PF:
	case ICE_VSI_SWITCHDEV_CTRL:
		max_agg_nodes = ICE_MAX_PF_AGG_NODES;
		agg_node_id_start = ICE_PF_AGG_NODE_ID_START;
		agg_node_iter = &pf->pf_agg_node[0];
		break;
	case ICE_VSI_VF:
		 
		max_agg_nodes = ICE_MAX_VF_AGG_NODES;
		agg_node_id_start = ICE_VF_AGG_NODE_ID_START;
		agg_node_iter = &pf->vf_agg_node[0];
		break;
	default:
		 
		dev_dbg(dev, "unexpected VSI type %s\n",
			ice_vsi_type_str(vsi->type));
		return;
	}

	 
	for (node_offset = 0; node_offset < max_agg_nodes; node_offset++) {
		 
		if (agg_node_iter->num_vsis &&
		    agg_node_iter->num_vsis == ICE_MAX_VSIS_IN_AGG_NODE) {
			agg_node_iter++;
			continue;
		}

		if (agg_node_iter->valid &&
		    agg_node_iter->agg_id != ICE_INVALID_AGG_NODE_ID) {
			agg_id = agg_node_iter->agg_id;
			agg_node = agg_node_iter;
			break;
		}

		 
		if (agg_node_iter->agg_id == ICE_INVALID_AGG_NODE_ID) {
			agg_id = node_offset + agg_node_id_start;
			agg_node = agg_node_iter;
			break;
		}
		 
		agg_node_iter++;
	}

	if (!agg_node)
		return;

	 
	if (!agg_node->valid) {
		status = ice_cfg_agg(port_info, agg_id, ICE_AGG_TYPE_AGG,
				     (u8)vsi->tc_cfg.ena_tc);
		if (status) {
			dev_err(dev, "unable to create aggregator node with agg_id %u\n",
				agg_id);
			return;
		}
		 
		agg_node->valid = true;
		agg_node->agg_id = agg_id;
	}

	 
	status = ice_move_vsi_to_agg(port_info, agg_id, vsi->idx,
				     (u8)vsi->tc_cfg.ena_tc);
	if (status) {
		dev_err(dev, "unable to move VSI idx %u into aggregator %u node",
			vsi->idx, agg_id);
		return;
	}

	 
	agg_node->num_vsis++;

	 
	vsi->agg_node = agg_node;
	dev_dbg(dev, "successfully moved VSI idx %u tc_bitmap 0x%x) into aggregator node %d which has num_vsis %u\n",
		vsi->idx, vsi->tc_cfg.ena_tc, vsi->agg_node->agg_id,
		vsi->agg_node->num_vsis);
}

static int ice_vsi_cfg_tc_lan(struct ice_pf *pf, struct ice_vsi *vsi)
{
	u16 max_txqs[ICE_MAX_TRAFFIC_CLASS] = { 0 };
	struct device *dev = ice_pf_to_dev(pf);
	int ret, i;

	 
	ice_for_each_traffic_class(i) {
		if (!(vsi->tc_cfg.ena_tc & BIT(i)))
			continue;

		if (vsi->type == ICE_VSI_CHNL) {
			if (!vsi->alloc_txq && vsi->num_txq)
				max_txqs[i] = vsi->num_txq;
			else
				max_txqs[i] = pf->num_lan_tx;
		} else {
			max_txqs[i] = vsi->alloc_txq;
		}

		if (vsi->type == ICE_VSI_PF)
			max_txqs[i] += vsi->num_xdp_txq;
	}

	dev_dbg(dev, "vsi->tc_cfg.ena_tc = %d\n", vsi->tc_cfg.ena_tc);
	ret = ice_cfg_vsi_lan(vsi->port_info, vsi->idx, vsi->tc_cfg.ena_tc,
			      max_txqs);
	if (ret) {
		dev_err(dev, "VSI %d failed lan queue config, error %d\n",
			vsi->vsi_num, ret);
		return ret;
	}

	return 0;
}

 
static int
ice_vsi_cfg_def(struct ice_vsi *vsi, struct ice_vsi_cfg_params *params)
{
	struct device *dev = ice_pf_to_dev(vsi->back);
	struct ice_pf *pf = vsi->back;
	int ret;

	vsi->vsw = pf->first_sw;

	ret = ice_vsi_alloc_def(vsi, params->ch);
	if (ret)
		return ret;

	 
	ret = ice_vsi_alloc_stat_arrays(vsi);
	if (ret)
		goto unroll_vsi_alloc;

	ice_alloc_fd_res(vsi);

	ret = ice_vsi_get_qs(vsi);
	if (ret) {
		dev_err(dev, "Failed to allocate queues. vsi->idx = %d\n",
			vsi->idx);
		goto unroll_vsi_alloc_stat;
	}

	 
	ice_vsi_set_rss_params(vsi);

	 
	ice_vsi_set_tc_cfg(vsi);

	 
	ret = ice_vsi_init(vsi, params->flags);
	if (ret)
		goto unroll_get_qs;

	ice_vsi_init_vlan_ops(vsi);

	switch (vsi->type) {
	case ICE_VSI_CTRL:
	case ICE_VSI_SWITCHDEV_CTRL:
	case ICE_VSI_PF:
		ret = ice_vsi_alloc_q_vectors(vsi);
		if (ret)
			goto unroll_vsi_init;

		ret = ice_vsi_alloc_rings(vsi);
		if (ret)
			goto unroll_vector_base;

		ret = ice_vsi_alloc_ring_stats(vsi);
		if (ret)
			goto unroll_vector_base;

		ice_vsi_map_rings_to_vectors(vsi);
		vsi->stat_offsets_loaded = false;

		if (ice_is_xdp_ena_vsi(vsi)) {
			ret = ice_vsi_determine_xdp_res(vsi);
			if (ret)
				goto unroll_vector_base;
			ret = ice_prepare_xdp_rings(vsi, vsi->xdp_prog);
			if (ret)
				goto unroll_vector_base;
		}

		 
		if (vsi->type != ICE_VSI_CTRL)
			 
			if (test_bit(ICE_FLAG_RSS_ENA, pf->flags)) {
				ice_vsi_cfg_rss_lut_key(vsi);
				ice_vsi_set_rss_flow_fld(vsi);
			}
		ice_init_arfs(vsi);
		break;
	case ICE_VSI_CHNL:
		if (test_bit(ICE_FLAG_RSS_ENA, pf->flags)) {
			ice_vsi_cfg_rss_lut_key(vsi);
			ice_vsi_set_rss_flow_fld(vsi);
		}
		break;
	case ICE_VSI_VF:
		 
		ret = ice_vsi_alloc_q_vectors(vsi);
		if (ret)
			goto unroll_vsi_init;

		ret = ice_vsi_alloc_rings(vsi);
		if (ret)
			goto unroll_alloc_q_vector;

		ret = ice_vsi_alloc_ring_stats(vsi);
		if (ret)
			goto unroll_vector_base;

		vsi->stat_offsets_loaded = false;

		 
		if (test_bit(ICE_FLAG_RSS_ENA, pf->flags)) {
			ice_vsi_cfg_rss_lut_key(vsi);
			ice_vsi_set_vf_rss_flow_fld(vsi);
		}
		break;
	case ICE_VSI_LB:
		ret = ice_vsi_alloc_rings(vsi);
		if (ret)
			goto unroll_vsi_init;

		ret = ice_vsi_alloc_ring_stats(vsi);
		if (ret)
			goto unroll_vector_base;

		break;
	default:
		 
		ret = -EINVAL;
		goto unroll_vsi_init;
	}

	return 0;

unroll_vector_base:
	 
unroll_alloc_q_vector:
	ice_vsi_free_q_vectors(vsi);
unroll_vsi_init:
	ice_vsi_delete_from_hw(vsi);
unroll_get_qs:
	ice_vsi_put_qs(vsi);
unroll_vsi_alloc_stat:
	ice_vsi_free_stats(vsi);
unroll_vsi_alloc:
	ice_vsi_free_arrays(vsi);
	return ret;
}

 
int ice_vsi_cfg(struct ice_vsi *vsi, struct ice_vsi_cfg_params *params)
{
	struct ice_pf *pf = vsi->back;
	int ret;

	if (WARN_ON(params->type == ICE_VSI_VF && !params->vf))
		return -EINVAL;

	vsi->type = params->type;
	vsi->port_info = params->pi;

	 
	vsi->vf = params->vf;

	ret = ice_vsi_cfg_def(vsi, params);
	if (ret)
		return ret;

	ret = ice_vsi_cfg_tc_lan(vsi->back, vsi);
	if (ret)
		ice_vsi_decfg(vsi);

	if (vsi->type == ICE_VSI_CTRL) {
		if (vsi->vf) {
			WARN_ON(vsi->vf->ctrl_vsi_idx != ICE_NO_VSI);
			vsi->vf->ctrl_vsi_idx = vsi->idx;
		} else {
			WARN_ON(pf->ctrl_vsi_idx != ICE_NO_VSI);
			pf->ctrl_vsi_idx = vsi->idx;
		}
	}

	return ret;
}

 
void ice_vsi_decfg(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	int err;

	 
	if (!ice_is_safe_mode(pf) && vsi->type == ICE_VSI_PF &&
	    !test_bit(ICE_FLAG_FW_LLDP_AGENT, pf->flags))
		ice_cfg_sw_lldp(vsi, false, false);

	ice_rm_vsi_lan_cfg(vsi->port_info, vsi->idx);
	err = ice_rm_vsi_rdma_cfg(vsi->port_info, vsi->idx);
	if (err)
		dev_err(ice_pf_to_dev(pf), "Failed to remove RDMA scheduler config for VSI %u, err %d\n",
			vsi->vsi_num, err);

	if (ice_is_xdp_ena_vsi(vsi))
		 
		ice_destroy_xdp_rings(vsi);

	ice_vsi_clear_rings(vsi);
	ice_vsi_free_q_vectors(vsi);
	ice_vsi_put_qs(vsi);
	ice_vsi_free_arrays(vsi);

	 

	if (vsi->type == ICE_VSI_VF &&
	    vsi->agg_node && vsi->agg_node->valid)
		vsi->agg_node->num_vsis--;
}

 
struct ice_vsi *
ice_vsi_setup(struct ice_pf *pf, struct ice_vsi_cfg_params *params)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_vsi *vsi;
	int ret;

	 
	if (WARN_ON(!(params->flags & ICE_VSI_FLAG_INIT)) ||
	    WARN_ON(!params->pi))
		return NULL;

	vsi = ice_vsi_alloc(pf);
	if (!vsi) {
		dev_err(dev, "could not allocate VSI\n");
		return NULL;
	}

	ret = ice_vsi_cfg(vsi, params);
	if (ret)
		goto err_vsi_cfg;

	 
	if (!ice_is_safe_mode(pf) && vsi->type == ICE_VSI_PF) {
		ice_fltr_add_eth(vsi, ETH_P_PAUSE, ICE_FLTR_TX,
				 ICE_DROP_PACKET);
		ice_cfg_sw_lldp(vsi, true, true);
	}

	if (!vsi->agg_node)
		ice_set_agg_vsi(vsi);

	return vsi;

err_vsi_cfg:
	ice_vsi_free(vsi);

	return NULL;
}

 
static void ice_vsi_release_msix(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	u32 txq = 0;
	u32 rxq = 0;
	int i, q;

	ice_for_each_q_vector(vsi, i) {
		struct ice_q_vector *q_vector = vsi->q_vectors[i];

		ice_write_intrl(q_vector, 0);
		for (q = 0; q < q_vector->num_ring_tx; q++) {
			ice_write_itr(&q_vector->tx, 0);
			wr32(hw, QINT_TQCTL(vsi->txq_map[txq]), 0);
			if (ice_is_xdp_ena_vsi(vsi)) {
				u32 xdp_txq = txq + vsi->num_xdp_txq;

				wr32(hw, QINT_TQCTL(vsi->txq_map[xdp_txq]), 0);
			}
			txq++;
		}

		for (q = 0; q < q_vector->num_ring_rx; q++) {
			ice_write_itr(&q_vector->rx, 0);
			wr32(hw, QINT_RQCTL(vsi->rxq_map[rxq]), 0);
			rxq++;
		}
	}

	ice_flush(hw);
}

 
void ice_vsi_free_irq(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	int i;

	if (!vsi->q_vectors || !vsi->irqs_ready)
		return;

	ice_vsi_release_msix(vsi);
	if (vsi->type == ICE_VSI_VF)
		return;

	vsi->irqs_ready = false;
	ice_free_cpu_rx_rmap(vsi);

	ice_for_each_q_vector(vsi, i) {
		int irq_num;

		irq_num = vsi->q_vectors[i]->irq.virq;

		 
		if (!vsi->q_vectors[i] ||
		    !(vsi->q_vectors[i]->num_ring_tx ||
		      vsi->q_vectors[i]->num_ring_rx))
			continue;

		 
		if (!IS_ENABLED(CONFIG_RFS_ACCEL))
			irq_set_affinity_notifier(irq_num, NULL);

		 
		irq_set_affinity_hint(irq_num, NULL);
		synchronize_irq(irq_num);
		devm_free_irq(ice_pf_to_dev(pf), irq_num, vsi->q_vectors[i]);
	}
}

 
void ice_vsi_free_tx_rings(struct ice_vsi *vsi)
{
	int i;

	if (!vsi->tx_rings)
		return;

	ice_for_each_txq(vsi, i)
		if (vsi->tx_rings[i] && vsi->tx_rings[i]->desc)
			ice_free_tx_ring(vsi->tx_rings[i]);
}

 
void ice_vsi_free_rx_rings(struct ice_vsi *vsi)
{
	int i;

	if (!vsi->rx_rings)
		return;

	ice_for_each_rxq(vsi, i)
		if (vsi->rx_rings[i] && vsi->rx_rings[i]->desc)
			ice_free_rx_ring(vsi->rx_rings[i]);
}

 
void ice_vsi_close(struct ice_vsi *vsi)
{
	if (!test_and_set_bit(ICE_VSI_DOWN, vsi->state))
		ice_down(vsi);

	ice_vsi_free_irq(vsi);
	ice_vsi_free_tx_rings(vsi);
	ice_vsi_free_rx_rings(vsi);
}

 
int ice_ena_vsi(struct ice_vsi *vsi, bool locked)
{
	int err = 0;

	if (!test_bit(ICE_VSI_NEEDS_RESTART, vsi->state))
		return 0;

	clear_bit(ICE_VSI_NEEDS_RESTART, vsi->state);

	if (vsi->netdev && vsi->type == ICE_VSI_PF) {
		if (netif_running(vsi->netdev)) {
			if (!locked)
				rtnl_lock();

			err = ice_open_internal(vsi->netdev);

			if (!locked)
				rtnl_unlock();
		}
	} else if (vsi->type == ICE_VSI_CTRL) {
		err = ice_vsi_open_ctrl(vsi);
	}

	return err;
}

 
void ice_dis_vsi(struct ice_vsi *vsi, bool locked)
{
	if (test_bit(ICE_VSI_DOWN, vsi->state))
		return;

	set_bit(ICE_VSI_NEEDS_RESTART, vsi->state);

	if (vsi->type == ICE_VSI_PF && vsi->netdev) {
		if (netif_running(vsi->netdev)) {
			if (!locked)
				rtnl_lock();

			ice_vsi_close(vsi);

			if (!locked)
				rtnl_unlock();
		} else {
			ice_vsi_close(vsi);
		}
	} else if (vsi->type == ICE_VSI_CTRL ||
		   vsi->type == ICE_VSI_SWITCHDEV_CTRL) {
		ice_vsi_close(vsi);
	}
}

 
void ice_vsi_dis_irq(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	u32 val;
	int i;

	 
	if (vsi->tx_rings) {
		ice_for_each_txq(vsi, i) {
			if (vsi->tx_rings[i]) {
				u16 reg;

				reg = vsi->tx_rings[i]->reg_idx;
				val = rd32(hw, QINT_TQCTL(reg));
				val &= ~QINT_TQCTL_CAUSE_ENA_M;
				wr32(hw, QINT_TQCTL(reg), val);
			}
		}
	}

	if (vsi->rx_rings) {
		ice_for_each_rxq(vsi, i) {
			if (vsi->rx_rings[i]) {
				u16 reg;

				reg = vsi->rx_rings[i]->reg_idx;
				val = rd32(hw, QINT_RQCTL(reg));
				val &= ~QINT_RQCTL_CAUSE_ENA_M;
				wr32(hw, QINT_RQCTL(reg), val);
			}
		}
	}

	 
	ice_for_each_q_vector(vsi, i) {
		if (!vsi->q_vectors[i])
			continue;
		wr32(hw, GLINT_DYN_CTL(vsi->q_vectors[i]->reg_idx), 0);
	}

	ice_flush(hw);

	 
	if (vsi->type == ICE_VSI_VF)
		return;

	ice_for_each_q_vector(vsi, i)
		synchronize_irq(vsi->q_vectors[i]->irq.virq);
}

 
int ice_vsi_release(struct ice_vsi *vsi)
{
	struct ice_pf *pf;

	if (!vsi->back)
		return -ENODEV;
	pf = vsi->back;

	if (test_bit(ICE_FLAG_RSS_ENA, pf->flags))
		ice_rss_clean(vsi);

	ice_vsi_close(vsi);
	ice_vsi_decfg(vsi);

	 
	if (!ice_is_reset_in_progress(pf->state))
		ice_vsi_delete(vsi);

	return 0;
}

 
static int
ice_vsi_rebuild_get_coalesce(struct ice_vsi *vsi,
			     struct ice_coalesce_stored *coalesce)
{
	int i;

	ice_for_each_q_vector(vsi, i) {
		struct ice_q_vector *q_vector = vsi->q_vectors[i];

		coalesce[i].itr_tx = q_vector->tx.itr_settings;
		coalesce[i].itr_rx = q_vector->rx.itr_settings;
		coalesce[i].intrl = q_vector->intrl;

		if (i < vsi->num_txq)
			coalesce[i].tx_valid = true;
		if (i < vsi->num_rxq)
			coalesce[i].rx_valid = true;
	}

	return vsi->num_q_vectors;
}

 
static void
ice_vsi_rebuild_set_coalesce(struct ice_vsi *vsi,
			     struct ice_coalesce_stored *coalesce, int size)
{
	struct ice_ring_container *rc;
	int i;

	if ((size && !coalesce) || !vsi)
		return;

	 
	for (i = 0; i < size && i < vsi->num_q_vectors; i++) {
		 
		if (i < vsi->alloc_rxq && coalesce[i].rx_valid) {
			rc = &vsi->q_vectors[i]->rx;
			rc->itr_settings = coalesce[i].itr_rx;
			ice_write_itr(rc, rc->itr_setting);
		} else if (i < vsi->alloc_rxq) {
			rc = &vsi->q_vectors[i]->rx;
			rc->itr_settings = coalesce[0].itr_rx;
			ice_write_itr(rc, rc->itr_setting);
		}

		if (i < vsi->alloc_txq && coalesce[i].tx_valid) {
			rc = &vsi->q_vectors[i]->tx;
			rc->itr_settings = coalesce[i].itr_tx;
			ice_write_itr(rc, rc->itr_setting);
		} else if (i < vsi->alloc_txq) {
			rc = &vsi->q_vectors[i]->tx;
			rc->itr_settings = coalesce[0].itr_tx;
			ice_write_itr(rc, rc->itr_setting);
		}

		vsi->q_vectors[i]->intrl = coalesce[i].intrl;
		ice_set_q_vector_intrl(vsi->q_vectors[i]);
	}

	 
	for (; i < vsi->num_q_vectors; i++) {
		 
		rc = &vsi->q_vectors[i]->tx;
		rc->itr_settings = coalesce[0].itr_tx;
		ice_write_itr(rc, rc->itr_setting);

		 
		rc = &vsi->q_vectors[i]->rx;
		rc->itr_settings = coalesce[0].itr_rx;
		ice_write_itr(rc, rc->itr_setting);

		vsi->q_vectors[i]->intrl = coalesce[0].intrl;
		ice_set_q_vector_intrl(vsi->q_vectors[i]);
	}
}

 
static void
ice_vsi_realloc_stat_arrays(struct ice_vsi *vsi, int prev_txq, int prev_rxq)
{
	struct ice_vsi_stats *vsi_stat;
	struct ice_pf *pf = vsi->back;
	int i;

	if (!prev_txq || !prev_rxq)
		return;
	if (vsi->type == ICE_VSI_CHNL)
		return;

	vsi_stat = pf->vsi_stats[vsi->idx];

	if (vsi->num_txq < prev_txq) {
		for (i = vsi->num_txq; i < prev_txq; i++) {
			if (vsi_stat->tx_ring_stats[i]) {
				kfree_rcu(vsi_stat->tx_ring_stats[i], rcu);
				WRITE_ONCE(vsi_stat->tx_ring_stats[i], NULL);
			}
		}
	}

	if (vsi->num_rxq < prev_rxq) {
		for (i = vsi->num_rxq; i < prev_rxq; i++) {
			if (vsi_stat->rx_ring_stats[i]) {
				kfree_rcu(vsi_stat->rx_ring_stats[i], rcu);
				WRITE_ONCE(vsi_stat->rx_ring_stats[i], NULL);
			}
		}
	}
}

 
int ice_vsi_rebuild(struct ice_vsi *vsi, u32 vsi_flags)
{
	struct ice_vsi_cfg_params params = {};
	struct ice_coalesce_stored *coalesce;
	int ret, prev_txq, prev_rxq;
	int prev_num_q_vectors = 0;
	struct ice_pf *pf;

	if (!vsi)
		return -EINVAL;

	params = ice_vsi_to_params(vsi);
	params.flags = vsi_flags;

	pf = vsi->back;
	if (WARN_ON(vsi->type == ICE_VSI_VF && !vsi->vf))
		return -EINVAL;

	coalesce = kcalloc(vsi->num_q_vectors,
			   sizeof(struct ice_coalesce_stored), GFP_KERNEL);
	if (!coalesce)
		return -ENOMEM;

	prev_num_q_vectors = ice_vsi_rebuild_get_coalesce(vsi, coalesce);

	prev_txq = vsi->num_txq;
	prev_rxq = vsi->num_rxq;

	ice_vsi_decfg(vsi);
	ret = ice_vsi_cfg_def(vsi, &params);
	if (ret)
		goto err_vsi_cfg;

	ret = ice_vsi_cfg_tc_lan(pf, vsi);
	if (ret) {
		if (vsi_flags & ICE_VSI_FLAG_INIT) {
			ret = -EIO;
			goto err_vsi_cfg_tc_lan;
		}

		kfree(coalesce);
		return ice_schedule_reset(pf, ICE_RESET_PFR);
	}

	ice_vsi_realloc_stat_arrays(vsi, prev_txq, prev_rxq);

	ice_vsi_rebuild_set_coalesce(vsi, coalesce, prev_num_q_vectors);
	kfree(coalesce);

	return 0;

err_vsi_cfg_tc_lan:
	ice_vsi_decfg(vsi);
err_vsi_cfg:
	kfree(coalesce);
	return ret;
}

 
bool ice_is_reset_in_progress(unsigned long *state)
{
	return test_bit(ICE_RESET_OICR_RECV, state) ||
	       test_bit(ICE_PFR_REQ, state) ||
	       test_bit(ICE_CORER_REQ, state) ||
	       test_bit(ICE_GLOBR_REQ, state);
}

 
int ice_wait_for_reset(struct ice_pf *pf, unsigned long timeout)
{
	long ret;

	ret = wait_event_interruptible_timeout(pf->reset_wait_queue,
					       !ice_is_reset_in_progress(pf->state),
					       timeout);
	if (ret < 0)
		return ret;
	else if (!ret)
		return -EBUSY;
	else
		return 0;
}

 
static void ice_vsi_update_q_map(struct ice_vsi *vsi, struct ice_vsi_ctx *ctx)
{
	vsi->info.mapping_flags = ctx->info.mapping_flags;
	memcpy(&vsi->info.q_mapping, &ctx->info.q_mapping,
	       sizeof(vsi->info.q_mapping));
	memcpy(&vsi->info.tc_mapping, ctx->info.tc_mapping,
	       sizeof(vsi->info.tc_mapping));
}

 
void ice_vsi_cfg_netdev_tc(struct ice_vsi *vsi, u8 ena_tc)
{
	struct net_device *netdev = vsi->netdev;
	struct ice_pf *pf = vsi->back;
	int numtc = vsi->tc_cfg.numtc;
	struct ice_dcbx_cfg *dcbcfg;
	u8 netdev_tc;
	int i;

	if (!netdev)
		return;

	 
	if (vsi->type == ICE_VSI_CHNL)
		return;

	if (!ena_tc) {
		netdev_reset_tc(netdev);
		return;
	}

	if (vsi->type == ICE_VSI_PF && ice_is_adq_active(pf))
		numtc = vsi->all_numtc;

	if (netdev_set_num_tc(netdev, numtc))
		return;

	dcbcfg = &pf->hw.port_info->qos_cfg.local_dcbx_cfg;

	ice_for_each_traffic_class(i)
		if (vsi->tc_cfg.ena_tc & BIT(i))
			netdev_set_tc_queue(netdev,
					    vsi->tc_cfg.tc_info[i].netdev_tc,
					    vsi->tc_cfg.tc_info[i].qcount_tx,
					    vsi->tc_cfg.tc_info[i].qoffset);
	 
	ice_for_each_chnl_tc(i) {
		if (!(vsi->all_enatc & BIT(i)))
			break;
		if (!vsi->mqprio_qopt.qopt.count[i])
			break;
		netdev_set_tc_queue(netdev, i,
				    vsi->mqprio_qopt.qopt.count[i],
				    vsi->mqprio_qopt.qopt.offset[i]);
	}

	if (test_bit(ICE_FLAG_TC_MQPRIO, pf->flags))
		return;

	for (i = 0; i < ICE_MAX_USER_PRIORITY; i++) {
		u8 ets_tc = dcbcfg->etscfg.prio_table[i];

		 
		netdev_tc = vsi->tc_cfg.tc_info[ets_tc].netdev_tc;
		netdev_set_prio_tc_map(netdev, i, netdev_tc);
	}
}

 
static int
ice_vsi_setup_q_map_mqprio(struct ice_vsi *vsi, struct ice_vsi_ctx *ctxt,
			   u8 ena_tc)
{
	u16 pow, offset = 0, qcount_tx = 0, qcount_rx = 0, qmap;
	u16 tc0_offset = vsi->mqprio_qopt.qopt.offset[0];
	int tc0_qcount = vsi->mqprio_qopt.qopt.count[0];
	u16 new_txq, new_rxq;
	u8 netdev_tc = 0;
	int i;

	vsi->tc_cfg.ena_tc = ena_tc ? ena_tc : 1;

	pow = order_base_2(tc0_qcount);
	qmap = ((tc0_offset << ICE_AQ_VSI_TC_Q_OFFSET_S) &
		ICE_AQ_VSI_TC_Q_OFFSET_M) |
		((pow << ICE_AQ_VSI_TC_Q_NUM_S) & ICE_AQ_VSI_TC_Q_NUM_M);

	ice_for_each_traffic_class(i) {
		if (!(vsi->tc_cfg.ena_tc & BIT(i))) {
			 
			vsi->tc_cfg.tc_info[i].qoffset = 0;
			vsi->tc_cfg.tc_info[i].qcount_rx = 1;
			vsi->tc_cfg.tc_info[i].qcount_tx = 1;
			vsi->tc_cfg.tc_info[i].netdev_tc = 0;
			ctxt->info.tc_mapping[i] = 0;
			continue;
		}

		offset = vsi->mqprio_qopt.qopt.offset[i];
		qcount_rx = vsi->mqprio_qopt.qopt.count[i];
		qcount_tx = vsi->mqprio_qopt.qopt.count[i];
		vsi->tc_cfg.tc_info[i].qoffset = offset;
		vsi->tc_cfg.tc_info[i].qcount_rx = qcount_rx;
		vsi->tc_cfg.tc_info[i].qcount_tx = qcount_tx;
		vsi->tc_cfg.tc_info[i].netdev_tc = netdev_tc++;
	}

	if (vsi->all_numtc && vsi->all_numtc != vsi->tc_cfg.numtc) {
		ice_for_each_chnl_tc(i) {
			if (!(vsi->all_enatc & BIT(i)))
				continue;
			offset = vsi->mqprio_qopt.qopt.offset[i];
			qcount_rx = vsi->mqprio_qopt.qopt.count[i];
			qcount_tx = vsi->mqprio_qopt.qopt.count[i];
		}
	}

	new_txq = offset + qcount_tx;
	if (new_txq > vsi->alloc_txq) {
		dev_err(ice_pf_to_dev(vsi->back), "Trying to use more Tx queues (%u), than were allocated (%u)!\n",
			new_txq, vsi->alloc_txq);
		return -EINVAL;
	}

	new_rxq = offset + qcount_rx;
	if (new_rxq > vsi->alloc_rxq) {
		dev_err(ice_pf_to_dev(vsi->back), "Trying to use more Rx queues (%u), than were allocated (%u)!\n",
			new_rxq, vsi->alloc_rxq);
		return -EINVAL;
	}

	 
	vsi->num_txq = new_txq;
	vsi->num_rxq = new_rxq;

	 
	ctxt->info.tc_mapping[0] = cpu_to_le16(qmap);
	ctxt->info.q_mapping[0] = cpu_to_le16(vsi->rxq_map[0]);
	ctxt->info.q_mapping[1] = cpu_to_le16(tc0_qcount);

	 
	if (tc0_qcount && tc0_qcount < vsi->num_rxq) {
		vsi->cnt_q_avail = vsi->num_rxq - tc0_qcount;
		vsi->next_base_q = tc0_qcount;
	}
	dev_dbg(ice_pf_to_dev(vsi->back), "vsi->num_txq = %d\n",  vsi->num_txq);
	dev_dbg(ice_pf_to_dev(vsi->back), "vsi->num_rxq = %d\n",  vsi->num_rxq);
	dev_dbg(ice_pf_to_dev(vsi->back), "all_numtc %u, all_enatc: 0x%04x, tc_cfg.numtc %u\n",
		vsi->all_numtc, vsi->all_enatc, vsi->tc_cfg.numtc);

	return 0;
}

 
int ice_vsi_cfg_tc(struct ice_vsi *vsi, u8 ena_tc)
{
	u16 max_txqs[ICE_MAX_TRAFFIC_CLASS] = { 0 };
	struct ice_pf *pf = vsi->back;
	struct ice_tc_cfg old_tc_cfg;
	struct ice_vsi_ctx *ctx;
	struct device *dev;
	int i, ret = 0;
	u8 num_tc = 0;

	dev = ice_pf_to_dev(pf);
	if (vsi->tc_cfg.ena_tc == ena_tc &&
	    vsi->mqprio_qopt.mode != TC_MQPRIO_MODE_CHANNEL)
		return 0;

	ice_for_each_traffic_class(i) {
		 
		if (ena_tc & BIT(i))
			num_tc++;
		 
		max_txqs[i] = vsi->alloc_txq;
		 
		if (vsi->type == ICE_VSI_CHNL &&
		    test_bit(ICE_FLAG_TC_MQPRIO, pf->flags))
			max_txqs[i] = vsi->num_txq;
	}

	memcpy(&old_tc_cfg, &vsi->tc_cfg, sizeof(old_tc_cfg));
	vsi->tc_cfg.ena_tc = ena_tc;
	vsi->tc_cfg.numtc = num_tc;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->vf_num = 0;
	ctx->info = vsi->info;

	if (vsi->type == ICE_VSI_PF &&
	    test_bit(ICE_FLAG_TC_MQPRIO, pf->flags))
		ret = ice_vsi_setup_q_map_mqprio(vsi, ctx, ena_tc);
	else
		ret = ice_vsi_setup_q_map(vsi, ctx);

	if (ret) {
		memcpy(&vsi->tc_cfg, &old_tc_cfg, sizeof(vsi->tc_cfg));
		goto out;
	}

	 
	ctx->info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_RXQ_MAP_VALID);
	ret = ice_update_vsi(&pf->hw, vsi->idx, ctx, NULL);
	if (ret) {
		dev_info(dev, "Failed VSI Update\n");
		goto out;
	}

	if (vsi->type == ICE_VSI_PF &&
	    test_bit(ICE_FLAG_TC_MQPRIO, pf->flags))
		ret = ice_cfg_vsi_lan(vsi->port_info, vsi->idx, 1, max_txqs);
	else
		ret = ice_cfg_vsi_lan(vsi->port_info, vsi->idx,
				      vsi->tc_cfg.ena_tc, max_txqs);

	if (ret) {
		dev_err(dev, "VSI %d failed TC config, error %d\n",
			vsi->vsi_num, ret);
		goto out;
	}
	ice_vsi_update_q_map(vsi, ctx);
	vsi->info.valid_sections = 0;

	ice_vsi_cfg_netdev_tc(vsi, ena_tc);
out:
	kfree(ctx);
	return ret;
}

 
static void ice_update_ring_stats(struct ice_q_stats *stats, u64 pkts, u64 bytes)
{
	stats->bytes += bytes;
	stats->pkts += pkts;
}

 
void ice_update_tx_ring_stats(struct ice_tx_ring *tx_ring, u64 pkts, u64 bytes)
{
	u64_stats_update_begin(&tx_ring->ring_stats->syncp);
	ice_update_ring_stats(&tx_ring->ring_stats->stats, pkts, bytes);
	u64_stats_update_end(&tx_ring->ring_stats->syncp);
}

 
void ice_update_rx_ring_stats(struct ice_rx_ring *rx_ring, u64 pkts, u64 bytes)
{
	u64_stats_update_begin(&rx_ring->ring_stats->syncp);
	ice_update_ring_stats(&rx_ring->ring_stats->stats, pkts, bytes);
	u64_stats_update_end(&rx_ring->ring_stats->syncp);
}

 
bool ice_is_dflt_vsi_in_use(struct ice_port_info *pi)
{
	bool exists = false;

	ice_check_if_dflt_vsi(pi, 0, &exists);
	return exists;
}

 
bool ice_is_vsi_dflt_vsi(struct ice_vsi *vsi)
{
	return ice_check_if_dflt_vsi(vsi->port_info, vsi->idx, NULL);
}

 
int ice_set_dflt_vsi(struct ice_vsi *vsi)
{
	struct device *dev;
	int status;

	if (!vsi)
		return -EINVAL;

	dev = ice_pf_to_dev(vsi->back);

	if (ice_lag_is_switchdev_running(vsi->back)) {
		dev_dbg(dev, "VSI %d passed is a part of LAG containing interfaces in switchdev mode, nothing to do\n",
			vsi->vsi_num);
		return 0;
	}

	 
	if (ice_is_vsi_dflt_vsi(vsi)) {
		dev_dbg(dev, "VSI %d passed in is already the default forwarding VSI, nothing to do\n",
			vsi->vsi_num);
		return 0;
	}

	status = ice_cfg_dflt_vsi(vsi->port_info, vsi->idx, true, ICE_FLTR_RX);
	if (status) {
		dev_err(dev, "Failed to set VSI %d as the default forwarding VSI, error %d\n",
			vsi->vsi_num, status);
		return status;
	}

	return 0;
}

 
int ice_clear_dflt_vsi(struct ice_vsi *vsi)
{
	struct device *dev;
	int status;

	if (!vsi)
		return -EINVAL;

	dev = ice_pf_to_dev(vsi->back);

	 
	if (!ice_is_dflt_vsi_in_use(vsi->port_info))
		return -ENODEV;

	status = ice_cfg_dflt_vsi(vsi->port_info, vsi->idx, false,
				  ICE_FLTR_RX);
	if (status) {
		dev_err(dev, "Failed to clear the default forwarding VSI %d, error %d\n",
			vsi->vsi_num, status);
		return -EIO;
	}

	return 0;
}

 
int ice_get_link_speed_mbps(struct ice_vsi *vsi)
{
	unsigned int link_speed;

	link_speed = vsi->port_info->phy.link_info.link_speed;

	return (int)ice_get_link_speed(fls(link_speed) - 1);
}

 
int ice_get_link_speed_kbps(struct ice_vsi *vsi)
{
	int speed_mbps;

	speed_mbps = ice_get_link_speed_mbps(vsi);

	return speed_mbps * 1000;
}

 
int ice_set_min_bw_limit(struct ice_vsi *vsi, u64 min_tx_rate)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	int status;
	int speed;

	dev = ice_pf_to_dev(pf);
	if (!vsi->port_info) {
		dev_dbg(dev, "VSI %d, type %u specified doesn't have valid port_info\n",
			vsi->idx, vsi->type);
		return -EINVAL;
	}

	speed = ice_get_link_speed_kbps(vsi);
	if (min_tx_rate > (u64)speed) {
		dev_err(dev, "invalid min Tx rate %llu Kbps specified for %s %d is greater than current link speed %u Kbps\n",
			min_tx_rate, ice_vsi_type_str(vsi->type), vsi->idx,
			speed);
		return -EINVAL;
	}

	 
	if (min_tx_rate) {
		status = ice_cfg_vsi_bw_lmt_per_tc(vsi->port_info, vsi->idx, 0,
						   ICE_MIN_BW, min_tx_rate);
		if (status) {
			dev_err(dev, "failed to set min Tx rate(%llu Kbps) for %s %d\n",
				min_tx_rate, ice_vsi_type_str(vsi->type),
				vsi->idx);
			return status;
		}

		dev_dbg(dev, "set min Tx rate(%llu Kbps) for %s\n",
			min_tx_rate, ice_vsi_type_str(vsi->type));
	} else {
		status = ice_cfg_vsi_bw_dflt_lmt_per_tc(vsi->port_info,
							vsi->idx, 0,
							ICE_MIN_BW);
		if (status) {
			dev_err(dev, "failed to clear min Tx rate configuration for %s %d\n",
				ice_vsi_type_str(vsi->type), vsi->idx);
			return status;
		}

		dev_dbg(dev, "cleared min Tx rate configuration for %s %d\n",
			ice_vsi_type_str(vsi->type), vsi->idx);
	}

	return 0;
}

 
int ice_set_max_bw_limit(struct ice_vsi *vsi, u64 max_tx_rate)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	int status;
	int speed;

	dev = ice_pf_to_dev(pf);
	if (!vsi->port_info) {
		dev_dbg(dev, "VSI %d, type %u specified doesn't have valid port_info\n",
			vsi->idx, vsi->type);
		return -EINVAL;
	}

	speed = ice_get_link_speed_kbps(vsi);
	if (max_tx_rate > (u64)speed) {
		dev_err(dev, "invalid max Tx rate %llu Kbps specified for %s %d is greater than current link speed %u Kbps\n",
			max_tx_rate, ice_vsi_type_str(vsi->type), vsi->idx,
			speed);
		return -EINVAL;
	}

	 
	if (max_tx_rate) {
		status = ice_cfg_vsi_bw_lmt_per_tc(vsi->port_info, vsi->idx, 0,
						   ICE_MAX_BW, max_tx_rate);
		if (status) {
			dev_err(dev, "failed setting max Tx rate(%llu Kbps) for %s %d\n",
				max_tx_rate, ice_vsi_type_str(vsi->type),
				vsi->idx);
			return status;
		}

		dev_dbg(dev, "set max Tx rate(%llu Kbps) for %s %d\n",
			max_tx_rate, ice_vsi_type_str(vsi->type), vsi->idx);
	} else {
		status = ice_cfg_vsi_bw_dflt_lmt_per_tc(vsi->port_info,
							vsi->idx, 0,
							ICE_MAX_BW);
		if (status) {
			dev_err(dev, "failed clearing max Tx rate configuration for %s %d\n",
				ice_vsi_type_str(vsi->type), vsi->idx);
			return status;
		}

		dev_dbg(dev, "cleared max Tx rate configuration for %s %d\n",
			ice_vsi_type_str(vsi->type), vsi->idx);
	}

	return 0;
}

 
int ice_set_link(struct ice_vsi *vsi, bool ena)
{
	struct device *dev = ice_pf_to_dev(vsi->back);
	struct ice_port_info *pi = vsi->port_info;
	struct ice_hw *hw = pi->hw;
	int status;

	if (vsi->type != ICE_VSI_PF)
		return -EINVAL;

	status = ice_aq_set_link_restart_an(pi, ena, NULL);

	 
	if (status == -EIO) {
		if (hw->adminq.sq_last_status == ICE_AQ_RC_EMODE)
			dev_dbg(dev, "can't set link to %s, err %d aq_err %s. not fatal, continuing\n",
				(ena ? "ON" : "OFF"), status,
				ice_aq_str(hw->adminq.sq_last_status));
	} else if (status) {
		dev_err(dev, "can't set link to %s, err %d aq_err %s\n",
			(ena ? "ON" : "OFF"), status,
			ice_aq_str(hw->adminq.sq_last_status));
		return status;
	}

	return 0;
}

 
int ice_vsi_add_vlan_zero(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);
	struct ice_vlan vlan;
	int err;

	vlan = ICE_VLAN(0, 0, 0);
	err = vlan_ops->add_vlan(vsi, &vlan);
	if (err && err != -EEXIST)
		return err;

	 
	if (!ice_is_dvm_ena(&vsi->back->hw))
		return 0;

	vlan = ICE_VLAN(ETH_P_8021Q, 0, 0);
	err = vlan_ops->add_vlan(vsi, &vlan);
	if (err && err != -EEXIST)
		return err;

	return 0;
}

 
int ice_vsi_del_vlan_zero(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);
	struct ice_vlan vlan;
	int err;

	vlan = ICE_VLAN(0, 0, 0);
	err = vlan_ops->del_vlan(vsi, &vlan);
	if (err && err != -EEXIST)
		return err;

	 
	if (!ice_is_dvm_ena(&vsi->back->hw))
		return 0;

	vlan = ICE_VLAN(ETH_P_8021Q, 0, 0);
	err = vlan_ops->del_vlan(vsi, &vlan);
	if (err && err != -EEXIST)
		return err;

	 
	return ice_clear_vsi_promisc(&vsi->back->hw, vsi->idx,
				    ICE_MCAST_VLAN_PROMISC_BITS, 0);
}

 
static u16 ice_vsi_num_zero_vlans(struct ice_vsi *vsi)
{
#define ICE_DVM_NUM_ZERO_VLAN_FLTRS	2
#define ICE_SVM_NUM_ZERO_VLAN_FLTRS	1
	 
	if (vsi->type == ICE_VSI_VF) {
		if (WARN_ON(!vsi->vf))
			return 0;

		if (ice_vf_is_port_vlan_ena(vsi->vf))
			return 0;
	}

	if (ice_is_dvm_ena(&vsi->back->hw))
		return ICE_DVM_NUM_ZERO_VLAN_FLTRS;
	else
		return ICE_SVM_NUM_ZERO_VLAN_FLTRS;
}

 
bool ice_vsi_has_non_zero_vlans(struct ice_vsi *vsi)
{
	return (vsi->num_vlan > ice_vsi_num_zero_vlans(vsi));
}

 
u16 ice_vsi_num_non_zero_vlans(struct ice_vsi *vsi)
{
	return (vsi->num_vlan - ice_vsi_num_zero_vlans(vsi));
}

 
bool ice_is_feature_supported(struct ice_pf *pf, enum ice_feature f)
{
	if (f < 0 || f >= ICE_F_MAX)
		return false;

	return test_bit(f, pf->features);
}

 
void ice_set_feature_support(struct ice_pf *pf, enum ice_feature f)
{
	if (f < 0 || f >= ICE_F_MAX)
		return;

	set_bit(f, pf->features);
}

 
void ice_clear_feature_support(struct ice_pf *pf, enum ice_feature f)
{
	if (f < 0 || f >= ICE_F_MAX)
		return;

	clear_bit(f, pf->features);
}

 
void ice_init_feature_support(struct ice_pf *pf)
{
	switch (pf->hw.device_id) {
	case ICE_DEV_ID_E810C_BACKPLANE:
	case ICE_DEV_ID_E810C_QSFP:
	case ICE_DEV_ID_E810C_SFP:
		ice_set_feature_support(pf, ICE_F_DSCP);
		ice_set_feature_support(pf, ICE_F_PTP_EXTTS);
		if (ice_is_e810t(&pf->hw)) {
			ice_set_feature_support(pf, ICE_F_SMA_CTRL);
			if (ice_gnss_is_gps_present(&pf->hw))
				ice_set_feature_support(pf, ICE_F_GNSS);
		}
		break;
	default:
		break;
	}
}

 
int
ice_vsi_update_security(struct ice_vsi *vsi, void (*fill)(struct ice_vsi_ctx *))
{
	struct ice_vsi_ctx ctx = { 0 };

	ctx.info = vsi->info;
	ctx.info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_SECURITY_VALID);
	fill(&ctx);

	if (ice_update_vsi(&vsi->back->hw, vsi->idx, &ctx, NULL))
		return -ENODEV;

	vsi->info = ctx.info;
	return 0;
}

 
void ice_vsi_ctx_set_antispoof(struct ice_vsi_ctx *ctx)
{
	ctx->info.sec_flags |= ICE_AQ_VSI_SEC_FLAG_ENA_MAC_ANTI_SPOOF |
			       (ICE_AQ_VSI_SEC_TX_VLAN_PRUNE_ENA <<
				ICE_AQ_VSI_SEC_TX_PRUNE_ENA_S);
}

 
void ice_vsi_ctx_clear_antispoof(struct ice_vsi_ctx *ctx)
{
	ctx->info.sec_flags &= ~ICE_AQ_VSI_SEC_FLAG_ENA_MAC_ANTI_SPOOF &
			       ~(ICE_AQ_VSI_SEC_TX_VLAN_PRUNE_ENA <<
				 ICE_AQ_VSI_SEC_TX_PRUNE_ENA_S);
}

 
void ice_vsi_ctx_set_allow_override(struct ice_vsi_ctx *ctx)
{
	ctx->info.sec_flags |= ICE_AQ_VSI_SEC_FLAG_ALLOW_DEST_OVRD;
}

 
void ice_vsi_ctx_clear_allow_override(struct ice_vsi_ctx *ctx)
{
	ctx->info.sec_flags &= ~ICE_AQ_VSI_SEC_FLAG_ALLOW_DEST_OVRD;
}

 
int
ice_vsi_update_local_lb(struct ice_vsi *vsi, bool set)
{
	struct ice_vsi_ctx ctx = {
		.info	= vsi->info,
	};

	ctx.info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_SW_VALID);
	if (set)
		ctx.info.sw_flags |= ICE_AQ_VSI_SW_FLAG_LOCAL_LB;
	else
		ctx.info.sw_flags &= ~ICE_AQ_VSI_SW_FLAG_LOCAL_LB;

	if (ice_update_vsi(&vsi->back->hw, vsi->idx, &ctx, NULL))
		return -ENODEV;

	vsi->info = ctx.info;
	return 0;
}
