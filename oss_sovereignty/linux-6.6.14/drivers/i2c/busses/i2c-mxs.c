
 

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/stmp_device.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/dma/mxs-dma.h>

#define DRIVER_NAME "mxs-i2c"

#define MXS_I2C_CTRL0		(0x00)
#define MXS_I2C_CTRL0_SET	(0x04)
#define MXS_I2C_CTRL0_CLR	(0x08)

#define MXS_I2C_CTRL0_SFTRST			0x80000000
#define MXS_I2C_CTRL0_RUN			0x20000000
#define MXS_I2C_CTRL0_SEND_NAK_ON_LAST		0x02000000
#define MXS_I2C_CTRL0_PIO_MODE			0x01000000
#define MXS_I2C_CTRL0_RETAIN_CLOCK		0x00200000
#define MXS_I2C_CTRL0_POST_SEND_STOP		0x00100000
#define MXS_I2C_CTRL0_PRE_SEND_START		0x00080000
#define MXS_I2C_CTRL0_MASTER_MODE		0x00020000
#define MXS_I2C_CTRL0_DIRECTION			0x00010000
#define MXS_I2C_CTRL0_XFER_COUNT(v)		((v) & 0x0000FFFF)

#define MXS_I2C_TIMING0		(0x10)
#define MXS_I2C_TIMING1		(0x20)
#define MXS_I2C_TIMING2		(0x30)

#define MXS_I2C_CTRL1		(0x40)
#define MXS_I2C_CTRL1_SET	(0x44)
#define MXS_I2C_CTRL1_CLR	(0x48)

#define MXS_I2C_CTRL1_CLR_GOT_A_NAK		0x10000000
#define MXS_I2C_CTRL1_BUS_FREE_IRQ		0x80
#define MXS_I2C_CTRL1_DATA_ENGINE_CMPLT_IRQ	0x40
#define MXS_I2C_CTRL1_NO_SLAVE_ACK_IRQ		0x20
#define MXS_I2C_CTRL1_OVERSIZE_XFER_TERM_IRQ	0x10
#define MXS_I2C_CTRL1_EARLY_TERM_IRQ		0x08
#define MXS_I2C_CTRL1_MASTER_LOSS_IRQ		0x04
#define MXS_I2C_CTRL1_SLAVE_STOP_IRQ		0x02
#define MXS_I2C_CTRL1_SLAVE_IRQ			0x01

#define MXS_I2C_STAT		(0x50)
#define MXS_I2C_STAT_GOT_A_NAK			0x10000000
#define MXS_I2C_STAT_BUS_BUSY			0x00000800
#define MXS_I2C_STAT_CLK_GEN_BUSY		0x00000400

#define MXS_I2C_DATA(i2c)	((i2c->dev_type == MXS_I2C_V1) ? 0x60 : 0xa0)

#define MXS_I2C_DEBUG0_CLR(i2c)	((i2c->dev_type == MXS_I2C_V1) ? 0x78 : 0xb8)

#define MXS_I2C_DEBUG0_DMAREQ	0x80000000

#define MXS_I2C_IRQ_MASK	(MXS_I2C_CTRL1_DATA_ENGINE_CMPLT_IRQ | \
				 MXS_I2C_CTRL1_NO_SLAVE_ACK_IRQ | \
				 MXS_I2C_CTRL1_EARLY_TERM_IRQ | \
				 MXS_I2C_CTRL1_MASTER_LOSS_IRQ | \
				 MXS_I2C_CTRL1_SLAVE_STOP_IRQ | \
				 MXS_I2C_CTRL1_SLAVE_IRQ)


#define MXS_CMD_I2C_SELECT	(MXS_I2C_CTRL0_RETAIN_CLOCK |	\
				 MXS_I2C_CTRL0_PRE_SEND_START |	\
				 MXS_I2C_CTRL0_MASTER_MODE |	\
				 MXS_I2C_CTRL0_DIRECTION |	\
				 MXS_I2C_CTRL0_XFER_COUNT(1))

