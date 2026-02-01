
 

#include "iosm_ipc_protocol.h"
#include "iosm_ipc_protocol_ops.h"

 
static union ipc_mem_msg_entry *
ipc_protocol_free_msg_get(struct iosm_protocol *ipc_protocol, int *index)
{
	u32 head = le32_to_cpu(ipc_protocol->p_ap_shm->msg_head);
	u32 new_head = (head + 1) % IPC_MEM_MSG_ENTRIES;
	union ipc_mem_msg_entry *msg;

	if (new_head == le32_to_cpu(ipc_protocol->p_ap_shm->msg_tail)) {
		dev_err(ipc_protocol->dev, "message ring is full");
		return NULL;
	}

	 
	msg = &ipc_protocol->p_ap_shm->msg_ring[head];
	memset(msg, 0, sizeof(*msg));

	 
	*index = head;

	return msg;
}

 
void ipc_protocol_msg_hp_update(struct iosm_imem *ipc_imem)
{
	struct iosm_protocol *ipc_protocol = ipc_imem->ipc_protocol;
	u32 head = le32_to_cpu(ipc_protocol->p_ap_shm->msg_head);
	u32 new_head = (head + 1) % IPC_MEM_MSG_ENTRIES;

	 
	ipc_protocol->p_ap_shm->msg_head = cpu_to_le32(new_head);
	ipc_protocol->old_msg_tail =
		le32_to_cpu(ipc_protocol->p_ap_shm->msg_tail);

	ipc_pm_signal_hpda_doorbell(&ipc_protocol->pm, IPC_HP_MR, false);
}

 
static int ipc_protocol_msg_prepipe_open(struct iosm_protocol *ipc_protocol,
					 union ipc_msg_prep_args *args)
{
	int index;
	union ipc_mem_msg_entry *msg =
		ipc_protocol_free_msg_get(ipc_protocol, &index);
	struct ipc_pipe *pipe = args->pipe_open.pipe;
	struct ipc_protocol_td *tdr;
	struct sk_buff **skbr;

	if (!msg) {
		dev_err(ipc_protocol->dev, "failed to get free message");
		return -EIO;
	}

	 
	skbr = kcalloc(pipe->nr_of_entries, sizeof(*skbr), GFP_ATOMIC);
	if (!skbr)
		return -ENOMEM;

	 
	tdr = dma_alloc_coherent(&ipc_protocol->pcie->pci->dev,
				 pipe->nr_of_entries * sizeof(*tdr),
				 &pipe->phy_tdr_start, GFP_ATOMIC);
	if (!tdr) {
		kfree(skbr);
		dev_err(ipc_protocol->dev, "tdr alloc error");
		return -ENOMEM;
	}

	pipe->max_nr_of_queued_entries = pipe->nr_of_entries - 1;
	pipe->nr_of_queued_entries = 0;
	pipe->tdr_start = tdr;
	pipe->skbr_start = skbr;
	pipe->old_tail = 0;

	ipc_protocol->p_ap_shm->head_array[pipe->pipe_nr] = 0;

	msg->open_pipe.type_of_message = IPC_MEM_MSG_OPEN_PIPE;
	msg->open_pipe.pipe_nr = pipe->pipe_nr;
	msg->open_pipe.tdr_addr = cpu_to_le64(pipe->phy_tdr_start);
	msg->open_pipe.tdr_entries = cpu_to_le16(pipe->nr_of_entries);
	msg->open_pipe.accumulation_backoff =
				cpu_to_le32(pipe->accumulation_backoff);
	msg->open_pipe.irq_vector = cpu_to_le32(pipe->irq);

	return index;
}

static int ipc_protocol_msg_prepipe_close(struct iosm_protocol *ipc_protocol,
					  union ipc_msg_prep_args *args)
{
	int index = -1;
	union ipc_mem_msg_entry *msg =
		ipc_protocol_free_msg_get(ipc_protocol, &index);
	struct ipc_pipe *pipe = args->pipe_close.pipe;

	if (!msg)
		return -EIO;

	msg->close_pipe.type_of_message = IPC_MEM_MSG_CLOSE_PIPE;
	msg->close_pipe.pipe_nr = pipe->pipe_nr;

	dev_dbg(ipc_protocol->dev, "IPC_MEM_MSG_CLOSE_PIPE(pipe_nr=%d)",
		msg->close_pipe.pipe_nr);

	return index;
}

