
 

#include "i40e.h"

 

 
static void i40e_vc_vf_broadcast(struct i40e_pf *pf,
				 enum virtchnl_ops v_opcode,
				 int v_retval, u8 *msg,
				 u16 msglen)
{
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vf *vf = pf->vf;
	int i;

	for (i = 0; i < pf->num_alloc_vfs; i++, vf++) {
		int abs_vf_id = vf->vf_id + (int)hw->func_caps.vf_base_id;
		 
		if (!test_bit(I40E_VF_STATE_INIT, &vf->vf_states) &&
		    !test_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states))
			continue;

		 
		i40e_aq_send_msg_to_vf(hw, abs_vf_id, v_opcode, v_retval,
				       msg, msglen, NULL);
	}
}

 
static u32
i40e_vc_link_speed2mbps(enum i40e_aq_link_speed link_speed)
{
	switch (link_speed) {
	case I40E_LINK_SPEED_100MB:
		return SPEED_100;
	case I40E_LINK_SPEED_1GB:
		return SPEED_1000;
	case I40E_LINK_SPEED_2_5GB:
		return SPEED_2500;
	case I40E_LINK_SPEED_5GB:
		return SPEED_5000;
	case I40E_LINK_SPEED_10GB:
		return SPEED_10000;
	case I40E_LINK_SPEED_20GB:
		return SPEED_20000;
	case I40E_LINK_SPEED_25GB:
		return SPEED_25000;
	case I40E_LINK_SPEED_40GB:
		return SPEED_40000;
	case I40E_LINK_SPEED_UNKNOWN:
		return SPEED_UNKNOWN;
	}
	return SPEED_UNKNOWN;
}

 
static void i40e_set_vf_link_state(struct i40e_vf *vf,
				   struct virtchnl_pf_event *pfe, struct i40e_link_status *ls)
{
	u8 link_status = ls->link_info & I40E_AQ_LINK_UP;

	if (vf->link_forced)
		link_status = vf->link_up;

	if (vf->driver_caps & VIRTCHNL_VF_CAP_ADV_LINK_SPEED) {
		pfe->event_data.link_event_adv.link_speed = link_status ?
			i40e_vc_link_speed2mbps(ls->link_speed) : 0;
		pfe->event_data.link_event_adv.link_status = link_status;
	} else {
		pfe->event_data.link_event.link_speed = link_status ?
			i40e_virtchnl_link_speed(ls->link_speed) : 0;
		pfe->event_data.link_event.link_status = link_status;
	}
}

 
static void i40e_vc_notify_vf_link_state(struct i40e_vf *vf)
{
	struct virtchnl_pf_event pfe;
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_link_status *ls = &pf->hw.phy.link_info;
	int abs_vf_id = vf->vf_id + (int)hw->func_caps.vf_base_id;

	pfe.event = VIRTCHNL_EVENT_LINK_CHANGE;
	pfe.severity = PF_EVENT_SEVERITY_INFO;

	i40e_set_vf_link_state(vf, &pfe, ls);

	i40e_aq_send_msg_to_vf(hw, abs_vf_id, VIRTCHNL_OP_EVENT,
			       0, (u8 *)&pfe, sizeof(pfe), NULL);
}

 
void i40e_vc_notify_link_state(struct i40e_pf *pf)
{
	int i;

	for (i = 0; i < pf->num_alloc_vfs; i++)
		i40e_vc_notify_vf_link_state(&pf->vf[i]);
}

 
void i40e_vc_notify_reset(struct i40e_pf *pf)
{
	struct virtchnl_pf_event pfe;

	pfe.event = VIRTCHNL_EVENT_RESET_IMPENDING;
	pfe.severity = PF_EVENT_SEVERITY_CERTAIN_DOOM;
	i40e_vc_vf_broadcast(pf, VIRTCHNL_OP_EVENT, 0,
			     (u8 *)&pfe, sizeof(struct virtchnl_pf_event));
}

#ifdef CONFIG_PCI_IOV
void i40e_restore_all_vfs_msi_state(struct pci_dev *pdev)
{
	u16 vf_id;
	u16 pos;

	 
	if (!pdev->is_physfn)
		return;

	if (!pci_num_vf(pdev))
		return;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_SRIOV);
	if (pos) {
		struct pci_dev *vf_dev = NULL;

		pci_read_config_word(pdev, pos + PCI_SRIOV_VF_DID, &vf_id);
		while ((vf_dev = pci_get_device(pdev->vendor, vf_id, vf_dev))) {
			if (vf_dev->is_virtfn && vf_dev->physfn == pdev)
				pci_restore_msi_state(vf_dev);
		}
	}
}
#endif  

 
void i40e_vc_notify_vf_reset(struct i40e_vf *vf)
{
	struct virtchnl_pf_event pfe;
	int abs_vf_id;

	 
	if (!vf || vf->vf_id >= vf->pf->num_alloc_vfs)
		return;

	 
	if (!test_bit(I40E_VF_STATE_INIT, &vf->vf_states) &&
	    !test_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states))
		return;

	abs_vf_id = vf->vf_id + (int)vf->pf->hw.func_caps.vf_base_id;

	pfe.event = VIRTCHNL_EVENT_RESET_IMPENDING;
	pfe.severity = PF_EVENT_SEVERITY_CERTAIN_DOOM;
	i40e_aq_send_msg_to_vf(&vf->pf->hw, abs_vf_id, VIRTCHNL_OP_EVENT,
			       0, (u8 *)&pfe,
			       sizeof(struct virtchnl_pf_event), NULL);
}
 

 
static void i40e_vc_reset_vf(struct i40e_vf *vf, bool notify_vf)
{
	struct i40e_pf *pf = vf->pf;
	int i;

	if (notify_vf)
		i40e_vc_notify_vf_reset(vf);

	 
	for (i = 0; i < 20; i++) {
		 
		if (test_bit(__I40E_VFS_RELEASING, pf->state))
			return;
		if (i40e_reset_vf(vf, false))
			return;
		usleep_range(10000, 20000);
	}

	if (notify_vf)
		dev_warn(&vf->pf->pdev->dev,
			 "Failed to initiate reset for VF %d after 200 milliseconds\n",
			 vf->vf_id);
	else
		dev_dbg(&vf->pf->pdev->dev,
			"Failed to initiate reset for VF %d after 200 milliseconds\n",
			vf->vf_id);
}

 
static inline bool i40e_vc_isvalid_vsi_id(struct i40e_vf *vf, u16 vsi_id)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = i40e_find_vsi_from_id(pf, vsi_id);

	return (vsi && (vsi->vf_id == vf->vf_id));
}

 
static inline bool i40e_vc_isvalid_queue_id(struct i40e_vf *vf, u16 vsi_id,
					    u16 qid)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = i40e_find_vsi_from_id(pf, vsi_id);

	return (vsi && (qid < vsi->alloc_queue_pairs));
}

 
static inline bool i40e_vc_isvalid_vector_id(struct i40e_vf *vf, u32 vector_id)
{
	struct i40e_pf *pf = vf->pf;

	return vector_id < pf->hw.func_caps.num_msix_vectors_vf;
}

 

 
static u16 i40e_vc_get_pf_queue_id(struct i40e_vf *vf, u16 vsi_id,
				   u8 vsi_queue_id)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = i40e_find_vsi_from_id(pf, vsi_id);
	u16 pf_queue_id = I40E_QUEUE_END_OF_LIST;

	if (!vsi)
		return pf_queue_id;

	if (le16_to_cpu(vsi->info.mapping_flags) &
	    I40E_AQ_VSI_QUE_MAP_NONCONTIG)
		pf_queue_id =
			le16_to_cpu(vsi->info.queue_mapping[vsi_queue_id]);
	else
		pf_queue_id = le16_to_cpu(vsi->info.queue_mapping[0]) +
			      vsi_queue_id;

	return pf_queue_id;
}

 
static u16 i40e_get_real_pf_qid(struct i40e_vf *vf, u16 vsi_id, u16 queue_id)
{
	int i;

	if (vf->adq_enabled) {
		 
		for (i = 0; i < vf->num_tc; i++) {
			if (queue_id < vf->ch[i].num_qps) {
				vsi_id = vf->ch[i].vsi_id;
				break;
			}
			 
			queue_id -= vf->ch[i].num_qps;
			}
		}

	return i40e_vc_get_pf_queue_id(vf, vsi_id, queue_id);
}

 
static void i40e_config_irq_link_list(struct i40e_vf *vf, u16 vsi_id,
				      struct virtchnl_vector_map *vecmap)
{
	unsigned long linklistmap = 0, tempmap;
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	u16 vsi_queue_id, pf_queue_id;
	enum i40e_queue_type qtype;
	u16 next_q, vector_id, size;
	u32 reg, reg_idx;
	u16 itr_idx = 0;

	vector_id = vecmap->vector_id;
	 
	if (0 == vector_id)
		reg_idx = I40E_VPINT_LNKLST0(vf->vf_id);
	else
		reg_idx = I40E_VPINT_LNKLSTN(
		     ((pf->hw.func_caps.num_msix_vectors_vf - 1) * vf->vf_id) +
		     (vector_id - 1));

	if (vecmap->rxq_map == 0 && vecmap->txq_map == 0) {
		 
		wr32(hw, reg_idx, I40E_VPINT_LNKLST0_FIRSTQ_INDX_MASK);
		goto irq_list_done;
	}
	tempmap = vecmap->rxq_map;
	for_each_set_bit(vsi_queue_id, &tempmap, I40E_MAX_VSI_QP) {
		linklistmap |= (BIT(I40E_VIRTCHNL_SUPPORTED_QTYPES *
				    vsi_queue_id));
	}

	tempmap = vecmap->txq_map;
	for_each_set_bit(vsi_queue_id, &tempmap, I40E_MAX_VSI_QP) {
		linklistmap |= (BIT(I40E_VIRTCHNL_SUPPORTED_QTYPES *
				     vsi_queue_id + 1));
	}

	size = I40E_MAX_VSI_QP * I40E_VIRTCHNL_SUPPORTED_QTYPES;
	next_q = find_first_bit(&linklistmap, size);
	if (unlikely(next_q == size))
		goto irq_list_done;

	vsi_queue_id = next_q / I40E_VIRTCHNL_SUPPORTED_QTYPES;
	qtype = next_q % I40E_VIRTCHNL_SUPPORTED_QTYPES;
	pf_queue_id = i40e_get_real_pf_qid(vf, vsi_id, vsi_queue_id);
	reg = ((qtype << I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_SHIFT) | pf_queue_id);

	wr32(hw, reg_idx, reg);

	while (next_q < size) {
		switch (qtype) {
		case I40E_QUEUE_TYPE_RX:
			reg_idx = I40E_QINT_RQCTL(pf_queue_id);
			itr_idx = vecmap->rxitr_idx;
			break;
		case I40E_QUEUE_TYPE_TX:
			reg_idx = I40E_QINT_TQCTL(pf_queue_id);
			itr_idx = vecmap->txitr_idx;
			break;
		default:
			break;
		}

		next_q = find_next_bit(&linklistmap, size, next_q + 1);
		if (next_q < size) {
			vsi_queue_id = next_q / I40E_VIRTCHNL_SUPPORTED_QTYPES;
			qtype = next_q % I40E_VIRTCHNL_SUPPORTED_QTYPES;
			pf_queue_id = i40e_get_real_pf_qid(vf,
							   vsi_id,
							   vsi_queue_id);
		} else {
			pf_queue_id = I40E_QUEUE_END_OF_LIST;
			qtype = 0;
		}

		 
		reg = (vector_id) |
		    (qtype << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT) |
		    (pf_queue_id << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
		    BIT(I40E_QINT_RQCTL_CAUSE_ENA_SHIFT) |
		    (itr_idx << I40E_QINT_RQCTL_ITR_INDX_SHIFT);
		wr32(hw, reg_idx, reg);
	}

	 
	if ((vf->driver_caps & VIRTCHNL_VF_OFFLOAD_RX_POLLING) &&
	    (vector_id == 0)) {
		reg = rd32(hw, I40E_GLINT_CTL);
		if (!(reg & I40E_GLINT_CTL_DIS_AUTOMASK_VF0_MASK)) {
			reg |= I40E_GLINT_CTL_DIS_AUTOMASK_VF0_MASK;
			wr32(hw, I40E_GLINT_CTL, reg);
		}
	}

irq_list_done:
	i40e_flush(hw);
}

 
static void i40e_release_rdma_qvlist(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	struct virtchnl_rdma_qvlist_info *qvlist_info = vf->qvlist_info;
	u32 msix_vf;
	u32 i;

	if (!vf->qvlist_info)
		return;

	msix_vf = pf->hw.func_caps.num_msix_vectors_vf;
	for (i = 0; i < qvlist_info->num_vectors; i++) {
		struct virtchnl_rdma_qv_info *qv_info;
		u32 next_q_index, next_q_type;
		struct i40e_hw *hw = &pf->hw;
		u32 v_idx, reg_idx, reg;

		qv_info = &qvlist_info->qv_info[i];
		if (!qv_info)
			continue;
		v_idx = qv_info->v_idx;
		if (qv_info->ceq_idx != I40E_QUEUE_INVALID_IDX) {
			 
			reg_idx = (msix_vf - 1) * vf->vf_id + qv_info->ceq_idx;
			reg = rd32(hw, I40E_VPINT_CEQCTL(reg_idx));
			next_q_index = (reg & I40E_VPINT_CEQCTL_NEXTQ_INDX_MASK)
					>> I40E_VPINT_CEQCTL_NEXTQ_INDX_SHIFT;
			next_q_type = (reg & I40E_VPINT_CEQCTL_NEXTQ_TYPE_MASK)
					>> I40E_VPINT_CEQCTL_NEXTQ_TYPE_SHIFT;

			reg_idx = ((msix_vf - 1) * vf->vf_id) + (v_idx - 1);
			reg = (next_q_index &
			       I40E_VPINT_LNKLSTN_FIRSTQ_INDX_MASK) |
			       (next_q_type <<
			       I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_SHIFT);

			wr32(hw, I40E_VPINT_LNKLSTN(reg_idx), reg);
		}
	}
	kfree(vf->qvlist_info);
	vf->qvlist_info = NULL;
}

 
static int
i40e_config_rdma_qvlist(struct i40e_vf *vf,
			struct virtchnl_rdma_qvlist_info *qvlist_info)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	struct virtchnl_rdma_qv_info *qv_info;
	u32 v_idx, i, reg_idx, reg;
	u32 next_q_idx, next_q_type;
	size_t size;
	u32 msix_vf;
	int ret = 0;

	msix_vf = pf->hw.func_caps.num_msix_vectors_vf;

	if (qvlist_info->num_vectors > msix_vf) {
		dev_warn(&pf->pdev->dev,
			 "Incorrect number of iwarp vectors %u. Maximum %u allowed.\n",
			 qvlist_info->num_vectors,
			 msix_vf);
		ret = -EINVAL;
		goto err_out;
	}

	kfree(vf->qvlist_info);
	size = virtchnl_struct_size(vf->qvlist_info, qv_info,
				    qvlist_info->num_vectors);
	vf->qvlist_info = kzalloc(size, GFP_KERNEL);
	if (!vf->qvlist_info) {
		ret = -ENOMEM;
		goto err_out;
	}
	vf->qvlist_info->num_vectors = qvlist_info->num_vectors;

