
 

#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define UNIPHIER_FI2C_CR	0x00	 
#define     UNIPHIER_FI2C_CR_MST	BIT(3)	 
#define     UNIPHIER_FI2C_CR_STA	BIT(2)	 
#define     UNIPHIER_FI2C_CR_STO	BIT(1)	 
#define     UNIPHIER_FI2C_CR_NACK	BIT(0)	 
#define UNIPHIER_FI2C_DTTX	0x04	 
#define     UNIPHIER_FI2C_DTTX_CMD	BIT(8)	 
#define     UNIPHIER_FI2C_DTTX_RD	BIT(0)	 
#define UNIPHIER_FI2C_DTRX	0x04	 
#define UNIPHIER_FI2C_SLAD	0x0c	 
#define UNIPHIER_FI2C_CYC	0x10	 
#define UNIPHIER_FI2C_LCTL	0x14	 
#define UNIPHIER_FI2C_SSUT	0x18	 
#define UNIPHIER_FI2C_DSUT	0x1c	 
#define UNIPHIER_FI2C_INT	0x20	 
#define UNIPHIER_FI2C_IE	0x24	 
#define UNIPHIER_FI2C_IC	0x28	 
#define     UNIPHIER_FI2C_INT_TE	BIT(9)	 
#define     UNIPHIER_FI2C_INT_RF	BIT(8)	 
#define     UNIPHIER_FI2C_INT_TC	BIT(7)	 
#define     UNIPHIER_FI2C_INT_RC	BIT(6)	 
#define     UNIPHIER_FI2C_INT_TB	BIT(5)	 
#define     UNIPHIER_FI2C_INT_RB	BIT(4)	 
#define     UNIPHIER_FI2C_INT_NA	BIT(2)	 
#define     UNIPHIER_FI2C_INT_AL	BIT(1)	 
#define UNIPHIER_FI2C_SR	0x2c	 
#define     UNIPHIER_FI2C_SR_DB		BIT(12)	 
#define     UNIPHIER_FI2C_SR_STS	BIT(11)	 
#define     UNIPHIER_FI2C_SR_BB		BIT(8)	 
#define     UNIPHIER_FI2C_SR_RFF	BIT(3)	 
#define     UNIPHIER_FI2C_SR_RNE	BIT(2)	 
#define     UNIPHIER_FI2C_SR_TNF	BIT(1)	 
#define     UNIPHIER_FI2C_SR_TFE	BIT(0)	 
#define UNIPHIER_FI2C_RST	0x34	 
#define     UNIPHIER_FI2C_RST_TBRST	BIT(2)	 
#define     UNIPHIER_FI2C_RST_RBRST	BIT(1)	 
#define     UNIPHIER_FI2C_RST_RST	BIT(0)	 
#define UNIPHIER_FI2C_BM	0x38	 
#define     UNIPHIER_FI2C_BM_SDAO	BIT(3)	 
#define     UNIPHIER_FI2C_BM_SDAS	BIT(2)	 
#define     UNIPHIER_FI2C_BM_SCLO	BIT(1)	 
#define     UNIPHIER_FI2C_BM_SCLS	BIT(0)	 
#define UNIPHIER_FI2C_NOISE	0x3c	 
#define UNIPHIER_FI2C_TBC	0x40	 
#define UNIPHIER_FI2C_RBC	0x44	 
#define UNIPHIER_FI2C_TBCM	0x48	 
#define UNIPHIER_FI2C_RBCM	0x4c	 
#define UNIPHIER_FI2C_BRST	0x50	 
#define     UNIPHIER_FI2C_BRST_FOEN	BIT(1)	 
#define     UNIPHIER_FI2C_BRST_RSCL	BIT(0)	 

#define UNIPHIER_FI2C_INT_FAULTS	\
				(UNIPHIER_FI2C_INT_NA | UNIPHIER_FI2C_INT_AL)
#define UNIPHIER_FI2C_INT_STOP		\
				(UNIPHIER_FI2C_INT_TC | UNIPHIER_FI2C_INT_RC)

#define UNIPHIER_FI2C_RD		BIT(0)
#define UNIPHIER_FI2C_STOP		BIT(1)
#define UNIPHIER_FI2C_MANUAL_NACK	BIT(2)
#define UNIPHIER_FI2C_BYTE_WISE		BIT(3)
#define UNIPHIER_FI2C_DEFER_STOP_COMP	BIT(4)

