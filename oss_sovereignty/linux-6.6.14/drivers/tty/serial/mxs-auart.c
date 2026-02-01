
 

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>

#include <linux/gpio/consumer.h>
#include <linux/err.h>
#include <linux/irq.h>
#include "serial_mctrl_gpio.h"

#define MXS_AUART_PORTS 5
#define MXS_AUART_FIFO_SIZE		16

#define SET_REG				0x4
#define CLR_REG				0x8
#define TOG_REG				0xc

#define AUART_CTRL0			0x00000000
#define AUART_CTRL1			0x00000010
#define AUART_CTRL2			0x00000020
#define AUART_LINECTRL			0x00000030
#define AUART_LINECTRL2			0x00000040
#define AUART_INTR			0x00000050
#define AUART_DATA			0x00000060
#define AUART_STAT			0x00000070
#define AUART_DEBUG			0x00000080
#define AUART_VERSION			0x00000090
#define AUART_AUTOBAUD			0x000000a0

#define AUART_CTRL0_SFTRST			(1 << 31)
#define AUART_CTRL0_CLKGATE			(1 << 30)
#define AUART_CTRL0_RXTO_ENABLE			(1 << 27)
#define AUART_CTRL0_RXTIMEOUT(v)		(((v) & 0x7ff) << 16)
#define AUART_CTRL0_XFER_COUNT(v)		((v) & 0xffff)

#define AUART_CTRL1_XFER_COUNT(v)		((v) & 0xffff)

#define AUART_CTRL2_DMAONERR			(1 << 26)
#define AUART_CTRL2_TXDMAE			(1 << 25)
#define AUART_CTRL2_RXDMAE			(1 << 24)

#define AUART_CTRL2_CTSEN			(1 << 15)
#define AUART_CTRL2_RTSEN			(1 << 14)
#define AUART_CTRL2_RTS				(1 << 11)
#define AUART_CTRL2_RXE				(1 << 9)
#define AUART_CTRL2_TXE				(1 << 8)
#define AUART_CTRL2_UARTEN			(1 << 0)

#define AUART_LINECTRL_BAUD_DIV_MAX		0x003fffc0
#define AUART_LINECTRL_BAUD_DIV_MIN		0x000000ec
#define AUART_LINECTRL_BAUD_DIVINT_SHIFT	16
#define AUART_LINECTRL_BAUD_DIVINT_MASK		0xffff0000
#define AUART_LINECTRL_BAUD_DIVINT(v)		(((v) & 0xffff) << 16)
#define AUART_LINECTRL_BAUD_DIVFRAC_SHIFT	8
#define AUART_LINECTRL_BAUD_DIVFRAC_MASK	0x00003f00
#define AUART_LINECTRL_BAUD_DIVFRAC(v)		(((v) & 0x3f) << 8)
#define AUART_LINECTRL_SPS			(1 << 7)
#define AUART_LINECTRL_WLEN_MASK		0x00000060
#define AUART_LINECTRL_WLEN(v)			((((v) - 5) & 0x3) << 5)
#define AUART_LINECTRL_FEN			(1 << 4)
#define AUART_LINECTRL_STP2			(1 << 3)
#define AUART_LINECTRL_EPS			(1 << 2)
#define AUART_LINECTRL_PEN			(1 << 1)
#define AUART_LINECTRL_BRK			(1 << 0)

#define AUART_INTR_RTIEN			(1 << 22)
#define AUART_INTR_TXIEN			(1 << 21)
#define AUART_INTR_RXIEN			(1 << 20)
#define AUART_INTR_CTSMIEN			(1 << 17)
#define AUART_INTR_RTIS				(1 << 6)
#define AUART_INTR_TXIS				(1 << 5)
#define AUART_INTR_RXIS				(1 << 4)
#define AUART_INTR_CTSMIS			(1 << 1)

#define AUART_STAT_BUSY				(1 << 29)
#define AUART_STAT_CTS				(1 << 28)
#define AUART_STAT_TXFE				(1 << 27)
#define AUART_STAT_TXFF				(1 << 25)
#define AUART_STAT_RXFE				(1 << 24)
#define AUART_STAT_OERR				(1 << 19)
#define AUART_STAT_BERR				(1 << 18)
#define AUART_STAT_PERR				(1 << 17)
#define AUART_STAT_FERR				(1 << 16)
#define AUART_STAT_RXCOUNT_MASK			0xffff

 
#define ASM9260_HW_CTRL0			0x0000
 
#define ASM9260_BM_CTRL0_RXDMA_RUN		BIT(28)
 
#define ASM9260_BM_CTRL0_RXTO_SOURCE_STATUS	BIT(25)
 
#define ASM9260_BM_CTRL0_RXTO_ENABLE		BIT(24)
 
#define ASM9260_BM_CTRL0_RXTO_MASK		(0xff << 16)
 
#define ASM9260_BM_CTRL0_DEFAULT_RXTIMEOUT	(20 << 16)

 
#define ASM9260_HW_CTRL1			0x0010
 
#define ASM9260_BM_CTRL1_TXDMA_RUN		BIT(28)

#define ASM9260_HW_CTRL2			0x0020
 
#define ASM9260_BM_CTRL2_RXIFLSEL		(7 << 20)
#define ASM9260_BM_CTRL2_DEFAULT_RXIFLSEL	(3 << 20)
 
#define ASM9260_BM_CTRL2_TXIFLSEL		(7 << 16)
#define ASM9260_BM_CTRL2_DEFAULT_TXIFLSEL	(2 << 16)
 
#define ASM9260_BM_CTRL2_DTR			BIT(10)
 
#define ASM9260_BM_CTRL2_LBE			BIT(7)
#define ASM9260_BM_CTRL2_PORT_ENABLE		BIT(0)

#define ASM9260_HW_LINECTRL			0x0030
 
#define ASM9260_BM_LCTRL_SPS			BIT(7)
 
#define ASM9260_BM_LCTRL_WLEN			(3 << 5)
#define ASM9260_BM_LCTRL_CHRL_5			(0 << 5)
#define ASM9260_BM_LCTRL_CHRL_6			(1 << 5)
#define ASM9260_BM_LCTRL_CHRL_7			(2 << 5)
#define ASM9260_BM_LCTRL_CHRL_8			(3 << 5)

 
#define ASM9260_HW_INTR				0x0040
 
#define ASM9260_BM_INTR_TFEIEN			BIT(27)
 
#define ASM9260_BM_INTR_OEIEN			BIT(26)
 
#define ASM9260_BM_INTR_BEIEN			BIT(25)
 
#define ASM9260_BM_INTR_PEIEN			BIT(24)
 
#define ASM9260_BM_INTR_FEIEN			BIT(23)

 
#define ASM9260_BM_INTR_DSRMIEN			BIT(19)
 
#define ASM9260_BM_INTR_DCDMIEN			BIT(18)
 
