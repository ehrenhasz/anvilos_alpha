
 

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <asm/irq.h>

#include <linux/platform_data/i2c-s3c2410.h>

 

#define S3C2410_IICCON			0x00
#define S3C2410_IICSTAT			0x04
#define S3C2410_IICADD			0x08
#define S3C2410_IICDS			0x0C
#define S3C2440_IICLC			0x10

#define S3C2410_IICCON_ACKEN		(1 << 7)
#define S3C2410_IICCON_TXDIV_16		(0 << 6)
#define S3C2410_IICCON_TXDIV_512	(1 << 6)
#define S3C2410_IICCON_IRQEN		(1 << 5)
#define S3C2410_IICCON_IRQPEND		(1 << 4)
#define S3C2410_IICCON_SCALE(x)		((x) & 0xf)
#define S3C2410_IICCON_SCALEMASK	(0xf)

#define S3C2410_IICSTAT_MASTER_RX	(2 << 6)
#define S3C2410_IICSTAT_MASTER_TX	(3 << 6)
#define S3C2410_IICSTAT_SLAVE_RX	(0 << 6)
#define S3C2410_IICSTAT_SLAVE_TX	(1 << 6)
#define S3C2410_IICSTAT_MODEMASK	(3 << 6)

#define S3C2410_IICSTAT_START		(1 << 5)
#define S3C2410_IICSTAT_BUSBUSY		(1 << 5)
#define S3C2410_IICSTAT_TXRXEN		(1 << 4)
#define S3C2410_IICSTAT_ARBITR		(1 << 3)
#define S3C2410_IICSTAT_ASSLAVE		(1 << 2)
#define S3C2410_IICSTAT_ADDR0		(1 << 1)
#define S3C2410_IICSTAT_LASTBIT		(1 << 0)

#define S3C2410_IICLC_SDA_DELAY0	(0 << 0)
#define S3C2410_IICLC_SDA_DELAY5	(1 << 0)
#define S3C2410_IICLC_SDA_DELAY10	(2 << 0)
#define S3C2410_IICLC_SDA_DELAY15	(3 << 0)
#define S3C2410_IICLC_SDA_DELAY_MASK	(3 << 0)

#define S3C2410_IICLC_FILTER_ON		(1 << 2)

 
#define QUIRK_S3C2440		(1 << 0)
#define QUIRK_HDMIPHY		(1 << 1)
#define QUIRK_NO_GPIO		(1 << 2)
#define QUIRK_POLL		(1 << 3)

 
#define S3C2410_IDLE_TIMEOUT	5000

 
#define EXYNOS5_SYS_I2C_CFG	0x0234

 
enum s3c24xx_i2c_state {
	STATE_IDLE,
	STATE_START,
	STATE_READ,
	STATE_WRITE,
	STATE_STOP
};

struct s3c24xx_i2c {
	wait_queue_head_t	wait;
	kernel_ulong_t		quirks;

	struct i2c_msg		*msg;
	unsigned int		msg_num;
	unsigned int		msg_idx;
	unsigned int		msg_ptr;

	unsigned int		tx_setup;
	unsigned int		irq;

	enum s3c24xx_i2c_state	state;
	unsigned long		clkrate;

	void __iomem		*regs;
	struct clk		*clk;
	struct device		*dev;
	struct i2c_adapter	adap;

	struct s3c2410_platform_i2c	*pdata;
	struct gpio_desc	*gpios[2];
	struct pinctrl          *pctrl;
	struct regmap		*sysreg;
	unsigned int		sys_i2c_cfg;
};

static const struct platform_device_id s3c24xx_driver_ids[] = {
	{
		.name		= "s3c2410-i2c",
		.driver_data	= 0,
	}, {
		.name		= "s3c2440-i2c",
		.driver_data	= QUIRK_S3C2440,
	}, {
		.name		= "s3c2440-hdmiphy-i2c",
		.driver_data	= QUIRK_S3C2440 | QUIRK_HDMIPHY | QUIRK_NO_GPIO,
	}, { },
};
MODULE_DEVICE_TABLE(platform, s3c24xx_driver_ids);

