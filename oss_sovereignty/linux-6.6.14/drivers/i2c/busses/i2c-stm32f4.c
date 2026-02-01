
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#include "i2c-stm32.h"

 
#define STM32F4_I2C_CR1			0x00
#define STM32F4_I2C_CR2			0x04
#define STM32F4_I2C_DR			0x10
#define STM32F4_I2C_SR1			0x14
#define STM32F4_I2C_SR2			0x18
#define STM32F4_I2C_CCR			0x1C
#define STM32F4_I2C_TRISE		0x20
#define STM32F4_I2C_FLTR		0x24

 
#define STM32F4_I2C_CR1_POS		BIT(11)
#define STM32F4_I2C_CR1_ACK		BIT(10)
#define STM32F4_I2C_CR1_STOP		BIT(9)
#define STM32F4_I2C_CR1_START		BIT(8)
#define STM32F4_I2C_CR1_PE		BIT(0)

 
#define STM32F4_I2C_CR2_FREQ_MASK	GENMASK(5, 0)
#define STM32F4_I2C_CR2_FREQ(n)		((n) & STM32F4_I2C_CR2_FREQ_MASK)
#define STM32F4_I2C_CR2_ITBUFEN		BIT(10)
#define STM32F4_I2C_CR2_ITEVTEN		BIT(9)
#define STM32F4_I2C_CR2_ITERREN		BIT(8)
#define STM32F4_I2C_CR2_IRQ_MASK	(STM32F4_I2C_CR2_ITBUFEN | \
					 STM32F4_I2C_CR2_ITEVTEN | \
					 STM32F4_I2C_CR2_ITERREN)

 
#define STM32F4_I2C_SR1_AF		BIT(10)
#define STM32F4_I2C_SR1_ARLO		BIT(9)
#define STM32F4_I2C_SR1_BERR		BIT(8)
#define STM32F4_I2C_SR1_TXE		BIT(7)
#define STM32F4_I2C_SR1_RXNE		BIT(6)
#define STM32F4_I2C_SR1_BTF		BIT(2)
#define STM32F4_I2C_SR1_ADDR		BIT(1)
#define STM32F4_I2C_SR1_SB		BIT(0)
#define STM32F4_I2C_SR1_ITEVTEN_MASK	(STM32F4_I2C_SR1_BTF | \
					 STM32F4_I2C_SR1_ADDR | \
					 STM32F4_I2C_SR1_SB)
#define STM32F4_I2C_SR1_ITBUFEN_MASK	(STM32F4_I2C_SR1_TXE | \
					 STM32F4_I2C_SR1_RXNE)
#define STM32F4_I2C_SR1_ITERREN_MASK	(STM32F4_I2C_SR1_AF | \
					 STM32F4_I2C_SR1_ARLO | \
					 STM32F4_I2C_SR1_BERR)

 
#define STM32F4_I2C_SR2_BUSY		BIT(1)

 
#define STM32F4_I2C_CCR_CCR_MASK	GENMASK(11, 0)
#define STM32F4_I2C_CCR_CCR(n)		((n) & STM32F4_I2C_CCR_CCR_MASK)
#define STM32F4_I2C_CCR_FS		BIT(15)
#define STM32F4_I2C_CCR_DUTY		BIT(14)

 
#define STM32F4_I2C_TRISE_VALUE_MASK	GENMASK(5, 0)
#define STM32F4_I2C_TRISE_VALUE(n)	((n) & STM32F4_I2C_TRISE_VALUE_MASK)

#define STM32F4_I2C_MIN_STANDARD_FREQ	2U
#define STM32F4_I2C_MIN_FAST_FREQ	6U
#define STM32F4_I2C_MAX_FREQ		46U
#define HZ_TO_MHZ			1000000

 
struct stm32f4_i2c_msg {
	u8 addr;
	u32 count;
	u8 *buf;
	int result;
	bool stop;
};

 
struct stm32f4_i2c_dev {
	struct i2c_adapter adap;
	struct device *dev;
	void __iomem *base;
	struct completion complete;
	struct clk *clk;
	int speed;
	int parent_rate;
	struct stm32f4_i2c_msg msg;
};

