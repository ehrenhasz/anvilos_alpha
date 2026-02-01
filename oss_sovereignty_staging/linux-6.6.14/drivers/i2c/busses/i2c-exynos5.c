
 

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/spinlock.h>

 

 
#define HSI2C_CTL		0x00
#define HSI2C_FIFO_CTL		0x04
#define HSI2C_TRAILIG_CTL	0x08
#define HSI2C_CLK_CTL		0x0C
#define HSI2C_CLK_SLOT		0x10
#define HSI2C_INT_ENABLE	0x20
#define HSI2C_INT_STATUS	0x24
#define HSI2C_ERR_STATUS	0x2C
#define HSI2C_FIFO_STATUS	0x30
#define HSI2C_TX_DATA		0x34
#define HSI2C_RX_DATA		0x38
#define HSI2C_CONF		0x40
#define HSI2C_AUTO_CONF		0x44
#define HSI2C_TIMEOUT		0x48
#define HSI2C_MANUAL_CMD	0x4C
#define HSI2C_TRANS_STATUS	0x50
#define HSI2C_TIMING_HS1	0x54
#define HSI2C_TIMING_HS2	0x58
#define HSI2C_TIMING_HS3	0x5C
#define HSI2C_TIMING_FS1	0x60
#define HSI2C_TIMING_FS2	0x64
#define HSI2C_TIMING_FS3	0x68
#define HSI2C_TIMING_SLA	0x6C
#define HSI2C_ADDR		0x70

 
#define HSI2C_FUNC_MODE_I2C			(1u << 0)
#define HSI2C_MASTER				(1u << 3)
#define HSI2C_RXCHON				(1u << 6)
#define HSI2C_TXCHON				(1u << 7)
#define HSI2C_SW_RST				(1u << 31)

 
#define HSI2C_RXFIFO_EN				(1u << 0)
#define HSI2C_TXFIFO_EN				(1u << 1)
#define HSI2C_RXFIFO_TRIGGER_LEVEL(x)		((x) << 4)
#define HSI2C_TXFIFO_TRIGGER_LEVEL(x)		((x) << 16)

 
#define HSI2C_TRAILING_COUNT			(0xf)

 
#define HSI2C_INT_TX_ALMOSTEMPTY_EN		(1u << 0)
#define HSI2C_INT_RX_ALMOSTFULL_EN		(1u << 1)
#define HSI2C_INT_TRAILING_EN			(1u << 6)

 
#define HSI2C_INT_TX_ALMOSTEMPTY		(1u << 0)
#define HSI2C_INT_RX_ALMOSTFULL			(1u << 1)
#define HSI2C_INT_TX_UNDERRUN			(1u << 2)
#define HSI2C_INT_TX_OVERRUN			(1u << 3)
#define HSI2C_INT_RX_UNDERRUN			(1u << 4)
#define HSI2C_INT_RX_OVERRUN			(1u << 5)
#define HSI2C_INT_TRAILING			(1u << 6)
#define HSI2C_INT_I2C				(1u << 9)

#define HSI2C_INT_TRANS_DONE			(1u << 7)
#define HSI2C_INT_TRANS_ABORT			(1u << 8)
#define HSI2C_INT_NO_DEV_ACK			(1u << 9)
#define HSI2C_INT_NO_DEV			(1u << 10)
#define HSI2C_INT_TIMEOUT			(1u << 11)
#define HSI2C_INT_I2C_TRANS			(HSI2C_INT_TRANS_DONE |	\
						HSI2C_INT_TRANS_ABORT |	\
						HSI2C_INT_NO_DEV_ACK |	\
						HSI2C_INT_NO_DEV |	\
						HSI2C_INT_TIMEOUT)

 
