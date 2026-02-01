

 

#include <linux/skbuff.h>

#include "mlxbf_gige.h"
#include "mlxbf_gige_regs.h"

 
int mlxbf_gige_tx_init(struct mlxbf_gige *priv)
{
	size_t size;

	size = MLXBF_GIGE_TX_WQE_SZ * priv->tx_q_entries;
	priv->tx_wqe_base = dma_alloc_coherent(priv->dev, size,
					       &priv->tx_wqe_base_dma,
					       GFP_KERNEL);
	if (!priv->tx_wqe_base)
		return -ENOMEM;

	priv->tx_wqe_next = priv->tx_wqe_base;

	 
	writeq(priv->tx_wqe_base_dma, priv->base + MLXBF_GIGE_TX_WQ_BASE);

	 
	priv->tx_cc = dma_alloc_coherent(priv->dev, MLXBF_GIGE_TX_CC_SZ,
					 &priv->tx_cc_dma, GFP_KERNEL);
	if (!priv->tx_cc) {
		dma_free_coherent(priv->dev, size,
				  priv->tx_wqe_base, priv->tx_wqe_base_dma);
		return -ENOMEM;
	}

	 
	writeq(priv->tx_cc_dma, priv->base + MLXBF_GIGE_TX_CI_UPDATE_ADDRESS);

	writeq(ilog2(priv->tx_q_entries),
	       priv->base + MLXBF_GIGE_TX_WQ_SIZE_LOG2);

	priv->prev_tx_ci = 0;
	priv->tx_pi = 0;

	return 0;
}

 
void mlxbf_gige_tx_deinit(struct mlxbf_gige *priv)
{
	u64 *tx_wqe_addr;
	size_t size;
	int i;

	tx_wqe_addr = priv->tx_wqe_base;

	for (i = 0; i < priv->tx_q_entries; i++) {
		if (priv->tx_skb[i]) {
			dma_unmap_single(priv->dev, *tx_wqe_addr,
					 priv->tx_skb[i]->len, DMA_TO_DEVICE);
			dev_kfree_skb(priv->tx_skb[i]);
			priv->tx_skb[i] = NULL;
		}
		tx_wqe_addr += 2;
	}

	size = MLXBF_GIGE_TX_WQE_SZ * priv->tx_q_entries;
	dma_free_coherent(priv->dev, size,
			  priv->tx_wqe_base, priv->tx_wqe_base_dma);

	dma_free_coherent(priv->dev, MLXBF_GIGE_TX_CC_SZ,
			  priv->tx_cc, priv->tx_cc_dma);

	priv->tx_wqe_base = NULL;
	priv->tx_wqe_base_dma = 0;
	priv->tx_cc = NULL;
	priv->tx_cc_dma = 0;
	priv->tx_wqe_next = NULL;
	writeq(0, priv->base + MLXBF_GIGE_TX_WQ_BASE);
	writeq(0, priv->base + MLXBF_GIGE_TX_CI_UPDATE_ADDRESS);
}

 
static u16 mlxbf_gige_tx_buffs_avail(struct mlxbf_gige *priv)
{
	unsigned long flags;
	u16 avail;

	spin_lock_irqsave(&priv->lock, flags);

	if (priv->prev_tx_ci == priv->tx_pi)
		avail = priv->tx_q_entries - 1;
	else
		avail = ((priv->tx_q_entries + priv->prev_tx_ci - priv->tx_pi)
			  % priv->tx_q_entries) - 1;

	spin_unlock_irqrestore(&priv->lock, flags);

	return avail;
}

