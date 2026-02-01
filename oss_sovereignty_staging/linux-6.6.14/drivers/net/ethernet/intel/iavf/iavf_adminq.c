
 

#include "iavf_status.h"
#include "iavf_type.h"
#include "iavf_register.h"
#include "iavf_adminq.h"
#include "iavf_prototype.h"

 
static void iavf_adminq_init_regs(struct iavf_hw *hw)
{
	 
	hw->aq.asq.tail = IAVF_VF_ATQT1;
	hw->aq.asq.head = IAVF_VF_ATQH1;
	hw->aq.asq.len  = IAVF_VF_ATQLEN1;
	hw->aq.asq.bal  = IAVF_VF_ATQBAL1;
	hw->aq.asq.bah  = IAVF_VF_ATQBAH1;
	hw->aq.arq.tail = IAVF_VF_ARQT1;
	hw->aq.arq.head = IAVF_VF_ARQH1;
	hw->aq.arq.len  = IAVF_VF_ARQLEN1;
	hw->aq.arq.bal  = IAVF_VF_ARQBAL1;
	hw->aq.arq.bah  = IAVF_VF_ARQBAH1;
}

 
static enum iavf_status iavf_alloc_adminq_asq_ring(struct iavf_hw *hw)
{
	enum iavf_status ret_code;

	ret_code = iavf_allocate_dma_mem(hw, &hw->aq.asq.desc_buf,
					 iavf_mem_atq_ring,
					 (hw->aq.num_asq_entries *
					 sizeof(struct iavf_aq_desc)),
					 IAVF_ADMINQ_DESC_ALIGNMENT);
	if (ret_code)
		return ret_code;

	ret_code = iavf_allocate_virt_mem(hw, &hw->aq.asq.cmd_buf,
					  (hw->aq.num_asq_entries *
					  sizeof(struct iavf_asq_cmd_details)));
	if (ret_code) {
		iavf_free_dma_mem(hw, &hw->aq.asq.desc_buf);
		return ret_code;
	}

	return ret_code;
}

 
static enum iavf_status iavf_alloc_adminq_arq_ring(struct iavf_hw *hw)
{
	enum iavf_status ret_code;

	ret_code = iavf_allocate_dma_mem(hw, &hw->aq.arq.desc_buf,
					 iavf_mem_arq_ring,
					 (hw->aq.num_arq_entries *
					 sizeof(struct iavf_aq_desc)),
					 IAVF_ADMINQ_DESC_ALIGNMENT);

	return ret_code;
}

 
static void iavf_free_adminq_asq(struct iavf_hw *hw)
{
	iavf_free_dma_mem(hw, &hw->aq.asq.desc_buf);
}

 
static void iavf_free_adminq_arq(struct iavf_hw *hw)
{
	iavf_free_dma_mem(hw, &hw->aq.arq.desc_buf);
}

 
static enum iavf_status iavf_alloc_arq_bufs(struct iavf_hw *hw)
{
	struct iavf_aq_desc *desc;
	struct iavf_dma_mem *bi;
	enum iavf_status ret_code;
	int i;

	 

	 
	ret_code = iavf_allocate_virt_mem(hw, &hw->aq.arq.dma_head,
					  (hw->aq.num_arq_entries *
					   sizeof(struct iavf_dma_mem)));
	if (ret_code)
		goto alloc_arq_bufs;
	hw->aq.arq.r.arq_bi = (struct iavf_dma_mem *)hw->aq.arq.dma_head.va;

	 
	for (i = 0; i < hw->aq.num_arq_entries; i++) {
		bi = &hw->aq.arq.r.arq_bi[i];
		ret_code = iavf_allocate_dma_mem(hw, bi,
						 iavf_mem_arq_buf,
						 hw->aq.arq_buf_size,
						 IAVF_ADMINQ_DESC_ALIGNMENT);
		if (ret_code)
			goto unwind_alloc_arq_bufs;

		 
		desc = IAVF_ADMINQ_DESC(hw->aq.arq, i);

		desc->flags = cpu_to_le16(IAVF_AQ_FLAG_BUF);
		if (hw->aq.arq_buf_size > IAVF_AQ_LARGE_BUF)
			desc->flags |= cpu_to_le16(IAVF_AQ_FLAG_LB);
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
		iavf_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);
	iavf_free_virt_mem(hw, &hw->aq.arq.dma_head);

