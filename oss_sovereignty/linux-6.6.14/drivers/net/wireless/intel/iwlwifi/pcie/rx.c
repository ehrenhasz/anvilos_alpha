
 
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/gfp.h>

#include "iwl-prph.h"
#include "iwl-io.h"
#include "internal.h"
#include "iwl-op-mode.h"
#include "iwl-context-info-gen3.h"

 

 

 
static int iwl_rxq_space(const struct iwl_rxq *rxq)
{
	 
	WARN_ON(rxq->queue_size & (rxq->queue_size - 1));

	 
	return (rxq->read - rxq->write - 1) & (rxq->queue_size - 1);
}

 
static inline __le32 iwl_pcie_dma_addr2rbd_ptr(dma_addr_t dma_addr)
{
	return cpu_to_le32((u32)(dma_addr >> 8));
}

 
int iwl_pcie_rx_stop(struct iwl_trans *trans)
{
	if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210) {
		 
		iwl_write_umac_prph(trans, RFH_RXF_DMA_CFG_GEN3, 0);
		return iwl_poll_umac_prph_bit(trans, RFH_GEN_STATUS_GEN3,
					      RXF_DMA_IDLE, RXF_DMA_IDLE, 1000);
	} else if (trans->trans_cfg->mq_rx_supported) {
		iwl_write_prph(trans, RFH_RXF_DMA_CFG, 0);
		return iwl_poll_prph_bit(trans, RFH_GEN_STATUS,
					   RXF_DMA_IDLE, RXF_DMA_IDLE, 1000);
	} else {
		iwl_write_direct32(trans, FH_MEM_RCSR_CHNL0_CONFIG_REG, 0);
		return iwl_poll_direct_bit(trans, FH_MEM_RSSR_RX_STATUS_REG,
					   FH_RSSR_CHNL0_RX_STATUS_CHNL_IDLE,
					   1000);
	}
}

 
static void iwl_pcie_rxq_inc_wr_ptr(struct iwl_trans *trans,
				    struct iwl_rxq *rxq)
{
	u32 reg;

	lockdep_assert_held(&rxq->lock);

	 
	if (!trans->trans_cfg->base_params->shadow_reg_enable &&
	    test_bit(STATUS_TPOWER_PMI, &trans->status)) {
		reg = iwl_read32(trans, CSR_UCODE_DRV_GP1);

		if (reg & CSR_UCODE_DRV_GP1_BIT_MAC_SLEEP) {
			IWL_DEBUG_INFO(trans, "Rx queue requesting wakeup, GP1 = 0x%x\n",
				       reg);
			iwl_set_bit(trans, CSR_GP_CNTRL,
				    CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
			rxq->need_update = true;
			return;
		}
	}

	rxq->write_actual = round_down(rxq->write, 8);
	if (!trans->trans_cfg->mq_rx_supported)
		iwl_write32(trans, FH_RSCSR_CHNL0_WPTR, rxq->write_actual);
	else if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_BZ)
		iwl_write32(trans, HBUS_TARG_WRPTR, rxq->write_actual |
			    HBUS_TARG_WRPTR_RX_Q(rxq->id));
	else
		iwl_write32(trans, RFH_Q_FRBDCB_WIDX_TRG(rxq->id),
			    rxq->write_actual);
}

static void iwl_pcie_rxq_check_wrptr(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	int i;

	for (i = 0; i < trans->num_rx_queues; i++) {
		struct iwl_rxq *rxq = &trans_pcie->rxq[i];

		if (!rxq->need_update)
			continue;
		spin_lock_bh(&rxq->lock);
		iwl_pcie_rxq_inc_wr_ptr(trans, rxq);
		rxq->need_update = false;
		spin_unlock_bh(&rxq->lock);
	}
}

static void iwl_pcie_restock_bd(struct iwl_trans *trans,
				struct iwl_rxq *rxq,
				struct iwl_rx_mem_buffer *rxb)
{
	if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210) {
		struct iwl_rx_transfer_desc *bd = rxq->bd;

		BUILD_BUG_ON(sizeof(*bd) != 2 * sizeof(u64));

		bd[rxq->write].addr = cpu_to_le64(rxb->page_dma);
		bd[rxq->write].rbid = cpu_to_le16(rxb->vid);
	} else {
		__le64 *bd = rxq->bd;

		bd[rxq->write] = cpu_to_le64(rxb->page_dma | rxb->vid);
	}

	IWL_DEBUG_RX(trans, "Assigned virtual RB ID %u to queue %d index %d\n",
		     (u32)rxb->vid, rxq->id, rxq->write);
}

 
static void iwl_pcie_rxmq_restock(struct iwl_trans *trans,
				  struct iwl_rxq *rxq)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rx_mem_buffer *rxb;

	 
	if (!test_bit(STATUS_DEVICE_ENABLED, &trans->status))
		return;

	spin_lock_bh(&rxq->lock);
	while (rxq->free_count) {
		 
		rxb = list_first_entry(&rxq->rx_free, struct iwl_rx_mem_buffer,
				       list);
		list_del(&rxb->list);
		rxb->invalid = false;
		 
		WARN_ON(rxb->page_dma & trans_pcie->supported_dma_mask);
		 
		iwl_pcie_restock_bd(trans, rxq, rxb);
		rxq->write = (rxq->write + 1) & (rxq->queue_size - 1);
		rxq->free_count--;
	}
	spin_unlock_bh(&rxq->lock);

	 
	if (rxq->write_actual != (rxq->write & ~0x7)) {
		spin_lock_bh(&rxq->lock);
		iwl_pcie_rxq_inc_wr_ptr(trans, rxq);
		spin_unlock_bh(&rxq->lock);
	}
}

 
static void iwl_pcie_rxsq_restock(struct iwl_trans *trans,
				  struct iwl_rxq *rxq)
{
	struct iwl_rx_mem_buffer *rxb;

	 
	if (!test_bit(STATUS_DEVICE_ENABLED, &trans->status))
		return;

	spin_lock_bh(&rxq->lock);
	while ((iwl_rxq_space(rxq) > 0) && (rxq->free_count)) {
		__le32 *bd = (__le32 *)rxq->bd;
		 
		rxb = rxq->queue[rxq->write];
		BUG_ON(rxb && rxb->page);

		 
		rxb = list_first_entry(&rxq->rx_free, struct iwl_rx_mem_buffer,
				       list);
		list_del(&rxb->list);
		rxb->invalid = false;

		 
		bd[rxq->write] = iwl_pcie_dma_addr2rbd_ptr(rxb->page_dma);
		rxq->queue[rxq->write] = rxb;
		rxq->write = (rxq->write + 1) & RX_QUEUE_MASK;
		rxq->free_count--;
	}
	spin_unlock_bh(&rxq->lock);

	 
	if (rxq->write_actual != (rxq->write & ~0x7)) {
		spin_lock_bh(&rxq->lock);
		iwl_pcie_rxq_inc_wr_ptr(trans, rxq);
		spin_unlock_bh(&rxq->lock);
	}
}

 
static
void iwl_pcie_rxq_restock(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
	if (trans->trans_cfg->mq_rx_supported)
		iwl_pcie_rxmq_restock(trans, rxq);
	else
		iwl_pcie_rxsq_restock(trans, rxq);
}

 
static struct page *iwl_pcie_rx_alloc_page(struct iwl_trans *trans,
					   u32 *offset, gfp_t priority)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	unsigned int rbsize = iwl_trans_get_rb_size(trans_pcie->rx_buf_size);
	unsigned int allocsize = PAGE_SIZE << trans_pcie->rx_page_order;
	struct page *page;
	gfp_t gfp_mask = priority;

	if (trans_pcie->rx_page_order > 0)
		gfp_mask |= __GFP_COMP;

	if (trans_pcie->alloc_page) {
		spin_lock_bh(&trans_pcie->alloc_page_lock);
		 
		if (trans_pcie->alloc_page) {
			*offset = trans_pcie->alloc_page_used;
			page = trans_pcie->alloc_page;
			trans_pcie->alloc_page_used += rbsize;
			if (trans_pcie->alloc_page_used >= allocsize)
				trans_pcie->alloc_page = NULL;
			else
				get_page(page);
			spin_unlock_bh(&trans_pcie->alloc_page_lock);
			return page;
		}
		spin_unlock_bh(&trans_pcie->alloc_page_lock);
	}

	 
	page = alloc_pages(gfp_mask, trans_pcie->rx_page_order);
	if (!page) {
		if (net_ratelimit())
			IWL_DEBUG_INFO(trans, "alloc_pages failed, order: %d\n",
				       trans_pcie->rx_page_order);
		 
		if (!(gfp_mask & __GFP_NOWARN) && net_ratelimit())
			IWL_CRIT(trans,
				 "Failed to alloc_pages\n");
		return NULL;
	}

	if (2 * rbsize <= allocsize) {
		spin_lock_bh(&trans_pcie->alloc_page_lock);
		if (!trans_pcie->alloc_page) {
			get_page(page);
			trans_pcie->alloc_page = page;
			trans_pcie->alloc_page_used = rbsize;
		}
		spin_unlock_bh(&trans_pcie->alloc_page_lock);
	}

	*offset = 0;
	return page;
}

 
void iwl_pcie_rxq_alloc_rbs(struct iwl_trans *trans, gfp_t priority,
			    struct iwl_rxq *rxq)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rx_mem_buffer *rxb;
	struct page *page;

	while (1) {
		unsigned int offset;

		spin_lock_bh(&rxq->lock);
		if (list_empty(&rxq->rx_used)) {
			spin_unlock_bh(&rxq->lock);
			return;
		}
		spin_unlock_bh(&rxq->lock);

		page = iwl_pcie_rx_alloc_page(trans, &offset, priority);
		if (!page)
			return;

		spin_lock_bh(&rxq->lock);

		if (list_empty(&rxq->rx_used)) {
			spin_unlock_bh(&rxq->lock);
			__free_pages(page, trans_pcie->rx_page_order);
			return;
		}
		rxb = list_first_entry(&rxq->rx_used, struct iwl_rx_mem_buffer,
				       list);
		list_del(&rxb->list);
		spin_unlock_bh(&rxq->lock);

		BUG_ON(rxb->page);
		rxb->page = page;
		rxb->offset = offset;
		 
		rxb->page_dma =
			dma_map_page(trans->dev, page, rxb->offset,
				     trans_pcie->rx_buf_bytes,
				     DMA_FROM_DEVICE);
		if (dma_mapping_error(trans->dev, rxb->page_dma)) {
			rxb->page = NULL;
			spin_lock_bh(&rxq->lock);
			list_add(&rxb->list, &rxq->rx_used);
			spin_unlock_bh(&rxq->lock);
			__free_pages(page, trans_pcie->rx_page_order);
			return;
		}

		spin_lock_bh(&rxq->lock);

		list_add_tail(&rxb->list, &rxq->rx_free);
		rxq->free_count++;

		spin_unlock_bh(&rxq->lock);
	}
}