	msix_vf = pf->hw.func_caps.num_msix_vectors_vf;
	for (i = 0; i < qvlist_info->num_vectors; i++) {
		qv_info = &qvlist_info->qv_info[i];
		if (!qv_info)
			continue;

		 
		if (!i40e_vc_isvalid_vector_id(vf, qv_info->v_idx)) {
			ret = -EINVAL;
			goto err_free;
		}

		v_idx = qv_info->v_idx;

		vf->qvlist_info->qv_info[i] = *qv_info;

		reg_idx = ((msix_vf - 1) * vf->vf_id) + (v_idx - 1);
		 
		reg = rd32(hw, I40E_VPINT_LNKLSTN(reg_idx));
		next_q_idx = ((reg & I40E_VPINT_LNKLSTN_FIRSTQ_INDX_MASK) >>
				I40E_VPINT_LNKLSTN_FIRSTQ_INDX_SHIFT);
		next_q_type = ((reg & I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_MASK) >>
				I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_SHIFT);

		if (qv_info->ceq_idx != I40E_QUEUE_INVALID_IDX) {
			reg_idx = (msix_vf - 1) * vf->vf_id + qv_info->ceq_idx;
			reg = (I40E_VPINT_CEQCTL_CAUSE_ENA_MASK |
			(v_idx << I40E_VPINT_CEQCTL_MSIX_INDX_SHIFT) |
			(qv_info->itr_idx << I40E_VPINT_CEQCTL_ITR_INDX_SHIFT) |
			(next_q_type << I40E_VPINT_CEQCTL_NEXTQ_TYPE_SHIFT) |
			(next_q_idx << I40E_VPINT_CEQCTL_NEXTQ_INDX_SHIFT));
			wr32(hw, I40E_VPINT_CEQCTL(reg_idx), reg);

			reg_idx = ((msix_vf - 1) * vf->vf_id) + (v_idx - 1);
			reg = (qv_info->ceq_idx &
			       I40E_VPINT_LNKLSTN_FIRSTQ_INDX_MASK) |
			       (I40E_QUEUE_TYPE_PE_CEQ <<
			       I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_SHIFT);
			wr32(hw, I40E_VPINT_LNKLSTN(reg_idx), reg);
		}

		if (qv_info->aeq_idx != I40E_QUEUE_INVALID_IDX) {
			reg = (I40E_VPINT_AEQCTL_CAUSE_ENA_MASK |
			(v_idx << I40E_VPINT_AEQCTL_MSIX_INDX_SHIFT) |
			(qv_info->itr_idx << I40E_VPINT_AEQCTL_ITR_INDX_SHIFT));

			wr32(hw, I40E_VPINT_AEQCTL(vf->vf_id), reg);
		}
	}

	return 0;
err_free:
	kfree(vf->qvlist_info);
	vf->qvlist_info = NULL;
err_out:
	return ret;
}

 
static int i40e_config_vsi_tx_queue(struct i40e_vf *vf, u16 vsi_id,
				    u16 vsi_queue_id,
				    struct virtchnl_txq_info *info)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_hmc_obj_txq tx_ctx;
	struct i40e_vsi *vsi;
	u16 pf_queue_id;
	u32 qtx_ctl;
	int ret = 0;

	if (!i40e_vc_isvalid_vsi_id(vf, info->vsi_id)) {
		ret = -ENOENT;
		goto error_context;
	}
	pf_queue_id = i40e_vc_get_pf_queue_id(vf, vsi_id, vsi_queue_id);
	vsi = i40e_find_vsi_from_id(pf, vsi_id);
	if (!vsi) {
		ret = -ENOENT;
		goto error_context;
	}

	 
	memset(&tx_ctx, 0, sizeof(struct i40e_hmc_obj_txq));

	 
	tx_ctx.base = info->dma_ring_addr / 128;
	tx_ctx.qlen = info->ring_len;
	tx_ctx.rdylist = le16_to_cpu(vsi->info.qs_handle[0]);
	tx_ctx.rdylist_act = 0;
	tx_ctx.head_wb_ena = info->headwb_enabled;
	tx_ctx.head_wb_addr = info->dma_headwb_addr;

	 
	ret = i40e_clear_lan_tx_queue_context(hw, pf_queue_id);
	if (ret) {
		dev_err(&pf->pdev->dev,
			"Failed to clear VF LAN Tx queue context %d, error: %d\n",
			pf_queue_id, ret);
		ret = -ENOENT;
		goto error_context;
	}

	 
	ret = i40e_set_lan_tx_queue_context(hw, pf_queue_id, &tx_ctx);
	if (ret) {
		dev_err(&pf->pdev->dev,
			"Failed to set VF LAN Tx queue context %d error: %d\n",
			pf_queue_id, ret);
		ret = -ENOENT;
		goto error_context;
	}

	 
	qtx_ctl = I40E_QTX_CTL_VF_QUEUE;
	qtx_ctl |= ((hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT)
		    & I40E_QTX_CTL_PF_INDX_MASK);
	qtx_ctl |= (((vf->vf_id + hw->func_caps.vf_base_id)
		     << I40E_QTX_CTL_VFVM_INDX_SHIFT)
		    & I40E_QTX_CTL_VFVM_INDX_MASK);
	wr32(hw, I40E_QTX_CTL(pf_queue_id), qtx_ctl);
	i40e_flush(hw);

error_context:
	return ret;
}

 
static int i40e_config_vsi_rx_queue(struct i40e_vf *vf, u16 vsi_id,
				    u16 vsi_queue_id,
				    struct virtchnl_rxq_info *info)
{
	u16 pf_queue_id = i40e_vc_get_pf_queue_id(vf, vsi_id, vsi_queue_id);
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = pf->vsi[vf->lan_vsi_idx];
	struct i40e_hw *hw = &pf->hw;
	struct i40e_hmc_obj_rxq rx_ctx;
	int ret = 0;

	 
	memset(&rx_ctx, 0, sizeof(struct i40e_hmc_obj_rxq));

	 
	rx_ctx.base = info->dma_ring_addr / 128;
	rx_ctx.qlen = info->ring_len;

	if (info->splithdr_enabled) {
		rx_ctx.hsplit_0 = I40E_RX_SPLIT_L2      |
				  I40E_RX_SPLIT_IP      |
				  I40E_RX_SPLIT_TCP_UDP |
				  I40E_RX_SPLIT_SCTP;
		 
		if (info->hdr_size > ((2 * 1024) - 64)) {
			ret = -EINVAL;
			goto error_param;
		}
		rx_ctx.hbuff = info->hdr_size >> I40E_RXQ_CTX_HBUFF_SHIFT;

		 
		rx_ctx.dtype = I40E_RX_DTYPE_HEADER_SPLIT;
	}

	 
	if (info->databuffer_size > ((16 * 1024) - 128)) {
		ret = -EINVAL;
		goto error_param;
	}
	rx_ctx.dbuff = info->databuffer_size >> I40E_RXQ_CTX_DBUFF_SHIFT;

	 
	if (info->max_pkt_size >= (16 * 1024) || info->max_pkt_size < 64) {
		ret = -EINVAL;
		goto error_param;
	}
	rx_ctx.rxmax = info->max_pkt_size;

	 
	if (vsi->info.pvid)
		rx_ctx.rxmax += VLAN_HLEN;

	 
	rx_ctx.dsize = 1;

	 
	rx_ctx.lrxqthresh = 1;
	rx_ctx.crcstrip = 1;
	rx_ctx.prefena = 1;
	rx_ctx.l2tsel = 1;

	 
	ret = i40e_clear_lan_rx_queue_context(hw, pf_queue_id);
	if (ret) {
		dev_err(&pf->pdev->dev,
			"Failed to clear VF LAN Rx queue context %d, error: %d\n",
			pf_queue_id, ret);
		ret = -ENOENT;
		goto error_param;
	}

	 
	ret = i40e_set_lan_rx_queue_context(hw, pf_queue_id, &rx_ctx);
	if (ret) {
		dev_err(&pf->pdev->dev,
			"Failed to set VF LAN Rx queue context %d error: %d\n",
			pf_queue_id, ret);
		ret = -ENOENT;
		goto error_param;
	}

error_param:
	return ret;
}

 
static int i40e_alloc_vsi_res(struct i40e_vf *vf, u8 idx)
{
	struct i40e_mac_filter *f = NULL;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi;
	u64 max_tx_rate = 0;
	int ret = 0;

	vsi = i40e_vsi_setup(pf, I40E_VSI_SRIOV, pf->vsi[pf->lan_vsi]->seid,
			     vf->vf_id);

	if (!vsi) {
		dev_err(&pf->pdev->dev,
			"add vsi failed for VF %d, aq_err %d\n",
			vf->vf_id, pf->hw.aq.asq_last_status);
		ret = -ENOENT;
		goto error_alloc_vsi_res;
	}

	if (!idx) {
		u64 hena = i40e_pf_get_default_rss_hena(pf);
		u8 broadcast[ETH_ALEN];

		vf->lan_vsi_idx = vsi->idx;
		vf->lan_vsi_id = vsi->id;
		 
		if (vf->port_vlan_id)
			i40e_vsi_add_pvid(vsi, vf->port_vlan_id);

		spin_lock_bh(&vsi->mac_filter_hash_lock);
		if (is_valid_ether_addr(vf->default_lan_addr.addr)) {
			f = i40e_add_mac_filter(vsi,
						vf->default_lan_addr.addr);
			if (!f)
				dev_info(&pf->pdev->dev,
					 "Could not add MAC filter %pM for VF %d\n",
					vf->default_lan_addr.addr, vf->vf_id);
		}
		eth_broadcast_addr(broadcast);
		f = i40e_add_mac_filter(vsi, broadcast);
		if (!f)
			dev_info(&pf->pdev->dev,
				 "Could not allocate VF broadcast filter\n");
		spin_unlock_bh(&vsi->mac_filter_hash_lock);
		wr32(&pf->hw, I40E_VFQF_HENA1(0, vf->vf_id), (u32)hena);
		wr32(&pf->hw, I40E_VFQF_HENA1(1, vf->vf_id), (u32)(hena >> 32));
		 
		ret = i40e_sync_vsi_filters(vsi);
		if (ret)
			dev_err(&pf->pdev->dev, "Unable to program ucast filters\n");
	}

	 
	if (vf->adq_enabled) {
		vf->ch[idx].vsi_idx = vsi->idx;
		vf->ch[idx].vsi_id = vsi->id;
	}

	 
	if (vf->tx_rate) {
		max_tx_rate = vf->tx_rate;
	} else if (vf->ch[idx].max_tx_rate) {
		max_tx_rate = vf->ch[idx].max_tx_rate;
	}

	if (max_tx_rate) {
		max_tx_rate = div_u64(max_tx_rate, I40E_BW_CREDIT_DIVISOR);
		ret = i40e_aq_config_vsi_bw_limit(&pf->hw, vsi->seid,
						  max_tx_rate, 0, NULL);
		if (ret)
			dev_err(&pf->pdev->dev, "Unable to set tx rate, VF %d, error code %d.\n",
				vf->vf_id, ret);
	}

error_alloc_vsi_res:
	return ret;
}

 
static void i40e_map_pf_queues_to_vsi(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	u32 reg, num_tc = 1;  
	u16 vsi_id, qps;
	int i, j;

	if (vf->adq_enabled)
		num_tc = vf->num_tc;

	for (i = 0; i < num_tc; i++) {
		if (vf->adq_enabled) {
			qps = vf->ch[i].num_qps;
			vsi_id =  vf->ch[i].vsi_id;
		} else {
			qps = pf->vsi[vf->lan_vsi_idx]->alloc_queue_pairs;
			vsi_id = vf->lan_vsi_id;
		}

		for (j = 0; j < 7; j++) {
			if (j * 2 >= qps) {
				 
				reg = 0x07FF07FF;
			} else {
				u16 qid = i40e_vc_get_pf_queue_id(vf,
								  vsi_id,
								  j * 2);
				reg = qid;
				qid = i40e_vc_get_pf_queue_id(vf, vsi_id,
							      (j * 2) + 1);
				reg |= qid << 16;
			}
			i40e_write_rx_ctl(hw,
					  I40E_VSILAN_QTABLE(j, vsi_id),
					  reg);
		}
	}
}

 
static void i40e_map_pf_to_vf_queues(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	u32 reg, total_qps = 0;
	u32 qps, num_tc = 1;  
	u16 vsi_id, qid;
	int i, j;

	if (vf->adq_enabled)
		num_tc = vf->num_tc;

	for (i = 0; i < num_tc; i++) {
		if (vf->adq_enabled) {
			qps = vf->ch[i].num_qps;
			vsi_id =  vf->ch[i].vsi_id;
		} else {
			qps = pf->vsi[vf->lan_vsi_idx]->alloc_queue_pairs;
			vsi_id = vf->lan_vsi_id;
		}

		for (j = 0; j < qps; j++) {
			qid = i40e_vc_get_pf_queue_id(vf, vsi_id, j);

			reg = (qid & I40E_VPLAN_QTABLE_QINDEX_MASK);
			wr32(hw, I40E_VPLAN_QTABLE(total_qps, vf->vf_id),
			     reg);
			total_qps++;
		}
	}
}

 
static void i40e_enable_vf_mappings(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	u32 reg;

	 
	i40e_write_rx_ctl(hw, I40E_VSILAN_QBASE(vf->lan_vsi_id),
			  I40E_VSILAN_QBASE_VSIQTABLE_ENA_MASK);

	 
	reg = I40E_VPLAN_MAPENA_TXRX_ENA_MASK;
	wr32(hw, I40E_VPLAN_MAPENA(vf->vf_id), reg);

	i40e_map_pf_to_vf_queues(vf);
	i40e_map_pf_queues_to_vsi(vf);

	i40e_flush(hw);
}

 
static void i40e_disable_vf_mappings(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	int i;

	 
	wr32(hw, I40E_VPLAN_MAPENA(vf->vf_id), 0);
	for (i = 0; i < I40E_MAX_VSI_QP; i++)
		wr32(hw, I40E_VPLAN_QTABLE(i, vf->vf_id),
		     I40E_QUEUE_END_OF_LIST);
	i40e_flush(hw);
}

 
static void i40e_free_vf_res(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	u32 reg_idx, reg;
	int i, j, msix_vf;

	 
	clear_bit(I40E_VF_STATE_INIT, &vf->vf_states);

	 
	if (vf->num_queue_pairs > I40E_DEFAULT_QUEUES_PER_VF) {
		pf->queues_left += vf->num_queue_pairs -
				   I40E_DEFAULT_QUEUES_PER_VF;
	}

	 
	if (vf->lan_vsi_idx) {
		i40e_vsi_release(pf->vsi[vf->lan_vsi_idx]);
		vf->lan_vsi_idx = 0;
		vf->lan_vsi_id = 0;
	}

	 
	if (vf->adq_enabled && vf->ch[0].vsi_idx) {
		for (j = 0; j < vf->num_tc; j++) {
			 
			if (j)
				i40e_vsi_release(pf->vsi[vf->ch[j].vsi_idx]);
			vf->ch[j].vsi_idx = 0;
			vf->ch[j].vsi_id = 0;
		}
	}
	msix_vf = pf->hw.func_caps.num_msix_vectors_vf;

	 
	for (i = 0; i < msix_vf; i++) {
		 
		if (0 == i)
			reg_idx = I40E_VFINT_DYN_CTL0(vf->vf_id);
		else
			reg_idx = I40E_VFINT_DYN_CTLN(((msix_vf - 1) *
						      (vf->vf_id))
						     + (i - 1));
		wr32(hw, reg_idx, I40E_VFINT_DYN_CTLN_CLEARPBA_MASK);
		i40e_flush(hw);
	}

	 
	for (i = 0; i < msix_vf; i++) {
		 
		if (0 == i)
			reg_idx = I40E_VPINT_LNKLST0(vf->vf_id);
		else
			reg_idx = I40E_VPINT_LNKLSTN(((msix_vf - 1) *
						      (vf->vf_id))
						     + (i - 1));
		reg = (I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_MASK |
		       I40E_VPINT_LNKLSTN_FIRSTQ_INDX_MASK);
		wr32(hw, reg_idx, reg);
		i40e_flush(hw);
	}
	 
	vf->num_queue_pairs = 0;
	clear_bit(I40E_VF_STATE_MC_PROMISC, &vf->vf_states);
	clear_bit(I40E_VF_STATE_UC_PROMISC, &vf->vf_states);
}

 
static int i40e_alloc_vf_res(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	int total_queue_pairs = 0;
	int ret, idx;

	if (vf->num_req_queues &&
	    vf->num_req_queues <= pf->queues_left + I40E_DEFAULT_QUEUES_PER_VF)
		pf->num_vf_qps = vf->num_req_queues;
	else
		pf->num_vf_qps = I40E_DEFAULT_QUEUES_PER_VF;

	 
	ret = i40e_alloc_vsi_res(vf, 0);
	if (ret)
		goto error_alloc;
	total_queue_pairs += pf->vsi[vf->lan_vsi_idx]->alloc_queue_pairs;

	 
	if (vf->adq_enabled) {
		if (pf->queues_left >=
		    (I40E_MAX_VF_QUEUES - I40E_DEFAULT_QUEUES_PER_VF)) {
			 
			for (idx = 1; idx < vf->num_tc; idx++) {
				ret = i40e_alloc_vsi_res(vf, idx);
				if (ret)
					goto error_alloc;
			}
			 
			total_queue_pairs = I40E_MAX_VF_QUEUES;
		} else {
			dev_info(&pf->pdev->dev, "VF %d: Not enough queues to allocate, disabling ADq\n",
				 vf->vf_id);
			vf->adq_enabled = false;
		}
	}

	 
	if (total_queue_pairs > I40E_DEFAULT_QUEUES_PER_VF)
		pf->queues_left -=
			total_queue_pairs - I40E_DEFAULT_QUEUES_PER_VF;

	if (vf->trusted)
		set_bit(I40E_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps);
	else
		clear_bit(I40E_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps);

	 
	vf->num_queue_pairs = total_queue_pairs;

	 
	set_bit(I40E_VF_STATE_INIT, &vf->vf_states);

error_alloc:
	if (ret)
		i40e_free_vf_res(vf);

	return ret;
}