static inline void stm32f4_i2c_set_bits(void __iomem *reg, u32 mask)
{
	writel_relaxed(readl_relaxed(reg) | mask, reg);
}

static inline void stm32f4_i2c_clr_bits(void __iomem *reg, u32 mask)
{
	writel_relaxed(readl_relaxed(reg) & ~mask, reg);
}

static void stm32f4_i2c_disable_irq(struct stm32f4_i2c_dev *i2c_dev)
{
	void __iomem *reg = i2c_dev->base + STM32F4_I2C_CR2;

	stm32f4_i2c_clr_bits(reg, STM32F4_I2C_CR2_IRQ_MASK);
}

static int stm32f4_i2c_set_periph_clk_freq(struct stm32f4_i2c_dev *i2c_dev)
{
	u32 freq;
	u32 cr2 = 0;

	i2c_dev->parent_rate = clk_get_rate(i2c_dev->clk);
	freq = DIV_ROUND_UP(i2c_dev->parent_rate, HZ_TO_MHZ);

	if (i2c_dev->speed == STM32_I2C_SPEED_STANDARD) {
		 
		if (freq < STM32F4_I2C_MIN_STANDARD_FREQ ||
		    freq > STM32F4_I2C_MAX_FREQ) {
			dev_err(i2c_dev->dev,
				"bad parent clk freq for standard mode\n");
			return -EINVAL;
		}
	} else {
		 
		if (freq < STM32F4_I2C_MIN_FAST_FREQ ||
		    freq > STM32F4_I2C_MAX_FREQ) {
			dev_err(i2c_dev->dev,
				"bad parent clk freq for fast mode\n");
			return -EINVAL;
		}
	}

	cr2 |= STM32F4_I2C_CR2_FREQ(freq);
	writel_relaxed(cr2, i2c_dev->base + STM32F4_I2C_CR2);

	return 0;
}

static void stm32f4_i2c_set_rise_time(struct stm32f4_i2c_dev *i2c_dev)
{
	u32 freq = DIV_ROUND_UP(i2c_dev->parent_rate, HZ_TO_MHZ);
	u32 trise;

	 
	if (i2c_dev->speed == STM32_I2C_SPEED_STANDARD)
		trise = freq + 1;
	else
		trise = freq * 3 / 10 + 1;

	writel_relaxed(STM32F4_I2C_TRISE_VALUE(trise),
		       i2c_dev->base + STM32F4_I2C_TRISE);
}

static void stm32f4_i2c_set_speed_mode(struct stm32f4_i2c_dev *i2c_dev)
{
	u32 val;
	u32 ccr = 0;

	if (i2c_dev->speed == STM32_I2C_SPEED_STANDARD) {
		 
		val = i2c_dev->parent_rate / (I2C_MAX_STANDARD_MODE_FREQ * 2);
	} else {
		 
		val = DIV_ROUND_UP(i2c_dev->parent_rate, I2C_MAX_FAST_MODE_FREQ * 3);

		 
		ccr |= STM32F4_I2C_CCR_FS;
	}

	ccr |= STM32F4_I2C_CCR_CCR(val);
	writel_relaxed(ccr, i2c_dev->base + STM32F4_I2C_CCR);
}

 
static int stm32f4_i2c_hw_config(struct stm32f4_i2c_dev *i2c_dev)
{
	int ret;

	ret = stm32f4_i2c_set_periph_clk_freq(i2c_dev);
	if (ret)
		return ret;

	stm32f4_i2c_set_rise_time(i2c_dev);

	stm32f4_i2c_set_speed_mode(i2c_dev);

	 
	writel_relaxed(STM32F4_I2C_CR1_PE, i2c_dev->base + STM32F4_I2C_CR1);

	return 0;
}

