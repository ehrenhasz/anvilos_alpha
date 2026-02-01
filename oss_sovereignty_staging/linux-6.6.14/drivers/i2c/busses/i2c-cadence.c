
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset.h>

 
#define CDNS_I2C_CR_OFFSET		0x00  
#define CDNS_I2C_SR_OFFSET		0x04  
#define CDNS_I2C_ADDR_OFFSET		0x08  
#define CDNS_I2C_DATA_OFFSET		0x0C  
#define CDNS_I2C_ISR_OFFSET		0x10  
#define CDNS_I2C_XFER_SIZE_OFFSET	0x14  
#define CDNS_I2C_TIME_OUT_OFFSET	0x1C  
#define CDNS_I2C_IMR_OFFSET		0x20  
#define CDNS_I2C_IER_OFFSET		0x24  
#define CDNS_I2C_IDR_OFFSET		0x28  

 
#define CDNS_I2C_CR_HOLD		BIT(4)  
#define CDNS_I2C_CR_ACK_EN		BIT(3)
#define CDNS_I2C_CR_NEA			BIT(2)
#define CDNS_I2C_CR_MS			BIT(1)
 
#define CDNS_I2C_CR_RW			BIT(0)
 
#define CDNS_I2C_CR_CLR_FIFO		BIT(6)
#define CDNS_I2C_CR_DIVA_SHIFT		14
#define CDNS_I2C_CR_DIVA_MASK		(3 << CDNS_I2C_CR_DIVA_SHIFT)
#define CDNS_I2C_CR_DIVB_SHIFT		8
#define CDNS_I2C_CR_DIVB_MASK		(0x3f << CDNS_I2C_CR_DIVB_SHIFT)

#define CDNS_I2C_CR_MASTER_EN_MASK	(CDNS_I2C_CR_NEA | \
					 CDNS_I2C_CR_ACK_EN | \
					 CDNS_I2C_CR_MS)

#define CDNS_I2C_CR_SLAVE_EN_MASK	~CDNS_I2C_CR_MASTER_EN_MASK

 
#define CDNS_I2C_SR_BA		BIT(8)
#define CDNS_I2C_SR_TXDV	BIT(6)
#define CDNS_I2C_SR_RXDV	BIT(5)
#define CDNS_I2C_SR_RXRW	BIT(3)

 
#define CDNS_I2C_ADDR_MASK	0x000003FF  

 
#define CDNS_I2C_IXR_ARB_LOST		BIT(9)
#define CDNS_I2C_IXR_RX_UNF		BIT(7)
#define CDNS_I2C_IXR_TX_OVF		BIT(6)
#define CDNS_I2C_IXR_RX_OVF		BIT(5)
#define CDNS_I2C_IXR_SLV_RDY		BIT(4)
#define CDNS_I2C_IXR_TO			BIT(3)
#define CDNS_I2C_IXR_NACK		BIT(2)
#define CDNS_I2C_IXR_DATA		BIT(1)
#define CDNS_I2C_IXR_COMP		BIT(0)

#define CDNS_I2C_IXR_ALL_INTR_MASK	(CDNS_I2C_IXR_ARB_LOST | \
					 CDNS_I2C_IXR_RX_UNF | \
					 CDNS_I2C_IXR_TX_OVF | \
					 CDNS_I2C_IXR_RX_OVF | \
					 CDNS_I2C_IXR_SLV_RDY | \
					 CDNS_I2C_IXR_TO | \
					 CDNS_I2C_IXR_NACK | \
					 CDNS_I2C_IXR_DATA | \
					 CDNS_I2C_IXR_COMP)

#define CDNS_I2C_IXR_ERR_INTR_MASK	(CDNS_I2C_IXR_ARB_LOST | \
					 CDNS_I2C_IXR_RX_UNF | \
					 CDNS_I2C_IXR_TX_OVF | \
					 CDNS_I2C_IXR_RX_OVF | \
					 CDNS_I2C_IXR_NACK)

#define CDNS_I2C_ENABLED_INTR_MASK	(CDNS_I2C_IXR_ARB_LOST | \
					 CDNS_I2C_IXR_RX_UNF | \
					 CDNS_I2C_IXR_TX_OVF | \
					 CDNS_I2C_IXR_RX_OVF | \
					 CDNS_I2C_IXR_NACK | \
					 CDNS_I2C_IXR_DATA | \
					 CDNS_I2C_IXR_COMP)

#define CDNS_I2C_IXR_SLAVE_INTR_MASK	(CDNS_I2C_IXR_RX_UNF | \
					 CDNS_I2C_IXR_TX_OVF | \
					 CDNS_I2C_IXR_RX_OVF | \
					 CDNS_I2C_IXR_TO | \
					 CDNS_I2C_IXR_NACK | \
					 CDNS_I2C_IXR_DATA | \
					 CDNS_I2C_IXR_COMP)

#define CDNS_I2C_TIMEOUT		msecs_to_jiffies(1000)
 
#define CNDS_I2C_PM_TIMEOUT		1000	 

#define CDNS_I2C_FIFO_DEPTH_DEFAULT	16
#define CDNS_I2C_MAX_TRANSFER_SIZE	255
 