#define VF_DEVICE_STATUS 0xAA
#define VF_TRANS_PENDING_MASK 0x20
 
static int i40e_quiesce_vf_pci(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	int vf_abs_id, i;
	u32 reg;

	vf_abs_id = vf->vf_id + hw->func_caps.vf_base_id;

	wr32(hw, I40E_PF_PCI_CIAA,
	     VF_DEVICE_STATUS | (vf_abs_id << I40E_PF_PCI_CIAA_VF_NUM_SHIFT));
	for (i = 0; i < 100; i++) {
		reg = rd32(hw, I40E_PF_PCI_CIAD);
		if ((reg & VF_TRANS_PENDING_MASK) == 0)
			return 0;
		udelay(1);
	}
	return -EIO;
}

 
static int __i40e_getnum_vf_vsi_vlan_filters(struct i40e_vsi *vsi)
{
	struct i40e_mac_filter *f;
	u16 num_vlans = 0, bkt;

	hash_for_each(vsi->mac_filter_hash, bkt, f, hlist) {
		if (f->vlan >= 0 && f->vlan <= I40E_MAX_VLANID)
			num_vlans++;
	}

	return num_vlans;
}

 
static int i40e_getnum_vf_vsi_vlan_filters(struct i40e_vsi *vsi)
{
	int num_vlans;

	spin_lock_bh(&vsi->mac_filter_hash_lock);
	num_vlans = __i40e_getnum_vf_vsi_vlan_filters(vsi);
	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	return num_vlans;
}

 
static void i40e_get_vlan_list_sync(struct i40e_vsi *vsi, u16 *num_vlans,
				    s16 **vlan_list)
{
	struct i40e_mac_filter *f;
	int i = 0;
	int bkt;

	spin_lock_bh(&vsi->mac_filter_hash_lock);
	*num_vlans = __i40e_getnum_vf_vsi_vlan_filters(vsi);
	*vlan_list = kcalloc(*num_vlans, sizeof(**vlan_list), GFP_ATOMIC);
	if (!(*vlan_list))
		goto err;

	hash_for_each(vsi->mac_filter_hash, bkt, f, hlist) {
		if (f->vlan < 0 || f->vlan > I40E_MAX_VLANID)
			continue;
		(*vlan_list)[i++] = f->vlan;
	}
err:
	spin_unlock_bh(&vsi->mac_filter_hash_lock);
}

 
static int
i40e_set_vsi_promisc(struct i40e_vf *vf, u16 seid, bool multi_enable,
		     bool unicast_enable, s16 *vl, u16 num_vlans)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	int aq_ret, aq_tmp = 0;
	int i;

	 
	if (!num_vlans || !vl) {
		aq_ret = i40e_aq_set_vsi_multicast_promiscuous(hw, seid,
							       multi_enable,
							       NULL);
		if (aq_ret) {
			int aq_err = pf->hw.aq.asq_last_status;

			dev_err(&pf->pdev->dev,
				"VF %d failed to set multicast promiscuous mode err %pe aq_err %s\n",
				vf->vf_id,
				ERR_PTR(aq_ret),
				i40e_aq_str(&pf->hw, aq_err));

			return aq_ret;
		}

		aq_ret = i40e_aq_set_vsi_unicast_promiscuous(hw, seid,
							     unicast_enable,
							     NULL, true);

		if (aq_ret) {
			int aq_err = pf->hw.aq.asq_last_status;

			dev_err(&pf->pdev->dev,
				"VF %d failed to set unicast promiscuous mode err %pe aq_err %s\n",
				vf->vf_id,
				ERR_PTR(aq_ret),
				i40e_aq_str(&pf->hw, aq_err));
		}

		return aq_ret;
	}

	for (i = 0; i < num_vlans; i++) {
		aq_ret = i40e_aq_set_vsi_mc_promisc_on_vlan(hw, seid,
							    multi_enable,
							    vl[i], NULL);
		if (aq_ret) {
			int aq_err = pf->hw.aq.asq_last_status;

			dev_err(&pf->pdev->dev,
				"VF %d failed to set multicast promiscuous mode err %pe aq_err %s\n",
				vf->vf_id,
				ERR_PTR(aq_ret),
				i40e_aq_str(&pf->hw, aq_err));

			if (!aq_tmp)
				aq_tmp = aq_ret;
		}

		aq_ret = i40e_aq_set_vsi_uc_promisc_on_vlan(hw, seid,
							    unicast_enable,
							    vl[i], NULL);
		if (aq_ret) {
			int aq_err = pf->hw.aq.asq_last_status;

			dev_err(&pf->pdev->dev,
				"VF %d failed to set unicast promiscuous mode err %pe aq_err %s\n",
				vf->vf_id,
				ERR_PTR(aq_ret),
				i40e_aq_str(&pf->hw, aq_err));

			if (!aq_tmp)
				aq_tmp = aq_ret;
		}
	}

	if (aq_tmp)
		aq_ret = aq_tmp;

	return aq_ret;
}

 
static int i40e_config_vf_promiscuous_mode(struct i40e_vf *vf,
					   u16 vsi_id,
					   bool allmulti,
					   bool alluni)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi;
	int aq_ret = 0;
	u16 num_vlans;
	s16 *vl;

	vsi = i40e_find_vsi_from_id(pf, vsi_id);
	if (!i40e_vc_isvalid_vsi_id(vf, vsi_id) || !vsi)
		return -EINVAL;

	if (vf->port_vlan_id) {
		aq_ret = i40e_set_vsi_promisc(vf, vsi->seid, allmulti,
					      alluni, &vf->port_vlan_id, 1);
		return aq_ret;
	} else if (i40e_getnum_vf_vsi_vlan_filters(vsi)) {
		i40e_get_vlan_list_sync(vsi, &num_vlans, &vl);

		if (!vl)
			return -ENOMEM;

		aq_ret = i40e_set_vsi_promisc(vf, vsi->seid, allmulti, alluni,
					      vl, num_vlans);
		kfree(vl);
		return aq_ret;
	}

	 
	aq_ret = i40e_set_vsi_promisc(vf, vsi->seid, allmulti, alluni,
				      NULL, 0);
	return aq_ret;
}

 
static int i40e_sync_vfr_reset(struct i40e_hw *hw, int vf_id)
{
	u32 reg;
	int i;

	for (i = 0; i < I40E_VFR_WAIT_COUNT; i++) {
		reg = rd32(hw, I40E_VFINT_ICR0_ENA(vf_id)) &
			   I40E_VFINT_ICR0_ADMINQ_MASK;
		if (reg)
			return 0;

		usleep_range(100, 200);
	}

	return -EAGAIN;
}

 
static void i40e_trigger_vf_reset(struct i40e_vf *vf, bool flr)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	u32 reg, reg_idx, bit_idx;
	bool vf_active;
	u32 radq;

	 
	vf_active = test_and_clear_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states);

	 
	clear_bit(I40E_VF_STATE_INIT, &vf->vf_states);

	 
	if (!flr) {
		 
		radq = rd32(hw, I40E_VFINT_ICR0_ENA(vf->vf_id)) &
			    I40E_VFINT_ICR0_ADMINQ_MASK;
		if (vf_active && !radq)
			 
			if (i40e_sync_vfr_reset(hw, vf->vf_id))
				dev_info(&pf->pdev->dev,
					 "Reset VF %d never finished\n",
				vf->vf_id);

		 
		reg = rd32(hw, I40E_VPGEN_VFRTRIG(vf->vf_id));
		reg |= I40E_VPGEN_VFRTRIG_VFSWR_MASK;
		wr32(hw, I40E_VPGEN_VFRTRIG(vf->vf_id), reg);
		i40e_flush(hw);
	}
	 
	reg_idx = (hw->func_caps.vf_base_id + vf->vf_id) / 32;
	bit_idx = (hw->func_caps.vf_base_id + vf->vf_id) % 32;
	wr32(hw, I40E_GLGEN_VFLRSTAT(reg_idx), BIT(bit_idx));
	i40e_flush(hw);

	if (i40e_quiesce_vf_pci(vf))
		dev_err(&pf->pdev->dev, "VF %d PCI transactions stuck\n",
			vf->vf_id);
}

 
static void i40e_cleanup_reset_vf(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	u32 reg;

	 
	i40e_config_vf_promiscuous_mode(vf, vf->lan_vsi_id, false, false);

	 
	i40e_free_vf_res(vf);

	 
	reg = rd32(hw, I40E_VPGEN_VFRTRIG(vf->vf_id));
	reg &= ~I40E_VPGEN_VFRTRIG_VFSWR_MASK;
	wr32(hw, I40E_VPGEN_VFRTRIG(vf->vf_id), reg);

	 
	if (!i40e_alloc_vf_res(vf)) {
		int abs_vf_id = vf->vf_id + hw->func_caps.vf_base_id;
		i40e_enable_vf_mappings(vf);
		set_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states);
		clear_bit(I40E_VF_STATE_DISABLED, &vf->vf_states);
		 
		if (!test_and_clear_bit(I40E_VF_STATE_PRE_ENABLE,
					&vf->vf_states))
			i40e_notify_client_of_vf_reset(pf, abs_vf_id);
		vf->num_vlan = 0;
	}

	 
	wr32(hw, I40E_VFGEN_RSTAT1(vf->vf_id), VIRTCHNL_VFR_VFACTIVE);
}

 
bool i40e_reset_vf(struct i40e_vf *vf, bool flr)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	bool rsd = false;
	u32 reg;
	int i;

	if (test_bit(__I40E_VF_RESETS_DISABLED, pf->state))
		return true;

	 
	if (test_bit(__I40E_VF_DISABLE, pf->state))
		return true;

	 
	if (test_and_set_bit(I40E_VF_STATE_RESETTING, &vf->vf_states))
		return true;

	i40e_trigger_vf_reset(vf, flr);

	 
	for (i = 0; i < 10; i++) {
		 
		usleep_range(10000, 20000);
		reg = rd32(hw, I40E_VPGEN_VFRSTAT(vf->vf_id));
		if (reg & I40E_VPGEN_VFRSTAT_VFRD_MASK) {
			rsd = true;
			break;
		}
	}

	if (flr)
		usleep_range(10000, 20000);

	if (!rsd)
		dev_err(&pf->pdev->dev, "VF reset check timeout on VF %d\n",
			vf->vf_id);
	usleep_range(10000, 20000);

	 
	if (vf->lan_vsi_idx != 0)
		i40e_vsi_stop_rings(pf->vsi[vf->lan_vsi_idx]);

	i40e_cleanup_reset_vf(vf);

	i40e_flush(hw);
	usleep_range(20000, 40000);
	clear_bit(I40E_VF_STATE_RESETTING, &vf->vf_states);

	return true;
}

 
bool i40e_reset_all_vfs(struct i40e_pf *pf, bool flr)
{
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vf *vf;
	int i, v;
	u32 reg;

	 
	if (!pf->num_alloc_vfs)
		return false;

	 
	if (test_and_set_bit(__I40E_VF_DISABLE, pf->state))
		return false;

	 
	for (v = 0; v < pf->num_alloc_vfs; v++) {
		vf = &pf->vf[v];
		 
		if (!test_bit(I40E_VF_STATE_RESETTING, &vf->vf_states))
			i40e_trigger_vf_reset(&pf->vf[v], flr);
	}

	 
	for (i = 0, v = 0; i < 10 && v < pf->num_alloc_vfs; i++) {
		usleep_range(10000, 20000);

		 
		while (v < pf->num_alloc_vfs) {
			vf = &pf->vf[v];
			if (!test_bit(I40E_VF_STATE_RESETTING, &vf->vf_states)) {
				reg = rd32(hw, I40E_VPGEN_VFRSTAT(vf->vf_id));
				if (!(reg & I40E_VPGEN_VFRSTAT_VFRD_MASK))
					break;
			}

			 
			v++;
		}
	}

	if (flr)
		usleep_range(10000, 20000);

	 
	if (v < pf->num_alloc_vfs)
		dev_err(&pf->pdev->dev, "VF reset check timeout on VF %d\n",
			pf->vf[v].vf_id);
	usleep_range(10000, 20000);

	 
	for (v = 0; v < pf->num_alloc_vfs; v++) {
		 
		if (pf->vf[v].lan_vsi_idx == 0)
			continue;

		 
		if (test_bit(I40E_VF_STATE_RESETTING, &vf->vf_states))
			continue;

		i40e_vsi_stop_rings_no_wait(pf->vsi[pf->vf[v].lan_vsi_idx]);
	}

	 
	for (v = 0; v < pf->num_alloc_vfs; v++) {
		 
		if (pf->vf[v].lan_vsi_idx == 0)
			continue;

		 
		if (test_bit(I40E_VF_STATE_RESETTING, &vf->vf_states))
			continue;

		i40e_vsi_wait_queues_disabled(pf->vsi[pf->vf[v].lan_vsi_idx]);
	}

	 
	mdelay(50);

	 
	for (v = 0; v < pf->num_alloc_vfs; v++) {
		 
		if (test_bit(I40E_VF_STATE_RESETTING, &vf->vf_states))
			continue;

		i40e_cleanup_reset_vf(&pf->vf[v]);
	}

	i40e_flush(hw);
	usleep_range(20000, 40000);
	clear_bit(__I40E_VF_DISABLE, pf->state);

	return true;
}

 
void i40e_free_vfs(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u32 reg_idx, bit_idx;
	int i, tmp, vf_id;

	if (!pf->vf)
		return;

	set_bit(__I40E_VFS_RELEASING, pf->state);
	while (test_and_set_bit(__I40E_VF_DISABLE, pf->state))
		usleep_range(1000, 2000);

	i40e_notify_client_of_vf_enable(pf, 0);

	 
	if (!pci_vfs_assigned(pf->pdev))
		pci_disable_sriov(pf->pdev);
	else
		dev_warn(&pf->pdev->dev, "VFs are assigned - not disabling SR-IOV\n");

	 
	for (i = 0; i < pf->num_alloc_vfs; i++) {
		if (test_bit(I40E_VF_STATE_INIT, &pf->vf[i].vf_states))
			continue;

		i40e_vsi_stop_rings_no_wait(pf->vsi[pf->vf[i].lan_vsi_idx]);
	}

	for (i = 0; i < pf->num_alloc_vfs; i++) {
		if (test_bit(I40E_VF_STATE_INIT, &pf->vf[i].vf_states))
			continue;

		i40e_vsi_wait_queues_disabled(pf->vsi[pf->vf[i].lan_vsi_idx]);
	}

	 
	tmp = pf->num_alloc_vfs;
	pf->num_alloc_vfs = 0;
	for (i = 0; i < tmp; i++) {
		if (test_bit(I40E_VF_STATE_INIT, &pf->vf[i].vf_states))
			i40e_free_vf_res(&pf->vf[i]);
		 
		i40e_disable_vf_mappings(&pf->vf[i]);
	}

	kfree(pf->vf);
	pf->vf = NULL;

	 
	if (!pci_vfs_assigned(pf->pdev)) {
		 
		for (vf_id = 0; vf_id < tmp; vf_id++) {
			reg_idx = (hw->func_caps.vf_base_id + vf_id) / 32;
			bit_idx = (hw->func_caps.vf_base_id + vf_id) % 32;
			wr32(hw, I40E_GLGEN_VFLRSTAT(reg_idx), BIT(bit_idx));
		}
	}
	clear_bit(__I40E_VF_DISABLE, pf->state);
	clear_bit(__I40E_VFS_RELEASING, pf->state);
}

#ifdef CONFIG_PCI_IOV
 