static int i2c_s3c_irq_nextbyte(struct s3c24xx_i2c *i2c, unsigned long iicstat);

#ifdef CONFIG_OF
static const struct of_device_id s3c24xx_i2c_match[] = {
	{ .compatible = "samsung,s3c2410-i2c", .data = (void *)0 },
	{ .compatible = "samsung,s3c2440-i2c", .data = (void *)QUIRK_S3C2440 },
	{ .compatible = "samsung,s3c2440-hdmiphy-i2c",
	  .data = (void *)(QUIRK_S3C2440 | QUIRK_HDMIPHY | QUIRK_NO_GPIO) },
	{ .compatible = "samsung,exynos5-sata-phy-i2c",
	  .data = (void *)(QUIRK_S3C2440 | QUIRK_POLL | QUIRK_NO_GPIO) },
	{},
};
MODULE_DEVICE_TABLE(of, s3c24xx_i2c_match);
#endif

 
static inline kernel_ulong_t s3c24xx_get_device_quirks(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		return (kernel_ulong_t)of_device_get_match_data(&pdev->dev);

	return platform_get_device_id(pdev)->driver_data;
}

 
static inline void s3c24xx_i2c_master_complete(struct s3c24xx_i2c *i2c, int ret)
{
	dev_dbg(i2c->dev, "master_complete %d\n", ret);

	i2c->msg_ptr = 0;
	i2c->msg = NULL;
	i2c->msg_idx++;
	i2c->msg_num = 0;
	if (ret)
		i2c->msg_idx = ret;

	if (!(i2c->quirks & QUIRK_POLL))
		wake_up(&i2c->wait);
}

static inline void s3c24xx_i2c_disable_ack(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	tmp = readl(i2c->regs + S3C2410_IICCON);
	writel(tmp & ~S3C2410_IICCON_ACKEN, i2c->regs + S3C2410_IICCON);
}

static inline void s3c24xx_i2c_enable_ack(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	tmp = readl(i2c->regs + S3C2410_IICCON);
	writel(tmp | S3C2410_IICCON_ACKEN, i2c->regs + S3C2410_IICCON);
}

 
static inline void s3c24xx_i2c_disable_irq(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	tmp = readl(i2c->regs + S3C2410_IICCON);
	writel(tmp & ~S3C2410_IICCON_IRQEN, i2c->regs + S3C2410_IICCON);
}

static inline void s3c24xx_i2c_enable_irq(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	tmp = readl(i2c->regs + S3C2410_IICCON);
	writel(tmp | S3C2410_IICCON_IRQEN, i2c->regs + S3C2410_IICCON);
}

static bool is_ack(struct s3c24xx_i2c *i2c)
{
	int tries;

	for (tries = 50; tries; --tries) {
		unsigned long tmp = readl(i2c->regs + S3C2410_IICCON);

		if (!(tmp & S3C2410_IICCON_ACKEN)) {
			 
			usleep_range(100, 200);
			return true;
		}
		if (tmp & S3C2410_IICCON_IRQPEND) {
			if (!(readl(i2c->regs + S3C2410_IICSTAT)
				& S3C2410_IICSTAT_LASTBIT))
				return true;
		}
		usleep_range(1000, 2000);
	}
	dev_err(i2c->dev, "ack was not received\n");
	return false;
}

 
static void s3c24xx_i2c_message_start(struct s3c24xx_i2c *i2c,
				      struct i2c_msg *msg)
{
	unsigned int addr = (msg->addr & 0x7f) << 1;
	unsigned long stat;
	unsigned long iiccon;

	stat = 0;
	stat |=  S3C2410_IICSTAT_TXRXEN;

	if (msg->flags & I2C_M_RD) {
		stat |= S3C2410_IICSTAT_MASTER_RX;
		addr |= 1;
	} else
		stat |= S3C2410_IICSTAT_MASTER_TX;

