
 

 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_data/i2c-xiic.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#define DRIVER_NAME "xiic-i2c"
#define DYNAMIC_MODE_READ_BROKEN_BIT	BIT(0)
#define SMBUS_BLOCK_READ_MIN_LEN	3

enum xilinx_i2c_state {
	STATE_DONE,
	STATE_ERROR,
	STATE_START
};

enum xiic_endian {
	LITTLE,
	BIG
};

enum i2c_scl_freq {
	REG_VALUES_100KHZ = 0,
	REG_VALUES_400KHZ = 1,
	REG_VALUES_1MHZ = 2
};

 
struct xiic_i2c {
	struct device *dev;
	void __iomem *base;
	struct completion completion;
	struct i2c_adapter adap;
	struct i2c_msg *tx_msg;
	struct mutex lock;
	unsigned int tx_pos;
	unsigned int nmsgs;
	struct i2c_msg *rx_msg;
	int rx_pos;
	enum xiic_endian endianness;
	struct clk *clk;
	enum xilinx_i2c_state state;
	bool singlemaster;
	bool dynamic;
	bool prev_msg_tx;
	u32 quirks;
	bool smbus_block_read;
	unsigned long input_clk;
	unsigned int i2c_clk;
};

struct xiic_version_data {
	u32 quirks;
};

 
struct timing_regs {
	unsigned int tsusta;
	unsigned int tsusto;
	unsigned int thdsta;
	unsigned int tsudat;
	unsigned int tbuf;
};

 
static const struct timing_regs timing_reg_values[] = {
	{ 5700, 5000, 4300, 550, 5000 },  
	{ 900, 900, 900, 400, 1600 },     
	{ 380, 380, 380, 170, 620 },      
};

#define XIIC_MSB_OFFSET 0
#define XIIC_REG_OFFSET (0x100 + XIIC_MSB_OFFSET)

 
#define XIIC_CR_REG_OFFSET   (0x00 + XIIC_REG_OFFSET)	 
#define XIIC_SR_REG_OFFSET   (0x04 + XIIC_REG_OFFSET)	 
#define XIIC_DTR_REG_OFFSET  (0x08 + XIIC_REG_OFFSET)	 
#define XIIC_DRR_REG_OFFSET  (0x0C + XIIC_REG_OFFSET)	 
#define XIIC_ADR_REG_OFFSET  (0x10 + XIIC_REG_OFFSET)	 
#define XIIC_TFO_REG_OFFSET  (0x14 + XIIC_REG_OFFSET)	 
#define XIIC_RFO_REG_OFFSET  (0x18 + XIIC_REG_OFFSET)	 
#define XIIC_TBA_REG_OFFSET  (0x1C + XIIC_REG_OFFSET)	 
#define XIIC_RFD_REG_OFFSET  (0x20 + XIIC_REG_OFFSET)	 
#define XIIC_GPO_REG_OFFSET  (0x24 + XIIC_REG_OFFSET)	 

 
#define XIIC_TSUSTA_REG_OFFSET (0x28 + XIIC_REG_OFFSET)  
#define XIIC_TSUSTO_REG_OFFSET (0x2C + XIIC_REG_OFFSET)  
#define XIIC_THDSTA_REG_OFFSET (0x30 + XIIC_REG_OFFSET)  
#define XIIC_TSUDAT_REG_OFFSET (0x34 + XIIC_REG_OFFSET)  
#define XIIC_TBUF_REG_OFFSET   (0x38 + XIIC_REG_OFFSET)  
#define XIIC_THIGH_REG_OFFSET  (0x3C + XIIC_REG_OFFSET)  
#define XIIC_TLOW_REG_OFFSET   (0x40 + XIIC_REG_OFFSET)  
#define XIIC_THDDAT_REG_OFFSET (0x44 + XIIC_REG_OFFSET)  

 
#define XIIC_CR_ENABLE_DEVICE_MASK        0x01	 
#define XIIC_CR_TX_FIFO_RESET_MASK        0x02	 
#define XIIC_CR_MSMS_MASK                 0x04	 
#define XIIC_CR_DIR_IS_TX_MASK            0x08	 
#define XIIC_CR_NO_ACK_MASK               0x10	 
#define XIIC_CR_REPEATED_START_MASK       0x20	 
#define XIIC_CR_GENERAL_CALL_MASK         0x40	 

 
#define XIIC_SR_GEN_CALL_MASK             0x01	 
#define XIIC_SR_ADDR_AS_SLAVE_MASK        0x02	 
#define XIIC_SR_BUS_BUSY_MASK             0x04	 
#define XIIC_SR_MSTR_RDING_SLAVE_MASK     0x08	 
#define XIIC_SR_TX_FIFO_FULL_MASK         0x10	 
#define XIIC_SR_RX_FIFO_FULL_MASK         0x20	 
#define XIIC_SR_RX_FIFO_EMPTY_MASK        0x40	 
#define XIIC_SR_TX_FIFO_EMPTY_MASK        0x80	 

 
#define XIIC_INTR_ARB_LOST_MASK           0x01	 
#define XIIC_INTR_TX_ERROR_MASK           0x02	 
#define XIIC_INTR_TX_EMPTY_MASK           0x04	 
#define XIIC_INTR_RX_FULL_MASK            0x08	 
#define XIIC_INTR_BNB_MASK                0x10	 
#define XIIC_INTR_AAS_MASK                0x20	 
#define XIIC_INTR_NAAS_MASK               0x40	 
#define XIIC_INTR_TX_HALF_MASK            0x80	 

 
#define IIC_RX_FIFO_DEPTH         16	 
#define IIC_TX_FIFO_DEPTH         16	 

 
#define XIIC_TX_INTERRUPTS                           \
(XIIC_INTR_TX_ERROR_MASK | XIIC_INTR_TX_EMPTY_MASK | XIIC_INTR_TX_HALF_MASK)