void iwl_pcie_free_rbs_pool(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	int i;

	if (!trans_pcie->rx_pool)
		return;

	for (i = 0; i < RX_POOL_SIZE(trans_pcie->num_rx_bufs); i++) {
		if (!trans_pcie->rx_pool[i].page)
			continue;
		dma_unmap_page(trans->dev, trans_pcie->rx_pool[i].page_dma,
			       trans_pcie->rx_buf_bytes, DMA_FROM_DEVICE);
		__free_pages(trans_pcie->rx_pool[i].page,
			     trans_pcie->rx_page_order);
		trans_pcie->rx_pool[i].page = NULL;
	}
}

 
static void iwl_pcie_rx_allocator(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rb_allocator *rba = &trans_pcie->rba;
	struct list_head local_empty;
	int pending = atomic_read(&rba->req_pending);

	IWL_DEBUG_TPT(trans, "Pending allocation requests = %d\n", pending);

	 
	spin_lock_bh(&rba->lock);
	 
	list_replace_init(&rba->rbd_empty, &local_empty);
	spin_unlock_bh(&rba->lock);

	while (pending) {
		int i;
		LIST_HEAD(local_allocated);
		gfp_t gfp_mask = GFP_KERNEL;

		 
		if (pending < RX_PENDING_WATERMARK)
			gfp_mask |= __GFP_NOWARN;

		for (i = 0; i < RX_CLAIM_REQ_ALLOC;) {
			struct iwl_rx_mem_buffer *rxb;
			struct page *page;

			 
			BUG_ON(list_empty(&local_empty));
			 
			rxb = list_first_entry(&local_empty,
					       struct iwl_rx_mem_buffer, list);
			BUG_ON(rxb->page);

			 
			page = iwl_pcie_rx_alloc_page(trans, &rxb->offset,
						      gfp_mask);
			if (!page)
				continue;
			rxb->page = page;

			 
			rxb->page_dma = dma_map_page(trans->dev, page,
						     rxb->offset,
						     trans_pcie->rx_buf_bytes,
						     DMA_FROM_DEVICE);
			if (dma_mapping_error(trans->dev, rxb->page_dma)) {
				rxb->page = NULL;
				__free_pages(page, trans_pcie->rx_page_order);
				continue;
			}

			 
			list_move(&rxb->list, &local_allocated);
			i++;
		}

		atomic_dec(&rba->req_pending);
		pending--;

		if (!pending) {
			pending = atomic_read(&rba->req_pending);
			if (pending)
				IWL_DEBUG_TPT(trans,
					      "Got more pending allocation requests = %d\n",
					      pending);
		}

		spin_lock_bh(&rba->lock);
		 
		list_splice_tail(&local_allocated, &rba->rbd_allocated);
		 
		list_splice_tail_init(&rba->rbd_empty, &local_empty);
		spin_unlock_bh(&rba->lock);

		atomic_inc(&rba->req_ready);

	}

	spin_lock_bh(&rba->lock);
	 
	list_splice_tail(&local_empty, &rba->rbd_empty);
	spin_unlock_bh(&rba->lock);

	IWL_DEBUG_TPT(trans, "%s, exit.\n", __func__);
}

 
static void iwl_pcie_rx_allocator_get(struct iwl_trans *trans,
				      struct iwl_rxq *rxq)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rb_allocator *rba = &trans_pcie->rba;
	int i;

	lockdep_assert_held(&rxq->lock);

	 
	if (atomic_dec_if_positive(&rba->req_ready) < 0)
		return;

	spin_lock(&rba->lock);
	for (i = 0; i < RX_CLAIM_REQ_ALLOC; i++) {
		 
		struct iwl_rx_mem_buffer *rxb =
			list_first_entry(&rba->rbd_allocated,
					 struct iwl_rx_mem_buffer, list);

		list_move(&rxb->list, &rxq->rx_free);
	}
	spin_unlock(&rba->lock);

	rxq->used_count -= RX_CLAIM_REQ_ALLOC;
	rxq->free_count += RX_CLAIM_REQ_ALLOC;
}

void iwl_pcie_rx_allocator_work(struct work_struct *data)
{
	struct iwl_rb_allocator *rba_p =
		container_of(data, struct iwl_rb_allocator, rx_alloc);
	struct iwl_trans_pcie *trans_pcie =
		container_of(rba_p, struct iwl_trans_pcie, rba);

	iwl_pcie_rx_allocator(trans_pcie->trans);
}

static int iwl_pcie_free_bd_size(struct iwl_trans *trans)
{
	if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210)
		return sizeof(struct iwl_rx_transfer_desc);

	return trans->trans_cfg->mq_rx_supported ?
			sizeof(__le64) : sizeof(__le32);
}

static int iwl_pcie_used_bd_size(struct iwl_trans *trans)
{
	if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_BZ)
		return sizeof(struct iwl_rx_completion_desc_bz);

	if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210)
		return sizeof(struct iwl_rx_completion_desc);

	return sizeof(__le32);
}

static void iwl_pcie_free_rxq_dma(struct iwl_trans *trans,
				  struct iwl_rxq *rxq)
{
	int free_size = iwl_pcie_free_bd_size(trans);

	if (rxq->bd)
		dma_free_coherent(trans->dev,
				  free_size * rxq->queue_size,
				  rxq->bd, rxq->bd_dma);
	rxq->bd_dma = 0;
	rxq->bd = NULL;

	rxq->rb_stts_dma = 0;
	rxq->rb_stts = NULL;

	if (rxq->used_bd)
		dma_free_coherent(trans->dev,
				  iwl_pcie_used_bd_size(trans) *
					rxq->queue_size,
				  rxq->used_bd, rxq->used_bd_dma);
	rxq->used_bd_dma = 0;
	rxq->used_bd = NULL;
}

static size_t iwl_pcie_rb_stts_size(struct iwl_trans *trans)
{
	bool use_rx_td = (trans->trans_cfg->device_family >=
			  IWL_DEVICE_FAMILY_AX210);

	if (use_rx_td)
		return sizeof(__le16);

	return sizeof(struct iwl_rb_status);
}