#define MXS_CMD_I2C_WRITE	(MXS_I2C_CTRL0_PRE_SEND_START |	\
				 MXS_I2C_CTRL0_MASTER_MODE |	\
				 MXS_I2C_CTRL0_DIRECTION)

#define MXS_CMD_I2C_READ	(MXS_I2C_CTRL0_SEND_NAK_ON_LAST | \
				 MXS_I2C_CTRL0_MASTER_MODE)

enum mxs_i2c_devtype {
	MXS_I2C_UNKNOWN = 0,
	MXS_I2C_V1,
	MXS_I2C_V2,
};

 
struct mxs_i2c_dev {
	struct device *dev;
	enum mxs_i2c_devtype dev_type;
	void __iomem *regs;
	struct completion cmd_complete;
	int cmd_err;
	struct i2c_adapter adapter;

	uint32_t timing0;
	uint32_t timing1;
	uint32_t timing2;

	 
	struct dma_chan			*dmach;
	uint32_t			pio_data[2];
	uint32_t			addr_data;
	struct scatterlist		sg_io[2];
	bool				dma_read;
};

static int mxs_i2c_reset(struct mxs_i2c_dev *i2c)
{
	int ret = stmp_reset_block(i2c->regs);
	if (ret)
		return ret;

	 
	writel(i2c->timing0, i2c->regs + MXS_I2C_TIMING0);
	writel(i2c->timing1, i2c->regs + MXS_I2C_TIMING1);
	writel(i2c->timing2, i2c->regs + MXS_I2C_TIMING2);

	writel(MXS_I2C_IRQ_MASK << 8, i2c->regs + MXS_I2C_CTRL1_SET);

	return 0;
}

static void mxs_i2c_dma_finish(struct mxs_i2c_dev *i2c)
{
	if (i2c->dma_read) {
		dma_unmap_sg(i2c->dev, &i2c->sg_io[0], 1, DMA_TO_DEVICE);
		dma_unmap_sg(i2c->dev, &i2c->sg_io[1], 1, DMA_FROM_DEVICE);
	} else {
		dma_unmap_sg(i2c->dev, i2c->sg_io, 2, DMA_TO_DEVICE);
	}
}

static void mxs_i2c_dma_irq_callback(void *param)
{
	struct mxs_i2c_dev *i2c = param;

	complete(&i2c->cmd_complete);
	mxs_i2c_dma_finish(i2c);
}