	if (msg->flags & I2C_M_REV_DIR_ADDR)
		addr ^= 1;

	 
	s3c24xx_i2c_enable_ack(i2c);

	iiccon = readl(i2c->regs + S3C2410_IICCON);
	writel(stat, i2c->regs + S3C2410_IICSTAT);

	dev_dbg(i2c->dev, "START: %08lx to IICSTAT, %02x to DS\n", stat, addr);
	writeb(addr, i2c->regs + S3C2410_IICDS);

	 
	ndelay(i2c->tx_setup);

	dev_dbg(i2c->dev, "iiccon, %08lx\n", iiccon);
	writel(iiccon, i2c->regs + S3C2410_IICCON);

	stat |= S3C2410_IICSTAT_START;
	writel(stat, i2c->regs + S3C2410_IICSTAT);
}

static inline void s3c24xx_i2c_stop(struct s3c24xx_i2c *i2c, int ret)
{
	unsigned long iicstat = readl(i2c->regs + S3C2410_IICSTAT);

	dev_dbg(i2c->dev, "STOP\n");

	 
	if (i2c->quirks & QUIRK_HDMIPHY) {
		 
		iicstat &= ~S3C2410_IICSTAT_TXRXEN;
	} else {
		 
		iicstat &= ~S3C2410_IICSTAT_START;
	}
	writel(iicstat, i2c->regs + S3C2410_IICSTAT);

	i2c->state = STATE_STOP;

	s3c24xx_i2c_master_complete(i2c, ret);
	s3c24xx_i2c_disable_irq(i2c);
}

 

 
static inline int is_lastmsg(struct s3c24xx_i2c *i2c)
{
	return i2c->msg_idx >= (i2c->msg_num - 1);
}

 
static inline int is_msglast(struct s3c24xx_i2c *i2c)
{
	 
	if (i2c->msg->flags & I2C_M_RECV_LEN && i2c->msg->len == 1)
		return 0;

	return i2c->msg_ptr == i2c->msg->len-1;
}

 
static inline int is_msgend(struct s3c24xx_i2c *i2c)
{
	return i2c->msg_ptr >= i2c->msg->len;
}

 
static int i2c_s3c_irq_nextbyte(struct s3c24xx_i2c *i2c, unsigned long iicstat)
{
	unsigned long tmp;
	unsigned char byte;
	int ret = 0;

	switch (i2c->state) {

	case STATE_IDLE:
		dev_err(i2c->dev, "%s: called in STATE_IDLE\n", __func__);
		goto out;

	case STATE_STOP:
		dev_err(i2c->dev, "%s: called in STATE_STOP\n", __func__);
		s3c24xx_i2c_disable_irq(i2c);
		goto out_ack;

	case STATE_START:
		 
		if (iicstat & S3C2410_IICSTAT_LASTBIT &&
		    !(i2c->msg->flags & I2C_M_IGNORE_NAK)) {
			 
			dev_dbg(i2c->dev, "ack was not received\n");
			s3c24xx_i2c_stop(i2c, -ENXIO);
			goto out_ack;
		}

		if (i2c->msg->flags & I2C_M_RD)
			i2c->state = STATE_READ;
		else
			i2c->state = STATE_WRITE;

		 
		if (is_lastmsg(i2c) && i2c->msg->len == 0) {
			s3c24xx_i2c_stop(i2c, 0);
			goto out_ack;
		}

		if (i2c->state == STATE_READ)
			goto prepare_read;

		 
		fallthrough;
	case STATE_WRITE:
		 
		if (!(i2c->msg->flags & I2C_M_IGNORE_NAK)) {
			if (iicstat & S3C2410_IICSTAT_LASTBIT) {
				dev_dbg(i2c->dev, "WRITE: No Ack\n");

				s3c24xx_i2c_stop(i2c, -ECONNREFUSED);
				goto out_ack;
			}
		}

 retry_write:

		if (!is_msgend(i2c)) {
			byte = i2c->msg->buf[i2c->msg_ptr++];
			writeb(byte, i2c->regs + S3C2410_IICDS);

			 
			ndelay(i2c->tx_setup);

		} else if (!is_lastmsg(i2c)) {
			 

			dev_dbg(i2c->dev, "WRITE: Next Message\n");

			i2c->msg_ptr = 0;
			i2c->msg_idx++;
			i2c->msg++;

			 
			if (i2c->msg->flags & I2C_M_NOSTART) {

				if (i2c->msg->flags & I2C_M_RD) {
					 
					dev_dbg(i2c->dev,
						"missing START before write->read\n");
					s3c24xx_i2c_stop(i2c, -EINVAL);
					break;
				}

				goto retry_write;
			} else {
				 
				s3c24xx_i2c_message_start(i2c, i2c->msg);
				i2c->state = STATE_START;
			}

		} else {
			 
			s3c24xx_i2c_stop(i2c, 0);
		}
		break;

	case STATE_READ:
		 
		byte = readb(i2c->regs + S3C2410_IICDS);
		i2c->msg->buf[i2c->msg_ptr++] = byte;

		 
		if (i2c->msg->flags & I2C_M_RECV_LEN && i2c->msg->len == 1)
			i2c->msg->len += byte;
 prepare_read:
		if (is_msglast(i2c)) {
			 

			if (is_lastmsg(i2c))
				s3c24xx_i2c_disable_ack(i2c);

		} else if (is_msgend(i2c)) {
			 
			if (is_lastmsg(i2c)) {
				 
				dev_dbg(i2c->dev, "READ: Send Stop\n");

				s3c24xx_i2c_stop(i2c, 0);
			} else {
				 
				dev_dbg(i2c->dev, "READ: Next Transfer\n");

				i2c->msg_ptr = 0;
				i2c->msg_idx++;
				i2c->msg++;
			}
		}

		break;
	}

	 

 out_ack:
	tmp = readl(i2c->regs + S3C2410_IICCON);
	tmp &= ~S3C2410_IICCON_IRQPEND;
	writel(tmp, i2c->regs + S3C2410_IICCON);
 out:
	return ret;
}

 
static irqreturn_t s3c24xx_i2c_irq(int irqno, void *dev_id)
{
	struct s3c24xx_i2c *i2c = dev_id;
	unsigned long status;
	unsigned long tmp;

	status = readl(i2c->regs + S3C2410_IICSTAT);

	if (status & S3C2410_IICSTAT_ARBITR) {
		 
		dev_err(i2c->dev, "deal with arbitration loss\n");
	}

	if (i2c->state == STATE_IDLE) {
		dev_dbg(i2c->dev, "IRQ: error i2c->state == IDLE\n");

		tmp = readl(i2c->regs + S3C2410_IICCON);
		tmp &= ~S3C2410_IICCON_IRQPEND;
		writel(tmp, i2c->regs +  S3C2410_IICCON);
		goto out;
	}

	 
	i2c_s3c_irq_nextbyte(i2c, status);

 out:
	return IRQ_HANDLED;
}

 
static inline void s3c24xx_i2c_disable_bus(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	 
	tmp = readl(i2c->regs + S3C2410_IICSTAT);
	tmp &= ~S3C2410_IICSTAT_TXRXEN;
	writel(tmp, i2c->regs + S3C2410_IICSTAT);

	 
	tmp = readl(i2c->regs + S3C2410_IICCON);
	tmp &= ~(S3C2410_IICCON_IRQEN | S3C2410_IICCON_IRQPEND |
		S3C2410_IICCON_ACKEN);
	writel(tmp, i2c->regs + S3C2410_IICCON);
}


 
static int s3c24xx_i2c_set_master(struct s3c24xx_i2c *i2c)
{
	unsigned long iicstat;
	int timeout = 400;

	while (timeout-- > 0) {
		iicstat = readl(i2c->regs + S3C2410_IICSTAT);

		if (!(iicstat & S3C2410_IICSTAT_BUSBUSY))
			return 0;

		msleep(1);
	}

	return -ETIMEDOUT;
}

 
static void s3c24xx_i2c_wait_idle(struct s3c24xx_i2c *i2c)
{
	unsigned long iicstat;
	ktime_t start, now;
	unsigned long delay;
	int spins;

	 

	dev_dbg(i2c->dev, "waiting for bus idle\n");

	start = now = ktime_get();

	 
	spins = 3;
	iicstat = readl(i2c->regs + S3C2410_IICSTAT);
	while ((iicstat & S3C2410_IICSTAT_START) && --spins) {
		cpu_relax();
		iicstat = readl(i2c->regs + S3C2410_IICSTAT);
	}

	 
	delay = 1;
	while ((iicstat & S3C2410_IICSTAT_START) &&
	       ktime_us_delta(now, start) < S3C2410_IDLE_TIMEOUT) {
		usleep_range(delay, 2 * delay);
		if (delay < S3C2410_IDLE_TIMEOUT / 10)
			delay <<= 1;
		now = ktime_get();
		iicstat = readl(i2c->regs + S3C2410_IICSTAT);
	}

	if (iicstat & S3C2410_IICSTAT_START)
		dev_warn(i2c->dev, "timeout waiting for bus idle\n");
}

 
static int s3c24xx_i2c_doxfer(struct s3c24xx_i2c *i2c,
			      struct i2c_msg *msgs, int num)
{
	unsigned long timeout = 0;
	int ret;