bool mlxbf_gige_handle_tx_complete(struct mlxbf_gige *priv)
{
	struct net_device_stats *stats;
	u16 tx_wqe_index;
	u64 *tx_wqe_addr;
	u64 tx_status;
	u16 tx_ci;

	tx_status = readq(priv->base + MLXBF_GIGE_TX_STATUS);
	if (tx_status & MLXBF_GIGE_TX_STATUS_DATA_FIFO_FULL)
		priv->stats.tx_fifo_full++;
	tx_ci = readq(priv->base + MLXBF_GIGE_TX_CONSUMER_INDEX);
	stats = &priv->netdev->stats;

	 
	for (; priv->prev_tx_ci != tx_ci; priv->prev_tx_ci++) {
		tx_wqe_index = priv->prev_tx_ci % priv->tx_q_entries;
		 
		tx_wqe_addr = priv->tx_wqe_base +
			       (tx_wqe_index * MLXBF_GIGE_TX_WQE_SZ_QWORDS);

		stats->tx_packets++;
		stats->tx_bytes += MLXBF_GIGE_TX_WQE_PKT_LEN(tx_wqe_addr);

		dma_unmap_single(priv->dev, *tx_wqe_addr,
				 priv->tx_skb[tx_wqe_index]->len, DMA_TO_DEVICE);
		dev_consume_skb_any(priv->tx_skb[tx_wqe_index]);
		priv->tx_skb[tx_wqe_index] = NULL;

		 
		mb();
	}

	 
	if (netif_queue_stopped(priv->netdev) &&
	    mlxbf_gige_tx_buffs_avail(priv))
		netif_wake_queue(priv->netdev);

	return true;
}

 
void mlxbf_gige_update_tx_wqe_next(struct mlxbf_gige *priv)
{
	 
	priv->tx_wqe_next += MLXBF_GIGE_TX_WQE_SZ_QWORDS;

	 
	 
	if (priv->tx_wqe_next == (priv->tx_wqe_base +
				  (priv->tx_q_entries * MLXBF_GIGE_TX_WQE_SZ_QWORDS)))
		priv->tx_wqe_next = priv->tx_wqe_base;
}

netdev_tx_t mlxbf_gige_start_xmit(struct sk_buff *skb,
				  struct net_device *netdev)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);
	long buff_addr, start_dma_page, end_dma_page;
	struct sk_buff *tx_skb;
	dma_addr_t tx_buf_dma;
	unsigned long flags;
	u64 *tx_wqe_addr;
	u64 word2;

	 
	if (skb->len > MLXBF_GIGE_DEFAULT_BUF_SZ || skb_linearize(skb)) {
		dev_kfree_skb(skb);
		netdev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	buff_addr = (long)skb->data;
	start_dma_page = buff_addr >> MLXBF_GIGE_DMA_PAGE_SHIFT;
	end_dma_page   = (buff_addr + skb->len - 1) >> MLXBF_GIGE_DMA_PAGE_SHIFT;

	 
	if (start_dma_page != end_dma_page) {
		 
		tx_skb = mlxbf_gige_alloc_skb(priv, skb->len,
					      &tx_buf_dma, DMA_TO_DEVICE);
		if (!tx_skb) {
			 
			dev_kfree_skb(skb);
			netdev->stats.tx_dropped++;
			return NETDEV_TX_OK;
		}

		skb_put_data(tx_skb, skb->data, skb->len);

		 
		dev_kfree_skb(skb);
	} else {
		tx_skb = skb;
		tx_buf_dma = dma_map_single(priv->dev, skb->data,
					    skb->len, DMA_TO_DEVICE);
		if (dma_mapping_error(priv->dev, tx_buf_dma)) {
			dev_kfree_skb(skb);
			netdev->stats.tx_dropped++;
			return NETDEV_TX_OK;
		}
	}

	 
	tx_wqe_addr = priv->tx_wqe_next;

	mlxbf_gige_update_tx_wqe_next(priv);

	 
	*tx_wqe_addr = tx_buf_dma;

	 
	word2 = tx_skb->len & MLXBF_GIGE_TX_WQE_PKT_LEN_MASK;

	 
	*(tx_wqe_addr + 1) = word2;

	spin_lock_irqsave(&priv->lock, flags);
	priv->tx_skb[priv->tx_pi % priv->tx_q_entries] = tx_skb;
	priv->tx_pi++;
	spin_unlock_irqrestore(&priv->lock, flags);

	if (!netdev_xmit_more()) {
		 
		wmb();
		writeq(priv->tx_pi, priv->base + MLXBF_GIGE_TX_PRODUCER_INDEX);
	}

	 
	if (!mlxbf_gige_tx_buffs_avail(priv)) {
		 
		netif_stop_queue(netdev);

		 
		napi_schedule(&priv->napi);
	}

	return NETDEV_TX_OK;
}