#define XIIC_TX_RX_INTERRUPTS (XIIC_INTR_RX_FULL_MASK | XIIC_TX_INTERRUPTS)

 
#define XIIC_TX_DYN_START_MASK            0x0100  
#define XIIC_TX_DYN_STOP_MASK             0x0200  

 
#define MAX_READ_LENGTH_DYNAMIC                255  

 
#define XIIC_DGIER_OFFSET    0x1C  
#define XIIC_IISR_OFFSET     0x20  
#define XIIC_IIER_OFFSET     0x28  
#define XIIC_RESETR_OFFSET   0x40  

#define XIIC_RESET_MASK             0xAUL

#define XIIC_PM_TIMEOUT		1000	 
 
#define XIIC_I2C_TIMEOUT	(msecs_to_jiffies(1000))
 
#define XIIC_XFER_TIMEOUT	(msecs_to_jiffies(10000))

 
#define XIIC_GINTR_ENABLE_MASK      0x80000000UL

#define xiic_tx_space(i2c) ((i2c)->tx_msg->len - (i2c)->tx_pos)
#define xiic_rx_space(i2c) ((i2c)->rx_msg->len - (i2c)->rx_pos)

static int xiic_start_xfer(struct xiic_i2c *i2c, struct i2c_msg *msgs, int num);
static void __xiic_start_xfer(struct xiic_i2c *i2c);

 

static inline void xiic_setreg8(struct xiic_i2c *i2c, int reg, u8 value)
{
	if (i2c->endianness == LITTLE)
		iowrite8(value, i2c->base + reg);
	else
		iowrite8(value, i2c->base + reg + 3);
}

static inline u8 xiic_getreg8(struct xiic_i2c *i2c, int reg)
{
	u8 ret;

	if (i2c->endianness == LITTLE)
		ret = ioread8(i2c->base + reg);
	else
		ret = ioread8(i2c->base + reg + 3);
	return ret;
}

static inline void xiic_setreg16(struct xiic_i2c *i2c, int reg, u16 value)
{
	if (i2c->endianness == LITTLE)
		iowrite16(value, i2c->base + reg);
	else
		iowrite16be(value, i2c->base + reg + 2);
}

static inline void xiic_setreg32(struct xiic_i2c *i2c, int reg, int value)
{
	if (i2c->endianness == LITTLE)
		iowrite32(value, i2c->base + reg);
	else
		iowrite32be(value, i2c->base + reg);
}

static inline int xiic_getreg32(struct xiic_i2c *i2c, int reg)
{
	u32 ret;

	if (i2c->endianness == LITTLE)
		ret = ioread32(i2c->base + reg);
	else
		ret = ioread32be(i2c->base + reg);
	return ret;
}

static inline void xiic_irq_dis(struct xiic_i2c *i2c, u32 mask)
{
	u32 ier = xiic_getreg32(i2c, XIIC_IIER_OFFSET);

	xiic_setreg32(i2c, XIIC_IIER_OFFSET, ier & ~mask);
}

static inline void xiic_irq_en(struct xiic_i2c *i2c, u32 mask)
{
	u32 ier = xiic_getreg32(i2c, XIIC_IIER_OFFSET);

	xiic_setreg32(i2c, XIIC_IIER_OFFSET, ier | mask);
}

static inline void xiic_irq_clr(struct xiic_i2c *i2c, u32 mask)
{
	u32 isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET);

	xiic_setreg32(i2c, XIIC_IISR_OFFSET, isr & mask);
}

static inline void xiic_irq_clr_en(struct xiic_i2c *i2c, u32 mask)
{
	xiic_irq_clr(i2c, mask);
	xiic_irq_en(i2c, mask);
}

