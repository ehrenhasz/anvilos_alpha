
 

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#define MAX310X_NAME			"max310x"
#define MAX310X_MAJOR			204
#define MAX310X_MINOR			209
#define MAX310X_UART_NRMAX		16

 
#define MAX310X_RHR_REG			(0x00)  
#define MAX310X_THR_REG			(0x00)  
#define MAX310X_IRQEN_REG		(0x01)  
#define MAX310X_IRQSTS_REG		(0x02)  
#define MAX310X_LSR_IRQEN_REG		(0x03)  
#define MAX310X_LSR_IRQSTS_REG		(0x04)  
#define MAX310X_REG_05			(0x05)
#define MAX310X_SPCHR_IRQEN_REG		MAX310X_REG_05  
#define MAX310X_SPCHR_IRQSTS_REG	(0x06)  
#define MAX310X_STS_IRQEN_REG		(0x07)  
#define MAX310X_STS_IRQSTS_REG		(0x08)  
#define MAX310X_MODE1_REG		(0x09)  
#define MAX310X_MODE2_REG		(0x0a)  
#define MAX310X_LCR_REG			(0x0b)  
#define MAX310X_RXTO_REG		(0x0c)  
#define MAX310X_HDPIXDELAY_REG		(0x0d)  
#define MAX310X_IRDA_REG		(0x0e)  
#define MAX310X_FLOWLVL_REG		(0x0f)  
#define MAX310X_FIFOTRIGLVL_REG		(0x10)  
#define MAX310X_TXFIFOLVL_REG		(0x11)  
#define MAX310X_RXFIFOLVL_REG		(0x12)  
#define MAX310X_FLOWCTRL_REG		(0x13)  
#define MAX310X_XON1_REG		(0x14)  
#define MAX310X_XON2_REG		(0x15)  
#define MAX310X_XOFF1_REG		(0x16)  
#define MAX310X_XOFF2_REG		(0x17)  
#define MAX310X_GPIOCFG_REG		(0x18)  
#define MAX310X_GPIODATA_REG		(0x19)  
#define MAX310X_PLLCFG_REG		(0x1a)  
#define MAX310X_BRGCFG_REG		(0x1b)  
#define MAX310X_BRGDIVLSB_REG		(0x1c)  
#define MAX310X_BRGDIVMSB_REG		(0x1d)  
#define MAX310X_CLKSRC_REG		(0x1e)  
#define MAX310X_REG_1F			(0x1f)

#define MAX310X_REVID_REG		MAX310X_REG_1F  

#define MAX310X_GLOBALIRQ_REG		MAX310X_REG_1F  
#define MAX310X_GLOBALCMD_REG		MAX310X_REG_1F  

 
#define MAX310X_SPI_REVID_EXTREG	MAX310X_REG_05  
#define MAX310X_I2C_REVID_EXTREG	(0x25)  

 
#define MAX310X_IRQ_LSR_BIT		(1 << 0)  
#define MAX310X_IRQ_SPCHR_BIT		(1 << 1)  
#define MAX310X_IRQ_STS_BIT		(1 << 2)  
#define MAX310X_IRQ_RXFIFO_BIT		(1 << 3)  
#define MAX310X_IRQ_TXFIFO_BIT		(1 << 4)  
#define MAX310X_IRQ_TXEMPTY_BIT		(1 << 5)  
#define MAX310X_IRQ_RXEMPTY_BIT		(1 << 6)  
#define MAX310X_IRQ_CTS_BIT		(1 << 7)  

 
#define MAX310X_LSR_RXTO_BIT		(1 << 0)  
#define MAX310X_LSR_RXOVR_BIT		(1 << 1)  
#define MAX310X_LSR_RXPAR_BIT		(1 << 2)  
#define MAX310X_LSR_FRERR_BIT		(1 << 3)  
#define MAX310X_LSR_RXBRK_BIT		(1 << 4)  
#define MAX310X_LSR_RXNOISE_BIT		(1 << 5)  
#define MAX310X_LSR_CTS_BIT		(1 << 7)  

 
#define MAX310X_SPCHR_XON1_BIT		(1 << 0)  
#define MAX310X_SPCHR_XON2_BIT		(1 << 1)  
#define MAX310X_SPCHR_XOFF1_BIT		(1 << 2)  
#define MAX310X_SPCHR_XOFF2_BIT		(1 << 3)  
#define MAX310X_SPCHR_BREAK_BIT		(1 << 4)  
#define MAX310X_SPCHR_MULTIDROP_BIT	(1 << 5)  

 
#define MAX310X_STS_GPIO0_BIT		(1 << 0)  
#define MAX310X_STS_GPIO1_BIT		(1 << 1)  
#define MAX310X_STS_GPIO2_BIT		(1 << 2)  
#define MAX310X_STS_GPIO3_BIT		(1 << 3)  
#define MAX310X_STS_CLKREADY_BIT	(1 << 5)  
#define MAX310X_STS_SLEEP_BIT		(1 << 6)  

 
#define MAX310X_MODE1_RXDIS_BIT		(1 << 0)  
#define MAX310X_MODE1_TXDIS_BIT		(1 << 1)  
#define MAX310X_MODE1_TXHIZ_BIT		(1 << 2)  
#define MAX310X_MODE1_RTSHIZ_BIT	(1 << 3)  
#define MAX310X_MODE1_TRNSCVCTRL_BIT	(1 << 4)  
#define MAX310X_MODE1_FORCESLEEP_BIT	(1 << 5)  
#define MAX310X_MODE1_AUTOSLEEP_BIT	(1 << 6)  
#define MAX310X_MODE1_IRQSEL_BIT	(1 << 7)  

 
#define MAX310X_MODE2_RST_BIT		(1 << 0)  
#define MAX310X_MODE2_FIFORST_BIT	(1 << 1)  
#define MAX310X_MODE2_RXTRIGINV_BIT	(1 << 2)  
#define MAX310X_MODE2_RXEMPTINV_BIT	(1 << 3)  
#define MAX310X_MODE2_SPCHR_BIT		(1 << 4)  
#define MAX310X_MODE2_LOOPBACK_BIT	(1 << 5)  
#define MAX310X_MODE2_MULTIDROP_BIT	(1 << 6)  
#define MAX310X_MODE2_ECHOSUPR_BIT	(1 << 7)  

 
#define MAX310X_LCR_LENGTH0_BIT		(1 << 0)  
#define MAX310X_LCR_LENGTH1_BIT		(1 << 1)  
#define MAX310X_LCR_STOPLEN_BIT		(1 << 2)  
#define MAX310X_LCR_PARITY_BIT		(1 << 3)  
#define MAX310X_LCR_EVENPARITY_BIT	(1 << 4)  
#define MAX310X_LCR_FORCEPARITY_BIT	(1 << 5)  
#define MAX310X_LCR_TXBREAK_BIT		(1 << 6)  
#define MAX310X_LCR_RTS_BIT		(1 << 7)  

 
#define MAX310X_IRDA_IRDAEN_BIT		(1 << 0)  
#define MAX310X_IRDA_SIR_BIT		(1 << 1)  

 
#define MAX310X_FLOWLVL_HALT_MASK	(0x000f)  
#define MAX310X_FLOWLVL_RES_MASK	(0x00f0)  
#define MAX310X_FLOWLVL_HALT(words)	((words / 8) & 0x0f)
#define MAX310X_FLOWLVL_RES(words)	(((words / 8) & 0x0f) << 4)

 
#define MAX310X_FIFOTRIGLVL_TX_MASK	(0x0f)  
#define MAX310X_FIFOTRIGLVL_RX_MASK	(0xf0)  
#define MAX310X_FIFOTRIGLVL_TX(words)	((words / 8) & 0x0f)
#define MAX310X_FIFOTRIGLVL_RX(words)	(((words / 8) & 0x0f) << 4)

 
#define MAX310X_FLOWCTRL_AUTORTS_BIT	(1 << 0)  
#define MAX310X_FLOWCTRL_AUTOCTS_BIT	(1 << 1)  
#define MAX310X_FLOWCTRL_GPIADDR_BIT	(1 << 2)  
#define MAX310X_FLOWCTRL_SWFLOWEN_BIT	(1 << 3)  
#define MAX310X_FLOWCTRL_SWFLOW0_BIT	(1 << 4)  
#define MAX310X_FLOWCTRL_SWFLOW1_BIT	(1 << 5)  
#define MAX310X_FLOWCTRL_SWFLOW2_BIT	(1 << 6)  
#define MAX310X_FLOWCTRL_SWFLOW3_BIT	(1 << 7)  

 
#define MAX310X_PLLCFG_PREDIV_MASK	(0x3f)  
#define MAX310X_PLLCFG_PLLFACTOR_MASK	(0xc0)  

 
#define MAX310X_BRGCFG_2XMODE_BIT	(1 << 4)  
#define MAX310X_BRGCFG_4XMODE_BIT	(1 << 5)  

 
#define MAX310X_CLKSRC_CRYST_BIT	(1 << 1)  
#define MAX310X_CLKSRC_PLL_BIT		(1 << 2)  
#define MAX310X_CLKSRC_PLLBYP_BIT	(1 << 3)  
#define MAX310X_CLKSRC_EXTCLK_BIT	(1 << 4)  
#define MAX310X_CLKSRC_CLK2RTS_BIT	(1 << 7)  

 
#define MAX310X_EXTREG_ENBL		(0xce)
#define MAX310X_EXTREG_DSBL		(0xcd)

 
#define MAX310X_FIFO_SIZE		(128)
#define MAX310x_REV_MASK		(0xf8)
#define MAX310X_WRITE_BIT		0x80

 
#define MAX3107_REV_ID			(0xa0)

 
#define MAX3109_REV_ID			(0xc0)

 
#define MAX14830_BRGCFG_CLKDIS_BIT	(1 << 6)  
#define MAX14830_REV_ID			(0xb0)

