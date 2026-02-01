
 

#include "i40e_type.h"
#include "i40e_register.h"
#include "i40e_adminq.h"
#include "i40e_prototype.h"

static void i40e_resume_aq(struct i40e_hw *hw);

 
static void i40e_adminq_init_regs(struct i40e_hw *hw)
{
	 
	if (i40e_is_vf(hw)) {
		hw->aq.asq.tail = I40E_VF_ATQT1;
		hw->aq.asq.head = I40E_VF_ATQH1;
		hw->aq.asq.len  = I40E_VF_ATQLEN1;
		hw->aq.asq.bal  = I40E_VF_ATQBAL1;
		hw->aq.asq.bah  = I40E_VF_ATQBAH1;
		hw->aq.arq.tail = I40E_VF_ARQT1;
		hw->aq.arq.head = I40E_VF_ARQH1;
		hw->aq.arq.len  = I40E_VF_ARQLEN1;
		hw->aq.arq.bal  = I40E_VF_ARQBAL1;
		hw->aq.arq.bah  = I40E_VF_ARQBAH1;
	} else {
		hw->aq.asq.tail = I40E_PF_ATQT;
		hw->aq.asq.head = I40E_PF_ATQH;
		hw->aq.asq.len  = I40E_PF_ATQLEN;
		hw->aq.asq.bal  = I40E_PF_ATQBAL;
		hw->aq.asq.bah  = I40E_PF_ATQBAH;
		hw->aq.arq.tail = I40E_PF_ARQT;
		hw->aq.arq.head = I40E_PF_ARQH;
		hw->aq.arq.len  = I40E_PF_ARQLEN;
		hw->aq.arq.bal  = I40E_PF_ARQBAL;
		hw->aq.arq.bah  = I40E_PF_ARQBAH;
	}
}

 
static int i40e_alloc_adminq_asq_ring(struct i40e_hw *hw)
{
	int ret_code;

	ret_code = i40e_allocate_dma_mem(hw, &hw->aq.asq.desc_buf,
					 i40e_mem_atq_ring,
					 (hw->aq.num_asq_entries *
					 sizeof(struct i40e_aq_desc)),
					 I40E_ADMINQ_DESC_ALIGNMENT);
	if (ret_code)
		return ret_code;

	ret_code = i40e_allocate_virt_mem(hw, &hw->aq.asq.cmd_buf,
					  (hw->aq.num_asq_entries *
					  sizeof(struct i40e_asq_cmd_details)));
	if (ret_code) {
		i40e_free_dma_mem(hw, &hw->aq.asq.desc_buf);
		return ret_code;
	}

	return ret_code;
}

 
static int i40e_alloc_adminq_arq_ring(struct i40e_hw *hw)
{
	int ret_code;

	ret_code = i40e_allocate_dma_mem(hw, &hw->aq.arq.desc_buf,
					 i40e_mem_arq_ring,
					 (hw->aq.num_arq_entries *
					 sizeof(struct i40e_aq_desc)),
					 I40E_ADMINQ_DESC_ALIGNMENT);

	return ret_code;
}

 
static void i40e_free_adminq_asq(struct i40e_hw *hw)
{
	i40e_free_dma_mem(hw, &hw->aq.asq.desc_buf);
}

 
static void i40e_free_adminq_arq(struct i40e_hw *hw)
{
	i40e_free_dma_mem(hw, &hw->aq.arq.desc_buf);
}

 
static int i40e_alloc_arq_bufs(struct i40e_hw *hw)
{
	struct i40e_aq_desc *desc;
	struct i40e_dma_mem *bi;
	int ret_code;
	int i;

	 

	 
	ret_code = i40e_allocate_virt_mem(hw, &hw->aq.arq.dma_head,
		(hw->aq.num_arq_entries * sizeof(struct i40e_dma_mem)));
	if (ret_code)
		goto alloc_arq_bufs;
	hw->aq.arq.r.arq_bi = (struct i40e_dma_mem *)hw->aq.arq.dma_head.va;

	 
	for (i = 0; i < hw->aq.num_arq_entries; i++) {
		bi = &hw->aq.arq.r.arq_bi[i];
		ret_code = i40e_allocate_dma_mem(hw, bi,
						 i40e_mem_arq_buf,
						 hw->aq.arq_buf_size,
						 I40E_ADMINQ_DESC_ALIGNMENT);
		if (ret_code)
			goto unwind_alloc_arq_bufs;

		 
		desc = I40E_ADMINQ_DESC(hw->aq.arq, i);

		desc->flags = cpu_to_le16(I40E_AQ_FLAG_BUF);
		if (hw->aq.arq_buf_size > I40E_AQ_LARGE_BUF)
			desc->flags |= cpu_to_le16(I40E_AQ_FLAG_LB);
		desc->opcode = 0;
		 
		desc->datalen = cpu_to_le16((u16)bi->size);
		desc->retval = 0;
		desc->cookie_high = 0;
		desc->cookie_low = 0;
		desc->params.external.addr_high =
			cpu_to_le32(upper_32_bits(bi->pa));
		desc->params.external.addr_low =
			cpu_to_le32(lower_32_bits(bi->pa));
		desc->params.external.param0 = 0;
		desc->params.external.param1 = 0;
	}

alloc_arq_bufs:
	return ret_code;

unwind_alloc_arq_bufs:
	 
	i--;
	for (; i >= 0; i--)
		i40e_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);
	i40e_free_virt_mem(hw, &hw->aq.arq.dma_head);

	return ret_code;
}

 
static int i40e_alloc_asq_bufs(struct i40e_hw *hw)
{
	struct i40e_dma_mem *bi;
	int ret_code;
	int i;

	 
	ret_code = i40e_allocate_virt_mem(hw, &hw->aq.asq.dma_head,
		(hw->aq.num_asq_entries * sizeof(struct i40e_dma_mem)));
	if (ret_code)
		goto alloc_asq_bufs;
	hw->aq.asq.r.asq_bi = (struct i40e_dma_mem *)hw->aq.asq.dma_head.va;

	 
	for (i = 0; i < hw->aq.num_asq_entries; i++) {
		bi = &hw->aq.asq.r.asq_bi[i];
		ret_code = i40e_allocate_dma_mem(hw, bi,
						 i40e_mem_asq_buf,
						 hw->aq.asq_buf_size,
						 I40E_ADMINQ_DESC_ALIGNMENT);
		if (ret_code)
			goto unwind_alloc_asq_bufs;
	}
alloc_asq_bufs:
	return ret_code;

unwind_alloc_asq_bufs:
	 
	i--;
	for (; i >= 0; i--)
		i40e_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);
	i40e_free_virt_mem(hw, &hw->aq.asq.dma_head);

	return ret_code;
}

 
static void i40e_free_arq_bufs(struct i40e_hw *hw)
{
	int i;

	 
	for (i = 0; i < hw->aq.num_arq_entries; i++)
		i40e_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);

	 
	i40e_free_dma_mem(hw, &hw->aq.arq.desc_buf);

	 
	i40e_free_virt_mem(hw, &hw->aq.arq.dma_head);
}

 
static void i40e_free_asq_bufs(struct i40e_hw *hw)
{
	int i;

	 
	for (i = 0; i < hw->aq.num_asq_entries; i++)
		if (hw->aq.asq.r.asq_bi[i].pa)
			i40e_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);

	 
	i40e_free_virt_mem(hw, &hw->aq.asq.cmd_buf);

	 
	i40e_free_dma_mem(hw, &hw->aq.asq.desc_buf);

	 
	i40e_free_virt_mem(hw, &hw->aq.asq.dma_head);
}

 
static int i40e_config_asq_regs(struct i40e_hw *hw)
{
	int ret_code = 0;
	u32 reg = 0;

	 
	wr32(hw, hw->aq.asq.head, 0);
	wr32(hw, hw->aq.asq.tail, 0);

	 
	wr32(hw, hw->aq.asq.len, (hw->aq.num_asq_entries |
				  I40E_PF_ATQLEN_ATQENABLE_MASK));
	wr32(hw, hw->aq.asq.bal, lower_32_bits(hw->aq.asq.desc_buf.pa));
	wr32(hw, hw->aq.asq.bah, upper_32_bits(hw->aq.asq.desc_buf.pa));

	 
	reg = rd32(hw, hw->aq.asq.bal);
	if (reg != lower_32_bits(hw->aq.asq.desc_buf.pa))
		ret_code = -EIO;

	return ret_code;
}

 
static int i40e_config_arq_regs(struct i40e_hw *hw)
{
	int ret_code = 0;
	u32 reg = 0;

	 
	wr32(hw, hw->aq.arq.head, 0);
	wr32(hw, hw->aq.arq.tail, 0);

	 
	wr32(hw, hw->aq.arq.len, (hw->aq.num_arq_entries |
				  I40E_PF_ARQLEN_ARQENABLE_MASK));
	wr32(hw, hw->aq.arq.bal, lower_32_bits(hw->aq.arq.desc_buf.pa));
	wr32(hw, hw->aq.arq.bah, upper_32_bits(hw->aq.arq.desc_buf.pa));

	 
	wr32(hw, hw->aq.arq.tail, hw->aq.num_arq_entries - 1);

	 
	reg = rd32(hw, hw->aq.arq.bal);
	if (reg != lower_32_bits(hw->aq.arq.desc_buf.pa))
		ret_code = -EIO;

	return ret_code;
}

 
static int i40e_init_asq(struct i40e_hw *hw)
{
	int ret_code = 0;

	if (hw->aq.asq.count > 0) {
		 
		ret_code = -EBUSY;
		goto init_adminq_exit;
	}

	 
	if ((hw->aq.num_asq_entries == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		ret_code = -EIO;
		goto init_adminq_exit;
	}

	hw->aq.asq.next_to_use = 0;
	hw->aq.asq.next_to_clean = 0;

	 
	ret_code = i40e_alloc_adminq_asq_ring(hw);
	if (ret_code)
		goto init_adminq_exit;

	 
	ret_code = i40e_alloc_asq_bufs(hw);
	if (ret_code)
		goto init_adminq_free_rings;

	 
	ret_code = i40e_config_asq_regs(hw);
	if (ret_code)
		goto init_adminq_free_rings;

	 
	hw->aq.asq.count = hw->aq.num_asq_entries;
	goto init_adminq_exit;

init_adminq_free_rings:
	i40e_free_adminq_asq(hw);

init_adminq_exit:
	return ret_code;
}

 
static int i40e_init_arq(struct i40e_hw *hw)
{
	int ret_code = 0;

	if (hw->aq.arq.count > 0) {
		 
		ret_code = -EBUSY;
		goto init_adminq_exit;
	}

	 
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0)) {
		ret_code = -EIO;
		goto init_adminq_exit;
	}

	hw->aq.arq.next_to_use = 0;
	hw->aq.arq.next_to_clean = 0;

	 
	ret_code = i40e_alloc_adminq_arq_ring(hw);
	if (ret_code)
		goto init_adminq_exit;

	 
	ret_code = i40e_alloc_arq_bufs(hw);
	if (ret_code)
		goto init_adminq_free_rings;

	 
	ret_code = i40e_config_arq_regs(hw);
	if (ret_code)
		goto init_adminq_free_rings;

	 
	hw->aq.arq.count = hw->aq.num_arq_entries;
	goto init_adminq_exit;