static int mxs_i2c_dma_setup_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msg, u8 *buf, uint32_t flags)
{
	struct dma_async_tx_descriptor *desc;
	struct mxs_i2c_dev *i2c = i2c_get_adapdata(adap);

	i2c->addr_data = i2c_8bit_addr_from_msg(msg);

	if (msg->flags & I2C_M_RD) {
		i2c->dma_read = true;

		 

		 
		i2c->pio_data[0] = MXS_CMD_I2C_SELECT;
		desc = dmaengine_prep_slave_sg(i2c->dmach,
					(struct scatterlist *)&i2c->pio_data[0],
					1, DMA_TRANS_NONE, 0);
		if (!desc) {
			dev_err(i2c->dev,
				"Failed to get PIO reg. write descriptor.\n");
			goto select_init_pio_fail;
		}

		 
		sg_init_one(&i2c->sg_io[0], &i2c->addr_data, 1);
		dma_map_sg(i2c->dev, &i2c->sg_io[0], 1, DMA_TO_DEVICE);
		desc = dmaengine_prep_slave_sg(i2c->dmach, &i2c->sg_io[0], 1,
					DMA_MEM_TO_DEV,
					DMA_PREP_INTERRUPT |
					MXS_DMA_CTRL_WAIT4END);
		if (!desc) {
			dev_err(i2c->dev,
				"Failed to get DMA data write descriptor.\n");
			goto select_init_dma_fail;
		}

		 

		 
		i2c->pio_data[1] = flags | MXS_CMD_I2C_READ |
				MXS_I2C_CTRL0_XFER_COUNT(msg->len);
		desc = dmaengine_prep_slave_sg(i2c->dmach,
					(struct scatterlist *)&i2c->pio_data[1],
					1, DMA_TRANS_NONE, DMA_PREP_INTERRUPT);
		if (!desc) {
			dev_err(i2c->dev,
				"Failed to get PIO reg. write descriptor.\n");
			goto select_init_dma_fail;
		}

		 
		sg_init_one(&i2c->sg_io[1], buf, msg->len);
		dma_map_sg(i2c->dev, &i2c->sg_io[1], 1, DMA_FROM_DEVICE);
		desc = dmaengine_prep_slave_sg(i2c->dmach, &i2c->sg_io[1], 1,
					DMA_DEV_TO_MEM,
					DMA_PREP_INTERRUPT |
					MXS_DMA_CTRL_WAIT4END);
		if (!desc) {
			dev_err(i2c->dev,
				"Failed to get DMA data write descriptor.\n");
			goto read_init_dma_fail;
		}
	} else {
		i2c->dma_read = false;

		 

		 
		i2c->pio_data[0] = flags | MXS_CMD_I2C_WRITE |
				MXS_I2C_CTRL0_XFER_COUNT(msg->len + 1);
		desc = dmaengine_prep_slave_sg(i2c->dmach,
					(struct scatterlist *)&i2c->pio_data[0],
					1, DMA_TRANS_NONE, 0);
		if (!desc) {
			dev_err(i2c->dev,
				"Failed to get PIO reg. write descriptor.\n");
			goto write_init_pio_fail;
		}

		 
		sg_init_table(i2c->sg_io, 2);
		sg_set_buf(&i2c->sg_io[0], &i2c->addr_data, 1);
		sg_set_buf(&i2c->sg_io[1], buf, msg->len);
		dma_map_sg(i2c->dev, i2c->sg_io, 2, DMA_TO_DEVICE);
		desc = dmaengine_prep_slave_sg(i2c->dmach, i2c->sg_io, 2,
					DMA_MEM_TO_DEV,
					DMA_PREP_INTERRUPT |
					MXS_DMA_CTRL_WAIT4END);
		if (!desc) {
			dev_err(i2c->dev,
				"Failed to get DMA data write descriptor.\n");
			goto write_init_dma_fail;
		}
	}

	 
	desc->callback = mxs_i2c_dma_irq_callback;
	desc->callback_param = i2c;

	 
	dmaengine_submit(desc);
	dma_async_issue_pending(i2c->dmach);
	return 0;

 
read_init_dma_fail:
	dma_unmap_sg(i2c->dev, &i2c->sg_io[1], 1, DMA_FROM_DEVICE);
select_init_dma_fail:
	dma_unmap_sg(i2c->dev, &i2c->sg_io[0], 1, DMA_TO_DEVICE);
select_init_pio_fail:
	dmaengine_terminate_sync(i2c->dmach);
	return -EINVAL;

 
write_init_dma_fail:
	dma_unmap_sg(i2c->dev, i2c->sg_io, 2, DMA_TO_DEVICE);
write_init_pio_fail:
	dmaengine_terminate_sync(i2c->dmach);
	return -EINVAL;
}

static int mxs_i2c_pio_wait_xfer_end(struct mxs_i2c_dev *i2c)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	while (readl(i2c->regs + MXS_I2C_CTRL0) & MXS_I2C_CTRL0_RUN) {
		if (readl(i2c->regs + MXS_I2C_CTRL1) &
				MXS_I2C_CTRL1_NO_SLAVE_ACK_IRQ)
			return -ENXIO;
		if (time_after(jiffies, timeout))
			return -ETIMEDOUT;
		cond_resched();
	}

	return 0;
}