#define CDNS_I2C_TRANSFER_SIZE(max)	((max) - 3)

#define DRIVER_NAME		"cdns-i2c"

#define CDNS_I2C_DIVA_MAX	4
#define CDNS_I2C_DIVB_MAX	64

#define CDNS_I2C_TIMEOUT_MAX	0xFF

#define CDNS_I2C_BROKEN_HOLD_BIT	BIT(0)
#define CDNS_I2C_POLL_US	100000
#define CDNS_I2C_TIMEOUT_US	500000

#define cdns_i2c_readreg(offset)       readl_relaxed(id->membase + offset)
#define cdns_i2c_writereg(val, offset) writel_relaxed(val, id->membase + offset)

#if IS_ENABLED(CONFIG_I2C_SLAVE)
 
enum cdns_i2c_mode {
	CDNS_I2C_MODE_SLAVE,
	CDNS_I2C_MODE_MASTER,
};

 
enum cdns_i2c_slave_state {
	CDNS_I2C_SLAVE_STATE_IDLE,
	CDNS_I2C_SLAVE_STATE_SEND,
	CDNS_I2C_SLAVE_STATE_RECV,
};
#endif

 
struct cdns_i2c {
	struct device		*dev;
	void __iomem *membase;
	struct i2c_adapter adap;
	struct i2c_msg *p_msg;
	int err_status;
	struct completion xfer_done;
	unsigned char *p_send_buf;
	unsigned char *p_recv_buf;
	unsigned int send_count;
	unsigned int recv_count;
	unsigned int curr_recv_count;
	unsigned long input_clk;
	unsigned int i2c_clk;
	unsigned int bus_hold_flag;
	struct clk *clk;
	struct notifier_block clk_rate_change_nb;
	struct reset_control *reset;
	u32 quirks;
	u32 ctrl_reg;
	struct i2c_bus_recovery_info rinfo;
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	u16 ctrl_reg_diva_divb;
	struct i2c_client *slave;
	enum cdns_i2c_mode dev_mode;
	enum cdns_i2c_slave_state slave_state;
#endif
	u32 fifo_depth;
	unsigned int transfer_size;
};

struct cdns_platform_data {
	u32 quirks;
};

#define to_cdns_i2c(_nb)	container_of(_nb, struct cdns_i2c, \
					     clk_rate_change_nb)

 
static void cdns_i2c_clear_bus_hold(struct cdns_i2c *id)
{
	u32 reg = cdns_i2c_readreg(CDNS_I2C_CR_OFFSET);
	if (reg & CDNS_I2C_CR_HOLD)
		cdns_i2c_writereg(reg & ~CDNS_I2C_CR_HOLD, CDNS_I2C_CR_OFFSET);
}

static inline bool cdns_is_holdquirk(struct cdns_i2c *id, bool hold_wrkaround)
{
	return (hold_wrkaround &&
		(id->curr_recv_count == id->fifo_depth + 1));
}

#if IS_ENABLED(CONFIG_I2C_SLAVE)
static void cdns_i2c_set_mode(enum cdns_i2c_mode mode, struct cdns_i2c *id)
{
	 
	cdns_i2c_writereg(CDNS_I2C_IXR_ALL_INTR_MASK, CDNS_I2C_IDR_OFFSET);

	 
	cdns_i2c_writereg(CDNS_I2C_CR_CLR_FIFO, CDNS_I2C_CR_OFFSET);

	 
	id->dev_mode = mode;
	id->slave_state = CDNS_I2C_SLAVE_STATE_IDLE;

	switch (mode) {
	case CDNS_I2C_MODE_MASTER:
		 
		cdns_i2c_writereg(id->ctrl_reg_diva_divb |
				  CDNS_I2C_CR_MASTER_EN_MASK,
				  CDNS_I2C_CR_OFFSET);
		 
		usleep_range(115, 125);
		break;
	case CDNS_I2C_MODE_SLAVE:
		 
		cdns_i2c_writereg(id->ctrl_reg_diva_divb &
				  CDNS_I2C_CR_SLAVE_EN_MASK,
				  CDNS_I2C_CR_OFFSET);

		 
		cdns_i2c_writereg(id->slave->addr & CDNS_I2C_ADDR_MASK,
				  CDNS_I2C_ADDR_OFFSET);

		 
		cdns_i2c_writereg(CDNS_I2C_IXR_SLAVE_INTR_MASK,
				  CDNS_I2C_IER_OFFSET);
		break;
	}
}

static void cdns_i2c_slave_rcv_data(struct cdns_i2c *id)
{
	u8 bytes;
	unsigned char data;

	 
	if (id->slave_state == CDNS_I2C_SLAVE_STATE_IDLE) {
		id->slave_state = CDNS_I2C_SLAVE_STATE_RECV;
		i2c_slave_event(id->slave, I2C_SLAVE_WRITE_REQUESTED, NULL);
	}

	 
	bytes = cdns_i2c_readreg(CDNS_I2C_XFER_SIZE_OFFSET);

	 
	while (bytes--) {
		data = cdns_i2c_readreg(CDNS_I2C_DATA_OFFSET);
		i2c_slave_event(id->slave, I2C_SLAVE_WRITE_RECEIVED, &data);
	}
}