	ret = s3c24xx_i2c_set_master(i2c);
	if (ret != 0) {
		dev_err(i2c->dev, "cannot get bus (error %d)\n", ret);
		ret = -EAGAIN;
		goto out;
	}

	i2c->msg     = msgs;
	i2c->msg_num = num;
	i2c->msg_ptr = 0;
	i2c->msg_idx = 0;
	i2c->state   = STATE_START;

	s3c24xx_i2c_enable_irq(i2c);
	s3c24xx_i2c_message_start(i2c, msgs);

	if (i2c->quirks & QUIRK_POLL) {
		while ((i2c->msg_num != 0) && is_ack(i2c)) {
			unsigned long stat = readl(i2c->regs + S3C2410_IICSTAT);

			i2c_s3c_irq_nextbyte(i2c, stat);

			stat = readl(i2c->regs + S3C2410_IICSTAT);
			if (stat & S3C2410_IICSTAT_ARBITR)
				dev_err(i2c->dev, "deal with arbitration loss\n");
		}
	} else {
		timeout = wait_event_timeout(i2c->wait, i2c->msg_num == 0, HZ * 5);
	}

	ret = i2c->msg_idx;

	 
	if (timeout == 0)
		dev_dbg(i2c->dev, "timeout\n");
	else if (ret != num)
		dev_dbg(i2c->dev, "incomplete xfer (%d)\n", ret);

	 
	if (i2c->quirks & QUIRK_HDMIPHY)
		goto out;

