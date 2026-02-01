


#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>

 
#define CS_OFFSET				0x00000020
#define CS_ACK_SHIFT				3
#define CS_ACK_MASK				0x00000008
#define CS_ACK_CMD_GEN_START			0x00000000
#define CS_ACK_CMD_GEN_RESTART			0x00000001
#define CS_CMD_SHIFT				1
#define CS_CMD_CMD_NO_ACTION			0x00000000
#define CS_CMD_CMD_START_RESTART		0x00000001
#define CS_CMD_CMD_STOP				0x00000002
#define CS_EN_SHIFT				0
#define CS_EN_CMD_ENABLE_BSC			0x00000001

#define TIM_OFFSET				0x00000024
#define TIM_PRESCALE_SHIFT			6
#define TIM_P_SHIFT				3
#define TIM_NO_DIV_SHIFT			2
#define TIM_DIV_SHIFT				0

#define DAT_OFFSET				0x00000028

#define TOUT_OFFSET				0x0000002c

#define TXFCR_OFFSET				0x0000003c
#define TXFCR_FIFO_FLUSH_MASK			0x00000080
#define TXFCR_FIFO_EN_MASK			0x00000040

#define IER_OFFSET				0x00000044
#define IER_READ_COMPLETE_INT_MASK		0x00000010
#define IER_I2C_INT_EN_MASK			0x00000008
#define IER_FIFO_INT_EN_MASK			0x00000002
#define IER_NOACK_EN_MASK			0x00000001

#define ISR_OFFSET				0x00000048
#define ISR_RESERVED_MASK			0xffffff60
#define ISR_CMDBUSY_MASK			0x00000080
#define ISR_READ_COMPLETE_MASK			0x00000010
#define ISR_SES_DONE_MASK			0x00000008
#define ISR_ERR_MASK				0x00000004
#define ISR_TXFIFOEMPTY_MASK			0x00000002
#define ISR_NOACK_MASK				0x00000001

#define CLKEN_OFFSET				0x0000004C
#define CLKEN_AUTOSENSE_OFF_MASK		0x00000080
#define CLKEN_M_SHIFT				4
#define CLKEN_N_SHIFT				1
#define CLKEN_CLKEN_MASK			0x00000001

#define FIFO_STATUS_OFFSET			0x00000054
#define FIFO_STATUS_RXFIFO_EMPTY_MASK		0x00000004
#define FIFO_STATUS_TXFIFO_EMPTY_MASK		0x00000010

#define HSTIM_OFFSET				0x00000058
#define HSTIM_HS_MODE_MASK			0x00008000
#define HSTIM_HS_HOLD_SHIFT			10
#define HSTIM_HS_HIGH_PHASE_SHIFT		5
#define HSTIM_HS_SETUP_SHIFT			0

#define PADCTL_OFFSET				0x0000005c
#define PADCTL_PAD_OUT_EN_MASK			0x00000004

#define RXFCR_OFFSET				0x00000068
#define RXFCR_NACK_EN_SHIFT			7
#define RXFCR_READ_COUNT_SHIFT			0
#define RXFIFORDOUT_OFFSET			0x0000006c

 
#define MAX_RX_FIFO_SIZE		64U  
#define MAX_TX_FIFO_SIZE		64U  

#define STD_EXT_CLK_FREQ		13000000UL
#define HS_EXT_CLK_FREQ			104000000UL

#define MASTERCODE			0x08  

#define I2C_TIMEOUT			100  

 
enum bcm_kona_cmd_t {
	BCM_CMD_NOACTION = 0,
	BCM_CMD_START,
	BCM_CMD_RESTART,
	BCM_CMD_STOP,
};

enum bus_speed_index {
	BCM_SPD_100K = 0,
	BCM_SPD_400K,
	BCM_SPD_1MHZ,
};