init_adminq_free_rings:
	i40e_free_adminq_arq(hw);

init_adminq_exit:
	return ret_code;
}

 
static int i40e_shutdown_asq(struct i40e_hw *hw)
{
	int ret_code = 0;

	mutex_lock(&hw->aq.asq_mutex);

	if (hw->aq.asq.count == 0) {
		ret_code = -EBUSY;
		goto shutdown_asq_out;
	}

	 
	wr32(hw, hw->aq.asq.head, 0);
	wr32(hw, hw->aq.asq.tail, 0);
	wr32(hw, hw->aq.asq.len, 0);
	wr32(hw, hw->aq.asq.bal, 0);
	wr32(hw, hw->aq.asq.bah, 0);

	hw->aq.asq.count = 0;  

	 
	i40e_free_asq_bufs(hw);

shutdown_asq_out:
	mutex_unlock(&hw->aq.asq_mutex);
	return ret_code;
}

 
static int i40e_shutdown_arq(struct i40e_hw *hw)
{
	int ret_code = 0;

	mutex_lock(&hw->aq.arq_mutex);

	if (hw->aq.arq.count == 0) {
		ret_code = -EBUSY;
		goto shutdown_arq_out;
	}

	 
	wr32(hw, hw->aq.arq.head, 0);
	wr32(hw, hw->aq.arq.tail, 0);
	wr32(hw, hw->aq.arq.len, 0);
	wr32(hw, hw->aq.arq.bal, 0);
	wr32(hw, hw->aq.arq.bah, 0);

	hw->aq.arq.count = 0;  

	 
	i40e_free_arq_bufs(hw);

shutdown_arq_out:
	mutex_unlock(&hw->aq.arq_mutex);
	return ret_code;
}

 
static void i40e_set_hw_flags(struct i40e_hw *hw)
{
	struct i40e_adminq_info *aq = &hw->aq;

	hw->flags = 0;

	switch (hw->mac.type) {
	case I40E_MAC_XL710:
		if (aq->api_maj_ver > 1 ||
		    (aq->api_maj_ver == 1 &&
		     aq->api_min_ver >= I40E_MINOR_VER_GET_LINK_INFO_XL710)) {
			hw->flags |= I40E_HW_FLAG_AQ_PHY_ACCESS_CAPABLE;
			hw->flags |= I40E_HW_FLAG_FW_LLDP_STOPPABLE;
			 
			hw->flags |= I40E_HW_FLAG_802_1AD_CAPABLE;
		}
		break;
	case I40E_MAC_X722:
		hw->flags |= I40E_HW_FLAG_AQ_SRCTL_ACCESS_ENABLE |
			     I40E_HW_FLAG_NVM_READ_REQUIRES_LOCK;

		if (aq->api_maj_ver > 1 ||
		    (aq->api_maj_ver == 1 &&
		     aq->api_min_ver >= I40E_MINOR_VER_FW_LLDP_STOPPABLE_X722))
			hw->flags |= I40E_HW_FLAG_FW_LLDP_STOPPABLE;

		if (aq->api_maj_ver > 1 ||
		    (aq->api_maj_ver == 1 &&
		     aq->api_min_ver >= I40E_MINOR_VER_GET_LINK_INFO_X722))
			hw->flags |= I40E_HW_FLAG_AQ_PHY_ACCESS_CAPABLE;

		if (aq->api_maj_ver > 1 ||
		    (aq->api_maj_ver == 1 &&
		     aq->api_min_ver >= I40E_MINOR_VER_FW_REQUEST_FEC_X722))
			hw->flags |= I40E_HW_FLAG_X722_FEC_REQUEST_CAPABLE;

		fallthrough;
	default:
		break;
	}

	 
	if (aq->api_maj_ver > 1 ||
	    (aq->api_maj_ver == 1 &&
	     aq->api_min_ver >= 5))
		hw->flags |= I40E_HW_FLAG_NVM_READ_REQUIRES_LOCK;

	if (aq->api_maj_ver > 1 ||
	    (aq->api_maj_ver == 1 &&
	     aq->api_min_ver >= 8)) {
		hw->flags |= I40E_HW_FLAG_FW_LLDP_PERSISTENT;
		hw->flags |= I40E_HW_FLAG_DROP_MODE;
	}

	if (aq->api_maj_ver > 1 ||
	    (aq->api_maj_ver == 1 &&
	     aq->api_min_ver >= 9))
		hw->flags |= I40E_HW_FLAG_AQ_PHY_ACCESS_EXTENDED;
}

 
int i40e_init_adminq(struct i40e_hw *hw)
{
	u16 cfg_ptr, oem_hi, oem_lo;
	u16 eetrack_lo, eetrack_hi;
	int retry = 0;
	int ret_code;

	 
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.num_asq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		ret_code = -EIO;
		goto init_adminq_exit;
	}

	 
	i40e_adminq_init_regs(hw);

	 
	hw->aq.asq_cmd_timeout = I40E_ASQ_CMD_TIMEOUT;

	 
	ret_code = i40e_init_asq(hw);
	if (ret_code)
		goto init_adminq_destroy_locks;

	 
	ret_code = i40e_init_arq(hw);
	if (ret_code)
		goto init_adminq_free_asq;

	 
	do {
		ret_code = i40e_aq_get_firmware_version(hw,
							&hw->aq.fw_maj_ver,
							&hw->aq.fw_min_ver,
							&hw->aq.fw_build,
							&hw->aq.api_maj_ver,
							&hw->aq.api_min_ver,
							NULL);
		if (ret_code != -EIO)
			break;
		retry++;
		msleep(100);
		i40e_resume_aq(hw);
	} while (retry < 10);
	if (ret_code != 0)
		goto init_adminq_free_arq;

	 
	i40e_set_hw_flags(hw);

	 
	i40e_read_nvm_word(hw, I40E_SR_NVM_DEV_STARTER_VERSION,
			   &hw->nvm.version);
	i40e_read_nvm_word(hw, I40E_SR_NVM_EETRACK_LO, &eetrack_lo);
	i40e_read_nvm_word(hw, I40E_SR_NVM_EETRACK_HI, &eetrack_hi);
	hw->nvm.eetrack = (eetrack_hi << 16) | eetrack_lo;
	i40e_read_nvm_word(hw, I40E_SR_BOOT_CONFIG_PTR, &cfg_ptr);
	i40e_read_nvm_word(hw, (cfg_ptr + I40E_NVM_OEM_VER_OFF),
			   &oem_hi);
	i40e_read_nvm_word(hw, (cfg_ptr + (I40E_NVM_OEM_VER_OFF + 1)),
			   &oem_lo);
	hw->nvm.oem_ver = ((u32)oem_hi << 16) | oem_lo;

	if (hw->mac.type == I40E_MAC_XL710 &&
	    hw->aq.api_maj_ver == I40E_FW_API_VERSION_MAJOR &&
	    hw->aq.api_min_ver >= I40E_MINOR_VER_GET_LINK_INFO_XL710) {
		hw->flags |= I40E_HW_FLAG_AQ_PHY_ACCESS_CAPABLE;
		hw->flags |= I40E_HW_FLAG_FW_LLDP_STOPPABLE;
	}
	if (hw->mac.type == I40E_MAC_X722 &&
	    hw->aq.api_maj_ver == I40E_FW_API_VERSION_MAJOR &&
	    hw->aq.api_min_ver >= I40E_MINOR_VER_FW_LLDP_STOPPABLE_X722) {
		hw->flags |= I40E_HW_FLAG_FW_LLDP_STOPPABLE;
	}

	 
	if (hw->aq.api_maj_ver > 1 ||
	    (hw->aq.api_maj_ver == 1 &&
	     hw->aq.api_min_ver >= 7))
		hw->flags |= I40E_HW_FLAG_802_1AD_CAPABLE;

	if (hw->aq.api_maj_ver > I40E_FW_API_VERSION_MAJOR) {
		ret_code = -EIO;
		goto init_adminq_free_arq;
	}

	 
	i40e_aq_release_resource(hw, I40E_NVM_RESOURCE_ID, 0, NULL);
	hw->nvm_release_on_done = false;
	hw->nvmupd_state = I40E_NVMUPD_STATE_INIT;

	ret_code = 0;

	 
	goto init_adminq_exit;