#define HSI2C_RX_FIFO_EMPTY			(1u << 24)
#define HSI2C_RX_FIFO_FULL			(1u << 23)
#define HSI2C_RX_FIFO_LVL(x)			((x >> 16) & 0x7f)
#define HSI2C_TX_FIFO_EMPTY			(1u << 8)
#define HSI2C_TX_FIFO_FULL			(1u << 7)
#define HSI2C_TX_FIFO_LVL(x)			((x >> 0) & 0x7f)

 
#define HSI2C_AUTO_MODE				(1u << 31)
#define HSI2C_10BIT_ADDR_MODE			(1u << 30)
#define HSI2C_HS_MODE				(1u << 29)

 
#define HSI2C_READ_WRITE			(1u << 16)
#define HSI2C_STOP_AFTER_TRANS			(1u << 17)
#define HSI2C_MASTER_RUN			(1u << 31)

 
#define HSI2C_TIMEOUT_EN			(1u << 31)
#define HSI2C_TIMEOUT_MASK			0xff

 
#define HSI2C_CMD_READ_DATA			(1u << 4)
#define HSI2C_CMD_SEND_STOP			(1u << 2)

 
#define HSI2C_MASTER_BUSY			(1u << 17)
#define HSI2C_SLAVE_BUSY			(1u << 16)

 
#define HSI2C_TIMEOUT_AUTO			(1u << 4)
#define HSI2C_NO_DEV				(1u << 3)
#define HSI2C_NO_DEV_ACK			(1u << 2)
#define HSI2C_TRANS_ABORT			(1u << 1)
#define HSI2C_TRANS_DONE			(1u << 0)

 
#define HSI2C_MASTER_ST_MASK			0xf
#define HSI2C_MASTER_ST_IDLE			0x0
#define HSI2C_MASTER_ST_START			0x1
#define HSI2C_MASTER_ST_RESTART			0x2
#define HSI2C_MASTER_ST_STOP			0x3
#define HSI2C_MASTER_ST_MASTER_ID		0x4
#define HSI2C_MASTER_ST_ADDR0			0x5
#define HSI2C_MASTER_ST_ADDR1			0x6
#define HSI2C_MASTER_ST_ADDR2			0x7
#define HSI2C_MASTER_ST_ADDR_SR			0x8
#define HSI2C_MASTER_ST_READ			0x9
#define HSI2C_MASTER_ST_WRITE			0xa
#define HSI2C_MASTER_ST_NO_ACK			0xb
#define HSI2C_MASTER_ST_LOSE			0xc
#define HSI2C_MASTER_ST_WAIT			0xd
#define HSI2C_MASTER_ST_WAIT_CMD		0xe

 
#define HSI2C_SLV_ADDR_SLV(x)			((x & 0x3ff) << 0)
#define HSI2C_SLV_ADDR_MAS(x)			((x & 0x3ff) << 10)
#define HSI2C_MASTER_ID(x)			((x & 0xff) << 24)
#define MASTER_ID(x)				((x & 0x7) + 0x08)

#define EXYNOS5_I2C_TIMEOUT (msecs_to_jiffies(100))

enum i2c_type_exynos {
	I2C_TYPE_EXYNOS5,
	I2C_TYPE_EXYNOS7,
	I2C_TYPE_EXYNOSAUTOV9,
};

struct exynos5_i2c {
	struct i2c_adapter	adap;

	struct i2c_msg		*msg;
	struct completion	msg_complete;
	unsigned int		msg_ptr;

	unsigned int		irq;

	void __iomem		*regs;
	struct clk		*clk;		 
	struct clk		*pclk;		 
	struct device		*dev;
	int			state;

	spinlock_t		lock;		 

	 
	int			trans_done;

	 
	unsigned int		op_clock;

	 
	const struct exynos_hsi2c_variant *variant;
};

 
struct exynos_hsi2c_variant {
	unsigned int		fifo_depth;
	enum i2c_type_exynos	hw;
};

static const struct exynos_hsi2c_variant exynos5250_hsi2c_data = {
	.fifo_depth	= 64,
	.hw		= I2C_TYPE_EXYNOS5,
};

static const struct exynos_hsi2c_variant exynos5260_hsi2c_data = {
	.fifo_depth	= 16,
	.hw		= I2C_TYPE_EXYNOS5,
};

