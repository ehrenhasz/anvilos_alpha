
 

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/string.h>
#include <linux/scatterlist.h>

 

 
#define SLCR_FPGA_RST_CTRL_OFFSET	0x240
 
#define SLCR_LVL_SHFTR_EN_OFFSET	0x900

 

 
#define CTRL_OFFSET			0x00
 
#define LOCK_OFFSET			0x04
 
#define INT_STS_OFFSET			0x0c
 
#define INT_MASK_OFFSET			0x10
 
#define STATUS_OFFSET			0x14
 
#define DMA_SRC_ADDR_OFFSET		0x18
 
#define DMA_DST_ADDR_OFFSET		0x1c
 
#define DMA_SRC_LEN_OFFSET		0x20
 
#define DMA_DEST_LEN_OFFSET		0x24
 
#define UNLOCK_OFFSET			0x34
 
#define MCTRL_OFFSET			0x80

 

 
#define CTRL_PCFG_PROG_B_MASK		BIT(30)
 
#define CTRL_PCAP_PR_MASK		BIT(27)
 
#define CTRL_PCAP_MODE_MASK		BIT(26)
 
#define CTRL_PCAP_RATE_EN_MASK		BIT(25)
 
#define CTRL_SEC_EN_MASK		BIT(7)

 
 
#define MCTRL_PCAP_LPBK_MASK		BIT(4)

 

 
#define STATUS_DMA_Q_F			BIT(31)
#define STATUS_DMA_Q_E			BIT(30)
#define STATUS_PCFG_INIT_MASK		BIT(4)

 
 
#define IXR_DMA_DONE_MASK		BIT(13)
 
#define IXR_D_P_DONE_MASK		BIT(12)
  
#define IXR_PCFG_DONE_MASK		BIT(2)
#define IXR_ERROR_FLAGS_MASK		0x00F0C860
#define IXR_ALL_MASK			0xF8F7F87F

 

 
#define DMA_INVALID_ADDRESS		GENMASK(31, 0)
 
#define UNLOCK_MASK			0x757bdf0d
 
#define INIT_POLL_TIMEOUT		2500000
 
#define INIT_POLL_DELAY			20
 
#define DMA_SRC_LAST_TRANSFER		1
 
#define DMA_TIMEOUT_MS			5000

 
 
#define LVL_SHFTR_DISABLE_ALL_MASK	0x0
 
#define LVL_SHFTR_ENABLE_PS_TO_PL	0xa
 
#define LVL_SHFTR_ENABLE_PL_TO_PS	0xf
 
#define FPGA_RST_ALL_MASK		0xf
 
#define FPGA_RST_NONE_MASK		0x0

struct zynq_fpga_priv {
	int irq;
	struct clk *clk;

	void __iomem *io_base;
	struct regmap *slcr;

	spinlock_t dma_lock;
	unsigned int dma_elm;
	unsigned int dma_nelms;
	struct scatterlist *cur_sg;

	struct completion dma_done;
};

static inline void zynq_fpga_write(struct zynq_fpga_priv *priv, u32 offset,
				   u32 val)
{
	writel(val, priv->io_base + offset);
}

static inline u32 zynq_fpga_read(const struct zynq_fpga_priv *priv,
				 u32 offset)
{
	return readl(priv->io_base + offset);
}

#define zynq_fpga_poll_timeout(priv, addr, val, cond, sleep_us, timeout_us) \
	readl_poll_timeout(priv->io_base + addr, val, cond, sleep_us, \
			   timeout_us)

 