enum hs_bus_speed_index {
	BCM_SPD_3P4MHZ = 0,
};

 
struct bus_speed_cfg {
	uint8_t time_m;		 
	uint8_t time_n;		 
	uint8_t prescale;	 
	uint8_t time_p;		 
	uint8_t no_div;		 
	uint8_t time_div;	 
};

 
struct hs_bus_speed_cfg {
	uint8_t hs_hold;	 
	uint8_t hs_high_phase;	 
	uint8_t hs_setup;	 
	uint8_t prescale;	 
	uint8_t time_p;		 
	uint8_t no_div;		 
	uint8_t time_div;	 
};

static const struct bus_speed_cfg std_cfg_table[] = {
	[BCM_SPD_100K] = {0x01, 0x01, 0x03, 0x06, 0x00, 0x02},
	[BCM_SPD_400K] = {0x05, 0x01, 0x03, 0x05, 0x01, 0x02},
	[BCM_SPD_1MHZ] = {0x01, 0x01, 0x03, 0x01, 0x01, 0x03},
};

static const struct hs_bus_speed_cfg hs_cfg_table[] = {
	[BCM_SPD_3P4MHZ] = {0x01, 0x08, 0x14, 0x00, 0x06, 0x01, 0x00},
};

struct bcm_kona_i2c_dev {
	struct device *device;

	void __iomem *base;
	int irq;
	struct clk *external_clk;

	struct i2c_adapter adapter;

	struct completion done;

	const struct bus_speed_cfg *std_cfg;
	const struct hs_bus_speed_cfg *hs_cfg;
};

static void bcm_kona_i2c_send_cmd_to_ctrl(struct bcm_kona_i2c_dev *dev,
					  enum bcm_kona_cmd_t cmd)
{
	dev_dbg(dev->device, "%s, %d\n", __func__, cmd);

	switch (cmd) {
	case BCM_CMD_NOACTION:
		writel((CS_CMD_CMD_NO_ACTION << CS_CMD_SHIFT) |
		       (CS_EN_CMD_ENABLE_BSC << CS_EN_SHIFT),
		       dev->base + CS_OFFSET);
		break;

	case BCM_CMD_START:
		writel((CS_ACK_CMD_GEN_START << CS_ACK_SHIFT) |
		       (CS_CMD_CMD_START_RESTART << CS_CMD_SHIFT) |
		       (CS_EN_CMD_ENABLE_BSC << CS_EN_SHIFT),
		       dev->base + CS_OFFSET);
		break;

	case BCM_CMD_RESTART:
		writel((CS_ACK_CMD_GEN_RESTART << CS_ACK_SHIFT) |
		       (CS_CMD_CMD_START_RESTART << CS_CMD_SHIFT) |
		       (CS_EN_CMD_ENABLE_BSC << CS_EN_SHIFT),
		       dev->base + CS_OFFSET);
		break;

	case BCM_CMD_STOP:
		writel((CS_CMD_CMD_STOP << CS_CMD_SHIFT) |
		       (CS_EN_CMD_ENABLE_BSC << CS_EN_SHIFT),
		       dev->base + CS_OFFSET);
		break;

	default:
		dev_err(dev->device, "Unknown command %d\n", cmd);
	}
}

static void bcm_kona_i2c_enable_clock(struct bcm_kona_i2c_dev *dev)
{
	writel(readl(dev->base + CLKEN_OFFSET) | CLKEN_CLKEN_MASK,
	       dev->base + CLKEN_OFFSET);
}

static void bcm_kona_i2c_disable_clock(struct bcm_kona_i2c_dev *dev)
{
	writel(readl(dev->base + CLKEN_OFFSET) & ~CLKEN_CLKEN_MASK,
	       dev->base + CLKEN_OFFSET);
}