static void cdns_i2c_slave_send_data(struct cdns_i2c *id)
{
	u8 data;

	 
	if (id->slave_state == CDNS_I2C_SLAVE_STATE_IDLE) {
		id->slave_state = CDNS_I2C_SLAVE_STATE_SEND;
		i2c_slave_event(id->slave, I2C_SLAVE_READ_REQUESTED, &data);
	} else {
		i2c_slave_event(id->slave, I2C_SLAVE_READ_PROCESSED, &data);
	}

	 
	cdns_i2c_writereg(data, CDNS_I2C_DATA_OFFSET);
}

 
static irqreturn_t cdns_i2c_slave_isr(void *ptr)
{
	struct cdns_i2c *id = ptr;
	unsigned int isr_status, i2c_status;

	 
	isr_status = cdns_i2c_readreg(CDNS_I2C_ISR_OFFSET);
	cdns_i2c_writereg(isr_status, CDNS_I2C_ISR_OFFSET);

	 
	isr_status &= ~cdns_i2c_readreg(CDNS_I2C_IMR_OFFSET);

	 
	i2c_status = cdns_i2c_readreg(CDNS_I2C_SR_OFFSET);

	 
	if (i2c_status & CDNS_I2C_SR_RXRW) {
		 
		if (isr_status & CDNS_I2C_IXR_DATA)
			cdns_i2c_slave_send_data(id);

		if (isr_status & CDNS_I2C_IXR_COMP) {
			id->slave_state = CDNS_I2C_SLAVE_STATE_IDLE;
			i2c_slave_event(id->slave, I2C_SLAVE_STOP, NULL);
		}
	} else {
		 
		if (isr_status & CDNS_I2C_IXR_DATA)
			cdns_i2c_slave_rcv_data(id);

		if (isr_status & CDNS_I2C_IXR_COMP) {
			cdns_i2c_slave_rcv_data(id);
			id->slave_state = CDNS_I2C_SLAVE_STATE_IDLE;
			i2c_slave_event(id->slave, I2C_SLAVE_STOP, NULL);
		}
	}

	 
	if (isr_status & (CDNS_I2C_IXR_NACK | CDNS_I2C_IXR_RX_OVF |
			  CDNS_I2C_IXR_RX_UNF | CDNS_I2C_IXR_TX_OVF)) {
		id->slave_state = CDNS_I2C_SLAVE_STATE_IDLE;
		i2c_slave_event(id->slave, I2C_SLAVE_STOP, NULL);
		cdns_i2c_writereg(CDNS_I2C_CR_CLR_FIFO, CDNS_I2C_CR_OFFSET);
	}

	return IRQ_HANDLED;
}
#endif

 
static irqreturn_t cdns_i2c_master_isr(void *ptr)
{
	unsigned int isr_status, avail_bytes;
	unsigned int bytes_to_send;
	bool updatetx;
	struct cdns_i2c *id = ptr;
	 
	int done_flag = 0;
	irqreturn_t status = IRQ_NONE;

	isr_status = cdns_i2c_readreg(CDNS_I2C_ISR_OFFSET);
	cdns_i2c_writereg(isr_status, CDNS_I2C_ISR_OFFSET);
	id->err_status = 0;

	 
	if (isr_status & (CDNS_I2C_IXR_NACK | CDNS_I2C_IXR_ARB_LOST)) {
		done_flag = 1;
		status = IRQ_HANDLED;
	}

	 
	updatetx = id->recv_count > id->curr_recv_count;

	 
	if (id->p_recv_buf &&
	    ((isr_status & CDNS_I2C_IXR_COMP) ||
	     (isr_status & CDNS_I2C_IXR_DATA))) {
		 
		while (cdns_i2c_readreg(CDNS_I2C_SR_OFFSET) &
		       CDNS_I2C_SR_RXDV) {
			if (id->recv_count > 0) {
				*(id->p_recv_buf)++ =
					cdns_i2c_readreg(CDNS_I2C_DATA_OFFSET);
				id->recv_count--;
				id->curr_recv_count--;

				 
				if (id->recv_count <= id->fifo_depth &&
				    !id->bus_hold_flag)
					cdns_i2c_clear_bus_hold(id);

			} else {
				dev_err(id->adap.dev.parent,
					"xfer_size reg rollover. xfer aborted!\n");
				id->err_status |= CDNS_I2C_IXR_TO;
				break;
			}

			if (cdns_is_holdquirk(id, updatetx))
				break;
		}

		 
		if (cdns_is_holdquirk(id, updatetx)) {
			 
			while (cdns_i2c_readreg(CDNS_I2C_XFER_SIZE_OFFSET) !=
			       (id->curr_recv_count - id->fifo_depth))
				;

			 
			if (((int)(id->recv_count) - id->fifo_depth) >
			    id->transfer_size) {
				cdns_i2c_writereg(id->transfer_size,
						  CDNS_I2C_XFER_SIZE_OFFSET);
				id->curr_recv_count = id->transfer_size +
						      id->fifo_depth;
			} else {
				cdns_i2c_writereg(id->recv_count -
						  id->fifo_depth,
						  CDNS_I2C_XFER_SIZE_OFFSET);
				id->curr_recv_count = id->recv_count;
			}
		}

		 
		if ((isr_status & CDNS_I2C_IXR_COMP) && !id->recv_count) {
			if (!id->bus_hold_flag)
				cdns_i2c_clear_bus_hold(id);
			done_flag = 1;
		}

		status = IRQ_HANDLED;
	}

	 
	if ((isr_status & CDNS_I2C_IXR_COMP) && !id->p_recv_buf) {
		 
		if (id->send_count) {
			avail_bytes = id->fifo_depth -
			    cdns_i2c_readreg(CDNS_I2C_XFER_SIZE_OFFSET);
			if (id->send_count > avail_bytes)
				bytes_to_send = avail_bytes;
			else
				bytes_to_send = id->send_count;

			while (bytes_to_send--) {
				cdns_i2c_writereg(
					(*(id->p_send_buf)++),
					 CDNS_I2C_DATA_OFFSET);
				id->send_count--;
			}
		} else {
			 
			done_flag = 1;
		}
		if (!id->send_count && !id->bus_hold_flag)
			cdns_i2c_clear_bus_hold(id);

		status = IRQ_HANDLED;
	}

	 
	id->err_status |= isr_status & CDNS_I2C_IXR_ERR_INTR_MASK;
	if (id->err_status)
		status = IRQ_HANDLED;

	if (done_flag)
		complete(&id->xfer_done);

	return status;
}

 
static irqreturn_t cdns_i2c_isr(int irq, void *ptr)
{
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	struct cdns_i2c *id = ptr;

	if (id->dev_mode == CDNS_I2C_MODE_SLAVE)
		return cdns_i2c_slave_isr(ptr);
#endif
	return cdns_i2c_master_isr(ptr);
}

 
static void cdns_i2c_mrecv(struct cdns_i2c *id)
{
	unsigned int ctrl_reg;
	unsigned int isr_status;
	unsigned long flags;
	bool hold_clear = false;
	bool irq_save = false;

	u32 addr;

	id->p_recv_buf = id->p_msg->buf;
	id->recv_count = id->p_msg->len;

	 
	ctrl_reg = cdns_i2c_readreg(CDNS_I2C_CR_OFFSET);
	ctrl_reg |= CDNS_I2C_CR_RW | CDNS_I2C_CR_CLR_FIFO;

	 
	if (id->p_msg->flags & I2C_M_RECV_LEN)
		id->recv_count = I2C_SMBUS_BLOCK_MAX + id->p_msg->len;

	id->curr_recv_count = id->recv_count;

	 
	if (id->recv_count > id->fifo_depth)
		ctrl_reg |= CDNS_I2C_CR_HOLD;

	cdns_i2c_writereg(ctrl_reg, CDNS_I2C_CR_OFFSET);

	 
	isr_status = cdns_i2c_readreg(CDNS_I2C_ISR_OFFSET);
	cdns_i2c_writereg(isr_status, CDNS_I2C_ISR_OFFSET);

	 
	if (id->recv_count > id->transfer_size) {
		cdns_i2c_writereg(id->transfer_size,
				  CDNS_I2C_XFER_SIZE_OFFSET);
		id->curr_recv_count = id->transfer_size;
	} else {
		cdns_i2c_writereg(id->recv_count, CDNS_I2C_XFER_SIZE_OFFSET);
	}

	 
	if (!id->bus_hold_flag && id->recv_count <= id->fifo_depth) {
		if (ctrl_reg & CDNS_I2C_CR_HOLD) {
			hold_clear = true;
			if (id->quirks & CDNS_I2C_BROKEN_HOLD_BIT)
				irq_save = true;
		}
	}

	addr = id->p_msg->addr;
	addr &= CDNS_I2C_ADDR_MASK;

	if (hold_clear) {
		ctrl_reg &= ~CDNS_I2C_CR_HOLD;
		 
		if (irq_save)
			local_irq_save(flags);

		cdns_i2c_writereg(addr, CDNS_I2C_ADDR_OFFSET);
		cdns_i2c_writereg(ctrl_reg, CDNS_I2C_CR_OFFSET);
		 
		cdns_i2c_readreg(CDNS_I2C_CR_OFFSET);

		if (irq_save)
			local_irq_restore(flags);
	} else {
		cdns_i2c_writereg(addr, CDNS_I2C_ADDR_OFFSET);
	}

	cdns_i2c_writereg(CDNS_I2C_ENABLED_INTR_MASK, CDNS_I2C_IER_OFFSET);
}

 
static void cdns_i2c_msend(struct cdns_i2c *id)
{
	unsigned int avail_bytes;
	unsigned int bytes_to_send;
	unsigned int ctrl_reg;
	unsigned int isr_status;

	id->p_recv_buf = NULL;
	id->p_send_buf = id->p_msg->buf;
	id->send_count = id->p_msg->len;

	 
	ctrl_reg = cdns_i2c_readreg(CDNS_I2C_CR_OFFSET);
	ctrl_reg &= ~CDNS_I2C_CR_RW;
	ctrl_reg |= CDNS_I2C_CR_CLR_FIFO;

	 
	if (id->send_count > id->fifo_depth)
		ctrl_reg |= CDNS_I2C_CR_HOLD;
	cdns_i2c_writereg(ctrl_reg, CDNS_I2C_CR_OFFSET);

	 
	isr_status = cdns_i2c_readreg(CDNS_I2C_ISR_OFFSET);
	cdns_i2c_writereg(isr_status, CDNS_I2C_ISR_OFFSET);

	 
	avail_bytes = id->fifo_depth -
				cdns_i2c_readreg(CDNS_I2C_XFER_SIZE_OFFSET);

	if (id->send_count > avail_bytes)
		bytes_to_send = avail_bytes;
	else
		bytes_to_send = id->send_count;

	while (bytes_to_send--) {
		cdns_i2c_writereg((*(id->p_send_buf)++), CDNS_I2C_DATA_OFFSET);
		id->send_count--;
	}

	 
	if (!id->bus_hold_flag && !id->send_count)
		cdns_i2c_clear_bus_hold(id);
	 
	cdns_i2c_writereg(id->p_msg->addr & CDNS_I2C_ADDR_MASK,
						CDNS_I2C_ADDR_OFFSET);

	cdns_i2c_writereg(CDNS_I2C_ENABLED_INTR_MASK, CDNS_I2C_IER_OFFSET);
}

 
static void cdns_i2c_master_reset(struct i2c_adapter *adap)
{
	struct cdns_i2c *id = adap->algo_data;
	u32 regval;

	 
	cdns_i2c_writereg(CDNS_I2C_IXR_ALL_INTR_MASK, CDNS_I2C_IDR_OFFSET);
	 
	regval = cdns_i2c_readreg(CDNS_I2C_CR_OFFSET);
	regval &= ~CDNS_I2C_CR_HOLD;
	regval |= CDNS_I2C_CR_CLR_FIFO;
	cdns_i2c_writereg(regval, CDNS_I2C_CR_OFFSET);
	 
	cdns_i2c_writereg(0, CDNS_I2C_XFER_SIZE_OFFSET);
	 
	regval = cdns_i2c_readreg(CDNS_I2C_ISR_OFFSET);
	cdns_i2c_writereg(regval, CDNS_I2C_ISR_OFFSET);
	 
	regval = cdns_i2c_readreg(CDNS_I2C_SR_OFFSET);
	cdns_i2c_writereg(regval, CDNS_I2C_SR_OFFSET);
}