static int xiic_clear_rx_fifo(struct xiic_i2c *i2c)
{
	u8 sr;
	unsigned long timeout;

	timeout = jiffies + XIIC_I2C_TIMEOUT;
	for (sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET);
		!(sr & XIIC_SR_RX_FIFO_EMPTY_MASK);
		sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET)) {
		xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);
		if (time_after(jiffies, timeout)) {
			dev_err(i2c->dev, "Failed to clear rx fifo\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int xiic_wait_tx_empty(struct xiic_i2c *i2c)
{
	u8 isr;
	unsigned long timeout;

	timeout = jiffies + XIIC_I2C_TIMEOUT;
	for (isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET);
		!(isr & XIIC_INTR_TX_EMPTY_MASK);
			isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET)) {
		if (time_after(jiffies, timeout)) {
			dev_err(i2c->dev, "Timeout waiting at Tx empty\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

 
static int xiic_setclk(struct xiic_i2c *i2c)
{
	unsigned int clk_in_mhz;
	unsigned int index = 0;
	u32 reg_val;

	dev_dbg(i2c->adap.dev.parent,
		"%s entry, i2c->input_clk: %ld, i2c->i2c_clk: %d\n",
		__func__, i2c->input_clk, i2c->i2c_clk);

	 
	if (!i2c->i2c_clk || !i2c->input_clk)
		return 0;

	clk_in_mhz = DIV_ROUND_UP(i2c->input_clk, 1000000);

	switch (i2c->i2c_clk) {
	case I2C_MAX_FAST_MODE_PLUS_FREQ:
		index = REG_VALUES_1MHZ;
		break;
	case I2C_MAX_FAST_MODE_FREQ:
		index = REG_VALUES_400KHZ;
		break;
	case I2C_MAX_STANDARD_MODE_FREQ:
		index = REG_VALUES_100KHZ;
		break;
	default:
		dev_warn(i2c->adap.dev.parent, "Unsupported scl frequency\n");
		return -EINVAL;
	}

	 

	 
	reg_val = (DIV_ROUND_UP(i2c->input_clk, 2 * i2c->i2c_clk)) - 7;
	if (reg_val == 0)
		return -EINVAL;

	xiic_setreg32(i2c, XIIC_THIGH_REG_OFFSET, reg_val - 1);

	 
	xiic_setreg32(i2c, XIIC_TLOW_REG_OFFSET, reg_val - 1);

	 
	reg_val = (timing_reg_values[index].tsusta * clk_in_mhz) / 1000;
	xiic_setreg32(i2c, XIIC_TSUSTA_REG_OFFSET, reg_val - 1);

	 
	reg_val = (timing_reg_values[index].tsusto * clk_in_mhz) / 1000;
	xiic_setreg32(i2c, XIIC_TSUSTO_REG_OFFSET, reg_val - 1);

	 
	reg_val = (timing_reg_values[index].thdsta * clk_in_mhz) / 1000;
	xiic_setreg32(i2c, XIIC_THDSTA_REG_OFFSET, reg_val - 1);

	 
	reg_val = (timing_reg_values[index].tsudat * clk_in_mhz) / 1000;
	xiic_setreg32(i2c, XIIC_TSUDAT_REG_OFFSET, reg_val - 1);

	 
	reg_val = (timing_reg_values[index].tbuf * clk_in_mhz) / 1000;
	xiic_setreg32(i2c, XIIC_TBUF_REG_OFFSET, reg_val - 1);

	 
	xiic_setreg32(i2c, XIIC_THDDAT_REG_OFFSET, 1);

	return 0;
}

static int xiic_reinit(struct xiic_i2c *i2c)
{
	int ret;

	xiic_setreg32(i2c, XIIC_RESETR_OFFSET, XIIC_RESET_MASK);

	ret = xiic_setclk(i2c);
	if (ret)
		return ret;

	 
	xiic_setreg8(i2c, XIIC_RFD_REG_OFFSET, IIC_RX_FIFO_DEPTH - 1);

	 
	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, XIIC_CR_TX_FIFO_RESET_MASK);

	 
	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, XIIC_CR_ENABLE_DEVICE_MASK);

	 
	ret = xiic_clear_rx_fifo(i2c);
	if (ret)
		return ret;

	 
	xiic_setreg32(i2c, XIIC_DGIER_OFFSET, XIIC_GINTR_ENABLE_MASK);

	xiic_irq_clr_en(i2c, XIIC_INTR_ARB_LOST_MASK);

	return 0;
}

static void xiic_deinit(struct xiic_i2c *i2c)
{
	u8 cr;

	xiic_setreg32(i2c, XIIC_RESETR_OFFSET, XIIC_RESET_MASK);

	 
	cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, cr & ~XIIC_CR_ENABLE_DEVICE_MASK);
}

static void xiic_smbus_block_read_setup(struct xiic_i2c *i2c)
{
	u8 rxmsg_len, rfd_set = 0;

	 
	i2c->rx_msg->flags &= ~I2C_M_RECV_LEN;

	 
	i2c->smbus_block_read = true;

	 
	rxmsg_len = xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);

	i2c->rx_msg->buf[i2c->rx_pos++] = rxmsg_len;

	 
	if (rxmsg_len <= I2C_SMBUS_BLOCK_MAX) {
		 
		if (rxmsg_len > IIC_RX_FIFO_DEPTH) {
			 
			rfd_set = IIC_RX_FIFO_DEPTH - 1;
			i2c->rx_msg->len = rxmsg_len + 1;
		} else if ((rxmsg_len == 1) ||
			(rxmsg_len == 0)) {
			 
			rfd_set = 0;
			i2c->rx_msg->len = SMBUS_BLOCK_READ_MIN_LEN;
		} else {
			 
			rfd_set = rxmsg_len - 2;
			i2c->rx_msg->len = rxmsg_len + 1;
		}
		xiic_setreg8(i2c, XIIC_RFD_REG_OFFSET, rfd_set);

		return;
	}

	 
	i2c->tx_msg->len = 3;
	i2c->smbus_block_read = false;
	dev_err(i2c->adap.dev.parent, "smbus_block_read Invalid msg length\n");
}