#define ASM9260_BM_INTR_RIMIEN			BIT(16)
 
#define ASM9260_BM_INTR_ABTO			BIT(13)
#define ASM9260_BM_INTR_ABEO			BIT(12)
 
#define ASM9260_BM_INTR_TFEIS			BIT(11)
 
#define ASM9260_BM_INTR_OEIS			BIT(10)
 
#define ASM9260_BM_INTR_BEIS			BIT(9)
 
#define ASM9260_BM_INTR_PEIS			BIT(8)
 
#define ASM9260_BM_INTR_FEIS			BIT(7)
#define ASM9260_BM_INTR_DSRMIS			BIT(3)
#define ASM9260_BM_INTR_DCDMIS			BIT(2)
#define ASM9260_BM_INTR_RIMIS			BIT(0)

 
#define ASM9260_HW_DATA				0x0050

#define ASM9260_HW_STAT				0x0060
 
#define ASM9260_BM_STAT_PRESENT			BIT(31)
 
#define ASM9260_BM_STAT_HISPEED			BIT(30)
 
#define ASM9260_BM_STAT_RXFULL			BIT(26)

 
#define ASM9260_HW_DEBUG			0x0070
 
#define ASM9260_BM_DEBUG_TXDMARUN		BIT(5)
#define ASM9260_BM_DEBUG_RXDMARUN		BIT(4)
 
#define ASM9260_BM_DEBUG_TXCMDEND		BIT(3)
#define ASM9260_BM_DEBUG_RXCMDEND		BIT(2)
 
#define ASM9260_BM_DEBUG_TXDMARQ		BIT(1)
#define ASM9260_BM_DEBUG_RXDMARQ		BIT(0)

#define ASM9260_HW_ILPR				0x0080

#define ASM9260_HW_RS485CTRL			0x0090
 
#define ASM9260_BM_RS485CTRL_ONIV		BIT(5)
 
#define ASM9260_BM_RS485CTRL_DIR_CTRL		BIT(4)
 
#define ASM9260_BM_RS485CTRL_PINSEL		BIT(3)
 
#define ASM9260_BM_RS485CTRL_AADEN		BIT(2)
 
#define ASM9260_BM_RS485CTRL_RXDIS		BIT(1)
 
#define ASM9260_BM_RS485CTRL_RS485EN		BIT(0)

#define ASM9260_HW_RS485ADRMATCH		0x00a0
 
#define ASM9260_BM_RS485ADRMATCH_MASK		(0xff << 0)

#define ASM9260_HW_RS485DLY			0x00b0
 
#define ASM9260_BM_RS485DLY_MASK		(0xff << 0)

#define ASM9260_HW_AUTOBAUD			0x00c0
 
#define ASM9260_BM_AUTOBAUD_TO_INT_CLR		BIT(9)
 
#define ASM9260_BM_AUTOBAUD_EO_INT_CLR		BIT(8)
 
#define ASM9260_BM_AUTOBAUD_AUTORESTART		BIT(2)
 
#define ASM9260_BM_AUTOBAUD_MODE		BIT(1)
 
#define ASM9260_BM_AUTOBAUD_START		BIT(0)

#define ASM9260_HW_CTRL3			0x00d0
#define ASM9260_BM_CTRL3_OUTCLK_DIV_MASK	(0xffff << 16)
 
#define ASM9260_BM_CTRL3_MASTERMODE		BIT(6)
 
#define ASM9260_BM_CTRL3_SYNCMODE		BIT(4)
 
#define ASM9260_BM_CTRL3_MSBF			BIT(2)
 
#define ASM9260_BM_CTRL3_BAUD8			BIT(1)
 
#define ASM9260_BM_CTRL3_9BIT			BIT(0)

#define ASM9260_HW_ISO7816_CTRL			0x00e0
 
#define ASM9260_BM_ISO7816CTRL_HS		BIT(12)
 
#define ASM9260_BM_ISO7816CTRL_DS_NACK		BIT(8)
#define ASM9260_BM_ISO7816CTRL_MAX_ITER_MASK	(0xff << 4)
 
#define ASM9260_BM_ISO7816CTRL_INACK		BIT(3)
#define ASM9260_BM_ISO7816CTRL_NEG_DATA		BIT(2)
 
#define ASM9260_BM_ISO7816CTRL_ENABLE		BIT(0)

#define ASM9260_HW_ISO7816_ERRCNT		0x00f0
 
#define ASM9260_BM_ISO7816_NB_ERRORS_MASK	(0xff << 0)

#define ASM9260_HW_ISO7816_STATUS		0x0100
 
#define ASM9260_BM_ISO7816_STAT_ITERATION	BIT(0)

 

static struct uart_driver auart_driver;

enum mxs_auart_type {
	IMX23_AUART,
	IMX28_AUART,
	ASM9260_AUART,
};

struct vendor_data {
	const u16	*reg_offset;
};

enum {
	REG_CTRL0,
	REG_CTRL1,
	REG_CTRL2,
	REG_LINECTRL,
	REG_LINECTRL2,
	REG_INTR,
	REG_DATA,
	REG_STAT,
	REG_DEBUG,
	REG_VERSION,
	REG_AUTOBAUD,

	 
	REG_ARRAY_SIZE,
};

static const u16 mxs_asm9260_offsets[REG_ARRAY_SIZE] = {
	[REG_CTRL0] = ASM9260_HW_CTRL0,
	[REG_CTRL1] = ASM9260_HW_CTRL1,
	[REG_CTRL2] = ASM9260_HW_CTRL2,
	[REG_LINECTRL] = ASM9260_HW_LINECTRL,
	[REG_INTR] = ASM9260_HW_INTR,
	[REG_DATA] = ASM9260_HW_DATA,
	[REG_STAT] = ASM9260_HW_STAT,
	[REG_DEBUG] = ASM9260_HW_DEBUG,
	[REG_AUTOBAUD] = ASM9260_HW_AUTOBAUD,
};

static const u16 mxs_stmp37xx_offsets[REG_ARRAY_SIZE] = {
	[REG_CTRL0] = AUART_CTRL0,
	[REG_CTRL1] = AUART_CTRL1,
	[REG_CTRL2] = AUART_CTRL2,
	[REG_LINECTRL] = AUART_LINECTRL,
	[REG_LINECTRL2] = AUART_LINECTRL2,
	[REG_INTR] = AUART_INTR,
	[REG_DATA] = AUART_DATA,
	[REG_STAT] = AUART_STAT,
	[REG_DEBUG] = AUART_DEBUG,
	[REG_VERSION] = AUART_VERSION,
	[REG_AUTOBAUD] = AUART_AUTOBAUD,
};

static const struct vendor_data vendor_alphascale_asm9260 = {
	.reg_offset = mxs_asm9260_offsets,
};

static const struct vendor_data vendor_freescale_stmp37xx = {
	.reg_offset = mxs_stmp37xx_offsets,
};