static const struct exynos_hsi2c_variant exynos7_hsi2c_data = {
	.fifo_depth	= 16,
	.hw		= I2C_TYPE_EXYNOS7,
};

static const struct exynos_hsi2c_variant exynosautov9_hsi2c_data = {
	.fifo_depth	= 64,
	.hw		= I2C_TYPE_EXYNOSAUTOV9,
};

static const struct of_device_id exynos5_i2c_match[] = {
	{
		.compatible = "samsung,exynos5-hsi2c",
		.data = &exynos5250_hsi2c_data
	}, {
		.compatible = "samsung,exynos5250-hsi2c",
		.data = &exynos5250_hsi2c_data
	}, {
		.compatible = "samsung,exynos5260-hsi2c",
		.data = &exynos5260_hsi2c_data
	}, {
		.compatible = "samsung,exynos7-hsi2c",
		.data = &exynos7_hsi2c_data
	}, {
		.compatible = "samsung,exynosautov9-hsi2c",
		.data = &exynosautov9_hsi2c_data
	}, {},
};
MODULE_DEVICE_TABLE(of, exynos5_i2c_match);

static void exynos5_i2c_clr_pend_irq(struct exynos5_i2c *i2c)
{
	writel(readl(i2c->regs + HSI2C_INT_STATUS),
				i2c->regs + HSI2C_INT_STATUS);
}

 
static int exynos5_i2c_set_timing(struct exynos5_i2c *i2c, bool hs_timings)
{
	u32 i2c_timing_s1;
	u32 i2c_timing_s2;
	u32 i2c_timing_s3;
	u32 i2c_timing_sla;
	unsigned int t_start_su, t_start_hd;
	unsigned int t_stop_su;
	unsigned int t_data_su, t_data_hd;
	unsigned int t_scl_l, t_scl_h;
	unsigned int t_sr_release;
	unsigned int t_ftl_cycle;
	unsigned int clkin = clk_get_rate(i2c->clk);
	unsigned int op_clk = hs_timings ? i2c->op_clock :
		(i2c->op_clock >= I2C_MAX_FAST_MODE_PLUS_FREQ) ? I2C_MAX_STANDARD_MODE_FREQ :
		i2c->op_clock;
	int div, clk_cycle, temp;

	 
	if (i2c->variant->hw == I2C_TYPE_EXYNOSAUTOV9) {
		div = ((clkin / (16 * i2c->op_clock)) - 1);
		i2c_timing_s3 = div << 16;
		if (hs_timings)
			writel(i2c_timing_s3, i2c->regs + HSI2C_TIMING_HS3);
		else
			writel(i2c_timing_s3, i2c->regs + HSI2C_TIMING_FS3);

		return 0;
	}

	 
	t_ftl_cycle = (readl(i2c->regs + HSI2C_CONF) >> 16) & 0x7;
	temp = clkin / op_clk - 8 - t_ftl_cycle;
	if (i2c->variant->hw != I2C_TYPE_EXYNOS7)
		temp -= t_ftl_cycle;
	div = temp / 512;
	clk_cycle = temp / (div + 1) - 2;
	if (temp < 4 || div >= 256 || clk_cycle < 2) {
		dev_err(i2c->dev, "%s clock set-up failed\n",
			hs_timings ? "HS" : "FS");
		return -EINVAL;
	}

	t_scl_l = clk_cycle / 2;
	t_scl_h = clk_cycle / 2;
	t_start_su = t_scl_l;
	t_start_hd = t_scl_l;
	t_stop_su = t_scl_l;
	t_data_su = t_scl_l / 2;
	t_data_hd = t_scl_l / 2;
	t_sr_release = clk_cycle;

	i2c_timing_s1 = t_start_su << 24 | t_start_hd << 16 | t_stop_su << 8;
	i2c_timing_s2 = t_data_su << 24 | t_scl_l << 8 | t_scl_h << 0;
	i2c_timing_s3 = div << 16 | t_sr_release << 0;
	i2c_timing_sla = t_data_hd << 0;

	dev_dbg(i2c->dev, "tSTART_SU: %X, tSTART_HD: %X, tSTOP_SU: %X\n",
		t_start_su, t_start_hd, t_stop_su);
	dev_dbg(i2c->dev, "tDATA_SU: %X, tSCL_L: %X, tSCL_H: %X\n",
		t_data_su, t_scl_l, t_scl_h);
	dev_dbg(i2c->dev, "nClkDiv: %X, tSR_RELEASE: %X\n",
		div, t_sr_release);
	dev_dbg(i2c->dev, "tDATA_HD: %X\n", t_data_hd);

	if (hs_timings) {
		writel(i2c_timing_s1, i2c->regs + HSI2C_TIMING_HS1);
		writel(i2c_timing_s2, i2c->regs + HSI2C_TIMING_HS2);
		writel(i2c_timing_s3, i2c->regs + HSI2C_TIMING_HS3);
	} else {
		writel(i2c_timing_s1, i2c->regs + HSI2C_TIMING_FS1);
		writel(i2c_timing_s2, i2c->regs + HSI2C_TIMING_FS2);
		writel(i2c_timing_s3, i2c->regs + HSI2C_TIMING_FS3);
	}
	writel(i2c_timing_sla, i2c->regs + HSI2C_TIMING_SLA);

	return 0;
}