static int mxs_i2c_pio_check_error_state(struct mxs_i2c_dev *i2c)
{
	u32 state;

	state = readl(i2c->regs + MXS_I2C_CTRL1_CLR) & MXS_I2C_IRQ_MASK;

	if (state & MXS_I2C_CTRL1_NO_SLAVE_ACK_IRQ)
		i2c->cmd_err = -ENXIO;
	else if (state & (MXS_I2C_CTRL1_EARLY_TERM_IRQ |
			  MXS_I2C_CTRL1_MASTER_LOSS_IRQ |
			  MXS_I2C_CTRL1_SLAVE_STOP_IRQ |
			  MXS_I2C_CTRL1_SLAVE_IRQ))
		i2c->cmd_err = -EIO;

	return i2c->cmd_err;
}

static void mxs_i2c_pio_trigger_cmd(struct mxs_i2c_dev *i2c, u32 cmd)
{
	u32 reg;

	writel(cmd, i2c->regs + MXS_I2C_CTRL0);

	 
	reg = readl(i2c->regs + MXS_I2C_CTRL0);
	reg |= MXS_I2C_CTRL0_RUN;
	writel(reg, i2c->regs + MXS_I2C_CTRL0);
}

 
static void mxs_i2c_pio_trigger_write_cmd(struct mxs_i2c_dev *i2c, u32 cmd,
					  u32 data)
{
	writel(cmd, i2c->regs + MXS_I2C_CTRL0);

	if (i2c->dev_type == MXS_I2C_V1)
		writel(MXS_I2C_CTRL0_PIO_MODE, i2c->regs + MXS_I2C_CTRL0_SET);

	writel(data, i2c->regs + MXS_I2C_DATA(i2c));
	writel(MXS_I2C_CTRL0_RUN, i2c->regs + MXS_I2C_CTRL0_SET);
}