static int cdns_i2c_process_msg(struct cdns_i2c *id, struct i2c_msg *msg,
		struct i2c_adapter *adap)
{
	unsigned long time_left, msg_timeout;
	u32 reg;

	id->p_msg = msg;
	id->err_status = 0;
	reinit_completion(&id->xfer_done);

	 
	reg = cdns_i2c_readreg(CDNS_I2C_CR_OFFSET);
	if (msg->flags & I2C_M_TEN) {
		if (reg & CDNS_I2C_CR_NEA)
			cdns_i2c_writereg(reg & ~CDNS_I2C_CR_NEA,
					CDNS_I2C_CR_OFFSET);
	} else {
		if (!(reg & CDNS_I2C_CR_NEA))
			cdns_i2c_writereg(reg | CDNS_I2C_CR_NEA,
					CDNS_I2C_CR_OFFSET);
	}

	 
	if (msg->flags & I2C_M_RD)
		cdns_i2c_mrecv(id);
	else
		cdns_i2c_msend(id);

	 
	msg_timeout = msecs_to_jiffies((1000 * msg->len * BITS_PER_BYTE) / id->i2c_clk);
	 
	msg_timeout += msecs_to_jiffies(500);

	if (msg_timeout < adap->timeout)
		msg_timeout = adap->timeout;

	 
	time_left = wait_for_completion_timeout(&id->xfer_done, msg_timeout);
	if (time_left == 0) {
		cdns_i2c_master_reset(adap);
		dev_err(id->adap.dev.parent,
				"timeout waiting on completion\n");
		return -ETIMEDOUT;
	}