struct mxs_auart_port {
	struct uart_port port;

#define MXS_AUART_DMA_ENABLED	0x2
#define MXS_AUART_DMA_TX_SYNC	2   
#define MXS_AUART_DMA_RX_READY	3   
#define MXS_AUART_RTSCTS	4   
	unsigned long flags;
	unsigned int mctrl_prev;
	enum mxs_auart_type devtype;
	const struct vendor_data *vendor;

	struct clk *clk;
	struct clk *clk_ahb;
	struct device *dev;

	 
	struct scatterlist tx_sgl;
	struct dma_chan	*tx_dma_chan;
	void *tx_dma_buf;

	struct scatterlist rx_sgl;
	struct dma_chan	*rx_dma_chan;
	void *rx_dma_buf;

	struct mctrl_gpios	*gpios;
	int			gpio_irq[UART_GPIO_MAX];
	bool			ms_irq_enabled;
};

static const struct of_device_id mxs_auart_dt_ids[] = {
	{
		.compatible = "fsl,imx28-auart",
		.data = (const void *)IMX28_AUART
	}, {
		.compatible = "fsl,imx23-auart",
		.data = (const void *)IMX23_AUART
	}, {
		.compatible = "alphascale,asm9260-auart",
		.data = (const void *)ASM9260_AUART
	}, {   }
};
MODULE_DEVICE_TABLE(of, mxs_auart_dt_ids);

static inline int is_imx28_auart(struct mxs_auart_port *s)
{
	return s->devtype == IMX28_AUART;
}

static inline int is_asm9260_auart(struct mxs_auart_port *s)
{
	return s->devtype == ASM9260_AUART;
}

static inline bool auart_dma_enabled(struct mxs_auart_port *s)
{
	return s->flags & MXS_AUART_DMA_ENABLED;
}

static unsigned int mxs_reg_to_offset(const struct mxs_auart_port *uap,
				      unsigned int reg)
{
	return uap->vendor->reg_offset[reg];
}

static unsigned int mxs_read(const struct mxs_auart_port *uap,
			     unsigned int reg)
{
	void __iomem *addr = uap->port.membase + mxs_reg_to_offset(uap, reg);

	return readl_relaxed(addr);
}

static void mxs_write(unsigned int val, struct mxs_auart_port *uap,
		      unsigned int reg)
{
	void __iomem *addr = uap->port.membase + mxs_reg_to_offset(uap, reg);

	writel_relaxed(val, addr);
}

static void mxs_set(unsigned int val, struct mxs_auart_port *uap,
		    unsigned int reg)
{
	void __iomem *addr = uap->port.membase + mxs_reg_to_offset(uap, reg);

	writel_relaxed(val, addr + SET_REG);
}

static void mxs_clr(unsigned int val, struct mxs_auart_port *uap,
		    unsigned int reg)
{
	void __iomem *addr = uap->port.membase + mxs_reg_to_offset(uap, reg);

	writel_relaxed(val, addr + CLR_REG);
}

static void mxs_auart_stop_tx(struct uart_port *u);

#define to_auart_port(u) container_of(u, struct mxs_auart_port, port)

static void mxs_auart_tx_chars(struct mxs_auart_port *s);

static void dma_tx_callback(void *param)
{
	struct mxs_auart_port *s = param;
	struct circ_buf *xmit = &s->port.state->xmit;

	dma_unmap_sg(s->dev, &s->tx_sgl, 1, DMA_TO_DEVICE);

	 
	clear_bit(MXS_AUART_DMA_TX_SYNC, &s->flags);
	smp_mb__after_atomic();

	 
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&s->port);

	mxs_auart_tx_chars(s);
}

static int mxs_auart_dma_tx(struct mxs_auart_port *s, int size)
{
	struct dma_async_tx_descriptor *desc;
	struct scatterlist *sgl = &s->tx_sgl;
	struct dma_chan *channel = s->tx_dma_chan;
	u32 pio;

	 
	pio = AUART_CTRL1_XFER_COUNT(size);
	desc = dmaengine_prep_slave_sg(channel, (struct scatterlist *)&pio,
					1, DMA_TRANS_NONE, 0);
	if (!desc) {
		dev_err(s->dev, "step 1 error\n");
		return -EINVAL;
	}

	 
	sg_init_one(sgl, s->tx_dma_buf, size);
	dma_map_sg(s->dev, sgl, 1, DMA_TO_DEVICE);
	desc = dmaengine_prep_slave_sg(channel, sgl,
			1, DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(s->dev, "step 2 error\n");
		return -EINVAL;
	}

	 
	desc->callback = dma_tx_callback;
	desc->callback_param = s;
	dmaengine_submit(desc);
	dma_async_issue_pending(channel);
	return 0;
}

static void mxs_auart_tx_chars(struct mxs_auart_port *s)
{
	struct circ_buf *xmit = &s->port.state->xmit;
	bool pending;
	u8 ch;

	if (auart_dma_enabled(s)) {
		u32 i = 0;
		int size;
		void *buffer = s->tx_dma_buf;

		if (test_and_set_bit(MXS_AUART_DMA_TX_SYNC, &s->flags))
			return;

		while (!uart_circ_empty(xmit) && !uart_tx_stopped(&s->port)) {
			size = min_t(u32, UART_XMIT_SIZE - i,
				     CIRC_CNT_TO_END(xmit->head,
						     xmit->tail,
						     UART_XMIT_SIZE));
			memcpy(buffer + i, xmit->buf + xmit->tail, size);
			xmit->tail = (xmit->tail + size) & (UART_XMIT_SIZE - 1);

			i += size;
			if (i >= UART_XMIT_SIZE)
				break;
		}

		if (uart_tx_stopped(&s->port))
			mxs_auart_stop_tx(&s->port);

		if (i) {
			mxs_auart_dma_tx(s, i);
		} else {
			clear_bit(MXS_AUART_DMA_TX_SYNC, &s->flags);
			smp_mb__after_atomic();
		}
		return;
	}

	pending = uart_port_tx(&s->port, ch,
		!(mxs_read(s, REG_STAT) & AUART_STAT_TXFF),
		mxs_write(ch, s, REG_DATA));
	if (pending)
		mxs_set(AUART_INTR_TXIEN, s, REG_INTR);
	else
		mxs_clr(AUART_INTR_TXIEN, s, REG_INTR);
}