static inline void zynq_fpga_set_irq(struct zynq_fpga_priv *priv, u32 enable)
{
	zynq_fpga_write(priv, INT_MASK_OFFSET, ~enable);
}

 
static void zynq_step_dma(struct zynq_fpga_priv *priv)
{
	u32 addr;
	u32 len;
	bool first;

	first = priv->dma_elm == 0;
	while (priv->cur_sg) {
		 
		if (zynq_fpga_read(priv, STATUS_OFFSET) & STATUS_DMA_Q_F)
			break;

		addr = sg_dma_address(priv->cur_sg);
		len = sg_dma_len(priv->cur_sg);
		if (priv->dma_elm + 1 == priv->dma_nelms) {
			 
			addr |= DMA_SRC_LAST_TRANSFER;
			priv->cur_sg = NULL;
		} else {
			priv->cur_sg = sg_next(priv->cur_sg);
			priv->dma_elm++;
		}

		zynq_fpga_write(priv, DMA_SRC_ADDR_OFFSET, addr);
		zynq_fpga_write(priv, DMA_DST_ADDR_OFFSET, DMA_INVALID_ADDRESS);
		zynq_fpga_write(priv, DMA_SRC_LEN_OFFSET, len / 4);
		zynq_fpga_write(priv, DMA_DEST_LEN_OFFSET, 0);
	}

	 
	if (first && priv->cur_sg) {
		zynq_fpga_set_irq(priv,
				  IXR_DMA_DONE_MASK | IXR_ERROR_FLAGS_MASK);
	} else if (!priv->cur_sg) {
		 
		zynq_fpga_set_irq(priv,
				  IXR_D_P_DONE_MASK | IXR_ERROR_FLAGS_MASK);
	}
}

static irqreturn_t zynq_fpga_isr(int irq, void *data)
{
	struct zynq_fpga_priv *priv = data;
	u32 intr_status;

	 
	spin_lock(&priv->dma_lock);
	intr_status = zynq_fpga_read(priv, INT_STS_OFFSET);
	if (!(intr_status & IXR_ERROR_FLAGS_MASK) &&
	    (intr_status & IXR_DMA_DONE_MASK) && priv->cur_sg) {
		zynq_fpga_write(priv, INT_STS_OFFSET, IXR_DMA_DONE_MASK);
		zynq_step_dma(priv);
		spin_unlock(&priv->dma_lock);
		return IRQ_HANDLED;
	}
	spin_unlock(&priv->dma_lock);

	zynq_fpga_set_irq(priv, 0);
	complete(&priv->dma_done);

	return IRQ_HANDLED;
}

 
static bool zynq_fpga_has_sync(const u8 *buf, size_t count)
{
	for (; count >= 4; buf += 4, count -= 4)
		if (buf[0] == 0x66 && buf[1] == 0x55 && buf[2] == 0x99 &&
		    buf[3] == 0xaa)
			return true;
	return false;
}