struct max310x_if_cfg {
	int (*extended_reg_enable)(struct device *dev, bool enable);

	unsigned int rev_id_reg;
};

struct max310x_devtype {
	struct {
		unsigned short min;
		unsigned short max;
	} slave_addr;
	char	name[9];
	int	nr;
	u8	mode1;
	int	(*detect)(struct device *);
	void	(*power)(struct uart_port *, int);
};

struct max310x_one {
	struct uart_port	port;
	struct work_struct	tx_work;
	struct work_struct	md_work;
	struct work_struct	rs_work;
	struct regmap		*regmap;

	u8 rx_buf[MAX310X_FIFO_SIZE];
};
#define to_max310x_port(_port) \
	container_of(_port, struct max310x_one, port)

struct max310x_port {
	const struct max310x_devtype *devtype;
	const struct max310x_if_cfg *if_cfg;
	struct regmap		*regmap;
	struct clk		*clk;
#ifdef CONFIG_GPIOLIB
	struct gpio_chip	gpio;
#endif
	struct max310x_one	p[];
};

static struct uart_driver max310x_uart = {
	.owner		= THIS_MODULE,
	.driver_name	= MAX310X_NAME,
	.dev_name	= "ttyMAX",
	.major		= MAX310X_MAJOR,
	.minor		= MAX310X_MINOR,
	.nr		= MAX310X_UART_NRMAX,
};

static DECLARE_BITMAP(max310x_lines, MAX310X_UART_NRMAX);

static u8 max310x_port_read(struct uart_port *port, u8 reg)
{
	struct max310x_one *one = to_max310x_port(port);
	unsigned int val = 0;

	regmap_read(one->regmap, reg, &val);

	return val;
}

static void max310x_port_write(struct uart_port *port, u8 reg, u8 val)
{
	struct max310x_one *one = to_max310x_port(port);

	regmap_write(one->regmap, reg, val);
}

static void max310x_port_update(struct uart_port *port, u8 reg, u8 mask, u8 val)
{
	struct max310x_one *one = to_max310x_port(port);

	regmap_update_bits(one->regmap, reg, mask, val);
}

static int max3107_detect(struct device *dev)
{
	struct max310x_port *s = dev_get_drvdata(dev);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(s->regmap, MAX310X_REVID_REG, &val);
	if (ret)
		return ret;

	if (((val & MAX310x_REV_MASK) != MAX3107_REV_ID)) {
		dev_err(dev,
			"%s ID 0x%02x does not match\n", s->devtype->name, val);
		return -ENODEV;
	}

	return 0;
}

static int max3108_detect(struct device *dev)
{
	struct max310x_port *s = dev_get_drvdata(dev);
	unsigned int val = 0;
	int ret;

	 
	ret = regmap_read(s->regmap, MAX310X_CLKSRC_REG, &val);
	if (ret)
		return ret;

	if (val != (MAX310X_CLKSRC_EXTCLK_BIT | MAX310X_CLKSRC_PLLBYP_BIT)) {
		dev_err(dev, "%s not present\n", s->devtype->name);
		return -ENODEV;
	}

	return 0;
}

static int max3109_detect(struct device *dev)
{
	struct max310x_port *s = dev_get_drvdata(dev);
	unsigned int val = 0;
	int ret;

	ret = s->if_cfg->extended_reg_enable(dev, true);
	if (ret)
		return ret;

	regmap_read(s->regmap, s->if_cfg->rev_id_reg, &val);
	s->if_cfg->extended_reg_enable(dev, false);
	if (((val & MAX310x_REV_MASK) != MAX3109_REV_ID)) {
		dev_err(dev,
			"%s ID 0x%02x does not match\n", s->devtype->name, val);
		return -ENODEV;
	}

	return 0;
}