#define UNIPHIER_FI2C_FIFO_SIZE		8

struct uniphier_fi2c_priv {
	struct completion comp;
	struct i2c_adapter adap;
	void __iomem *membase;
	struct clk *clk;
	unsigned int len;
	u8 *buf;
	u32 enabled_irqs;
	int error;
	unsigned int flags;
	unsigned int busy_cnt;
	unsigned int clk_cycle;
	spinlock_t lock;	 
};

static void uniphier_fi2c_fill_txfifo(struct uniphier_fi2c_priv *priv,
				      bool first)
{
	int fifo_space = UNIPHIER_FI2C_FIFO_SIZE;

	 
	if (first)
		fifo_space--;

	while (priv->len) {
		if (fifo_space-- <= 0)
			break;

		writel(*priv->buf++, priv->membase + UNIPHIER_FI2C_DTTX);
		priv->len--;
	}
}

static void uniphier_fi2c_drain_rxfifo(struct uniphier_fi2c_priv *priv)
{
	int fifo_left = priv->flags & UNIPHIER_FI2C_BYTE_WISE ?
						1 : UNIPHIER_FI2C_FIFO_SIZE;

	while (priv->len) {
		if (fifo_left-- <= 0)
			break;

		*priv->buf++ = readl(priv->membase + UNIPHIER_FI2C_DTRX);
		priv->len--;
	}
}

static void uniphier_fi2c_set_irqs(struct uniphier_fi2c_priv *priv)
{
	writel(priv->enabled_irqs, priv->membase + UNIPHIER_FI2C_IE);
}

static void uniphier_fi2c_clear_irqs(struct uniphier_fi2c_priv *priv,
				     u32 mask)
{
	writel(mask, priv->membase + UNIPHIER_FI2C_IC);
}

static void uniphier_fi2c_stop(struct uniphier_fi2c_priv *priv)
{
	priv->enabled_irqs |= UNIPHIER_FI2C_INT_STOP;
	uniphier_fi2c_set_irqs(priv);
	writel(UNIPHIER_FI2C_CR_MST | UNIPHIER_FI2C_CR_STO,
	       priv->membase + UNIPHIER_FI2C_CR);
}

static irqreturn_t uniphier_fi2c_interrupt(int irq, void *dev_id)
{
	struct uniphier_fi2c_priv *priv = dev_id;
	u32 irq_status;

	spin_lock(&priv->lock);

	irq_status = readl(priv->membase + UNIPHIER_FI2C_INT);
	irq_status &= priv->enabled_irqs;

	if (irq_status & UNIPHIER_FI2C_INT_STOP)
		goto complete;

	if (unlikely(irq_status & UNIPHIER_FI2C_INT_AL)) {
		priv->error = -EAGAIN;
		goto complete;
	}

	if (unlikely(irq_status & UNIPHIER_FI2C_INT_NA)) {
		priv->error = -ENXIO;
		if (priv->flags & UNIPHIER_FI2C_RD) {
			 
			uniphier_fi2c_stop(priv);
			priv->flags |= UNIPHIER_FI2C_DEFER_STOP_COMP;
			goto complete;
		}
		goto stop;
	}

	if (irq_status & UNIPHIER_FI2C_INT_TE) {
		if (!priv->len)
			goto data_done;

		uniphier_fi2c_fill_txfifo(priv, false);
		goto handled;
	}

	if (irq_status & (UNIPHIER_FI2C_INT_RF | UNIPHIER_FI2C_INT_RB)) {
		uniphier_fi2c_drain_rxfifo(priv);
		 
		if (!priv->len && (irq_status & UNIPHIER_FI2C_INT_RB))
			goto data_done;

		if (unlikely(priv->flags & UNIPHIER_FI2C_MANUAL_NACK)) {
			if (priv->len <= UNIPHIER_FI2C_FIFO_SIZE &&
			    !(priv->flags & UNIPHIER_FI2C_BYTE_WISE)) {
				priv->enabled_irqs |= UNIPHIER_FI2C_INT_RB;
				uniphier_fi2c_set_irqs(priv);
				priv->flags |= UNIPHIER_FI2C_BYTE_WISE;
			}
			if (priv->len <= 1)
				writel(UNIPHIER_FI2C_CR_MST |
				       UNIPHIER_FI2C_CR_NACK,
				       priv->membase + UNIPHIER_FI2C_CR);
		}

		goto handled;
	}

	spin_unlock(&priv->lock);

	return IRQ_NONE;

data_done:
	if (priv->flags & UNIPHIER_FI2C_STOP) {
stop:
		uniphier_fi2c_stop(priv);
	} else {
complete:
		priv->enabled_irqs = 0;
		uniphier_fi2c_set_irqs(priv);
		complete(&priv->comp);
	}

handled:
	 
	uniphier_fi2c_clear_irqs(priv, irq_status);

	spin_unlock(&priv->lock);

	return IRQ_HANDLED;
}