static int stm32f4_i2c_wait_free_bus(struct stm32f4_i2c_dev *i2c_dev)
{
	u32 status;
	int ret;

	ret = readl_relaxed_poll_timeout(i2c_dev->base + STM32F4_I2C_SR2,
					 status,
					 !(status & STM32F4_I2C_SR2_BUSY),
					 10, 1000);
	if (ret) {
		dev_dbg(i2c_dev->dev, "bus not free\n");
		ret = -EBUSY;
	}

	return ret;
}

 
static void stm32f4_i2c_write_byte(struct stm32f4_i2c_dev *i2c_dev, u8 byte)
{
	writel_relaxed(byte, i2c_dev->base + STM32F4_I2C_DR);
}

 
static void stm32f4_i2c_write_msg(struct stm32f4_i2c_dev *i2c_dev)
{
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;

	stm32f4_i2c_write_byte(i2c_dev, *msg->buf++);
	msg->count--;
}

static void stm32f4_i2c_read_msg(struct stm32f4_i2c_dev *i2c_dev)
{
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;
	u32 rbuf;

	rbuf = readl_relaxed(i2c_dev->base + STM32F4_I2C_DR);
	*msg->buf++ = rbuf;
	msg->count--;
}

static void stm32f4_i2c_terminate_xfer(struct stm32f4_i2c_dev *i2c_dev)
{
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;
	void __iomem *reg;

	stm32f4_i2c_disable_irq(i2c_dev);

	reg = i2c_dev->base + STM32F4_I2C_CR1;
	if (msg->stop)
		stm32f4_i2c_set_bits(reg, STM32F4_I2C_CR1_STOP);
	else
		stm32f4_i2c_set_bits(reg, STM32F4_I2C_CR1_START);

	complete(&i2c_dev->complete);
}

 
static void stm32f4_i2c_handle_write(struct stm32f4_i2c_dev *i2c_dev)
{
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;
	void __iomem *reg = i2c_dev->base + STM32F4_I2C_CR2;

	if (msg->count) {
		stm32f4_i2c_write_msg(i2c_dev);
		if (!msg->count) {
			 
			stm32f4_i2c_clr_bits(reg, STM32F4_I2C_CR2_ITBUFEN);
		}
	} else {
		stm32f4_i2c_terminate_xfer(i2c_dev);
	}
}

 
static void stm32f4_i2c_handle_read(struct stm32f4_i2c_dev *i2c_dev)
{
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;
	void __iomem *reg = i2c_dev->base + STM32F4_I2C_CR2;

	switch (msg->count) {
	case 1:
		stm32f4_i2c_disable_irq(i2c_dev);
		stm32f4_i2c_read_msg(i2c_dev);
		complete(&i2c_dev->complete);
		break;
	 
	case 2:
	case 3:
		stm32f4_i2c_clr_bits(reg, STM32F4_I2C_CR2_ITBUFEN);
		break;
	 
	default:
		stm32f4_i2c_read_msg(i2c_dev);
	}
}

 
static void stm32f4_i2c_handle_rx_done(struct stm32f4_i2c_dev *i2c_dev)
{
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;
	void __iomem *reg;
	u32 mask;
	int i;

	switch (msg->count) {
	case 2:
		 
		reg = i2c_dev->base + STM32F4_I2C_CR1;
		if (msg->stop)
			stm32f4_i2c_set_bits(reg, STM32F4_I2C_CR1_STOP);
		else
			stm32f4_i2c_set_bits(reg, STM32F4_I2C_CR1_START);

		for (i = 2; i > 0; i--)
			stm32f4_i2c_read_msg(i2c_dev);

		reg = i2c_dev->base + STM32F4_I2C_CR2;
		mask = STM32F4_I2C_CR2_ITEVTEN | STM32F4_I2C_CR2_ITERREN;
		stm32f4_i2c_clr_bits(reg, mask);

		complete(&i2c_dev->complete);
		break;
	case 3:
		 
		reg = i2c_dev->base + STM32F4_I2C_CR1;
		stm32f4_i2c_clr_bits(reg, STM32F4_I2C_CR1_ACK);
		stm32f4_i2c_read_msg(i2c_dev);
		break;
	default:
		stm32f4_i2c_read_msg(i2c_dev);
	}
}

 
static void stm32f4_i2c_handle_rx_addr(struct stm32f4_i2c_dev *i2c_dev)
{
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;
	u32 cr1;

	switch (msg->count) {
	case 0:
		stm32f4_i2c_terminate_xfer(i2c_dev);

		 
		readl_relaxed(i2c_dev->base + STM32F4_I2C_SR2);
		break;
	case 1:
		 
		cr1 = readl_relaxed(i2c_dev->base + STM32F4_I2C_CR1);
		cr1 &= ~(STM32F4_I2C_CR1_ACK | STM32F4_I2C_CR1_POS);
		writel_relaxed(cr1, i2c_dev->base + STM32F4_I2C_CR1);

		readl_relaxed(i2c_dev->base + STM32F4_I2C_SR2);

		if (msg->stop)
			cr1 |= STM32F4_I2C_CR1_STOP;
		else
			cr1 |= STM32F4_I2C_CR1_START;
		writel_relaxed(cr1, i2c_dev->base + STM32F4_I2C_CR1);
		break;
	case 2:
		 
		cr1 = readl_relaxed(i2c_dev->base + STM32F4_I2C_CR1);
		cr1 &= ~STM32F4_I2C_CR1_ACK;
		cr1 |= STM32F4_I2C_CR1_POS;
		writel_relaxed(cr1, i2c_dev->base + STM32F4_I2C_CR1);

		readl_relaxed(i2c_dev->base + STM32F4_I2C_SR2);
		break;

	default:
		 
		cr1 = readl_relaxed(i2c_dev->base + STM32F4_I2C_CR1);
		cr1 |= STM32F4_I2C_CR1_ACK;
		cr1 &= ~STM32F4_I2C_CR1_POS;
		writel_relaxed(cr1, i2c_dev->base + STM32F4_I2C_CR1);

		readl_relaxed(i2c_dev->base + STM32F4_I2C_SR2);
		break;
	}
}

 
static irqreturn_t stm32f4_i2c_isr_event(int irq, void *data)
{
	struct stm32f4_i2c_dev *i2c_dev = data;
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;
	u32 possible_status = STM32F4_I2C_SR1_ITEVTEN_MASK;
	u32 status, ien, event, cr2;

	cr2 = readl_relaxed(i2c_dev->base + STM32F4_I2C_CR2);
	ien = cr2 & STM32F4_I2C_CR2_IRQ_MASK;

	 
	if (ien & STM32F4_I2C_CR2_ITBUFEN)
		possible_status |= STM32F4_I2C_SR1_ITBUFEN_MASK;

	status = readl_relaxed(i2c_dev->base + STM32F4_I2C_SR1);
	event = status & possible_status;
	if (!event) {
		dev_dbg(i2c_dev->dev,
			"spurious evt irq (status=0x%08x, ien=0x%08x)\n",
			status, ien);
		return IRQ_NONE;
	}

	 
	if (event & STM32F4_I2C_SR1_SB)
		stm32f4_i2c_write_byte(i2c_dev, msg->addr);

	 
	if (event & STM32F4_I2C_SR1_ADDR) {
		if (msg->addr & I2C_M_RD)
			stm32f4_i2c_handle_rx_addr(i2c_dev);
		else
			readl_relaxed(i2c_dev->base + STM32F4_I2C_SR2);

		 
		cr2 |= STM32F4_I2C_CR2_ITBUFEN;
		writel_relaxed(cr2, i2c_dev->base + STM32F4_I2C_CR2);
	}

	 
	if ((event & STM32F4_I2C_SR1_TXE) && !(msg->addr & I2C_M_RD))
		stm32f4_i2c_handle_write(i2c_dev);

	 
	if ((event & STM32F4_I2C_SR1_RXNE) && (msg->addr & I2C_M_RD))
		stm32f4_i2c_handle_read(i2c_dev);

	 
	if (event & STM32F4_I2C_SR1_BTF) {
		if (msg->addr & I2C_M_RD)
			stm32f4_i2c_handle_rx_done(i2c_dev);
		else
			stm32f4_i2c_handle_write(i2c_dev);
	}

	return IRQ_HANDLED;
}

 
static irqreturn_t stm32f4_i2c_isr_error(int irq, void *data)
{
	struct stm32f4_i2c_dev *i2c_dev = data;
	struct stm32f4_i2c_msg *msg = &i2c_dev->msg;
	void __iomem *reg;
	u32 status;

	status = readl_relaxed(i2c_dev->base + STM32F4_I2C_SR1);

	 
	if (status & STM32F4_I2C_SR1_ARLO) {
		status &= ~STM32F4_I2C_SR1_ARLO;
		writel_relaxed(status, i2c_dev->base + STM32F4_I2C_SR1);
		msg->result = -EAGAIN;
	}

	 
	if (status & STM32F4_I2C_SR1_AF) {
		if (!(msg->addr & I2C_M_RD)) {
			reg = i2c_dev->base + STM32F4_I2C_CR1;
			stm32f4_i2c_set_bits(reg, STM32F4_I2C_CR1_STOP);
		}
		status &= ~STM32F4_I2C_SR1_AF;
		writel_relaxed(status, i2c_dev->base + STM32F4_I2C_SR1);
		msg->result = -EIO;
	}

	 
	if (status & STM32F4_I2C_SR1_BERR) {
		status &= ~STM32F4_I2C_SR1_BERR;
		writel_relaxed(status, i2c_dev->base + STM32F4_I2C_SR1);
		msg->result = -EIO;
	}

	stm32f4_i2c_disable_irq(i2c_dev);
	complete(&i2c_dev->complete);

	return IRQ_HANDLED;
}

 
static int stm32f4_i2c_xfer_msg(struct stm32f4_i2c_dev *i2c_dev,
				struct i2c_msg *msg, bool is_first,
				bool is_last)
{
	struct stm32f4_i2c_msg *f4_msg = &i2c_dev->msg;
	void __iomem *reg = i2c_dev->base + STM32F4_I2C_CR1;
	unsigned long timeout;
	u32 mask;
	int ret;

	f4_msg->addr = i2c_8bit_addr_from_msg(msg);
	f4_msg->buf = msg->buf;
	f4_msg->count = msg->len;
	f4_msg->result = 0;
	f4_msg->stop = is_last;

	reinit_completion(&i2c_dev->complete);

	 
	mask = STM32F4_I2C_CR2_ITEVTEN | STM32F4_I2C_CR2_ITERREN;
	stm32f4_i2c_set_bits(i2c_dev->base + STM32F4_I2C_CR2, mask);

	if (is_first) {
		ret = stm32f4_i2c_wait_free_bus(i2c_dev);
		if (ret)
			return ret;

		 
		stm32f4_i2c_set_bits(reg, STM32F4_I2C_CR1_START);
	}

	timeout = wait_for_completion_timeout(&i2c_dev->complete,
					      i2c_dev->adap.timeout);
	ret = f4_msg->result;

	if (!timeout)
		ret = -ETIMEDOUT;

	return ret;
}

 
static int stm32f4_i2c_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[],
			    int num)
{
	struct stm32f4_i2c_dev *i2c_dev = i2c_get_adapdata(i2c_adap);
	int ret, i;

	ret = clk_enable(i2c_dev->clk);
	if (ret) {
		dev_err(i2c_dev->dev, "Failed to enable clock\n");
		return ret;
	}

	for (i = 0; i < num && !ret; i++)
		ret = stm32f4_i2c_xfer_msg(i2c_dev, &msgs[i], i == 0,
					   i == num - 1);

	clk_disable(i2c_dev->clk);

	return (ret < 0) ? ret : num;
}