static int iwl_pcie_alloc_rxq_dma(struct iwl_trans *trans,
				  struct iwl_rxq *rxq)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	size_t rb_stts_size = iwl_pcie_rb_stts_size(trans);
	struct device *dev = trans->dev;
	int i;
	int free_size;

	spin_lock_init(&rxq->lock);
	if (trans->trans_cfg->mq_rx_supported)
		rxq->queue_size = trans->cfg->num_rbds;
	else
		rxq->queue_size = RX_QUEUE_SIZE;

	free_size = iwl_pcie_free_bd_size(trans);

	 
	rxq->bd = dma_alloc_coherent(dev, free_size * rxq->queue_size,
				     &rxq->bd_dma, GFP_KERNEL);
	if (!rxq->bd)
		goto err;

	if (trans->trans_cfg->mq_rx_supported) {
		rxq->used_bd = dma_alloc_coherent(dev,
						  iwl_pcie_used_bd_size(trans) *
							rxq->queue_size,
						  &rxq->used_bd_dma,
						  GFP_KERNEL);
		if (!rxq->used_bd)
			goto err;
	}

	rxq->rb_stts = (u8 *)trans_pcie->base_rb_stts + rxq->id * rb_stts_size;
	rxq->rb_stts_dma =
		trans_pcie->base_rb_stts_dma + rxq->id * rb_stts_size;

	return 0;

err:
	for (i = 0; i < trans->num_rx_queues; i++) {
		struct iwl_rxq *rxq = &trans_pcie->rxq[i];

		iwl_pcie_free_rxq_dma(trans, rxq);
	}

	return -ENOMEM;
}

static int iwl_pcie_rx_alloc(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	size_t rb_stts_size = iwl_pcie_rb_stts_size(trans);
	struct iwl_rb_allocator *rba = &trans_pcie->rba;
	int i, ret;

	if (WARN_ON(trans_pcie->rxq))
		return -EINVAL;

	trans_pcie->rxq = kcalloc(trans->num_rx_queues, sizeof(struct iwl_rxq),
				  GFP_KERNEL);
	trans_pcie->rx_pool = kcalloc(RX_POOL_SIZE(trans_pcie->num_rx_bufs),
				      sizeof(trans_pcie->rx_pool[0]),
				      GFP_KERNEL);
	trans_pcie->global_table =
		kcalloc(RX_POOL_SIZE(trans_pcie->num_rx_bufs),
			sizeof(trans_pcie->global_table[0]),
			GFP_KERNEL);
	if (!trans_pcie->rxq || !trans_pcie->rx_pool ||
	    !trans_pcie->global_table) {
		ret = -ENOMEM;
		goto err;
	}

	spin_lock_init(&rba->lock);

	 
	trans_pcie->base_rb_stts =
			dma_alloc_coherent(trans->dev,
					   rb_stts_size * trans->num_rx_queues,
					   &trans_pcie->base_rb_stts_dma,
					   GFP_KERNEL);
	if (!trans_pcie->base_rb_stts) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < trans->num_rx_queues; i++) {
		struct iwl_rxq *rxq = &trans_pcie->rxq[i];

		rxq->id = i;
		ret = iwl_pcie_alloc_rxq_dma(trans, rxq);
		if (ret)
			goto err;
	}
	return 0;

err:
	if (trans_pcie->base_rb_stts) {
		dma_free_coherent(trans->dev,
				  rb_stts_size * trans->num_rx_queues,
				  trans_pcie->base_rb_stts,
				  trans_pcie->base_rb_stts_dma);
		trans_pcie->base_rb_stts = NULL;
		trans_pcie->base_rb_stts_dma = 0;
	}
	kfree(trans_pcie->rx_pool);
	trans_pcie->rx_pool = NULL;
	kfree(trans_pcie->global_table);
	trans_pcie->global_table = NULL;
	kfree(trans_pcie->rxq);
	trans_pcie->rxq = NULL;

	return ret;
}

static void iwl_pcie_rx_hw_init(struct iwl_trans *trans, struct iwl_rxq *rxq)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	u32 rb_size;
	const u32 rfdnlog = RX_QUEUE_SIZE_LOG;  

	switch (trans_pcie->rx_buf_size) {
	case IWL_AMSDU_4K:
		rb_size = FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_4K;
		break;
	case IWL_AMSDU_8K:
		rb_size = FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_8K;
		break;
	case IWL_AMSDU_12K:
		rb_size = FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_12K;
		break;
	default:
		WARN_ON(1);
		rb_size = FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_4K;
	}

	if (!iwl_trans_grab_nic_access(trans))
		return;

	 
	iwl_write32(trans, FH_MEM_RCSR_CHNL0_CONFIG_REG, 0);
	 
	iwl_write32(trans, FH_MEM_RCSR_CHNL0_RBDCB_WPTR, 0);
	iwl_write32(trans, FH_MEM_RCSR_CHNL0_FLUSH_RB_REQ, 0);
	iwl_write32(trans, FH_RSCSR_CHNL0_RDPTR, 0);

	 
	iwl_write32(trans, FH_RSCSR_CHNL0_RBDCB_WPTR_REG, 0);

	 
	iwl_write32(trans, FH_RSCSR_CHNL0_RBDCB_BASE_REG,
		    (u32)(rxq->bd_dma >> 8));

	 
	iwl_write32(trans, FH_RSCSR_CHNL0_STTS_WPTR_REG,
		    rxq->rb_stts_dma >> 4);

	 
	iwl_write32(trans, FH_MEM_RCSR_CHNL0_CONFIG_REG,
		    FH_RCSR_RX_CONFIG_CHNL_EN_ENABLE_VAL |
		    FH_RCSR_CHNL0_RX_IGNORE_RXF_EMPTY |
		    FH_RCSR_CHNL0_RX_CONFIG_IRQ_DEST_INT_HOST_VAL |
		    rb_size |
		    (RX_RB_TIMEOUT << FH_RCSR_RX_CONFIG_REG_IRQ_RBTH_POS) |
		    (rfdnlog << FH_RCSR_RX_CONFIG_RBDCB_SIZE_POS));

	iwl_trans_release_nic_access(trans);

	 
	iwl_write8(trans, CSR_INT_COALESCING, IWL_HOST_INT_TIMEOUT_DEF);

	 
	if (trans->cfg->host_interrupt_operation_mode)
		iwl_set_bit(trans, CSR_INT_COALESCING, IWL_HOST_INT_OPER_MODE);
}

static void iwl_pcie_rx_mq_hw_init(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	u32 rb_size, enabled = 0;
	int i;

	switch (trans_pcie->rx_buf_size) {
	case IWL_AMSDU_2K:
		rb_size = RFH_RXF_DMA_RB_SIZE_2K;
		break;
	case IWL_AMSDU_4K:
		rb_size = RFH_RXF_DMA_RB_SIZE_4K;
		break;
	case IWL_AMSDU_8K:
		rb_size = RFH_RXF_DMA_RB_SIZE_8K;
		break;
	case IWL_AMSDU_12K:
		rb_size = RFH_RXF_DMA_RB_SIZE_12K;
		break;
	default:
		WARN_ON(1);
		rb_size = RFH_RXF_DMA_RB_SIZE_4K;
	}

	if (!iwl_trans_grab_nic_access(trans))
		return;

	 
	iwl_write_prph_no_grab(trans, RFH_RXF_DMA_CFG, 0);
	 
	iwl_write_prph_no_grab(trans, RFH_RXF_RXQ_ACTIVE, 0);

	for (i = 0; i < trans->num_rx_queues; i++) {
		 
		iwl_write_prph64_no_grab(trans,
					 RFH_Q_FRBDCB_BA_LSB(i),
					 trans_pcie->rxq[i].bd_dma);
		 
		iwl_write_prph64_no_grab(trans,
					 RFH_Q_URBDCB_BA_LSB(i),
					 trans_pcie->rxq[i].used_bd_dma);
		 
		iwl_write_prph64_no_grab(trans,
					 RFH_Q_URBD_STTS_WPTR_LSB(i),
					 trans_pcie->rxq[i].rb_stts_dma);
		 
		iwl_write_prph_no_grab(trans, RFH_Q_FRBDCB_WIDX(i), 0);
		iwl_write_prph_no_grab(trans, RFH_Q_FRBDCB_RIDX(i), 0);
		iwl_write_prph_no_grab(trans, RFH_Q_URBDCB_WIDX(i), 0);

		enabled |= BIT(i) | BIT(i + 16);
	}

	 
	iwl_write_prph_no_grab(trans, RFH_RXF_DMA_CFG,
			       RFH_DMA_EN_ENABLE_VAL | rb_size |
			       RFH_RXF_DMA_MIN_RB_4_8 |
			       RFH_RXF_DMA_DROP_TOO_LARGE_MASK |
			       RFH_RXF_DMA_RBDCB_SIZE_512);

	 
	iwl_write_prph_no_grab(trans, RFH_GEN_CFG,
			       RFH_GEN_CFG_RFH_DMA_SNOOP |
			       RFH_GEN_CFG_VAL(DEFAULT_RXQ_NUM, 0) |
			       RFH_GEN_CFG_SERVICE_DMA_SNOOP |
			       RFH_GEN_CFG_VAL(RB_CHUNK_SIZE,
					       trans->trans_cfg->integrated ?
					       RFH_GEN_CFG_RB_CHUNK_SIZE_64 :
					       RFH_GEN_CFG_RB_CHUNK_SIZE_128));
	 
	iwl_write_prph_no_grab(trans, RFH_RXF_RXQ_ACTIVE, enabled);

	iwl_trans_release_nic_access(trans);

	 
	iwl_write8(trans, CSR_INT_COALESCING, IWL_HOST_INT_TIMEOUT_DEF);
}