	return ret_code;
}

 
static enum iavf_status iavf_alloc_asq_bufs(struct iavf_hw *hw)
{
	struct iavf_dma_mem *bi;
	enum iavf_status ret_code;
	int i;

	 
	ret_code = iavf_allocate_virt_mem(hw, &hw->aq.asq.dma_head,
					  (hw->aq.num_asq_entries *
					   sizeof(struct iavf_dma_mem)));
	if (ret_code)
		goto alloc_asq_bufs;
	hw->aq.asq.r.asq_bi = (struct iavf_dma_mem *)hw->aq.asq.dma_head.va;

	 
	for (i = 0; i < hw->aq.num_asq_entries; i++) {
		bi = &hw->aq.asq.r.asq_bi[i];
		ret_code = iavf_allocate_dma_mem(hw, bi,
						 iavf_mem_asq_buf,
						 hw->aq.asq_buf_size,
						 IAVF_ADMINQ_DESC_ALIGNMENT);
		if (ret_code)
			goto unwind_alloc_asq_bufs;
	}
alloc_asq_bufs:
	return ret_code;

unwind_alloc_asq_bufs:
	 
	i--;
	for (; i >= 0; i--)
		iavf_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);
	iavf_free_virt_mem(hw, &hw->aq.asq.dma_head);

	return ret_code;
}

 
static void iavf_free_arq_bufs(struct iavf_hw *hw)
{
	int i;

	 
	for (i = 0; i < hw->aq.num_arq_entries; i++)
		iavf_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);

	 
	iavf_free_dma_mem(hw, &hw->aq.arq.desc_buf);

	 
	iavf_free_virt_mem(hw, &hw->aq.arq.dma_head);
}

 
static void iavf_free_asq_bufs(struct iavf_hw *hw)
{
	int i;

	 
	for (i = 0; i < hw->aq.num_asq_entries; i++)
		if (hw->aq.asq.r.asq_bi[i].pa)
			iavf_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);

	 
	iavf_free_virt_mem(hw, &hw->aq.asq.cmd_buf);

	 
	iavf_free_dma_mem(hw, &hw->aq.asq.desc_buf);

	 
	iavf_free_virt_mem(hw, &hw->aq.asq.dma_head);
}

 
static enum iavf_status iavf_config_asq_regs(struct iavf_hw *hw)
{
	enum iavf_status ret_code = 0;
	u32 reg = 0;

	 
	wr32(hw, hw->aq.asq.head, 0);
	wr32(hw, hw->aq.asq.tail, 0);

	 
	wr32(hw, hw->aq.asq.len, (hw->aq.num_asq_entries |
				  IAVF_VF_ATQLEN1_ATQENABLE_MASK));
	wr32(hw, hw->aq.asq.bal, lower_32_bits(hw->aq.asq.desc_buf.pa));
	wr32(hw, hw->aq.asq.bah, upper_32_bits(hw->aq.asq.desc_buf.pa));

	 
	reg = rd32(hw, hw->aq.asq.bal);
	if (reg != lower_32_bits(hw->aq.asq.desc_buf.pa))
		ret_code = IAVF_ERR_ADMIN_QUEUE_ERROR;

	return ret_code;
}

 
static enum iavf_status iavf_config_arq_regs(struct iavf_hw *hw)
{
	enum iavf_status ret_code = 0;
	u32 reg = 0;

	 
	wr32(hw, hw->aq.arq.head, 0);
	wr32(hw, hw->aq.arq.tail, 0);

	 
	wr32(hw, hw->aq.arq.len, (hw->aq.num_arq_entries |
				  IAVF_VF_ARQLEN1_ARQENABLE_MASK));
	wr32(hw, hw->aq.arq.bal, lower_32_bits(hw->aq.arq.desc_buf.pa));
	wr32(hw, hw->aq.arq.bah, upper_32_bits(hw->aq.arq.desc_buf.pa));

	 
	wr32(hw, hw->aq.arq.tail, hw->aq.num_arq_entries - 1);

	 
	reg = rd32(hw, hw->aq.arq.bal);
	if (reg != lower_32_bits(hw->aq.arq.desc_buf.pa))
		ret_code = IAVF_ERR_ADMIN_QUEUE_ERROR;