static void xiic_read_rx(struct xiic_i2c *i2c)
{
	u8 bytes_in_fifo, cr = 0, bytes_to_read = 0;
	u32 bytes_rem = 0;
	int i;

	bytes_in_fifo = xiic_getreg8(i2c, XIIC_RFO_REG_OFFSET) + 1;

	dev_dbg(i2c->adap.dev.parent,
		"%s entry, bytes in fifo: %d, rem: %d, SR: 0x%x, CR: 0x%x\n",
		__func__, bytes_in_fifo, xiic_rx_space(i2c),
		xiic_getreg8(i2c, XIIC_SR_REG_OFFSET),
		xiic_getreg8(i2c, XIIC_CR_REG_OFFSET));

	if (bytes_in_fifo > xiic_rx_space(i2c))
		bytes_in_fifo = xiic_rx_space(i2c);

	bytes_to_read = bytes_in_fifo;

	if (!i2c->dynamic) {
		bytes_rem = xiic_rx_space(i2c) - bytes_in_fifo;

		 
		if (i2c->rx_msg->flags & I2C_M_RECV_LEN) {
			xiic_smbus_block_read_setup(i2c);
			return;
		}

		if (bytes_rem > IIC_RX_FIFO_DEPTH) {
			bytes_to_read = bytes_in_fifo;
		} else if (bytes_rem > 1) {
			bytes_to_read = bytes_rem - 1;
		} else if (bytes_rem == 1) {
			bytes_to_read = 1;
			 
			cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
			xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, cr |
					XIIC_CR_NO_ACK_MASK);
		} else if (bytes_rem == 0) {
			bytes_to_read = bytes_in_fifo;

			 
			if (i2c->nmsgs == 1) {
				cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
				xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, cr &
						~XIIC_CR_MSMS_MASK);
			}

			 
			cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
			xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, cr &
					~XIIC_CR_NO_ACK_MASK);
		}
	}

	 
	for (i = 0; i < bytes_to_read; i++) {
		i2c->rx_msg->buf[i2c->rx_pos++] =
			xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);
	}

	if (i2c->dynamic) {
		u8 bytes;

		 
		bytes = min_t(u8, xiic_rx_space(i2c), IIC_RX_FIFO_DEPTH);
		bytes--;
		xiic_setreg8(i2c, XIIC_RFD_REG_OFFSET, bytes);
	}
}

static int xiic_tx_fifo_space(struct xiic_i2c *i2c)
{
	 
	return IIC_TX_FIFO_DEPTH - xiic_getreg8(i2c, XIIC_TFO_REG_OFFSET) - 1;
}

static void xiic_fill_tx_fifo(struct xiic_i2c *i2c)
{
	u8 fifo_space = xiic_tx_fifo_space(i2c);
	int len = xiic_tx_space(i2c);

	len = (len > fifo_space) ? fifo_space : len;

	dev_dbg(i2c->adap.dev.parent, "%s entry, len: %d, fifo space: %d\n",
		__func__, len, fifo_space);

	while (len--) {
		u16 data = i2c->tx_msg->buf[i2c->tx_pos++];

		if (!xiic_tx_space(i2c) && i2c->nmsgs == 1) {
			 
			if (i2c->dynamic) {
				data |= XIIC_TX_DYN_STOP_MASK;
			} else {
				u8 cr;
				int status;

				 
				status = xiic_wait_tx_empty(i2c);
				if (status)
					return;

				 
				cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
				xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, cr &
					     ~XIIC_CR_MSMS_MASK);
			}
			dev_dbg(i2c->adap.dev.parent, "%s TX STOP\n", __func__);
		}
		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET, data);
	}
}

static void xiic_wakeup(struct xiic_i2c *i2c, enum xilinx_i2c_state code)
{
	i2c->tx_msg = NULL;
	i2c->rx_msg = NULL;
	i2c->nmsgs = 0;
	i2c->state = code;
	complete(&i2c->completion);
}