static void max310x_power(struct uart_port *port, int on)
{
	max310x_port_update(port, MAX310X_MODE1_REG,
			    MAX310X_MODE1_FORCESLEEP_BIT,
			    on ? 0 : MAX310X_MODE1_FORCESLEEP_BIT);
	if (on)
		msleep(50);
}

static int max14830_detect(struct device *dev)
{
	struct max310x_port *s = dev_get_drvdata(dev);
	unsigned int val = 0;
	int ret;

	ret = s->if_cfg->extended_reg_enable(dev, true);
	if (ret)
		return ret;
	
	regmap_read(s->regmap, s->if_cfg->rev_id_reg, &val);
	s->if_cfg->extended_reg_enable(dev, false);
	if (((val & MAX310x_REV_MASK) != MAX14830_REV_ID)) {
		dev_err(dev,
			"%s ID 0x%02x does not match\n", s->devtype->name, val);
		return -ENODEV;
	}

	return 0;
}

static void max14830_power(struct uart_port *port, int on)
{
	max310x_port_update(port, MAX310X_BRGCFG_REG,
			    MAX14830_BRGCFG_CLKDIS_BIT,
			    on ? 0 : MAX14830_BRGCFG_CLKDIS_BIT);
	if (on)
		msleep(50);
}

static const struct max310x_devtype max3107_devtype = {
	.name	= "MAX3107",
	.nr	= 1,
	.mode1	= MAX310X_MODE1_AUTOSLEEP_BIT | MAX310X_MODE1_IRQSEL_BIT,
	.detect	= max3107_detect,
	.power	= max310x_power,
	.slave_addr	= {
		.min = 0x2c,
		.max = 0x2f,
	},
};

static const struct max310x_devtype max3108_devtype = {
	.name	= "MAX3108",
	.nr	= 1,
	.mode1	= MAX310X_MODE1_AUTOSLEEP_BIT,
	.detect	= max3108_detect,
	.power	= max310x_power,
	.slave_addr	= {
		.min = 0x60,
		.max = 0x6f,
	},
};

static const struct max310x_devtype max3109_devtype = {
	.name	= "MAX3109",
	.nr	= 2,
	.mode1	= MAX310X_MODE1_AUTOSLEEP_BIT,
	.detect	= max3109_detect,
	.power	= max310x_power,
	.slave_addr	= {
		.min = 0x60,
		.max = 0x6f,
	},
};

static const struct max310x_devtype max14830_devtype = {
	.name	= "MAX14830",
	.nr	= 4,
	.mode1	= MAX310X_MODE1_IRQSEL_BIT,
	.detect	= max14830_detect,
	.power	= max14830_power,
	.slave_addr	= {
		.min = 0x60,
		.max = 0x6f,
	},
};

static bool max310x_reg_writeable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX310X_IRQSTS_REG:
	case MAX310X_LSR_IRQSTS_REG:
	case MAX310X_SPCHR_IRQSTS_REG:
	case MAX310X_STS_IRQSTS_REG:
	case MAX310X_TXFIFOLVL_REG:
	case MAX310X_RXFIFOLVL_REG:
		return false;
	default:
		break;
	}

	return true;
}

static bool max310x_reg_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX310X_RHR_REG:
	case MAX310X_IRQSTS_REG:
	case MAX310X_LSR_IRQSTS_REG:
	case MAX310X_SPCHR_IRQSTS_REG:
	case MAX310X_STS_IRQSTS_REG:
	case MAX310X_TXFIFOLVL_REG:
	case MAX310X_RXFIFOLVL_REG:
	case MAX310X_GPIODATA_REG:
	case MAX310X_BRGDIVLSB_REG:
	case MAX310X_REG_05:
	case MAX310X_REG_1F:
		return true;
	default:
		break;
	}

	return false;
}

static bool max310x_reg_precious(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX310X_RHR_REG:
	case MAX310X_IRQSTS_REG:
	case MAX310X_SPCHR_IRQSTS_REG:
	case MAX310X_STS_IRQSTS_REG:
		return true;
	default:
		break;
	}

	return false;
}

static bool max310x_reg_noinc(struct device *dev, unsigned int reg)
{
	return reg == MAX310X_RHR_REG;
}

static int max310x_set_baud(struct uart_port *port, int baud)
{
	unsigned int mode = 0, div = 0, frac = 0, c = 0, F = 0;

	 
	div = port->uartclk / baud;
	if (div < 8) {
		 
		c = 4;
		mode = MAX310X_BRGCFG_4XMODE_BIT;
	} else if (div < 16) {
		 
		c = 8;
		mode = MAX310X_BRGCFG_2XMODE_BIT;
	} else {
		c = 16;
	}

	 
	div /= c;
	F = c*baud;

	 
	if (div > 0)
		frac = (16*(port->uartclk % F)) / F;
	else
		div = 1;

	max310x_port_write(port, MAX310X_BRGDIVMSB_REG, div >> 8);
	max310x_port_write(port, MAX310X_BRGDIVLSB_REG, div);
	max310x_port_write(port, MAX310X_BRGCFG_REG, frac | mode);

	 
	return (16*port->uartclk) / (c*(16*div + frac));
}

static int max310x_update_best_err(unsigned long f, long *besterr)
{
	 
	long err = f % (460800 * 16);

	if ((*besterr < 0) || (*besterr > err)) {
		*besterr = err;
		return 0;
	}

	return 1;
}

static u32 max310x_set_ref_clk(struct device *dev, struct max310x_port *s,
			       unsigned long freq, bool xtal)
{
	unsigned int div, clksrc, pllcfg = 0;
	long besterr = -1;
	unsigned long fdiv, fmul, bestfreq = freq;

	 
	max310x_update_best_err(freq, &besterr);

	 
	for (div = 1; (div <= 63) && besterr; div++) {
		fdiv = DIV_ROUND_CLOSEST(freq, div);

		 
		fmul = fdiv * 6;
		if ((fdiv >= 500000) && (fdiv <= 800000))
			if (!max310x_update_best_err(fmul, &besterr)) {
				pllcfg = (0 << 6) | div;
				bestfreq = fmul;
			}
		 
		fmul = fdiv * 48;
		if ((fdiv >= 850000) && (fdiv <= 1200000))
			if (!max310x_update_best_err(fmul, &besterr)) {
				pllcfg = (1 << 6) | div;
				bestfreq = fmul;
			}
		 
		fmul = fdiv * 96;
		if ((fdiv >= 425000) && (fdiv <= 1000000))
			if (!max310x_update_best_err(fmul, &besterr)) {
				pllcfg = (2 << 6) | div;
				bestfreq = fmul;
			}
		 
		fmul = fdiv * 144;
		if ((fdiv >= 390000) && (fdiv <= 667000))
			if (!max310x_update_best_err(fmul, &besterr)) {
				pllcfg = (3 << 6) | div;
				bestfreq = fmul;
			}
	}

	 
	clksrc = MAX310X_CLKSRC_EXTCLK_BIT | (xtal ? MAX310X_CLKSRC_CRYST_BIT : 0);

	 
	if (pllcfg) {
		clksrc |= MAX310X_CLKSRC_PLL_BIT;
		regmap_write(s->regmap, MAX310X_PLLCFG_REG, pllcfg);
	} else
		clksrc |= MAX310X_CLKSRC_PLLBYP_BIT;

	regmap_write(s->regmap, MAX310X_CLKSRC_REG, clksrc);

	 
	if (xtal) {
		unsigned int val;
		msleep(10);
		regmap_read(s->regmap, MAX310X_STS_IRQSTS_REG, &val);
		if (!(val & MAX310X_STS_CLKREADY_BIT)) {
			dev_warn(dev, "clock is not stable yet\n");
		}
	}

	return bestfreq;
}