	cdns_i2c_writereg(CDNS_I2C_IXR_ALL_INTR_MASK,
			  CDNS_I2C_IDR_OFFSET);

	 
	if (id->err_status & CDNS_I2C_IXR_ARB_LOST)
		return -EAGAIN;

	if (msg->flags & I2C_M_RECV_LEN)
		msg->len += min_t(unsigned int, msg->buf[0], I2C_SMBUS_BLOCK_MAX);

	return 0;
}

 
static int cdns_i2c_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
				int num)
{
	int ret, count;
	u32 reg;
	struct cdns_i2c *id = adap->algo_data;
	bool hold_quirk;
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	bool change_role = false;
#endif

	ret = pm_runtime_resume_and_get(id->dev);
	if (ret < 0)
		return ret;

#if IS_ENABLED(CONFIG_I2C_SLAVE)
	 
	if (id->dev_mode == CDNS_I2C_MODE_SLAVE) {
		if (id->slave_state != CDNS_I2C_SLAVE_STATE_IDLE) {
			ret = -EAGAIN;
			goto out;
		}

		 
		cdns_i2c_set_mode(CDNS_I2C_MODE_MASTER, id);

		 
		change_role = true;
	}
#endif

	 

	ret = readl_relaxed_poll_timeout(id->membase + CDNS_I2C_SR_OFFSET,
					 reg,
					 !(reg & CDNS_I2C_SR_BA),
					 CDNS_I2C_POLL_US, CDNS_I2C_TIMEOUT_US);
	if (ret) {
		ret = -EAGAIN;
		if (id->adap.bus_recovery_info)
			i2c_recover_bus(adap);
		goto out;
	}

	hold_quirk = !!(id->quirks & CDNS_I2C_BROKEN_HOLD_BIT);
	 
	if (num > 1) {
		 
		for (count = 0; (count < num - 1 && hold_quirk); count++) {
			if (msgs[count].flags & I2C_M_RD) {
				dev_warn(adap->dev.parent,
					 "Can't do repeated start after a receive message\n");
				ret = -EOPNOTSUPP;
				goto out;
			}
		}
		id->bus_hold_flag = 1;
		reg = cdns_i2c_readreg(CDNS_I2C_CR_OFFSET);
		reg |= CDNS_I2C_CR_HOLD;
		cdns_i2c_writereg(reg, CDNS_I2C_CR_OFFSET);
	} else {
		id->bus_hold_flag = 0;
	}

	 
	for (count = 0; count < num; count++, msgs++) {
		if (count == (num - 1))
			id->bus_hold_flag = 0;

		ret = cdns_i2c_process_msg(id, msgs, adap);
		if (ret)
			goto out;

		 
		if (id->err_status) {
			cdns_i2c_master_reset(adap);

			if (id->err_status & CDNS_I2C_IXR_NACK) {
				ret = -ENXIO;
				goto out;
			}
			ret = -EIO;
			goto out;
		}
	}

	ret = num;

out:

#if IS_ENABLED(CONFIG_I2C_SLAVE)
	 
	if (change_role)
		cdns_i2c_set_mode(CDNS_I2C_MODE_SLAVE, id);
#endif

	pm_runtime_mark_last_busy(id->dev);
	pm_runtime_put_autosuspend(id->dev);
	return ret;
}

 
static u32 cdns_i2c_func(struct i2c_adapter *adap)
{
	u32 func = I2C_FUNC_I2C | I2C_FUNC_10BIT_ADDR |
			(I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK) |
			I2C_FUNC_SMBUS_BLOCK_DATA;

#if IS_ENABLED(CONFIG_I2C_SLAVE)
	func |= I2C_FUNC_SLAVE;
#endif

	return func;
}