static irqreturn_t bcm_kona_i2c_isr(int irq, void *devid)
{
	struct bcm_kona_i2c_dev *dev = devid;
	uint32_t status = readl(dev->base + ISR_OFFSET);

	if ((status & ~ISR_RESERVED_MASK) == 0)
		return IRQ_NONE;

	 
	if (status & ISR_NOACK_MASK)
		writel(TXFCR_FIFO_FLUSH_MASK | TXFCR_FIFO_EN_MASK,
		       dev->base + TXFCR_OFFSET);

	writel(status & ~ISR_RESERVED_MASK, dev->base + ISR_OFFSET);
	complete(&dev->done);

	return IRQ_HANDLED;
}

 
static int bcm_kona_i2c_wait_if_busy(struct bcm_kona_i2c_dev *dev)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(I2C_TIMEOUT);

	while (readl(dev->base + ISR_OFFSET) & ISR_CMDBUSY_MASK)
		if (time_after(jiffies, timeout)) {
			dev_err(dev->device, "CMDBUSY timeout\n");
			return -ETIMEDOUT;
		}

	return 0;
}

 
static int bcm_kona_send_i2c_cmd(struct bcm_kona_i2c_dev *dev,
				 enum bcm_kona_cmd_t cmd)
{
	int rc;
	unsigned long time_left = msecs_to_jiffies(I2C_TIMEOUT);

	 
	rc = bcm_kona_i2c_wait_if_busy(dev);
	if (rc < 0)
		return rc;

	 
	writel(IER_I2C_INT_EN_MASK, dev->base + IER_OFFSET);

	 
	reinit_completion(&dev->done);

	 
	bcm_kona_i2c_send_cmd_to_ctrl(dev, cmd);

	 
	time_left = wait_for_completion_timeout(&dev->done, time_left);

	 
	writel(0, dev->base + IER_OFFSET);

	if (!time_left) {
		dev_err(dev->device, "controller timed out\n");
		rc = -ETIMEDOUT;
	}

	 
	bcm_kona_i2c_send_cmd_to_ctrl(dev, BCM_CMD_NOACTION);

	return rc;
}

 
static int bcm_kona_i2c_read_fifo_single(struct bcm_kona_i2c_dev *dev,
					 uint8_t *buf, unsigned int len,
					 unsigned int last_byte_nak)
{
	unsigned long time_left = msecs_to_jiffies(I2C_TIMEOUT);

	 
	reinit_completion(&dev->done);

	 
	writel(IER_READ_COMPLETE_INT_MASK, dev->base + IER_OFFSET);

	 
	writel((last_byte_nak << RXFCR_NACK_EN_SHIFT) |
	       (len << RXFCR_READ_COUNT_SHIFT),
		dev->base + RXFCR_OFFSET);

	 
	time_left = wait_for_completion_timeout(&dev->done, time_left);

	 
	writel(0, dev->base + IER_OFFSET);

	if (!time_left) {
		dev_err(dev->device, "RX FIFO time out\n");
		return -EREMOTEIO;
	}

	 
	for (; len > 0; len--, buf++)
		*buf = readl(dev->base + RXFIFORDOUT_OFFSET);

	return 0;
}

 
static int bcm_kona_i2c_read_fifo(struct bcm_kona_i2c_dev *dev,
				  struct i2c_msg *msg)
{
	unsigned int bytes_to_read = MAX_RX_FIFO_SIZE;
	unsigned int last_byte_nak = 0;
	unsigned int bytes_read = 0;
	int rc;

	uint8_t *tmp_buf = msg->buf;

	while (bytes_read < msg->len) {
		if (msg->len - bytes_read <= MAX_RX_FIFO_SIZE) {
			last_byte_nak = 1;  
			bytes_to_read = msg->len - bytes_read;
		}

		rc = bcm_kona_i2c_read_fifo_single(dev, tmp_buf, bytes_to_read,
						   last_byte_nak);
		if (rc < 0)
			return -EREMOTEIO;

		bytes_read += bytes_to_read;
		tmp_buf += bytes_to_read;
	}