static int zynq_fpga_ops_write_init(struct fpga_manager *mgr,
				    struct fpga_image_info *info,
				    const char *buf, size_t count)
{
	struct zynq_fpga_priv *priv;
	u32 ctrl, status;
	int err;

	priv = mgr->priv;

	err = clk_enable(priv->clk);
	if (err)
		return err;

	 
	if (info->flags & FPGA_MGR_ENCRYPTED_BITSTREAM) {
		ctrl = zynq_fpga_read(priv, CTRL_OFFSET);
		if (!(ctrl & CTRL_SEC_EN_MASK)) {
			dev_err(&mgr->dev,
				"System not secure, can't use encrypted bitstreams\n");
			err = -EINVAL;
			goto out_err;
		}
	}

	 
	if (!(info->flags & FPGA_MGR_PARTIAL_RECONFIG)) {
		if (!zynq_fpga_has_sync(buf, count)) {
			dev_err(&mgr->dev,
				"Invalid bitstream, could not find a sync word. Bitstream must be a byte swapped .bin file\n");
			err = -EINVAL;
			goto out_err;
		}

		 
		regmap_write(priv->slcr, SLCR_FPGA_RST_CTRL_OFFSET,
			     FPGA_RST_ALL_MASK);

		 
		regmap_write(priv->slcr, SLCR_LVL_SHFTR_EN_OFFSET,
			     LVL_SHFTR_DISABLE_ALL_MASK);
		 
		regmap_write(priv->slcr, SLCR_LVL_SHFTR_EN_OFFSET,
			     LVL_SHFTR_ENABLE_PS_TO_PL);

		 
		ctrl = zynq_fpga_read(priv, CTRL_OFFSET);
		ctrl |= CTRL_PCFG_PROG_B_MASK;

		zynq_fpga_write(priv, CTRL_OFFSET, ctrl);

		err = zynq_fpga_poll_timeout(priv, STATUS_OFFSET, status,
					     status & STATUS_PCFG_INIT_MASK,
					     INIT_POLL_DELAY,
					     INIT_POLL_TIMEOUT);
		if (err) {
			dev_err(&mgr->dev, "Timeout waiting for PCFG_INIT\n");
			goto out_err;
		}

		ctrl = zynq_fpga_read(priv, CTRL_OFFSET);
		ctrl &= ~CTRL_PCFG_PROG_B_MASK;

		zynq_fpga_write(priv, CTRL_OFFSET, ctrl);

		err = zynq_fpga_poll_timeout(priv, STATUS_OFFSET, status,
					     !(status & STATUS_PCFG_INIT_MASK),
					     INIT_POLL_DELAY,
					     INIT_POLL_TIMEOUT);
		if (err) {
			dev_err(&mgr->dev, "Timeout waiting for !PCFG_INIT\n");
			goto out_err;
		}

		ctrl = zynq_fpga_read(priv, CTRL_OFFSET);
		ctrl |= CTRL_PCFG_PROG_B_MASK;

		zynq_fpga_write(priv, CTRL_OFFSET, ctrl);

		err = zynq_fpga_poll_timeout(priv, STATUS_OFFSET, status,
					     status & STATUS_PCFG_INIT_MASK,
					     INIT_POLL_DELAY,
					     INIT_POLL_TIMEOUT);
		if (err) {
			dev_err(&mgr->dev, "Timeout waiting for PCFG_INIT\n");
			goto out_err;
		}
	}

	 
	ctrl = zynq_fpga_read(priv, CTRL_OFFSET);
	if (info->flags & FPGA_MGR_ENCRYPTED_BITSTREAM)
		zynq_fpga_write(priv, CTRL_OFFSET,
				(CTRL_PCAP_PR_MASK | CTRL_PCAP_MODE_MASK
				 | CTRL_PCAP_RATE_EN_MASK | ctrl));
	else
		zynq_fpga_write(priv, CTRL_OFFSET,
				(CTRL_PCAP_PR_MASK | CTRL_PCAP_MODE_MASK
				 | ctrl));


	 
	status = zynq_fpga_read(priv, STATUS_OFFSET);
	if ((status & STATUS_DMA_Q_F) ||
	    (status & STATUS_DMA_Q_E) != STATUS_DMA_Q_E) {
		dev_err(&mgr->dev, "DMA command queue not right\n");
		err = -EBUSY;
		goto out_err;
	}

	 
	ctrl = zynq_fpga_read(priv, MCTRL_OFFSET);
	zynq_fpga_write(priv, MCTRL_OFFSET, (~MCTRL_PCAP_LPBK_MASK & ctrl));

	clk_disable(priv->clk);

	return 0;

out_err:
	clk_disable(priv->clk);

	return err;
}