void iwl_pcie_rx_init_rxb_lists(struct iwl_rxq *rxq)
{
	lockdep_assert_held(&rxq->lock);

	INIT_LIST_HEAD(&rxq->rx_free);
	INIT_LIST_HEAD(&rxq->rx_used);
	rxq->free_count = 0;
	rxq->used_count = 0;
}

static int iwl_pcie_rx_handle(struct iwl_trans *trans, int queue, int budget);

static int iwl_pcie_napi_poll(struct napi_struct *napi, int budget)
{
	struct iwl_rxq *rxq = container_of(napi, struct iwl_rxq, napi);
	struct iwl_trans_pcie *trans_pcie;
	struct iwl_trans *trans;
	int ret;

	trans_pcie = container_of(napi->dev, struct iwl_trans_pcie, napi_dev);
	trans = trans_pcie->trans;

	ret = iwl_pcie_rx_handle(trans, rxq->id, budget);

	IWL_DEBUG_ISR(trans, "[%d] handled %d, budget %d\n",
		      rxq->id, ret, budget);

	if (ret < budget) {
		spin_lock(&trans_pcie->irq_lock);
		if (test_bit(STATUS_INT_ENABLED, &trans->status))
			_iwl_enable_interrupts(trans);
		spin_unlock(&trans_pcie->irq_lock);

		napi_complete_done(&rxq->napi, ret);
	}

	return ret;
}

static int iwl_pcie_napi_poll_msix(struct napi_struct *napi, int budget)
{
	struct iwl_rxq *rxq = container_of(napi, struct iwl_rxq, napi);
	struct iwl_trans_pcie *trans_pcie;
	struct iwl_trans *trans;
	int ret;

	trans_pcie = container_of(napi->dev, struct iwl_trans_pcie, napi_dev);
	trans = trans_pcie->trans;

	ret = iwl_pcie_rx_handle(trans, rxq->id, budget);
	IWL_DEBUG_ISR(trans, "[%d] handled %d, budget %d\n", rxq->id, ret,
		      budget);

	if (ret < budget) {
		int irq_line = rxq->id;

		 
		if (trans_pcie->shared_vec_mask & IWL_SHARED_IRQ_FIRST_RSS &&
		    rxq->id == 1)
			irq_line = 0;

		spin_lock(&trans_pcie->irq_lock);
		iwl_pcie_clear_irq(trans, irq_line);
		spin_unlock(&trans_pcie->irq_lock);

		napi_complete_done(&rxq->napi, ret);
	}

	return ret;
}

void iwl_pcie_rx_napi_sync(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	int i;

	if (unlikely(!trans_pcie->rxq))
		return;

	for (i = 0; i < trans->num_rx_queues; i++) {
		struct iwl_rxq *rxq = &trans_pcie->rxq[i];

		if (rxq && rxq->napi.poll)
			napi_synchronize(&rxq->napi);
	}
}

static int _iwl_pcie_rx_init(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rxq *def_rxq;
	struct iwl_rb_allocator *rba = &trans_pcie->rba;
	int i, err, queue_size, allocator_pool_size, num_alloc;

	if (!trans_pcie->rxq) {
		err = iwl_pcie_rx_alloc(trans);
		if (err)
			return err;
	}
	def_rxq = trans_pcie->rxq;

	cancel_work_sync(&rba->rx_alloc);

	spin_lock_bh(&rba->lock);
	atomic_set(&rba->req_pending, 0);
	atomic_set(&rba->req_ready, 0);
	INIT_LIST_HEAD(&rba->rbd_allocated);
	INIT_LIST_HEAD(&rba->rbd_empty);
	spin_unlock_bh(&rba->lock);

	 
	iwl_pcie_free_rbs_pool(trans);

	for (i = 0; i < RX_QUEUE_SIZE; i++)
		def_rxq->queue[i] = NULL;

	for (i = 0; i < trans->num_rx_queues; i++) {
		struct iwl_rxq *rxq = &trans_pcie->rxq[i];

		spin_lock_bh(&rxq->lock);
		 
		rxq->read = 0;
		rxq->write = 0;
		rxq->write_actual = 0;
		memset(rxq->rb_stts, 0,
		       (trans->trans_cfg->device_family >=
			IWL_DEVICE_FAMILY_AX210) ?
		       sizeof(__le16) : sizeof(struct iwl_rb_status));

		iwl_pcie_rx_init_rxb_lists(rxq);

		spin_unlock_bh(&rxq->lock);

		if (!rxq->napi.poll) {
			int (*poll)(struct napi_struct *, int) = iwl_pcie_napi_poll;

			if (trans_pcie->msix_enabled)
				poll = iwl_pcie_napi_poll_msix;

			netif_napi_add(&trans_pcie->napi_dev, &rxq->napi,
				       poll);
			napi_enable(&rxq->napi);
		}

	}

	 
	queue_size = trans->trans_cfg->mq_rx_supported ?
			trans_pcie->num_rx_bufs - 1 : RX_QUEUE_SIZE;
	allocator_pool_size = trans->num_rx_queues *
		(RX_CLAIM_REQ_ALLOC - RX_POST_REQ_ALLOC);
	num_alloc = queue_size + allocator_pool_size;

	for (i = 0; i < num_alloc; i++) {
		struct iwl_rx_mem_buffer *rxb = &trans_pcie->rx_pool[i];

		if (i < allocator_pool_size)
			list_add(&rxb->list, &rba->rbd_empty);
		else
			list_add(&rxb->list, &def_rxq->rx_used);
		trans_pcie->global_table[i] = rxb;
		rxb->vid = (u16)(i + 1);
		rxb->invalid = true;
	}

	iwl_pcie_rxq_alloc_rbs(trans, GFP_KERNEL, def_rxq);

	return 0;
}

int iwl_pcie_rx_init(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	int ret = _iwl_pcie_rx_init(trans);

	if (ret)
		return ret;

	if (trans->trans_cfg->mq_rx_supported)
		iwl_pcie_rx_mq_hw_init(trans);
	else
		iwl_pcie_rx_hw_init(trans, trans_pcie->rxq);

	iwl_pcie_rxq_restock(trans, trans_pcie->rxq);

	spin_lock_bh(&trans_pcie->rxq->lock);
	iwl_pcie_rxq_inc_wr_ptr(trans, trans_pcie->rxq);
	spin_unlock_bh(&trans_pcie->rxq->lock);

	return 0;
}

int iwl_pcie_gen2_rx_init(struct iwl_trans *trans)
{
	 
	iwl_write8(trans, CSR_INT_COALESCING, IWL_HOST_INT_TIMEOUT_DEF);

	 
	return _iwl_pcie_rx_init(trans);
}

void iwl_pcie_rx_free(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	size_t rb_stts_size = iwl_pcie_rb_stts_size(trans);
	struct iwl_rb_allocator *rba = &trans_pcie->rba;
	int i;

	 
	if (!trans_pcie->rxq) {
		IWL_DEBUG_INFO(trans, "Free NULL rx context\n");
		return;
	}

	cancel_work_sync(&rba->rx_alloc);

	iwl_pcie_free_rbs_pool(trans);

	if (trans_pcie->base_rb_stts) {
		dma_free_coherent(trans->dev,
				  rb_stts_size * trans->num_rx_queues,
				  trans_pcie->base_rb_stts,
				  trans_pcie->base_rb_stts_dma);
		trans_pcie->base_rb_stts = NULL;
		trans_pcie->base_rb_stts_dma = 0;
	}

	for (i = 0; i < trans->num_rx_queues; i++) {
		struct iwl_rxq *rxq = &trans_pcie->rxq[i];

		iwl_pcie_free_rxq_dma(trans, rxq);

		if (rxq->napi.poll) {
			napi_disable(&rxq->napi);
			netif_napi_del(&rxq->napi);
		}
	}
	kfree(trans_pcie->rx_pool);
	kfree(trans_pcie->global_table);
	kfree(trans_pcie->rxq);

	if (trans_pcie->alloc_page)
		__free_pages(trans_pcie->alloc_page, trans_pcie->rx_page_order);
}