static void max310x_batch_write(struct uart_port *port, u8 *txbuf, unsigned int len)
{
	struct max310x_one *one = to_max310x_port(port);

	regmap_noinc_write(one->regmap, MAX310X_THR_REG, txbuf, len);
}

static void max310x_batch_read(struct uart_port *port, u8 *rxbuf, unsigned int len)
{
	struct max310x_one *one = to_max310x_port(port);

	regmap_noinc_read(one->regmap, MAX310X_RHR_REG, rxbuf, len);
}

static void max310x_handle_rx(struct uart_port *port, unsigned int rxlen)
{
	struct max310x_one *one = to_max310x_port(port);
	unsigned int sts, i;
	u8 ch, flag;

	if (port->read_status_mask == MAX310X_LSR_RXOVR_BIT) {
		 

		sts = max310x_port_read(port, MAX310X_LSR_IRQSTS_REG);
		max310x_batch_read(port, one->rx_buf, rxlen);

		port->icount.rx += rxlen;
		flag = TTY_NORMAL;
		sts &= port->read_status_mask;

		if (sts & MAX310X_LSR_RXOVR_BIT) {
			dev_warn_ratelimited(port->dev, "Hardware RX FIFO overrun\n");
			port->icount.overrun++;
		}

		for (i = 0; i < (rxlen - 1); ++i)
			uart_insert_char(port, sts, 0, one->rx_buf[i], flag);

		 
		uart_insert_char(port, sts, MAX310X_LSR_RXOVR_BIT,
				 one->rx_buf[rxlen-1], flag);

	} else {
		if (unlikely(rxlen >= port->fifosize)) {
			dev_warn_ratelimited(port->dev, "Possible RX FIFO overrun\n");
			port->icount.buf_overrun++;
			 
			rxlen = port->fifosize;
		}

		while (rxlen--) {
			ch = max310x_port_read(port, MAX310X_RHR_REG);
			sts = max310x_port_read(port, MAX310X_LSR_IRQSTS_REG);

			sts &= MAX310X_LSR_RXPAR_BIT | MAX310X_LSR_FRERR_BIT |
			       MAX310X_LSR_RXOVR_BIT | MAX310X_LSR_RXBRK_BIT;

			port->icount.rx++;
			flag = TTY_NORMAL;

			if (unlikely(sts)) {
				if (sts & MAX310X_LSR_RXBRK_BIT) {
					port->icount.brk++;
					if (uart_handle_break(port))
						continue;
				} else if (sts & MAX310X_LSR_RXPAR_BIT)
					port->icount.parity++;
				else if (sts & MAX310X_LSR_FRERR_BIT)
					port->icount.frame++;
				else if (sts & MAX310X_LSR_RXOVR_BIT)
					port->icount.overrun++;

				sts &= port->read_status_mask;
				if (sts & MAX310X_LSR_RXBRK_BIT)
					flag = TTY_BREAK;
				else if (sts & MAX310X_LSR_RXPAR_BIT)
					flag = TTY_PARITY;
				else if (sts & MAX310X_LSR_FRERR_BIT)
					flag = TTY_FRAME;
				else if (sts & MAX310X_LSR_RXOVR_BIT)
					flag = TTY_OVERRUN;
			}

			if (uart_handle_sysrq_char(port, ch))
				continue;

			if (sts & port->ignore_status_mask)
				continue;

			uart_insert_char(port, sts, MAX310X_LSR_RXOVR_BIT, ch, flag);
		}
	}

	tty_flip_buffer_push(&port->state->port);
}

static void max310x_handle_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int txlen, to_send, until_end;

	if (unlikely(port->x_char)) {
		max310x_port_write(port, MAX310X_THR_REG, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(port))
		return;

	 
	to_send = uart_circ_chars_pending(xmit);
	until_end = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
	if (likely(to_send)) {
		 
		txlen = max310x_port_read(port, MAX310X_TXFIFOLVL_REG);
		txlen = port->fifosize - txlen;
		to_send = (to_send > txlen) ? txlen : to_send;

		if (until_end < to_send) {
			 
			max310x_batch_write(port, xmit->buf + xmit->tail, until_end);
			max310x_batch_write(port, xmit->buf, to_send - until_end);
		} else {
			max310x_batch_write(port, xmit->buf + xmit->tail, to_send);
		}
		uart_xmit_advance(port, to_send);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);
}

static void max310x_start_tx(struct uart_port *port)
{
	struct max310x_one *one = to_max310x_port(port);

	schedule_work(&one->tx_work);
}

static irqreturn_t max310x_port_irq(struct max310x_port *s, int portno)
{
	struct uart_port *port = &s->p[portno].port;
	irqreturn_t res = IRQ_NONE;

	do {
		unsigned int ists, lsr, rxlen;

		 
		ists = max310x_port_read(port, MAX310X_IRQSTS_REG);
		rxlen = max310x_port_read(port, MAX310X_RXFIFOLVL_REG);
		if (!ists && !rxlen)
			break;

		res = IRQ_HANDLED;

		if (ists & MAX310X_IRQ_CTS_BIT) {
			lsr = max310x_port_read(port, MAX310X_LSR_IRQSTS_REG);
			uart_handle_cts_change(port, lsr & MAX310X_LSR_CTS_BIT);
		}
		if (rxlen)
			max310x_handle_rx(port, rxlen);
		if (ists & MAX310X_IRQ_TXEMPTY_BIT)
			max310x_start_tx(port);
	} while (1);
	return res;
}

static irqreturn_t max310x_ist(int irq, void *dev_id)
{
	struct max310x_port *s = (struct max310x_port *)dev_id;
	bool handled = false;

	if (s->devtype->nr > 1) {
		do {
			unsigned int val = ~0;

			WARN_ON_ONCE(regmap_read(s->regmap,
						 MAX310X_GLOBALIRQ_REG, &val));
			val = ((1 << s->devtype->nr) - 1) & ~val;
			if (!val)
				break;
			if (max310x_port_irq(s, fls(val) - 1) == IRQ_HANDLED)
				handled = true;
		} while (1);
	} else {
		if (max310x_port_irq(s, 0) == IRQ_HANDLED)
			handled = true;
	}

	return IRQ_RETVAL(handled);
}