init_adminq_free_arq:
	i40e_shutdown_arq(hw);
init_adminq_free_asq:
	i40e_shutdown_asq(hw);
init_adminq_destroy_locks:

init_adminq_exit:
	return ret_code;
}

 
void i40e_shutdown_adminq(struct i40e_hw *hw)
{
	if (i40e_check_asq_alive(hw))
		i40e_aq_queue_shutdown(hw, true);

	i40e_shutdown_asq(hw);
	i40e_shutdown_arq(hw);

	if (hw->nvm_buff.va)
		i40e_free_virt_mem(hw, &hw->nvm_buff);
}

 
static u16 i40e_clean_asq(struct i40e_hw *hw)
{
	struct i40e_adminq_ring *asq = &(hw->aq.asq);
	struct i40e_asq_cmd_details *details;
	u16 ntc = asq->next_to_clean;
	struct i40e_aq_desc desc_cb;
	struct i40e_aq_desc *desc;

	desc = I40E_ADMINQ_DESC(*asq, ntc);
	details = I40E_ADMINQ_DETAILS(*asq, ntc);
	while (rd32(hw, hw->aq.asq.head) != ntc) {
		i40e_debug(hw, I40E_DEBUG_AQ_COMMAND,
			   "ntc %d head %d.\n", ntc, rd32(hw, hw->aq.asq.head));

		if (details->callback) {
			I40E_ADMINQ_CALLBACK cb_func =
					(I40E_ADMINQ_CALLBACK)details->callback;
			desc_cb = *desc;
			cb_func(hw, &desc_cb);
		}
		memset(desc, 0, sizeof(*desc));
		memset(details, 0, sizeof(*details));
		ntc++;
		if (ntc == asq->count)
			ntc = 0;
		desc = I40E_ADMINQ_DESC(*asq, ntc);
		details = I40E_ADMINQ_DETAILS(*asq, ntc);
	}

	asq->next_to_clean = ntc;

	return I40E_DESC_UNUSED(asq);
}

 
static bool i40e_asq_done(struct i40e_hw *hw)
{
	 
	return rd32(hw, hw->aq.asq.head) == hw->aq.asq.next_to_use;

}

 
static int
i40e_asq_send_command_atomic_exec(struct i40e_hw *hw,
				  struct i40e_aq_desc *desc,
				  void *buff,  
				  u16  buff_size,
				  struct i40e_asq_cmd_details *cmd_details,
				  bool is_atomic_context)
{
	struct i40e_dma_mem *dma_buff = NULL;
	struct i40e_asq_cmd_details *details;
	struct i40e_aq_desc *desc_on_ring;
	bool cmd_completed = false;
	u16  retval = 0;
	int status = 0;
	u32  val = 0;

	if (hw->aq.asq.count == 0) {
		i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: Admin queue not initialized.\n");
		status = -EIO;
		goto asq_send_command_error;
	}

	hw->aq.asq_last_status = I40E_AQ_RC_OK;

	val = rd32(hw, hw->aq.asq.head);
	if (val >= hw->aq.num_asq_entries) {
		i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: head overrun at %d\n", val);
		status = -ENOSPC;
		goto asq_send_command_error;
	}

	details = I40E_ADMINQ_DETAILS(hw->aq.asq, hw->aq.asq.next_to_use);
	if (cmd_details) {
		*details = *cmd_details;

		 
		if (details->cookie) {
			desc->cookie_high =
				cpu_to_le32(upper_32_bits(details->cookie));
			desc->cookie_low =
				cpu_to_le32(lower_32_bits(details->cookie));
		}
	} else {
		memset(details, 0, sizeof(struct i40e_asq_cmd_details));
	}

	 
	desc->flags &= ~cpu_to_le16(details->flags_dis);
	desc->flags |= cpu_to_le16(details->flags_ena);

	if (buff_size > hw->aq.asq_buf_size) {
		i40e_debug(hw,
			   I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: Invalid buffer size: %d.\n",
			   buff_size);
		status = -EINVAL;
		goto asq_send_command_error;
	}

	if (details->postpone && !details->async) {
		i40e_debug(hw,
			   I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: Async flag not set along with postpone flag");
		status = -EINVAL;
		goto asq_send_command_error;
	}

	 
	 
	if (i40e_clean_asq(hw) == 0) {
		i40e_debug(hw,
			   I40E_DEBUG_AQ_MESSAGE,
			   "AQTX: Error queue is full.\n");
		status = -ENOSPC;
		goto asq_send_command_error;
	}

	 
	desc_on_ring = I40E_ADMINQ_DESC(hw->aq.asq, hw->aq.asq.next_to_use);

	 
	*desc_on_ring = *desc;

	 
	if (buff != NULL) {
		dma_buff = &(hw->aq.asq.r.asq_bi[hw->aq.asq.next_to_use]);
		 
		memcpy(dma_buff->va, buff, buff_size);
		desc_on_ring->datalen = cpu_to_le16(buff_size);

		 
		desc_on_ring->params.external.addr_high =
				cpu_to_le32(upper_32_bits(dma_buff->pa));
		desc_on_ring->params.external.addr_low =
				cpu_to_le32(lower_32_bits(dma_buff->pa));
	}

	 
	i40e_debug(hw, I40E_DEBUG_AQ_COMMAND, "AQTX: desc and buffer:\n");
	i40e_debug_aq(hw, I40E_DEBUG_AQ_COMMAND, (void *)desc_on_ring,
		      buff, buff_size);
	(hw->aq.asq.next_to_use)++;
	if (hw->aq.asq.next_to_use == hw->aq.asq.count)
		hw->aq.asq.next_to_use = 0;
	if (!details->postpone)
		wr32(hw, hw->aq.asq.tail, hw->aq.asq.next_to_use);

	 
	if (!details->async && !details->postpone) {
		u32 total_delay = 0;

		do {
			 
			if (i40e_asq_done(hw))
				break;

			if (is_atomic_context)
				udelay(50);
			else
				usleep_range(40, 60);

			total_delay += 50;
		} while (total_delay < hw->aq.asq_cmd_timeout);
	}

	 
	if (i40e_asq_done(hw)) {
		*desc = *desc_on_ring;
		if (buff != NULL)
			memcpy(buff, dma_buff->va, buff_size);
		retval = le16_to_cpu(desc->retval);
		if (retval != 0) {
			i40e_debug(hw,
				   I40E_DEBUG_AQ_MESSAGE,
				   "AQTX: Command completed with error 0x%X.\n",
				   retval);

			 
			retval &= 0xff;
		}
		cmd_completed = true;
		if ((enum i40e_admin_queue_err)retval == I40E_AQ_RC_OK)
			status = 0;
		else if ((enum i40e_admin_queue_err)retval == I40E_AQ_RC_EBUSY)
			status = -EBUSY;
		else
			status = -EIO;
		hw->aq.asq_last_status = (enum i40e_admin_queue_err)retval;
	}

	i40e_debug(hw, I40E_DEBUG_AQ_COMMAND,
		   "AQTX: desc and buffer writeback:\n");
	i40e_debug_aq(hw, I40E_DEBUG_AQ_COMMAND, (void *)desc, buff, buff_size);

	 
	if (details->wb_desc)
		*details->wb_desc = *desc_on_ring;

	 
	if ((!cmd_completed) &&
	    (!details->async && !details->postpone)) {
		if (rd32(hw, hw->aq.asq.len) & I40E_GL_ATQLEN_ATQCRIT_MASK) {
			i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
				   "AQTX: AQ Critical error.\n");
			status = -EIO;
		} else {
			i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
				   "AQTX: Writeback timeout.\n");
			status = -EIO;
		}
	}