	s3c24xx_i2c_wait_idle(i2c);

	s3c24xx_i2c_disable_bus(i2c);

 out:
	i2c->state = STATE_IDLE;

	return ret;
}

 
static int s3c24xx_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	struct s3c24xx_i2c *i2c = (struct s3c24xx_i2c *)adap->algo_data;
	int retry;
	int ret;

	ret = clk_enable(i2c->clk);
	if (ret)
		return ret;

	for (retry = 0; retry < adap->retries; retry++) {

		ret = s3c24xx_i2c_doxfer(i2c, msgs, num);

		if (ret != -EAGAIN) {
			clk_disable(i2c->clk);
			return ret;
		}

		dev_dbg(i2c->dev, "Retrying transmission (%d)\n", retry);

		udelay(100);
	}

	clk_disable(i2c->clk);
	return -EREMOTEIO;
}

 
static u32 s3c24xx_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL_ALL | I2C_FUNC_NOSTART |
		I2C_FUNC_PROTOCOL_MANGLING;
}

 
static const struct i2c_algorithm s3c24xx_i2c_algorithm = {
	.master_xfer		= s3c24xx_i2c_xfer,
	.functionality		= s3c24xx_i2c_func,
};

 
static int s3c24xx_i2c_calcdivisor(unsigned long clkin, unsigned int wanted,
				   unsigned int *div1, unsigned int *divs)
{
	unsigned int calc_divs = clkin / wanted;
	unsigned int calc_div1;

	if (calc_divs > (16*16))
		calc_div1 = 512;
	else
		calc_div1 = 16;

	calc_divs += calc_div1-1;
	calc_divs /= calc_div1;

	if (calc_divs == 0)
		calc_divs = 1;
	if (calc_divs > 17)
		calc_divs = 17;

	*divs = calc_divs;
	*div1 = calc_div1;

	return clkin / (calc_divs * calc_div1);
}

 
static int s3c24xx_i2c_clockrate(struct s3c24xx_i2c *i2c, unsigned int *got)
{
	struct s3c2410_platform_i2c *pdata = i2c->pdata;
	unsigned long clkin = clk_get_rate(i2c->clk);
	unsigned int divs, div1;
	unsigned long target_frequency;
	u32 iiccon;
	int freq;

	i2c->clkrate = clkin;
	clkin /= 1000;	 

	dev_dbg(i2c->dev, "pdata desired frequency %lu\n", pdata->frequency);

	target_frequency = pdata->frequency ?: I2C_MAX_STANDARD_MODE_FREQ;

	target_frequency /= 1000;  

	freq = s3c24xx_i2c_calcdivisor(clkin, target_frequency, &div1, &divs);

	if (freq > target_frequency) {
		dev_err(i2c->dev,
			"Unable to achieve desired frequency %luKHz."	\
			" Lowest achievable %dKHz\n", target_frequency, freq);
		return -EINVAL;
	}

	*got = freq;

	iiccon = readl(i2c->regs + S3C2410_IICCON);
	iiccon &= ~(S3C2410_IICCON_SCALEMASK | S3C2410_IICCON_TXDIV_512);
	iiccon |= (divs-1);

	if (div1 == 512)
		iiccon |= S3C2410_IICCON_TXDIV_512;

	if (i2c->quirks & QUIRK_POLL)
		iiccon |= S3C2410_IICCON_SCALE(2);

	writel(iiccon, i2c->regs + S3C2410_IICCON);

	if (i2c->quirks & QUIRK_S3C2440) {
		unsigned long sda_delay;

		if (pdata->sda_delay) {
			sda_delay = clkin * pdata->sda_delay;
			sda_delay = DIV_ROUND_UP(sda_delay, 1000000);
			sda_delay = DIV_ROUND_UP(sda_delay, 5);
			if (sda_delay > 3)
				sda_delay = 3;
			sda_delay |= S3C2410_IICLC_FILTER_ON;
		} else
			sda_delay = 0;

		dev_dbg(i2c->dev, "IICLC=%08lx\n", sda_delay);
		writel(sda_delay, i2c->regs + S3C2440_IICLC);
	}

	return 0;
}