static void iwl_pcie_rx_move_to_allocator(struct iwl_rxq *rxq,
					  struct iwl_rb_allocator *rba)
{
	spin_lock(&rba->lock);
	list_splice_tail_init(&rxq->rx_used, &rba->rbd_empty);
	spin_unlock(&rba->lock);
}

 
static void iwl_pcie_rx_reuse_rbd(struct iwl_trans *trans,
				  struct iwl_rx_mem_buffer *rxb,
				  struct iwl_rxq *rxq, bool emergency)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rb_allocator *rba = &trans_pcie->rba;

	 
	list_add_tail(&rxb->list, &rxq->rx_used);

	if (unlikely(emergency))
		return;

	 
	rxq->used_count++;

	 
	if ((rxq->used_count % RX_CLAIM_REQ_ALLOC) == RX_POST_REQ_ALLOC) {
		 
		iwl_pcie_rx_move_to_allocator(rxq, rba);

		atomic_inc(&rba->req_pending);
		queue_work(rba->alloc_wq, &rba->rx_alloc);
	}
}

static void iwl_pcie_rx_handle_rb(struct iwl_trans *trans,
				struct iwl_rxq *rxq,
				struct iwl_rx_mem_buffer *rxb,
				bool emergency,
				int i)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_txq *txq = trans->txqs.txq[trans->txqs.cmd.q_id];
	bool page_stolen = false;
	int max_len = trans_pcie->rx_buf_bytes;
	u32 offset = 0;

	if (WARN_ON(!rxb))
		return;

	dma_unmap_page(trans->dev, rxb->page_dma, max_len, DMA_FROM_DEVICE);

	while (offset + sizeof(u32) + sizeof(struct iwl_cmd_header) < max_len) {
		struct iwl_rx_packet *pkt;
		bool reclaim;
		int len;
		struct iwl_rx_cmd_buffer rxcb = {
			._offset = rxb->offset + offset,
			._rx_page_order = trans_pcie->rx_page_order,
			._page = rxb->page,
			._page_stolen = false,
			.truesize = max_len,
		};

		pkt = rxb_addr(&rxcb);

		if (pkt->len_n_flags == cpu_to_le32(FH_RSCSR_FRAME_INVALID)) {
			IWL_DEBUG_RX(trans,
				     "Q %d: RB end marker at offset %d\n",
				     rxq->id, offset);
			break;
		}

		WARN((le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_RXQ_MASK) >>
			FH_RSCSR_RXQ_POS != rxq->id,
		     "frame on invalid queue - is on %d and indicates %d\n",
		     rxq->id,
		     (le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_RXQ_MASK) >>
			FH_RSCSR_RXQ_POS);

		IWL_DEBUG_RX(trans,
			     "Q %d: cmd at offset %d: %s (%.2x.%2x, seq 0x%x)\n",
			     rxq->id, offset,
			     iwl_get_cmd_string(trans,
						WIDE_ID(pkt->hdr.group_id, pkt->hdr.cmd)),
			     pkt->hdr.group_id, pkt->hdr.cmd,
			     le16_to_cpu(pkt->hdr.sequence));

		len = iwl_rx_packet_len(pkt);
		len += sizeof(u32);  

		offset += ALIGN(len, FH_RSCSR_FRAME_ALIGN);

		 
		if (len < sizeof(*pkt) || offset > max_len)
			break;

		trace_iwlwifi_dev_rx(trans->dev, trans, pkt, len);
		trace_iwlwifi_dev_rx_data(trans->dev, trans, pkt, len);

		 
		reclaim = !(pkt->hdr.sequence & SEQ_RX_FRAME);
		if (reclaim && !pkt->hdr.group_id) {
			int i;

			for (i = 0; i < trans_pcie->n_no_reclaim_cmds; i++) {
				if (trans_pcie->no_reclaim_cmds[i] ==
							pkt->hdr.cmd) {
					reclaim = false;
					break;
				}
			}
		}

		if (rxq->id == IWL_DEFAULT_RX_QUEUE)
			iwl_op_mode_rx(trans->op_mode, &rxq->napi,
				       &rxcb);
		else
			iwl_op_mode_rx_rss(trans->op_mode, &rxq->napi,
					   &rxcb, rxq->id);

		 

		if (reclaim && txq) {
			u16 sequence = le16_to_cpu(pkt->hdr.sequence);
			int index = SEQ_TO_INDEX(sequence);
			int cmd_index = iwl_txq_get_cmd_index(txq, index);

			kfree_sensitive(txq->entries[cmd_index].free_buf);
			txq->entries[cmd_index].free_buf = NULL;

			 
			if (!rxcb._page_stolen)
				iwl_pcie_hcmd_complete(trans, &rxcb);
			else
				IWL_WARN(trans, "Claim null rxb?\n");
		}

		page_stolen |= rxcb._page_stolen;
		if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210)
			break;
	}

	 
	if (page_stolen) {
		__free_pages(rxb->page, trans_pcie->rx_page_order);
		rxb->page = NULL;
	}

	 
	if (rxb->page != NULL) {
		rxb->page_dma =
			dma_map_page(trans->dev, rxb->page, rxb->offset,
				     trans_pcie->rx_buf_bytes,
				     DMA_FROM_DEVICE);
		if (dma_mapping_error(trans->dev, rxb->page_dma)) {
			 
			__free_pages(rxb->page, trans_pcie->rx_page_order);
			rxb->page = NULL;
			iwl_pcie_rx_reuse_rbd(trans, rxb, rxq, emergency);
		} else {
			list_add_tail(&rxb->list, &rxq->rx_free);
			rxq->free_count++;
		}
	} else
		iwl_pcie_rx_reuse_rbd(trans, rxb, rxq, emergency);
}

static struct iwl_rx_mem_buffer *iwl_pcie_get_rxb(struct iwl_trans *trans,
						  struct iwl_rxq *rxq, int i,
						  bool *join)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rx_mem_buffer *rxb;
	u16 vid;

	BUILD_BUG_ON(sizeof(struct iwl_rx_completion_desc) != 32);
	BUILD_BUG_ON(sizeof(struct iwl_rx_completion_desc_bz) != 4);

	if (!trans->trans_cfg->mq_rx_supported) {
		rxb = rxq->queue[i];
		rxq->queue[i] = NULL;
		return rxb;
	}

	if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_BZ) {
		struct iwl_rx_completion_desc_bz *cd = rxq->used_bd;

		vid = le16_to_cpu(cd[i].rbid);
		*join = cd[i].flags & IWL_RX_CD_FLAGS_FRAGMENTED;
	} else if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210) {
		struct iwl_rx_completion_desc *cd = rxq->used_bd;

		vid = le16_to_cpu(cd[i].rbid);
		*join = cd[i].flags & IWL_RX_CD_FLAGS_FRAGMENTED;
	} else {
		__le32 *cd = rxq->used_bd;

		vid = le32_to_cpu(cd[i]) & 0x0FFF;  
	}

	if (!vid || vid > RX_POOL_SIZE(trans_pcie->num_rx_bufs))
		goto out_err;

	rxb = trans_pcie->global_table[vid - 1];
	if (rxb->invalid)
		goto out_err;

	IWL_DEBUG_RX(trans, "Got virtual RB ID %u\n", (u32)rxb->vid);

	rxb->invalid = true;

	return rxb;

out_err:
	WARN(1, "Invalid rxb from HW %u\n", (u32)vid);
	iwl_force_nmi(trans);
	return NULL;
}

 
static int iwl_pcie_rx_handle(struct iwl_trans *trans, int queue, int budget)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct iwl_rxq *rxq;
	u32 r, i, count = 0, handled = 0;
	bool emergency = false;

	if (WARN_ON_ONCE(!trans_pcie->rxq || !trans_pcie->rxq[queue].bd))
		return budget;

	rxq = &trans_pcie->rxq[queue];

restart:
	spin_lock(&rxq->lock);
	 
	r = le16_to_cpu(iwl_get_closed_rb_stts(trans, rxq)) & 0x0FFF;
	i = rxq->read;

	 
	r &= (rxq->queue_size - 1);

	 
	if (i == r)
		IWL_DEBUG_RX(trans, "Q %d: HW = SW = %d\n", rxq->id, r);

	while (i != r && ++handled < budget) {
		struct iwl_rb_allocator *rba = &trans_pcie->rba;
		struct iwl_rx_mem_buffer *rxb;
		 
		u32 rb_pending_alloc =
			atomic_read(&trans_pcie->rba.req_pending) *
			RX_CLAIM_REQ_ALLOC;
		bool join = false;

		if (unlikely(rb_pending_alloc >= rxq->queue_size / 2 &&
			     !emergency)) {
			iwl_pcie_rx_move_to_allocator(rxq, rba);
			emergency = true;
			IWL_DEBUG_TPT(trans,
				      "RX path is in emergency. Pending allocations %d\n",
				      rb_pending_alloc);
		}

		IWL_DEBUG_RX(trans, "Q %d: HW = %d, SW = %d\n", rxq->id, r, i);

		rxb = iwl_pcie_get_rxb(trans, rxq, i, &join);
		if (!rxb)
			goto out;

		if (unlikely(join || rxq->next_rb_is_fragment)) {
			rxq->next_rb_is_fragment = join;
			 
			list_add_tail(&rxb->list, &rxq->rx_free);
			rxq->free_count++;
		} else {
			iwl_pcie_rx_handle_rb(trans, rxq, rxb, emergency, i);
		}

		i = (i + 1) & (rxq->queue_size - 1);

		 
		if (rxq->used_count >= RX_CLAIM_REQ_ALLOC)
			iwl_pcie_rx_allocator_get(trans, rxq);

		if (rxq->used_count % RX_CLAIM_REQ_ALLOC == 0 && !emergency) {
			 
			iwl_pcie_rx_move_to_allocator(rxq, rba);
		} else if (emergency) {
			count++;
			if (count == 8) {
				count = 0;
				if (rb_pending_alloc < rxq->queue_size / 3) {
					IWL_DEBUG_TPT(trans,
						      "RX path exited emergency. Pending allocations %d\n",
						      rb_pending_alloc);
					emergency = false;
				}

				rxq->read = i;
				spin_unlock(&rxq->lock);
				iwl_pcie_rxq_alloc_rbs(trans, GFP_ATOMIC, rxq);
				iwl_pcie_rxq_restock(trans, rxq);
				goto restart;
			}
		}
	}