static void uniphier_fi2c_tx_init(struct uniphier_fi2c_priv *priv, u16 addr,
				  bool repeat)
{
	priv->enabled_irqs |= UNIPHIER_FI2C_INT_TE;
	uniphier_fi2c_set_irqs(priv);

	 
	writel(0, priv->membase + UNIPHIER_FI2C_TBC);
	 
	writel(UNIPHIER_FI2C_DTTX_CMD | addr << 1,
	       priv->membase + UNIPHIER_FI2C_DTTX);
	 
	if (!repeat)
		uniphier_fi2c_fill_txfifo(priv, true);
}

static void uniphier_fi2c_rx_init(struct uniphier_fi2c_priv *priv, u16 addr)
{
	priv->flags |= UNIPHIER_FI2C_RD;

	if (likely(priv->len < 256)) {
		 
		writel(priv->len, priv->membase + UNIPHIER_FI2C_RBC);
		priv->enabled_irqs |= UNIPHIER_FI2C_INT_RF |
				      UNIPHIER_FI2C_INT_RB;
	} else {
		 
		writel(0, priv->membase + UNIPHIER_FI2C_RBC);
		priv->flags |= UNIPHIER_FI2C_MANUAL_NACK;
		priv->enabled_irqs |= UNIPHIER_FI2C_INT_RF;
	}

	uniphier_fi2c_set_irqs(priv);

	 
	writel(UNIPHIER_FI2C_DTTX_CMD | UNIPHIER_FI2C_DTTX_RD | addr << 1,
	       priv->membase + UNIPHIER_FI2C_DTTX);
}

static void uniphier_fi2c_reset(struct uniphier_fi2c_priv *priv)
{
	writel(UNIPHIER_FI2C_RST_RST, priv->membase + UNIPHIER_FI2C_RST);
}

static void uniphier_fi2c_prepare_operation(struct uniphier_fi2c_priv *priv)
{
	writel(UNIPHIER_FI2C_BRST_FOEN | UNIPHIER_FI2C_BRST_RSCL,
	       priv->membase + UNIPHIER_FI2C_BRST);
}

static void uniphier_fi2c_recover(struct uniphier_fi2c_priv *priv)
{
	uniphier_fi2c_reset(priv);
	i2c_recover_bus(&priv->adap);
}

static int uniphier_fi2c_master_xfer_one(struct i2c_adapter *adap,
					 struct i2c_msg *msg, bool repeat,
					 bool stop)
{
	struct uniphier_fi2c_priv *priv = i2c_get_adapdata(adap);
	bool is_read = msg->flags & I2C_M_RD;
	unsigned long time_left, flags;

	priv->len = msg->len;
	priv->buf = msg->buf;
	priv->enabled_irqs = UNIPHIER_FI2C_INT_FAULTS;
	priv->error = 0;
	priv->flags = 0;

	if (stop)
		priv->flags |= UNIPHIER_FI2C_STOP;

	reinit_completion(&priv->comp);
	uniphier_fi2c_clear_irqs(priv, U32_MAX);
	writel(UNIPHIER_FI2C_RST_TBRST | UNIPHIER_FI2C_RST_RBRST,
	       priv->membase + UNIPHIER_FI2C_RST);	 

	spin_lock_irqsave(&priv->lock, flags);

	if (is_read)
		uniphier_fi2c_rx_init(priv, msg->addr);
	else
		uniphier_fi2c_tx_init(priv, msg->addr, repeat);

	 
	if (!repeat)
		writel(UNIPHIER_FI2C_CR_MST | UNIPHIER_FI2C_CR_STA,
		       priv->membase + UNIPHIER_FI2C_CR);

	spin_unlock_irqrestore(&priv->lock, flags);

	time_left = wait_for_completion_timeout(&priv->comp, adap->timeout);

	spin_lock_irqsave(&priv->lock, flags);
	priv->enabled_irqs = 0;
	uniphier_fi2c_set_irqs(priv);
	spin_unlock_irqrestore(&priv->lock, flags);

	if (!time_left) {
		dev_err(&adap->dev, "transaction timeout.\n");
		uniphier_fi2c_recover(priv);
		return -ETIMEDOUT;
	}

	if (unlikely(priv->flags & UNIPHIER_FI2C_DEFER_STOP_COMP)) {
		u32 status;
		int ret;

		ret = readl_poll_timeout(priv->membase + UNIPHIER_FI2C_SR,
					 status,
					 (status & UNIPHIER_FI2C_SR_STS) &&
					 !(status & UNIPHIER_FI2C_SR_BB),
					 1, 20);
		if (ret) {
			dev_err(&adap->dev,
				"stop condition was not completed.\n");
			uniphier_fi2c_recover(priv);
			return ret;
		}
	}

	return priv->error;
}