#ifdef CONFIG_OF
static int s3c24xx_i2c_parse_dt_gpio(struct s3c24xx_i2c *i2c)
{
	int i;

	if (i2c->quirks & QUIRK_NO_GPIO)
		return 0;

	for (i = 0; i < 2; i++) {
		i2c->gpios[i] = devm_gpiod_get_index(i2c->dev, NULL,
						     i, GPIOD_ASIS);
		if (IS_ERR(i2c->gpios[i])) {
			dev_err(i2c->dev, "i2c gpio invalid at index %d\n", i);
			return -EINVAL;
		}
	}
	return 0;
}

#else
static int s3c24xx_i2c_parse_dt_gpio(struct s3c24xx_i2c *i2c)
{
	return 0;
}
#endif

 
static int s3c24xx_i2c_init(struct s3c24xx_i2c *i2c)
{
	struct s3c2410_platform_i2c *pdata;
	unsigned int freq;

	 

	pdata = i2c->pdata;

	 

	writeb(pdata->slave_addr, i2c->regs + S3C2410_IICADD);

	dev_info(i2c->dev, "slave address 0x%02x\n", pdata->slave_addr);

	writel(0, i2c->regs + S3C2410_IICCON);
	writel(0, i2c->regs + S3C2410_IICSTAT);

	 

	if (s3c24xx_i2c_clockrate(i2c, &freq) != 0) {
		dev_err(i2c->dev, "cannot meet bus frequency required\n");
		return -EINVAL;
	}

	 

	dev_info(i2c->dev, "bus frequency set to %d KHz\n", freq);
	dev_dbg(i2c->dev, "S3C2410_IICCON=0x%02x\n",
		readl(i2c->regs + S3C2410_IICCON));

	return 0;
}