static int ipc_protocol_msg_prep_sleep(struct iosm_protocol *ipc_protocol,
				       union ipc_msg_prep_args *args)
{
	int index = -1;
	union ipc_mem_msg_entry *msg =
		ipc_protocol_free_msg_get(ipc_protocol, &index);

	if (!msg) {
		dev_err(ipc_protocol->dev, "failed to get free message");
		return -EIO;
	}

	 
	msg->host_sleep.type_of_message = IPC_MEM_MSG_SLEEP;
	msg->host_sleep.target = args->sleep.target;  

	 
	msg->host_sleep.state = args->sleep.state;

	dev_dbg(ipc_protocol->dev, "IPC_MEM_MSG_SLEEP(target=%d; state=%d)",
		msg->host_sleep.target, msg->host_sleep.state);

	return index;
}

static int ipc_protocol_msg_prep_feature_set(struct iosm_protocol *ipc_protocol,
					     union ipc_msg_prep_args *args)
{
	int index = -1;
	union ipc_mem_msg_entry *msg =
		ipc_protocol_free_msg_get(ipc_protocol, &index);

	if (!msg) {
		dev_err(ipc_protocol->dev, "failed to get free message");
		return -EIO;
	}

	msg->feature_set.type_of_message = IPC_MEM_MSG_FEATURE_SET;
	msg->feature_set.reset_enable = args->feature_set.reset_enable <<
					RESET_BIT;

	dev_dbg(ipc_protocol->dev, "IPC_MEM_MSG_FEATURE_SET(reset_enable=%d)",
		msg->feature_set.reset_enable >> RESET_BIT);

	return index;
}

 
bool ipc_protocol_msg_process(struct iosm_imem *ipc_imem, int irq)
{
	struct iosm_protocol *ipc_protocol = ipc_imem->ipc_protocol;
	struct ipc_rsp **rsp_ring = ipc_protocol->rsp_ring;
	bool msg_processed = false;
	u32 i;

	if (le32_to_cpu(ipc_protocol->p_ap_shm->msg_tail) >=
			IPC_MEM_MSG_ENTRIES) {
		dev_err(ipc_protocol->dev, "msg_tail out of range: %d",
			le32_to_cpu(ipc_protocol->p_ap_shm->msg_tail));
		return msg_processed;
	}

	if (irq != IMEM_IRQ_DONT_CARE &&
	    irq != ipc_protocol->p_ap_shm->ci.msg_irq_vector)
		return msg_processed;

	for (i = ipc_protocol->old_msg_tail;
	     i != le32_to_cpu(ipc_protocol->p_ap_shm->msg_tail);
	     i = (i + 1) % IPC_MEM_MSG_ENTRIES) {
		union ipc_mem_msg_entry *msg =
			&ipc_protocol->p_ap_shm->msg_ring[i];

		dev_dbg(ipc_protocol->dev, "msg[%d]: type=%u status=%d", i,
			msg->common.type_of_message,
			msg->common.completion_status);

		 
		if (rsp_ring[i]) {
			rsp_ring[i]->status =
				le32_to_cpu(msg->common.completion_status);
			complete(&rsp_ring[i]->completion);
			rsp_ring[i] = NULL;
		}
		msg_processed = true;
	}

	ipc_protocol->old_msg_tail = i;
	return msg_processed;
}

 
bool ipc_protocol_ul_td_send(struct iosm_protocol *ipc_protocol,
			     struct ipc_pipe *pipe,
			     struct sk_buff_head *p_ul_list)
{
	struct ipc_protocol_td *td;
	bool hpda_pending = false;
	struct sk_buff *skb;
	s32 free_elements;
	u32 head;
	u32 tail;

	if (!ipc_protocol->p_ap_shm) {
		dev_err(ipc_protocol->dev, "driver is not initialized");
		return false;
	}

	 
	head = le32_to_cpu(ipc_protocol->p_ap_shm->head_array[pipe->pipe_nr]);
	tail = pipe->old_tail;

	while (!skb_queue_empty(p_ul_list)) {
		if (head < tail)
			free_elements = tail - head - 1;
		else
			free_elements =
				pipe->nr_of_entries - head + ((s32)tail - 1);

		if (free_elements <= 0) {
			dev_dbg(ipc_protocol->dev,
				"no free td elements for UL pipe %d",
				pipe->pipe_nr);
			break;
		}

		 
		td = &pipe->tdr_start[head];

		 
		skb = skb_dequeue(p_ul_list);
		if (WARN_ON(!skb))
			break;

		 
		pipe->skbr_start[head] = skb;

		td->buffer.address = IPC_CB(skb)->mapping;
		td->scs = cpu_to_le32(skb->len) & cpu_to_le32(SIZE_MASK);
		td->next = 0;

		pipe->nr_of_queued_entries++;

		 
		head++;
		if (head >= pipe->nr_of_entries)
			head = 0;

		ipc_protocol->p_ap_shm->head_array[pipe->pipe_nr] =
			cpu_to_le32(head);
	}