static int exynos5_hsi2c_clock_setup(struct exynos5_i2c *i2c)
{
	 
	int ret = exynos5_i2c_set_timing(i2c, false);

	if (ret < 0 || i2c->op_clock < I2C_MAX_FAST_MODE_PLUS_FREQ)
		return ret;

	return exynos5_i2c_set_timing(i2c, true);
}

 
static void exynos5_i2c_init(struct exynos5_i2c *i2c)
{
	u32 i2c_conf = readl(i2c->regs + HSI2C_CONF);
	u32 i2c_timeout = readl(i2c->regs + HSI2C_TIMEOUT);

	 
	i2c_timeout &= ~HSI2C_TIMEOUT_EN;
	writel(i2c_timeout, i2c->regs + HSI2C_TIMEOUT);

	writel((HSI2C_FUNC_MODE_I2C | HSI2C_MASTER),
					i2c->regs + HSI2C_CTL);
	writel(HSI2C_TRAILING_COUNT, i2c->regs + HSI2C_TRAILIG_CTL);

	if (i2c->op_clock >= I2C_MAX_FAST_MODE_PLUS_FREQ) {
		writel(HSI2C_MASTER_ID(MASTER_ID(i2c->adap.nr)),
					i2c->regs + HSI2C_ADDR);
		i2c_conf |= HSI2C_HS_MODE;
	}

	writel(i2c_conf | HSI2C_AUTO_MODE, i2c->regs + HSI2C_CONF);
}