	return 0;
}

 
static int bcm_kona_i2c_write_byte(struct bcm_kona_i2c_dev *dev, uint8_t data,
				   unsigned int nak_expected)
{
	int rc;
	unsigned long time_left = msecs_to_jiffies(I2C_TIMEOUT);
	unsigned int nak_received;

	 
	rc = bcm_kona_i2c_wait_if_busy(dev);
	if (rc < 0)
		return rc;

	 
	writel(ISR_SES_DONE_MASK, dev->base + ISR_OFFSET);

	 
	writel(IER_I2C_INT_EN_MASK, dev->base + IER_OFFSET);

	 
	reinit_completion(&dev->done);

	 
	writel(data, dev->base + DAT_OFFSET);

	 
	time_left = wait_for_completion_timeout(&dev->done, time_left);

	 
	writel(0, dev->base + IER_OFFSET);

	if (!time_left) {
		dev_dbg(dev->device, "controller timed out\n");
		return -ETIMEDOUT;
	}

	nak_received = readl(dev->base + CS_OFFSET) & CS_ACK_MASK ? 1 : 0;

	if (nak_received ^ nak_expected) {
		dev_dbg(dev->device, "unexpected NAK/ACK\n");
		return -EREMOTEIO;
	}

	return 0;
}

 
static int bcm_kona_i2c_write_fifo_single(struct bcm_kona_i2c_dev *dev,
					  uint8_t *buf, unsigned int len)
{
	int k;
	unsigned long time_left = msecs_to_jiffies(I2C_TIMEOUT);
	unsigned int fifo_status;

	 
	reinit_completion(&dev->done);

	 
	writel(IER_FIFO_INT_EN_MASK | IER_NOACK_EN_MASK,
	       dev->base + IER_OFFSET);

	 
	disable_irq(dev->irq);

	 
	for (k = 0; k < len; k++)
		writel(buf[k], (dev->base + DAT_OFFSET));

	 
	enable_irq(dev->irq);

	 
	do {
		time_left = wait_for_completion_timeout(&dev->done, time_left);
		fifo_status = readl(dev->base + FIFO_STATUS_OFFSET);
	} while (time_left && !(fifo_status & FIFO_STATUS_TXFIFO_EMPTY_MASK));

	 
	writel(0, dev->base + IER_OFFSET);

	 
	if (readl(dev->base + CS_OFFSET) & CS_ACK_MASK) {
		dev_err(dev->device, "unexpected NAK\n");
		return -EREMOTEIO;
	}

	 
	if (!time_left) {
		dev_err(dev->device, "completion timed out\n");
		return -EREMOTEIO;
	}

	return 0;
}


 
static int bcm_kona_i2c_write_fifo(struct bcm_kona_i2c_dev *dev,
				   struct i2c_msg *msg)
{
	unsigned int bytes_to_write = MAX_TX_FIFO_SIZE;
	unsigned int bytes_written = 0;
	int rc;

	uint8_t *tmp_buf = msg->buf;

	while (bytes_written < msg->len) {
		if (msg->len - bytes_written <= MAX_TX_FIFO_SIZE)
			bytes_to_write = msg->len - bytes_written;

		rc = bcm_kona_i2c_write_fifo_single(dev, tmp_buf,
						    bytes_to_write);
		if (rc < 0)
			return -EREMOTEIO;

		bytes_written += bytes_to_write;
		tmp_buf += bytes_to_write;
	}

	return 0;
}

 
static int bcm_kona_i2c_do_addr(struct bcm_kona_i2c_dev *dev,
				     struct i2c_msg *msg)
{
	unsigned char addr;

	if (msg->flags & I2C_M_TEN) {
		 
		addr = 0xF0 | ((msg->addr & 0x300) >> 7);
		if (bcm_kona_i2c_write_byte(dev, addr, 0) < 0)
			return -EREMOTEIO;

		 
		addr = msg->addr & 0xFF;
		if (bcm_kona_i2c_write_byte(dev, addr, 0) < 0)
			return -EREMOTEIO;

		if (msg->flags & I2C_M_RD) {
			 
			if (bcm_kona_send_i2c_cmd(dev, BCM_CMD_RESTART) < 0)
				return -EREMOTEIO;

			 
			addr = 0xF0 | ((msg->addr & 0x300) >> 7) | 0x01;
			if (bcm_kona_i2c_write_byte(dev, addr, 0) < 0)
				return -EREMOTEIO;
		}
	} else {
		addr = i2c_8bit_addr_from_msg(msg);

		if (bcm_kona_i2c_write_byte(dev, addr, 0) < 0)
			return -EREMOTEIO;
	}

	return 0;
}