static void max310x_tx_proc(struct work_struct *ws)
{
	struct max310x_one *one = container_of(ws, struct max310x_one, tx_work);

	max310x_handle_tx(&one->port);
}

static unsigned int max310x_tx_empty(struct uart_port *port)
{
	u8 lvl = max310x_port_read(port, MAX310X_TXFIFOLVL_REG);

	return lvl ? 0 : TIOCSER_TEMT;
}

static unsigned int max310x_get_mctrl(struct uart_port *port)
{
	 
	return TIOCM_DSR | TIOCM_CAR;
}

static void max310x_md_proc(struct work_struct *ws)
{
	struct max310x_one *one = container_of(ws, struct max310x_one, md_work);

	max310x_port_update(&one->port, MAX310X_MODE2_REG,
			    MAX310X_MODE2_LOOPBACK_BIT,
			    (one->port.mctrl & TIOCM_LOOP) ?
			    MAX310X_MODE2_LOOPBACK_BIT : 0);
}

static void max310x_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct max310x_one *one = to_max310x_port(port);

	schedule_work(&one->md_work);
}

static void max310x_break_ctl(struct uart_port *port, int break_state)
{
	max310x_port_update(port, MAX310X_LCR_REG,
			    MAX310X_LCR_TXBREAK_BIT,
			    break_state ? MAX310X_LCR_TXBREAK_BIT : 0);
}

static void max310x_set_termios(struct uart_port *port,
				struct ktermios *termios,
				const struct ktermios *old)
{
	unsigned int lcr = 0, flow = 0;
	int baud;

	 
	termios->c_cflag &= ~CMSPAR;

	 
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		break;
	case CS6:
		lcr = MAX310X_LCR_LENGTH0_BIT;
		break;
	case CS7:
		lcr = MAX310X_LCR_LENGTH1_BIT;
		break;
	case CS8:
	default:
		lcr = MAX310X_LCR_LENGTH1_BIT | MAX310X_LCR_LENGTH0_BIT;
		break;
	}

	 
	if (termios->c_cflag & PARENB) {
		lcr |= MAX310X_LCR_PARITY_BIT;
		if (!(termios->c_cflag & PARODD))
			lcr |= MAX310X_LCR_EVENPARITY_BIT;
	}

	 
	if (termios->c_cflag & CSTOPB)
		lcr |= MAX310X_LCR_STOPLEN_BIT;  

	 
	max310x_port_write(port, MAX310X_LCR_REG, lcr);

	 
	port->read_status_mask = MAX310X_LSR_RXOVR_BIT;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= MAX310X_LSR_RXPAR_BIT |
					  MAX310X_LSR_FRERR_BIT;
	if (termios->c_iflag & (IGNBRK | BRKINT | PARMRK))
		port->read_status_mask |= MAX310X_LSR_RXBRK_BIT;

	 
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNBRK)
		port->ignore_status_mask |= MAX310X_LSR_RXBRK_BIT;
	if (!(termios->c_cflag & CREAD))
		port->ignore_status_mask |= MAX310X_LSR_RXPAR_BIT |
					    MAX310X_LSR_RXOVR_BIT |
					    MAX310X_LSR_FRERR_BIT |
					    MAX310X_LSR_RXBRK_BIT;

	 
	max310x_port_write(port, MAX310X_XON1_REG, termios->c_cc[VSTART]);
	max310x_port_write(port, MAX310X_XOFF1_REG, termios->c_cc[VSTOP]);

	 
	if (termios->c_cflag & CRTSCTS || termios->c_iflag & IXOFF) {
		max310x_port_update(port, MAX310X_MODE1_REG,
				    MAX310X_MODE1_TXDIS_BIT,
				    MAX310X_MODE1_TXDIS_BIT);
	}

	port->status &= ~(UPSTAT_AUTOCTS | UPSTAT_AUTORTS | UPSTAT_AUTOXOFF);

	if (termios->c_cflag & CRTSCTS) {
		 
		port->status |= UPSTAT_AUTOCTS | UPSTAT_AUTORTS;
		flow |= MAX310X_FLOWCTRL_AUTOCTS_BIT |
			MAX310X_FLOWCTRL_AUTORTS_BIT;
	}
	if (termios->c_iflag & IXON)
		flow |= MAX310X_FLOWCTRL_SWFLOW3_BIT |
			MAX310X_FLOWCTRL_SWFLOWEN_BIT;
	if (termios->c_iflag & IXOFF) {
		port->status |= UPSTAT_AUTOXOFF;
		flow |= MAX310X_FLOWCTRL_SWFLOW1_BIT |
			MAX310X_FLOWCTRL_SWFLOWEN_BIT;
	}
	max310x_port_write(port, MAX310X_FLOWCTRL_REG, flow);

	 
	if (!(termios->c_cflag & CRTSCTS) && !(termios->c_iflag & IXOFF)) {
		max310x_port_update(port, MAX310X_MODE1_REG,
				    MAX310X_MODE1_TXDIS_BIT,
				    0);
	}

	 
	baud = uart_get_baud_rate(port, termios, old,
				  port->uartclk / 16 / 0xffff,
				  port->uartclk / 4);

	 
	baud = max310x_set_baud(port, baud);

	 
	uart_update_timeout(port, termios->c_cflag, baud);
}

static void max310x_rs_proc(struct work_struct *ws)
{
	struct max310x_one *one = container_of(ws, struct max310x_one, rs_work);
	unsigned int delay, mode1 = 0, mode2 = 0;

	delay = (one->port.rs485.delay_rts_before_send << 4) |
		one->port.rs485.delay_rts_after_send;
	max310x_port_write(&one->port, MAX310X_HDPIXDELAY_REG, delay);

	if (one->port.rs485.flags & SER_RS485_ENABLED) {
		mode1 = MAX310X_MODE1_TRNSCVCTRL_BIT;

		if (!(one->port.rs485.flags & SER_RS485_RX_DURING_TX))
			mode2 = MAX310X_MODE2_ECHOSUPR_BIT;
	}

	max310x_port_update(&one->port, MAX310X_MODE1_REG,
			MAX310X_MODE1_TRNSCVCTRL_BIT, mode1);
	max310x_port_update(&one->port, MAX310X_MODE2_REG,
			MAX310X_MODE2_ECHOSUPR_BIT, mode2);
}

static int max310x_rs485_config(struct uart_port *port, struct ktermios *termios,
				struct serial_rs485 *rs485)
{
	struct max310x_one *one = to_max310x_port(port);

	if ((rs485->delay_rts_before_send > 0x0f) ||
	    (rs485->delay_rts_after_send > 0x0f))
		return -ERANGE;

	port->rs485 = *rs485;

	schedule_work(&one->rs_work);

	return 0;
}