	return ret_code;
}

 
static enum iavf_status iavf_init_asq(struct iavf_hw *hw)
{
	enum iavf_status ret_code = 0;
	int i;

	if (hw->aq.asq.count > 0) {
		 
		ret_code = IAVF_ERR_NOT_READY;
		goto init_adminq_exit;
	}

	 
	if ((hw->aq.num_asq_entries == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		ret_code = IAVF_ERR_CONFIG;
		goto init_adminq_exit;
	}

	hw->aq.asq.next_to_use = 0;
	hw->aq.asq.next_to_clean = 0;

	 
	ret_code = iavf_alloc_adminq_asq_ring(hw);
	if (ret_code)
		goto init_adminq_exit;

	 
	ret_code = iavf_alloc_asq_bufs(hw);
	if (ret_code)
		goto init_adminq_free_rings;

	 
	ret_code = iavf_config_asq_regs(hw);
	if (ret_code)
		goto init_free_asq_bufs;

	 
	hw->aq.asq.count = hw->aq.num_asq_entries;
	goto init_adminq_exit;

init_free_asq_bufs:
	for (i = 0; i < hw->aq.num_asq_entries; i++)
		iavf_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);
	iavf_free_virt_mem(hw, &hw->aq.asq.dma_head);

init_adminq_free_rings:
	iavf_free_adminq_asq(hw);

init_adminq_exit:
	return ret_code;
}

 
static enum iavf_status iavf_init_arq(struct iavf_hw *hw)
{
	enum iavf_status ret_code = 0;
	int i;

	if (hw->aq.arq.count > 0) {
		 
		ret_code = IAVF_ERR_NOT_READY;
		goto init_adminq_exit;
	}

	 
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0)) {
		ret_code = IAVF_ERR_CONFIG;
		goto init_adminq_exit;
	}

	hw->aq.arq.next_to_use = 0;
	hw->aq.arq.next_to_clean = 0;

	 
	ret_code = iavf_alloc_adminq_arq_ring(hw);
	if (ret_code)
		goto init_adminq_exit;

	 
	ret_code = iavf_alloc_arq_bufs(hw);
	if (ret_code)
		goto init_adminq_free_rings;

	 
	ret_code = iavf_config_arq_regs(hw);
	if (ret_code)
		goto init_free_arq_bufs;

	 
	hw->aq.arq.count = hw->aq.num_arq_entries;
	goto init_adminq_exit;

init_free_arq_bufs:
	for (i = 0; i < hw->aq.num_arq_entries; i++)
		iavf_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);
	iavf_free_virt_mem(hw, &hw->aq.arq.dma_head);
init_adminq_free_rings:
	iavf_free_adminq_arq(hw);

init_adminq_exit:
	return ret_code;
}

 
static enum iavf_status iavf_shutdown_asq(struct iavf_hw *hw)
{
	enum iavf_status ret_code = 0;

	mutex_lock(&hw->aq.asq_mutex);

	if (hw->aq.asq.count == 0) {
		ret_code = IAVF_ERR_NOT_READY;
		goto shutdown_asq_out;
	}

	 
	wr32(hw, hw->aq.asq.head, 0);
	wr32(hw, hw->aq.asq.tail, 0);
	wr32(hw, hw->aq.asq.len, 0);
	wr32(hw, hw->aq.asq.bal, 0);
	wr32(hw, hw->aq.asq.bah, 0);

	hw->aq.asq.count = 0;  

	 
	iavf_free_asq_bufs(hw);

shutdown_asq_out:
	mutex_unlock(&hw->aq.asq_mutex);
	return ret_code;
}

 
static enum iavf_status iavf_shutdown_arq(struct iavf_hw *hw)
{
	enum iavf_status ret_code = 0;

	mutex_lock(&hw->aq.arq_mutex);

	if (hw->aq.arq.count == 0) {
		ret_code = IAVF_ERR_NOT_READY;
		goto shutdown_arq_out;
	}

	 
	wr32(hw, hw->aq.arq.head, 0);
	wr32(hw, hw->aq.arq.tail, 0);
	wr32(hw, hw->aq.arq.len, 0);
	wr32(hw, hw->aq.arq.bal, 0);
	wr32(hw, hw->aq.arq.bah, 0);