static irqreturn_t xiic_process(int irq, void *dev_id)
{
	struct xiic_i2c *i2c = dev_id;
	u32 pend, isr, ier;
	u32 clr = 0;
	int xfer_more = 0;
	int wakeup_req = 0;
	enum xilinx_i2c_state wakeup_code = STATE_DONE;
	int ret;

	 
	mutex_lock(&i2c->lock);
	isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET);
	ier = xiic_getreg32(i2c, XIIC_IIER_OFFSET);
	pend = isr & ier;

	dev_dbg(i2c->adap.dev.parent, "%s: IER: 0x%x, ISR: 0x%x, pend: 0x%x\n",
		__func__, ier, isr, pend);
	dev_dbg(i2c->adap.dev.parent, "%s: SR: 0x%x, msg: %p, nmsgs: %d\n",
		__func__, xiic_getreg8(i2c, XIIC_SR_REG_OFFSET),
		i2c->tx_msg, i2c->nmsgs);
	dev_dbg(i2c->adap.dev.parent, "%s, ISR: 0x%x, CR: 0x%x\n",
		__func__, xiic_getreg32(i2c, XIIC_IISR_OFFSET),
		xiic_getreg8(i2c, XIIC_CR_REG_OFFSET));

	 
	if ((pend & XIIC_INTR_ARB_LOST_MASK) ||
	    ((pend & XIIC_INTR_TX_ERROR_MASK) &&
	    !(pend & XIIC_INTR_RX_FULL_MASK))) {
		 

		dev_dbg(i2c->adap.dev.parent, "%s error\n", __func__);

		 
		ret = xiic_reinit(i2c);
		if (ret < 0)
			dev_dbg(i2c->adap.dev.parent, "reinit failed\n");

		if (i2c->rx_msg) {
			wakeup_req = 1;
			wakeup_code = STATE_ERROR;
		}
		if (i2c->tx_msg) {
			wakeup_req = 1;
			wakeup_code = STATE_ERROR;
		}
		 
		goto out;
	}
	if (pend & XIIC_INTR_RX_FULL_MASK) {
		 

		clr |= XIIC_INTR_RX_FULL_MASK;
		if (!i2c->rx_msg) {
			dev_dbg(i2c->adap.dev.parent,
				"%s unexpected RX IRQ\n", __func__);
			xiic_clear_rx_fifo(i2c);
			goto out;
		}

		xiic_read_rx(i2c);
		if (xiic_rx_space(i2c) == 0) {
			 
			i2c->rx_msg = NULL;

			 
			clr |= (isr & XIIC_INTR_TX_ERROR_MASK);

			dev_dbg(i2c->adap.dev.parent,
				"%s end of message, nmsgs: %d\n",
				__func__, i2c->nmsgs);

			 
			if (i2c->nmsgs > 1) {
				i2c->nmsgs--;
				i2c->tx_msg++;
				dev_dbg(i2c->adap.dev.parent,
					"%s will start next...\n", __func__);
				xfer_more = 1;
			}
		}
	}
	if (pend & (XIIC_INTR_TX_EMPTY_MASK | XIIC_INTR_TX_HALF_MASK)) {
		 

		clr |= (pend &
			(XIIC_INTR_TX_EMPTY_MASK | XIIC_INTR_TX_HALF_MASK));

		if (!i2c->tx_msg) {
			dev_dbg(i2c->adap.dev.parent,
				"%s unexpected TX IRQ\n", __func__);
			goto out;
		}

		xiic_fill_tx_fifo(i2c);

		 
		if (!xiic_tx_space(i2c) && xiic_tx_fifo_space(i2c) >= 2) {
			dev_dbg(i2c->adap.dev.parent,
				"%s end of message sent, nmsgs: %d\n",
				__func__, i2c->nmsgs);
			if (i2c->nmsgs > 1) {
				i2c->nmsgs--;
				i2c->tx_msg++;
				xfer_more = 1;
			} else {
				xiic_irq_dis(i2c, XIIC_INTR_TX_HALF_MASK);

				dev_dbg(i2c->adap.dev.parent,
					"%s Got TX IRQ but no more to do...\n",
					__func__);
			}
		} else if (!xiic_tx_space(i2c) && (i2c->nmsgs == 1))
			 
			xiic_irq_dis(i2c, XIIC_INTR_TX_HALF_MASK);
	}

	if (pend & XIIC_INTR_BNB_MASK) {
		 
		clr |= XIIC_INTR_BNB_MASK;

		 
		xiic_irq_dis(i2c, XIIC_INTR_BNB_MASK);

		if (i2c->tx_msg && i2c->smbus_block_read) {
			i2c->smbus_block_read = false;
			 
			i2c->tx_msg->len = 1;
		}

		if (!i2c->tx_msg)
			goto out;

		wakeup_req = 1;

		if (i2c->nmsgs == 1 && !i2c->rx_msg &&
		    xiic_tx_space(i2c) == 0)
			wakeup_code = STATE_DONE;
		else
			wakeup_code = STATE_ERROR;
	}

out:
	dev_dbg(i2c->adap.dev.parent, "%s clr: 0x%x\n", __func__, clr);

	xiic_setreg32(i2c, XIIC_IISR_OFFSET, clr);
	if (xfer_more)
		__xiic_start_xfer(i2c);
	if (wakeup_req)
		xiic_wakeup(i2c, wakeup_code);

	WARN_ON(xfer_more && wakeup_req);

	mutex_unlock(&i2c->lock);
	return IRQ_HANDLED;
}

static int xiic_bus_busy(struct xiic_i2c *i2c)
{
	u8 sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET);

	return (sr & XIIC_SR_BUS_BUSY_MASK) ? -EBUSY : 0;
}

static int xiic_busy(struct xiic_i2c *i2c)
{
	int tries = 3;
	int err;

	if (i2c->tx_msg || i2c->rx_msg)
		return -EBUSY;

	 
	if (i2c->singlemaster) {
		return 0;
	}

	 
	err = xiic_bus_busy(i2c);
	while (err && tries--) {
		msleep(1);
		err = xiic_bus_busy(i2c);
	}

	return err;
}