#ifdef CONFIG_OF
 
static void
s3c24xx_i2c_parse_dt(struct device_node *np, struct s3c24xx_i2c *i2c)
{
	struct s3c2410_platform_i2c *pdata = i2c->pdata;
	int id;

	if (!np)
		return;

	pdata->bus_num = -1;  
	of_property_read_u32(np, "samsung,i2c-sda-delay", &pdata->sda_delay);
	of_property_read_u32(np, "samsung,i2c-slave-addr", &pdata->slave_addr);
	of_property_read_u32(np, "samsung,i2c-max-bus-freq",
				(u32 *)&pdata->frequency);
	 
	id = of_alias_get_id(np, "i2c");
	i2c->sysreg = syscon_regmap_lookup_by_phandle(np,
			"samsung,sysreg-phandle");
	if (IS_ERR(i2c->sysreg))
		return;

	regmap_update_bits(i2c->sysreg, EXYNOS5_SYS_I2C_CFG, BIT(id), 0);
}
#else
static void
s3c24xx_i2c_parse_dt(struct device_node *np, struct s3c24xx_i2c *i2c) { }
#endif

static int s3c24xx_i2c_probe(struct platform_device *pdev)
{
	struct s3c24xx_i2c *i2c;
	struct s3c2410_platform_i2c *pdata = NULL;
	struct resource *res;
	int ret;

	if (!pdev->dev.of_node) {
		pdata = dev_get_platdata(&pdev->dev);
		if (!pdata) {
			dev_err(&pdev->dev, "no platform data\n");
			return -EINVAL;
		}
	}

	i2c = devm_kzalloc(&pdev->dev, sizeof(struct s3c24xx_i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	i2c->pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!i2c->pdata)
		return -ENOMEM;

	i2c->quirks = s3c24xx_get_device_quirks(pdev);
	i2c->sysreg = ERR_PTR(-ENOENT);
	if (pdata)
		memcpy(i2c->pdata, pdata, sizeof(*pdata));
	else
		s3c24xx_i2c_parse_dt(pdev->dev.of_node, i2c);

	strscpy(i2c->adap.name, "s3c2410-i2c", sizeof(i2c->adap.name));
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &s3c24xx_i2c_algorithm;
	i2c->adap.retries = 2;
	i2c->adap.class = I2C_CLASS_DEPRECATED;
	i2c->tx_setup = 50;

	init_waitqueue_head(&i2c->wait);

	 
	i2c->dev = &pdev->dev;
	i2c->clk = devm_clk_get(&pdev->dev, "i2c");
	if (IS_ERR(i2c->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		return -ENOENT;
	}

	dev_dbg(&pdev->dev, "clock source %p\n", i2c->clk);

	 
	i2c->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(i2c->regs))
		return PTR_ERR(i2c->regs);

	dev_dbg(&pdev->dev, "registers %p (%p)\n",
		i2c->regs, res);

	 
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->pctrl = devm_pinctrl_get_select_default(i2c->dev);

	 
	if (i2c->pdata->cfg_gpio)
		i2c->pdata->cfg_gpio(to_platform_device(i2c->dev));
	else if (IS_ERR(i2c->pctrl) && s3c24xx_i2c_parse_dt_gpio(i2c))
		return -EINVAL;

	 
	ret = clk_prepare_enable(i2c->clk);
	if (ret) {
		dev_err(&pdev->dev, "I2C clock enable failed\n");
		return ret;
	}

	ret = s3c24xx_i2c_init(i2c);
	clk_disable(i2c->clk);
	if (ret != 0) {
		dev_err(&pdev->dev, "I2C controller init failed\n");
		clk_unprepare(i2c->clk);
		return ret;
	}

	 
	if (!(i2c->quirks & QUIRK_POLL)) {
		i2c->irq = ret = platform_get_irq(pdev, 0);
		if (ret < 0) {
			clk_unprepare(i2c->clk);
			return ret;
		}

		ret = devm_request_irq(&pdev->dev, i2c->irq, s3c24xx_i2c_irq,
				       0, dev_name(&pdev->dev), i2c);
		if (ret != 0) {
			dev_err(&pdev->dev, "cannot claim IRQ %d\n", i2c->irq);
			clk_unprepare(i2c->clk);
			return ret;
		}
	}

	 
	i2c->adap.nr = i2c->pdata->bus_num;
	i2c->adap.dev.of_node = pdev->dev.of_node;

	platform_set_drvdata(pdev, i2c);

	pm_runtime_enable(&pdev->dev);

	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret < 0) {
		pm_runtime_disable(&pdev->dev);
		clk_unprepare(i2c->clk);
		return ret;
	}

	dev_info(&pdev->dev, "%s: S3C I2C adapter\n", dev_name(&i2c->adap.dev));
	return 0;
}