int i40e_alloc_vfs(struct i40e_pf *pf, u16 num_alloc_vfs)
{
	struct i40e_vf *vfs;
	int i, ret = 0;

	 
	i40e_irq_dynamic_disable_icr0(pf);

	 
	if (pci_num_vf(pf->pdev) != num_alloc_vfs) {
		ret = pci_enable_sriov(pf->pdev, num_alloc_vfs);
		if (ret) {
			pf->flags &= ~I40E_FLAG_VEB_MODE_ENABLED;
			pf->num_alloc_vfs = 0;
			goto err_iov;
		}
	}
	 
	vfs = kcalloc(num_alloc_vfs, sizeof(struct i40e_vf), GFP_KERNEL);
	if (!vfs) {
		ret = -ENOMEM;
		goto err_alloc;
	}
	pf->vf = vfs;

	 
	for (i = 0; i < num_alloc_vfs; i++) {
		vfs[i].pf = pf;
		vfs[i].parent_type = I40E_SWITCH_ELEMENT_TYPE_VEB;
		vfs[i].vf_id = i;

		 
		set_bit(I40E_VIRTCHNL_VF_CAP_L2, &vfs[i].vf_caps);
		vfs[i].spoofchk = true;

		set_bit(I40E_VF_STATE_PRE_ENABLE, &vfs[i].vf_states);

	}
	pf->num_alloc_vfs = num_alloc_vfs;

	 
	i40e_reset_all_vfs(pf, false);

	i40e_notify_client_of_vf_enable(pf, num_alloc_vfs);

err_alloc:
	if (ret)
		i40e_free_vfs(pf);
err_iov:
	 
	i40e_irq_dynamic_enable_icr0(pf);
	return ret;
}

#endif
 
static int i40e_pci_sriov_enable(struct pci_dev *pdev, int num_vfs)
{
#ifdef CONFIG_PCI_IOV
	struct i40e_pf *pf = pci_get_drvdata(pdev);
	int pre_existing_vfs = pci_num_vf(pdev);
	int err = 0;

	if (test_bit(__I40E_TESTING, pf->state)) {
		dev_warn(&pdev->dev,
			 "Cannot enable SR-IOV virtual functions while the device is undergoing diagnostic testing\n");
		err = -EPERM;
		goto err_out;
	}

	if (pre_existing_vfs && pre_existing_vfs != num_vfs)
		i40e_free_vfs(pf);
	else if (pre_existing_vfs && pre_existing_vfs == num_vfs)
		goto out;

	if (num_vfs > pf->num_req_vfs) {
		dev_warn(&pdev->dev, "Unable to enable %d VFs. Limited to %d VFs due to device resource constraints.\n",
			 num_vfs, pf->num_req_vfs);
		err = -EPERM;
		goto err_out;
	}

	dev_info(&pdev->dev, "Allocating %d VFs.\n", num_vfs);
	err = i40e_alloc_vfs(pf, num_vfs);
	if (err) {
		dev_warn(&pdev->dev, "Failed to enable SR-IOV: %d\n", err);
		goto err_out;
	}

out:
	return num_vfs;

err_out:
	return err;
#endif
	return 0;
}

 
int i40e_pci_sriov_configure(struct pci_dev *pdev, int num_vfs)
{
	struct i40e_pf *pf = pci_get_drvdata(pdev);
	int ret = 0;

	if (test_and_set_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state)) {
		dev_warn(&pdev->dev, "Unable to configure VFs, other operation is pending.\n");
		return -EAGAIN;
	}

	if (num_vfs) {
		if (!(pf->flags & I40E_FLAG_VEB_MODE_ENABLED)) {
			pf->flags |= I40E_FLAG_VEB_MODE_ENABLED;
			i40e_do_reset_safe(pf, I40E_PF_RESET_AND_REBUILD_FLAG);
		}
		ret = i40e_pci_sriov_enable(pdev, num_vfs);
		goto sriov_configure_out;
	}

	if (!pci_vfs_assigned(pf->pdev)) {
		i40e_free_vfs(pf);
		pf->flags &= ~I40E_FLAG_VEB_MODE_ENABLED;
		i40e_do_reset_safe(pf, I40E_PF_RESET_AND_REBUILD_FLAG);
	} else {
		dev_warn(&pdev->dev, "Unable to free VFs because some are assigned to VMs.\n");
		ret = -EINVAL;
		goto sriov_configure_out;
	}
sriov_configure_out:
	clear_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state);
	return ret;
}

 

 
static int i40e_vc_send_msg_to_vf(struct i40e_vf *vf, u32 v_opcode,
				  u32 v_retval, u8 *msg, u16 msglen)
{
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	int abs_vf_id;
	int aq_ret;

	 
	if (!vf || vf->vf_id >= vf->pf->num_alloc_vfs)
		return -EINVAL;

	pf = vf->pf;
	hw = &pf->hw;
	abs_vf_id = vf->vf_id + hw->func_caps.vf_base_id;

	aq_ret = i40e_aq_send_msg_to_vf(hw, abs_vf_id,	v_opcode, v_retval,
					msg, msglen, NULL);
	if (aq_ret) {
		dev_info(&pf->pdev->dev,
			 "Unable to send the message to VF %d aq_err %d\n",
			 vf->vf_id, pf->hw.aq.asq_last_status);
		return -EIO;
	}

	return 0;
}

 
static int i40e_vc_send_resp_to_vf(struct i40e_vf *vf,
				   enum virtchnl_ops opcode,
				   int retval)
{
	return i40e_vc_send_msg_to_vf(vf, opcode, retval, NULL, 0);
}

 
static bool i40e_sync_vf_state(struct i40e_vf *vf, enum i40e_vf_states state)
{
	int i;

	 
	for (i = 0; i < I40E_VF_STATE_WAIT_COUNT; i++) {
		if (test_bit(state, &vf->vf_states))
			return true;
		usleep_range(10000, 20000);
	}

	return test_bit(state, &vf->vf_states);
}

 
static int i40e_vc_get_version_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_version_info info = {
		VIRTCHNL_VERSION_MAJOR, VIRTCHNL_VERSION_MINOR
	};

	vf->vf_ver = *(struct virtchnl_version_info *)msg;
	 
	if (VF_IS_V10(&vf->vf_ver))
		info.minor = VIRTCHNL_VERSION_MINOR_NO_VF_CAPS;
	return i40e_vc_send_msg_to_vf(vf, VIRTCHNL_OP_VERSION,
				      0, (u8 *)&info,
				      sizeof(struct virtchnl_version_info));
}

 
static void i40e_del_qch(struct i40e_vf *vf)
{
	struct i40e_pf *pf = vf->pf;
	int i;

	 
	for (i = 1; i < vf->num_tc; i++) {
		if (vf->ch[i].vsi_idx) {
			i40e_vsi_release(pf->vsi[vf->ch[i].vsi_idx]);
			vf->ch[i].vsi_idx = 0;
			vf->ch[i].vsi_id = 0;
		}
	}
}

 
static u16 i40e_vc_get_max_frame_size(struct i40e_vf *vf)
{
	u16 max_frame_size = vf->pf->hw.phy.link_info.max_frame_size;

	if (vf->port_vlan_id)
		max_frame_size -= VLAN_HLEN;

	return max_frame_size;
}

 
static int i40e_vc_get_vf_resources_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_vf_resource *vfres = NULL;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi;
	int num_vsis = 1;
	int aq_ret = 0;
	size_t len = 0;
	int ret;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_INIT)) {
		aq_ret = -EINVAL;
		goto err;
	}

	len = virtchnl_struct_size(vfres, vsi_res, num_vsis);
	vfres = kzalloc(len, GFP_KERNEL);
	if (!vfres) {
		aq_ret = -ENOMEM;
		len = 0;
		goto err;
	}
	if (VF_IS_V11(&vf->vf_ver))
		vf->driver_caps = *(u32 *)msg;
	else
		vf->driver_caps = VIRTCHNL_VF_OFFLOAD_L2 |
				  VIRTCHNL_VF_OFFLOAD_RSS_REG |
				  VIRTCHNL_VF_OFFLOAD_VLAN;

	vfres->vf_cap_flags = VIRTCHNL_VF_OFFLOAD_L2;
	vfres->vf_cap_flags |= VIRTCHNL_VF_CAP_ADV_LINK_SPEED;
	vsi = pf->vsi[vf->lan_vsi_idx];
	if (!vsi->info.pvid)
		vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_VLAN;

	if (i40e_vf_client_capable(pf, vf->vf_id) &&
	    (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_RDMA)) {
		vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_RDMA;
		set_bit(I40E_VF_STATE_RDMAENA, &vf->vf_states);
	} else {
		clear_bit(I40E_VF_STATE_RDMAENA, &vf->vf_states);
	}

	if (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_RSS_PF;
	} else {
		if ((pf->hw_features & I40E_HW_RSS_AQ_CAPABLE) &&
		    (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_RSS_AQ))
			vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_RSS_AQ;
		else
			vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_RSS_REG;
	}

	if (pf->hw_features & I40E_HW_MULTIPLE_TCP_UDP_RSS_PCTYPE) {
		if (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_RSS_PCTYPE_V2)
			vfres->vf_cap_flags |=
				VIRTCHNL_VF_OFFLOAD_RSS_PCTYPE_V2;
	}

	if (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_ENCAP)
		vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_ENCAP;

	if ((pf->hw_features & I40E_HW_OUTER_UDP_CSUM_CAPABLE) &&
	    (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_ENCAP_CSUM))
		vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_ENCAP_CSUM;

	if (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_RX_POLLING) {
		if (pf->flags & I40E_FLAG_MFP_ENABLED) {
			dev_err(&pf->pdev->dev,
				"VF %d requested polling mode: this feature is supported only when the device is running in single function per port (SFP) mode\n",
				 vf->vf_id);
			aq_ret = -EINVAL;
			goto err;
		}
		vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_RX_POLLING;
	}

	if (pf->hw_features & I40E_HW_WB_ON_ITR_CAPABLE) {
		if (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_WB_ON_ITR)
			vfres->vf_cap_flags |=
					VIRTCHNL_VF_OFFLOAD_WB_ON_ITR;
	}

	if (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_REQ_QUEUES)
		vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_REQ_QUEUES;

	if (vf->driver_caps & VIRTCHNL_VF_OFFLOAD_ADQ)
		vfres->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_ADQ;

	vfres->num_vsis = num_vsis;
	vfres->num_queue_pairs = vf->num_queue_pairs;
	vfres->max_vectors = pf->hw.func_caps.num_msix_vectors_vf;
	vfres->rss_key_size = I40E_HKEY_ARRAY_SIZE;
	vfres->rss_lut_size = I40E_VF_HLUT_ARRAY_SIZE;
	vfres->max_mtu = i40e_vc_get_max_frame_size(vf);

	if (vf->lan_vsi_idx) {
		vfres->vsi_res[0].vsi_id = vf->lan_vsi_id;
		vfres->vsi_res[0].vsi_type = VIRTCHNL_VSI_SRIOV;
		vfres->vsi_res[0].num_queue_pairs = vsi->alloc_queue_pairs;
		 
		vfres->vsi_res[0].qset_handle
					  = le16_to_cpu(vsi->info.qs_handle[0]);
		if (!(vf->driver_caps & VIRTCHNL_VF_OFFLOAD_USO) && !vf->pf_set_mac) {
			i40e_del_mac_filter(vsi, vf->default_lan_addr.addr);
			eth_zero_addr(vf->default_lan_addr.addr);
		}
		ether_addr_copy(vfres->vsi_res[0].default_mac_addr,
				vf->default_lan_addr.addr);
	}
	set_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states);

err:
	 
	ret = i40e_vc_send_msg_to_vf(vf, VIRTCHNL_OP_GET_VF_RESOURCES,
				     aq_ret, (u8 *)vfres, len);

	kfree(vfres);
	return ret;
}

 
static int i40e_vc_config_promiscuous_mode_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_promisc_info *info =
	    (struct virtchnl_promisc_info *)msg;
	struct i40e_pf *pf = vf->pf;
	bool allmulti = false;
	bool alluni = false;
	int aq_ret = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err_out;
	}
	if (!test_bit(I40E_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps)) {
		dev_err(&pf->pdev->dev,
			"Unprivileged VF %d is attempting to configure promiscuous mode\n",
			vf->vf_id);

		 
		aq_ret = 0;
		goto err_out;
	}

	if (info->flags > I40E_MAX_VF_PROMISC_FLAGS) {
		aq_ret = -EINVAL;
		goto err_out;
	}

	if (!i40e_vc_isvalid_vsi_id(vf, info->vsi_id)) {
		aq_ret = -EINVAL;
		goto err_out;
	}

	 
	if (info->flags & FLAG_VF_MULTICAST_PROMISC)
		allmulti = true;

	if (info->flags & FLAG_VF_UNICAST_PROMISC)
		alluni = true;
	aq_ret = i40e_config_vf_promiscuous_mode(vf, info->vsi_id, allmulti,
						 alluni);
	if (aq_ret)
		goto err_out;

	if (allmulti) {
		if (!test_and_set_bit(I40E_VF_STATE_MC_PROMISC,
				      &vf->vf_states))
			dev_info(&pf->pdev->dev,
				 "VF %d successfully set multicast promiscuous mode\n",
				 vf->vf_id);
	} else if (test_and_clear_bit(I40E_VF_STATE_MC_PROMISC,
				      &vf->vf_states))
		dev_info(&pf->pdev->dev,
			 "VF %d successfully unset multicast promiscuous mode\n",
			 vf->vf_id);

	if (alluni) {
		if (!test_and_set_bit(I40E_VF_STATE_UC_PROMISC,
				      &vf->vf_states))
			dev_info(&pf->pdev->dev,
				 "VF %d successfully set unicast promiscuous mode\n",
				 vf->vf_id);
	} else if (test_and_clear_bit(I40E_VF_STATE_UC_PROMISC,
				      &vf->vf_states))
		dev_info(&pf->pdev->dev,
			 "VF %d successfully unset unicast promiscuous mode\n",
			 vf->vf_id);

err_out:
	 
	return i40e_vc_send_resp_to_vf(vf,
				       VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE,
				       aq_ret);
}

 
static int i40e_vc_config_queues_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_vsi_queue_config_info *qci =
	    (struct virtchnl_vsi_queue_config_info *)msg;
	struct virtchnl_queue_pair_info *qpi;
	u16 vsi_id, vsi_queue_id = 0;
	struct i40e_pf *pf = vf->pf;
	int i, j = 0, idx = 0;
	struct i40e_vsi *vsi;
	u16 num_qps_all = 0;
	int aq_ret = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (!i40e_vc_isvalid_vsi_id(vf, qci->vsi_id)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (qci->num_queue_pairs > I40E_MAX_VF_QUEUES) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (vf->adq_enabled) {
		for (i = 0; i < vf->num_tc; i++)
			num_qps_all += vf->ch[i].num_qps;
		if (num_qps_all != qci->num_queue_pairs) {
			aq_ret = -EINVAL;
			goto error_param;
		}
	}

	vsi_id = qci->vsi_id;

	for (i = 0; i < qci->num_queue_pairs; i++) {
		qpi = &qci->qpair[i];

		if (!vf->adq_enabled) {
			if (!i40e_vc_isvalid_queue_id(vf, vsi_id,
						      qpi->txq.queue_id)) {
				aq_ret = -EINVAL;
				goto error_param;
			}

			vsi_queue_id = qpi->txq.queue_id;

			if (qpi->txq.vsi_id != qci->vsi_id ||
			    qpi->rxq.vsi_id != qci->vsi_id ||
			    qpi->rxq.queue_id != vsi_queue_id) {
				aq_ret = -EINVAL;
				goto error_param;
			}
		}

		if (vf->adq_enabled) {
			if (idx >= ARRAY_SIZE(vf->ch)) {
				aq_ret = -ENODEV;
				goto error_param;
			}
			vsi_id = vf->ch[idx].vsi_id;
		}

		if (i40e_config_vsi_rx_queue(vf, vsi_id, vsi_queue_id,
					     &qpi->rxq) ||
		    i40e_config_vsi_tx_queue(vf, vsi_id, vsi_queue_id,
					     &qpi->txq)) {
			aq_ret = -EINVAL;
			goto error_param;
		}

		 
		if (vf->adq_enabled) {
			if (idx >= ARRAY_SIZE(vf->ch)) {
				aq_ret = -ENODEV;
				goto error_param;
			}
			if (j == (vf->ch[idx].num_qps - 1)) {
				idx++;
				j = 0;  
				vsi_queue_id = 0;
			} else {
				j++;
				vsi_queue_id++;
			}
		}
	}
	 
	if (!vf->adq_enabled) {
		pf->vsi[vf->lan_vsi_idx]->num_queue_pairs =
			qci->num_queue_pairs;
	} else {
		for (i = 0; i < vf->num_tc; i++) {
			vsi = pf->vsi[vf->ch[i].vsi_idx];
			vsi->num_queue_pairs = vf->ch[i].num_qps;

			if (i40e_update_adq_vsi_queues(vsi, i)) {
				aq_ret = -EIO;
				goto error_param;
			}
		}
	}

error_param:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_CONFIG_VSI_QUEUES,
				       aq_ret);
}

 
static int i40e_validate_queue_map(struct i40e_vf *vf, u16 vsi_id,
				   unsigned long queuemap)
{
	u16 vsi_queue_id, queue_id;

	for_each_set_bit(vsi_queue_id, &queuemap, I40E_MAX_VSI_QP) {
		if (vf->adq_enabled) {
			vsi_id = vf->ch[vsi_queue_id / I40E_MAX_VF_VSI].vsi_id;
			queue_id = (vsi_queue_id % I40E_DEFAULT_QUEUES_PER_VF);
		} else {
			queue_id = vsi_queue_id;
		}

		if (!i40e_vc_isvalid_queue_id(vf, vsi_id, queue_id))
			return -EINVAL;
	}

	return 0;
}

 
static int i40e_vc_config_irq_map_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_irq_map_info *irqmap_info =
	    (struct virtchnl_irq_map_info *)msg;
	struct virtchnl_vector_map *map;
	int aq_ret = 0;
	u16 vsi_id;
	int i;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (irqmap_info->num_vectors >
	    vf->pf->hw.func_caps.num_msix_vectors_vf) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	for (i = 0; i < irqmap_info->num_vectors; i++) {
		map = &irqmap_info->vecmap[i];
		 
		if (!i40e_vc_isvalid_vector_id(vf, map->vector_id) ||
		    !i40e_vc_isvalid_vsi_id(vf, map->vsi_id)) {
			aq_ret = -EINVAL;
			goto error_param;
		}
		vsi_id = map->vsi_id;

		if (i40e_validate_queue_map(vf, vsi_id, map->rxq_map)) {
			aq_ret = -EINVAL;
			goto error_param;
		}

		if (i40e_validate_queue_map(vf, vsi_id, map->txq_map)) {
			aq_ret = -EINVAL;
			goto error_param;
		}

		i40e_config_irq_link_list(vf, vsi_id, map);
	}