static void xiic_start_recv(struct xiic_i2c *i2c)
{
	u16 rx_watermark;
	u8 cr = 0, rfd_set = 0;
	struct i2c_msg *msg = i2c->rx_msg = i2c->tx_msg;

	dev_dbg(i2c->adap.dev.parent, "%s entry, ISR: 0x%x, CR: 0x%x\n",
		__func__, xiic_getreg32(i2c, XIIC_IISR_OFFSET),
		xiic_getreg8(i2c, XIIC_CR_REG_OFFSET));

	 
	xiic_irq_dis(i2c, XIIC_INTR_TX_HALF_MASK | XIIC_INTR_TX_EMPTY_MASK);

	if (i2c->dynamic) {
		u8 bytes;
		u16 val;

		 
		xiic_irq_clr_en(i2c, XIIC_INTR_RX_FULL_MASK |
				XIIC_INTR_TX_ERROR_MASK);

		 
		rx_watermark = msg->len;
		bytes = min_t(u8, rx_watermark, IIC_RX_FIFO_DEPTH);

		if (rx_watermark > 0)
			bytes--;
		xiic_setreg8(i2c, XIIC_RFD_REG_OFFSET, bytes);

		 
		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET,
			      i2c_8bit_addr_from_msg(msg) |
			      XIIC_TX_DYN_START_MASK);

		 
		val = (i2c->nmsgs == 1) ? XIIC_TX_DYN_STOP_MASK : 0;
		val |= msg->len;

		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET, val);

		xiic_irq_clr_en(i2c, XIIC_INTR_BNB_MASK);
	} else {
		 
		if (i2c->prev_msg_tx) {
			int status;

			status = xiic_wait_tx_empty(i2c);
			if (status)
				return;
		}

		cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);

		 
		rx_watermark = msg->len;
		if (rx_watermark > IIC_RX_FIFO_DEPTH) {
			rfd_set = IIC_RX_FIFO_DEPTH - 1;
		} else if (rx_watermark == 1) {
			rfd_set = rx_watermark - 1;

			 
			if (!(i2c->rx_msg->flags & I2C_M_RECV_LEN)) {
				 
				cr |= XIIC_CR_NO_ACK_MASK;
			}
		} else if (rx_watermark == 0) {
			rfd_set = rx_watermark;
		} else {
			rfd_set = rx_watermark - 2;
		}
		 
		if (cr & XIIC_CR_MSMS_MASK) {
			 
			xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, (cr |
					XIIC_CR_REPEATED_START_MASK) &
					~(XIIC_CR_DIR_IS_TX_MASK));
		}

		xiic_setreg8(i2c, XIIC_RFD_REG_OFFSET, rfd_set);

		 
		xiic_irq_clr_en(i2c, XIIC_INTR_RX_FULL_MASK |
				XIIC_INTR_TX_ERROR_MASK);

		 
		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET,
			      i2c_8bit_addr_from_msg(msg));

		 
		if ((cr & XIIC_CR_MSMS_MASK) == 0) {
			xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, (cr |
					XIIC_CR_MSMS_MASK)
					& ~(XIIC_CR_DIR_IS_TX_MASK));
		}
		dev_dbg(i2c->adap.dev.parent, "%s end, ISR: 0x%x, CR: 0x%x\n",
			__func__, xiic_getreg32(i2c, XIIC_IISR_OFFSET),
			xiic_getreg8(i2c, XIIC_CR_REG_OFFSET));
	}

	if (i2c->nmsgs == 1)
		 
		xiic_irq_clr_en(i2c, XIIC_INTR_BNB_MASK);

	 
	i2c->tx_pos = msg->len;

	 
	xiic_setreg32(i2c, XIIC_DGIER_OFFSET, XIIC_GINTR_ENABLE_MASK);

	i2c->prev_msg_tx = false;
}