static void exynos5_i2c_reset(struct exynos5_i2c *i2c)
{
	u32 i2c_ctl;

	 
	i2c_ctl = readl(i2c->regs + HSI2C_CTL);
	i2c_ctl |= HSI2C_SW_RST;
	writel(i2c_ctl, i2c->regs + HSI2C_CTL);

	i2c_ctl = readl(i2c->regs + HSI2C_CTL);
	i2c_ctl &= ~HSI2C_SW_RST;
	writel(i2c_ctl, i2c->regs + HSI2C_CTL);

	 
	exynos5_hsi2c_clock_setup(i2c);
	 
	exynos5_i2c_init(i2c);
}

 
static irqreturn_t exynos5_i2c_irq(int irqno, void *dev_id)
{
	struct exynos5_i2c *i2c = dev_id;
	u32 fifo_level, int_status, fifo_status, trans_status;
	unsigned char byte;
	int len = 0;

	i2c->state = -EINVAL;

	spin_lock(&i2c->lock);

	int_status = readl(i2c->regs + HSI2C_INT_STATUS);
	writel(int_status, i2c->regs + HSI2C_INT_STATUS);

	 
	switch (i2c->variant->hw) {
	case I2C_TYPE_EXYNOSAUTOV9:
		fallthrough;
	case I2C_TYPE_EXYNOS7:
		if (int_status & HSI2C_INT_TRANS_DONE) {
			i2c->trans_done = 1;
			i2c->state = 0;
		} else if (int_status & HSI2C_INT_TRANS_ABORT) {
			dev_dbg(i2c->dev, "Deal with arbitration lose\n");
			i2c->state = -EAGAIN;
			goto stop;
		} else if (int_status & HSI2C_INT_NO_DEV_ACK) {
			dev_dbg(i2c->dev, "No ACK from device\n");
			i2c->state = -ENXIO;
			goto stop;
		} else if (int_status & HSI2C_INT_NO_DEV) {
			dev_dbg(i2c->dev, "No device\n");
			i2c->state = -ENXIO;
			goto stop;
		} else if (int_status & HSI2C_INT_TIMEOUT) {
			dev_dbg(i2c->dev, "Accessing device timed out\n");
			i2c->state = -ETIMEDOUT;
			goto stop;
		}

		break;
	case I2C_TYPE_EXYNOS5:
		if (!(int_status & HSI2C_INT_I2C))
			break;

		trans_status = readl(i2c->regs + HSI2C_TRANS_STATUS);
		if (trans_status & HSI2C_NO_DEV_ACK) {
			dev_dbg(i2c->dev, "No ACK from device\n");
			i2c->state = -ENXIO;
			goto stop;
		} else if (trans_status & HSI2C_NO_DEV) {
			dev_dbg(i2c->dev, "No device\n");
			i2c->state = -ENXIO;
			goto stop;
		} else if (trans_status & HSI2C_TRANS_ABORT) {
			dev_dbg(i2c->dev, "Deal with arbitration lose\n");
			i2c->state = -EAGAIN;
			goto stop;
		} else if (trans_status & HSI2C_TIMEOUT_AUTO) {
			dev_dbg(i2c->dev, "Accessing device timed out\n");
			i2c->state = -ETIMEDOUT;
			goto stop;
		} else if (trans_status & HSI2C_TRANS_DONE) {
			i2c->trans_done = 1;
			i2c->state = 0;
		}

		break;
	}

	if ((i2c->msg->flags & I2C_M_RD) && (int_status &
			(HSI2C_INT_TRAILING | HSI2C_INT_RX_ALMOSTFULL))) {
		fifo_status = readl(i2c->regs + HSI2C_FIFO_STATUS);
		fifo_level = HSI2C_RX_FIFO_LVL(fifo_status);
		len = min(fifo_level, i2c->msg->len - i2c->msg_ptr);

		while (len > 0) {
			byte = (unsigned char)
				readl(i2c->regs + HSI2C_RX_DATA);
			i2c->msg->buf[i2c->msg_ptr++] = byte;
			len--;
		}
		i2c->state = 0;
	} else if (int_status & HSI2C_INT_TX_ALMOSTEMPTY) {
		fifo_status = readl(i2c->regs + HSI2C_FIFO_STATUS);
		fifo_level = HSI2C_TX_FIFO_LVL(fifo_status);

		len = i2c->variant->fifo_depth - fifo_level;
		if (len > (i2c->msg->len - i2c->msg_ptr)) {
			u32 int_en = readl(i2c->regs + HSI2C_INT_ENABLE);

			int_en &= ~HSI2C_INT_TX_ALMOSTEMPTY_EN;
			writel(int_en, i2c->regs + HSI2C_INT_ENABLE);
			len = i2c->msg->len - i2c->msg_ptr;
		}

		while (len > 0) {
			byte = i2c->msg->buf[i2c->msg_ptr++];
			writel(byte, i2c->regs + HSI2C_TX_DATA);
			len--;
		}
		i2c->state = 0;
	}

 stop:
	if ((i2c->trans_done && (i2c->msg->len == i2c->msg_ptr)) ||
	    (i2c->state < 0)) {
		writel(0, i2c->regs + HSI2C_INT_ENABLE);
		exynos5_i2c_clr_pend_irq(i2c);
		complete(&i2c->msg_complete);
	}

	spin_unlock(&i2c->lock);

	return IRQ_HANDLED;
}

 
static int exynos5_i2c_wait_bus_idle(struct exynos5_i2c *i2c)
{
	unsigned long stop_time;
	u32 trans_status;

	 
	stop_time = jiffies + msecs_to_jiffies(100) + 1;
	do {
		trans_status = readl(i2c->regs + HSI2C_TRANS_STATUS);
		if (!(trans_status & HSI2C_MASTER_BUSY))
			return 0;

		usleep_range(50, 200);
	} while (time_before(jiffies, stop_time));

	return -EBUSY;
}

