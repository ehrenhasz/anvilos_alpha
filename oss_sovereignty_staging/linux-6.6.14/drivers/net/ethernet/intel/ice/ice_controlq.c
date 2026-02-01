
 

#include "ice_common.h"

#define ICE_CQ_INIT_REGS(qinfo, prefix)				\
do {								\
	(qinfo)->sq.head = prefix##_ATQH;			\
	(qinfo)->sq.tail = prefix##_ATQT;			\
	(qinfo)->sq.len = prefix##_ATQLEN;			\
	(qinfo)->sq.bah = prefix##_ATQBAH;			\
	(qinfo)->sq.bal = prefix##_ATQBAL;			\
	(qinfo)->sq.len_mask = prefix##_ATQLEN_ATQLEN_M;	\
	(qinfo)->sq.len_ena_mask = prefix##_ATQLEN_ATQENABLE_M;	\
	(qinfo)->sq.len_crit_mask = prefix##_ATQLEN_ATQCRIT_M;	\
	(qinfo)->sq.head_mask = prefix##_ATQH_ATQH_M;		\
	(qinfo)->rq.head = prefix##_ARQH;			\
	(qinfo)->rq.tail = prefix##_ARQT;			\
	(qinfo)->rq.len = prefix##_ARQLEN;			\
	(qinfo)->rq.bah = prefix##_ARQBAH;			\
	(qinfo)->rq.bal = prefix##_ARQBAL;			\
	(qinfo)->rq.len_mask = prefix##_ARQLEN_ARQLEN_M;	\
	(qinfo)->rq.len_ena_mask = prefix##_ARQLEN_ARQENABLE_M;	\
	(qinfo)->rq.len_crit_mask = prefix##_ARQLEN_ARQCRIT_M;	\
	(qinfo)->rq.head_mask = prefix##_ARQH_ARQH_M;		\
} while (0)

 
static void ice_adminq_init_regs(struct ice_hw *hw)
{
	struct ice_ctl_q_info *cq = &hw->adminq;

	ICE_CQ_INIT_REGS(cq, PF_FW);
}

 
static void ice_mailbox_init_regs(struct ice_hw *hw)
{
	struct ice_ctl_q_info *cq = &hw->mailboxq;

	ICE_CQ_INIT_REGS(cq, PF_MBX);
}

 
static void ice_sb_init_regs(struct ice_hw *hw)
{
	struct ice_ctl_q_info *cq = &hw->sbq;

	ICE_CQ_INIT_REGS(cq, PF_SB);
}

 
bool ice_check_sq_alive(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	 
	if (cq->sq.len && cq->sq.len_mask && cq->sq.len_ena_mask)
		return (rd32(hw, cq->sq.len) & (cq->sq.len_mask |
						cq->sq.len_ena_mask)) ==
			(cq->num_sq_entries | cq->sq.len_ena_mask);

	return false;
}

 
static int
ice_alloc_ctrlq_sq_ring(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	size_t size = cq->num_sq_entries * sizeof(struct ice_aq_desc);

	cq->sq.desc_buf.va = dmam_alloc_coherent(ice_hw_to_dev(hw), size,
						 &cq->sq.desc_buf.pa,
						 GFP_KERNEL | __GFP_ZERO);
	if (!cq->sq.desc_buf.va)
		return -ENOMEM;
	cq->sq.desc_buf.size = size;

	cq->sq.cmd_buf = devm_kcalloc(ice_hw_to_dev(hw), cq->num_sq_entries,
				      sizeof(struct ice_sq_cd), GFP_KERNEL);
	if (!cq->sq.cmd_buf) {
		dmam_free_coherent(ice_hw_to_dev(hw), cq->sq.desc_buf.size,
				   cq->sq.desc_buf.va, cq->sq.desc_buf.pa);
		cq->sq.desc_buf.va = NULL;
		cq->sq.desc_buf.pa = 0;
		cq->sq.desc_buf.size = 0;
		return -ENOMEM;
	}

	return 0;
}

 
static int
ice_alloc_ctrlq_rq_ring(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	size_t size = cq->num_rq_entries * sizeof(struct ice_aq_desc);

	cq->rq.desc_buf.va = dmam_alloc_coherent(ice_hw_to_dev(hw), size,
						 &cq->rq.desc_buf.pa,
						 GFP_KERNEL | __GFP_ZERO);
	if (!cq->rq.desc_buf.va)
		return -ENOMEM;
	cq->rq.desc_buf.size = size;
	return 0;
}

 
static void ice_free_cq_ring(struct ice_hw *hw, struct ice_ctl_q_ring *ring)
{
	dmam_free_coherent(ice_hw_to_dev(hw), ring->desc_buf.size,
			   ring->desc_buf.va, ring->desc_buf.pa);
	ring->desc_buf.va = NULL;
	ring->desc_buf.pa = 0;
	ring->desc_buf.size = 0;
}

 
static int
ice_alloc_rq_bufs(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	int i;

	 
	cq->rq.dma_head = devm_kcalloc(ice_hw_to_dev(hw), cq->num_rq_entries,
				       sizeof(cq->rq.desc_buf), GFP_KERNEL);
	if (!cq->rq.dma_head)
		return -ENOMEM;
	cq->rq.r.rq_bi = (struct ice_dma_mem *)cq->rq.dma_head;

	 
	for (i = 0; i < cq->num_rq_entries; i++) {
		struct ice_aq_desc *desc;
		struct ice_dma_mem *bi;

		bi = &cq->rq.r.rq_bi[i];
		bi->va = dmam_alloc_coherent(ice_hw_to_dev(hw),
					     cq->rq_buf_size, &bi->pa,
					     GFP_KERNEL | __GFP_ZERO);
		if (!bi->va)
			goto unwind_alloc_rq_bufs;
		bi->size = cq->rq_buf_size;

		 
		desc = ICE_CTL_Q_DESC(cq->rq, i);

		desc->flags = cpu_to_le16(ICE_AQ_FLAG_BUF);
		if (cq->rq_buf_size > ICE_AQ_LG_BUF)
			desc->flags |= cpu_to_le16(ICE_AQ_FLAG_LB);
		desc->opcode = 0;
		 
		desc->datalen = cpu_to_le16(bi->size);
		desc->retval = 0;
		desc->cookie_high = 0;
		desc->cookie_low = 0;
		desc->params.generic.addr_high =
			cpu_to_le32(upper_32_bits(bi->pa));
		desc->params.generic.addr_low =
			cpu_to_le32(lower_32_bits(bi->pa));
		desc->params.generic.param0 = 0;
		desc->params.generic.param1 = 0;
	}
	return 0;

unwind_alloc_rq_bufs:
	 
	i--;
	for (; i >= 0; i--) {
		dmam_free_coherent(ice_hw_to_dev(hw), cq->rq.r.rq_bi[i].size,
				   cq->rq.r.rq_bi[i].va, cq->rq.r.rq_bi[i].pa);
		cq->rq.r.rq_bi[i].va = NULL;
		cq->rq.r.rq_bi[i].pa = 0;
		cq->rq.r.rq_bi[i].size = 0;
	}
	cq->rq.r.rq_bi = NULL;
	devm_kfree(ice_hw_to_dev(hw), cq->rq.dma_head);
	cq->rq.dma_head = NULL;

	return -ENOMEM;
}

 
static int
ice_alloc_sq_bufs(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	int i;

	 
	cq->sq.dma_head = devm_kcalloc(ice_hw_to_dev(hw), cq->num_sq_entries,
				       sizeof(cq->sq.desc_buf), GFP_KERNEL);
	if (!cq->sq.dma_head)
		return -ENOMEM;
	cq->sq.r.sq_bi = (struct ice_dma_mem *)cq->sq.dma_head;

	 
	for (i = 0; i < cq->num_sq_entries; i++) {
		struct ice_dma_mem *bi;

		bi = &cq->sq.r.sq_bi[i];
		bi->va = dmam_alloc_coherent(ice_hw_to_dev(hw),
					     cq->sq_buf_size, &bi->pa,
					     GFP_KERNEL | __GFP_ZERO);
		if (!bi->va)
			goto unwind_alloc_sq_bufs;
		bi->size = cq->sq_buf_size;
	}
	return 0;

unwind_alloc_sq_bufs:
	 
	i--;
	for (; i >= 0; i--) {
		dmam_free_coherent(ice_hw_to_dev(hw), cq->sq.r.sq_bi[i].size,
				   cq->sq.r.sq_bi[i].va, cq->sq.r.sq_bi[i].pa);
		cq->sq.r.sq_bi[i].va = NULL;
		cq->sq.r.sq_bi[i].pa = 0;
		cq->sq.r.sq_bi[i].size = 0;
	}
	cq->sq.r.sq_bi = NULL;
	devm_kfree(ice_hw_to_dev(hw), cq->sq.dma_head);
	cq->sq.dma_head = NULL;

	return -ENOMEM;
}