static void xiic_start_send(struct xiic_i2c *i2c)
{
	u8 cr = 0;
	u16 data;
	struct i2c_msg *msg = i2c->tx_msg;

	dev_dbg(i2c->adap.dev.parent, "%s entry, msg: %p, len: %d",
		__func__, msg, msg->len);
	dev_dbg(i2c->adap.dev.parent, "%s entry, ISR: 0x%x, CR: 0x%x\n",
		__func__, xiic_getreg32(i2c, XIIC_IISR_OFFSET),
		xiic_getreg8(i2c, XIIC_CR_REG_OFFSET));

	if (i2c->dynamic) {
		 
		data = i2c_8bit_addr_from_msg(msg) |
				XIIC_TX_DYN_START_MASK;

		if (i2c->nmsgs == 1 && msg->len == 0)
			 
			data |= XIIC_TX_DYN_STOP_MASK;

		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET, data);

		 
		xiic_irq_clr_en(i2c, XIIC_INTR_TX_EMPTY_MASK |
				XIIC_INTR_TX_ERROR_MASK |
				XIIC_INTR_BNB_MASK |
				((i2c->nmsgs > 1 || xiic_tx_space(i2c)) ?
				XIIC_INTR_TX_HALF_MASK : 0));

		xiic_fill_tx_fifo(i2c);
	} else {
		 
		if (i2c->prev_msg_tx) {
			int status;

			status = xiic_wait_tx_empty(i2c);
			if (status)
				return;
		}
		 
		cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
		if (cr & XIIC_CR_MSMS_MASK) {
			 
			xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, (cr |
					XIIC_CR_REPEATED_START_MASK |
					XIIC_CR_DIR_IS_TX_MASK) &
					~(XIIC_CR_NO_ACK_MASK));
		}

		 
		data = i2c_8bit_addr_from_msg(msg);
		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET, data);

		 
		xiic_fill_tx_fifo(i2c);

		if ((cr & XIIC_CR_MSMS_MASK) == 0) {
			 
			cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
			xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, cr |
					XIIC_CR_MSMS_MASK |
					XIIC_CR_DIR_IS_TX_MASK);
		}

		 
		xiic_irq_clr_en(i2c, XIIC_INTR_TX_EMPTY_MASK |
				XIIC_INTR_TX_ERROR_MASK |
				XIIC_INTR_BNB_MASK);
	}
	i2c->prev_msg_tx = true;
}

static void __xiic_start_xfer(struct xiic_i2c *i2c)
{
	int fifo_space = xiic_tx_fifo_space(i2c);

	dev_dbg(i2c->adap.dev.parent, "%s entry, msg: %p, fifos space: %d\n",
		__func__, i2c->tx_msg, fifo_space);

	if (!i2c->tx_msg)
		return;

	i2c->rx_pos = 0;
	i2c->tx_pos = 0;
	i2c->state = STATE_START;
	if (i2c->tx_msg->flags & I2C_M_RD) {
		 
		xiic_start_recv(i2c);
	} else {
		xiic_start_send(i2c);
	}
}

static int xiic_start_xfer(struct xiic_i2c *i2c, struct i2c_msg *msgs, int num)
{
	bool broken_read, max_read_len, smbus_blk_read;
	int ret, count;

	mutex_lock(&i2c->lock);

	ret = xiic_busy(i2c);
	if (ret)
		goto out;

	i2c->tx_msg = msgs;
	i2c->rx_msg = NULL;
	i2c->nmsgs = num;
	init_completion(&i2c->completion);

	 
	i2c->dynamic = true;

	 
	i2c->prev_msg_tx = false;

	 
	for (count = 0; count < i2c->nmsgs; count++) {
		broken_read = (i2c->quirks & DYNAMIC_MODE_READ_BROKEN_BIT) &&
				(i2c->tx_msg[count].flags & I2C_M_RD);
		max_read_len = (i2c->tx_msg[count].flags & I2C_M_RD) &&
				(i2c->tx_msg[count].len > MAX_READ_LENGTH_DYNAMIC);
		smbus_blk_read = (i2c->tx_msg[count].flags & I2C_M_RECV_LEN);

		if (broken_read || max_read_len || smbus_blk_read) {
			i2c->dynamic = false;
			break;
		}
	}

	ret = xiic_reinit(i2c);
	if (!ret)
		__xiic_start_xfer(i2c);

out:
	mutex_unlock(&i2c->lock);

	return ret;
}

static int xiic_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct xiic_i2c *i2c = i2c_get_adapdata(adap);
	int err;

	dev_dbg(adap->dev.parent, "%s entry SR: 0x%x\n", __func__,
		xiic_getreg8(i2c, XIIC_SR_REG_OFFSET));

	err = pm_runtime_resume_and_get(i2c->dev);
	if (err < 0)
		return err;

	err = xiic_start_xfer(i2c, msgs, num);
	if (err < 0) {
		dev_err(adap->dev.parent, "Error xiic_start_xfer\n");
		goto out;
	}

	err = wait_for_completion_timeout(&i2c->completion, XIIC_XFER_TIMEOUT);
	mutex_lock(&i2c->lock);
	if (err == 0) {	 
		i2c->tx_msg = NULL;
		i2c->rx_msg = NULL;
		i2c->nmsgs = 0;
		err = -ETIMEDOUT;
	} else {
		err = (i2c->state == STATE_DONE) ? num : -EIO;
	}
	mutex_unlock(&i2c->lock);

out:
	pm_runtime_mark_last_busy(i2c->dev);
	pm_runtime_put_autosuspend(i2c->dev);
	return err;
}

static u32 xiic_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_SMBUS_BLOCK_DATA;
}

static const struct i2c_algorithm xiic_algorithm = {
	.master_xfer = xiic_xfer,
	.functionality = xiic_func,
};

static const struct i2c_adapter xiic_adapter = {
	.owner = THIS_MODULE,
	.class = I2C_CLASS_DEPRECATED,
	.algo = &xiic_algorithm,
};

#if defined(CONFIG_OF)
static const struct xiic_version_data xiic_2_00 = {
	.quirks = DYNAMIC_MODE_READ_BROKEN_BIT,
};