static void exynos5_i2c_bus_recover(struct exynos5_i2c *i2c)
{
	u32 val;

	val = readl(i2c->regs + HSI2C_CTL) | HSI2C_RXCHON;
	writel(val, i2c->regs + HSI2C_CTL);
	val = readl(i2c->regs + HSI2C_CONF) & ~HSI2C_AUTO_MODE;
	writel(val, i2c->regs + HSI2C_CONF);

	 
	writel(HSI2C_CMD_READ_DATA, i2c->regs + HSI2C_MANUAL_CMD);
	exynos5_i2c_wait_bus_idle(i2c);
	writel(HSI2C_CMD_SEND_STOP, i2c->regs + HSI2C_MANUAL_CMD);
	exynos5_i2c_wait_bus_idle(i2c);

	val = readl(i2c->regs + HSI2C_CTL) & ~HSI2C_RXCHON;
	writel(val, i2c->regs + HSI2C_CTL);
	val = readl(i2c->regs + HSI2C_CONF) | HSI2C_AUTO_MODE;
	writel(val, i2c->regs + HSI2C_CONF);
}

static void exynos5_i2c_bus_check(struct exynos5_i2c *i2c)
{
	unsigned long timeout;

	if (i2c->variant->hw == I2C_TYPE_EXYNOS5)
		return;

	 
	timeout = jiffies + msecs_to_jiffies(100);
	for (;;) {
		u32 st = readl(i2c->regs + HSI2C_TRANS_STATUS);

		if ((st & HSI2C_MASTER_ST_MASK) != HSI2C_MASTER_ST_LOSE)
			return;

		if (time_is_before_jiffies(timeout))
			return;

		exynos5_i2c_bus_recover(i2c);
	}
}

 
static void exynos5_i2c_message_start(struct exynos5_i2c *i2c, int stop)
{
	u32 i2c_ctl;
	u32 int_en = 0;
	u32 i2c_auto_conf = 0;
	u32 i2c_addr = 0;
	u32 fifo_ctl;
	unsigned long flags;
	unsigned short trig_lvl;

	if (i2c->variant->hw == I2C_TYPE_EXYNOS5)
		int_en |= HSI2C_INT_I2C;
	else
		int_en |= HSI2C_INT_I2C_TRANS;

	i2c_ctl = readl(i2c->regs + HSI2C_CTL);
	i2c_ctl &= ~(HSI2C_TXCHON | HSI2C_RXCHON);
	fifo_ctl = HSI2C_RXFIFO_EN | HSI2C_TXFIFO_EN;

	if (i2c->msg->flags & I2C_M_RD) {
		i2c_ctl |= HSI2C_RXCHON;

		i2c_auto_conf |= HSI2C_READ_WRITE;

		trig_lvl = (i2c->msg->len > i2c->variant->fifo_depth) ?
			(i2c->variant->fifo_depth * 3 / 4) : i2c->msg->len;
		fifo_ctl |= HSI2C_RXFIFO_TRIGGER_LEVEL(trig_lvl);

		int_en |= (HSI2C_INT_RX_ALMOSTFULL_EN |
			HSI2C_INT_TRAILING_EN);
	} else {
		i2c_ctl |= HSI2C_TXCHON;

		trig_lvl = (i2c->msg->len > i2c->variant->fifo_depth) ?
			(i2c->variant->fifo_depth * 1 / 4) : i2c->msg->len;
		fifo_ctl |= HSI2C_TXFIFO_TRIGGER_LEVEL(trig_lvl);

		int_en |= HSI2C_INT_TX_ALMOSTEMPTY_EN;
	}

	i2c_addr = HSI2C_SLV_ADDR_MAS(i2c->msg->addr);

	if (i2c->op_clock >= I2C_MAX_FAST_MODE_PLUS_FREQ)
		i2c_addr |= HSI2C_MASTER_ID(MASTER_ID(i2c->adap.nr));

	writel(i2c_addr, i2c->regs + HSI2C_ADDR);

	writel(fifo_ctl, i2c->regs + HSI2C_FIFO_CTL);
	writel(i2c_ctl, i2c->regs + HSI2C_CTL);

	exynos5_i2c_bus_check(i2c);

	 
	spin_lock_irqsave(&i2c->lock, flags);
	writel(int_en, i2c->regs + HSI2C_INT_ENABLE);

	if (stop == 1)
		i2c_auto_conf |= HSI2C_STOP_AFTER_TRANS;
	i2c_auto_conf |= i2c->msg->len;
	i2c_auto_conf |= HSI2C_MASTER_RUN;
	writel(i2c_auto_conf, i2c->regs + HSI2C_AUTO_CONF);
	spin_unlock_irqrestore(&i2c->lock, flags);
}