static int max310x_startup(struct uart_port *port)
{
	struct max310x_port *s = dev_get_drvdata(port->dev);
	unsigned int val;

	s->devtype->power(port, 1);

	 
	max310x_port_update(port, MAX310X_MODE1_REG,
			    MAX310X_MODE1_TRNSCVCTRL_BIT, 0);

	 
	val = MAX310X_MODE2_RXEMPTINV_BIT | MAX310X_MODE2_FIFORST_BIT;
	max310x_port_write(port, MAX310X_MODE2_REG, val);
	max310x_port_update(port, MAX310X_MODE2_REG,
			    MAX310X_MODE2_FIFORST_BIT, 0);

	 
	val = (clamp(port->rs485.delay_rts_before_send, 0U, 15U) << 4) |
		clamp(port->rs485.delay_rts_after_send, 0U, 15U);
	max310x_port_write(port, MAX310X_HDPIXDELAY_REG, val);

	if (port->rs485.flags & SER_RS485_ENABLED) {
		max310x_port_update(port, MAX310X_MODE1_REG,
				    MAX310X_MODE1_TRNSCVCTRL_BIT,
				    MAX310X_MODE1_TRNSCVCTRL_BIT);

		if (!(port->rs485.flags & SER_RS485_RX_DURING_TX))
			max310x_port_update(port, MAX310X_MODE2_REG,
					    MAX310X_MODE2_ECHOSUPR_BIT,
					    MAX310X_MODE2_ECHOSUPR_BIT);
	}

	 
	 
	max310x_port_write(port, MAX310X_FLOWLVL_REG,
			   MAX310X_FLOWLVL_RES(48) | MAX310X_FLOWLVL_HALT(96));

	 
	max310x_port_read(port, MAX310X_IRQSTS_REG);

	 
	val = MAX310X_IRQ_RXEMPTY_BIT | MAX310X_IRQ_TXEMPTY_BIT;
	max310x_port_write(port, MAX310X_IRQEN_REG, val | MAX310X_IRQ_CTS_BIT);

	return 0;
}

static void max310x_shutdown(struct uart_port *port)
{
	struct max310x_port *s = dev_get_drvdata(port->dev);

	 
	max310x_port_write(port, MAX310X_IRQEN_REG, 0);

	s->devtype->power(port, 0);
}

static const char *max310x_type(struct uart_port *port)
{
	struct max310x_port *s = dev_get_drvdata(port->dev);

	return (port->type == PORT_MAX310X) ? s->devtype->name : NULL;
}

static int max310x_request_port(struct uart_port *port)
{
	 
	return 0;
}

static void max310x_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_MAX310X;
}

static int max310x_verify_port(struct uart_port *port, struct serial_struct *s)
{
	if ((s->type != PORT_UNKNOWN) && (s->type != PORT_MAX310X))
		return -EINVAL;
	if (s->irq != port->irq)
		return -EINVAL;

	return 0;
}

static void max310x_null_void(struct uart_port *port)
{
	 
}

static const struct uart_ops max310x_ops = {
	.tx_empty	= max310x_tx_empty,
	.set_mctrl	= max310x_set_mctrl,
	.get_mctrl	= max310x_get_mctrl,
	.stop_tx	= max310x_null_void,
	.start_tx	= max310x_start_tx,
	.stop_rx	= max310x_null_void,
	.break_ctl	= max310x_break_ctl,
	.startup	= max310x_startup,
	.shutdown	= max310x_shutdown,
	.set_termios	= max310x_set_termios,
	.type		= max310x_type,
	.request_port	= max310x_request_port,
	.release_port	= max310x_null_void,
	.config_port	= max310x_config_port,
	.verify_port	= max310x_verify_port,
};

static int __maybe_unused max310x_suspend(struct device *dev)
{
	struct max310x_port *s = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < s->devtype->nr; i++) {
		uart_suspend_port(&max310x_uart, &s->p[i].port);
		s->devtype->power(&s->p[i].port, 0);
	}

	return 0;
}

static int __maybe_unused max310x_resume(struct device *dev)
{
	struct max310x_port *s = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < s->devtype->nr; i++) {
		s->devtype->power(&s->p[i].port, 1);
		uart_resume_port(&max310x_uart, &s->p[i].port);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(max310x_pm_ops, max310x_suspend, max310x_resume);

#ifdef CONFIG_GPIOLIB
static int max310x_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	unsigned int val;
	struct max310x_port *s = gpiochip_get_data(chip);
	struct uart_port *port = &s->p[offset / 4].port;

	val = max310x_port_read(port, MAX310X_GPIODATA_REG);

	return !!((val >> 4) & (1 << (offset % 4)));
}

static void max310x_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct max310x_port *s = gpiochip_get_data(chip);
	struct uart_port *port = &s->p[offset / 4].port;

	max310x_port_update(port, MAX310X_GPIODATA_REG, 1 << (offset % 4),
			    value ? 1 << (offset % 4) : 0);
}

static int max310x_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct max310x_port *s = gpiochip_get_data(chip);
	struct uart_port *port = &s->p[offset / 4].port;

	max310x_port_update(port, MAX310X_GPIOCFG_REG, 1 << (offset % 4), 0);

	return 0;
}

static int max310x_gpio_direction_output(struct gpio_chip *chip,
					 unsigned offset, int value)
{
	struct max310x_port *s = gpiochip_get_data(chip);
	struct uart_port *port = &s->p[offset / 4].port;

	max310x_port_update(port, MAX310X_GPIODATA_REG, 1 << (offset % 4),
			    value ? 1 << (offset % 4) : 0);
	max310x_port_update(port, MAX310X_GPIOCFG_REG, 1 << (offset % 4),
			    1 << (offset % 4));

	return 0;
}

static int max310x_gpio_set_config(struct gpio_chip *chip, unsigned int offset,
				   unsigned long config)
{
	struct max310x_port *s = gpiochip_get_data(chip);
	struct uart_port *port = &s->p[offset / 4].port;

	switch (pinconf_to_config_param(config)) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		max310x_port_update(port, MAX310X_GPIOCFG_REG,
				1 << ((offset % 4) + 4),
				1 << ((offset % 4) + 4));
		return 0;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		max310x_port_update(port, MAX310X_GPIOCFG_REG,
				1 << ((offset % 4) + 4), 0);
		return 0;
	default:
		return -ENOTSUPP;
	}
}
#endif

static const struct serial_rs485 max310x_rs485_supported = {
	.flags = SER_RS485_ENABLED | SER_RS485_RTS_ON_SEND | SER_RS485_RX_DURING_TX,
	.delay_rts_before_send = 1,
	.delay_rts_after_send = 1,
};

static int max310x_probe(struct device *dev, const struct max310x_devtype *devtype,
			 const struct max310x_if_cfg *if_cfg,
			 struct regmap *regmaps[], int irq)
{
	int i, ret, fmin, fmax, freq;
	struct max310x_port *s;
	u32 uartclk = 0;
	bool xtal;