static void mxs_auart_rx_char(struct mxs_auart_port *s)
{
	u32 stat;
	u8 c, flag;

	c = mxs_read(s, REG_DATA);
	stat = mxs_read(s, REG_STAT);

	flag = TTY_NORMAL;
	s->port.icount.rx++;

	if (stat & AUART_STAT_BERR) {
		s->port.icount.brk++;
		if (uart_handle_break(&s->port))
			goto out;
	} else if (stat & AUART_STAT_PERR) {
		s->port.icount.parity++;
	} else if (stat & AUART_STAT_FERR) {
		s->port.icount.frame++;
	}

	 
	stat &= s->port.read_status_mask;

	if (stat & AUART_STAT_BERR) {
		flag = TTY_BREAK;
	} else if (stat & AUART_STAT_PERR)
		flag = TTY_PARITY;
	else if (stat & AUART_STAT_FERR)
		flag = TTY_FRAME;

	if (stat & AUART_STAT_OERR)
		s->port.icount.overrun++;

	if (uart_handle_sysrq_char(&s->port, c))
		goto out;

	uart_insert_char(&s->port, stat, AUART_STAT_OERR, c, flag);
out:
	mxs_write(stat, s, REG_STAT);
}

static void mxs_auart_rx_chars(struct mxs_auart_port *s)
{
	u32 stat = 0;

	for (;;) {
		stat = mxs_read(s, REG_STAT);
		if (stat & AUART_STAT_RXFE)
			break;
		mxs_auart_rx_char(s);
	}

	mxs_write(stat, s, REG_STAT);
	tty_flip_buffer_push(&s->port.state->port);
}

static int mxs_auart_request_port(struct uart_port *u)
{
	return 0;
}

static int mxs_auart_verify_port(struct uart_port *u,
				    struct serial_struct *ser)
{
	if (u->type != PORT_UNKNOWN && u->type != PORT_IMX)
		return -EINVAL;
	return 0;
}

static void mxs_auart_config_port(struct uart_port *u, int flags)
{
}

static const char *mxs_auart_type(struct uart_port *u)
{
	struct mxs_auart_port *s = to_auart_port(u);

	return dev_name(s->dev);
}

static void mxs_auart_release_port(struct uart_port *u)
{
}

static void mxs_auart_set_mctrl(struct uart_port *u, unsigned mctrl)
{
	struct mxs_auart_port *s = to_auart_port(u);

	u32 ctrl = mxs_read(s, REG_CTRL2);

	ctrl &= ~(AUART_CTRL2_RTSEN | AUART_CTRL2_RTS);
	if (mctrl & TIOCM_RTS) {
		if (uart_cts_enabled(u))
			ctrl |= AUART_CTRL2_RTSEN;
		else
			ctrl |= AUART_CTRL2_RTS;
	}

	mxs_write(ctrl, s, REG_CTRL2);

	mctrl_gpio_set(s->gpios, mctrl);
}

#define MCTRL_ANY_DELTA        (TIOCM_RI | TIOCM_DSR | TIOCM_CD | TIOCM_CTS)
static u32 mxs_auart_modem_status(struct mxs_auart_port *s, u32 mctrl)
{
	u32 mctrl_diff;

	mctrl_diff = mctrl ^ s->mctrl_prev;
	s->mctrl_prev = mctrl;
	if (mctrl_diff & MCTRL_ANY_DELTA && s->ms_irq_enabled &&
						s->port.state != NULL) {
		if (mctrl_diff & TIOCM_RI)
			s->port.icount.rng++;
		if (mctrl_diff & TIOCM_DSR)
			s->port.icount.dsr++;
		if (mctrl_diff & TIOCM_CD)
			uart_handle_dcd_change(&s->port, mctrl & TIOCM_CD);
		if (mctrl_diff & TIOCM_CTS)
			uart_handle_cts_change(&s->port, mctrl & TIOCM_CTS);

		wake_up_interruptible(&s->port.state->port.delta_msr_wait);
	}
	return mctrl;
}

static u32 mxs_auart_get_mctrl(struct uart_port *u)
{
	struct mxs_auart_port *s = to_auart_port(u);
	u32 stat = mxs_read(s, REG_STAT);
	u32 mctrl = 0;

	if (stat & AUART_STAT_CTS)
		mctrl |= TIOCM_CTS;

	return mctrl_gpio_get(s->gpios, &mctrl);
}

 
static void mxs_auart_enable_ms(struct uart_port *port)
{
	struct mxs_auart_port *s = to_auart_port(port);

	 
	if (s->ms_irq_enabled)
		return;

	s->ms_irq_enabled = true;

	if (s->gpio_irq[UART_GPIO_CTS] >= 0)
		enable_irq(s->gpio_irq[UART_GPIO_CTS]);
	 

	if (s->gpio_irq[UART_GPIO_DSR] >= 0)
		enable_irq(s->gpio_irq[UART_GPIO_DSR]);

	if (s->gpio_irq[UART_GPIO_RI] >= 0)
		enable_irq(s->gpio_irq[UART_GPIO_RI]);

	if (s->gpio_irq[UART_GPIO_DCD] >= 0)
		enable_irq(s->gpio_irq[UART_GPIO_DCD]);
}

 
static void mxs_auart_disable_ms(struct uart_port *port)
{
	struct mxs_auart_port *s = to_auart_port(port);

	 
	if (!s->ms_irq_enabled)
		return;

	s->ms_irq_enabled = false;

	if (s->gpio_irq[UART_GPIO_CTS] >= 0)
		disable_irq(s->gpio_irq[UART_GPIO_CTS]);
	 

	if (s->gpio_irq[UART_GPIO_DSR] >= 0)
		disable_irq(s->gpio_irq[UART_GPIO_DSR]);

	if (s->gpio_irq[UART_GPIO_RI] >= 0)
		disable_irq(s->gpio_irq[UART_GPIO_RI]);

	if (s->gpio_irq[UART_GPIO_DCD] >= 0)
		disable_irq(s->gpio_irq[UART_GPIO_DCD]);
}

static int mxs_auart_dma_prep_rx(struct mxs_auart_port *s);
static void dma_rx_callback(void *arg)
{
	struct mxs_auart_port *s = (struct mxs_auart_port *) arg;
	struct tty_port *port = &s->port.state->port;
	int count;
	u32 stat;

	dma_unmap_sg(s->dev, &s->rx_sgl, 1, DMA_FROM_DEVICE);

	stat = mxs_read(s, REG_STAT);
	stat &= ~(AUART_STAT_OERR | AUART_STAT_BERR |
			AUART_STAT_PERR | AUART_STAT_FERR);

	count = stat & AUART_STAT_RXCOUNT_MASK;
	tty_insert_flip_string(port, s->rx_dma_buf, count);

	mxs_write(stat, s, REG_STAT);
	tty_flip_buffer_push(port);

	 
	mxs_auart_dma_prep_rx(s);
}