error_param:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_CONFIG_IRQ_MAP,
				       aq_ret);
}

 
static int i40e_ctrl_vf_tx_rings(struct i40e_vsi *vsi, unsigned long q_map,
				 bool enable)
{
	struct i40e_pf *pf = vsi->back;
	int ret = 0;
	u16 q_id;

	for_each_set_bit(q_id, &q_map, I40E_MAX_VF_QUEUES) {
		ret = i40e_control_wait_tx_q(vsi->seid, pf,
					     vsi->base_queue + q_id,
					     false  , enable);
		if (ret)
			break;
	}
	return ret;
}

 
static int i40e_ctrl_vf_rx_rings(struct i40e_vsi *vsi, unsigned long q_map,
				 bool enable)
{
	struct i40e_pf *pf = vsi->back;
	int ret = 0;
	u16 q_id;

	for_each_set_bit(q_id, &q_map, I40E_MAX_VF_QUEUES) {
		ret = i40e_control_wait_rx_q(pf, vsi->base_queue + q_id,
					     enable);
		if (ret)
			break;
	}
	return ret;
}

 
static bool i40e_vc_validate_vqs_bitmaps(struct virtchnl_queue_select *vqs)
{
	if ((!vqs->rx_queues && !vqs->tx_queues) ||
	    vqs->rx_queues >= BIT(I40E_MAX_VF_QUEUES) ||
	    vqs->tx_queues >= BIT(I40E_MAX_VF_QUEUES))
		return false;

	return true;
}

 
static int i40e_vc_enable_queues_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_queue_select *vqs =
	    (struct virtchnl_queue_select *)msg;
	struct i40e_pf *pf = vf->pf;
	int aq_ret = 0;
	int i;

	if (!test_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (!i40e_vc_isvalid_vsi_id(vf, vqs->vsi_id)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (!i40e_vc_validate_vqs_bitmaps(vqs)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	 
	if (i40e_ctrl_vf_rx_rings(pf->vsi[vf->lan_vsi_idx], vqs->rx_queues,
				  true)) {
		aq_ret = -EIO;
		goto error_param;
	}
	if (i40e_ctrl_vf_tx_rings(pf->vsi[vf->lan_vsi_idx], vqs->tx_queues,
				  true)) {
		aq_ret = -EIO;
		goto error_param;
	}

	 
	if (vf->adq_enabled) {
		 
		for (i = 1; i < vf->num_tc; i++) {
			if (i40e_vsi_start_rings(pf->vsi[vf->ch[i].vsi_idx]))
				aq_ret = -EIO;
		}
	}

error_param:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_ENABLE_QUEUES,
				       aq_ret);
}

 
static int i40e_vc_disable_queues_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_queue_select *vqs =
	    (struct virtchnl_queue_select *)msg;
	struct i40e_pf *pf = vf->pf;
	int aq_ret = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (!i40e_vc_isvalid_vsi_id(vf, vqs->vsi_id)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (!i40e_vc_validate_vqs_bitmaps(vqs)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	 
	if (i40e_ctrl_vf_tx_rings(pf->vsi[vf->lan_vsi_idx], vqs->tx_queues,
				  false)) {
		aq_ret = -EIO;
		goto error_param;
	}
	if (i40e_ctrl_vf_rx_rings(pf->vsi[vf->lan_vsi_idx], vqs->rx_queues,
				  false)) {
		aq_ret = -EIO;
		goto error_param;
	}
error_param:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_DISABLE_QUEUES,
				       aq_ret);
}

 
static int i40e_check_enough_queue(struct i40e_vf *vf, u16 needed)
{
	unsigned int  i, cur_queues, more, pool_size;
	struct i40e_lump_tracking *pile;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi;

	vsi = pf->vsi[vf->lan_vsi_idx];
	cur_queues = vsi->alloc_queue_pairs;

	 
	if (cur_queues >= needed)
		return vsi->base_queue;

	pile = pf->qp_pile;
	if (cur_queues > 0) {
		 
		more = needed - cur_queues;
		for (i = vsi->base_queue + cur_queues;
			i < pile->num_entries; i++) {
			if (pile->list[i] & I40E_PILE_VALID_BIT)
				break;

			if (more-- == 1)
				 
				return vsi->base_queue;
		}
	}

	pool_size = 0;
	for (i = 0; i < pile->num_entries; i++) {
		if (pile->list[i] & I40E_PILE_VALID_BIT) {
			pool_size = 0;
			continue;
		}
		if (needed <= ++pool_size)
			 
			return i;
	}

	return -ENOMEM;
}

 
static int i40e_vc_request_queues_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_vf_res_request *vfres =
		(struct virtchnl_vf_res_request *)msg;
	u16 req_pairs = vfres->num_queue_pairs;
	u8 cur_pairs = vf->num_queue_pairs;
	struct i40e_pf *pf = vf->pf;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE))
		return -EINVAL;

	if (req_pairs > I40E_MAX_VF_QUEUES) {
		dev_err(&pf->pdev->dev,
			"VF %d tried to request more than %d queues.\n",
			vf->vf_id,
			I40E_MAX_VF_QUEUES);
		vfres->num_queue_pairs = I40E_MAX_VF_QUEUES;
	} else if (req_pairs - cur_pairs > pf->queues_left) {
		dev_warn(&pf->pdev->dev,
			 "VF %d requested %d more queues, but only %d left.\n",
			 vf->vf_id,
			 req_pairs - cur_pairs,
			 pf->queues_left);
		vfres->num_queue_pairs = pf->queues_left + cur_pairs;
	} else if (i40e_check_enough_queue(vf, req_pairs) < 0) {
		dev_warn(&pf->pdev->dev,
			 "VF %d requested %d more queues, but there is not enough for it.\n",
			 vf->vf_id,
			 req_pairs - cur_pairs);
		vfres->num_queue_pairs = cur_pairs;
	} else {
		 
		vf->num_req_queues = req_pairs;
		i40e_vc_reset_vf(vf, true);
		return 0;
	}

	return i40e_vc_send_msg_to_vf(vf, VIRTCHNL_OP_REQUEST_QUEUES, 0,
				      (u8 *)vfres, sizeof(*vfres));
}

 
static int i40e_vc_get_stats_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_queue_select *vqs =
	    (struct virtchnl_queue_select *)msg;
	struct i40e_pf *pf = vf->pf;
	struct i40e_eth_stats stats;
	int aq_ret = 0;
	struct i40e_vsi *vsi;

	memset(&stats, 0, sizeof(struct i40e_eth_stats));

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (!i40e_vc_isvalid_vsi_id(vf, vqs->vsi_id)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	vsi = pf->vsi[vf->lan_vsi_idx];
	if (!vsi) {
		aq_ret = -EINVAL;
		goto error_param;
	}
	i40e_update_eth_stats(vsi);
	stats = vsi->eth_stats;

error_param:
	 
	return i40e_vc_send_msg_to_vf(vf, VIRTCHNL_OP_GET_STATS, aq_ret,
				      (u8 *)&stats, sizeof(stats));
}

#define I40E_MAX_MACVLAN_PER_HW 3072
#define I40E_MAX_MACVLAN_PER_PF(num_ports) (I40E_MAX_MACVLAN_PER_HW /	\
	(num_ports))
 
#define I40E_VC_MAX_MAC_ADDR_PER_VF (16 + 1 + 1)
#define I40E_VC_MAX_VLAN_PER_VF 16

#define I40E_VC_MAX_MACVLAN_PER_TRUSTED_VF(vf_num, num_ports)		\
({	typeof(vf_num) vf_num_ = (vf_num);				\
	typeof(num_ports) num_ports_ = (num_ports);			\
	((I40E_MAX_MACVLAN_PER_PF(num_ports_) - vf_num_ *		\
	I40E_VC_MAX_MAC_ADDR_PER_VF) / vf_num_) +			\
	I40E_VC_MAX_MAC_ADDR_PER_VF; })
 
static inline int i40e_check_vf_permission(struct i40e_vf *vf,
					   struct virtchnl_ether_addr_list *al)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = pf->vsi[vf->lan_vsi_idx];
	struct i40e_hw *hw = &pf->hw;
	int mac2add_cnt = 0;
	int i;

	for (i = 0; i < al->num_elements; i++) {
		struct i40e_mac_filter *f;
		u8 *addr = al->list[i].addr;

		if (is_broadcast_ether_addr(addr) ||
		    is_zero_ether_addr(addr)) {
			dev_err(&pf->pdev->dev, "invalid VF MAC addr %pM\n",
				addr);
			return -EINVAL;
		}

		 
		if (!test_bit(I40E_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps) &&
		    !is_multicast_ether_addr(addr) && vf->pf_set_mac &&
		    !ether_addr_equal(addr, vf->default_lan_addr.addr)) {
			dev_err(&pf->pdev->dev,
				"VF attempting to override administratively set MAC address, bring down and up the VF interface to resume normal operation\n");
			return -EPERM;
		}

		 
		f = i40e_find_mac(vsi, addr);
		if (!f)
			++mac2add_cnt;
	}

	 
	if (!test_bit(I40E_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps)) {
		if ((i40e_count_filters(vsi) + mac2add_cnt) >
		    I40E_VC_MAX_MAC_ADDR_PER_VF) {
			dev_err(&pf->pdev->dev,
				"Cannot add more MAC addresses, VF is not trusted, switch the VF to trusted to add more functionality\n");
			return -EPERM;
		}
	 
	} else {
		if ((i40e_count_filters(vsi) + mac2add_cnt) >
		    I40E_VC_MAX_MACVLAN_PER_TRUSTED_VF(pf->num_alloc_vfs,
						       hw->num_ports)) {
			dev_err(&pf->pdev->dev,
				"Cannot add more MAC addresses, trusted VF exhausted it's resources\n");
			return -EPERM;
		}
	}
	return 0;
}

 
static u8
i40e_vc_ether_addr_type(struct virtchnl_ether_addr *vc_ether_addr)
{
	return vc_ether_addr->type & VIRTCHNL_ETHER_ADDR_TYPE_MASK;
}

 
static bool
i40e_is_vc_addr_legacy(struct virtchnl_ether_addr *vc_ether_addr)
{
	return i40e_vc_ether_addr_type(vc_ether_addr) ==
		VIRTCHNL_ETHER_ADDR_LEGACY;
}

 
static bool
i40e_is_vc_addr_primary(struct virtchnl_ether_addr *vc_ether_addr)
{
	return i40e_vc_ether_addr_type(vc_ether_addr) ==
		VIRTCHNL_ETHER_ADDR_PRIMARY;
}

 
static void
i40e_update_vf_mac_addr(struct i40e_vf *vf,
			struct virtchnl_ether_addr *vc_ether_addr)
{
	u8 *mac_addr = vc_ether_addr->addr;

	if (!is_valid_ether_addr(mac_addr))
		return;

	 
	if (i40e_is_vc_addr_primary(vc_ether_addr)) {
		ether_addr_copy(vf->default_lan_addr.addr, mac_addr);
	} else if (i40e_is_vc_addr_legacy(vc_ether_addr)) {
		if (is_zero_ether_addr(vf->default_lan_addr.addr))
			ether_addr_copy(vf->default_lan_addr.addr, mac_addr);
	}
}

 
static int i40e_vc_add_mac_addr_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_ether_addr_list *al =
	    (struct virtchnl_ether_addr_list *)msg;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	int ret = 0;
	int i;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE) ||
	    !i40e_vc_isvalid_vsi_id(vf, al->vsi_id)) {
		ret = -EINVAL;
		goto error_param;
	}

	vsi = pf->vsi[vf->lan_vsi_idx];

	 
	spin_lock_bh(&vsi->mac_filter_hash_lock);

	ret = i40e_check_vf_permission(vf, al);
	if (ret) {
		spin_unlock_bh(&vsi->mac_filter_hash_lock);
		goto error_param;
	}

	 
	for (i = 0; i < al->num_elements; i++) {
		struct i40e_mac_filter *f;

		f = i40e_find_mac(vsi, al->list[i].addr);
		if (!f) {
			f = i40e_add_mac_filter(vsi, al->list[i].addr);

			if (!f) {
				dev_err(&pf->pdev->dev,
					"Unable to add MAC filter %pM for VF %d\n",
					al->list[i].addr, vf->vf_id);
				ret = -EINVAL;
				spin_unlock_bh(&vsi->mac_filter_hash_lock);
				goto error_param;
			}
		}
		i40e_update_vf_mac_addr(vf, &al->list[i]);
	}
	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	 
	ret = i40e_sync_vsi_filters(vsi);
	if (ret)
		dev_err(&pf->pdev->dev, "Unable to program VF %d MAC filters, error %d\n",
			vf->vf_id, ret);

error_param:
	 
	return i40e_vc_send_msg_to_vf(vf, VIRTCHNL_OP_ADD_ETH_ADDR,
				      ret, NULL, 0);
}

 
static int i40e_vc_del_mac_addr_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_ether_addr_list *al =
	    (struct virtchnl_ether_addr_list *)msg;
	bool was_unimac_deleted = false;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	int ret = 0;
	int i;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE) ||
	    !i40e_vc_isvalid_vsi_id(vf, al->vsi_id)) {
		ret = -EINVAL;
		goto error_param;
	}

	for (i = 0; i < al->num_elements; i++) {
		if (is_broadcast_ether_addr(al->list[i].addr) ||
		    is_zero_ether_addr(al->list[i].addr)) {
			dev_err(&pf->pdev->dev, "Invalid MAC addr %pM for VF %d\n",
				al->list[i].addr, vf->vf_id);
			ret = -EINVAL;
			goto error_param;
		}
		if (ether_addr_equal(al->list[i].addr, vf->default_lan_addr.addr))
			was_unimac_deleted = true;
	}
	vsi = pf->vsi[vf->lan_vsi_idx];

	spin_lock_bh(&vsi->mac_filter_hash_lock);
	 
	for (i = 0; i < al->num_elements; i++)
		if (i40e_del_mac_filter(vsi, al->list[i].addr)) {
			ret = -EINVAL;
			spin_unlock_bh(&vsi->mac_filter_hash_lock);
			goto error_param;
		}

	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	if (was_unimac_deleted)
		eth_zero_addr(vf->default_lan_addr.addr);

	 
	ret = i40e_sync_vsi_filters(vsi);
	if (ret)
		dev_err(&pf->pdev->dev, "Unable to program VF %d MAC filters, error %d\n",
			vf->vf_id, ret);

	if (vf->trusted && was_unimac_deleted) {
		struct i40e_mac_filter *f;
		struct hlist_node *h;
		u8 *macaddr = NULL;
		int bkt;

		 
		spin_lock_bh(&vsi->mac_filter_hash_lock);
		hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist) {
			if (is_valid_ether_addr(f->macaddr))
				macaddr = f->macaddr;
		}
		if (macaddr)
			ether_addr_copy(vf->default_lan_addr.addr, macaddr);
		spin_unlock_bh(&vsi->mac_filter_hash_lock);
	}