static int mxs_i2c_pio_setup_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msg, uint32_t flags)
{
	struct mxs_i2c_dev *i2c = i2c_get_adapdata(adap);
	uint32_t addr_data = i2c_8bit_addr_from_msg(msg);
	uint32_t data = 0;
	int i, ret, xlen = 0, xmit = 0;
	uint32_t start;

	 
	writel(MXS_I2C_IRQ_MASK << 8, i2c->regs + MXS_I2C_CTRL1_CLR);

	 
	if (msg->flags & I2C_M_RD) {
		 
		BUG_ON(msg->len > 4);

		 
		mxs_i2c_pio_trigger_write_cmd(i2c, MXS_CMD_I2C_SELECT,
					      addr_data);

		ret = mxs_i2c_pio_wait_xfer_end(i2c);
		if (ret) {
			dev_dbg(i2c->dev,
				"PIO: Failed to send SELECT command!\n");
			goto cleanup;
		}

		 
		mxs_i2c_pio_trigger_cmd(i2c,
					MXS_CMD_I2C_READ | flags |
					MXS_I2C_CTRL0_XFER_COUNT(msg->len));

		ret = mxs_i2c_pio_wait_xfer_end(i2c);
		if (ret) {
			dev_dbg(i2c->dev,
				"PIO: Failed to send READ command!\n");
			goto cleanup;
		}

		data = readl(i2c->regs + MXS_I2C_DATA(i2c));
		for (i = 0; i < msg->len; i++) {
			msg->buf[i] = data & 0xff;
			data >>= 8;
		}
	} else {
		 

		 

		data = addr_data << 24;

		 
		start = MXS_I2C_CTRL0_PRE_SEND_START;

		 
		if (msg->len > 3)
			start |= MXS_I2C_CTRL0_RETAIN_CLOCK;

		for (i = 0; i < msg->len; i++) {
			data >>= 8;
			data |= (msg->buf[i] << 24);

			xmit = 0;

			 
			if (i + 1 == msg->len) {
				 
				start |= flags;
				 
				start &= ~MXS_I2C_CTRL0_RETAIN_CLOCK;
				xmit = 1;
			}

			 
			if ((i & 3) == 2)
				xmit = 1;

			 
			if (!xmit)
				continue;

			 

			if ((i % 4) == 3)
				xlen = 1;
			else
				xlen = (i % 4) + 2;

			data >>= (4 - xlen) * 8;

			dev_dbg(i2c->dev,
				"PIO: len=%i pos=%i total=%i [W%s%s%s]\n",
				xlen, i, msg->len,
				start & MXS_I2C_CTRL0_PRE_SEND_START ? "S" : "",
				start & MXS_I2C_CTRL0_POST_SEND_STOP ? "E" : "",
				start & MXS_I2C_CTRL0_RETAIN_CLOCK ? "C" : "");

			writel(MXS_I2C_DEBUG0_DMAREQ,
			       i2c->regs + MXS_I2C_DEBUG0_CLR(i2c));

			mxs_i2c_pio_trigger_write_cmd(i2c,
				start | MXS_I2C_CTRL0_MASTER_MODE |
				MXS_I2C_CTRL0_DIRECTION |
				MXS_I2C_CTRL0_XFER_COUNT(xlen), data);

			 
			start &= ~MXS_I2C_CTRL0_PRE_SEND_START;

			 
			ret = mxs_i2c_pio_wait_xfer_end(i2c);
			if (ret) {
				dev_dbg(i2c->dev,
					"PIO: Failed to finish WRITE cmd!\n");
				break;
			}

			 
			ret = readl(i2c->regs + MXS_I2C_STAT) &
				    MXS_I2C_STAT_GOT_A_NAK;
			if (ret) {
				ret = -ENXIO;
				goto cleanup;
			}
		}
	}

	 
	ret = mxs_i2c_pio_check_error_state(i2c);

cleanup:
	 
	writel(MXS_I2C_IRQ_MASK, i2c->regs + MXS_I2C_CTRL1_CLR);
	writel(MXS_I2C_IRQ_MASK << 8, i2c->regs + MXS_I2C_CTRL1_SET);

	 
	if (i2c->dev_type == MXS_I2C_V1)
		writel(MXS_I2C_CTRL0_PIO_MODE, i2c->regs + MXS_I2C_CTRL0_CLR);

	return ret;
}

 
static int mxs_i2c_xfer_msg(struct i2c_adapter *adap, struct i2c_msg *msg,
				int stop)
{
	struct mxs_i2c_dev *i2c = i2c_get_adapdata(adap);
	int ret;
	int flags;
	u8 *dma_buf;
	int use_pio = 0;
	unsigned long time_left;

	flags = stop ? MXS_I2C_CTRL0_POST_SEND_STOP : 0;

	dev_dbg(i2c->dev, "addr: 0x%04x, len: %d, flags: 0x%x, stop: %d\n",
		msg->addr, msg->len, msg->flags, stop);

	 
	if ((msg->flags & I2C_M_RD) && (msg->len <= 4))
		use_pio = 1;
	if (!(msg->flags & I2C_M_RD) && (msg->len < 7))
		use_pio = 1;

	i2c->cmd_err = 0;
	if (use_pio) {
		ret = mxs_i2c_pio_setup_xfer(adap, msg, flags);
		 
		if (ret && (ret != -ENXIO))
			mxs_i2c_reset(i2c);
	} else {
		dma_buf = i2c_get_dma_safe_msg_buf(msg, 1);
		if (!dma_buf)
			return -ENOMEM;

		reinit_completion(&i2c->cmd_complete);
		ret = mxs_i2c_dma_setup_xfer(adap, msg, dma_buf, flags);
		if (ret) {
			i2c_put_dma_safe_msg_buf(dma_buf, msg, false);
			return ret;
		}

		time_left = wait_for_completion_timeout(&i2c->cmd_complete,
						msecs_to_jiffies(1000));
		i2c_put_dma_safe_msg_buf(dma_buf, msg, true);
		if (!time_left)
			goto timeout;

		ret = i2c->cmd_err;
	}

	if (ret == -ENXIO) {
		 
		writel(MXS_I2C_CTRL1_CLR_GOT_A_NAK,
		       i2c->regs + MXS_I2C_CTRL1_SET);
	}

	 
	if (i2c->dev_type == MXS_I2C_V1)
		mxs_i2c_reset(i2c);

	dev_dbg(i2c->dev, "Done with err=%d\n", ret);

	return ret;

timeout:
	dev_dbg(i2c->dev, "Timeout!\n");
	mxs_i2c_dma_finish(i2c);
	ret = mxs_i2c_reset(i2c);
	if (ret)
		return ret;

	return -ETIMEDOUT;
}