static int mxs_auart_dma_prep_rx(struct mxs_auart_port *s)
{
	struct dma_async_tx_descriptor *desc;
	struct scatterlist *sgl = &s->rx_sgl;
	struct dma_chan *channel = s->rx_dma_chan;
	u32 pio[1];

	 
	pio[0] = AUART_CTRL0_RXTO_ENABLE
		| AUART_CTRL0_RXTIMEOUT(0x80)
		| AUART_CTRL0_XFER_COUNT(UART_XMIT_SIZE);
	desc = dmaengine_prep_slave_sg(channel, (struct scatterlist *)pio,
					1, DMA_TRANS_NONE, 0);
	if (!desc) {
		dev_err(s->dev, "step 1 error\n");
		return -EINVAL;
	}

	 
	sg_init_one(sgl, s->rx_dma_buf, UART_XMIT_SIZE);
	dma_map_sg(s->dev, sgl, 1, DMA_FROM_DEVICE);
	desc = dmaengine_prep_slave_sg(channel, sgl, 1, DMA_DEV_TO_MEM,
					DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(s->dev, "step 2 error\n");
		return -1;
	}

	 
	desc->callback = dma_rx_callback;
	desc->callback_param = s;
	dmaengine_submit(desc);
	dma_async_issue_pending(channel);
	return 0;
}

static void mxs_auart_dma_exit_channel(struct mxs_auart_port *s)
{
	if (s->tx_dma_chan) {
		dma_release_channel(s->tx_dma_chan);
		s->tx_dma_chan = NULL;
	}
	if (s->rx_dma_chan) {
		dma_release_channel(s->rx_dma_chan);
		s->rx_dma_chan = NULL;
	}

	kfree(s->tx_dma_buf);
	kfree(s->rx_dma_buf);
	s->tx_dma_buf = NULL;
	s->rx_dma_buf = NULL;
}

static void mxs_auart_dma_exit(struct mxs_auart_port *s)
{

	mxs_clr(AUART_CTRL2_TXDMAE | AUART_CTRL2_RXDMAE | AUART_CTRL2_DMAONERR,
		s, REG_CTRL2);

	mxs_auart_dma_exit_channel(s);
	s->flags &= ~MXS_AUART_DMA_ENABLED;
	clear_bit(MXS_AUART_DMA_TX_SYNC, &s->flags);
	clear_bit(MXS_AUART_DMA_RX_READY, &s->flags);
}

static int mxs_auart_dma_init(struct mxs_auart_port *s)
{
	if (auart_dma_enabled(s))
		return 0;

	 
	s->rx_dma_chan = dma_request_slave_channel(s->dev, "rx");
	if (!s->rx_dma_chan)
		goto err_out;
	s->rx_dma_buf = kzalloc(UART_XMIT_SIZE, GFP_KERNEL | GFP_DMA);
	if (!s->rx_dma_buf)
		goto err_out;

	 
	s->tx_dma_chan = dma_request_slave_channel(s->dev, "tx");
	if (!s->tx_dma_chan)
		goto err_out;
	s->tx_dma_buf = kzalloc(UART_XMIT_SIZE, GFP_KERNEL | GFP_DMA);
	if (!s->tx_dma_buf)
		goto err_out;

	 
	s->flags |= MXS_AUART_DMA_ENABLED;
	dev_dbg(s->dev, "enabled the DMA support.");

	 
	s->port.fifosize = UART_XMIT_SIZE;

	return 0;

err_out:
	mxs_auart_dma_exit_channel(s);
	return -EINVAL;

}

#define RTS_AT_AUART()	!mctrl_gpio_to_gpiod(s->gpios, UART_GPIO_RTS)
#define CTS_AT_AUART()	!mctrl_gpio_to_gpiod(s->gpios, UART_GPIO_CTS)
static void mxs_auart_settermios(struct uart_port *u,
				 struct ktermios *termios,
				 const struct ktermios *old)
{
	struct mxs_auart_port *s = to_auart_port(u);
	u32 ctrl, ctrl2, div;
	unsigned int cflag, baud, baud_min, baud_max;

	cflag = termios->c_cflag;

	ctrl = AUART_LINECTRL_FEN;
	ctrl2 = mxs_read(s, REG_CTRL2);

	ctrl |= AUART_LINECTRL_WLEN(tty_get_char_size(cflag));

	 
	if (cflag & PARENB) {
		ctrl |= AUART_LINECTRL_PEN;
		if ((cflag & PARODD) == 0)
			ctrl |= AUART_LINECTRL_EPS;
		if (cflag & CMSPAR)
			ctrl |= AUART_LINECTRL_SPS;
	}

	u->read_status_mask = AUART_STAT_OERR;

	if (termios->c_iflag & INPCK)
		u->read_status_mask |= AUART_STAT_PERR;
	if (termios->c_iflag & (IGNBRK | BRKINT | PARMRK))
		u->read_status_mask |= AUART_STAT_BERR;

	 
	u->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		u->ignore_status_mask |= AUART_STAT_PERR;
	if (termios->c_iflag & IGNBRK) {
		u->ignore_status_mask |= AUART_STAT_BERR;
		 
		if (termios->c_iflag & IGNPAR)
			u->ignore_status_mask |= AUART_STAT_OERR;
	}

	 
	if (cflag & CREAD)
		ctrl2 |= AUART_CTRL2_RXE;
	else
		ctrl2 &= ~AUART_CTRL2_RXE;

	 
	if (cflag & CSTOPB)
		ctrl |= AUART_LINECTRL_STP2;

	 
	ctrl2 &= ~(AUART_CTRL2_CTSEN | AUART_CTRL2_RTSEN);
	if (cflag & CRTSCTS) {
		 
		if (is_imx28_auart(s)
				&& test_bit(MXS_AUART_RTSCTS, &s->flags)) {
			if (!mxs_auart_dma_init(s))
				 
				ctrl2 |= AUART_CTRL2_TXDMAE | AUART_CTRL2_RXDMAE
				       | AUART_CTRL2_DMAONERR;
		}
		 
		ctrl2 |= AUART_CTRL2_RTSEN;
		if (CTS_AT_AUART())
			ctrl2 |= AUART_CTRL2_CTSEN;
	}

	 
	if (is_asm9260_auart(s)) {
		baud = uart_get_baud_rate(u, termios, old,
					  u->uartclk * 4 / 0x3FFFFF,
					  u->uartclk / 16);
		div = u->uartclk * 4 / baud;
	} else {
		baud_min = DIV_ROUND_UP(u->uartclk * 32,
					AUART_LINECTRL_BAUD_DIV_MAX);
		baud_max = u->uartclk * 32 / AUART_LINECTRL_BAUD_DIV_MIN;
		baud = uart_get_baud_rate(u, termios, old, baud_min, baud_max);
		div = DIV_ROUND_CLOSEST(u->uartclk * 32, baud);
	}

	ctrl |= AUART_LINECTRL_BAUD_DIVFRAC(div & 0x3F);
	ctrl |= AUART_LINECTRL_BAUD_DIVINT(div >> 6);
	mxs_write(ctrl, s, REG_LINECTRL);

	mxs_write(ctrl2, s, REG_CTRL2);

	uart_update_timeout(u, termios->c_cflag, baud);

	 
	if (auart_dma_enabled(s) &&
		!test_and_set_bit(MXS_AUART_DMA_RX_READY, &s->flags)) {
		if (!mxs_auart_dma_prep_rx(s)) {
			 
			mxs_clr(AUART_INTR_RXIEN | AUART_INTR_RTIEN,
				s, REG_INTR);
		} else {
			mxs_auart_dma_exit(s);
			dev_err(s->dev, "We can not start up the DMA.\n");
		}
	}

	 
	if (UART_ENABLE_MS(u, termios->c_cflag))
		mxs_auart_enable_ms(u);
	else
		mxs_auart_disable_ms(u);
}