error_param:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_DEL_ETH_ADDR, ret);
}

 
static int i40e_vc_add_vlan_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_vlan_filter_list *vfl =
	    (struct virtchnl_vlan_filter_list *)msg;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	int aq_ret = 0;
	int i;

	if ((vf->num_vlan >= I40E_VC_MAX_VLAN_PER_VF) &&
	    !test_bit(I40E_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps)) {
		dev_err(&pf->pdev->dev,
			"VF is not trusted, switch the VF to trusted to add more VLAN addresses\n");
		goto error_param;
	}
	if (!test_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states) ||
	    !i40e_vc_isvalid_vsi_id(vf, vfl->vsi_id)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	for (i = 0; i < vfl->num_elements; i++) {
		if (vfl->vlan_id[i] > I40E_MAX_VLANID) {
			aq_ret = -EINVAL;
			dev_err(&pf->pdev->dev,
				"invalid VF VLAN id %d\n", vfl->vlan_id[i]);
			goto error_param;
		}
	}
	vsi = pf->vsi[vf->lan_vsi_idx];
	if (vsi->info.pvid) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	i40e_vlan_stripping_enable(vsi);
	for (i = 0; i < vfl->num_elements; i++) {
		 
		int ret = i40e_vsi_add_vlan(vsi, vfl->vlan_id[i]);
		if (!ret)
			vf->num_vlan++;

		if (test_bit(I40E_VF_STATE_UC_PROMISC, &vf->vf_states))
			i40e_aq_set_vsi_uc_promisc_on_vlan(&pf->hw, vsi->seid,
							   true,
							   vfl->vlan_id[i],
							   NULL);
		if (test_bit(I40E_VF_STATE_MC_PROMISC, &vf->vf_states))
			i40e_aq_set_vsi_mc_promisc_on_vlan(&pf->hw, vsi->seid,
							   true,
							   vfl->vlan_id[i],
							   NULL);

		if (ret)
			dev_err(&pf->pdev->dev,
				"Unable to add VLAN filter %d for VF %d, error %d\n",
				vfl->vlan_id[i], vf->vf_id, ret);
	}

error_param:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_ADD_VLAN, aq_ret);
}

 
static int i40e_vc_remove_vlan_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_vlan_filter_list *vfl =
	    (struct virtchnl_vlan_filter_list *)msg;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	int aq_ret = 0;
	int i;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE) ||
	    !i40e_vc_isvalid_vsi_id(vf, vfl->vsi_id)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	for (i = 0; i < vfl->num_elements; i++) {
		if (vfl->vlan_id[i] > I40E_MAX_VLANID) {
			aq_ret = -EINVAL;
			goto error_param;
		}
	}

	vsi = pf->vsi[vf->lan_vsi_idx];
	if (vsi->info.pvid) {
		if (vfl->num_elements > 1 || vfl->vlan_id[0])
			aq_ret = -EINVAL;
		goto error_param;
	}

	for (i = 0; i < vfl->num_elements; i++) {
		i40e_vsi_kill_vlan(vsi, vfl->vlan_id[i]);
		vf->num_vlan--;

		if (test_bit(I40E_VF_STATE_UC_PROMISC, &vf->vf_states))
			i40e_aq_set_vsi_uc_promisc_on_vlan(&pf->hw, vsi->seid,
							   false,
							   vfl->vlan_id[i],
							   NULL);
		if (test_bit(I40E_VF_STATE_MC_PROMISC, &vf->vf_states))
			i40e_aq_set_vsi_mc_promisc_on_vlan(&pf->hw, vsi->seid,
							   false,
							   vfl->vlan_id[i],
							   NULL);
	}

error_param:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_DEL_VLAN, aq_ret);
}

 
static int i40e_vc_rdma_msg(struct i40e_vf *vf, u8 *msg, u16 msglen)
{
	struct i40e_pf *pf = vf->pf;
	int abs_vf_id = vf->vf_id + pf->hw.func_caps.vf_base_id;
	int aq_ret = 0;

	if (!test_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states) ||
	    !test_bit(I40E_VF_STATE_RDMAENA, &vf->vf_states)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	i40e_notify_client_of_vf_msg(pf->vsi[pf->lan_vsi], abs_vf_id,
				     msg, msglen);

error_param:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_RDMA,
				       aq_ret);
}

 
static int i40e_vc_rdma_qvmap_msg(struct i40e_vf *vf, u8 *msg, bool config)
{
	struct virtchnl_rdma_qvlist_info *qvlist_info =
				(struct virtchnl_rdma_qvlist_info *)msg;
	int aq_ret = 0;

	if (!test_bit(I40E_VF_STATE_ACTIVE, &vf->vf_states) ||
	    !test_bit(I40E_VF_STATE_RDMAENA, &vf->vf_states)) {
		aq_ret = -EINVAL;
		goto error_param;
	}

	if (config) {
		if (i40e_config_rdma_qvlist(vf, qvlist_info))
			aq_ret = -EINVAL;
	} else {
		i40e_release_rdma_qvlist(vf);
	}

error_param:
	 
	return i40e_vc_send_resp_to_vf(vf,
			       config ? VIRTCHNL_OP_CONFIG_RDMA_IRQ_MAP :
			       VIRTCHNL_OP_RELEASE_RDMA_IRQ_MAP,
			       aq_ret);
}

 
static int i40e_vc_config_rss_key(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_rss_key *vrk =
		(struct virtchnl_rss_key *)msg;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	int aq_ret = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE) ||
	    !i40e_vc_isvalid_vsi_id(vf, vrk->vsi_id) ||
	    vrk->key_len != I40E_HKEY_ARRAY_SIZE) {
		aq_ret = -EINVAL;
		goto err;
	}

	vsi = pf->vsi[vf->lan_vsi_idx];
	aq_ret = i40e_config_rss(vsi, vrk->key, NULL, 0);
err:
	 
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_CONFIG_RSS_KEY,
				       aq_ret);
}

 
static int i40e_vc_config_rss_lut(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_rss_lut *vrl =
		(struct virtchnl_rss_lut *)msg;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	int aq_ret = 0;
	u16 i;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE) ||
	    !i40e_vc_isvalid_vsi_id(vf, vrl->vsi_id) ||
	    vrl->lut_entries != I40E_VF_HLUT_ARRAY_SIZE) {
		aq_ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < vrl->lut_entries; i++)
		if (vrl->lut[i] >= vf->num_queue_pairs) {
			aq_ret = -EINVAL;
			goto err;
		}

	vsi = pf->vsi[vf->lan_vsi_idx];
	aq_ret = i40e_config_rss(vsi, NULL, vrl->lut, I40E_VF_HLUT_ARRAY_SIZE);
	 
err:
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_CONFIG_RSS_LUT,
				       aq_ret);
}

 
static int i40e_vc_get_rss_hena(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_rss_hena *vrh = NULL;
	struct i40e_pf *pf = vf->pf;
	int aq_ret = 0;
	int len = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err;
	}
	len = sizeof(struct virtchnl_rss_hena);

	vrh = kzalloc(len, GFP_KERNEL);
	if (!vrh) {
		aq_ret = -ENOMEM;
		len = 0;
		goto err;
	}
	vrh->hena = i40e_pf_get_default_rss_hena(pf);
err:
	 
	aq_ret = i40e_vc_send_msg_to_vf(vf, VIRTCHNL_OP_GET_RSS_HENA_CAPS,
					aq_ret, (u8 *)vrh, len);
	kfree(vrh);
	return aq_ret;
}

 
static int i40e_vc_set_rss_hena(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_rss_hena *vrh =
		(struct virtchnl_rss_hena *)msg;
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = &pf->hw;
	int aq_ret = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err;
	}
	i40e_write_rx_ctl(hw, I40E_VFQF_HENA1(0, vf->vf_id), (u32)vrh->hena);
	i40e_write_rx_ctl(hw, I40E_VFQF_HENA1(1, vf->vf_id),
			  (u32)(vrh->hena >> 32));

	 
err:
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_SET_RSS_HENA, aq_ret);
}

 
static int i40e_vc_enable_vlan_stripping(struct i40e_vf *vf, u8 *msg)
{
	struct i40e_vsi *vsi;
	int aq_ret = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err;
	}

	vsi = vf->pf->vsi[vf->lan_vsi_idx];
	i40e_vlan_stripping_enable(vsi);

	 
err:
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_ENABLE_VLAN_STRIPPING,
				       aq_ret);
}

 
static int i40e_vc_disable_vlan_stripping(struct i40e_vf *vf, u8 *msg)
{
	struct i40e_vsi *vsi;
	int aq_ret = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err;
	}

	vsi = vf->pf->vsi[vf->lan_vsi_idx];
	i40e_vlan_stripping_disable(vsi);

	 
err:
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_DISABLE_VLAN_STRIPPING,
				       aq_ret);
}

 
static int i40e_validate_cloud_filter(struct i40e_vf *vf,
				      struct virtchnl_filter *tc_filter)
{
	struct virtchnl_l4_spec mask = tc_filter->mask.tcp_spec;
	struct virtchnl_l4_spec data = tc_filter->data.tcp_spec;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	struct i40e_mac_filter *f;
	struct hlist_node *h;
	bool found = false;
	int bkt;

	if (tc_filter->action != VIRTCHNL_ACTION_TC_REDIRECT) {
		dev_info(&pf->pdev->dev,
			 "VF %d: ADQ doesn't support this action (%d)\n",
			 vf->vf_id, tc_filter->action);
		goto err;
	}

	 
	if (!tc_filter->action_meta ||
	    tc_filter->action_meta > vf->num_tc) {
		dev_info(&pf->pdev->dev, "VF %d: Invalid TC number %u\n",
			 vf->vf_id, tc_filter->action_meta);
		goto err;
	}

	 
	if (mask.dst_mac[0] && !mask.dst_ip[0]) {
		vsi = pf->vsi[vf->lan_vsi_idx];
		f = i40e_find_mac(vsi, data.dst_mac);

		if (!f) {
			dev_info(&pf->pdev->dev,
				 "Destination MAC %pM doesn't belong to VF %d\n",
				 data.dst_mac, vf->vf_id);
			goto err;
		}

		if (mask.vlan_id) {
			hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f,
					   hlist) {
				if (f->vlan == ntohs(data.vlan_id)) {
					found = true;
					break;
				}
			}
			if (!found) {
				dev_info(&pf->pdev->dev,
					 "VF %d doesn't have any VLAN id %u\n",
					 vf->vf_id, ntohs(data.vlan_id));
				goto err;
			}
		}
	} else {
		 
		if (!test_bit(I40E_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps)) {
			dev_err(&pf->pdev->dev,
				"VF %d not trusted, make VF trusted to add advanced mode ADq cloud filters\n",
				vf->vf_id);
			return -EIO;
		}
	}

	if (mask.dst_mac[0] & data.dst_mac[0]) {
		if (is_broadcast_ether_addr(data.dst_mac) ||
		    is_zero_ether_addr(data.dst_mac)) {
			dev_info(&pf->pdev->dev, "VF %d: Invalid Dest MAC addr %pM\n",
				 vf->vf_id, data.dst_mac);
			goto err;
		}
	}

	if (mask.src_mac[0] & data.src_mac[0]) {
		if (is_broadcast_ether_addr(data.src_mac) ||
		    is_zero_ether_addr(data.src_mac)) {
			dev_info(&pf->pdev->dev, "VF %d: Invalid Source MAC addr %pM\n",
				 vf->vf_id, data.src_mac);
			goto err;
		}
	}

	if (mask.dst_port & data.dst_port) {
		if (!data.dst_port) {
			dev_info(&pf->pdev->dev, "VF %d: Invalid Dest port\n",
				 vf->vf_id);
			goto err;
		}
	}

	if (mask.src_port & data.src_port) {
		if (!data.src_port) {
			dev_info(&pf->pdev->dev, "VF %d: Invalid Source port\n",
				 vf->vf_id);
			goto err;
		}
	}

	if (tc_filter->flow_type != VIRTCHNL_TCP_V6_FLOW &&
	    tc_filter->flow_type != VIRTCHNL_TCP_V4_FLOW) {
		dev_info(&pf->pdev->dev, "VF %d: Invalid Flow type\n",
			 vf->vf_id);
		goto err;
	}

	if (mask.vlan_id & data.vlan_id) {
		if (ntohs(data.vlan_id) > I40E_MAX_VLANID) {
			dev_info(&pf->pdev->dev, "VF %d: invalid VLAN ID\n",
				 vf->vf_id);
			goto err;
		}
	}

	return 0;
err:
	return -EIO;
}

 
static struct i40e_vsi *i40e_find_vsi_from_seid(struct i40e_vf *vf, u16 seid)
{
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	int i;