out:
	 
	rxq->read = i;
	spin_unlock(&rxq->lock);

	 
	if (unlikely(emergency && count))
		iwl_pcie_rxq_alloc_rbs(trans, GFP_ATOMIC, rxq);

	iwl_pcie_rxq_restock(trans, rxq);

	return handled;
}

static struct iwl_trans_pcie *iwl_pcie_get_trans_pcie(struct msix_entry *entry)
{
	u8 queue = entry->entry;
	struct msix_entry *entries = entry - queue;

	return container_of(entries, struct iwl_trans_pcie, msix_entries[0]);
}

 
irqreturn_t iwl_pcie_irq_rx_msix_handler(int irq, void *dev_id)
{
	struct msix_entry *entry = dev_id;
	struct iwl_trans_pcie *trans_pcie = iwl_pcie_get_trans_pcie(entry);
	struct iwl_trans *trans = trans_pcie->trans;
	struct iwl_rxq *rxq;

	trace_iwlwifi_dev_irq_msix(trans->dev, entry, false, 0, 0);

	if (WARN_ON(entry->entry >= trans->num_rx_queues))
		return IRQ_NONE;

	if (!trans_pcie->rxq) {
		if (net_ratelimit())
			IWL_ERR(trans,
				"[%d] Got MSI-X interrupt before we have Rx queues\n",
				entry->entry);
		return IRQ_NONE;
	}

	rxq = &trans_pcie->rxq[entry->entry];
	lock_map_acquire(&trans->sync_cmd_lockdep_map);
	IWL_DEBUG_ISR(trans, "[%d] Got interrupt\n", entry->entry);

	local_bh_disable();
	if (napi_schedule_prep(&rxq->napi))
		__napi_schedule(&rxq->napi);
	else
		iwl_pcie_clear_irq(trans, entry->entry);
	local_bh_enable();

	lock_map_release(&trans->sync_cmd_lockdep_map);

	return IRQ_HANDLED;
}

 
static void iwl_pcie_irq_handle_error(struct iwl_trans *trans)
{
	int i;

	 
	if (trans->cfg->internal_wimax_coex &&
	    !trans->cfg->apmg_not_supported &&
	    (!(iwl_read_prph(trans, APMG_CLK_CTRL_REG) &
			     APMS_CLK_VAL_MRB_FUNC_MODE) ||
	     (iwl_read_prph(trans, APMG_PS_CTRL_REG) &
			    APMG_PS_CTRL_VAL_RESET_REQ))) {
		clear_bit(STATUS_SYNC_HCMD_ACTIVE, &trans->status);
		iwl_op_mode_wimax_active(trans->op_mode);
		wake_up(&trans->wait_command_queue);
		return;
	}

	for (i = 0; i < trans->trans_cfg->base_params->num_of_queues; i++) {
		if (!trans->txqs.txq[i])
			continue;
		del_timer(&trans->txqs.txq[i]->stuck_timer);
	}

	 
	iwl_trans_fw_error(trans, false);

	clear_bit(STATUS_SYNC_HCMD_ACTIVE, &trans->status);
	wake_up(&trans->wait_command_queue);
}

static u32 iwl_pcie_int_cause_non_ict(struct iwl_trans *trans)
{
	u32 inta;

	lockdep_assert_held(&IWL_TRANS_GET_PCIE_TRANS(trans)->irq_lock);

	trace_iwlwifi_dev_irq(trans->dev);

	 
	inta = iwl_read32(trans, CSR_INT);

	 
	return inta;
}

 
#define ICT_SHIFT	12
#define ICT_SIZE	(1 << ICT_SHIFT)
#define ICT_COUNT	(ICT_SIZE / sizeof(u32))

 
static u32 iwl_pcie_int_cause_ict(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	u32 inta;
	u32 val = 0;
	u32 read;

	trace_iwlwifi_dev_irq(trans->dev);

	 
	read = le32_to_cpu(trans_pcie->ict_tbl[trans_pcie->ict_index]);
	trace_iwlwifi_dev_ict_read(trans->dev, trans_pcie->ict_index, read);
	if (!read)
		return 0;

	 
	do {
		val |= read;
		IWL_DEBUG_ISR(trans, "ICT index %d value 0x%08X\n",
				trans_pcie->ict_index, read);
		trans_pcie->ict_tbl[trans_pcie->ict_index] = 0;
		trans_pcie->ict_index =
			((trans_pcie->ict_index + 1) & (ICT_COUNT - 1));

		read = le32_to_cpu(trans_pcie->ict_tbl[trans_pcie->ict_index]);
		trace_iwlwifi_dev_ict_read(trans->dev, trans_pcie->ict_index,
					   read);
	} while (read);

	 
	if (val == 0xffffffff)
		val = 0;

	 
	if (val & 0xC0000)
		val |= 0x8000;

	inta = (0xff & val) | ((0xff00 & val) << 16);
	return inta;
}

void iwl_pcie_handle_rfkill_irq(struct iwl_trans *trans, bool from_irq)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct isr_statistics *isr_stats = &trans_pcie->isr_stats;
	bool hw_rfkill, prev, report;

	mutex_lock(&trans_pcie->mutex);
	prev = test_bit(STATUS_RFKILL_OPMODE, &trans->status);
	hw_rfkill = iwl_is_rfkill_set(trans);
	if (hw_rfkill) {
		set_bit(STATUS_RFKILL_OPMODE, &trans->status);
		set_bit(STATUS_RFKILL_HW, &trans->status);
	}
	if (trans_pcie->opmode_down)
		report = hw_rfkill;
	else
		report = test_bit(STATUS_RFKILL_OPMODE, &trans->status);

	IWL_WARN(trans, "RF_KILL bit toggled to %s.\n",
		 hw_rfkill ? "disable radio" : "enable radio");

	isr_stats->rfkill++;

	if (prev != report)
		iwl_trans_pcie_rf_kill(trans, report, from_irq);
	mutex_unlock(&trans_pcie->mutex);

	if (hw_rfkill) {
		if (test_and_clear_bit(STATUS_SYNC_HCMD_ACTIVE,
				       &trans->status))
			IWL_DEBUG_RF_KILL(trans,
					  "Rfkill while SYNC HCMD in flight\n");
		wake_up(&trans->wait_command_queue);
	} else {
		clear_bit(STATUS_RFKILL_HW, &trans->status);
		if (trans_pcie->opmode_down)
			clear_bit(STATUS_RFKILL_OPMODE, &trans->status);
	}
}