static int mxs_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
			int num)
{
	int i;
	int err;

	for (i = 0; i < num; i++) {
		err = mxs_i2c_xfer_msg(adap, &msgs[i], i == (num - 1));
		if (err)
			return err;
	}

	return num;
}

static u32 mxs_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static irqreturn_t mxs_i2c_isr(int this_irq, void *dev_id)
{
	struct mxs_i2c_dev *i2c = dev_id;
	u32 stat = readl(i2c->regs + MXS_I2C_CTRL1) & MXS_I2C_IRQ_MASK;

	if (!stat)
		return IRQ_NONE;

	if (stat & MXS_I2C_CTRL1_NO_SLAVE_ACK_IRQ)
		i2c->cmd_err = -ENXIO;
	else if (stat & (MXS_I2C_CTRL1_EARLY_TERM_IRQ |
		    MXS_I2C_CTRL1_MASTER_LOSS_IRQ |
		    MXS_I2C_CTRL1_SLAVE_STOP_IRQ | MXS_I2C_CTRL1_SLAVE_IRQ))
		 
		i2c->cmd_err = -EIO;

	writel(stat, i2c->regs + MXS_I2C_CTRL1_CLR);

	return IRQ_HANDLED;
}

static const struct i2c_algorithm mxs_i2c_algo = {
	.master_xfer = mxs_i2c_xfer,
	.functionality = mxs_i2c_func,
};

static const struct i2c_adapter_quirks mxs_i2c_quirks = {
	.flags = I2C_AQ_NO_ZERO_LEN,
};

static void mxs_i2c_derive_timing(struct mxs_i2c_dev *i2c, uint32_t speed)
{
	 
	const uint32_t clk = 24000000;
	uint32_t divider;
	uint16_t high_count, low_count, rcv_count, xmit_count;
	uint32_t bus_free, leadin;
	struct device *dev = i2c->dev;

	divider = DIV_ROUND_UP(clk, speed);

	if (divider < 25) {
		 
		divider = 25;
		dev_warn(dev,
			"Speed too high (%u.%03u kHz), using %u.%03u kHz\n",
			speed / 1000, speed % 1000,
			clk / divider / 1000, clk / divider % 1000);
	} else if (divider > 1897) {
		 
		divider = 1897;
		dev_warn(dev,
			"Speed too low (%u.%03u kHz), using %u.%03u kHz\n",
			speed / 1000, speed % 1000,
			clk / divider / 1000, clk / divider % 1000);
	}

	 
	if (speed > I2C_MAX_STANDARD_MODE_FREQ) {
		 
		low_count = DIV_ROUND_CLOSEST(divider * 13, (13 + 6));
		high_count = DIV_ROUND_CLOSEST(divider * 6, (13 + 6));
		leadin = DIV_ROUND_UP(600 * (clk / 1000000), 1000);
		bus_free = DIV_ROUND_UP(1300 * (clk / 1000000), 1000);
	} else {
		 
		low_count = DIV_ROUND_CLOSEST(divider * 47, (47 + 40));
		high_count = DIV_ROUND_CLOSEST(divider * 40, (47 + 40));
		leadin = DIV_ROUND_UP(4700 * (clk / 1000000), 1000);
		bus_free = DIV_ROUND_UP(4700 * (clk / 1000000), 1000);
	}
	rcv_count = high_count * 3 / 8;
	xmit_count = low_count * 3 / 8;

	dev_dbg(dev,
		"speed=%u(actual %u) divider=%u low=%u high=%u xmit=%u rcv=%u leadin=%u bus_free=%u\n",
		speed, clk / divider, divider, low_count, high_count,
		xmit_count, rcv_count, leadin, bus_free);

	low_count -= 2;
	high_count -= 7;
	i2c->timing0 = (high_count << 16) | rcv_count;
	i2c->timing1 = (low_count << 16) | xmit_count;
	i2c->timing2 = (bus_free << 16 | leadin);
}