static int zynq_fpga_ops_write(struct fpga_manager *mgr, struct sg_table *sgt)
{
	struct zynq_fpga_priv *priv;
	const char *why;
	int err;
	u32 intr_status;
	unsigned long timeout;
	unsigned long flags;
	struct scatterlist *sg;
	int i;

	priv = mgr->priv;

	 
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		if ((sg->offset % 8) || (sg->length % 4)) {
			dev_err(&mgr->dev,
			    "Invalid bitstream, chunks must be aligned\n");
			return -EINVAL;
		}
	}

	priv->dma_nelms =
	    dma_map_sg(mgr->dev.parent, sgt->sgl, sgt->nents, DMA_TO_DEVICE);
	if (priv->dma_nelms == 0) {
		dev_err(&mgr->dev, "Unable to DMA map (TO_DEVICE)\n");
		return -ENOMEM;
	}

	 
	err = clk_enable(priv->clk);
	if (err)
		goto out_free;

	zynq_fpga_write(priv, INT_STS_OFFSET, IXR_ALL_MASK);
	reinit_completion(&priv->dma_done);

	 
	spin_lock_irqsave(&priv->dma_lock, flags);
	priv->dma_elm = 0;
	priv->cur_sg = sgt->sgl;
	zynq_step_dma(priv);
	spin_unlock_irqrestore(&priv->dma_lock, flags);

	timeout = wait_for_completion_timeout(&priv->dma_done,
					      msecs_to_jiffies(DMA_TIMEOUT_MS));

	spin_lock_irqsave(&priv->dma_lock, flags);
	zynq_fpga_set_irq(priv, 0);
	priv->cur_sg = NULL;
	spin_unlock_irqrestore(&priv->dma_lock, flags);

	intr_status = zynq_fpga_read(priv, INT_STS_OFFSET);
	zynq_fpga_write(priv, INT_STS_OFFSET, IXR_ALL_MASK);

	 

	if (intr_status & IXR_ERROR_FLAGS_MASK) {
		why = "DMA reported error";
		err = -EIO;
		goto out_report;
	}

	if (priv->cur_sg ||
	    !((intr_status & IXR_D_P_DONE_MASK) == IXR_D_P_DONE_MASK)) {
		if (timeout == 0)
			why = "DMA timed out";
		else
			why = "DMA did not complete";
		err = -EIO;
		goto out_report;
	}

	err = 0;
	goto out_clk;

out_report:
	dev_err(&mgr->dev,
		"%s: INT_STS:0x%x CTRL:0x%x LOCK:0x%x INT_MASK:0x%x STATUS:0x%x MCTRL:0x%x\n",
		why,
		intr_status,
		zynq_fpga_read(priv, CTRL_OFFSET),
		zynq_fpga_read(priv, LOCK_OFFSET),
		zynq_fpga_read(priv, INT_MASK_OFFSET),
		zynq_fpga_read(priv, STATUS_OFFSET),
		zynq_fpga_read(priv, MCTRL_OFFSET));

out_clk:
	clk_disable(priv->clk);

out_free:
	dma_unmap_sg(mgr->dev.parent, sgt->sgl, sgt->nents, DMA_TO_DEVICE);
	return err;
}

static int zynq_fpga_ops_write_complete(struct fpga_manager *mgr,
					struct fpga_image_info *info)
{
	struct zynq_fpga_priv *priv = mgr->priv;
	int err;
	u32 intr_status;

	err = clk_enable(priv->clk);
	if (err)
		return err;

	err = zynq_fpga_poll_timeout(priv, INT_STS_OFFSET, intr_status,
				     intr_status & IXR_PCFG_DONE_MASK,
				     INIT_POLL_DELAY,
				     INIT_POLL_TIMEOUT);

	 
	zynq_fpga_write(priv, CTRL_OFFSET,
			zynq_fpga_read(priv, CTRL_OFFSET) & ~CTRL_PCAP_PR_MASK);

	clk_disable(priv->clk);

	if (err)
		return err;

	 
	if (!(info->flags & FPGA_MGR_PARTIAL_RECONFIG)) {
		 
		regmap_write(priv->slcr, SLCR_LVL_SHFTR_EN_OFFSET,
			     LVL_SHFTR_ENABLE_PL_TO_PS);

		 
		regmap_write(priv->slcr, SLCR_FPGA_RST_CTRL_OFFSET,
			     FPGA_RST_NONE_MASK);
	}

	return 0;
}

static enum fpga_mgr_states zynq_fpga_ops_state(struct fpga_manager *mgr)
{
	int err;
	u32 intr_status;
	struct zynq_fpga_priv *priv;