irqreturn_t iwl_pcie_irq_handler(int irq, void *dev_id)
{
	struct iwl_trans *trans = dev_id;
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	struct isr_statistics *isr_stats = &trans_pcie->isr_stats;
	u32 inta = 0;
	u32 handled = 0;
	bool polling = false;

	lock_map_acquire(&trans->sync_cmd_lockdep_map);

	spin_lock_bh(&trans_pcie->irq_lock);

	 
	if (likely(trans_pcie->use_ict))
		inta = iwl_pcie_int_cause_ict(trans);
	else
		inta = iwl_pcie_int_cause_non_ict(trans);

	if (iwl_have_debug_level(IWL_DL_ISR)) {
		IWL_DEBUG_ISR(trans,
			      "ISR inta 0x%08x, enabled 0x%08x(sw), enabled(hw) 0x%08x, fh 0x%08x\n",
			      inta, trans_pcie->inta_mask,
			      iwl_read32(trans, CSR_INT_MASK),
			      iwl_read32(trans, CSR_FH_INT_STATUS));
		if (inta & (~trans_pcie->inta_mask))
			IWL_DEBUG_ISR(trans,
				      "We got a masked interrupt (0x%08x)\n",
				      inta & (~trans_pcie->inta_mask));
	}

	inta &= trans_pcie->inta_mask;

	 
	if (unlikely(!inta)) {
		IWL_DEBUG_ISR(trans, "Ignore interrupt, inta == 0\n");
		 
		if (test_bit(STATUS_INT_ENABLED, &trans->status))
			_iwl_enable_interrupts(trans);
		spin_unlock_bh(&trans_pcie->irq_lock);
		lock_map_release(&trans->sync_cmd_lockdep_map);
		return IRQ_NONE;
	}

	if (unlikely(inta == 0xFFFFFFFF || iwl_trans_is_hw_error_value(inta))) {
		 
		IWL_WARN(trans, "HARDWARE GONE?? INTA == 0x%08x\n", inta);
		spin_unlock_bh(&trans_pcie->irq_lock);
		goto out;
	}

	 
	 
	iwl_write32(trans, CSR_INT, inta | ~trans_pcie->inta_mask);

	if (iwl_have_debug_level(IWL_DL_ISR))
		IWL_DEBUG_ISR(trans, "inta 0x%08x, enabled 0x%08x\n",
			      inta, iwl_read32(trans, CSR_INT_MASK));

	spin_unlock_bh(&trans_pcie->irq_lock);

	 
	if (inta & CSR_INT_BIT_HW_ERR) {
		IWL_ERR(trans, "Hardware error detected.  Restarting.\n");

		 
		iwl_disable_interrupts(trans);

		isr_stats->hw++;
		iwl_pcie_irq_handle_error(trans);

		handled |= CSR_INT_BIT_HW_ERR;

		goto out;
	}

	 
	if (inta & CSR_INT_BIT_SCD) {
		IWL_DEBUG_ISR(trans,
			      "Scheduler finished to transmit the frame/frames.\n");
		isr_stats->sch++;
	}

	 
	if (inta & CSR_INT_BIT_ALIVE) {
		IWL_DEBUG_ISR(trans, "Alive interrupt\n");
		isr_stats->alive++;
		if (trans->trans_cfg->gen2) {
			 
			iwl_pcie_rxmq_restock(trans, trans_pcie->rxq);
		}

		handled |= CSR_INT_BIT_ALIVE;
	}

	 
	inta &= ~(CSR_INT_BIT_SCD | CSR_INT_BIT_ALIVE);

	 
	if (inta & CSR_INT_BIT_RF_KILL) {
		iwl_pcie_handle_rfkill_irq(trans, true);
		handled |= CSR_INT_BIT_RF_KILL;
	}

	 
	if (inta & CSR_INT_BIT_CT_KILL) {
		IWL_ERR(trans, "Microcode CT kill error detected.\n");
		isr_stats->ctkill++;
		handled |= CSR_INT_BIT_CT_KILL;
	}

	 
	if (inta & CSR_INT_BIT_SW_ERR) {
		IWL_ERR(trans, "Microcode SW error detected. "
			" Restarting 0x%X.\n", inta);
		isr_stats->sw++;
		iwl_pcie_irq_handle_error(trans);
		handled |= CSR_INT_BIT_SW_ERR;
	}

	 
	if (inta & CSR_INT_BIT_WAKEUP) {
		IWL_DEBUG_ISR(trans, "Wakeup interrupt\n");
		iwl_pcie_rxq_check_wrptr(trans);
		iwl_pcie_txq_check_wrptrs(trans);

		isr_stats->wakeup++;

		handled |= CSR_INT_BIT_WAKEUP;
	}

	 
	if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX |
		    CSR_INT_BIT_RX_PERIODIC)) {
		IWL_DEBUG_ISR(trans, "Rx interrupt\n");
		if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX)) {
			handled |= (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX);
			iwl_write32(trans, CSR_FH_INT_STATUS,
					CSR_FH_INT_RX_MASK);
		}
		if (inta & CSR_INT_BIT_RX_PERIODIC) {
			handled |= CSR_INT_BIT_RX_PERIODIC;
			iwl_write32(trans,
				CSR_INT, CSR_INT_BIT_RX_PERIODIC);
		}
		 

		 
		iwl_write8(trans, CSR_INT_PERIODIC_REG,
			    CSR_INT_PERIODIC_DIS);

		 
		if (inta & (CSR_INT_BIT_FH_RX | CSR_INT_BIT_SW_RX))
			iwl_write8(trans, CSR_INT_PERIODIC_REG,
				   CSR_INT_PERIODIC_ENA);

		isr_stats->rx++;

		local_bh_disable();
		if (napi_schedule_prep(&trans_pcie->rxq[0].napi)) {
			polling = true;
			__napi_schedule(&trans_pcie->rxq[0].napi);
		}
		local_bh_enable();
	}

	 
	if (inta & CSR_INT_BIT_FH_TX) {
		iwl_write32(trans, CSR_FH_INT_STATUS, CSR_FH_INT_TX_MASK);
		IWL_DEBUG_ISR(trans, "uCode load interrupt\n");
		isr_stats->tx++;
		handled |= CSR_INT_BIT_FH_TX;
		 
		trans_pcie->ucode_write_complete = true;
		wake_up(&trans_pcie->ucode_write_waitq);
		 
		if (trans_pcie->imr_status == IMR_D2S_REQUESTED) {
			trans_pcie->imr_status = IMR_D2S_COMPLETED;
			wake_up(&trans_pcie->ucode_write_waitq);
		}
	}

	if (inta & ~handled) {
		IWL_ERR(trans, "Unhandled INTA bits 0x%08x\n", inta & ~handled);
		isr_stats->unhandled++;
	}

	if (inta & ~(trans_pcie->inta_mask)) {
		IWL_WARN(trans, "Disabled INTA bits 0x%08x were pending\n",
			 inta & ~trans_pcie->inta_mask);
	}

	if (!polling) {
		spin_lock_bh(&trans_pcie->irq_lock);
		 
		if (test_bit(STATUS_INT_ENABLED, &trans->status))
			_iwl_enable_interrupts(trans);
		 
		else if (handled & CSR_INT_BIT_FH_TX)
			iwl_enable_fw_load_int(trans);
		 
		else if (handled & CSR_INT_BIT_RF_KILL)
			iwl_enable_rfkill_int(trans);
		 
		else if (handled & (CSR_INT_BIT_ALIVE | CSR_INT_BIT_FH_RX))
			iwl_enable_fw_load_int_ctx_info(trans);
		spin_unlock_bh(&trans_pcie->irq_lock);
	}

out:
	lock_map_release(&trans->sync_cmd_lockdep_map);
	return IRQ_HANDLED;
}

 

 
void iwl_pcie_free_ict(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);

	if (trans_pcie->ict_tbl) {
		dma_free_coherent(trans->dev, ICT_SIZE,
				  trans_pcie->ict_tbl,
				  trans_pcie->ict_tbl_dma);
		trans_pcie->ict_tbl = NULL;
		trans_pcie->ict_tbl_dma = 0;
	}
}

 
int iwl_pcie_alloc_ict(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);

	trans_pcie->ict_tbl =
		dma_alloc_coherent(trans->dev, ICT_SIZE,
				   &trans_pcie->ict_tbl_dma, GFP_KERNEL);
	if (!trans_pcie->ict_tbl)
		return -ENOMEM;

	 
	if (WARN_ON(trans_pcie->ict_tbl_dma & (ICT_SIZE - 1))) {
		iwl_pcie_free_ict(trans);
		return -EINVAL;
	}

	return 0;
}

 
void iwl_pcie_reset_ict(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);
	u32 val;

	if (!trans_pcie->ict_tbl)
		return;

	spin_lock_bh(&trans_pcie->irq_lock);
	_iwl_disable_interrupts(trans);

	memset(trans_pcie->ict_tbl, 0, ICT_SIZE);

	val = trans_pcie->ict_tbl_dma >> ICT_SHIFT;

	val |= CSR_DRAM_INT_TBL_ENABLE |
	       CSR_DRAM_INIT_TBL_WRAP_CHECK |
	       CSR_DRAM_INIT_TBL_WRITE_POINTER;

	IWL_DEBUG_ISR(trans, "CSR_DRAM_INT_TBL_REG =0x%x\n", val);

	iwl_write32(trans, CSR_DRAM_INT_TBL_REG, val);
	trans_pcie->use_ict = true;
	trans_pcie->ict_index = 0;
	iwl_write32(trans, CSR_INT, trans_pcie->inta_mask);
	_iwl_enable_interrupts(trans);
	spin_unlock_bh(&trans_pcie->irq_lock);
}

 
void iwl_pcie_disable_ict(struct iwl_trans *trans)
{
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);

	spin_lock_bh(&trans_pcie->irq_lock);
	trans_pcie->use_ict = false;
	spin_unlock_bh(&trans_pcie->irq_lock);
}

irqreturn_t iwl_pcie_isr(int irq, void *data)
{
	struct iwl_trans *trans = data;

	if (!trans)
		return IRQ_NONE;

	 
	iwl_write32(trans, CSR_INT_MASK, 0x00000000);

	return IRQ_WAKE_THREAD;
}