	for (i = 0; i < devtype->nr; i++)
		if (IS_ERR(regmaps[i]))
			return PTR_ERR(regmaps[i]);

	 
	s = devm_kzalloc(dev, struct_size(s, p, devtype->nr), GFP_KERNEL);
	if (!s) {
		dev_err(dev, "Error allocating port structure\n");
		return -ENOMEM;
	}

	 
	device_property_read_u32(dev, "clock-frequency", &uartclk);

	xtal = device_property_match_string(dev, "clock-names", "osc") < 0;
	if (xtal)
		s->clk = devm_clk_get_optional(dev, "xtal");
	else
		s->clk = devm_clk_get_optional(dev, "osc");
	if (IS_ERR(s->clk))
		return PTR_ERR(s->clk);

	ret = clk_prepare_enable(s->clk);
	if (ret)
		return ret;

	freq = clk_get_rate(s->clk);
	if (freq == 0)
		freq = uartclk;
	if (freq == 0) {
		dev_err(dev, "Cannot get clock rate\n");
		ret = -EINVAL;
		goto out_clk;
	}

	if (xtal) {
		fmin = 1000000;
		fmax = 4000000;
	} else {
		fmin = 500000;
		fmax = 35000000;
	}

	 
	if (freq < fmin || freq > fmax) {
		ret = -ERANGE;
		goto out_clk;
	}

	s->regmap = regmaps[0];
	s->devtype = devtype;
	s->if_cfg = if_cfg;
	dev_set_drvdata(dev, s);

	 
	ret = devtype->detect(dev);
	if (ret)
		goto out_clk;

	for (i = 0; i < devtype->nr; i++) {
		 
		regmap_write(regmaps[i], MAX310X_MODE2_REG,
			     MAX310X_MODE2_RST_BIT);
		 
		regmap_write(regmaps[i], MAX310X_MODE2_REG, 0);

		 
		do {
			regmap_read(regmaps[i], MAX310X_BRGDIVLSB_REG, &ret);
		} while (ret != 0x01);

		regmap_write(regmaps[i], MAX310X_MODE1_REG, devtype->mode1);
	}

	uartclk = max310x_set_ref_clk(dev, s, freq, xtal);
	dev_dbg(dev, "Reference clock set to %i Hz\n", uartclk);

	for (i = 0; i < devtype->nr; i++) {
		unsigned int line;

		line = find_first_zero_bit(max310x_lines, MAX310X_UART_NRMAX);
		if (line == MAX310X_UART_NRMAX) {
			ret = -ERANGE;
			goto out_uart;
		}

		 
		s->p[i].port.line	= line;
		s->p[i].port.dev	= dev;
		s->p[i].port.irq	= irq;
		s->p[i].port.type	= PORT_MAX310X;
		s->p[i].port.fifosize	= MAX310X_FIFO_SIZE;
		s->p[i].port.flags	= UPF_FIXED_TYPE | UPF_LOW_LATENCY;
		s->p[i].port.iotype	= UPIO_PORT;
		s->p[i].port.iobase	= i;
		 
		s->p[i].port.membase	= (void __iomem *)~0;
		s->p[i].port.uartclk	= uartclk;
		s->p[i].port.rs485_config = max310x_rs485_config;
		s->p[i].port.rs485_supported = max310x_rs485_supported;
		s->p[i].port.ops	= &max310x_ops;
		s->p[i].regmap		= regmaps[i];

		 
		max310x_port_write(&s->p[i].port, MAX310X_IRQEN_REG, 0);
		 
		max310x_port_read(&s->p[i].port, MAX310X_IRQSTS_REG);
		 
		INIT_WORK(&s->p[i].tx_work, max310x_tx_proc);
		 
		INIT_WORK(&s->p[i].md_work, max310x_md_proc);
		 
		INIT_WORK(&s->p[i].rs_work, max310x_rs_proc);

		 
		ret = uart_add_one_port(&max310x_uart, &s->p[i].port);
		if (ret) {
			s->p[i].port.dev = NULL;
			goto out_uart;
		}
		set_bit(line, max310x_lines);

		 
		devtype->power(&s->p[i].port, 0);
	}

#ifdef CONFIG_GPIOLIB
	 
	s->gpio.owner		= THIS_MODULE;
	s->gpio.parent		= dev;
	s->gpio.label		= devtype->name;
	s->gpio.direction_input	= max310x_gpio_direction_input;
	s->gpio.get		= max310x_gpio_get;
	s->gpio.direction_output= max310x_gpio_direction_output;
	s->gpio.set		= max310x_gpio_set;
	s->gpio.set_config	= max310x_gpio_set_config;
	s->gpio.base		= -1;
	s->gpio.ngpio		= devtype->nr * 4;
	s->gpio.can_sleep	= 1;
	ret = devm_gpiochip_add_data(dev, &s->gpio, s);
	if (ret)
		goto out_uart;
#endif

	 
	ret = devm_request_threaded_irq(dev, irq, NULL, max310x_ist,
					IRQF_ONESHOT | IRQF_SHARED, dev_name(dev), s);
	if (!ret)
		return 0;

	dev_err(dev, "Unable to reguest IRQ %i\n", irq);

out_uart:
	for (i = 0; i < devtype->nr; i++) {
		if (s->p[i].port.dev) {
			uart_remove_one_port(&max310x_uart, &s->p[i].port);
			clear_bit(s->p[i].port.line, max310x_lines);
		}
	}

out_clk:
	clk_disable_unprepare(s->clk);

	return ret;
}

static void max310x_remove(struct device *dev)
{
	struct max310x_port *s = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < s->devtype->nr; i++) {
		cancel_work_sync(&s->p[i].tx_work);
		cancel_work_sync(&s->p[i].md_work);
		cancel_work_sync(&s->p[i].rs_work);
		uart_remove_one_port(&max310x_uart, &s->p[i].port);
		clear_bit(s->p[i].port.line, max310x_lines);
		s->devtype->power(&s->p[i].port, 0);
	}

	clk_disable_unprepare(s->clk);
}

static const struct of_device_id __maybe_unused max310x_dt_ids[] = {
	{ .compatible = "maxim,max3107",	.data = &max3107_devtype, },
	{ .compatible = "maxim,max3108",	.data = &max3108_devtype, },
	{ .compatible = "maxim,max3109",	.data = &max3109_devtype, },
	{ .compatible = "maxim,max14830",	.data = &max14830_devtype },
	{ }
};
MODULE_DEVICE_TABLE(of, max310x_dt_ids);