	for (i = 0; i < vf->num_tc ; i++) {
		vsi = i40e_find_vsi_from_id(pf, vf->ch[i].vsi_id);
		if (vsi && vsi->seid == seid)
			return vsi;
	}
	return NULL;
}

 
static void i40e_del_all_cloud_filters(struct i40e_vf *vf)
{
	struct i40e_cloud_filter *cfilter = NULL;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	struct hlist_node *node;
	int ret;

	hlist_for_each_entry_safe(cfilter, node,
				  &vf->cloud_filter_list, cloud_node) {
		vsi = i40e_find_vsi_from_seid(vf, cfilter->seid);

		if (!vsi) {
			dev_err(&pf->pdev->dev, "VF %d: no VSI found for matching %u seid, can't delete cloud filter\n",
				vf->vf_id, cfilter->seid);
			continue;
		}

		if (cfilter->dst_port)
			ret = i40e_add_del_cloud_filter_big_buf(vsi, cfilter,
								false);
		else
			ret = i40e_add_del_cloud_filter(vsi, cfilter, false);
		if (ret)
			dev_err(&pf->pdev->dev,
				"VF %d: Failed to delete cloud filter, err %pe aq_err %s\n",
				vf->vf_id, ERR_PTR(ret),
				i40e_aq_str(&pf->hw,
					    pf->hw.aq.asq_last_status));

		hlist_del(&cfilter->cloud_node);
		kfree(cfilter);
		vf->num_cloud_filters--;
	}
}

 
static int i40e_vc_del_cloud_filter(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_filter *vcf = (struct virtchnl_filter *)msg;
	struct virtchnl_l4_spec mask = vcf->mask.tcp_spec;
	struct virtchnl_l4_spec tcf = vcf->data.tcp_spec;
	struct i40e_cloud_filter cfilter, *cf = NULL;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	struct hlist_node *node;
	int aq_ret = 0;
	int i, ret;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err;
	}

	if (!vf->adq_enabled) {
		dev_info(&pf->pdev->dev,
			 "VF %d: ADq not enabled, can't apply cloud filter\n",
			 vf->vf_id);
		aq_ret = -EINVAL;
		goto err;
	}

	if (i40e_validate_cloud_filter(vf, vcf)) {
		dev_info(&pf->pdev->dev,
			 "VF %d: Invalid input, can't apply cloud filter\n",
			 vf->vf_id);
		aq_ret = -EINVAL;
		goto err;
	}

	memset(&cfilter, 0, sizeof(cfilter));
	 
	for (i = 0; i < ETH_ALEN; i++)
		cfilter.dst_mac[i] = mask.dst_mac[i] & tcf.dst_mac[i];

	 
	for (i = 0; i < ETH_ALEN; i++)
		cfilter.src_mac[i] = mask.src_mac[i] & tcf.src_mac[i];

	cfilter.vlan_id = mask.vlan_id & tcf.vlan_id;
	cfilter.dst_port = mask.dst_port & tcf.dst_port;
	cfilter.src_port = mask.src_port & tcf.src_port;

	switch (vcf->flow_type) {
	case VIRTCHNL_TCP_V4_FLOW:
		cfilter.n_proto = ETH_P_IP;
		if (mask.dst_ip[0] & tcf.dst_ip[0])
			memcpy(&cfilter.ip.v4.dst_ip, tcf.dst_ip,
			       ARRAY_SIZE(tcf.dst_ip));
		else if (mask.src_ip[0] & tcf.dst_ip[0])
			memcpy(&cfilter.ip.v4.src_ip, tcf.src_ip,
			       ARRAY_SIZE(tcf.dst_ip));
		break;
	case VIRTCHNL_TCP_V6_FLOW:
		cfilter.n_proto = ETH_P_IPV6;
		if (mask.dst_ip[3] & tcf.dst_ip[3])
			memcpy(&cfilter.ip.v6.dst_ip6, tcf.dst_ip,
			       sizeof(cfilter.ip.v6.dst_ip6));
		if (mask.src_ip[3] & tcf.src_ip[3])
			memcpy(&cfilter.ip.v6.src_ip6, tcf.src_ip,
			       sizeof(cfilter.ip.v6.src_ip6));
		break;
	default:
		 
		dev_info(&pf->pdev->dev, "VF %d: Flow type not configured\n",
			 vf->vf_id);
	}

	 
	vsi = pf->vsi[vf->ch[vcf->action_meta].vsi_idx];
	cfilter.seid = vsi->seid;
	cfilter.flags = vcf->field_flags;

	 
	if (tcf.dst_port)
		ret = i40e_add_del_cloud_filter_big_buf(vsi, &cfilter, false);
	else
		ret = i40e_add_del_cloud_filter(vsi, &cfilter, false);
	if (ret) {
		dev_err(&pf->pdev->dev,
			"VF %d: Failed to delete cloud filter, err %pe aq_err %s\n",
			vf->vf_id, ERR_PTR(ret),
			i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		goto err;
	}

	hlist_for_each_entry_safe(cf, node,
				  &vf->cloud_filter_list, cloud_node) {
		if (cf->seid != cfilter.seid)
			continue;
		if (mask.dst_port)
			if (cfilter.dst_port != cf->dst_port)
				continue;
		if (mask.dst_mac[0])
			if (!ether_addr_equal(cf->src_mac, cfilter.src_mac))
				continue;
		 
		if (cfilter.n_proto == ETH_P_IP && mask.dst_ip[0])
			if (memcmp(&cfilter.ip.v4.dst_ip, &cf->ip.v4.dst_ip,
				   ARRAY_SIZE(tcf.dst_ip)))
				continue;
		 
		if (cfilter.n_proto == ETH_P_IPV6 && mask.dst_ip[3])
			if (memcmp(&cfilter.ip.v6.dst_ip6, &cf->ip.v6.dst_ip6,
				   sizeof(cfilter.ip.v6.src_ip6)))
				continue;
		if (mask.vlan_id)
			if (cfilter.vlan_id != cf->vlan_id)
				continue;

		hlist_del(&cf->cloud_node);
		kfree(cf);
		vf->num_cloud_filters--;
	}

err:
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_DEL_CLOUD_FILTER,
				       aq_ret);
}

 
static int i40e_vc_add_cloud_filter(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_filter *vcf = (struct virtchnl_filter *)msg;
	struct virtchnl_l4_spec mask = vcf->mask.tcp_spec;
	struct virtchnl_l4_spec tcf = vcf->data.tcp_spec;
	struct i40e_cloud_filter *cfilter = NULL;
	struct i40e_pf *pf = vf->pf;
	struct i40e_vsi *vsi = NULL;
	int aq_ret = 0;
	int i;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err_out;
	}

	if (!vf->adq_enabled) {
		dev_info(&pf->pdev->dev,
			 "VF %d: ADq is not enabled, can't apply cloud filter\n",
			 vf->vf_id);
		aq_ret = -EINVAL;
		goto err_out;
	}

	if (i40e_validate_cloud_filter(vf, vcf)) {
		dev_info(&pf->pdev->dev,
			 "VF %d: Invalid input/s, can't apply cloud filter\n",
			 vf->vf_id);
		aq_ret = -EINVAL;
		goto err_out;
	}

	cfilter = kzalloc(sizeof(*cfilter), GFP_KERNEL);
	if (!cfilter) {
		aq_ret = -ENOMEM;
		goto err_out;
	}

	 
	for (i = 0; i < ETH_ALEN; i++)
		cfilter->dst_mac[i] = mask.dst_mac[i] & tcf.dst_mac[i];

	 
	for (i = 0; i < ETH_ALEN; i++)
		cfilter->src_mac[i] = mask.src_mac[i] & tcf.src_mac[i];

	cfilter->vlan_id = mask.vlan_id & tcf.vlan_id;
	cfilter->dst_port = mask.dst_port & tcf.dst_port;
	cfilter->src_port = mask.src_port & tcf.src_port;

	switch (vcf->flow_type) {
	case VIRTCHNL_TCP_V4_FLOW:
		cfilter->n_proto = ETH_P_IP;
		if (mask.dst_ip[0] & tcf.dst_ip[0])
			memcpy(&cfilter->ip.v4.dst_ip, tcf.dst_ip,
			       ARRAY_SIZE(tcf.dst_ip));
		else if (mask.src_ip[0] & tcf.dst_ip[0])
			memcpy(&cfilter->ip.v4.src_ip, tcf.src_ip,
			       ARRAY_SIZE(tcf.dst_ip));
		break;
	case VIRTCHNL_TCP_V6_FLOW:
		cfilter->n_proto = ETH_P_IPV6;
		if (mask.dst_ip[3] & tcf.dst_ip[3])
			memcpy(&cfilter->ip.v6.dst_ip6, tcf.dst_ip,
			       sizeof(cfilter->ip.v6.dst_ip6));
		if (mask.src_ip[3] & tcf.src_ip[3])
			memcpy(&cfilter->ip.v6.src_ip6, tcf.src_ip,
			       sizeof(cfilter->ip.v6.src_ip6));
		break;
	default:
		 
		dev_info(&pf->pdev->dev, "VF %d: Flow type not configured\n",
			 vf->vf_id);
	}

	 
	vsi = pf->vsi[vf->ch[vcf->action_meta].vsi_idx];
	cfilter->seid = vsi->seid;
	cfilter->flags = vcf->field_flags;

	 
	if (tcf.dst_port)
		aq_ret = i40e_add_del_cloud_filter_big_buf(vsi, cfilter, true);
	else
		aq_ret = i40e_add_del_cloud_filter(vsi, cfilter, true);
	if (aq_ret) {
		dev_err(&pf->pdev->dev,
			"VF %d: Failed to add cloud filter, err %pe aq_err %s\n",
			vf->vf_id, ERR_PTR(aq_ret),
			i40e_aq_str(&pf->hw, pf->hw.aq.asq_last_status));
		goto err_free;
	}

	INIT_HLIST_NODE(&cfilter->cloud_node);
	hlist_add_head(&cfilter->cloud_node, &vf->cloud_filter_list);
	 
	cfilter = NULL;
	vf->num_cloud_filters++;
err_free:
	kfree(cfilter);
err_out:
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_ADD_CLOUD_FILTER,
				       aq_ret);
}

 
static int i40e_vc_add_qch_msg(struct i40e_vf *vf, u8 *msg)
{
	struct virtchnl_tc_info *tci =
		(struct virtchnl_tc_info *)msg;
	struct i40e_pf *pf = vf->pf;
	struct i40e_link_status *ls = &pf->hw.phy.link_info;
	int i, adq_request_qps = 0;
	int aq_ret = 0;
	u64 speed = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err;
	}

	 
	if (vf->spoofchk) {
		dev_err(&pf->pdev->dev,
			"Spoof check is ON, turn it OFF to enable ADq\n");
		aq_ret = -EINVAL;
		goto err;
	}

	if (!(vf->driver_caps & VIRTCHNL_VF_OFFLOAD_ADQ)) {
		dev_err(&pf->pdev->dev,
			"VF %d attempting to enable ADq, but hasn't properly negotiated that capability\n",
			vf->vf_id);
		aq_ret = -EINVAL;
		goto err;
	}

	 
	if (!tci->num_tc || tci->num_tc > I40E_MAX_VF_VSI) {
		dev_err(&pf->pdev->dev,
			"VF %d trying to set %u TCs, valid range 1-%u TCs per VF\n",
			vf->vf_id, tci->num_tc, I40E_MAX_VF_VSI);
		aq_ret = -EINVAL;
		goto err;
	}

	 
	for (i = 0; i < tci->num_tc; i++)
		if (!tci->list[i].count ||
		    tci->list[i].count > I40E_DEFAULT_QUEUES_PER_VF) {
			dev_err(&pf->pdev->dev,
				"VF %d: TC %d trying to set %u queues, valid range 1-%u queues per TC\n",
				vf->vf_id, i, tci->list[i].count,
				I40E_DEFAULT_QUEUES_PER_VF);
			aq_ret = -EINVAL;
			goto err;
		}

	 
	adq_request_qps = I40E_MAX_VF_QUEUES - I40E_DEFAULT_QUEUES_PER_VF;

	if (pf->queues_left < adq_request_qps) {
		dev_err(&pf->pdev->dev,
			"No queues left to allocate to VF %d\n",
			vf->vf_id);
		aq_ret = -EINVAL;
		goto err;
	} else {
		 
		vf->num_queue_pairs = I40E_MAX_VF_QUEUES;
	}

	 
	speed = i40e_vc_link_speed2mbps(ls->link_speed);
	if (speed == SPEED_UNKNOWN) {
		dev_err(&pf->pdev->dev,
			"Cannot detect link speed\n");
		aq_ret = -EINVAL;
		goto err;
	}

	 
	vf->num_tc = tci->num_tc;
	for (i = 0; i < vf->num_tc; i++) {
		if (tci->list[i].max_tx_rate) {
			if (tci->list[i].max_tx_rate > speed) {
				dev_err(&pf->pdev->dev,
					"Invalid max tx rate %llu specified for VF %d.",
					tci->list[i].max_tx_rate,
					vf->vf_id);
				aq_ret = -EINVAL;
				goto err;
			} else {
				vf->ch[i].max_tx_rate =
					tci->list[i].max_tx_rate;
			}
		}
		vf->ch[i].num_qps = tci->list[i].count;
	}

	 
	vf->adq_enabled = true;

	 
	i40e_vc_reset_vf(vf, true);

	return 0;

	 
err:
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_ENABLE_CHANNELS,
				       aq_ret);
}

 
static int i40e_vc_del_qch_msg(struct i40e_vf *vf, u8 *msg)
{
	struct i40e_pf *pf = vf->pf;
	int aq_ret = 0;

	if (!i40e_sync_vf_state(vf, I40E_VF_STATE_ACTIVE)) {
		aq_ret = -EINVAL;
		goto err;
	}

	if (vf->adq_enabled) {
		i40e_del_all_cloud_filters(vf);
		i40e_del_qch(vf);
		vf->adq_enabled = false;
		vf->num_tc = 0;
		dev_info(&pf->pdev->dev,
			 "Deleting Queue Channels and cloud filters for ADq on VF %d\n",
			 vf->vf_id);
	} else {
		dev_info(&pf->pdev->dev, "VF %d trying to delete queue channels but ADq isn't enabled\n",
			 vf->vf_id);
		aq_ret = -EINVAL;
	}

	 
	i40e_vc_reset_vf(vf, true);

	return 0;

err:
	return i40e_vc_send_resp_to_vf(vf, VIRTCHNL_OP_DISABLE_CHANNELS,
				       aq_ret);
}

 
int i40e_vc_process_vf_msg(struct i40e_pf *pf, s16 vf_id, u32 v_opcode,
			   u32 __always_unused v_retval, u8 *msg, u16 msglen)
{
	struct i40e_hw *hw = &pf->hw;
	int local_vf_id = vf_id - (s16)hw->func_caps.vf_base_id;
	struct i40e_vf *vf;
	int ret;

	pf->vf_aq_requests++;
	if (local_vf_id < 0 || local_vf_id >= pf->num_alloc_vfs)
		return -EINVAL;
	vf = &(pf->vf[local_vf_id]);

	 
	if (test_bit(I40E_VF_STATE_DISABLED, &vf->vf_states))
		return -EINVAL;

	 
	ret = virtchnl_vc_validate_vf_msg(&vf->vf_ver, v_opcode, msg, msglen);

	if (ret) {
		i40e_vc_send_resp_to_vf(vf, v_opcode, -EINVAL);
		dev_err(&pf->pdev->dev, "Invalid message from VF %d, opcode %d, len %d\n",
			local_vf_id, v_opcode, msglen);
		return ret;
	}

	switch (v_opcode) {
	case VIRTCHNL_OP_VERSION:
		ret = i40e_vc_get_version_msg(vf, msg);
		break;
	case VIRTCHNL_OP_GET_VF_RESOURCES:
		ret = i40e_vc_get_vf_resources_msg(vf, msg);
		i40e_vc_notify_vf_link_state(vf);
		break;
	case VIRTCHNL_OP_RESET_VF:
		i40e_vc_reset_vf(vf, false);
		ret = 0;
		break;
	case VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		ret = i40e_vc_config_promiscuous_mode_msg(vf, msg);
		break;
	case VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		ret = i40e_vc_config_queues_msg(vf, msg);
		break;
	case VIRTCHNL_OP_CONFIG_IRQ_MAP:
		ret = i40e_vc_config_irq_map_msg(vf, msg);
		break;
	case VIRTCHNL_OP_ENABLE_QUEUES:
		ret = i40e_vc_enable_queues_msg(vf, msg);
		i40e_vc_notify_vf_link_state(vf);
		break;
	case VIRTCHNL_OP_DISABLE_QUEUES:
		ret = i40e_vc_disable_queues_msg(vf, msg);
		break;
	case VIRTCHNL_OP_ADD_ETH_ADDR:
		ret = i40e_vc_add_mac_addr_msg(vf, msg);
		break;
	case VIRTCHNL_OP_DEL_ETH_ADDR:
		ret = i40e_vc_del_mac_addr_msg(vf, msg);
		break;
	case VIRTCHNL_OP_ADD_VLAN:
		ret = i40e_vc_add_vlan_msg(vf, msg);
		break;
	case VIRTCHNL_OP_DEL_VLAN:
		ret = i40e_vc_remove_vlan_msg(vf, msg);
		break;
	case VIRTCHNL_OP_GET_STATS:
		ret = i40e_vc_get_stats_msg(vf, msg);
		break;
	case VIRTCHNL_OP_RDMA:
		ret = i40e_vc_rdma_msg(vf, msg, msglen);
		break;
	case VIRTCHNL_OP_CONFIG_RDMA_IRQ_MAP:
		ret = i40e_vc_rdma_qvmap_msg(vf, msg, true);
		break;
	case VIRTCHNL_OP_RELEASE_RDMA_IRQ_MAP:
		ret = i40e_vc_rdma_qvmap_msg(vf, msg, false);
		break;
	case VIRTCHNL_OP_CONFIG_RSS_KEY:
		ret = i40e_vc_config_rss_key(vf, msg);
		break;
	case VIRTCHNL_OP_CONFIG_RSS_LUT:
		ret = i40e_vc_config_rss_lut(vf, msg);
		break;
	case VIRTCHNL_OP_GET_RSS_HENA_CAPS:
		ret = i40e_vc_get_rss_hena(vf, msg);
		break;
	case VIRTCHNL_OP_SET_RSS_HENA:
		ret = i40e_vc_set_rss_hena(vf, msg);
		break;
	case VIRTCHNL_OP_ENABLE_VLAN_STRIPPING:
		ret = i40e_vc_enable_vlan_stripping(vf, msg);
		break;
	case VIRTCHNL_OP_DISABLE_VLAN_STRIPPING:
		ret = i40e_vc_disable_vlan_stripping(vf, msg);
		break;
	case VIRTCHNL_OP_REQUEST_QUEUES:
		ret = i40e_vc_request_queues_msg(vf, msg);
		break;
	case VIRTCHNL_OP_ENABLE_CHANNELS:
		ret = i40e_vc_add_qch_msg(vf, msg);
		break;
	case VIRTCHNL_OP_DISABLE_CHANNELS:
		ret = i40e_vc_del_qch_msg(vf, msg);
		break;
	case VIRTCHNL_OP_ADD_CLOUD_FILTER:
		ret = i40e_vc_add_cloud_filter(vf, msg);
		break;
	case VIRTCHNL_OP_DEL_CLOUD_FILTER:
		ret = i40e_vc_del_cloud_filter(vf, msg);
		break;
	case VIRTCHNL_OP_UNKNOWN:
	default:
		dev_err(&pf->pdev->dev, "Unsupported opcode %d from VF %d\n",
			v_opcode, local_vf_id);
		ret = i40e_vc_send_resp_to_vf(vf, v_opcode,
					      -EOPNOTSUPP);
		break;
	}

	return ret;
}

 
int i40e_vc_process_vflr_event(struct i40e_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u32 reg, reg_idx, bit_idx;
	struct i40e_vf *vf;
	int vf_id;

	if (!test_bit(__I40E_VFLR_EVENT_PENDING, pf->state))
		return 0;

	 
	reg = rd32(hw, I40E_PFINT_ICR0_ENA);
	reg |= I40E_PFINT_ICR0_ENA_VFLR_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);
	i40e_flush(hw);

	clear_bit(__I40E_VFLR_EVENT_PENDING, pf->state);
	for (vf_id = 0; vf_id < pf->num_alloc_vfs; vf_id++) {
		reg_idx = (hw->func_caps.vf_base_id + vf_id) / 32;
		bit_idx = (hw->func_caps.vf_base_id + vf_id) % 32;
		 
		vf = &pf->vf[vf_id];
		reg = rd32(hw, I40E_GLGEN_VFLRSTAT(reg_idx));
		if (reg & BIT(bit_idx))
			 
			i40e_reset_vf(vf, true);
	}

	return 0;
}

 
static int i40e_validate_vf(struct i40e_pf *pf, int vf_id)
{
	struct i40e_vsi *vsi;
	struct i40e_vf *vf;
	int ret = 0;

	if (vf_id >= pf->num_alloc_vfs) {
		dev_err(&pf->pdev->dev,
			"Invalid VF Identifier %d\n", vf_id);
		ret = -EINVAL;
		goto err_out;
	}
	vf = &pf->vf[vf_id];
	vsi = i40e_find_vsi_from_id(pf, vf->lan_vsi_id);
	if (!vsi)
		ret = -EINVAL;
err_out:
	return ret;
}

 
static bool i40e_check_vf_init_timeout(struct i40e_vf *vf)
{
	int i;

	 
	for (i = 0; i < 15; i++) {
		if (test_bit(I40E_VF_STATE_INIT, &vf->vf_states))
			return true;
		msleep(20);
	}

	if (!test_bit(I40E_VF_STATE_INIT, &vf->vf_states)) {
		dev_err(&vf->pf->pdev->dev,
			"VF %d still in reset. Try again.\n", vf->vf_id);
		return false;
	}

	return true;
}

 
int i40e_ndo_set_vf_mac(struct net_device *netdev, int vf_id, u8 *mac)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_mac_filter *f;
	struct i40e_vf *vf;
	int ret = 0;
	struct hlist_node *h;
	int bkt;

	if (test_and_set_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state)) {
		dev_warn(&pf->pdev->dev, "Unable to configure VFs, other operation is pending.\n");
		return -EAGAIN;
	}

	 
	ret = i40e_validate_vf(pf, vf_id);
	if (ret)
		goto error_param;

	vf = &pf->vf[vf_id];
	if (!i40e_check_vf_init_timeout(vf)) {
		ret = -EAGAIN;
		goto error_param;
	}
	vsi = pf->vsi[vf->lan_vsi_idx];

	if (is_multicast_ether_addr(mac)) {
		dev_err(&pf->pdev->dev,
			"Invalid Ethernet address %pM for VF %d\n", mac, vf_id);
		ret = -EINVAL;
		goto error_param;
	}

	 
	spin_lock_bh(&vsi->mac_filter_hash_lock);

	 
	if (!is_zero_ether_addr(vf->default_lan_addr.addr))
		i40e_del_mac_filter(vsi, vf->default_lan_addr.addr);

	 
	hash_for_each_safe(vsi->mac_filter_hash, bkt, h, f, hlist)
		__i40e_del_filter(vsi, f);

	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	 
	if (i40e_sync_vsi_filters(vsi)) {
		dev_err(&pf->pdev->dev, "Unable to program ucast filters\n");
		ret = -EIO;
		goto error_param;
	}
	ether_addr_copy(vf->default_lan_addr.addr, mac);

	if (is_zero_ether_addr(mac)) {
		vf->pf_set_mac = false;
		dev_info(&pf->pdev->dev, "Removing MAC on VF %d\n", vf_id);
	} else {
		vf->pf_set_mac = true;
		dev_info(&pf->pdev->dev, "Setting MAC %pM on VF %d\n",
			 mac, vf_id);
	}

	 
	i40e_vc_reset_vf(vf, true);
	dev_info(&pf->pdev->dev, "Bring down and up the VF interface to make this change effective.\n");