irqreturn_t iwl_pcie_msix_isr(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

irqreturn_t iwl_pcie_irq_msix_handler(int irq, void *dev_id)
{
	struct msix_entry *entry = dev_id;
	struct iwl_trans_pcie *trans_pcie = iwl_pcie_get_trans_pcie(entry);
	struct iwl_trans *trans = trans_pcie->trans;
	struct isr_statistics *isr_stats = &trans_pcie->isr_stats;
	u32 inta_fh_msk = ~MSIX_FH_INT_CAUSES_DATA_QUEUE;
	u32 inta_fh, inta_hw;
	bool polling = false;
	bool sw_err;

	if (trans_pcie->shared_vec_mask & IWL_SHARED_IRQ_NON_RX)
		inta_fh_msk |= MSIX_FH_INT_CAUSES_Q0;

	if (trans_pcie->shared_vec_mask & IWL_SHARED_IRQ_FIRST_RSS)
		inta_fh_msk |= MSIX_FH_INT_CAUSES_Q1;

	lock_map_acquire(&trans->sync_cmd_lockdep_map);

	spin_lock_bh(&trans_pcie->irq_lock);
	inta_fh = iwl_read32(trans, CSR_MSIX_FH_INT_CAUSES_AD);
	inta_hw = iwl_read32(trans, CSR_MSIX_HW_INT_CAUSES_AD);
	 
	iwl_write32(trans, CSR_MSIX_FH_INT_CAUSES_AD, inta_fh & inta_fh_msk);
	iwl_write32(trans, CSR_MSIX_HW_INT_CAUSES_AD, inta_hw);
	spin_unlock_bh(&trans_pcie->irq_lock);

	trace_iwlwifi_dev_irq_msix(trans->dev, entry, true, inta_fh, inta_hw);

	if (unlikely(!(inta_fh | inta_hw))) {
		IWL_DEBUG_ISR(trans, "Ignore interrupt, inta == 0\n");
		lock_map_release(&trans->sync_cmd_lockdep_map);
		return IRQ_NONE;
	}

	if (iwl_have_debug_level(IWL_DL_ISR)) {
		IWL_DEBUG_ISR(trans,
			      "ISR[%d] inta_fh 0x%08x, enabled (sw) 0x%08x (hw) 0x%08x\n",
			      entry->entry, inta_fh, trans_pcie->fh_mask,
			      iwl_read32(trans, CSR_MSIX_FH_INT_MASK_AD));
		if (inta_fh & ~trans_pcie->fh_mask)
			IWL_DEBUG_ISR(trans,
				      "We got a masked interrupt (0x%08x)\n",
				      inta_fh & ~trans_pcie->fh_mask);
	}

	inta_fh &= trans_pcie->fh_mask;

	if ((trans_pcie->shared_vec_mask & IWL_SHARED_IRQ_NON_RX) &&
	    inta_fh & MSIX_FH_INT_CAUSES_Q0) {
		local_bh_disable();
		if (napi_schedule_prep(&trans_pcie->rxq[0].napi)) {
			polling = true;
			__napi_schedule(&trans_pcie->rxq[0].napi);
		}
		local_bh_enable();
	}

	if ((trans_pcie->shared_vec_mask & IWL_SHARED_IRQ_FIRST_RSS) &&
	    inta_fh & MSIX_FH_INT_CAUSES_Q1) {
		local_bh_disable();
		if (napi_schedule_prep(&trans_pcie->rxq[1].napi)) {
			polling = true;
			__napi_schedule(&trans_pcie->rxq[1].napi);
		}
		local_bh_enable();
	}

	 
	if (inta_fh & MSIX_FH_INT_CAUSES_D2S_CH0_NUM &&
	    trans_pcie->imr_status == IMR_D2S_REQUESTED) {
		IWL_DEBUG_ISR(trans, "IMR Complete interrupt\n");
		isr_stats->tx++;

		 
		if (trans_pcie->imr_status == IMR_D2S_REQUESTED) {
			trans_pcie->imr_status = IMR_D2S_COMPLETED;
			wake_up(&trans_pcie->ucode_write_waitq);
		}
	} else if (inta_fh & MSIX_FH_INT_CAUSES_D2S_CH0_NUM) {
		IWL_DEBUG_ISR(trans, "uCode load interrupt\n");
		isr_stats->tx++;
		 
		trans_pcie->ucode_write_complete = true;
		wake_up(&trans_pcie->ucode_write_waitq);

		 
		if (trans_pcie->imr_status == IMR_D2S_REQUESTED) {
			trans_pcie->imr_status = IMR_D2S_COMPLETED;
			wake_up(&trans_pcie->ucode_write_waitq);
		}
	}

	if (trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_BZ)
		sw_err = inta_hw & MSIX_HW_INT_CAUSES_REG_SW_ERR_BZ;
	else
		sw_err = inta_hw & MSIX_HW_INT_CAUSES_REG_SW_ERR;

	 
	if ((inta_fh & MSIX_FH_INT_CAUSES_FH_ERR) || sw_err) {
		IWL_ERR(trans,
			"Microcode SW error detected. Restarting 0x%X.\n",
			inta_fh);
		isr_stats->sw++;
		 
		if (trans_pcie->imr_status == IMR_D2S_REQUESTED) {
			trans_pcie->imr_status = IMR_D2S_ERROR;
			wake_up(&trans_pcie->imr_waitq);
		} else if (trans_pcie->fw_reset_state == FW_RESET_REQUESTED) {
			trans_pcie->fw_reset_state = FW_RESET_ERROR;
			wake_up(&trans_pcie->fw_reset_waitq);
		} else {
			iwl_pcie_irq_handle_error(trans);
		}
	}

	 
	if (iwl_have_debug_level(IWL_DL_ISR)) {
		IWL_DEBUG_ISR(trans,
			      "ISR[%d] inta_hw 0x%08x, enabled (sw) 0x%08x (hw) 0x%08x\n",
			      entry->entry, inta_hw, trans_pcie->hw_mask,
			      iwl_read32(trans, CSR_MSIX_HW_INT_MASK_AD));
		if (inta_hw & ~trans_pcie->hw_mask)
			IWL_DEBUG_ISR(trans,
				      "We got a masked interrupt 0x%08x\n",
				      inta_hw & ~trans_pcie->hw_mask);
	}

	inta_hw &= trans_pcie->hw_mask;

	 
	if (inta_hw & MSIX_HW_INT_CAUSES_REG_ALIVE) {
		IWL_DEBUG_ISR(trans, "Alive interrupt\n");
		isr_stats->alive++;
		if (trans->trans_cfg->gen2) {
			 
			iwl_pcie_rxmq_restock(trans, trans_pcie->rxq);
		}
	}

	 
	if (inta_hw & MSIX_HW_INT_CAUSES_REG_WAKEUP && trans_pcie->prph_info) {
		u32 sleep_notif =
			le32_to_cpu(trans_pcie->prph_info->sleep_notif);
		if (sleep_notif == IWL_D3_SLEEP_STATUS_SUSPEND ||
		    sleep_notif == IWL_D3_SLEEP_STATUS_RESUME) {
			IWL_DEBUG_ISR(trans,
				      "Sx interrupt: sleep notification = 0x%x\n",
				      sleep_notif);
			trans_pcie->sx_complete = true;
			wake_up(&trans_pcie->sx_waitq);
		} else {
			 
			IWL_DEBUG_ISR(trans, "Wakeup interrupt\n");
			iwl_pcie_rxq_check_wrptr(trans);
			iwl_pcie_txq_check_wrptrs(trans);

			isr_stats->wakeup++;
		}
	}

	 
	if (inta_hw & MSIX_HW_INT_CAUSES_REG_CT_KILL) {
		IWL_ERR(trans, "Microcode CT kill error detected.\n");
		isr_stats->ctkill++;
	}

	 
	if (inta_hw & MSIX_HW_INT_CAUSES_REG_RF_KILL)
		iwl_pcie_handle_rfkill_irq(trans, true);

	if (inta_hw & MSIX_HW_INT_CAUSES_REG_HW_ERR) {
		IWL_ERR(trans,
			"Hardware error detected. Restarting.\n");

		isr_stats->hw++;
		trans->dbg.hw_error = true;
		iwl_pcie_irq_handle_error(trans);
	}

	if (inta_hw & MSIX_HW_INT_CAUSES_REG_RESET_DONE) {
		IWL_DEBUG_ISR(trans, "Reset flow completed\n");
		trans_pcie->fw_reset_state = FW_RESET_OK;
		wake_up(&trans_pcie->fw_reset_waitq);
	}

	if (!polling)
		iwl_pcie_clear_irq(trans, entry->entry);

	lock_map_release(&trans->sync_cmd_lockdep_map);

	return IRQ_HANDLED;
}