static struct regmap_config regcfg = {
	.reg_bits = 8,
	.val_bits = 8,
	.write_flag_mask = MAX310X_WRITE_BIT,
	.cache_type = REGCACHE_RBTREE,
	.max_register = MAX310X_REG_1F,
	.writeable_reg = max310x_reg_writeable,
	.volatile_reg = max310x_reg_volatile,
	.precious_reg = max310x_reg_precious,
	.writeable_noinc_reg = max310x_reg_noinc,
	.readable_noinc_reg = max310x_reg_noinc,
	.max_raw_read = MAX310X_FIFO_SIZE,
	.max_raw_write = MAX310X_FIFO_SIZE,
};

#ifdef CONFIG_SPI_MASTER
static int max310x_spi_extended_reg_enable(struct device *dev, bool enable)
{
	struct max310x_port *s = dev_get_drvdata(dev);

	return regmap_write(s->regmap, MAX310X_GLOBALCMD_REG,
			    enable ? MAX310X_EXTREG_ENBL : MAX310X_EXTREG_DSBL);
}

static const struct max310x_if_cfg __maybe_unused max310x_spi_if_cfg = {
	.extended_reg_enable = max310x_spi_extended_reg_enable,
	.rev_id_reg = MAX310X_SPI_REVID_EXTREG,
};

static int max310x_spi_probe(struct spi_device *spi)
{
	const struct max310x_devtype *devtype;
	struct regmap *regmaps[4];
	unsigned int i;
	int ret;

	 
	spi->bits_per_word	= 8;
	spi->mode		= spi->mode ? : SPI_MODE_0;
	spi->max_speed_hz	= spi->max_speed_hz ? : 26000000;
	ret = spi_setup(spi);
	if (ret)
		return ret;

	devtype = device_get_match_data(&spi->dev);
	if (!devtype)
		devtype = (struct max310x_devtype *)spi_get_device_id(spi)->driver_data;

	for (i = 0; i < devtype->nr; i++) {
		u8 port_mask = i * 0x20;
		regcfg.read_flag_mask = port_mask;
		regcfg.write_flag_mask = port_mask | MAX310X_WRITE_BIT;
		regmaps[i] = devm_regmap_init_spi(spi, &regcfg);
	}

	return max310x_probe(&spi->dev, devtype, &max310x_spi_if_cfg, regmaps, spi->irq);
}

static void max310x_spi_remove(struct spi_device *spi)
{
	max310x_remove(&spi->dev);
}

static const struct spi_device_id max310x_id_table[] = {
	{ "max3107",	(kernel_ulong_t)&max3107_devtype, },
	{ "max3108",	(kernel_ulong_t)&max3108_devtype, },
	{ "max3109",	(kernel_ulong_t)&max3109_devtype, },
	{ "max14830",	(kernel_ulong_t)&max14830_devtype, },
	{ }
};
MODULE_DEVICE_TABLE(spi, max310x_id_table);

static struct spi_driver max310x_spi_driver = {
	.driver = {
		.name		= MAX310X_NAME,
		.of_match_table	= max310x_dt_ids,
		.pm		= &max310x_pm_ops,
	},
	.probe		= max310x_spi_probe,
	.remove		= max310x_spi_remove,
	.id_table	= max310x_id_table,
};
#endif

#ifdef CONFIG_I2C
static int max310x_i2c_extended_reg_enable(struct device *dev, bool enable)
{
	return 0;
}

static struct regmap_config regcfg_i2c = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.writeable_reg = max310x_reg_writeable,
	.volatile_reg = max310x_reg_volatile,
	.precious_reg = max310x_reg_precious,
	.max_register = MAX310X_I2C_REVID_EXTREG,
	.writeable_noinc_reg = max310x_reg_noinc,
	.readable_noinc_reg = max310x_reg_noinc,
	.max_raw_read = MAX310X_FIFO_SIZE,
	.max_raw_write = MAX310X_FIFO_SIZE,
};

static const struct max310x_if_cfg max310x_i2c_if_cfg = {
	.extended_reg_enable = max310x_i2c_extended_reg_enable,
	.rev_id_reg = MAX310X_I2C_REVID_EXTREG,
};

static unsigned short max310x_i2c_slave_addr(unsigned short addr,
					     unsigned int nr)
{
	 

	addr -= nr * 0x10;

	if (nr >= 2)
		addr -= 0x20;

	return addr;
}

static int max310x_i2c_probe(struct i2c_client *client)
{
	const struct max310x_devtype *devtype =
			device_get_match_data(&client->dev);
	struct i2c_client *port_client;
	struct regmap *regmaps[4];
	unsigned int i;
	u8 port_addr;

	if (client->addr < devtype->slave_addr.min ||
		client->addr > devtype->slave_addr.max)
		return dev_err_probe(&client->dev, -EINVAL,
				     "Slave addr 0x%x outside of range [0x%x, 0x%x]\n",
				     client->addr, devtype->slave_addr.min,
				     devtype->slave_addr.max);

	regmaps[0] = devm_regmap_init_i2c(client, &regcfg_i2c);

	for (i = 1; i < devtype->nr; i++) {
		port_addr = max310x_i2c_slave_addr(client->addr, i);
		port_client = devm_i2c_new_dummy_device(&client->dev,
							client->adapter,
							port_addr);

		regmaps[i] = devm_regmap_init_i2c(port_client, &regcfg_i2c);
	}

	return max310x_probe(&client->dev, devtype, &max310x_i2c_if_cfg,
			     regmaps, client->irq);
}

static void max310x_i2c_remove(struct i2c_client *client)
{
	max310x_remove(&client->dev);
}

static struct i2c_driver max310x_i2c_driver = {
	.driver = {
		.name		= MAX310X_NAME,
		.of_match_table	= max310x_dt_ids,
		.pm		= &max310x_pm_ops,
	},
	.probe		= max310x_i2c_probe,
	.remove		= max310x_i2c_remove,
};
#endif

static int __init max310x_uart_init(void)
{
	int ret;

	bitmap_zero(max310x_lines, MAX310X_UART_NRMAX);

	ret = uart_register_driver(&max310x_uart);
	if (ret)
		return ret;

#ifdef CONFIG_SPI_MASTER
	ret = spi_register_driver(&max310x_spi_driver);
	if (ret)
		goto err_spi_register;
#endif

#ifdef CONFIG_I2C
	ret = i2c_add_driver(&max310x_i2c_driver);
	if (ret)
		goto err_i2c_register;
#endif

	return 0;

#ifdef CONFIG_I2C
err_i2c_register:
	spi_unregister_driver(&max310x_spi_driver);
#endif

err_spi_register:
	uart_unregister_driver(&max310x_uart);

	return ret;
}
module_init(max310x_uart_init);

static void __exit max310x_uart_exit(void)
{
#ifdef CONFIG_I2C
	i2c_del_driver(&max310x_i2c_driver);
#endif

#ifdef CONFIG_SPI_MASTER
	spi_unregister_driver(&max310x_spi_driver);
#endif

	uart_unregister_driver(&max310x_uart);
}
module_exit(max310x_uart_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Shiyan <shc_work@mail.ru>");
MODULE_DESCRIPTION("MAX310X serial driver");