asq_send_command_error:
	return status;
}

 
int
i40e_asq_send_command_atomic(struct i40e_hw *hw,
			     struct i40e_aq_desc *desc,
			     void *buff,  
			     u16  buff_size,
			     struct i40e_asq_cmd_details *cmd_details,
			     bool is_atomic_context)
{
	int status;

	mutex_lock(&hw->aq.asq_mutex);
	status = i40e_asq_send_command_atomic_exec(hw, desc, buff, buff_size,
						   cmd_details,
						   is_atomic_context);

	mutex_unlock(&hw->aq.asq_mutex);
	return status;
}

int
i40e_asq_send_command(struct i40e_hw *hw, struct i40e_aq_desc *desc,
		      void *buff,   u16  buff_size,
		      struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_asq_send_command_atomic(hw, desc, buff, buff_size,
					    cmd_details, false);
}

 
int
i40e_asq_send_command_atomic_v2(struct i40e_hw *hw,
				struct i40e_aq_desc *desc,
				void *buff,  
				u16  buff_size,
				struct i40e_asq_cmd_details *cmd_details,
				bool is_atomic_context,
				enum i40e_admin_queue_err *aq_status)
{
	int status;

	mutex_lock(&hw->aq.asq_mutex);
	status = i40e_asq_send_command_atomic_exec(hw, desc, buff,
						   buff_size,
						   cmd_details,
						   is_atomic_context);
	if (aq_status)
		*aq_status = hw->aq.asq_last_status;
	mutex_unlock(&hw->aq.asq_mutex);
	return status;
}