#if IS_ENABLED(CONFIG_I2C_SLAVE)
static int cdns_reg_slave(struct i2c_client *slave)
{
	int ret;
	struct cdns_i2c *id = container_of(slave->adapter, struct cdns_i2c,
									adap);

	if (id->slave)
		return -EBUSY;

	if (slave->flags & I2C_CLIENT_TEN)
		return -EAFNOSUPPORT;

	ret = pm_runtime_resume_and_get(id->dev);
	if (ret < 0)
		return ret;

	 
	id->slave = slave;

	 
	cdns_i2c_set_mode(CDNS_I2C_MODE_SLAVE, id);

	return 0;
}

static int cdns_unreg_slave(struct i2c_client *slave)
{
	struct cdns_i2c *id = container_of(slave->adapter, struct cdns_i2c,
									adap);

	pm_runtime_put(id->dev);

	 
	id->slave = NULL;

	 
	cdns_i2c_set_mode(CDNS_I2C_MODE_MASTER, id);

	return 0;
}
#endif

static const struct i2c_algorithm cdns_i2c_algo = {
	.master_xfer	= cdns_i2c_master_xfer,
	.functionality	= cdns_i2c_func,
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	.reg_slave	= cdns_reg_slave,
	.unreg_slave	= cdns_unreg_slave,
#endif
};

 
static int cdns_i2c_calc_divs(unsigned long *f, unsigned long input_clk,
		unsigned int *a, unsigned int *b)
{
	unsigned long fscl = *f, best_fscl = *f, actual_fscl, temp;
	unsigned int div_a, div_b, calc_div_a = 0, calc_div_b = 0;
	unsigned int last_error, current_error;

	 
	temp = input_clk / (22 * fscl);

	 
	if (!temp || (temp > (CDNS_I2C_DIVA_MAX * CDNS_I2C_DIVB_MAX)))
		return -EINVAL;

	last_error = -1;
	for (div_a = 0; div_a < CDNS_I2C_DIVA_MAX; div_a++) {
		div_b = DIV_ROUND_UP(input_clk, 22 * fscl * (div_a + 1));

		if ((div_b < 1) || (div_b > CDNS_I2C_DIVB_MAX))
			continue;
		div_b--;

		actual_fscl = input_clk / (22 * (div_a + 1) * (div_b + 1));

		if (actual_fscl > fscl)
			continue;

		current_error = fscl - actual_fscl;

		if (last_error > current_error) {
			calc_div_a = div_a;
			calc_div_b = div_b;
			best_fscl = actual_fscl;
			last_error = current_error;
		}
	}

	*a = calc_div_a;
	*b = calc_div_b;
	*f = best_fscl;

	return 0;
}

 
static int cdns_i2c_setclk(unsigned long clk_in, struct cdns_i2c *id)
{
	unsigned int div_a, div_b;
	unsigned int ctrl_reg;
	int ret = 0;
	unsigned long fscl = id->i2c_clk;

	ret = cdns_i2c_calc_divs(&fscl, clk_in, &div_a, &div_b);
	if (ret)
		return ret;

	ctrl_reg = id->ctrl_reg;
	ctrl_reg &= ~(CDNS_I2C_CR_DIVA_MASK | CDNS_I2C_CR_DIVB_MASK);
	ctrl_reg |= ((div_a << CDNS_I2C_CR_DIVA_SHIFT) |
			(div_b << CDNS_I2C_CR_DIVB_SHIFT));
	id->ctrl_reg = ctrl_reg;
	cdns_i2c_writereg(ctrl_reg, CDNS_I2C_CR_OFFSET);
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	id->ctrl_reg_diva_divb = ctrl_reg & (CDNS_I2C_CR_DIVA_MASK |
				 CDNS_I2C_CR_DIVB_MASK);
#endif
	return 0;
}

 
static int cdns_i2c_clk_notifier_cb(struct notifier_block *nb, unsigned long
		event, void *data)
{
	struct clk_notifier_data *ndata = data;
	struct cdns_i2c *id = to_cdns_i2c(nb);

	if (pm_runtime_suspended(id->dev))
		return NOTIFY_OK;

	switch (event) {
	case PRE_RATE_CHANGE:
	{
		unsigned long input_clk = ndata->new_rate;
		unsigned long fscl = id->i2c_clk;
		unsigned int div_a, div_b;
		int ret;

		ret = cdns_i2c_calc_divs(&fscl, input_clk, &div_a, &div_b);
		if (ret) {
			dev_warn(id->adap.dev.parent,
					"clock rate change rejected\n");
			return NOTIFY_STOP;
		}

		 
		if (ndata->new_rate > ndata->old_rate)
			cdns_i2c_setclk(ndata->new_rate, id);

		return NOTIFY_OK;
	}
	case POST_RATE_CHANGE:
		id->input_clk = ndata->new_rate;
		 
		if (ndata->new_rate < ndata->old_rate)
			cdns_i2c_setclk(ndata->new_rate, id);
		return NOTIFY_OK;
	case ABORT_RATE_CHANGE:
		 
		if (ndata->new_rate > ndata->old_rate)
			cdns_i2c_setclk(ndata->old_rate, id);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

 
static int __maybe_unused cdns_i2c_runtime_suspend(struct device *dev)
{
	struct cdns_i2c *xi2c = dev_get_drvdata(dev);

	clk_disable(xi2c->clk);

	return 0;
}

 
static void cdns_i2c_init(struct cdns_i2c *id)
{
	cdns_i2c_writereg(id->ctrl_reg, CDNS_I2C_CR_OFFSET);
	 
	cdns_i2c_writereg(CDNS_I2C_TIMEOUT_MAX, CDNS_I2C_TIME_OUT_OFFSET);
}

 
static int __maybe_unused cdns_i2c_runtime_resume(struct device *dev)
{
	struct cdns_i2c *xi2c = dev_get_drvdata(dev);
	int ret;

	ret = clk_enable(xi2c->clk);
	if (ret) {
		dev_err(dev, "Cannot enable clock.\n");
		return ret;
	}
	cdns_i2c_init(xi2c);

	return 0;
}

static const struct dev_pm_ops cdns_i2c_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(cdns_i2c_runtime_suspend,
			   cdns_i2c_runtime_resume, NULL)
};

static const struct cdns_platform_data r1p10_i2c_def = {
	.quirks = CDNS_I2C_BROKEN_HOLD_BIT,
};

static const struct of_device_id cdns_i2c_of_match[] = {
	{ .compatible = "cdns,i2c-r1p10", .data = &r1p10_i2c_def },
	{ .compatible = "cdns,i2c-r1p14",},
	{   }
};
MODULE_DEVICE_TABLE(of, cdns_i2c_of_match);

 
static void cdns_i2c_detect_transfer_size(struct cdns_i2c *id)
{
	u32 val;

	 
	cdns_i2c_writereg(CDNS_I2C_CR_MS | CDNS_I2C_CR_RW, CDNS_I2C_CR_OFFSET);

	 
	cdns_i2c_writereg(CDNS_I2C_MAX_TRANSFER_SIZE, CDNS_I2C_XFER_SIZE_OFFSET);
	val = cdns_i2c_readreg(CDNS_I2C_XFER_SIZE_OFFSET);
	id->transfer_size = CDNS_I2C_TRANSFER_SIZE(val);
	cdns_i2c_writereg(0, CDNS_I2C_XFER_SIZE_OFFSET);
	cdns_i2c_writereg(0, CDNS_I2C_CR_OFFSET);
}

 
static int cdns_i2c_probe(struct platform_device *pdev)
{
	struct resource *r_mem;
	struct cdns_i2c *id;
	int ret, irq;
	const struct of_device_id *match;

	id = devm_kzalloc(&pdev->dev, sizeof(*id), GFP_KERNEL);
	if (!id)
		return -ENOMEM;

	id->dev = &pdev->dev;
	platform_set_drvdata(pdev, id);

	match = of_match_node(cdns_i2c_of_match, pdev->dev.of_node);
	if (match && match->data) {
		const struct cdns_platform_data *data = match->data;
		id->quirks = data->quirks;
	}

	id->rinfo.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(id->rinfo.pinctrl)) {
		int err = PTR_ERR(id->rinfo.pinctrl);

		dev_info(&pdev->dev, "can't get pinctrl, bus recovery not supported\n");
		if (err != -ENODEV)
			return err;
	} else {
		id->adap.bus_recovery_info = &id->rinfo;
	}

	id->membase = devm_platform_get_and_ioremap_resource(pdev, 0, &r_mem);
	if (IS_ERR(id->membase))
		return PTR_ERR(id->membase);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	id->adap.owner = THIS_MODULE;
	id->adap.dev.of_node = pdev->dev.of_node;
	id->adap.algo = &cdns_i2c_algo;
	id->adap.timeout = CDNS_I2C_TIMEOUT;
	id->adap.retries = 3;		 
	id->adap.algo_data = id;
	id->adap.dev.parent = &pdev->dev;
	init_completion(&id->xfer_done);
	snprintf(id->adap.name, sizeof(id->adap.name),
		 "Cadence I2C at %08lx", (unsigned long)r_mem->start);

	id->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(id->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(id->clk),
				     "input clock not found.\n");

	id->reset = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(id->reset))
		return dev_err_probe(&pdev->dev, PTR_ERR(id->reset),
				     "Failed to request reset.\n");

	ret = clk_prepare_enable(id->clk);
	if (ret)
		dev_err(&pdev->dev, "Unable to enable clock.\n");

	ret = reset_control_deassert(id->reset);
	if (ret) {
		dev_err_probe(&pdev->dev, ret,
			      "Failed to de-assert reset.\n");
		goto err_clk_dis;
	}

	pm_runtime_set_autosuspend_delay(id->dev, CNDS_I2C_PM_TIMEOUT);
	pm_runtime_use_autosuspend(id->dev);
	pm_runtime_set_active(id->dev);
	pm_runtime_enable(id->dev);

	id->clk_rate_change_nb.notifier_call = cdns_i2c_clk_notifier_cb;
	if (clk_notifier_register(id->clk, &id->clk_rate_change_nb))
		dev_warn(&pdev->dev, "Unable to register clock notifier.\n");
	id->input_clk = clk_get_rate(id->clk);

	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency",
			&id->i2c_clk);
	if (ret || (id->i2c_clk > I2C_MAX_FAST_MODE_FREQ))
		id->i2c_clk = I2C_MAX_STANDARD_MODE_FREQ;