error_param:
	clear_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state);
	return ret;
}

 
int i40e_ndo_set_vf_port_vlan(struct net_device *netdev, int vf_id,
			      u16 vlan_id, u8 qos, __be16 vlan_proto)
{
	u16 vlanprio = vlan_id | (qos << I40E_VLAN_PRIORITY_SHIFT);
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	bool allmulti = false, alluni = false;
	struct i40e_pf *pf = np->vsi->back;
	struct i40e_vsi *vsi;
	struct i40e_vf *vf;
	int ret = 0;

	if (test_and_set_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state)) {
		dev_warn(&pf->pdev->dev, "Unable to configure VFs, other operation is pending.\n");
		return -EAGAIN;
	}

	 
	ret = i40e_validate_vf(pf, vf_id);
	if (ret)
		goto error_pvid;

	if ((vlan_id > I40E_MAX_VLANID) || (qos > 7)) {
		dev_err(&pf->pdev->dev, "Invalid VF Parameters\n");
		ret = -EINVAL;
		goto error_pvid;
	}

	if (vlan_proto != htons(ETH_P_8021Q)) {
		dev_err(&pf->pdev->dev, "VF VLAN protocol is not supported\n");
		ret = -EPROTONOSUPPORT;
		goto error_pvid;
	}

	vf = &pf->vf[vf_id];
	if (!i40e_check_vf_init_timeout(vf)) {
		ret = -EAGAIN;
		goto error_pvid;
	}
	vsi = pf->vsi[vf->lan_vsi_idx];

	if (le16_to_cpu(vsi->info.pvid) == vlanprio)
		 
		goto error_pvid;

	i40e_vlan_stripping_enable(vsi);

	 
	spin_lock_bh(&vsi->mac_filter_hash_lock);

	 
	if ((!(vlan_id || qos) ||
	     vlanprio != le16_to_cpu(vsi->info.pvid)) &&
	    vsi->info.pvid) {
		ret = i40e_add_vlan_all_mac(vsi, I40E_VLAN_ANY);
		if (ret) {
			dev_info(&vsi->back->pdev->dev,
				 "add VF VLAN failed, ret=%d aq_err=%d\n", ret,
				 vsi->back->hw.aq.asq_last_status);
			spin_unlock_bh(&vsi->mac_filter_hash_lock);
			goto error_pvid;
		}
	}

	if (vsi->info.pvid) {
		 
		i40e_rm_vlan_all_mac(vsi, (le16_to_cpu(vsi->info.pvid) &
					   VLAN_VID_MASK));
	}

	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	 
	ret = i40e_config_vf_promiscuous_mode(vf, vf->lan_vsi_id,
					      allmulti, alluni);
	if (ret) {
		dev_err(&pf->pdev->dev, "Unable to config VF promiscuous mode\n");
		goto error_pvid;
	}

	if (vlan_id || qos)
		ret = i40e_vsi_add_pvid(vsi, vlanprio);
	else
		i40e_vsi_remove_pvid(vsi);
	spin_lock_bh(&vsi->mac_filter_hash_lock);

	if (vlan_id) {
		dev_info(&pf->pdev->dev, "Setting VLAN %d, QOS 0x%x on VF %d\n",
			 vlan_id, qos, vf_id);

		 
		ret = i40e_add_vlan_all_mac(vsi, vlan_id);
		if (ret) {
			dev_info(&vsi->back->pdev->dev,
				 "add VF VLAN failed, ret=%d aq_err=%d\n", ret,
				 vsi->back->hw.aq.asq_last_status);
			spin_unlock_bh(&vsi->mac_filter_hash_lock);
			goto error_pvid;
		}

		 
		i40e_rm_vlan_all_mac(vsi, I40E_VLAN_ANY);
	}

	spin_unlock_bh(&vsi->mac_filter_hash_lock);

	if (test_bit(I40E_VF_STATE_UC_PROMISC, &vf->vf_states))
		alluni = true;

	if (test_bit(I40E_VF_STATE_MC_PROMISC, &vf->vf_states))
		allmulti = true;

	 
	i40e_service_event_schedule(vsi->back);

	if (ret) {
		dev_err(&pf->pdev->dev, "Unable to update VF vsi context\n");
		goto error_pvid;
	}

	 
	vf->port_vlan_id = le16_to_cpu(vsi->info.pvid);

	i40e_vc_reset_vf(vf, true);
	 
	vsi = pf->vsi[vf->lan_vsi_idx];

	ret = i40e_config_vf_promiscuous_mode(vf, vsi->id, allmulti, alluni);
	if (ret) {
		dev_err(&pf->pdev->dev, "Unable to config vf promiscuous mode\n");
		goto error_pvid;
	}

	ret = 0;

error_pvid:
	clear_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state);
	return ret;
}

 
int i40e_ndo_set_vf_bw(struct net_device *netdev, int vf_id, int min_tx_rate,
		       int max_tx_rate)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_pf *pf = np->vsi->back;
	struct i40e_vsi *vsi;
	struct i40e_vf *vf;
	int ret = 0;

	if (test_and_set_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state)) {
		dev_warn(&pf->pdev->dev, "Unable to configure VFs, other operation is pending.\n");
		return -EAGAIN;
	}

	 
	ret = i40e_validate_vf(pf, vf_id);
	if (ret)
		goto error;

	if (min_tx_rate) {
		dev_err(&pf->pdev->dev, "Invalid min tx rate (%d) (greater than 0) specified for VF %d.\n",
			min_tx_rate, vf_id);
		ret = -EINVAL;
		goto error;
	}

	vf = &pf->vf[vf_id];
	if (!i40e_check_vf_init_timeout(vf)) {
		ret = -EAGAIN;
		goto error;
	}
	vsi = pf->vsi[vf->lan_vsi_idx];

	ret = i40e_set_bw_limit(vsi, vsi->seid, max_tx_rate);
	if (ret)
		goto error;

	vf->tx_rate = max_tx_rate;
error:
	clear_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state);
	return ret;
}

 
int i40e_ndo_get_vf_config(struct net_device *netdev,
			   int vf_id, struct ifla_vf_info *ivi)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_vf *vf;
	int ret = 0;

	if (test_and_set_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state)) {
		dev_warn(&pf->pdev->dev, "Unable to configure VFs, other operation is pending.\n");
		return -EAGAIN;
	}

	 
	ret = i40e_validate_vf(pf, vf_id);
	if (ret)
		goto error_param;

	vf = &pf->vf[vf_id];
	 
	vsi = pf->vsi[vf->lan_vsi_idx];
	if (!vsi) {
		ret = -ENOENT;
		goto error_param;
	}

	ivi->vf = vf_id;

	ether_addr_copy(ivi->mac, vf->default_lan_addr.addr);

	ivi->max_tx_rate = vf->tx_rate;
	ivi->min_tx_rate = 0;
	ivi->vlan = le16_to_cpu(vsi->info.pvid) & I40E_VLAN_MASK;
	ivi->qos = (le16_to_cpu(vsi->info.pvid) & I40E_PRIORITY_MASK) >>
		   I40E_VLAN_PRIORITY_SHIFT;
	if (vf->link_forced == false)
		ivi->linkstate = IFLA_VF_LINK_STATE_AUTO;
	else if (vf->link_up == true)
		ivi->linkstate = IFLA_VF_LINK_STATE_ENABLE;
	else
		ivi->linkstate = IFLA_VF_LINK_STATE_DISABLE;
	ivi->spoofchk = vf->spoofchk;
	ivi->trusted = vf->trusted;
	ret = 0;

error_param:
	clear_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state);
	return ret;
}

 
int i40e_ndo_set_vf_link_state(struct net_device *netdev, int vf_id, int link)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_pf *pf = np->vsi->back;
	struct i40e_link_status *ls = &pf->hw.phy.link_info;
	struct virtchnl_pf_event pfe;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vf *vf;
	int abs_vf_id;
	int ret = 0;

	if (test_and_set_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state)) {
		dev_warn(&pf->pdev->dev, "Unable to configure VFs, other operation is pending.\n");
		return -EAGAIN;
	}

	 
	if (vf_id >= pf->num_alloc_vfs) {
		dev_err(&pf->pdev->dev, "Invalid VF Identifier %d\n", vf_id);
		ret = -EINVAL;
		goto error_out;
	}

	vf = &pf->vf[vf_id];
	abs_vf_id = vf->vf_id + hw->func_caps.vf_base_id;

	pfe.event = VIRTCHNL_EVENT_LINK_CHANGE;
	pfe.severity = PF_EVENT_SEVERITY_INFO;

	switch (link) {
	case IFLA_VF_LINK_STATE_AUTO:
		vf->link_forced = false;
		i40e_set_vf_link_state(vf, &pfe, ls);
		break;
	case IFLA_VF_LINK_STATE_ENABLE:
		vf->link_forced = true;
		vf->link_up = true;
		i40e_set_vf_link_state(vf, &pfe, ls);
		break;
	case IFLA_VF_LINK_STATE_DISABLE:
		vf->link_forced = true;
		vf->link_up = false;
		i40e_set_vf_link_state(vf, &pfe, ls);
		break;
	default:
		ret = -EINVAL;
		goto error_out;
	}
	 
	i40e_aq_send_msg_to_vf(hw, abs_vf_id, VIRTCHNL_OP_EVENT,
			       0, (u8 *)&pfe, sizeof(pfe), NULL);

error_out:
	clear_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state);
	return ret;
}

 
int i40e_ndo_set_vf_spoofchk(struct net_device *netdev, int vf_id, bool enable)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_vsi_context ctxt;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_vf *vf;
	int ret = 0;

	if (test_and_set_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state)) {
		dev_warn(&pf->pdev->dev, "Unable to configure VFs, other operation is pending.\n");
		return -EAGAIN;
	}

	 
	if (vf_id >= pf->num_alloc_vfs) {
		dev_err(&pf->pdev->dev, "Invalid VF Identifier %d\n", vf_id);
		ret = -EINVAL;
		goto out;
	}

	vf = &(pf->vf[vf_id]);
	if (!i40e_check_vf_init_timeout(vf)) {
		ret = -EAGAIN;
		goto out;
	}

	if (enable == vf->spoofchk)
		goto out;

	vf->spoofchk = enable;
	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.seid = pf->vsi[vf->lan_vsi_idx]->seid;
	ctxt.pf_num = pf->hw.pf_id;
	ctxt.info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_SECURITY_VALID);
	if (enable)
		ctxt.info.sec_flags |= (I40E_AQ_VSI_SEC_FLAG_ENABLE_VLAN_CHK |
					I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK);
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret) {
		dev_err(&pf->pdev->dev, "Error %d updating VSI parameters\n",
			ret);
		ret = -EIO;
	}
out:
	clear_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state);
	return ret;
}

 
int i40e_ndo_set_vf_trust(struct net_device *netdev, int vf_id, bool setting)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_pf *pf = np->vsi->back;
	struct i40e_vf *vf;
	int ret = 0;

	if (test_and_set_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state)) {
		dev_warn(&pf->pdev->dev, "Unable to configure VFs, other operation is pending.\n");
		return -EAGAIN;
	}

	 
	if (vf_id >= pf->num_alloc_vfs) {
		dev_err(&pf->pdev->dev, "Invalid VF Identifier %d\n", vf_id);
		ret = -EINVAL;
		goto out;
	}

	if (pf->flags & I40E_FLAG_MFP_ENABLED) {
		dev_err(&pf->pdev->dev, "Trusted VF not supported in MFP mode.\n");
		ret = -EINVAL;
		goto out;
	}

	vf = &pf->vf[vf_id];

	if (setting == vf->trusted)
		goto out;

	vf->trusted = setting;

	 
	set_bit(__I40E_MACVLAN_SYNC_PENDING, pf->state);
	pf->vsi[vf->lan_vsi_idx]->flags |= I40E_VSI_FLAG_FILTER_CHANGED;

	i40e_vc_reset_vf(vf, true);
	dev_info(&pf->pdev->dev, "VF %u is now %strusted\n",
		 vf_id, setting ? "" : "un");

	if (vf->adq_enabled) {
		if (!vf->trusted) {
			dev_info(&pf->pdev->dev,
				 "VF %u no longer Trusted, deleting all cloud filters\n",
				 vf_id);
			i40e_del_all_cloud_filters(vf);
		}
	}

out:
	clear_bit(__I40E_VIRTCHNL_OP_PENDING, pf->state);
	return ret;
}

 
int i40e_get_vf_stats(struct net_device *netdev, int vf_id,
		      struct ifla_vf_stats *vf_stats)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_pf *pf = np->vsi->back;
	struct i40e_eth_stats *stats;
	struct i40e_vsi *vsi;
	struct i40e_vf *vf;

	 
	if (i40e_validate_vf(pf, vf_id))
		return -EINVAL;

	vf = &pf->vf[vf_id];
	if (!test_bit(I40E_VF_STATE_INIT, &vf->vf_states)) {
		dev_err(&pf->pdev->dev, "VF %d in reset. Try again.\n", vf_id);
		return -EBUSY;
	}

	vsi = pf->vsi[vf->lan_vsi_idx];
	if (!vsi)
		return -EINVAL;

	i40e_update_eth_stats(vsi);
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

	return 0;
}