	hw->aq.arq.count = 0;  

	 
	iavf_free_arq_bufs(hw);

shutdown_arq_out:
	mutex_unlock(&hw->aq.arq_mutex);
	return ret_code;
}

 
enum iavf_status iavf_init_adminq(struct iavf_hw *hw)
{
	enum iavf_status ret_code;

	 
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.num_asq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		ret_code = IAVF_ERR_CONFIG;
		goto init_adminq_exit;
	}

	 
	iavf_adminq_init_regs(hw);

	 
	hw->aq.asq_cmd_timeout = IAVF_ASQ_CMD_TIMEOUT;

	 
	ret_code = iavf_init_asq(hw);
	if (ret_code)
		goto init_adminq_destroy_locks;

	 
	ret_code = iavf_init_arq(hw);
	if (ret_code)
		goto init_adminq_free_asq;

	 
	goto init_adminq_exit;

init_adminq_free_asq:
	iavf_shutdown_asq(hw);
init_adminq_destroy_locks:

init_adminq_exit:
	return ret_code;
}

 
enum iavf_status iavf_shutdown_adminq(struct iavf_hw *hw)
{
	if (iavf_check_asq_alive(hw))
		iavf_aq_queue_shutdown(hw, true);

	iavf_shutdown_asq(hw);
	iavf_shutdown_arq(hw);

	return 0;
}

 
static u16 iavf_clean_asq(struct iavf_hw *hw)
{
	struct iavf_adminq_ring *asq = &hw->aq.asq;
	struct iavf_asq_cmd_details *details;
	u16 ntc = asq->next_to_clean;
	struct iavf_aq_desc desc_cb;
	struct iavf_aq_desc *desc;

	desc = IAVF_ADMINQ_DESC(*asq, ntc);
	details = IAVF_ADMINQ_DETAILS(*asq, ntc);
	while (rd32(hw, hw->aq.asq.head) != ntc) {
		iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE,
			   "ntc %d head %d.\n", ntc, rd32(hw, hw->aq.asq.head));

		if (details->callback) {
			IAVF_ADMINQ_CALLBACK cb_func =
					(IAVF_ADMINQ_CALLBACK)details->callback;
			desc_cb = *desc;
			cb_func(hw, &desc_cb);
		}
		memset((void *)desc, 0, sizeof(struct iavf_aq_desc));
		memset((void *)details, 0,
		       sizeof(struct iavf_asq_cmd_details));
		ntc++;
		if (ntc == asq->count)
			ntc = 0;
		desc = IAVF_ADMINQ_DESC(*asq, ntc);
		details = IAVF_ADMINQ_DETAILS(*asq, ntc);
	}

	asq->next_to_clean = ntc;

	return IAVF_DESC_UNUSED(asq);
}

 
bool iavf_asq_done(struct iavf_hw *hw)
{
	 
	return rd32(hw, hw->aq.asq.head) == hw->aq.asq.next_to_use;
}

 
enum iavf_status iavf_asq_send_command(struct iavf_hw *hw,
				       struct iavf_aq_desc *desc,
				       void *buff,  
				       u16  buff_size,
				       struct iavf_asq_cmd_details *cmd_details)
{
	struct iavf_dma_mem *dma_buff = NULL;
	struct iavf_asq_cmd_details *details;
	struct iavf_aq_desc *desc_on_ring;
	bool cmd_completed = false;
	enum iavf_status status = 0;
	u16  retval = 0;
	u32  val = 0;

	mutex_lock(&hw->aq.asq_mutex);