static void bcm_kona_i2c_enable_autosense(struct bcm_kona_i2c_dev *dev)
{
	writel(readl(dev->base + CLKEN_OFFSET) & ~CLKEN_AUTOSENSE_OFF_MASK,
	       dev->base + CLKEN_OFFSET);
}

static void bcm_kona_i2c_config_timing(struct bcm_kona_i2c_dev *dev)
{
	writel(readl(dev->base + HSTIM_OFFSET) & ~HSTIM_HS_MODE_MASK,
	       dev->base + HSTIM_OFFSET);

	writel((dev->std_cfg->prescale << TIM_PRESCALE_SHIFT) |
	       (dev->std_cfg->time_p << TIM_P_SHIFT) |
	       (dev->std_cfg->no_div << TIM_NO_DIV_SHIFT) |
	       (dev->std_cfg->time_div	<< TIM_DIV_SHIFT),
	       dev->base + TIM_OFFSET);

	writel((dev->std_cfg->time_m << CLKEN_M_SHIFT) |
	       (dev->std_cfg->time_n << CLKEN_N_SHIFT) |
	       CLKEN_CLKEN_MASK,
	       dev->base + CLKEN_OFFSET);
}

static void bcm_kona_i2c_config_timing_hs(struct bcm_kona_i2c_dev *dev)
{
	writel((dev->hs_cfg->prescale << TIM_PRESCALE_SHIFT) |
	       (dev->hs_cfg->time_p << TIM_P_SHIFT) |
	       (dev->hs_cfg->no_div << TIM_NO_DIV_SHIFT) |
	       (dev->hs_cfg->time_div << TIM_DIV_SHIFT),
	       dev->base + TIM_OFFSET);

	writel((dev->hs_cfg->hs_hold << HSTIM_HS_HOLD_SHIFT) |
	       (dev->hs_cfg->hs_high_phase << HSTIM_HS_HIGH_PHASE_SHIFT) |
	       (dev->hs_cfg->hs_setup << HSTIM_HS_SETUP_SHIFT),
	       dev->base + HSTIM_OFFSET);

	writel(readl(dev->base + HSTIM_OFFSET) | HSTIM_HS_MODE_MASK,
	       dev->base + HSTIM_OFFSET);
}

static int bcm_kona_i2c_switch_to_hs(struct bcm_kona_i2c_dev *dev)
{
	int rc;

	 
	rc = bcm_kona_i2c_write_byte(dev, MASTERCODE, 1);
	if (rc < 0) {
		pr_err("High speed handshake failed\n");
		return rc;
	}

	 
	rc = clk_set_rate(dev->external_clk, HS_EXT_CLK_FREQ);
	if (rc) {
		dev_err(dev->device, "%s: clk_set_rate returned %d\n",
			__func__, rc);
		return rc;
	}

	 
	bcm_kona_i2c_config_timing_hs(dev);

	 
	rc = bcm_kona_send_i2c_cmd(dev, BCM_CMD_RESTART);
	if (rc < 0)
		dev_err(dev->device, "High speed restart command failed\n");

	return rc;
}

static int bcm_kona_i2c_switch_to_std(struct bcm_kona_i2c_dev *dev)
{
	int rc;

	 
	bcm_kona_i2c_config_timing(dev);

	 
	rc = clk_set_rate(dev->external_clk, STD_EXT_CLK_FREQ);
	if (rc) {
		dev_err(dev->device, "%s: clk_set_rate returned %d\n",
			__func__, rc);
	}

	return rc;
}

 
static int bcm_kona_i2c_xfer(struct i2c_adapter *adapter,
			     struct i2c_msg msgs[], int num)
{
	struct bcm_kona_i2c_dev *dev = i2c_get_adapdata(adapter);
	struct i2c_msg *pmsg;
	int rc = 0;
	int i;

