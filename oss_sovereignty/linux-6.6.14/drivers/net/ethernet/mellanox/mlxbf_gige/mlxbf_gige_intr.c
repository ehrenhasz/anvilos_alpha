

 

#include <linux/interrupt.h>

#include "mlxbf_gige.h"
#include "mlxbf_gige_regs.h"

static irqreturn_t mlxbf_gige_error_intr(int irq, void *dev_id)
{
	struct mlxbf_gige *priv;
	u64 int_status;

	priv = dev_id;

	int_status = readq(priv->base + MLXBF_GIGE_INT_STATUS);

	if (int_status & MLXBF_GIGE_INT_STATUS_HW_ACCESS_ERROR)
		priv->stats.hw_access_errors++;

	if (int_status & MLXBF_GIGE_INT_STATUS_TX_CHECKSUM_INPUTS) {
		priv->stats.tx_invalid_checksums++;
		 
	}

	if (int_status & MLXBF_GIGE_INT_STATUS_TX_SMALL_FRAME_SIZE) {
		priv->stats.tx_small_frames++;
		 
	}

	if (int_status & MLXBF_GIGE_INT_STATUS_TX_PI_CI_EXCEED_WQ_SIZE)
		priv->stats.tx_index_errors++;

	if (int_status & MLXBF_GIGE_INT_STATUS_SW_CONFIG_ERROR)
		priv->stats.sw_config_errors++;

	if (int_status & MLXBF_GIGE_INT_STATUS_SW_ACCESS_ERROR)
		priv->stats.sw_access_errors++;

	 

	int_status &= ~MLXBF_GIGE_INT_STATUS_RX_RECEIVE_PACKET;

	writeq(int_status, priv->base + MLXBF_GIGE_INT_STATUS);

	return IRQ_HANDLED;
}

static irqreturn_t mlxbf_gige_rx_intr(int irq, void *dev_id)
{
	struct mlxbf_gige *priv;

	priv = dev_id;

	 

	napi_schedule(&priv->napi);

	return IRQ_HANDLED;
}

static irqreturn_t mlxbf_gige_llu_plu_intr(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

int mlxbf_gige_request_irqs(struct mlxbf_gige *priv)
{
	int err;

	err = request_irq(priv->error_irq, mlxbf_gige_error_intr, 0,
			  "mlxbf_gige_error", priv);
	if (err) {
		dev_err(priv->dev, "Request error_irq failure\n");
		return err;
	}

	err = request_irq(priv->rx_irq, mlxbf_gige_rx_intr, 0,
			  "mlxbf_gige_rx", priv);
	if (err) {
		dev_err(priv->dev, "Request rx_irq failure\n");
		goto free_error_irq;
	}

	err = request_irq(priv->llu_plu_irq, mlxbf_gige_llu_plu_intr, 0,
			  "mlxbf_gige_llu_plu", priv);
	if (err) {
		dev_err(priv->dev, "Request llu_plu_irq failure\n");
		goto free_rx_irq;
	}

	return 0;

free_rx_irq:
	free_irq(priv->rx_irq, priv);

free_error_irq:
	free_irq(priv->error_irq, priv);

	return err;
}

void mlxbf_gige_free_irqs(struct mlxbf_gige *priv)
{
	free_irq(priv->error_irq, priv);
	free_irq(priv->rx_irq, priv);
	free_irq(priv->llu_plu_irq, priv);
}