static void mxs_auart_set_ldisc(struct uart_port *port,
				struct ktermios *termios)
{
	if (termios->c_line == N_PPS) {
		port->flags |= UPF_HARDPPS_CD;
		mxs_auart_enable_ms(port);
	} else {
		port->flags &= ~UPF_HARDPPS_CD;
	}
}

static irqreturn_t mxs_auart_irq_handle(int irq, void *context)
{
	u32 istat;
	struct mxs_auart_port *s = context;
	u32 mctrl_temp = s->mctrl_prev;
	u32 stat = mxs_read(s, REG_STAT);

	istat = mxs_read(s, REG_INTR);

	 
	mxs_clr(istat & (AUART_INTR_RTIS | AUART_INTR_TXIS | AUART_INTR_RXIS
		| AUART_INTR_CTSMIS), s, REG_INTR);

	 
	if (irq == s->gpio_irq[UART_GPIO_CTS] ||
	    irq == s->gpio_irq[UART_GPIO_DCD] ||
	    irq == s->gpio_irq[UART_GPIO_DSR] ||
	    irq == s->gpio_irq[UART_GPIO_RI])
		mxs_auart_modem_status(s,
				mctrl_gpio_get(s->gpios, &mctrl_temp));

	if (istat & AUART_INTR_CTSMIS) {
		if (CTS_AT_AUART() && s->ms_irq_enabled)
			uart_handle_cts_change(&s->port,
					stat & AUART_STAT_CTS);
		mxs_clr(AUART_INTR_CTSMIS, s, REG_INTR);
		istat &= ~AUART_INTR_CTSMIS;
	}

	if (istat & (AUART_INTR_RTIS | AUART_INTR_RXIS)) {
		if (!auart_dma_enabled(s))
			mxs_auart_rx_chars(s);
		istat &= ~(AUART_INTR_RTIS | AUART_INTR_RXIS);
	}

	if (istat & AUART_INTR_TXIS) {
		mxs_auart_tx_chars(s);
		istat &= ~AUART_INTR_TXIS;
	}

	return IRQ_HANDLED;
}

static void mxs_auart_reset_deassert(struct mxs_auart_port *s)
{
	int i;
	unsigned int reg;

	mxs_clr(AUART_CTRL0_SFTRST, s, REG_CTRL0);

	for (i = 0; i < 10000; i++) {
		reg = mxs_read(s, REG_CTRL0);
		if (!(reg & AUART_CTRL0_SFTRST))
			break;
		udelay(3);
	}
	mxs_clr(AUART_CTRL0_CLKGATE, s, REG_CTRL0);
}

static void mxs_auart_reset_assert(struct mxs_auart_port *s)
{
	int i;
	u32 reg;

	reg = mxs_read(s, REG_CTRL0);
	 
	if (reg & AUART_CTRL0_SFTRST)
		return;

	mxs_clr(AUART_CTRL0_CLKGATE, s, REG_CTRL0);
	mxs_set(AUART_CTRL0_SFTRST, s, REG_CTRL0);

	for (i = 0; i < 1000; i++) {
		reg = mxs_read(s, REG_CTRL0);
		 
		if (reg & AUART_CTRL0_CLKGATE)
			return;
		udelay(10);
	}

	dev_err(s->dev, "Failed to reset the unit.");
}

static int mxs_auart_startup(struct uart_port *u)
{
	int ret;
	struct mxs_auart_port *s = to_auart_port(u);

	ret = clk_prepare_enable(s->clk);
	if (ret)
		return ret;

	if (uart_console(u)) {
		mxs_clr(AUART_CTRL0_CLKGATE, s, REG_CTRL0);
	} else {
		 
		mxs_auart_reset_assert(s);
		mxs_auart_reset_deassert(s);
	}

	mxs_set(AUART_CTRL2_UARTEN, s, REG_CTRL2);

	mxs_write(AUART_INTR_RXIEN | AUART_INTR_RTIEN | AUART_INTR_CTSMIEN,
		  s, REG_INTR);

	 
	u->fifosize = MXS_AUART_FIFO_SIZE;

	 
	mxs_set(AUART_LINECTRL_FEN, s, REG_LINECTRL);

	 
	mctrl_gpio_get(s->gpios, &s->mctrl_prev);

	s->ms_irq_enabled = false;
	return 0;
}

static void mxs_auart_shutdown(struct uart_port *u)
{
	struct mxs_auart_port *s = to_auart_port(u);

	mxs_auart_disable_ms(u);

	if (auart_dma_enabled(s))
		mxs_auart_dma_exit(s);

	if (uart_console(u)) {
		mxs_clr(AUART_CTRL2_UARTEN, s, REG_CTRL2);

		mxs_clr(AUART_INTR_RXIEN | AUART_INTR_RTIEN |
			AUART_INTR_CTSMIEN, s, REG_INTR);
		mxs_set(AUART_CTRL0_CLKGATE, s, REG_CTRL0);
	} else {
		mxs_auart_reset_assert(s);
	}

	clk_disable_unprepare(s->clk);
}

static unsigned int mxs_auart_tx_empty(struct uart_port *u)
{
	struct mxs_auart_port *s = to_auart_port(u);

	if ((mxs_read(s, REG_STAT) &
		 (AUART_STAT_TXFE | AUART_STAT_BUSY)) == AUART_STAT_TXFE)
		return TIOCSER_TEMT;

	return 0;
}

static void mxs_auart_start_tx(struct uart_port *u)
{
	struct mxs_auart_port *s = to_auart_port(u);

	 
	mxs_set(AUART_CTRL2_TXE, s, REG_CTRL2);

	mxs_auart_tx_chars(s);
}

static void mxs_auart_stop_tx(struct uart_port *u)
{
	struct mxs_auart_port *s = to_auart_port(u);

	mxs_clr(AUART_CTRL2_TXE, s, REG_CTRL2);
}

static void mxs_auart_stop_rx(struct uart_port *u)
{
	struct mxs_auart_port *s = to_auart_port(u);

	mxs_clr(AUART_CTRL2_RXE, s, REG_CTRL2);
}

static void mxs_auart_break_ctl(struct uart_port *u, int ctl)
{
	struct mxs_auart_port *s = to_auart_port(u);

	if (ctl)
		mxs_set(AUART_LINECTRL_BRK, s, REG_LINECTRL);
	else
		mxs_clr(AUART_LINECTRL_BRK, s, REG_LINECTRL);
}