	priv = mgr->priv;

	err = clk_enable(priv->clk);
	if (err)
		return FPGA_MGR_STATE_UNKNOWN;

	intr_status = zynq_fpga_read(priv, INT_STS_OFFSET);
	clk_disable(priv->clk);

	if (intr_status & IXR_PCFG_DONE_MASK)
		return FPGA_MGR_STATE_OPERATING;

	return FPGA_MGR_STATE_UNKNOWN;
}

static const struct fpga_manager_ops zynq_fpga_ops = {
	.initial_header_size = 128,
	.state = zynq_fpga_ops_state,
	.write_init = zynq_fpga_ops_write_init,
	.write_sg = zynq_fpga_ops_write,
	.write_complete = zynq_fpga_ops_write_complete,
};

static int zynq_fpga_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct zynq_fpga_priv *priv;
	struct fpga_manager *mgr;
	int err;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	spin_lock_init(&priv->dma_lock);

	priv->io_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->io_base))
		return PTR_ERR(priv->io_base);

	priv->slcr = syscon_regmap_lookup_by_phandle(dev->of_node,
		"syscon");
	if (IS_ERR(priv->slcr)) {
		dev_err(dev, "unable to get zynq-slcr regmap\n");
		return PTR_ERR(priv->slcr);
	}

	init_completion(&priv->dma_done);

	priv->irq = platform_get_irq(pdev, 0);
	if (priv->irq < 0)
		return priv->irq;

	priv->clk = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(priv->clk))
		return dev_err_probe(dev, PTR_ERR(priv->clk),
				     "input clock not found\n");

	err = clk_prepare_enable(priv->clk);
	if (err) {
		dev_err(dev, "unable to enable clock\n");
		return err;
	}

	 
	zynq_fpga_write(priv, UNLOCK_OFFSET, UNLOCK_MASK);

	zynq_fpga_set_irq(priv, 0);
	zynq_fpga_write(priv, INT_STS_OFFSET, IXR_ALL_MASK);
	err = devm_request_irq(dev, priv->irq, zynq_fpga_isr, 0, dev_name(dev),
			       priv);
	if (err) {
		dev_err(dev, "unable to request IRQ\n");
		clk_disable_unprepare(priv->clk);
		return err;
	}

	clk_disable(priv->clk);

	mgr = fpga_mgr_register(dev, "Xilinx Zynq FPGA Manager",
				&zynq_fpga_ops, priv);
	if (IS_ERR(mgr)) {
		dev_err(dev, "unable to register FPGA manager\n");
		clk_unprepare(priv->clk);
		return PTR_ERR(mgr);
	}

	platform_set_drvdata(pdev, mgr);

	return 0;
}

static int zynq_fpga_remove(struct platform_device *pdev)
{
	struct zynq_fpga_priv *priv;
	struct fpga_manager *mgr;

	mgr = platform_get_drvdata(pdev);
	priv = mgr->priv;

	fpga_mgr_unregister(mgr);

	clk_unprepare(priv->clk);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id zynq_fpga_of_match[] = {
	{ .compatible = "xlnx,zynq-devcfg-1.0", },
	{},
};

MODULE_DEVICE_TABLE(of, zynq_fpga_of_match);
#endif

static struct platform_driver zynq_fpga_driver = {
	.probe = zynq_fpga_probe,
	.remove = zynq_fpga_remove,
	.driver = {
		.name = "zynq_fpga_manager",
		.of_match_table = of_match_ptr(zynq_fpga_of_match),
	},
};

module_platform_driver(zynq_fpga_driver);

MODULE_AUTHOR("Moritz Fischer <moritz.fischer@ettus.com>");
MODULE_AUTHOR("Michal Simek <michal.simek@xilinx.com>");
MODULE_DESCRIPTION("Xilinx Zynq FPGA Manager");
MODULE_LICENSE("GPL v2");