static int mxs_i2c_get_ofdata(struct mxs_i2c_dev *i2c)
{
	uint32_t speed;
	struct device *dev = i2c->dev;
	struct device_node *node = dev->of_node;
	int ret;

	ret = of_property_read_u32(node, "clock-frequency", &speed);
	if (ret) {
		dev_warn(dev, "No I2C speed selected, using 100kHz\n");
		speed = I2C_MAX_STANDARD_MODE_FREQ;
	}

	mxs_i2c_derive_timing(i2c, speed);

	return 0;
}

static const struct of_device_id mxs_i2c_dt_ids[] = {
	{ .compatible = "fsl,imx23-i2c", .data = (void *)MXS_I2C_V1, },
	{ .compatible = "fsl,imx28-i2c", .data = (void *)MXS_I2C_V2, },
	{   }
};
MODULE_DEVICE_TABLE(of, mxs_i2c_dt_ids);

static int mxs_i2c_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mxs_i2c_dev *i2c;
	struct i2c_adapter *adap;
	int err, irq;

	i2c = devm_kzalloc(dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	i2c->dev_type = (uintptr_t)of_device_get_match_data(&pdev->dev);

	i2c->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(i2c->regs))
		return PTR_ERR(i2c->regs);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	err = devm_request_irq(dev, irq, mxs_i2c_isr, 0, dev_name(dev), i2c);
	if (err)
		return err;

	i2c->dev = dev;

	init_completion(&i2c->cmd_complete);

	if (dev->of_node) {
		err = mxs_i2c_get_ofdata(i2c);
		if (err)
			return err;
	}

	 
	i2c->dmach = dma_request_chan(dev, "rx-tx");
	if (IS_ERR(i2c->dmach)) {
		return dev_err_probe(dev, PTR_ERR(i2c->dmach),
				     "Failed to request dma\n");
	}

	platform_set_drvdata(pdev, i2c);

	 
	err = mxs_i2c_reset(i2c);
	if (err)
		return err;

	adap = &i2c->adapter;
	strscpy(adap->name, "MXS I2C adapter", sizeof(adap->name));
	adap->owner = THIS_MODULE;
	adap->algo = &mxs_i2c_algo;
	adap->quirks = &mxs_i2c_quirks;
	adap->dev.parent = dev;
	adap->nr = pdev->id;
	adap->dev.of_node = pdev->dev.of_node;
	i2c_set_adapdata(adap, i2c);
	err = i2c_add_numbered_adapter(adap);
	if (err) {
		writel(MXS_I2C_CTRL0_SFTRST,
				i2c->regs + MXS_I2C_CTRL0_SET);
		return err;
	}

	return 0;
}

static void mxs_i2c_remove(struct platform_device *pdev)
{
	struct mxs_i2c_dev *i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c->adapter);

	if (i2c->dmach)
		dma_release_channel(i2c->dmach);

	writel(MXS_I2C_CTRL0_SFTRST, i2c->regs + MXS_I2C_CTRL0_SET);
}

static struct platform_driver mxs_i2c_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = mxs_i2c_dt_ids,
		   },
	.probe = mxs_i2c_probe,
	.remove_new = mxs_i2c_remove,
};

static int __init mxs_i2c_init(void)
{
	return platform_driver_register(&mxs_i2c_driver);
}
subsys_initcall(mxs_i2c_init);

static void __exit mxs_i2c_exit(void)
{
	platform_driver_unregister(&mxs_i2c_driver);
}
module_exit(mxs_i2c_exit);

MODULE_AUTHOR("Marek Vasut <marex@denx.de>");
MODULE_AUTHOR("Wolfram Sang <kernel@pengutronix.de>");
MODULE_DESCRIPTION("MXS I2C Bus Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