static const struct uart_ops mxs_auart_ops = {
	.tx_empty       = mxs_auart_tx_empty,
	.start_tx       = mxs_auart_start_tx,
	.stop_tx	= mxs_auart_stop_tx,
	.stop_rx	= mxs_auart_stop_rx,
	.enable_ms      = mxs_auart_enable_ms,
	.break_ctl      = mxs_auart_break_ctl,
	.set_mctrl	= mxs_auart_set_mctrl,
	.get_mctrl      = mxs_auart_get_mctrl,
	.startup	= mxs_auart_startup,
	.shutdown       = mxs_auart_shutdown,
	.set_termios    = mxs_auart_settermios,
	.set_ldisc      = mxs_auart_set_ldisc,
	.type	   	= mxs_auart_type,
	.release_port   = mxs_auart_release_port,
	.request_port   = mxs_auart_request_port,
	.config_port    = mxs_auart_config_port,
	.verify_port    = mxs_auart_verify_port,
};

static struct mxs_auart_port *auart_port[MXS_AUART_PORTS];

#ifdef CONFIG_SERIAL_MXS_AUART_CONSOLE
static void mxs_auart_console_putchar(struct uart_port *port, unsigned char ch)
{
	struct mxs_auart_port *s = to_auart_port(port);
	unsigned int to = 1000;

	while (mxs_read(s, REG_STAT) & AUART_STAT_TXFF) {
		if (!to--)
			break;
		udelay(1);
	}

	mxs_write(ch, s, REG_DATA);
}

static void
auart_console_write(struct console *co, const char *str, unsigned int count)
{
	struct mxs_auart_port *s;
	struct uart_port *port;
	unsigned int old_ctrl0, old_ctrl2;
	unsigned int to = 20000;

	if (co->index >= MXS_AUART_PORTS || co->index < 0)
		return;

	s = auart_port[co->index];
	port = &s->port;

	clk_enable(s->clk);

	 
	old_ctrl2 = mxs_read(s, REG_CTRL2);
	old_ctrl0 = mxs_read(s, REG_CTRL0);

	mxs_clr(AUART_CTRL0_CLKGATE, s, REG_CTRL0);
	mxs_set(AUART_CTRL2_UARTEN | AUART_CTRL2_TXE, s, REG_CTRL2);

	uart_console_write(port, str, count, mxs_auart_console_putchar);

	 
	while (mxs_read(s, REG_STAT) & AUART_STAT_BUSY) {
		udelay(1);
		if (!to--)
			break;
	}

	 
	if (!(mxs_read(s, REG_STAT) & AUART_STAT_BUSY)) {
		mxs_write(old_ctrl0, s, REG_CTRL0);
		mxs_write(old_ctrl2, s, REG_CTRL2);
	}

	clk_disable(s->clk);
}

static void __init
auart_console_get_options(struct mxs_auart_port *s, int *baud,
			  int *parity, int *bits)
{
	struct uart_port *port = &s->port;
	unsigned int lcr_h, quot;

	if (!(mxs_read(s, REG_CTRL2) & AUART_CTRL2_UARTEN))
		return;

	lcr_h = mxs_read(s, REG_LINECTRL);

	*parity = 'n';
	if (lcr_h & AUART_LINECTRL_PEN) {
		if (lcr_h & AUART_LINECTRL_EPS)
			*parity = 'e';
		else
			*parity = 'o';
	}

	if ((lcr_h & AUART_LINECTRL_WLEN_MASK) == AUART_LINECTRL_WLEN(7))
		*bits = 7;
	else
		*bits = 8;

	quot = ((mxs_read(s, REG_LINECTRL) & AUART_LINECTRL_BAUD_DIVINT_MASK))
		>> (AUART_LINECTRL_BAUD_DIVINT_SHIFT - 6);
	quot |= ((mxs_read(s, REG_LINECTRL) & AUART_LINECTRL_BAUD_DIVFRAC_MASK))
		>> AUART_LINECTRL_BAUD_DIVFRAC_SHIFT;
	if (quot == 0)
		quot = 1;

	*baud = (port->uartclk << 2) / quot;
}

static int __init
auart_console_setup(struct console *co, char *options)
{
	struct mxs_auart_port *s;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	int ret;

	 
	if (co->index == -1 || co->index >= ARRAY_SIZE(auart_port))
		co->index = 0;
	s = auart_port[co->index];
	if (!s)
		return -ENODEV;

	ret = clk_prepare_enable(s->clk);
	if (ret)
		return ret;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		auart_console_get_options(s, &baud, &parity, &bits);

	ret = uart_set_options(&s->port, co, baud, parity, bits, flow);

	clk_disable_unprepare(s->clk);

	return ret;
}

static struct console auart_console = {
	.name		= "ttyAPP",
	.write		= auart_console_write,
	.device		= uart_console_device,
	.setup		= auart_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &auart_driver,
};
#endif

static struct uart_driver auart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "ttyAPP",
	.dev_name	= "ttyAPP",
	.major		= 0,
	.minor		= 0,
	.nr		= MXS_AUART_PORTS,
#ifdef CONFIG_SERIAL_MXS_AUART_CONSOLE
	.cons =		&auart_console,
#endif
};

static void mxs_init_regs(struct mxs_auart_port *s)
{
	if (is_asm9260_auart(s))
		s->vendor = &vendor_alphascale_asm9260;
	else
		s->vendor = &vendor_freescale_stmp37xx;
}

static int mxs_get_clks(struct mxs_auart_port *s,
			struct platform_device *pdev)
{
	int err;

	if (!is_asm9260_auart(s)) {
		s->clk = devm_clk_get(&pdev->dev, NULL);
		return PTR_ERR_OR_ZERO(s->clk);
	}

	s->clk = devm_clk_get(s->dev, "mod");
	if (IS_ERR(s->clk)) {
		dev_err(s->dev, "Failed to get \"mod\" clk\n");
		return PTR_ERR(s->clk);
	}

	s->clk_ahb = devm_clk_get(s->dev, "ahb");
	if (IS_ERR(s->clk_ahb)) {
		dev_err(s->dev, "Failed to get \"ahb\" clk\n");
		return PTR_ERR(s->clk_ahb);
	}

	err = clk_prepare_enable(s->clk_ahb);
	if (err) {
		dev_err(s->dev, "Failed to enable ahb_clk!\n");
		return err;
	}

	err = clk_set_rate(s->clk, clk_get_rate(s->clk_ahb));
	if (err) {
		dev_err(s->dev, "Failed to set rate!\n");
		goto disable_clk_ahb;
	}

	err = clk_prepare_enable(s->clk);
	if (err) {
		dev_err(s->dev, "Failed to enable clk!\n");
		goto disable_clk_ahb;
	}