static u32 stm32f4_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm stm32f4_i2c_algo = {
	.master_xfer = stm32f4_i2c_xfer,
	.functionality = stm32f4_i2c_func,
};

static int stm32f4_i2c_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm32f4_i2c_dev *i2c_dev;
	struct resource *res;
	u32 irq_event, irq_error, clk_rate;
	struct i2c_adapter *adap;
	struct reset_control *rst;
	int ret;

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev)
		return -ENOMEM;

	i2c_dev->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(i2c_dev->base))
		return PTR_ERR(i2c_dev->base);

	irq_event = irq_of_parse_and_map(np, 0);
	if (!irq_event) {
		dev_err(&pdev->dev, "IRQ event missing or invalid\n");
		return -EINVAL;
	}

	irq_error = irq_of_parse_and_map(np, 1);
	if (!irq_error) {
		dev_err(&pdev->dev, "IRQ error missing or invalid\n");
		return -EINVAL;
	}

	i2c_dev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(i2c_dev->clk)) {
		dev_err(&pdev->dev, "Error: Missing controller clock\n");
		return PTR_ERR(i2c_dev->clk);
	}
	ret = clk_prepare_enable(i2c_dev->clk);
	if (ret) {
		dev_err(i2c_dev->dev, "Failed to prepare_enable clock\n");
		return ret;
	}

	rst = devm_reset_control_get_exclusive(&pdev->dev, NULL);
	if (IS_ERR(rst)) {
		ret = dev_err_probe(&pdev->dev, PTR_ERR(rst),
				    "Error: Missing reset ctrl\n");
		goto clk_free;
	}
	reset_control_assert(rst);
	udelay(2);
	reset_control_deassert(rst);

	i2c_dev->speed = STM32_I2C_SPEED_STANDARD;
	ret = of_property_read_u32(np, "clock-frequency", &clk_rate);
	if (!ret && clk_rate >= I2C_MAX_FAST_MODE_FREQ)
		i2c_dev->speed = STM32_I2C_SPEED_FAST;

	i2c_dev->dev = &pdev->dev;

	ret = devm_request_irq(&pdev->dev, irq_event, stm32f4_i2c_isr_event, 0,
			       pdev->name, i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq event %i\n",
			irq_event);
		goto clk_free;
	}

	ret = devm_request_irq(&pdev->dev, irq_error, stm32f4_i2c_isr_error, 0,
			       pdev->name, i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq error %i\n",
			irq_error);
		goto clk_free;
	}

	ret = stm32f4_i2c_hw_config(i2c_dev);
	if (ret)
		goto clk_free;

	adap = &i2c_dev->adap;
	i2c_set_adapdata(adap, i2c_dev);
	snprintf(adap->name, sizeof(adap->name), "STM32 I2C(%pa)", &res->start);
	adap->owner = THIS_MODULE;
	adap->timeout = 2 * HZ;
	adap->retries = 0;
	adap->algo = &stm32f4_i2c_algo;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;

	init_completion(&i2c_dev->complete);

	ret = i2c_add_adapter(adap);
	if (ret)
		goto clk_free;

	platform_set_drvdata(pdev, i2c_dev);

	clk_disable(i2c_dev->clk);

	dev_info(i2c_dev->dev, "STM32F4 I2C driver registered\n");

	return 0;

clk_free:
	clk_disable_unprepare(i2c_dev->clk);
	return ret;
}

static void stm32f4_i2c_remove(struct platform_device *pdev)
{
	struct stm32f4_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c_dev->adap);

	clk_unprepare(i2c_dev->clk);
}

static const struct of_device_id stm32f4_i2c_match[] = {
	{ .compatible = "st,stm32f4-i2c", },
	{},
};
MODULE_DEVICE_TABLE(of, stm32f4_i2c_match);

static struct platform_driver stm32f4_i2c_driver = {
	.driver = {
		.name = "stm32f4-i2c",
		.of_match_table = stm32f4_i2c_match,
	},
	.probe = stm32f4_i2c_probe,
	.remove_new = stm32f4_i2c_remove,
};

module_platform_driver(stm32f4_i2c_driver);

MODULE_AUTHOR("M'boumba Cedric Madianga <cedric.madianga@gmail.com>");
MODULE_DESCRIPTION("STMicroelectronics STM32F4 I2C driver");
MODULE_LICENSE("GPL v2");