int
i40e_asq_send_command_v2(struct i40e_hw *hw, struct i40e_aq_desc *desc,
			 void *buff,   u16  buff_size,
			 struct i40e_asq_cmd_details *cmd_details,
			 enum i40e_admin_queue_err *aq_status)
{
	return i40e_asq_send_command_atomic_v2(hw, desc, buff, buff_size,
					       cmd_details, true, aq_status);
}

 
void i40e_fill_default_direct_cmd_desc(struct i40e_aq_desc *desc,
				       u16 opcode)
{
	 
	memset((void *)desc, 0, sizeof(struct i40e_aq_desc));
	desc->opcode = cpu_to_le16(opcode);
	desc->flags = cpu_to_le16(I40E_AQ_FLAG_SI);
}

 
int i40e_clean_arq_element(struct i40e_hw *hw,
			   struct i40e_arq_event_info *e,
			   u16 *pending)
{
	u16 ntc = hw->aq.arq.next_to_clean;
	struct i40e_aq_desc *desc;
	struct i40e_dma_mem *bi;
	int ret_code = 0;
	u16 desc_idx;
	u16 datalen;
	u16 flags;
	u16 ntu;

	 
	memset(&e->desc, 0, sizeof(e->desc));

	 
	mutex_lock(&hw->aq.arq_mutex);

	if (hw->aq.arq.count == 0) {
		i40e_debug(hw, I40E_DEBUG_AQ_MESSAGE,
			   "AQRX: Admin queue not initialized.\n");
		ret_code = -EIO;
		goto clean_arq_element_err;
	}

	 
	ntu = rd32(hw, hw->aq.arq.head) & I40E_PF_ARQH_ARQH_MASK;
	if (ntu == ntc) {
		 
		ret_code = -EALREADY;
		goto clean_arq_element_out;
	}

	 
	desc = I40E_ADMINQ_DESC(hw->aq.arq, ntc);
	desc_idx = ntc;

	hw->aq.arq_last_status =
		(enum i40e_admin_queue_err)le16_to_cpu(desc->retval);
	flags = le16_to_cpu(desc->flags);
	if (flags & I40E_AQ_FLAG_ERR) {
		ret_code = -EIO;
		i40e_debug(hw,
			   I40E_DEBUG_AQ_MESSAGE,
			   "AQRX: Event received with error 0x%X.\n",
			   hw->aq.arq_last_status);
	}

	e->desc = *desc;
	datalen = le16_to_cpu(desc->datalen);
	e->msg_len = min(datalen, e->buf_len);
	if (e->msg_buf != NULL && (e->msg_len != 0))
		memcpy(e->msg_buf, hw->aq.arq.r.arq_bi[desc_idx].va,
		       e->msg_len);

	i40e_debug(hw, I40E_DEBUG_AQ_COMMAND, "AQRX: desc and buffer:\n");
	i40e_debug_aq(hw, I40E_DEBUG_AQ_COMMAND, (void *)desc, e->msg_buf,
		      hw->aq.arq_buf_size);

	 
	bi = &hw->aq.arq.r.arq_bi[ntc];
	memset((void *)desc, 0, sizeof(struct i40e_aq_desc));

	desc->flags = cpu_to_le16(I40E_AQ_FLAG_BUF);
	if (hw->aq.arq_buf_size > I40E_AQ_LARGE_BUF)
		desc->flags |= cpu_to_le16(I40E_AQ_FLAG_LB);
	desc->datalen = cpu_to_le16((u16)bi->size);
	desc->params.external.addr_high = cpu_to_le32(upper_32_bits(bi->pa));
	desc->params.external.addr_low = cpu_to_le32(lower_32_bits(bi->pa));

	 
	wr32(hw, hw->aq.arq.tail, ntc);
	 
	ntc++;
	if (ntc == hw->aq.num_arq_entries)
		ntc = 0;
	hw->aq.arq.next_to_clean = ntc;
	hw->aq.arq.next_to_use = ntu;

	i40e_nvmupd_check_wait_event(hw, le16_to_cpu(e->desc.opcode), &e->desc);
clean_arq_element_out:
	 
	if (pending)
		*pending = (ntc > ntu ? hw->aq.arq.count : 0) + (ntu - ntc);
clean_arq_element_err:
	mutex_unlock(&hw->aq.arq_mutex);

	return ret_code;
}

static void i40e_resume_aq(struct i40e_hw *hw)
{
	 
	hw->aq.asq.next_to_use = 0;
	hw->aq.asq.next_to_clean = 0;

	i40e_config_asq_regs(hw);

	hw->aq.arq.next_to_use = 0;
	hw->aq.arq.next_to_clean = 0;

	i40e_config_arq_regs(hw);
}