	if (hw->aq.asq.count == 0) {
		iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE,
			   "AQTX: Admin queue not initialized.\n");
		status = IAVF_ERR_QUEUE_EMPTY;
		goto asq_send_command_error;
	}

	hw->aq.asq_last_status = IAVF_AQ_RC_OK;

	val = rd32(hw, hw->aq.asq.head);
	if (val >= hw->aq.num_asq_entries) {
		iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE,
			   "AQTX: head overrun at %d\n", val);
		status = IAVF_ERR_QUEUE_EMPTY;
		goto asq_send_command_error;
	}

	details = IAVF_ADMINQ_DETAILS(hw->aq.asq, hw->aq.asq.next_to_use);
	if (cmd_details) {
		*details = *cmd_details;

		 
		if (details->cookie) {
			desc->cookie_high =
				cpu_to_le32(upper_32_bits(details->cookie));
			desc->cookie_low =
				cpu_to_le32(lower_32_bits(details->cookie));
		}
	} else {
		memset(details, 0, sizeof(struct iavf_asq_cmd_details));
	}

	 
	desc->flags &= ~cpu_to_le16(details->flags_dis);
	desc->flags |= cpu_to_le16(details->flags_ena);

	if (buff_size > hw->aq.asq_buf_size) {
		iavf_debug(hw,
			   IAVF_DEBUG_AQ_MESSAGE,
			   "AQTX: Invalid buffer size: %d.\n",
			   buff_size);
		status = IAVF_ERR_INVALID_SIZE;
		goto asq_send_command_error;
	}

	if (details->postpone && !details->async) {
		iavf_debug(hw,
			   IAVF_DEBUG_AQ_MESSAGE,
			   "AQTX: Async flag not set along with postpone flag");
		status = IAVF_ERR_PARAM;
		goto asq_send_command_error;
	}

	 
	 
	if (iavf_clean_asq(hw) == 0) {
		iavf_debug(hw,
			   IAVF_DEBUG_AQ_MESSAGE,
			   "AQTX: Error queue is full.\n");
		status = IAVF_ERR_ADMIN_QUEUE_FULL;
		goto asq_send_command_error;
	}

	 
	desc_on_ring = IAVF_ADMINQ_DESC(hw->aq.asq, hw->aq.asq.next_to_use);

	 
	*desc_on_ring = *desc;

	 
	if (buff) {
		dma_buff = &hw->aq.asq.r.asq_bi[hw->aq.asq.next_to_use];
		 
		memcpy(dma_buff->va, buff, buff_size);
		desc_on_ring->datalen = cpu_to_le16(buff_size);

		 
		desc_on_ring->params.external.addr_high =
				cpu_to_le32(upper_32_bits(dma_buff->pa));
		desc_on_ring->params.external.addr_low =
				cpu_to_le32(lower_32_bits(dma_buff->pa));
	}

	 
	iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE, "AQTX: desc and buffer:\n");
	iavf_debug_aq(hw, IAVF_DEBUG_AQ_COMMAND, (void *)desc_on_ring,
		      buff, buff_size);
	(hw->aq.asq.next_to_use)++;
	if (hw->aq.asq.next_to_use == hw->aq.asq.count)
		hw->aq.asq.next_to_use = 0;
	if (!details->postpone)
		wr32(hw, hw->aq.asq.tail, hw->aq.asq.next_to_use);

	 
	if (!details->async && !details->postpone) {
		u32 total_delay = 0;

		do {
			 
			if (iavf_asq_done(hw))
				break;
			udelay(50);
			total_delay += 50;
		} while (total_delay < hw->aq.asq_cmd_timeout);
	}

	 
	if (iavf_asq_done(hw)) {
		*desc = *desc_on_ring;
		if (buff)
			memcpy(buff, dma_buff->va, buff_size);
		retval = le16_to_cpu(desc->retval);
		if (retval != 0) {
			iavf_debug(hw,
				   IAVF_DEBUG_AQ_MESSAGE,
				   "AQTX: Command completed with error 0x%X.\n",
				   retval);

			 
			retval &= 0xff;
		}
		cmd_completed = true;
		if ((enum iavf_admin_queue_err)retval == IAVF_AQ_RC_OK)
			status = 0;
		else if ((enum iavf_admin_queue_err)retval == IAVF_AQ_RC_EBUSY)
			status = IAVF_ERR_NOT_READY;
		else
			status = IAVF_ERR_ADMIN_QUEUE_ERROR;
		hw->aq.asq_last_status = (enum iavf_admin_queue_err)retval;
	}

	iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE,
		   "AQTX: desc and buffer writeback:\n");
	iavf_debug_aq(hw, IAVF_DEBUG_AQ_COMMAND, (void *)desc, buff, buff_size);

	 
	if (details->wb_desc)
		*details->wb_desc = *desc_on_ring;

	 
	if ((!cmd_completed) &&
	    (!details->async && !details->postpone)) {
		if (rd32(hw, hw->aq.asq.len) & IAVF_VF_ATQLEN1_ATQCRIT_MASK) {
			iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE,
				   "AQTX: AQ Critical error.\n");
			status = IAVF_ERR_ADMIN_QUEUE_CRITICAL_ERROR;
		} else {
			iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE,
				   "AQTX: Writeback timeout.\n");
			status = IAVF_ERR_ADMIN_QUEUE_TIMEOUT;
		}
	}