static int exynos5_i2c_xfer_msg(struct exynos5_i2c *i2c,
			      struct i2c_msg *msgs, int stop)
{
	unsigned long timeout;
	int ret;

	i2c->msg = msgs;
	i2c->msg_ptr = 0;
	i2c->trans_done = 0;

	reinit_completion(&i2c->msg_complete);

	exynos5_i2c_message_start(i2c, stop);

	timeout = wait_for_completion_timeout(&i2c->msg_complete,
					      EXYNOS5_I2C_TIMEOUT);
	if (timeout == 0)
		ret = -ETIMEDOUT;
	else
		ret = i2c->state;

	 
	if (ret == 0 && stop)
		ret = exynos5_i2c_wait_bus_idle(i2c);

	if (ret < 0) {
		exynos5_i2c_reset(i2c);
		if (ret == -ETIMEDOUT)
			dev_warn(i2c->dev, "%s timeout\n",
				 (msgs->flags & I2C_M_RD) ? "rx" : "tx");
	}

	 
	return ret;
}

static int exynos5_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	struct exynos5_i2c *i2c = adap->algo_data;
	int i, ret;

	ret = clk_enable(i2c->pclk);
	if (ret)
		return ret;

	ret = clk_enable(i2c->clk);
	if (ret)
		goto err_pclk;

	for (i = 0; i < num; ++i) {
		ret = exynos5_i2c_xfer_msg(i2c, msgs + i, i + 1 == num);
		if (ret)
			break;
	}

	clk_disable(i2c->clk);
err_pclk:
	clk_disable(i2c->pclk);

	return ret ?: num;
}

static u32 exynos5_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static const struct i2c_algorithm exynos5_i2c_algorithm = {
	.master_xfer		= exynos5_i2c_xfer,
	.functionality		= exynos5_i2c_func,
};