static int uniphier_fi2c_check_bus_busy(struct i2c_adapter *adap)
{
	struct uniphier_fi2c_priv *priv = i2c_get_adapdata(adap);

	if (readl(priv->membase + UNIPHIER_FI2C_SR) & UNIPHIER_FI2C_SR_DB) {
		if (priv->busy_cnt++ > 3) {
			 
			uniphier_fi2c_recover(priv);
			priv->busy_cnt = 0;
		}

		return -EAGAIN;
	}

	priv->busy_cnt = 0;
	return 0;
}

static int uniphier_fi2c_master_xfer(struct i2c_adapter *adap,
				     struct i2c_msg *msgs, int num)
{
	struct i2c_msg *msg, *emsg = msgs + num;
	bool repeat = false;
	int ret;

	ret = uniphier_fi2c_check_bus_busy(adap);
	if (ret)
		return ret;

	for (msg = msgs; msg < emsg; msg++) {
		 
		bool stop = (msg + 1 == emsg) || (msg->flags & I2C_M_STOP);

		ret = uniphier_fi2c_master_xfer_one(adap, msg, repeat, stop);
		if (ret)
			return ret;

		repeat = !stop;
	}

	return num;
}

static u32 uniphier_fi2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm uniphier_fi2c_algo = {
	.master_xfer = uniphier_fi2c_master_xfer,
	.functionality = uniphier_fi2c_functionality,
};

static int uniphier_fi2c_get_scl(struct i2c_adapter *adap)
{
	struct uniphier_fi2c_priv *priv = i2c_get_adapdata(adap);

	return !!(readl(priv->membase + UNIPHIER_FI2C_BM) &
							UNIPHIER_FI2C_BM_SCLS);
}

static void uniphier_fi2c_set_scl(struct i2c_adapter *adap, int val)
{
	struct uniphier_fi2c_priv *priv = i2c_get_adapdata(adap);

	writel(val ? UNIPHIER_FI2C_BRST_RSCL : 0,
	       priv->membase + UNIPHIER_FI2C_BRST);
}

static int uniphier_fi2c_get_sda(struct i2c_adapter *adap)
{
	struct uniphier_fi2c_priv *priv = i2c_get_adapdata(adap);

	return !!(readl(priv->membase + UNIPHIER_FI2C_BM) &
							UNIPHIER_FI2C_BM_SDAS);
}

static void uniphier_fi2c_unprepare_recovery(struct i2c_adapter *adap)
{
	uniphier_fi2c_prepare_operation(i2c_get_adapdata(adap));
}

static struct i2c_bus_recovery_info uniphier_fi2c_bus_recovery_info = {
	.recover_bus = i2c_generic_scl_recovery,
	.get_scl = uniphier_fi2c_get_scl,
	.set_scl = uniphier_fi2c_set_scl,
	.get_sda = uniphier_fi2c_get_sda,
	.unprepare_recovery = uniphier_fi2c_unprepare_recovery,
};