asq_send_command_error:
	mutex_unlock(&hw->aq.asq_mutex);
	return status;
}

 
void iavf_fill_default_direct_cmd_desc(struct iavf_aq_desc *desc, u16 opcode)
{
	 
	memset((void *)desc, 0, sizeof(struct iavf_aq_desc));
	desc->opcode = cpu_to_le16(opcode);
	desc->flags = cpu_to_le16(IAVF_AQ_FLAG_SI);
}

 
enum iavf_status iavf_clean_arq_element(struct iavf_hw *hw,
					struct iavf_arq_event_info *e,
					u16 *pending)
{
	u16 ntc = hw->aq.arq.next_to_clean;
	struct iavf_aq_desc *desc;
	enum iavf_status ret_code = 0;
	struct iavf_dma_mem *bi;
	u16 desc_idx;
	u16 datalen;
	u16 flags;
	u16 ntu;

	 
	memset(&e->desc, 0, sizeof(e->desc));

	 
	mutex_lock(&hw->aq.arq_mutex);

	if (hw->aq.arq.count == 0) {
		iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE,
			   "AQRX: Admin queue not initialized.\n");
		ret_code = IAVF_ERR_QUEUE_EMPTY;
		goto clean_arq_element_err;
	}

	 
	ntu = rd32(hw, hw->aq.arq.head) & IAVF_VF_ARQH1_ARQH_MASK;
	if (ntu == ntc) {
		 
		ret_code = IAVF_ERR_ADMIN_QUEUE_NO_WORK;
		goto clean_arq_element_out;
	}

	 
	desc = IAVF_ADMINQ_DESC(hw->aq.arq, ntc);
	desc_idx = ntc;

	hw->aq.arq_last_status =
		(enum iavf_admin_queue_err)le16_to_cpu(desc->retval);
	flags = le16_to_cpu(desc->flags);
	if (flags & IAVF_AQ_FLAG_ERR) {
		ret_code = IAVF_ERR_ADMIN_QUEUE_ERROR;
		iavf_debug(hw,
			   IAVF_DEBUG_AQ_MESSAGE,
			   "AQRX: Event received with error 0x%X.\n",
			   hw->aq.arq_last_status);
	}

	e->desc = *desc;
	datalen = le16_to_cpu(desc->datalen);
	e->msg_len = min(datalen, e->buf_len);
	if (e->msg_buf && (e->msg_len != 0))
		memcpy(e->msg_buf, hw->aq.arq.r.arq_bi[desc_idx].va,
		       e->msg_len);

	iavf_debug(hw, IAVF_DEBUG_AQ_MESSAGE, "AQRX: desc and buffer:\n");
	iavf_debug_aq(hw, IAVF_DEBUG_AQ_COMMAND, (void *)desc, e->msg_buf,
		      hw->aq.arq_buf_size);

	 
	bi = &hw->aq.arq.r.arq_bi[ntc];
	memset((void *)desc, 0, sizeof(struct iavf_aq_desc));

	desc->flags = cpu_to_le16(IAVF_AQ_FLAG_BUF);
	if (hw->aq.arq_buf_size > IAVF_AQ_LARGE_BUF)
		desc->flags |= cpu_to_le16(IAVF_AQ_FLAG_LB);
	desc->datalen = cpu_to_le16((u16)bi->size);
	desc->params.external.addr_high = cpu_to_le32(upper_32_bits(bi->pa));
	desc->params.external.addr_low = cpu_to_le32(lower_32_bits(bi->pa));

	 
	wr32(hw, hw->aq.arq.tail, ntc);
	 
	ntc++;
	if (ntc == hw->aq.num_arq_entries)
		ntc = 0;
	hw->aq.arq.next_to_clean = ntc;
	hw->aq.arq.next_to_use = ntu;

clean_arq_element_out:
	 
	if (pending)
		*pending = (ntc > ntu ? hw->aq.arq.count : 0) + (ntu - ntc);

clean_arq_element_err:
	mutex_unlock(&hw->aq.arq_mutex);

	return ret_code;
}