	rc = clk_prepare_enable(dev->external_clk);
	if (rc) {
		dev_err(dev->device, "%s: peri clock enable failed. err %d\n",
			__func__, rc);
		return rc;
	}

	 
	writel(0, dev->base + PADCTL_OFFSET);

	 
	bcm_kona_i2c_enable_clock(dev);

	 
	rc = bcm_kona_send_i2c_cmd(dev, BCM_CMD_START);
	if (rc < 0) {
		dev_err(dev->device, "Start command failed rc = %d\n", rc);
		goto xfer_disable_pad;
	}

	 
	if (dev->hs_cfg) {
		rc = bcm_kona_i2c_switch_to_hs(dev);
		if (rc < 0)
			goto xfer_send_stop;
	}

	 
	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];

		 
		if ((i != 0) && ((pmsg->flags & I2C_M_NOSTART) == 0)) {
			rc = bcm_kona_send_i2c_cmd(dev, BCM_CMD_RESTART);
			if (rc < 0) {
				dev_err(dev->device,
					"restart cmd failed rc = %d\n", rc);
				goto xfer_send_stop;
			}
		}

		 
		if (!(pmsg->flags & I2C_M_NOSTART)) {
			rc = bcm_kona_i2c_do_addr(dev, pmsg);
			if (rc < 0) {
				dev_err(dev->device,
					"NAK from addr %2.2x msg#%d rc = %d\n",
					pmsg->addr, i, rc);
				goto xfer_send_stop;
			}
		}

		 
		if (pmsg->flags & I2C_M_RD) {
			rc = bcm_kona_i2c_read_fifo(dev, pmsg);
			if (rc < 0) {
				dev_err(dev->device, "read failure\n");
				goto xfer_send_stop;
			}
		} else {
			rc = bcm_kona_i2c_write_fifo(dev, pmsg);
			if (rc < 0) {
				dev_err(dev->device, "write failure");
				goto xfer_send_stop;
			}
		}
	}

	rc = num;

xfer_send_stop:
	 
	bcm_kona_send_i2c_cmd(dev, BCM_CMD_STOP);

	 
	if (dev->hs_cfg) {
		int hs_rc = bcm_kona_i2c_switch_to_std(dev);

		if (hs_rc)
			rc = hs_rc;
	}

xfer_disable_pad:
	 
	writel(PADCTL_PAD_OUT_EN_MASK, dev->base + PADCTL_OFFSET);

	 
	bcm_kona_i2c_disable_clock(dev);

	clk_disable_unprepare(dev->external_clk);

	return rc;
}

static uint32_t bcm_kona_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR |
	    I2C_FUNC_NOSTART;
}

static const struct i2c_algorithm bcm_algo = {
	.master_xfer = bcm_kona_i2c_xfer,
	.functionality = bcm_kona_i2c_functionality,
};

static int bcm_kona_i2c_assign_bus_speed(struct bcm_kona_i2c_dev *dev)
{
	unsigned int bus_speed;
	int ret = of_property_read_u32(dev->device->of_node, "clock-frequency",
				       &bus_speed);
	if (ret < 0) {
		dev_err(dev->device, "missing clock-frequency property\n");
		return -ENODEV;
	}

	switch (bus_speed) {
	case I2C_MAX_STANDARD_MODE_FREQ:
		dev->std_cfg = &std_cfg_table[BCM_SPD_100K];
		break;
	case I2C_MAX_FAST_MODE_FREQ:
		dev->std_cfg = &std_cfg_table[BCM_SPD_400K];
		break;
	case I2C_MAX_FAST_MODE_PLUS_FREQ:
		dev->std_cfg = &std_cfg_table[BCM_SPD_1MHZ];
		break;
	case I2C_MAX_HIGH_SPEED_MODE_FREQ:
		 
		dev->std_cfg = &std_cfg_table[BCM_SPD_100K];
		dev->hs_cfg = &hs_cfg_table[BCM_SPD_3P4MHZ];
		break;
	default:
		pr_err("%d hz bus speed not supported\n", bus_speed);
		pr_err("Valid speeds are 100khz, 400khz, 1mhz, and 3.4mhz\n");
		return -EINVAL;
	}

	return 0;
}