#if IS_ENABLED(CONFIG_I2C_SLAVE)
	 
	id->dev_mode = CDNS_I2C_MODE_MASTER;
	id->slave_state = CDNS_I2C_SLAVE_STATE_IDLE;
#endif
	id->ctrl_reg = CDNS_I2C_CR_ACK_EN | CDNS_I2C_CR_NEA | CDNS_I2C_CR_MS;

	id->fifo_depth = CDNS_I2C_FIFO_DEPTH_DEFAULT;
	of_property_read_u32(pdev->dev.of_node, "fifo-depth", &id->fifo_depth);

	cdns_i2c_detect_transfer_size(id);

	ret = cdns_i2c_setclk(id->input_clk, id);
	if (ret) {
		dev_err(&pdev->dev, "invalid SCL clock: %u Hz\n", id->i2c_clk);
		ret = -EINVAL;
		goto err_clk_notifier_unregister;
	}

	ret = devm_request_irq(&pdev->dev, irq, cdns_i2c_isr, 0,
				 DRIVER_NAME, id);
	if (ret) {
		dev_err(&pdev->dev, "cannot get irq %d\n", irq);
		goto err_clk_notifier_unregister;
	}
	cdns_i2c_init(id);

	ret = i2c_add_adapter(&id->adap);
	if (ret < 0)
		goto err_clk_notifier_unregister;

	dev_info(&pdev->dev, "%u kHz mmio %08lx irq %d\n",
		 id->i2c_clk / 1000, (unsigned long)r_mem->start, irq);

	return 0;

err_clk_notifier_unregister:
	clk_notifier_unregister(id->clk, &id->clk_rate_change_nb);
	reset_control_assert(id->reset);
err_clk_dis:
	clk_disable_unprepare(id->clk);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	return ret;
}

 
static void cdns_i2c_remove(struct platform_device *pdev)
{
	struct cdns_i2c *id = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_dont_use_autosuspend(&pdev->dev);

	i2c_del_adapter(&id->adap);
	clk_notifier_unregister(id->clk, &id->clk_rate_change_nb);
	reset_control_assert(id->reset);
	clk_disable_unprepare(id->clk);
}

static struct platform_driver cdns_i2c_drv = {
	.driver = {
		.name  = DRIVER_NAME,
		.of_match_table = cdns_i2c_of_match,
		.pm = &cdns_i2c_dev_pm_ops,
	},
	.probe  = cdns_i2c_probe,
	.remove_new = cdns_i2c_remove,
};

module_platform_driver(cdns_i2c_drv);

MODULE_AUTHOR("Xilinx Inc.");
MODULE_DESCRIPTION("Cadence I2C bus driver");
MODULE_LICENSE("GPL");