static void s3c24xx_i2c_remove(struct platform_device *pdev)
{
	struct s3c24xx_i2c *i2c = platform_get_drvdata(pdev);

	clk_unprepare(i2c->clk);

	pm_runtime_disable(&pdev->dev);

	i2c_del_adapter(&i2c->adap);
}

static int s3c24xx_i2c_suspend_noirq(struct device *dev)
{
	struct s3c24xx_i2c *i2c = dev_get_drvdata(dev);

	i2c_mark_adapter_suspended(&i2c->adap);

	if (!IS_ERR(i2c->sysreg))
		regmap_read(i2c->sysreg, EXYNOS5_SYS_I2C_CFG, &i2c->sys_i2c_cfg);

	return 0;
}

static int s3c24xx_i2c_resume_noirq(struct device *dev)
{
	struct s3c24xx_i2c *i2c = dev_get_drvdata(dev);
	int ret;

	if (!IS_ERR(i2c->sysreg))
		regmap_write(i2c->sysreg, EXYNOS5_SYS_I2C_CFG, i2c->sys_i2c_cfg);

	ret = clk_enable(i2c->clk);
	if (ret)
		return ret;
	s3c24xx_i2c_init(i2c);
	clk_disable(i2c->clk);
	i2c_mark_adapter_resumed(&i2c->adap);

	return 0;
}

static const struct dev_pm_ops s3c24xx_i2c_dev_pm_ops = {
	NOIRQ_SYSTEM_SLEEP_PM_OPS(s3c24xx_i2c_suspend_noirq,
				  s3c24xx_i2c_resume_noirq)
};

static struct platform_driver s3c24xx_i2c_driver = {
	.probe		= s3c24xx_i2c_probe,
	.remove_new	= s3c24xx_i2c_remove,
	.id_table	= s3c24xx_driver_ids,
	.driver		= {
		.name	= "s3c-i2c",
		.pm	= pm_sleep_ptr(&s3c24xx_i2c_dev_pm_ops),
		.of_match_table = of_match_ptr(s3c24xx_i2c_match),
	},
};

static int __init i2c_adap_s3c_init(void)
{
	return platform_driver_register(&s3c24xx_i2c_driver);
}
subsys_initcall(i2c_adap_s3c_init);

static void __exit i2c_adap_s3c_exit(void)
{
	platform_driver_unregister(&s3c24xx_i2c_driver);
}
module_exit(i2c_adap_s3c_exit);

MODULE_DESCRIPTION("S3C24XX I2C Bus driver");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");