static void uniphier_fi2c_hw_init(struct uniphier_fi2c_priv *priv)
{
	unsigned int cyc = priv->clk_cycle;
	u32 tmp;

	tmp = readl(priv->membase + UNIPHIER_FI2C_CR);
	tmp |= UNIPHIER_FI2C_CR_MST;
	writel(tmp, priv->membase + UNIPHIER_FI2C_CR);

	uniphier_fi2c_reset(priv);

	 
	writel(cyc, priv->membase + UNIPHIER_FI2C_CYC);
	 
	writel(cyc * 5 / 9, priv->membase + UNIPHIER_FI2C_LCTL);
	 
	writel(cyc / 2, priv->membase + UNIPHIER_FI2C_SSUT);
	 
	writel(cyc / 16, priv->membase + UNIPHIER_FI2C_DSUT);

	uniphier_fi2c_prepare_operation(priv);
}

static int uniphier_fi2c_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct uniphier_fi2c_priv *priv;
	u32 bus_speed;
	unsigned long clk_rate;
	int irq, ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->membase = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->membase))
		return PTR_ERR(priv->membase);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	if (of_property_read_u32(dev->of_node, "clock-frequency", &bus_speed))
		bus_speed = I2C_MAX_STANDARD_MODE_FREQ;

	if (!bus_speed || bus_speed > I2C_MAX_FAST_MODE_FREQ) {
		dev_err(dev, "invalid clock-frequency %d\n", bus_speed);
		return -EINVAL;
	}

	priv->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "failed to enable clock\n");
		return PTR_ERR(priv->clk);
	}

	clk_rate = clk_get_rate(priv->clk);
	if (!clk_rate) {
		dev_err(dev, "input clock rate should not be zero\n");
		return -EINVAL;
	}

	priv->clk_cycle = clk_rate / bus_speed;
	init_completion(&priv->comp);
	spin_lock_init(&priv->lock);
	priv->adap.owner = THIS_MODULE;
	priv->adap.algo = &uniphier_fi2c_algo;
	priv->adap.dev.parent = dev;
	priv->adap.dev.of_node = dev->of_node;
	strscpy(priv->adap.name, "UniPhier FI2C", sizeof(priv->adap.name));
	priv->adap.bus_recovery_info = &uniphier_fi2c_bus_recovery_info;
	i2c_set_adapdata(&priv->adap, priv);
	platform_set_drvdata(pdev, priv);

	uniphier_fi2c_hw_init(priv);

	ret = devm_request_irq(dev, irq, uniphier_fi2c_interrupt, 0,
			       pdev->name, priv);
	if (ret) {
		dev_err(dev, "failed to request irq %d\n", irq);
		return ret;
	}

	return i2c_add_adapter(&priv->adap);
}

static void uniphier_fi2c_remove(struct platform_device *pdev)
{
	struct uniphier_fi2c_priv *priv = platform_get_drvdata(pdev);

	i2c_del_adapter(&priv->adap);
}

static int __maybe_unused uniphier_fi2c_suspend(struct device *dev)
{
	struct uniphier_fi2c_priv *priv = dev_get_drvdata(dev);

	clk_disable_unprepare(priv->clk);

	return 0;
}

static int __maybe_unused uniphier_fi2c_resume(struct device *dev)
{
	struct uniphier_fi2c_priv *priv = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return ret;

	uniphier_fi2c_hw_init(priv);

	return 0;
}

static const struct dev_pm_ops uniphier_fi2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(uniphier_fi2c_suspend, uniphier_fi2c_resume)
};

static const struct of_device_id uniphier_fi2c_match[] = {
	{ .compatible = "socionext,uniphier-fi2c" },
	{   }
};
MODULE_DEVICE_TABLE(of, uniphier_fi2c_match);

static struct platform_driver uniphier_fi2c_drv = {
	.probe  = uniphier_fi2c_probe,
	.remove_new = uniphier_fi2c_remove,
	.driver = {
		.name  = "uniphier-fi2c",
		.of_match_table = uniphier_fi2c_match,
		.pm = &uniphier_fi2c_pm_ops,
	},
};
module_platform_driver(uniphier_fi2c_drv);

MODULE_AUTHOR("Masahiro Yamada <yamada.masahiro@socionext.com>");
MODULE_DESCRIPTION("UniPhier FIFO-builtin I2C bus driver");
MODULE_LICENSE("GPL");