static int
ice_cfg_cq_regs(struct ice_hw *hw, struct ice_ctl_q_ring *ring, u16 num_entries)
{
	 
	wr32(hw, ring->head, 0);
	wr32(hw, ring->tail, 0);

	 
	wr32(hw, ring->len, (num_entries | ring->len_ena_mask));
	wr32(hw, ring->bal, lower_32_bits(ring->desc_buf.pa));
	wr32(hw, ring->bah, upper_32_bits(ring->desc_buf.pa));

	 
	if (rd32(hw, ring->bal) != lower_32_bits(ring->desc_buf.pa))
		return -EIO;

	return 0;
}

 
static int ice_cfg_sq_regs(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	return ice_cfg_cq_regs(hw, &cq->sq, cq->num_sq_entries);
}

 
static int ice_cfg_rq_regs(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	int status;

	status = ice_cfg_cq_regs(hw, &cq->rq, cq->num_rq_entries);
	if (status)
		return status;

	 
	wr32(hw, cq->rq.tail, (u32)(cq->num_rq_entries - 1));

	return 0;
}

#define ICE_FREE_CQ_BUFS(hw, qi, ring)					\
do {									\
	 						\
	if ((qi)->ring.r.ring##_bi) {					\
		int i;							\
									\
		for (i = 0; i < (qi)->num_##ring##_entries; i++)	\
			if ((qi)->ring.r.ring##_bi[i].pa) {		\
				dmam_free_coherent(ice_hw_to_dev(hw),	\
					(qi)->ring.r.ring##_bi[i].size,	\
					(qi)->ring.r.ring##_bi[i].va,	\
					(qi)->ring.r.ring##_bi[i].pa);	\
					(qi)->ring.r.ring##_bi[i].va = NULL;\
					(qi)->ring.r.ring##_bi[i].pa = 0;\
					(qi)->ring.r.ring##_bi[i].size = 0;\
		}							\
	}								\
	 					\
	devm_kfree(ice_hw_to_dev(hw), (qi)->ring.cmd_buf);		\
	 						\
	devm_kfree(ice_hw_to_dev(hw), (qi)->ring.dma_head);		\
} while (0)

 
static int ice_init_sq(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	int ret_code;

	if (cq->sq.count > 0) {
		 
		ret_code = -EBUSY;
		goto init_ctrlq_exit;
	}

	 
	if (!cq->num_sq_entries || !cq->sq_buf_size) {
		ret_code = -EIO;
		goto init_ctrlq_exit;
	}

	cq->sq.next_to_use = 0;
	cq->sq.next_to_clean = 0;

	 
	ret_code = ice_alloc_ctrlq_sq_ring(hw, cq);
	if (ret_code)
		goto init_ctrlq_exit;

	 
	ret_code = ice_alloc_sq_bufs(hw, cq);
	if (ret_code)
		goto init_ctrlq_free_rings;

	 
	ret_code = ice_cfg_sq_regs(hw, cq);
	if (ret_code)
		goto init_ctrlq_free_rings;

	 
	cq->sq.count = cq->num_sq_entries;
	goto init_ctrlq_exit;

init_ctrlq_free_rings:
	ICE_FREE_CQ_BUFS(hw, cq, sq);
	ice_free_cq_ring(hw, &cq->sq);

init_ctrlq_exit:
	return ret_code;
}

 
static int ice_init_rq(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	int ret_code;

	if (cq->rq.count > 0) {
		 
		ret_code = -EBUSY;
		goto init_ctrlq_exit;
	}

	 
	if (!cq->num_rq_entries || !cq->rq_buf_size) {
		ret_code = -EIO;
		goto init_ctrlq_exit;
	}

	cq->rq.next_to_use = 0;
	cq->rq.next_to_clean = 0;

	 
	ret_code = ice_alloc_ctrlq_rq_ring(hw, cq);
	if (ret_code)
		goto init_ctrlq_exit;

	 
	ret_code = ice_alloc_rq_bufs(hw, cq);
	if (ret_code)
		goto init_ctrlq_free_rings;

	 
	ret_code = ice_cfg_rq_regs(hw, cq);
	if (ret_code)
		goto init_ctrlq_free_rings;

	 
	cq->rq.count = cq->num_rq_entries;
	goto init_ctrlq_exit;

init_ctrlq_free_rings:
	ICE_FREE_CQ_BUFS(hw, cq, rq);
	ice_free_cq_ring(hw, &cq->rq);

init_ctrlq_exit:
	return ret_code;
}

 
static int ice_shutdown_sq(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	int ret_code = 0;

	mutex_lock(&cq->sq_lock);

	if (!cq->sq.count) {
		ret_code = -EBUSY;
		goto shutdown_sq_out;
	}

	 
	wr32(hw, cq->sq.head, 0);
	wr32(hw, cq->sq.tail, 0);
	wr32(hw, cq->sq.len, 0);
	wr32(hw, cq->sq.bal, 0);
	wr32(hw, cq->sq.bah, 0);

	cq->sq.count = 0;	 

	 
	ICE_FREE_CQ_BUFS(hw, cq, sq);
	ice_free_cq_ring(hw, &cq->sq);

shutdown_sq_out:
	mutex_unlock(&cq->sq_lock);
	return ret_code;
}

 
static bool ice_aq_ver_check(struct ice_hw *hw)
{
	if (hw->api_maj_ver > EXP_FW_API_VER_MAJOR) {
		 
		dev_warn(ice_hw_to_dev(hw),
			 "The driver for the device stopped because the NVM image is newer than expected. You must install the most recent version of the network driver.\n");
		return false;
	} else if (hw->api_maj_ver == EXP_FW_API_VER_MAJOR) {
		if (hw->api_min_ver > (EXP_FW_API_VER_MINOR + 2))
			dev_info(ice_hw_to_dev(hw),
				 "The driver for the device detected a newer version of the NVM image than expected. Please install the most recent version of the network driver.\n");
		else if ((hw->api_min_ver + 2) < EXP_FW_API_VER_MINOR)
			dev_info(ice_hw_to_dev(hw),
				 "The driver for the device detected an older version of the NVM image than expected. Please update the NVM image.\n");
	} else {
		 
		dev_info(ice_hw_to_dev(hw),
			 "The driver for the device detected an older version of the NVM image than expected. Please update the NVM image.\n");
	}
	return true;
}

 
static int ice_shutdown_rq(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	int ret_code = 0;

	mutex_lock(&cq->rq_lock);

	if (!cq->rq.count) {
		ret_code = -EBUSY;
		goto shutdown_rq_out;
	}

	 
	wr32(hw, cq->rq.head, 0);
	wr32(hw, cq->rq.tail, 0);
	wr32(hw, cq->rq.len, 0);
	wr32(hw, cq->rq.bal, 0);
	wr32(hw, cq->rq.bah, 0);

	 
	cq->rq.count = 0;

	 
	ICE_FREE_CQ_BUFS(hw, cq, rq);
	ice_free_cq_ring(hw, &cq->rq);

shutdown_rq_out:
	mutex_unlock(&cq->rq_lock);
	return ret_code;
}

 
static int ice_init_check_adminq(struct ice_hw *hw)
{
	struct ice_ctl_q_info *cq = &hw->adminq;
	int status;

	status = ice_aq_get_fw_ver(hw, NULL);
	if (status)
		goto init_ctrlq_free_rq;

	if (!ice_aq_ver_check(hw)) {
		status = -EIO;
		goto init_ctrlq_free_rq;
	}

	return 0;

init_ctrlq_free_rq:
	ice_shutdown_rq(hw, cq);
	ice_shutdown_sq(hw, cq);
	return status;
}

 
static int ice_init_ctrlq(struct ice_hw *hw, enum ice_ctl_q q_type)
{
	struct ice_ctl_q_info *cq;
	int ret_code;

	switch (q_type) {
	case ICE_CTL_Q_ADMIN:
		ice_adminq_init_regs(hw);
		cq = &hw->adminq;
		break;
	case ICE_CTL_Q_SB:
		ice_sb_init_regs(hw);
		cq = &hw->sbq;
		break;
	case ICE_CTL_Q_MAILBOX:
		ice_mailbox_init_regs(hw);
		cq = &hw->mailboxq;
		break;
	default:
		return -EINVAL;
	}
	cq->qtype = q_type;

	 
	if (!cq->num_rq_entries || !cq->num_sq_entries ||
	    !cq->rq_buf_size || !cq->sq_buf_size) {
		return -EIO;
	}

	 
	ret_code = ice_init_sq(hw, cq);
	if (ret_code)
		return ret_code;

	 
	ret_code = ice_init_rq(hw, cq);
	if (ret_code)
		goto init_ctrlq_free_sq;

	 
	return 0;

init_ctrlq_free_sq:
	ice_shutdown_sq(hw, cq);
	return ret_code;
}

 
bool ice_is_sbq_supported(struct ice_hw *hw)
{
	 
	return hw->mac_type == ICE_MAC_GENERIC;
}

 
struct ice_ctl_q_info *ice_get_sbq(struct ice_hw *hw)
{
	if (ice_is_sbq_supported(hw))
		return &hw->sbq;
	return &hw->adminq;
}

 
static void ice_shutdown_ctrlq(struct ice_hw *hw, enum ice_ctl_q q_type)
{
	struct ice_ctl_q_info *cq;

	switch (q_type) {
	case ICE_CTL_Q_ADMIN:
		cq = &hw->adminq;
		if (ice_check_sq_alive(hw, cq))
			ice_aq_q_shutdown(hw, true);
		break;
	case ICE_CTL_Q_SB:
		cq = &hw->sbq;
		break;
	case ICE_CTL_Q_MAILBOX:
		cq = &hw->mailboxq;
		break;
	default:
		return;
	}

	ice_shutdown_sq(hw, cq);
	ice_shutdown_rq(hw, cq);
}

 
void ice_shutdown_all_ctrlq(struct ice_hw *hw)
{
	 
	ice_shutdown_ctrlq(hw, ICE_CTL_Q_ADMIN);
	 
	if (ice_is_sbq_supported(hw))
		ice_shutdown_ctrlq(hw, ICE_CTL_Q_SB);
	 
	ice_shutdown_ctrlq(hw, ICE_CTL_Q_MAILBOX);
}

 
int ice_init_all_ctrlq(struct ice_hw *hw)
{
	u32 retry = 0;
	int status;

	 
	do {
		status = ice_init_ctrlq(hw, ICE_CTL_Q_ADMIN);
		if (status)
			return status;

		status = ice_init_check_adminq(hw);
		if (status != -EIO)
			break;

		ice_debug(hw, ICE_DBG_AQ_MSG, "Retry Admin Queue init due to FW critical error\n");
		ice_shutdown_ctrlq(hw, ICE_CTL_Q_ADMIN);
		msleep(ICE_CTL_Q_ADMIN_INIT_MSEC);
	} while (retry++ < ICE_CTL_Q_ADMIN_INIT_TIMEOUT);

	if (status)
		return status;
	 
	if (ice_is_sbq_supported(hw)) {
		status = ice_init_ctrlq(hw, ICE_CTL_Q_SB);
		if (status)
			return status;
	}
	 
	return ice_init_ctrlq(hw, ICE_CTL_Q_MAILBOX);
}

 
static void ice_init_ctrlq_locks(struct ice_ctl_q_info *cq)
{
	mutex_init(&cq->sq_lock);
	mutex_init(&cq->rq_lock);
}

 
int ice_create_all_ctrlq(struct ice_hw *hw)
{
	ice_init_ctrlq_locks(&hw->adminq);
	if (ice_is_sbq_supported(hw))
		ice_init_ctrlq_locks(&hw->sbq);
	ice_init_ctrlq_locks(&hw->mailboxq);

	return ice_init_all_ctrlq(hw);
}

 
static void ice_destroy_ctrlq_locks(struct ice_ctl_q_info *cq)
{
	mutex_destroy(&cq->sq_lock);
	mutex_destroy(&cq->rq_lock);
}

 
void ice_destroy_all_ctrlq(struct ice_hw *hw)
{
	 
	ice_shutdown_all_ctrlq(hw);

	ice_destroy_ctrlq_locks(&hw->adminq);
	if (ice_is_sbq_supported(hw))
		ice_destroy_ctrlq_locks(&hw->sbq);
	ice_destroy_ctrlq_locks(&hw->mailboxq);
}

 
static u16 ice_clean_sq(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	struct ice_ctl_q_ring *sq = &cq->sq;
	u16 ntc = sq->next_to_clean;
	struct ice_sq_cd *details;
	struct ice_aq_desc *desc;

	desc = ICE_CTL_Q_DESC(*sq, ntc);
	details = ICE_CTL_Q_DETAILS(*sq, ntc);

	while (rd32(hw, cq->sq.head) != ntc) {
		ice_debug(hw, ICE_DBG_AQ_MSG, "ntc %d head %d.\n", ntc, rd32(hw, cq->sq.head));
		memset(desc, 0, sizeof(*desc));
		memset(details, 0, sizeof(*details));
		ntc++;
		if (ntc == sq->count)
			ntc = 0;
		desc = ICE_CTL_Q_DESC(*sq, ntc);
		details = ICE_CTL_Q_DETAILS(*sq, ntc);
	}

	sq->next_to_clean = ntc;

	return ICE_CTL_Q_DESC_UNUSED(sq);
}

 
static void ice_debug_cq(struct ice_hw *hw, void *desc, void *buf, u16 buf_len)
{
	struct ice_aq_desc *cq_desc = desc;
	u16 len;

	if (!IS_ENABLED(CONFIG_DYNAMIC_DEBUG) &&
	    !((ICE_DBG_AQ_DESC | ICE_DBG_AQ_DESC_BUF) & hw->debug_mask))
		return;

	if (!desc)
		return;

	len = le16_to_cpu(cq_desc->datalen);

	ice_debug(hw, ICE_DBG_AQ_DESC, "CQ CMD: opcode 0x%04X, flags 0x%04X, datalen 0x%04X, retval 0x%04X\n",
		  le16_to_cpu(cq_desc->opcode),
		  le16_to_cpu(cq_desc->flags),
		  le16_to_cpu(cq_desc->datalen), le16_to_cpu(cq_desc->retval));
	ice_debug(hw, ICE_DBG_AQ_DESC, "\tcookie (h,l) 0x%08X 0x%08X\n",
		  le32_to_cpu(cq_desc->cookie_high),
		  le32_to_cpu(cq_desc->cookie_low));
	ice_debug(hw, ICE_DBG_AQ_DESC, "\tparam (0,1)  0x%08X 0x%08X\n",
		  le32_to_cpu(cq_desc->params.generic.param0),
		  le32_to_cpu(cq_desc->params.generic.param1));
	ice_debug(hw, ICE_DBG_AQ_DESC, "\taddr (h,l)   0x%08X 0x%08X\n",
		  le32_to_cpu(cq_desc->params.generic.addr_high),
		  le32_to_cpu(cq_desc->params.generic.addr_low));
	if (buf && cq_desc->datalen != 0) {
		ice_debug(hw, ICE_DBG_AQ_DESC_BUF, "Buffer:\n");
		if (buf_len < len)
			len = buf_len;

		ice_debug_array(hw, ICE_DBG_AQ_DESC_BUF, 16, 1, buf, len);
	}
}

 
static bool ice_sq_done(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	 
	return rd32(hw, cq->sq.head) == cq->sq.next_to_use;
}

 
int
ice_sq_send_cmd(struct ice_hw *hw, struct ice_ctl_q_info *cq,
		struct ice_aq_desc *desc, void *buf, u16 buf_size,
		struct ice_sq_cd *cd)
{
	struct ice_dma_mem *dma_buf = NULL;
	struct ice_aq_desc *desc_on_ring;
	bool cmd_completed = false;
	struct ice_sq_cd *details;
	unsigned long timeout;
	int status = 0;
	u16 retval = 0;
	u32 val = 0;

	 
	if (hw->reset_ongoing)
		return -EBUSY;
	mutex_lock(&cq->sq_lock);

	cq->sq_last_status = ICE_AQ_RC_OK;

	if (!cq->sq.count) {
		ice_debug(hw, ICE_DBG_AQ_MSG, "Control Send queue not initialized.\n");
		status = -EIO;
		goto sq_send_command_error;
	}

	if ((buf && !buf_size) || (!buf && buf_size)) {
		status = -EINVAL;
		goto sq_send_command_error;
	}

	if (buf) {
		if (buf_size > cq->sq_buf_size) {
			ice_debug(hw, ICE_DBG_AQ_MSG, "Invalid buffer size for Control Send queue: %d.\n",
				  buf_size);
			status = -EINVAL;
			goto sq_send_command_error;
		}

		desc->flags |= cpu_to_le16(ICE_AQ_FLAG_BUF);
		if (buf_size > ICE_AQ_LG_BUF)
			desc->flags |= cpu_to_le16(ICE_AQ_FLAG_LB);
	}

	val = rd32(hw, cq->sq.head);
	if (val >= cq->num_sq_entries) {
		ice_debug(hw, ICE_DBG_AQ_MSG, "head overrun at %d in the Control Send Queue ring\n",
			  val);
		status = -EIO;
		goto sq_send_command_error;
	}

	details = ICE_CTL_Q_DETAILS(cq->sq, cq->sq.next_to_use);
	if (cd)
		*details = *cd;
	else
		memset(details, 0, sizeof(*details));

	 
	if (ice_clean_sq(hw, cq) == 0) {
		ice_debug(hw, ICE_DBG_AQ_MSG, "Error: Control Send Queue is full.\n");
		status = -ENOSPC;
		goto sq_send_command_error;
	}

	 
	desc_on_ring = ICE_CTL_Q_DESC(cq->sq, cq->sq.next_to_use);

	 
	memcpy(desc_on_ring, desc, sizeof(*desc_on_ring));

	 
	if (buf) {
		dma_buf = &cq->sq.r.sq_bi[cq->sq.next_to_use];
		 
		memcpy(dma_buf->va, buf, buf_size);
		desc_on_ring->datalen = cpu_to_le16(buf_size);

		 
		desc_on_ring->params.generic.addr_high =
			cpu_to_le32(upper_32_bits(dma_buf->pa));
		desc_on_ring->params.generic.addr_low =
			cpu_to_le32(lower_32_bits(dma_buf->pa));
	}

	 
	ice_debug(hw, ICE_DBG_AQ_DESC, "ATQ: Control Send queue desc and buffer:\n");

	ice_debug_cq(hw, (void *)desc_on_ring, buf, buf_size);

	(cq->sq.next_to_use)++;
	if (cq->sq.next_to_use == cq->sq.count)
		cq->sq.next_to_use = 0;
	wr32(hw, cq->sq.tail, cq->sq.next_to_use);
	ice_flush(hw);

	 
	udelay(5);

	timeout = jiffies + ICE_CTL_Q_SQ_CMD_TIMEOUT;
	do {
		if (ice_sq_done(hw, cq))
			break;

		usleep_range(100, 150);
	} while (time_before(jiffies, timeout));

	 
	if (ice_sq_done(hw, cq)) {
		memcpy(desc, desc_on_ring, sizeof(*desc));
		if (buf) {
			 
			u16 copy_size = le16_to_cpu(desc->datalen);

			if (copy_size > buf_size) {
				ice_debug(hw, ICE_DBG_AQ_MSG, "Return len %d > than buf len %d\n",
					  copy_size, buf_size);
				status = -EIO;
			} else {
				memcpy(buf, dma_buf->va, copy_size);
			}
		}
		retval = le16_to_cpu(desc->retval);
		if (retval) {
			ice_debug(hw, ICE_DBG_AQ_MSG, "Control Send Queue command 0x%04X completed with error 0x%X\n",
				  le16_to_cpu(desc->opcode),
				  retval);

			 
			retval &= 0xff;
		}
		cmd_completed = true;
		if (!status && retval != ICE_AQ_RC_OK)
			status = -EIO;
		cq->sq_last_status = (enum ice_aq_err)retval;
	}

	ice_debug(hw, ICE_DBG_AQ_MSG, "ATQ: desc and buffer writeback:\n");

	ice_debug_cq(hw, (void *)desc, buf, buf_size);

	 
	if (details->wb_desc)
		memcpy(details->wb_desc, desc_on_ring,
		       sizeof(*details->wb_desc));

	 
	if (!cmd_completed) {
		if (rd32(hw, cq->rq.len) & cq->rq.len_crit_mask ||
		    rd32(hw, cq->sq.len) & cq->sq.len_crit_mask) {
			ice_debug(hw, ICE_DBG_AQ_MSG, "Critical FW error.\n");
			status = -EIO;
		} else {
			ice_debug(hw, ICE_DBG_AQ_MSG, "Control Send Queue Writeback timeout.\n");
			status = -EIO;
		}
	}

sq_send_command_error:
	mutex_unlock(&cq->sq_lock);
	return status;
}

 
void ice_fill_dflt_direct_cmd_desc(struct ice_aq_desc *desc, u16 opcode)
{
	 
	memset(desc, 0, sizeof(*desc));
	desc->opcode = cpu_to_le16(opcode);
	desc->flags = cpu_to_le16(ICE_AQ_FLAG_SI);
}

 
int
ice_clean_rq_elem(struct ice_hw *hw, struct ice_ctl_q_info *cq,
		  struct ice_rq_event_info *e, u16 *pending)
{
	u16 ntc = cq->rq.next_to_clean;
	enum ice_aq_err rq_last_status;
	struct ice_aq_desc *desc;
	struct ice_dma_mem *bi;
	int ret_code = 0;
	u16 desc_idx;
	u16 datalen;
	u16 flags;
	u16 ntu;

	 
	memset(&e->desc, 0, sizeof(e->desc));

	 
	mutex_lock(&cq->rq_lock);

	if (!cq->rq.count) {
		ice_debug(hw, ICE_DBG_AQ_MSG, "Control Receive queue not initialized.\n");
		ret_code = -EIO;
		goto clean_rq_elem_err;
	}

	 
	ntu = (u16)(rd32(hw, cq->rq.head) & cq->rq.head_mask);

	if (ntu == ntc) {
		 
		ret_code = -EALREADY;
		goto clean_rq_elem_out;
	}

	 
	desc = ICE_CTL_Q_DESC(cq->rq, ntc);
	desc_idx = ntc;

	rq_last_status = (enum ice_aq_err)le16_to_cpu(desc->retval);
	flags = le16_to_cpu(desc->flags);
	if (flags & ICE_AQ_FLAG_ERR) {
		ret_code = -EIO;
		ice_debug(hw, ICE_DBG_AQ_MSG, "Control Receive Queue Event 0x%04X received with error 0x%X\n",
			  le16_to_cpu(desc->opcode), rq_last_status);
	}
	memcpy(&e->desc, desc, sizeof(e->desc));
	datalen = le16_to_cpu(desc->datalen);
	e->msg_len = min_t(u16, datalen, e->buf_len);
	if (e->msg_buf && e->msg_len)
		memcpy(e->msg_buf, cq->rq.r.rq_bi[desc_idx].va, e->msg_len);

	ice_debug(hw, ICE_DBG_AQ_DESC, "ARQ: desc and buffer:\n");

	ice_debug_cq(hw, (void *)desc, e->msg_buf, cq->rq_buf_size);

	 
	bi = &cq->rq.r.rq_bi[ntc];
	memset(desc, 0, sizeof(*desc));

	desc->flags = cpu_to_le16(ICE_AQ_FLAG_BUF);
	if (cq->rq_buf_size > ICE_AQ_LG_BUF)
		desc->flags |= cpu_to_le16(ICE_AQ_FLAG_LB);
	desc->datalen = cpu_to_le16(bi->size);
	desc->params.generic.addr_high = cpu_to_le32(upper_32_bits(bi->pa));
	desc->params.generic.addr_low = cpu_to_le32(lower_32_bits(bi->pa));

	 
	wr32(hw, cq->rq.tail, ntc);
	 
	ntc++;
	if (ntc == cq->num_rq_entries)
		ntc = 0;
	cq->rq.next_to_clean = ntc;
	cq->rq.next_to_use = ntu;

clean_rq_elem_out:
	 
	if (pending) {
		 
		ntu = (u16)(rd32(hw, cq->rq.head) & cq->rq.head_mask);
		*pending = (u16)((ntc > ntu ? cq->rq.count : 0) + (ntu - ntc));
	}
clean_rq_elem_err:
	mutex_unlock(&cq->rq_lock);

	return ret_code;
}