	if (pipe->old_head != head) {
		dev_dbg(ipc_protocol->dev, "New UL TDs Pipe:%d", pipe->pipe_nr);

		pipe->old_head = head;
		 
		hpda_pending = true;
	}

	return hpda_pending;
}

 
struct sk_buff *ipc_protocol_ul_td_process(struct iosm_protocol *ipc_protocol,
					   struct ipc_pipe *pipe)
{
	struct ipc_protocol_td *p_td = &pipe->tdr_start[pipe->old_tail];
	struct sk_buff *skb = pipe->skbr_start[pipe->old_tail];

	pipe->nr_of_queued_entries--;
	pipe->old_tail++;
	if (pipe->old_tail >= pipe->nr_of_entries)
		pipe->old_tail = 0;

	if (!p_td->buffer.address) {
		dev_err(ipc_protocol->dev, "Td buffer address is NULL");
		return NULL;
	}

	if (p_td->buffer.address != IPC_CB(skb)->mapping) {
		dev_err(ipc_protocol->dev,
			"pipe %d: invalid buf_addr or skb_data",
			pipe->pipe_nr);
		return NULL;
	}

	return skb;
}

 
bool ipc_protocol_dl_td_prepare(struct iosm_protocol *ipc_protocol,
				struct ipc_pipe *pipe)
{
	struct ipc_protocol_td *td;
	dma_addr_t mapping = 0;
	u32 head, new_head;
	struct sk_buff *skb;
	u32 tail;

	 
	head = le32_to_cpu(ipc_protocol->p_ap_shm->head_array[pipe->pipe_nr]);
	tail = le32_to_cpu(ipc_protocol->p_ap_shm->tail_array[pipe->pipe_nr]);

	new_head = head + 1;
	if (new_head >= pipe->nr_of_entries)
		new_head = 0;

	if (new_head == tail)
		return false;

	 
	td = &pipe->tdr_start[head];

	 
	skb = ipc_pcie_alloc_skb(ipc_protocol->pcie, pipe->buf_size, GFP_ATOMIC,
				 &mapping, DMA_FROM_DEVICE,
				 IPC_MEM_DL_ETH_OFFSET);
	if (!skb)
		return false;

	td->buffer.address = mapping;
	td->scs = cpu_to_le32(pipe->buf_size) & cpu_to_le32(SIZE_MASK);
	td->next = 0;

	 
	ipc_protocol->p_ap_shm->head_array[pipe->pipe_nr] =
		cpu_to_le32(new_head);

	 
	pipe->skbr_start[head] = skb;

	pipe->nr_of_queued_entries++;

	return true;
}

 
struct sk_buff *ipc_protocol_dl_td_process(struct iosm_protocol *ipc_protocol,
					   struct ipc_pipe *pipe)
{
	struct ipc_protocol_td *p_td;
	struct sk_buff *skb;

	if (!pipe->tdr_start)
		return NULL;

	 
	p_td = &pipe->tdr_start[pipe->old_tail];
	skb = pipe->skbr_start[pipe->old_tail];

	 
	pipe->skbr_start[pipe->old_tail] = NULL;

	pipe->nr_of_queued_entries--;

	pipe->old_tail++;
	if (pipe->old_tail >= pipe->nr_of_entries)
		pipe->old_tail = 0;

	if (!skb) {
		dev_err(ipc_protocol->dev, "skb is null");
		goto ret;
	} else if (!p_td->buffer.address) {
		dev_err(ipc_protocol->dev, "td/buffer address is null");
		ipc_pcie_kfree_skb(ipc_protocol->pcie, skb);
		skb = NULL;
		goto ret;
	}