static const struct of_device_id xiic_of_match[] = {
	{ .compatible = "xlnx,xps-iic-2.00.a", .data = &xiic_2_00 },
	{ .compatible = "xlnx,axi-iic-2.1", },
	{},
};
MODULE_DEVICE_TABLE(of, xiic_of_match);
#endif

static int xiic_i2c_probe(struct platform_device *pdev)
{
	struct xiic_i2c *i2c;
	struct xiic_i2c_platform_data *pdata;
	const struct of_device_id *match;
	struct resource *res;
	int ret, irq;
	u8 i;
	u32 sr;

	i2c = devm_kzalloc(&pdev->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	match = of_match_node(xiic_of_match, pdev->dev.of_node);
	if (match && match->data) {
		const struct xiic_version_data *data = match->data;

		i2c->quirks = data->quirks;
	}

	i2c->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(i2c->base))
		return PTR_ERR(i2c->base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	pdata = dev_get_platdata(&pdev->dev);

	 
	platform_set_drvdata(pdev, i2c);
	i2c->adap = xiic_adapter;
	i2c_set_adapdata(&i2c->adap, i2c);
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.dev.of_node = pdev->dev.of_node;
	snprintf(i2c->adap.name, sizeof(i2c->adap.name),
		 DRIVER_NAME " %s", pdev->name);

	mutex_init(&i2c->lock);

	i2c->clk = devm_clk_get_enabled(&pdev->dev, NULL);
	if (IS_ERR(i2c->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(i2c->clk),
				     "failed to enable input clock.\n");

	i2c->dev = &pdev->dev;
	pm_runtime_set_autosuspend_delay(i2c->dev, XIIC_PM_TIMEOUT);
	pm_runtime_use_autosuspend(i2c->dev);
	pm_runtime_set_active(i2c->dev);
	pm_runtime_enable(i2c->dev);

	 
	i2c->input_clk = clk_get_rate(i2c->clk);
	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency",
				   &i2c->i2c_clk);
	 
	if (ret || i2c->i2c_clk > I2C_MAX_FAST_MODE_PLUS_FREQ)
		i2c->i2c_clk = 0;

	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					xiic_process, IRQF_ONESHOT,
					pdev->name, i2c);

	if (ret < 0) {
		dev_err(&pdev->dev, "Cannot claim IRQ\n");
		goto err_pm_disable;
	}

	i2c->singlemaster =
		of_property_read_bool(pdev->dev.of_node, "single-master");

	 
	i2c->endianness = LITTLE;
	xiic_setreg32(i2c, XIIC_CR_REG_OFFSET, XIIC_CR_TX_FIFO_RESET_MASK);
	 
	sr = xiic_getreg32(i2c, XIIC_SR_REG_OFFSET);
	if (!(sr & XIIC_SR_TX_FIFO_EMPTY_MASK))
		i2c->endianness = BIG;

	ret = xiic_reinit(i2c);
	if (ret < 0) {
		dev_err(&pdev->dev, "Cannot xiic_reinit\n");
		goto err_pm_disable;
	}

	 
	ret = i2c_add_adapter(&i2c->adap);
	if (ret) {
		xiic_deinit(i2c);
		goto err_pm_disable;
	}

	if (pdata) {
		 
		for (i = 0; i < pdata->num_devices; i++)
			i2c_new_client_device(&i2c->adap, pdata->devices + i);
	}

	dev_dbg(&pdev->dev, "mmio %08lx irq %d scl clock frequency %d\n",
		(unsigned long)res->start, irq, i2c->i2c_clk);

	return 0;

err_pm_disable:
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static void xiic_i2c_remove(struct platform_device *pdev)
{
	struct xiic_i2c *i2c = platform_get_drvdata(pdev);
	int ret;

	 
	i2c_del_adapter(&i2c->adap);

	ret = pm_runtime_get_sync(i2c->dev);

	if (ret < 0)
		dev_warn(&pdev->dev, "Failed to activate device for removal (%pe)\n",
			 ERR_PTR(ret));
	else
		xiic_deinit(i2c);

	pm_runtime_put_sync(i2c->dev);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_dont_use_autosuspend(&pdev->dev);
}

static int __maybe_unused xiic_i2c_runtime_suspend(struct device *dev)
{
	struct xiic_i2c *i2c = dev_get_drvdata(dev);

	clk_disable(i2c->clk);

	return 0;
}

static int __maybe_unused xiic_i2c_runtime_resume(struct device *dev)
{
	struct xiic_i2c *i2c = dev_get_drvdata(dev);
	int ret;

	ret = clk_enable(i2c->clk);
	if (ret) {
		dev_err(dev, "Cannot enable clock.\n");
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops xiic_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(xiic_i2c_runtime_suspend,
			   xiic_i2c_runtime_resume, NULL)
};

static struct platform_driver xiic_i2c_driver = {
	.probe   = xiic_i2c_probe,
	.remove_new = xiic_i2c_remove,
	.driver  = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(xiic_of_match),
		.pm = &xiic_dev_pm_ops,
	},
};

module_platform_driver(xiic_i2c_driver);

MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("info@mocean-labs.com");
MODULE_DESCRIPTION("Xilinx I2C bus driver");
MODULE_LICENSE("GPL v2");