static int exynos5_i2c_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct exynos5_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(&pdev->dev, sizeof(struct exynos5_i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	if (of_property_read_u32(np, "clock-frequency", &i2c->op_clock))
		i2c->op_clock = I2C_MAX_STANDARD_MODE_FREQ;

	strscpy(i2c->adap.name, "exynos5-i2c", sizeof(i2c->adap.name));
	i2c->adap.owner   = THIS_MODULE;
	i2c->adap.algo    = &exynos5_i2c_algorithm;
	i2c->adap.retries = 3;

	i2c->dev = &pdev->dev;
	i2c->clk = devm_clk_get(&pdev->dev, "hsi2c");
	if (IS_ERR(i2c->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		return -ENOENT;
	}

	i2c->pclk = devm_clk_get_optional(&pdev->dev, "hsi2c_pclk");
	if (IS_ERR(i2c->pclk)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(i2c->pclk),
				     "cannot get pclk");
	}

	ret = clk_prepare_enable(i2c->pclk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(i2c->clk);
	if (ret)
		goto err_pclk;

	i2c->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(i2c->regs)) {
		ret = PTR_ERR(i2c->regs);
		goto err_clk;
	}

	i2c->adap.dev.of_node = np;
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;

	 
	exynos5_i2c_clr_pend_irq(i2c);

	spin_lock_init(&i2c->lock);
	init_completion(&i2c->msg_complete);

	i2c->irq = ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		goto err_clk;

	ret = devm_request_irq(&pdev->dev, i2c->irq, exynos5_i2c_irq,
			       IRQF_NO_SUSPEND, dev_name(&pdev->dev), i2c);
	if (ret != 0) {
		dev_err(&pdev->dev, "cannot request HS-I2C IRQ %d\n", i2c->irq);
		goto err_clk;
	}

	i2c->variant = of_device_get_match_data(&pdev->dev);

	ret = exynos5_hsi2c_clock_setup(i2c);
	if (ret)
		goto err_clk;

	exynos5_i2c_reset(i2c);

	ret = i2c_add_adapter(&i2c->adap);
	if (ret < 0)
		goto err_clk;

	platform_set_drvdata(pdev, i2c);

	clk_disable(i2c->clk);
	clk_disable(i2c->pclk);

	return 0;

 err_clk:
	clk_disable_unprepare(i2c->clk);

 err_pclk:
	clk_disable_unprepare(i2c->pclk);
	return ret;
}

static void exynos5_i2c_remove(struct platform_device *pdev)
{
	struct exynos5_i2c *i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c->adap);

	clk_unprepare(i2c->clk);
	clk_unprepare(i2c->pclk);
}

static int exynos5_i2c_suspend_noirq(struct device *dev)
{
	struct exynos5_i2c *i2c = dev_get_drvdata(dev);

	i2c_mark_adapter_suspended(&i2c->adap);
	clk_unprepare(i2c->clk);
	clk_unprepare(i2c->pclk);

	return 0;
}

static int exynos5_i2c_resume_noirq(struct device *dev)
{
	struct exynos5_i2c *i2c = dev_get_drvdata(dev);
	int ret = 0;

	ret = clk_prepare_enable(i2c->pclk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(i2c->clk);
	if (ret)
		goto err_pclk;

	ret = exynos5_hsi2c_clock_setup(i2c);
	if (ret)
		goto err_clk;

	exynos5_i2c_init(i2c);
	clk_disable(i2c->clk);
	clk_disable(i2c->pclk);
	i2c_mark_adapter_resumed(&i2c->adap);

	return 0;

err_clk:
	clk_disable_unprepare(i2c->clk);
err_pclk:
	clk_disable_unprepare(i2c->pclk);
	return ret;
}

static const struct dev_pm_ops exynos5_i2c_dev_pm_ops = {
	NOIRQ_SYSTEM_SLEEP_PM_OPS(exynos5_i2c_suspend_noirq,
				  exynos5_i2c_resume_noirq)
};

static struct platform_driver exynos5_i2c_driver = {
	.probe		= exynos5_i2c_probe,
	.remove_new	= exynos5_i2c_remove,
	.driver		= {
		.name	= "exynos5-hsi2c",
		.pm	= pm_sleep_ptr(&exynos5_i2c_dev_pm_ops),
		.of_match_table = exynos5_i2c_match,
	},
};

module_platform_driver(exynos5_i2c_driver);

MODULE_DESCRIPTION("Exynos5 HS-I2C Bus driver");
MODULE_AUTHOR("Naveen Krishna Chatradhi <ch.naveen@samsung.com>");
MODULE_AUTHOR("Taekgyun Ko <taeggyun.ko@samsung.com>");
MODULE_LICENSE("GPL v2");