	return 0;

disable_clk_ahb:
	clk_disable_unprepare(s->clk_ahb);
	return err;
}

static int mxs_auart_init_gpios(struct mxs_auart_port *s, struct device *dev)
{
	enum mctrl_gpio_idx i;
	struct gpio_desc *gpiod;

	s->gpios = mctrl_gpio_init_noauto(dev, 0);
	if (IS_ERR(s->gpios))
		return PTR_ERR(s->gpios);

	 
	if (!RTS_AT_AUART() || !CTS_AT_AUART()) {
		if (test_bit(MXS_AUART_RTSCTS, &s->flags))
			dev_warn(dev,
				 "DMA and flow control via gpio may cause some problems. DMA disabled!\n");
		clear_bit(MXS_AUART_RTSCTS, &s->flags);
	}

	for (i = 0; i < UART_GPIO_MAX; i++) {
		gpiod = mctrl_gpio_to_gpiod(s->gpios, i);
		if (gpiod && (gpiod_get_direction(gpiod) == 1))
			s->gpio_irq[i] = gpiod_to_irq(gpiod);
		else
			s->gpio_irq[i] = -EINVAL;
	}

	return 0;
}

static void mxs_auart_free_gpio_irq(struct mxs_auart_port *s)
{
	enum mctrl_gpio_idx i;

	for (i = 0; i < UART_GPIO_MAX; i++)
		if (s->gpio_irq[i] >= 0)
			free_irq(s->gpio_irq[i], s);
}

static int mxs_auart_request_gpio_irq(struct mxs_auart_port *s)
{
	int *irq = s->gpio_irq;
	enum mctrl_gpio_idx i;
	int err = 0;

	for (i = 0; (i < UART_GPIO_MAX) && !err; i++) {
		if (irq[i] < 0)
			continue;

		irq_set_status_flags(irq[i], IRQ_NOAUTOEN);
		err = request_irq(irq[i], mxs_auart_irq_handle,
				IRQ_TYPE_EDGE_BOTH, dev_name(s->dev), s);
		if (err)
			dev_err(s->dev, "%s - Can't get %d irq\n",
				__func__, irq[i]);
	}

	 
	while (err && (i-- > 0))
		if (irq[i] >= 0)
			free_irq(irq[i], s);

	return err;
}

static int mxs_auart_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mxs_auart_port *s;
	u32 version;
	int ret, irq;
	struct resource *r;

	s = devm_kzalloc(&pdev->dev, sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;

	s->port.dev = &pdev->dev;
	s->dev = &pdev->dev;

	ret = of_alias_get_id(np, "serial");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias id: %d\n", ret);
		return ret;
	}
	s->port.line = ret;

	if (of_property_read_bool(np, "uart-has-rtscts") ||
	    of_property_read_bool(np, "fsl,uart-has-rtscts")  )
		set_bit(MXS_AUART_RTSCTS, &s->flags);

	if (s->port.line >= ARRAY_SIZE(auart_port)) {
		dev_err(&pdev->dev, "serial%d out of range\n", s->port.line);
		return -EINVAL;
	}

	s->devtype = (enum mxs_auart_type)of_device_get_match_data(&pdev->dev);

	ret = mxs_get_clks(s, pdev);
	if (ret)
		return ret;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		ret = -ENXIO;
		goto out_disable_clks;
	}

	s->port.mapbase = r->start;
	s->port.membase = ioremap(r->start, resource_size(r));
	if (!s->port.membase) {
		ret = -ENOMEM;
		goto out_disable_clks;
	}
	s->port.ops = &mxs_auart_ops;
	s->port.iotype = UPIO_MEM;
	s->port.fifosize = MXS_AUART_FIFO_SIZE;
	s->port.uartclk = clk_get_rate(s->clk);
	s->port.type = PORT_IMX;
	s->port.has_sysrq = IS_ENABLED(CONFIG_SERIAL_MXS_AUART_CONSOLE);

	mxs_init_regs(s);

	s->mctrl_prev = 0;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto out_iounmap;
	}

	s->port.irq = irq;
	ret = devm_request_irq(&pdev->dev, irq, mxs_auart_irq_handle, 0,
			       dev_name(&pdev->dev), s);
	if (ret)
		goto out_iounmap;

	platform_set_drvdata(pdev, s);

	ret = mxs_auart_init_gpios(s, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize GPIOs.\n");
		goto out_iounmap;
	}

	 
	ret = mxs_auart_request_gpio_irq(s);
	if (ret)
		goto out_iounmap;

	auart_port[s->port.line] = s;

	mxs_auart_reset_deassert(s);

	ret = uart_add_one_port(&auart_driver, &s->port);
	if (ret)
		goto out_free_qpio_irq;

	 
	if (is_asm9260_auart(s)) {
		dev_info(&pdev->dev, "Found APPUART ASM9260\n");
	} else {
		version = mxs_read(s, REG_VERSION);
		dev_info(&pdev->dev, "Found APPUART %d.%d.%d\n",
			 (version >> 24) & 0xff,
			 (version >> 16) & 0xff, version & 0xffff);
	}

	return 0;

out_free_qpio_irq:
	mxs_auart_free_gpio_irq(s);
	auart_port[pdev->id] = NULL;

out_iounmap:
	iounmap(s->port.membase);

out_disable_clks:
	if (is_asm9260_auart(s)) {
		clk_disable_unprepare(s->clk);
		clk_disable_unprepare(s->clk_ahb);
	}
	return ret;
}

static int mxs_auart_remove(struct platform_device *pdev)
{
	struct mxs_auart_port *s = platform_get_drvdata(pdev);

	uart_remove_one_port(&auart_driver, &s->port);
	auart_port[pdev->id] = NULL;
	mxs_auart_free_gpio_irq(s);
	iounmap(s->port.membase);
	if (is_asm9260_auart(s)) {
		clk_disable_unprepare(s->clk);
		clk_disable_unprepare(s->clk_ahb);
	}

	return 0;
}

static struct platform_driver mxs_auart_driver = {
	.probe = mxs_auart_probe,
	.remove = mxs_auart_remove,
	.driver = {
		.name = "mxs-auart",
		.of_match_table = mxs_auart_dt_ids,
	},
};

static int __init mxs_auart_init(void)
{
	int r;

	r = uart_register_driver(&auart_driver);
	if (r)
		goto out;

	r = platform_driver_register(&mxs_auart_driver);
	if (r)
		goto out_err;

	return 0;
out_err:
	uart_unregister_driver(&auart_driver);
out:
	return r;
}

static void __exit mxs_auart_exit(void)
{
	platform_driver_unregister(&mxs_auart_driver);
	uart_unregister_driver(&auart_driver);
}

module_init(mxs_auart_init);
module_exit(mxs_auart_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Freescale MXS application uart driver");
MODULE_ALIAS("platform:mxs-auart");