	if (p_td->buffer.address != IPC_CB(skb)->mapping) {
		dev_err(ipc_protocol->dev, "invalid buf=%llx or skb=%p",
			(unsigned long long)p_td->buffer.address, skb->data);
		ipc_pcie_kfree_skb(ipc_protocol->pcie, skb);
		skb = NULL;
		goto ret;
	} else if ((le32_to_cpu(p_td->scs) & SIZE_MASK) > pipe->buf_size) {
		dev_err(ipc_protocol->dev, "invalid buffer size %d > %d",
			le32_to_cpu(p_td->scs) & SIZE_MASK,
			pipe->buf_size);
		ipc_pcie_kfree_skb(ipc_protocol->pcie, skb);
		skb = NULL;
		goto ret;
	} else if (le32_to_cpu(p_td->scs) >> COMPLETION_STATUS ==
		  IPC_MEM_TD_CS_ABORT) {
		 
		dev_dbg(ipc_protocol->dev, "discard 'aborted' buffers");
		ipc_pcie_kfree_skb(ipc_protocol->pcie, skb);
		skb = NULL;
		goto ret;
	}

	 
	skb_put(skb, le32_to_cpu(p_td->scs) & SIZE_MASK);

ret:
	return skb;
}

void ipc_protocol_get_head_tail_index(struct iosm_protocol *ipc_protocol,
				      struct ipc_pipe *pipe, u32 *head,
				      u32 *tail)
{
	struct ipc_protocol_ap_shm *ipc_ap_shm = ipc_protocol->p_ap_shm;

	if (head)
		*head = le32_to_cpu(ipc_ap_shm->head_array[pipe->pipe_nr]);

	if (tail)
		*tail = le32_to_cpu(ipc_ap_shm->tail_array[pipe->pipe_nr]);
}

 
void ipc_protocol_pipe_cleanup(struct iosm_protocol *ipc_protocol,
			       struct ipc_pipe *pipe)
{
	struct sk_buff *skb;
	u32 head;
	u32 tail;

	 
	head = le32_to_cpu(ipc_protocol->p_ap_shm->head_array[pipe->pipe_nr]);
	tail = pipe->old_tail;

	 
	ipc_protocol->p_ap_shm->tail_array[pipe->pipe_nr] = 0;
	ipc_protocol->p_ap_shm->head_array[pipe->pipe_nr] = 0;

	 
	if (pipe->skbr_start) {
		while (head != tail) {
			 
			skb = pipe->skbr_start[tail];
			if (skb)
				ipc_pcie_kfree_skb(ipc_protocol->pcie, skb);

			tail++;
			if (tail >= pipe->nr_of_entries)
				tail = 0;
		}

		kfree(pipe->skbr_start);
		pipe->skbr_start = NULL;
	}

	pipe->old_tail = 0;

	 
	if (pipe->tdr_start) {
		dma_free_coherent(&ipc_protocol->pcie->pci->dev,
				  sizeof(*pipe->tdr_start) * pipe->nr_of_entries,
				  pipe->tdr_start, pipe->phy_tdr_start);

		pipe->tdr_start = NULL;
	}
}

enum ipc_mem_device_ipc_state ipc_protocol_get_ipc_status(struct iosm_protocol
							  *ipc_protocol)
{
	return (enum ipc_mem_device_ipc_state)
		le32_to_cpu(ipc_protocol->p_ap_shm->device_info.ipc_status);
}

enum ipc_mem_exec_stage
ipc_protocol_get_ap_exec_stage(struct iosm_protocol *ipc_protocol)
{
	return le32_to_cpu(ipc_protocol->p_ap_shm->device_info.execution_stage);
}

int ipc_protocol_msg_prep(struct iosm_imem *ipc_imem,
			  enum ipc_msg_prep_type msg_type,
			  union ipc_msg_prep_args *args)
{
	struct iosm_protocol *ipc_protocol = ipc_imem->ipc_protocol;

	switch (msg_type) {
	case IPC_MSG_PREP_SLEEP:
		return ipc_protocol_msg_prep_sleep(ipc_protocol, args);

	case IPC_MSG_PREP_PIPE_OPEN:
		return ipc_protocol_msg_prepipe_open(ipc_protocol, args);

	case IPC_MSG_PREP_PIPE_CLOSE:
		return ipc_protocol_msg_prepipe_close(ipc_protocol, args);

	case IPC_MSG_PREP_FEATURE_SET:
		return ipc_protocol_msg_prep_feature_set(ipc_protocol, args);

		 
	case IPC_MSG_PREP_MAP:
	case IPC_MSG_PREP_UNMAP:
	default:
		dev_err(ipc_protocol->dev,
			"unsupported message type: %d in protocol", msg_type);
		return -EINVAL;
	}
}

u32
ipc_protocol_pm_dev_get_sleep_notification(struct iosm_protocol *ipc_protocol)
{
	struct ipc_protocol_ap_shm *ipc_ap_shm = ipc_protocol->p_ap_shm;

	return le32_to_cpu(ipc_ap_shm->device_info.device_sleep_notification);
}