static int bcm_kona_i2c_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct bcm_kona_i2c_dev *dev;
	struct i2c_adapter *adap;

	 
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, dev);
	dev->device = &pdev->dev;
	init_completion(&dev->done);

	 
	dev->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dev->base))
		return PTR_ERR(dev->base);

	 
	dev->external_clk = devm_clk_get(dev->device, NULL);
	if (IS_ERR(dev->external_clk)) {
		dev_err(dev->device, "couldn't get clock\n");
		return -ENODEV;
	}

	rc = clk_set_rate(dev->external_clk, STD_EXT_CLK_FREQ);
	if (rc) {
		dev_err(dev->device, "%s: clk_set_rate returned %d\n",
			__func__, rc);
		return rc;
	}

	rc = clk_prepare_enable(dev->external_clk);
	if (rc) {
		dev_err(dev->device, "couldn't enable clock\n");
		return rc;
	}

	 
	rc = bcm_kona_i2c_assign_bus_speed(dev);
	if (rc)
		goto probe_disable_clk;

	 
	bcm_kona_i2c_enable_clock(dev);

	 
	bcm_kona_i2c_config_timing(dev);

	 
	writel(0, dev->base + TOUT_OFFSET);

	 
	bcm_kona_i2c_enable_autosense(dev);

	 
	writel(TXFCR_FIFO_FLUSH_MASK | TXFCR_FIFO_EN_MASK,
	       dev->base + TXFCR_OFFSET);

	 
	writel(0, dev->base + IER_OFFSET);

	 
	writel(ISR_CMDBUSY_MASK |
	       ISR_READ_COMPLETE_MASK |
	       ISR_SES_DONE_MASK |
	       ISR_ERR_MASK |
	       ISR_TXFIFOEMPTY_MASK |
	       ISR_NOACK_MASK,
	       dev->base + ISR_OFFSET);

	 
	dev->irq = platform_get_irq(pdev, 0);
	if (dev->irq < 0) {
		rc = dev->irq;
		goto probe_disable_clk;
	}

	 
	rc = devm_request_irq(&pdev->dev, dev->irq, bcm_kona_i2c_isr,
			      IRQF_SHARED, pdev->name, dev);
	if (rc) {
		dev_err(dev->device, "failed to request irq %i\n", dev->irq);
		goto probe_disable_clk;
	}

	 
	bcm_kona_i2c_send_cmd_to_ctrl(dev, BCM_CMD_NOACTION);

	 
	writel(PADCTL_PAD_OUT_EN_MASK, dev->base + PADCTL_OFFSET);

	 
	bcm_kona_i2c_disable_clock(dev);

	 
	clk_disable_unprepare(dev->external_clk);

	 
	adap = &dev->adapter;
	i2c_set_adapdata(adap, dev);
	adap->owner = THIS_MODULE;
	strscpy(adap->name, "Broadcom I2C adapter", sizeof(adap->name));
	adap->algo = &bcm_algo;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;

	rc = i2c_add_adapter(adap);
	if (rc)
		return rc;

	dev_info(dev->device, "device registered successfully\n");

	return 0;

probe_disable_clk:
	bcm_kona_i2c_disable_clock(dev);
	clk_disable_unprepare(dev->external_clk);

	return rc;
}

static void bcm_kona_i2c_remove(struct platform_device *pdev)
{
	struct bcm_kona_i2c_dev *dev = platform_get_drvdata(pdev);

	i2c_del_adapter(&dev->adapter);
}

static const struct of_device_id bcm_kona_i2c_of_match[] = {
	{.compatible = "brcm,kona-i2c",},
	{},
};
MODULE_DEVICE_TABLE(of, bcm_kona_i2c_of_match);

static struct platform_driver bcm_kona_i2c_driver = {
	.driver = {
		   .name = "bcm-kona-i2c",
		   .of_match_table = bcm_kona_i2c_of_match,
		   },
	.probe = bcm_kona_i2c_probe,
	.remove_new = bcm_kona_i2c_remove,
};
module_platform_driver(bcm_kona_i2c_driver);

MODULE_AUTHOR("Tim Kryger <tkryger@broadcom.com>");
MODULE_DESCRIPTION("Broadcom Kona I2C Driver");
MODULE_LICENSE("GPL v2");
