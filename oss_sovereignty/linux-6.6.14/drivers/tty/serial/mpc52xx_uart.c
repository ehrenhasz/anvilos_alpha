
 

#undef DEBUG

#include <linux/device.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/mpc52xx.h>
#include <asm/mpc52xx_psc.h>

#include <linux/serial_core.h>


 
#define SERIAL_PSC_MAJOR	204
#define SERIAL_PSC_MINOR	148


#define ISR_PASS_LIMIT 256	 


static struct uart_port mpc52xx_uart_ports[MPC52xx_PSC_MAXNUM];
	 

 
static struct device_node *mpc52xx_uart_nodes[MPC52xx_PSC_MAXNUM];

static void mpc52xx_uart_of_enumerate(void);


#define PSC(port) ((struct mpc52xx_psc __iomem *)((port)->membase))


 
static irqreturn_t mpc52xx_uart_int(int irq, void *dev_id);
static irqreturn_t mpc5xxx_uart_process_int(struct uart_port *port);

 
 
 

struct psc_ops {
	void		(*fifo_init)(struct uart_port *port);
	unsigned int	(*raw_rx_rdy)(struct uart_port *port);
	unsigned int	(*raw_tx_rdy)(struct uart_port *port);
	unsigned int	(*rx_rdy)(struct uart_port *port);
	unsigned int	(*tx_rdy)(struct uart_port *port);
	unsigned int	(*tx_empty)(struct uart_port *port);
	void		(*stop_rx)(struct uart_port *port);
	void		(*start_tx)(struct uart_port *port);
	void		(*stop_tx)(struct uart_port *port);
	void		(*rx_clr_irq)(struct uart_port *port);
	void		(*tx_clr_irq)(struct uart_port *port);
	void		(*write_char)(struct uart_port *port, unsigned char c);
	unsigned char	(*read_char)(struct uart_port *port);
	void		(*cw_disable_ints)(struct uart_port *port);
	void		(*cw_restore_ints)(struct uart_port *port);
	unsigned int	(*set_baudrate)(struct uart_port *port,
					struct ktermios *new,
					const struct ktermios *old);
	int		(*clock_alloc)(struct uart_port *port);
	void		(*clock_relse)(struct uart_port *port);
	int		(*clock)(struct uart_port *port, int enable);
	int		(*fifoc_init)(void);
	void		(*fifoc_uninit)(void);
	void		(*get_irq)(struct uart_port *, struct device_node *);
	irqreturn_t	(*handle_irq)(struct uart_port *port);
	u16		(*get_status)(struct uart_port *port);
	u8		(*get_ipcr)(struct uart_port *port);
	void		(*command)(struct uart_port *port, u8 cmd);
	void		(*set_mode)(struct uart_port *port, u8 mr1, u8 mr2);
	void		(*set_rts)(struct uart_port *port, int state);
	void		(*enable_ms)(struct uart_port *port);
	void		(*set_sicr)(struct uart_port *port, u32 val);
	void		(*set_imr)(struct uart_port *port, u16 val);
	u8		(*get_mr1)(struct uart_port *port);
};

 
static inline void mpc52xx_set_divisor(struct mpc52xx_psc __iomem *psc,
				       u16 prescaler, unsigned int divisor)
{
	 
	out_be16(&psc->mpc52xx_psc_clock_select, prescaler);
	out_8(&psc->ctur, divisor >> 8);
	out_8(&psc->ctlr, divisor & 0xff);
}

static u16 mpc52xx_psc_get_status(struct uart_port *port)
{
	return in_be16(&PSC(port)->mpc52xx_psc_status);
}

static u8 mpc52xx_psc_get_ipcr(struct uart_port *port)
{
	return in_8(&PSC(port)->mpc52xx_psc_ipcr);
}

static void mpc52xx_psc_command(struct uart_port *port, u8 cmd)
{
	out_8(&PSC(port)->command, cmd);
}

static void mpc52xx_psc_set_mode(struct uart_port *port, u8 mr1, u8 mr2)
{
	out_8(&PSC(port)->command, MPC52xx_PSC_SEL_MODE_REG_1);
	out_8(&PSC(port)->mode, mr1);
	out_8(&PSC(port)->mode, mr2);
}

static void mpc52xx_psc_set_rts(struct uart_port *port, int state)
{
	if (state)
		out_8(&PSC(port)->op1, MPC52xx_PSC_OP_RTS);
	else
		out_8(&PSC(port)->op0, MPC52xx_PSC_OP_RTS);
}

static void mpc52xx_psc_enable_ms(struct uart_port *port)
{
	struct mpc52xx_psc __iomem *psc = PSC(port);

	 
	in_8(&psc->mpc52xx_psc_ipcr);
	 
	out_8(&psc->mpc52xx_psc_acr, MPC52xx_PSC_IEC_CTS | MPC52xx_PSC_IEC_DCD);

	port->read_status_mask |= MPC52xx_PSC_IMR_IPC;
	out_be16(&psc->mpc52xx_psc_imr, port->read_status_mask);
}

static void mpc52xx_psc_set_sicr(struct uart_port *port, u32 val)
{
	out_be32(&PSC(port)->sicr, val);
}

static void mpc52xx_psc_set_imr(struct uart_port *port, u16 val)
{
	out_be16(&PSC(port)->mpc52xx_psc_imr, val);
}

static u8 mpc52xx_psc_get_mr1(struct uart_port *port)
{
	out_8(&PSC(port)->command, MPC52xx_PSC_SEL_MODE_REG_1);
	return in_8(&PSC(port)->mode);
}

#ifdef CONFIG_PPC_MPC52xx
#define FIFO_52xx(port) ((struct mpc52xx_psc_fifo __iomem *)(PSC(port)+1))
static void mpc52xx_psc_fifo_init(struct uart_port *port)
{
	struct mpc52xx_psc __iomem *psc = PSC(port);
	struct mpc52xx_psc_fifo __iomem *fifo = FIFO_52xx(port);

	out_8(&fifo->rfcntl, 0x00);
	out_be16(&fifo->rfalarm, 0x1ff);
	out_8(&fifo->tfcntl, 0x07);
	out_be16(&fifo->tfalarm, 0x80);

	port->read_status_mask |= MPC52xx_PSC_IMR_RXRDY | MPC52xx_PSC_IMR_TXRDY;
	out_be16(&psc->mpc52xx_psc_imr, port->read_status_mask);
}

static unsigned int mpc52xx_psc_raw_rx_rdy(struct uart_port *port)
{
	return in_be16(&PSC(port)->mpc52xx_psc_status)
	    & MPC52xx_PSC_SR_RXRDY;
}

static unsigned int mpc52xx_psc_raw_tx_rdy(struct uart_port *port)
{
	return in_be16(&PSC(port)->mpc52xx_psc_status)
	    & MPC52xx_PSC_SR_TXRDY;
}


static unsigned int mpc52xx_psc_rx_rdy(struct uart_port *port)
{
	return in_be16(&PSC(port)->mpc52xx_psc_isr)
	    & port->read_status_mask
	    & MPC52xx_PSC_IMR_RXRDY;
}

static unsigned int mpc52xx_psc_tx_rdy(struct uart_port *port)
{
	return in_be16(&PSC(port)->mpc52xx_psc_isr)
	    & port->read_status_mask
	    & MPC52xx_PSC_IMR_TXRDY;
}

static unsigned int mpc52xx_psc_tx_empty(struct uart_port *port)
{
	u16 sts = in_be16(&PSC(port)->mpc52xx_psc_status);

	return (sts & MPC52xx_PSC_SR_TXEMP) ? TIOCSER_TEMT : 0;
}

static void mpc52xx_psc_start_tx(struct uart_port *port)
{
	port->read_status_mask |= MPC52xx_PSC_IMR_TXRDY;
	out_be16(&PSC(port)->mpc52xx_psc_imr, port->read_status_mask);
}

static void mpc52xx_psc_stop_tx(struct uart_port *port)
{
	port->read_status_mask &= ~MPC52xx_PSC_IMR_TXRDY;
	out_be16(&PSC(port)->mpc52xx_psc_imr, port->read_status_mask);
}

static void mpc52xx_psc_stop_rx(struct uart_port *port)
{
	port->read_status_mask &= ~MPC52xx_PSC_IMR_RXRDY;
	out_be16(&PSC(port)->mpc52xx_psc_imr, port->read_status_mask);
}

static void mpc52xx_psc_rx_clr_irq(struct uart_port *port)
{
}

static void mpc52xx_psc_tx_clr_irq(struct uart_port *port)
{
}

static void mpc52xx_psc_write_char(struct uart_port *port, unsigned char c)
{
	out_8(&PSC(port)->mpc52xx_psc_buffer_8, c);
}

static unsigned char mpc52xx_psc_read_char(struct uart_port *port)
{
	return in_8(&PSC(port)->mpc52xx_psc_buffer_8);
}

static void mpc52xx_psc_cw_disable_ints(struct uart_port *port)
{
	out_be16(&PSC(port)->mpc52xx_psc_imr, 0);
}

static void mpc52xx_psc_cw_restore_ints(struct uart_port *port)
{
	out_be16(&PSC(port)->mpc52xx_psc_imr, port->read_status_mask);
}

static unsigned int mpc5200_psc_set_baudrate(struct uart_port *port,
					     struct ktermios *new,
					     const struct ktermios *old)
{
	unsigned int baud;
	unsigned int divisor;

	 
	baud = uart_get_baud_rate(port, new, old,
				  port->uartclk / (32 * 0xffff) + 1,
				  port->uartclk / 32);
	divisor = (port->uartclk + 16 * baud) / (32 * baud);

	 
	mpc52xx_set_divisor(PSC(port), 0xdd00, divisor);
	return baud;
}

static unsigned int mpc5200b_psc_set_baudrate(struct uart_port *port,
					      struct ktermios *new,
					      const struct ktermios *old)
{
	unsigned int baud;
	unsigned int divisor;
	u16 prescaler;

	 
	baud = uart_get_baud_rate(port, new, old,
				  port->uartclk / (32 * 0xffff) + 1,
				  port->uartclk / 4);
	divisor = (port->uartclk + 2 * baud) / (4 * baud);

	 
	if (divisor > 0xffff || baud <= 115200) {
		divisor = (divisor + 4) / 8;
		prescaler = 0xdd00;  
	} else
		prescaler = 0xff00;  
	mpc52xx_set_divisor(PSC(port), prescaler, divisor);
	return baud;
}

static void mpc52xx_psc_get_irq(struct uart_port *port, struct device_node *np)
{
	port->irqflags = 0;
	port->irq = irq_of_parse_and_map(np, 0);
}

 
static irqreturn_t mpc52xx_psc_handle_irq(struct uart_port *port)
{
	return mpc5xxx_uart_process_int(port);
}

static const struct psc_ops mpc52xx_psc_ops = {
	.fifo_init = mpc52xx_psc_fifo_init,
	.raw_rx_rdy = mpc52xx_psc_raw_rx_rdy,
	.raw_tx_rdy = mpc52xx_psc_raw_tx_rdy,
	.rx_rdy = mpc52xx_psc_rx_rdy,
	.tx_rdy = mpc52xx_psc_tx_rdy,
	.tx_empty = mpc52xx_psc_tx_empty,
	.stop_rx = mpc52xx_psc_stop_rx,
	.start_tx = mpc52xx_psc_start_tx,
	.stop_tx = mpc52xx_psc_stop_tx,
	.rx_clr_irq = mpc52xx_psc_rx_clr_irq,
	.tx_clr_irq = mpc52xx_psc_tx_clr_irq,
	.write_char = mpc52xx_psc_write_char,
	.read_char = mpc52xx_psc_read_char,
	.cw_disable_ints = mpc52xx_psc_cw_disable_ints,
	.cw_restore_ints = mpc52xx_psc_cw_restore_ints,
	.set_baudrate = mpc5200_psc_set_baudrate,
	.get_irq = mpc52xx_psc_get_irq,
	.handle_irq = mpc52xx_psc_handle_irq,
	.get_status = mpc52xx_psc_get_status,
	.get_ipcr = mpc52xx_psc_get_ipcr,
	.command = mpc52xx_psc_command,
	.set_mode = mpc52xx_psc_set_mode,
	.set_rts = mpc52xx_psc_set_rts,
	.enable_ms = mpc52xx_psc_enable_ms,
	.set_sicr = mpc52xx_psc_set_sicr,
	.set_imr = mpc52xx_psc_set_imr,
	.get_mr1 = mpc52xx_psc_get_mr1,
};

static const struct psc_ops mpc5200b_psc_ops = {
	.fifo_init = mpc52xx_psc_fifo_init,
	.raw_rx_rdy = mpc52xx_psc_raw_rx_rdy,
	.raw_tx_rdy = mpc52xx_psc_raw_tx_rdy,
	.rx_rdy = mpc52xx_psc_rx_rdy,
	.tx_rdy = mpc52xx_psc_tx_rdy,
	.tx_empty = mpc52xx_psc_tx_empty,
	.stop_rx = mpc52xx_psc_stop_rx,
	.start_tx = mpc52xx_psc_start_tx,
	.stop_tx = mpc52xx_psc_stop_tx,
	.rx_clr_irq = mpc52xx_psc_rx_clr_irq,
	.tx_clr_irq = mpc52xx_psc_tx_clr_irq,
	.write_char = mpc52xx_psc_write_char,
	.read_char = mpc52xx_psc_read_char,
	.cw_disable_ints = mpc52xx_psc_cw_disable_ints,
	.cw_restore_ints = mpc52xx_psc_cw_restore_ints,
	.set_baudrate = mpc5200b_psc_set_baudrate,
	.get_irq = mpc52xx_psc_get_irq,
	.handle_irq = mpc52xx_psc_handle_irq,
	.get_status = mpc52xx_psc_get_status,
	.get_ipcr = mpc52xx_psc_get_ipcr,
	.command = mpc52xx_psc_command,
	.set_mode = mpc52xx_psc_set_mode,
	.set_rts = mpc52xx_psc_set_rts,
	.enable_ms = mpc52xx_psc_enable_ms,
	.set_sicr = mpc52xx_psc_set_sicr,
	.set_imr = mpc52xx_psc_set_imr,
	.get_mr1 = mpc52xx_psc_get_mr1,
};

#endif  

#ifdef CONFIG_PPC_MPC512x
#define FIFO_512x(port) ((struct mpc512x_psc_fifo __iomem *)(PSC(port)+1))

 
struct psc_fifoc {
	u32 fifoc_cmd;
	u32 fifoc_int;
	u32 fifoc_dma;
	u32 fifoc_axe;
	u32 fifoc_debug;
};

static struct psc_fifoc __iomem *psc_fifoc;
static unsigned int psc_fifoc_irq;
static struct clk *psc_fifoc_clk;

static void mpc512x_psc_fifo_init(struct uart_port *port)
{
	 
	out_be16(&PSC(port)->mpc52xx_psc_clock_select, 0xdd00);

	out_be32(&FIFO_512x(port)->txcmd, MPC512x_PSC_FIFO_RESET_SLICE);
	out_be32(&FIFO_512x(port)->txcmd, MPC512x_PSC_FIFO_ENABLE_SLICE);
	out_be32(&FIFO_512x(port)->txalarm, 1);
	out_be32(&FIFO_512x(port)->tximr, 0);

	out_be32(&FIFO_512x(port)->rxcmd, MPC512x_PSC_FIFO_RESET_SLICE);
	out_be32(&FIFO_512x(port)->rxcmd, MPC512x_PSC_FIFO_ENABLE_SLICE);
	out_be32(&FIFO_512x(port)->rxalarm, 1);
	out_be32(&FIFO_512x(port)->rximr, 0);

	out_be32(&FIFO_512x(port)->tximr, MPC512x_PSC_FIFO_ALARM);
	out_be32(&FIFO_512x(port)->rximr, MPC512x_PSC_FIFO_ALARM);
}

static unsigned int mpc512x_psc_raw_rx_rdy(struct uart_port *port)
{
	return !(in_be32(&FIFO_512x(port)->rxsr) & MPC512x_PSC_FIFO_EMPTY);
}

static unsigned int mpc512x_psc_raw_tx_rdy(struct uart_port *port)
{
	return !(in_be32(&FIFO_512x(port)->txsr) & MPC512x_PSC_FIFO_FULL);
}

static unsigned int mpc512x_psc_rx_rdy(struct uart_port *port)
{
	return in_be32(&FIFO_512x(port)->rxsr)
	    & in_be32(&FIFO_512x(port)->rximr)
	    & MPC512x_PSC_FIFO_ALARM;
}

static unsigned int mpc512x_psc_tx_rdy(struct uart_port *port)
{
	return in_be32(&FIFO_512x(port)->txsr)
	    & in_be32(&FIFO_512x(port)->tximr)
	    & MPC512x_PSC_FIFO_ALARM;
}

static unsigned int mpc512x_psc_tx_empty(struct uart_port *port)
{
	return in_be32(&FIFO_512x(port)->txsr)
	    & MPC512x_PSC_FIFO_EMPTY;
}

static void mpc512x_psc_stop_rx(struct uart_port *port)
{
	unsigned long rx_fifo_imr;

	rx_fifo_imr = in_be32(&FIFO_512x(port)->rximr);
	rx_fifo_imr &= ~MPC512x_PSC_FIFO_ALARM;
	out_be32(&FIFO_512x(port)->rximr, rx_fifo_imr);
}

static void mpc512x_psc_start_tx(struct uart_port *port)
{
	unsigned long tx_fifo_imr;

	tx_fifo_imr = in_be32(&FIFO_512x(port)->tximr);
	tx_fifo_imr |= MPC512x_PSC_FIFO_ALARM;
	out_be32(&FIFO_512x(port)->tximr, tx_fifo_imr);
}

static void mpc512x_psc_stop_tx(struct uart_port *port)
{
	unsigned long tx_fifo_imr;

	tx_fifo_imr = in_be32(&FIFO_512x(port)->tximr);
	tx_fifo_imr &= ~MPC512x_PSC_FIFO_ALARM;
	out_be32(&FIFO_512x(port)->tximr, tx_fifo_imr);
}

static void mpc512x_psc_rx_clr_irq(struct uart_port *port)
{
	out_be32(&FIFO_512x(port)->rxisr, in_be32(&FIFO_512x(port)->rxisr));
}

static void mpc512x_psc_tx_clr_irq(struct uart_port *port)
{
	out_be32(&FIFO_512x(port)->txisr, in_be32(&FIFO_512x(port)->txisr));
}

static void mpc512x_psc_write_char(struct uart_port *port, unsigned char c)
{
	out_8(&FIFO_512x(port)->txdata_8, c);
}

static unsigned char mpc512x_psc_read_char(struct uart_port *port)
{
	return in_8(&FIFO_512x(port)->rxdata_8);
}

static void mpc512x_psc_cw_disable_ints(struct uart_port *port)
{
	port->read_status_mask =
		in_be32(&FIFO_512x(port)->tximr) << 16 |
		in_be32(&FIFO_512x(port)->rximr);
	out_be32(&FIFO_512x(port)->tximr, 0);
	out_be32(&FIFO_512x(port)->rximr, 0);
}

static void mpc512x_psc_cw_restore_ints(struct uart_port *port)
{
	out_be32(&FIFO_512x(port)->tximr,
		(port->read_status_mask >> 16) & 0x7f);
	out_be32(&FIFO_512x(port)->rximr, port->read_status_mask & 0x7f);
}

static unsigned int mpc512x_psc_set_baudrate(struct uart_port *port,
					     struct ktermios *new,
					     const struct ktermios *old)
{
	unsigned int baud;
	unsigned int divisor;

	 

	 
	baud = uart_get_baud_rate(port, new, old,
				  port->uartclk / (16 * 0xffff) + 1,
				  port->uartclk / 16);
	divisor = (port->uartclk + 8 * baud) / (16 * baud);

	 
	mpc52xx_set_divisor(PSC(port), 0xdd00, divisor);
	return baud;
}

 
static int __init mpc512x_psc_fifoc_init(void)
{
	int err;
	struct device_node *np;
	struct clk *clk;

	 
	err = -ENODEV;

	np = of_find_compatible_node(NULL, NULL,
				     "fsl,mpc5121-psc-fifo");
	if (!np) {
		pr_err("%s: Can't find FIFOC node\n", __func__);
		goto out_err;
	}

	clk = of_clk_get(np, 0);
	if (IS_ERR(clk)) {
		 
		clk = clk_get_sys(np->name, "ipg");
	}
	if (IS_ERR(clk)) {
		pr_err("%s: Can't lookup FIFO clock\n", __func__);
		err = PTR_ERR(clk);
		goto out_ofnode_put;
	}
	if (clk_prepare_enable(clk)) {
		pr_err("%s: Can't enable FIFO clock\n", __func__);
		clk_put(clk);
		goto out_ofnode_put;
	}
	psc_fifoc_clk = clk;

	psc_fifoc = of_iomap(np, 0);
	if (!psc_fifoc) {
		pr_err("%s: Can't map FIFOC\n", __func__);
		goto out_clk_disable;
	}

	psc_fifoc_irq = irq_of_parse_and_map(np, 0);
	if (psc_fifoc_irq == 0) {
		pr_err("%s: Can't get FIFOC irq\n", __func__);
		goto out_unmap;
	}

	of_node_put(np);
	return 0;

out_unmap:
	iounmap(psc_fifoc);
out_clk_disable:
	clk_disable_unprepare(psc_fifoc_clk);
	clk_put(psc_fifoc_clk);
out_ofnode_put:
	of_node_put(np);
out_err:
	return err;
}

static void __exit mpc512x_psc_fifoc_uninit(void)
{
	iounmap(psc_fifoc);

	 
	if (psc_fifoc_clk) {
		clk_disable_unprepare(psc_fifoc_clk);
		clk_put(psc_fifoc_clk);
		psc_fifoc_clk = NULL;
	}
}

 
static irqreturn_t mpc512x_psc_handle_irq(struct uart_port *port)
{
	unsigned long fifoc_int;
	int psc_num;

	 
	fifoc_int = in_be32(&psc_fifoc->fifoc_int);

	 
	psc_num = (port->mapbase & 0xf00) >> 8;
	if (test_bit(psc_num, &fifoc_int) ||
	    test_bit(psc_num + 16, &fifoc_int))
		return mpc5xxx_uart_process_int(port);

	return IRQ_NONE;
}

static struct clk *psc_mclk_clk[MPC52xx_PSC_MAXNUM];
static struct clk *psc_ipg_clk[MPC52xx_PSC_MAXNUM];

 
static int mpc512x_psc_alloc_clock(struct uart_port *port)
{
	int psc_num;
	struct clk *clk;
	int err;

	psc_num = (port->mapbase & 0xf00) >> 8;

	clk = devm_clk_get(port->dev, "mclk");
	if (IS_ERR(clk)) {
		dev_err(port->dev, "Failed to get MCLK!\n");
		err = PTR_ERR(clk);
		goto out_err;
	}
	err = clk_prepare_enable(clk);
	if (err) {
		dev_err(port->dev, "Failed to enable MCLK!\n");
		goto out_err;
	}
	psc_mclk_clk[psc_num] = clk;

	clk = devm_clk_get(port->dev, "ipg");
	if (IS_ERR(clk)) {
		dev_err(port->dev, "Failed to get IPG clock!\n");
		err = PTR_ERR(clk);
		goto out_err;
	}
	err = clk_prepare_enable(clk);
	if (err) {
		dev_err(port->dev, "Failed to enable IPG clock!\n");
		goto out_err;
	}
	psc_ipg_clk[psc_num] = clk;

	return 0;

out_err:
	if (psc_mclk_clk[psc_num]) {
		clk_disable_unprepare(psc_mclk_clk[psc_num]);
		psc_mclk_clk[psc_num] = NULL;
	}
	if (psc_ipg_clk[psc_num]) {
		clk_disable_unprepare(psc_ipg_clk[psc_num]);
		psc_ipg_clk[psc_num] = NULL;
	}
	return err;
}

 
static void mpc512x_psc_relse_clock(struct uart_port *port)
{
	int psc_num;
	struct clk *clk;

	psc_num = (port->mapbase & 0xf00) >> 8;
	clk = psc_mclk_clk[psc_num];
	if (clk) {
		clk_disable_unprepare(clk);
		psc_mclk_clk[psc_num] = NULL;
	}
	if (psc_ipg_clk[psc_num]) {
		clk_disable_unprepare(psc_ipg_clk[psc_num]);
		psc_ipg_clk[psc_num] = NULL;
	}
}

 
static int mpc512x_psc_endis_clock(struct uart_port *port, int enable)
{
	int psc_num;
	struct clk *psc_clk;
	int ret;

	if (uart_console(port))
		return 0;

	psc_num = (port->mapbase & 0xf00) >> 8;
	psc_clk = psc_mclk_clk[psc_num];
	if (!psc_clk) {
		dev_err(port->dev, "Failed to get PSC clock entry!\n");
		return -ENODEV;
	}

	dev_dbg(port->dev, "mclk %sable\n", enable ? "en" : "dis");
	if (enable) {
		ret = clk_enable(psc_clk);
		if (ret)
			dev_err(port->dev, "Failed to enable MCLK!\n");
		return ret;
	} else {
		clk_disable(psc_clk);
		return 0;
	}
}

static void mpc512x_psc_get_irq(struct uart_port *port, struct device_node *np)
{
	port->irqflags = IRQF_SHARED;
	port->irq = psc_fifoc_irq;
}

#define PSC_5125(port) ((struct mpc5125_psc __iomem *)((port)->membase))
#define FIFO_5125(port) ((struct mpc512x_psc_fifo __iomem *)(PSC_5125(port)+1))

static void mpc5125_psc_fifo_init(struct uart_port *port)
{
	 
	out_8(&PSC_5125(port)->mpc52xx_psc_clock_select, 0xdd);

	out_be32(&FIFO_5125(port)->txcmd, MPC512x_PSC_FIFO_RESET_SLICE);
	out_be32(&FIFO_5125(port)->txcmd, MPC512x_PSC_FIFO_ENABLE_SLICE);
	out_be32(&FIFO_5125(port)->txalarm, 1);
	out_be32(&FIFO_5125(port)->tximr, 0);

	out_be32(&FIFO_5125(port)->rxcmd, MPC512x_PSC_FIFO_RESET_SLICE);
	out_be32(&FIFO_5125(port)->rxcmd, MPC512x_PSC_FIFO_ENABLE_SLICE);
	out_be32(&FIFO_5125(port)->rxalarm, 1);
	out_be32(&FIFO_5125(port)->rximr, 0);

	out_be32(&FIFO_5125(port)->tximr, MPC512x_PSC_FIFO_ALARM);
	out_be32(&FIFO_5125(port)->rximr, MPC512x_PSC_FIFO_ALARM);
}

static unsigned int mpc5125_psc_raw_rx_rdy(struct uart_port *port)
{
	return !(in_be32(&FIFO_5125(port)->rxsr) & MPC512x_PSC_FIFO_EMPTY);
}

static unsigned int mpc5125_psc_raw_tx_rdy(struct uart_port *port)
{
	return !(in_be32(&FIFO_5125(port)->txsr) & MPC512x_PSC_FIFO_FULL);
}

static unsigned int mpc5125_psc_rx_rdy(struct uart_port *port)
{
	return in_be32(&FIFO_5125(port)->rxsr) &
	       in_be32(&FIFO_5125(port)->rximr) & MPC512x_PSC_FIFO_ALARM;
}

static unsigned int mpc5125_psc_tx_rdy(struct uart_port *port)
{
	return in_be32(&FIFO_5125(port)->txsr) &
	       in_be32(&FIFO_5125(port)->tximr) & MPC512x_PSC_FIFO_ALARM;
}

static unsigned int mpc5125_psc_tx_empty(struct uart_port *port)
{
	return in_be32(&FIFO_5125(port)->txsr) & MPC512x_PSC_FIFO_EMPTY;
}

static void mpc5125_psc_stop_rx(struct uart_port *port)
{
	unsigned long rx_fifo_imr;

	rx_fifo_imr = in_be32(&FIFO_5125(port)->rximr);
	rx_fifo_imr &= ~MPC512x_PSC_FIFO_ALARM;
	out_be32(&FIFO_5125(port)->rximr, rx_fifo_imr);
}

static void mpc5125_psc_start_tx(struct uart_port *port)
{
	unsigned long tx_fifo_imr;

	tx_fifo_imr = in_be32(&FIFO_5125(port)->tximr);
	tx_fifo_imr |= MPC512x_PSC_FIFO_ALARM;
	out_be32(&FIFO_5125(port)->tximr, tx_fifo_imr);
}

static void mpc5125_psc_stop_tx(struct uart_port *port)
{
	unsigned long tx_fifo_imr;

	tx_fifo_imr = in_be32(&FIFO_5125(port)->tximr);
	tx_fifo_imr &= ~MPC512x_PSC_FIFO_ALARM;
	out_be32(&FIFO_5125(port)->tximr, tx_fifo_imr);
}

static void mpc5125_psc_rx_clr_irq(struct uart_port *port)
{
	out_be32(&FIFO_5125(port)->rxisr, in_be32(&FIFO_5125(port)->rxisr));
}

static void mpc5125_psc_tx_clr_irq(struct uart_port *port)
{
	out_be32(&FIFO_5125(port)->txisr, in_be32(&FIFO_5125(port)->txisr));
}

static void mpc5125_psc_write_char(struct uart_port *port, unsigned char c)
{
	out_8(&FIFO_5125(port)->txdata_8, c);
}

static unsigned char mpc5125_psc_read_char(struct uart_port *port)
{
	return in_8(&FIFO_5125(port)->rxdata_8);
}

static void mpc5125_psc_cw_disable_ints(struct uart_port *port)
{
	port->read_status_mask =
		in_be32(&FIFO_5125(port)->tximr) << 16 |
		in_be32(&FIFO_5125(port)->rximr);
	out_be32(&FIFO_5125(port)->tximr, 0);
	out_be32(&FIFO_5125(port)->rximr, 0);
}

static void mpc5125_psc_cw_restore_ints(struct uart_port *port)
{
	out_be32(&FIFO_5125(port)->tximr,
		(port->read_status_mask >> 16) & 0x7f);
	out_be32(&FIFO_5125(port)->rximr, port->read_status_mask & 0x7f);
}

static inline void mpc5125_set_divisor(struct mpc5125_psc __iomem *psc,
		u8 prescaler, unsigned int divisor)
{
	 
	out_8(&psc->mpc52xx_psc_clock_select, prescaler);
	out_8(&psc->ctur, divisor >> 8);
	out_8(&psc->ctlr, divisor & 0xff);
}

static unsigned int mpc5125_psc_set_baudrate(struct uart_port *port,
					     struct ktermios *new,
					     const struct ktermios *old)
{
	unsigned int baud;
	unsigned int divisor;

	 

	 
	baud = uart_get_baud_rate(port, new, old,
				  port->uartclk / (16 * 0xffff) + 1,
				  port->uartclk / 16);
	divisor = (port->uartclk + 8 * baud) / (16 * baud);

	 
	mpc5125_set_divisor(PSC_5125(port), 0xdd, divisor);
	return baud;
}

 
static u16 mpc5125_psc_get_status(struct uart_port *port)
{
	return in_be16(&PSC_5125(port)->mpc52xx_psc_status);
}

static u8 mpc5125_psc_get_ipcr(struct uart_port *port)
{
	return in_8(&PSC_5125(port)->mpc52xx_psc_ipcr);
}

static void mpc5125_psc_command(struct uart_port *port, u8 cmd)
{
	out_8(&PSC_5125(port)->command, cmd);
}

static void mpc5125_psc_set_mode(struct uart_port *port, u8 mr1, u8 mr2)
{
	out_8(&PSC_5125(port)->mr1, mr1);
	out_8(&PSC_5125(port)->mr2, mr2);
}

static void mpc5125_psc_set_rts(struct uart_port *port, int state)
{
	if (state & TIOCM_RTS)
		out_8(&PSC_5125(port)->op1, MPC52xx_PSC_OP_RTS);
	else
		out_8(&PSC_5125(port)->op0, MPC52xx_PSC_OP_RTS);
}

static void mpc5125_psc_enable_ms(struct uart_port *port)
{
	struct mpc5125_psc __iomem *psc = PSC_5125(port);

	 
	in_8(&psc->mpc52xx_psc_ipcr);
	 
	out_8(&psc->mpc52xx_psc_acr, MPC52xx_PSC_IEC_CTS | MPC52xx_PSC_IEC_DCD);

	port->read_status_mask |= MPC52xx_PSC_IMR_IPC;
	out_be16(&psc->mpc52xx_psc_imr, port->read_status_mask);
}

static void mpc5125_psc_set_sicr(struct uart_port *port, u32 val)
{
	out_be32(&PSC_5125(port)->sicr, val);
}

static void mpc5125_psc_set_imr(struct uart_port *port, u16 val)
{
	out_be16(&PSC_5125(port)->mpc52xx_psc_imr, val);
}

static u8 mpc5125_psc_get_mr1(struct uart_port *port)
{
	return in_8(&PSC_5125(port)->mr1);
}

static const struct psc_ops mpc5125_psc_ops = {
	.fifo_init = mpc5125_psc_fifo_init,
	.raw_rx_rdy = mpc5125_psc_raw_rx_rdy,
	.raw_tx_rdy = mpc5125_psc_raw_tx_rdy,
	.rx_rdy = mpc5125_psc_rx_rdy,
	.tx_rdy = mpc5125_psc_tx_rdy,
	.tx_empty = mpc5125_psc_tx_empty,
	.stop_rx = mpc5125_psc_stop_rx,
	.start_tx = mpc5125_psc_start_tx,
	.stop_tx = mpc5125_psc_stop_tx,
	.rx_clr_irq = mpc5125_psc_rx_clr_irq,
	.tx_clr_irq = mpc5125_psc_tx_clr_irq,
	.write_char = mpc5125_psc_write_char,
	.read_char = mpc5125_psc_read_char,
	.cw_disable_ints = mpc5125_psc_cw_disable_ints,
	.cw_restore_ints = mpc5125_psc_cw_restore_ints,
	.set_baudrate = mpc5125_psc_set_baudrate,
	.clock_alloc = mpc512x_psc_alloc_clock,
	.clock_relse = mpc512x_psc_relse_clock,
	.clock = mpc512x_psc_endis_clock,
	.fifoc_init = mpc512x_psc_fifoc_init,
	.fifoc_uninit = mpc512x_psc_fifoc_uninit,
	.get_irq = mpc512x_psc_get_irq,
	.handle_irq = mpc512x_psc_handle_irq,
	.get_status = mpc5125_psc_get_status,
	.get_ipcr = mpc5125_psc_get_ipcr,
	.command = mpc5125_psc_command,
	.set_mode = mpc5125_psc_set_mode,
	.set_rts = mpc5125_psc_set_rts,
	.enable_ms = mpc5125_psc_enable_ms,
	.set_sicr = mpc5125_psc_set_sicr,
	.set_imr = mpc5125_psc_set_imr,
	.get_mr1 = mpc5125_psc_get_mr1,
};

static const struct psc_ops mpc512x_psc_ops = {
	.fifo_init = mpc512x_psc_fifo_init,
	.raw_rx_rdy = mpc512x_psc_raw_rx_rdy,
	.raw_tx_rdy = mpc512x_psc_raw_tx_rdy,
	.rx_rdy = mpc512x_psc_rx_rdy,
	.tx_rdy = mpc512x_psc_tx_rdy,
	.tx_empty = mpc512x_psc_tx_empty,
	.stop_rx = mpc512x_psc_stop_rx,
	.start_tx = mpc512x_psc_start_tx,
	.stop_tx = mpc512x_psc_stop_tx,
	.rx_clr_irq = mpc512x_psc_rx_clr_irq,
	.tx_clr_irq = mpc512x_psc_tx_clr_irq,
	.write_char = mpc512x_psc_write_char,
	.read_char = mpc512x_psc_read_char,
	.cw_disable_ints = mpc512x_psc_cw_disable_ints,
	.cw_restore_ints = mpc512x_psc_cw_restore_ints,
	.set_baudrate = mpc512x_psc_set_baudrate,
	.clock_alloc = mpc512x_psc_alloc_clock,
	.clock_relse = mpc512x_psc_relse_clock,
	.clock = mpc512x_psc_endis_clock,
	.fifoc_init = mpc512x_psc_fifoc_init,
	.fifoc_uninit = mpc512x_psc_fifoc_uninit,
	.get_irq = mpc512x_psc_get_irq,
	.handle_irq = mpc512x_psc_handle_irq,
	.get_status = mpc52xx_psc_get_status,
	.get_ipcr = mpc52xx_psc_get_ipcr,
	.command = mpc52xx_psc_command,
	.set_mode = mpc52xx_psc_set_mode,
	.set_rts = mpc52xx_psc_set_rts,
	.enable_ms = mpc52xx_psc_enable_ms,
	.set_sicr = mpc52xx_psc_set_sicr,
	.set_imr = mpc52xx_psc_set_imr,
	.get_mr1 = mpc52xx_psc_get_mr1,
};
#endif  


static const struct psc_ops *psc_ops;

 
 
 

static unsigned int
mpc52xx_uart_tx_empty(struct uart_port *port)
{
	return psc_ops->tx_empty(port) ? TIOCSER_TEMT : 0;
}

static void
mpc52xx_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	psc_ops->set_rts(port, mctrl & TIOCM_RTS);
}

static unsigned int
mpc52xx_uart_get_mctrl(struct uart_port *port)
{
	unsigned int ret = TIOCM_DSR;
	u8 status = psc_ops->get_ipcr(port);

	if (!(status & MPC52xx_PSC_CTS))
		ret |= TIOCM_CTS;
	if (!(status & MPC52xx_PSC_DCD))
		ret |= TIOCM_CAR;

	return ret;
}

static void
mpc52xx_uart_stop_tx(struct uart_port *port)
{
	 
	psc_ops->stop_tx(port);
}

static void
mpc52xx_uart_start_tx(struct uart_port *port)
{
	 
	psc_ops->start_tx(port);
}

static void
mpc52xx_uart_stop_rx(struct uart_port *port)
{
	 
	psc_ops->stop_rx(port);
}

static void
mpc52xx_uart_enable_ms(struct uart_port *port)
{
	psc_ops->enable_ms(port);
}

static void
mpc52xx_uart_break_ctl(struct uart_port *port, int ctl)
{
	unsigned long flags;
	spin_lock_irqsave(&port->lock, flags);

	if (ctl == -1)
		psc_ops->command(port, MPC52xx_PSC_START_BRK);
	else
		psc_ops->command(port, MPC52xx_PSC_STOP_BRK);

	spin_unlock_irqrestore(&port->lock, flags);
}

static int
mpc52xx_uart_startup(struct uart_port *port)
{
	int ret;

	if (psc_ops->clock) {
		ret = psc_ops->clock(port, 1);
		if (ret)
			return ret;
	}

	 
	ret = request_irq(port->irq, mpc52xx_uart_int,
			  port->irqflags, "mpc52xx_psc_uart", port);
	if (ret)
		return ret;

	 
	psc_ops->command(port, MPC52xx_PSC_RST_RX);
	psc_ops->command(port, MPC52xx_PSC_RST_TX);

	 
	msleep(1);

	psc_ops->set_sicr(port, 0);	 

	psc_ops->fifo_init(port);

	psc_ops->command(port, MPC52xx_PSC_TX_ENABLE);
	psc_ops->command(port, MPC52xx_PSC_RX_ENABLE);

	return 0;
}

static void
mpc52xx_uart_shutdown(struct uart_port *port)
{
	 
	psc_ops->command(port, MPC52xx_PSC_RST_RX);
	if (!uart_console(port))
		psc_ops->command(port, MPC52xx_PSC_RST_TX);

	port->read_status_mask = 0;
	psc_ops->set_imr(port, port->read_status_mask);

	if (psc_ops->clock)
		psc_ops->clock(port, 0);

	 
	psc_ops->cw_disable_ints(port);

	 
	free_irq(port->irq, port);
}

static void
mpc52xx_uart_set_termios(struct uart_port *port, struct ktermios *new,
			 const struct ktermios *old)
{
	unsigned long flags;
	unsigned char mr1, mr2;
	unsigned int j;
	unsigned int baud;

	 
	mr1 = 0;

	switch (new->c_cflag & CSIZE) {
	case CS5:	mr1 |= MPC52xx_PSC_MODE_5_BITS;
		break;
	case CS6:	mr1 |= MPC52xx_PSC_MODE_6_BITS;
		break;
	case CS7:	mr1 |= MPC52xx_PSC_MODE_7_BITS;
		break;
	case CS8:
	default:	mr1 |= MPC52xx_PSC_MODE_8_BITS;
	}

	if (new->c_cflag & PARENB) {
		if (new->c_cflag & CMSPAR)
			mr1 |= MPC52xx_PSC_MODE_PARFORCE;

		 
		mr1 |= (new->c_cflag & PARODD) ?
			MPC52xx_PSC_MODE_PARODD : MPC52xx_PSC_MODE_PAREVEN;
	} else {
		mr1 |= MPC52xx_PSC_MODE_PARNONE;
	}

	mr2 = 0;

	if (new->c_cflag & CSTOPB)
		mr2 |= MPC52xx_PSC_MODE_TWO_STOP;
	else
		mr2 |= ((new->c_cflag & CSIZE) == CS5) ?
			MPC52xx_PSC_MODE_ONE_STOP_5_BITS :
			MPC52xx_PSC_MODE_ONE_STOP;

	if (new->c_cflag & CRTSCTS) {
		mr1 |= MPC52xx_PSC_MODE_RXRTS;
		mr2 |= MPC52xx_PSC_MODE_TXCTS;
	}

	 
	spin_lock_irqsave(&port->lock, flags);

	 
	 
	j = 5000000;	 
	 
	 
	while (!mpc52xx_uart_tx_empty(port) && --j)
		udelay(1);

	if (!j)
		printk(KERN_ERR "mpc52xx_uart.c: "
			"Unable to flush RX & TX fifos in-time in set_termios."
			"Some chars may have been lost.\n");

	 
	psc_ops->command(port, MPC52xx_PSC_RST_RX);
	psc_ops->command(port, MPC52xx_PSC_RST_TX);

	 
	psc_ops->set_mode(port, mr1, mr2);
	baud = psc_ops->set_baudrate(port, new, old);

	 
	uart_update_timeout(port, new->c_cflag, baud);

	if (UART_ENABLE_MS(port, new->c_cflag))
		mpc52xx_uart_enable_ms(port);

	 
	psc_ops->command(port, MPC52xx_PSC_TX_ENABLE);
	psc_ops->command(port, MPC52xx_PSC_RX_ENABLE);

	 
	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *
mpc52xx_uart_type(struct uart_port *port)
{
	 
	return port->type == PORT_MPC52xx ? "MPC5xxx PSC" : NULL;
}

static void
mpc52xx_uart_release_port(struct uart_port *port)
{
	if (psc_ops->clock_relse)
		psc_ops->clock_relse(port);

	 
	if (port->flags & UPF_IOREMAP) {
		iounmap(port->membase);
		port->membase = NULL;
	}

	release_mem_region(port->mapbase, sizeof(struct mpc52xx_psc));
}

static int
mpc52xx_uart_request_port(struct uart_port *port)
{
	int err;

	if (port->flags & UPF_IOREMAP)  
		port->membase = ioremap(port->mapbase,
					sizeof(struct mpc52xx_psc));

	if (!port->membase)
		return -EINVAL;

	err = request_mem_region(port->mapbase, sizeof(struct mpc52xx_psc),
			"mpc52xx_psc_uart") != NULL ? 0 : -EBUSY;

	if (err)
		goto out_membase;

	if (psc_ops->clock_alloc) {
		err = psc_ops->clock_alloc(port);
		if (err)
			goto out_mapregion;
	}

	return 0;

out_mapregion:
	release_mem_region(port->mapbase, sizeof(struct mpc52xx_psc));
out_membase:
	if (port->flags & UPF_IOREMAP) {
		iounmap(port->membase);
		port->membase = NULL;
	}
	return err;
}

static void
mpc52xx_uart_config_port(struct uart_port *port, int flags)
{
	if ((flags & UART_CONFIG_TYPE)
		&& (mpc52xx_uart_request_port(port) == 0))
		port->type = PORT_MPC52xx;
}

static int
mpc52xx_uart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_MPC52xx)
		return -EINVAL;

	if ((ser->irq != port->irq) ||
	    (ser->io_type != UPIO_MEM) ||
	    (ser->baud_base != port->uartclk)  ||
	    (ser->iomem_base != (void *)port->mapbase) ||
	    (ser->hub6 != 0))
		return -EINVAL;

	return 0;
}


static const struct uart_ops mpc52xx_uart_ops = {
	.tx_empty	= mpc52xx_uart_tx_empty,
	.set_mctrl	= mpc52xx_uart_set_mctrl,
	.get_mctrl	= mpc52xx_uart_get_mctrl,
	.stop_tx	= mpc52xx_uart_stop_tx,
	.start_tx	= mpc52xx_uart_start_tx,
	.stop_rx	= mpc52xx_uart_stop_rx,
	.enable_ms	= mpc52xx_uart_enable_ms,
	.break_ctl	= mpc52xx_uart_break_ctl,
	.startup	= mpc52xx_uart_startup,
	.shutdown	= mpc52xx_uart_shutdown,
	.set_termios	= mpc52xx_uart_set_termios,
 
	.type		= mpc52xx_uart_type,
	.release_port	= mpc52xx_uart_release_port,
	.request_port	= mpc52xx_uart_request_port,
	.config_port	= mpc52xx_uart_config_port,
	.verify_port	= mpc52xx_uart_verify_port
};


 
 
 

static inline bool
mpc52xx_uart_int_rx_chars(struct uart_port *port)
{
	struct tty_port *tport = &port->state->port;
	unsigned char ch, flag;
	unsigned short status;

	 
	while (psc_ops->raw_rx_rdy(port)) {
		 
		ch = psc_ops->read_char(port);

		 
		if (uart_handle_sysrq_char(port, ch))
			continue;

		 

		flag = TTY_NORMAL;
		port->icount.rx++;

		status = psc_ops->get_status(port);

		if (status & (MPC52xx_PSC_SR_PE |
			      MPC52xx_PSC_SR_FE |
			      MPC52xx_PSC_SR_RB)) {

			if (status & MPC52xx_PSC_SR_RB) {
				flag = TTY_BREAK;
				uart_handle_break(port);
				port->icount.brk++;
			} else if (status & MPC52xx_PSC_SR_PE) {
				flag = TTY_PARITY;
				port->icount.parity++;
			}
			else if (status & MPC52xx_PSC_SR_FE) {
				flag = TTY_FRAME;
				port->icount.frame++;
			}

			 
			psc_ops->command(port, MPC52xx_PSC_RST_ERR_STAT);

		}
		tty_insert_flip_char(tport, ch, flag);
		if (status & MPC52xx_PSC_SR_OE) {
			 
			tty_insert_flip_char(tport, 0, TTY_OVERRUN);
			port->icount.overrun++;
		}
	}

	tty_flip_buffer_push(tport);

	return psc_ops->raw_rx_rdy(port);
}

static inline bool
mpc52xx_uart_int_tx_chars(struct uart_port *port)
{
	u8 ch;

	return uart_port_tx(port, ch,
		psc_ops->raw_tx_rdy(port),
		psc_ops->write_char(port, ch));
}

static irqreturn_t
mpc5xxx_uart_process_int(struct uart_port *port)
{
	unsigned long pass = ISR_PASS_LIMIT;
	bool keepgoing;
	u8 status;

	 
	do {
		 
		keepgoing = false;

		psc_ops->rx_clr_irq(port);
		if (psc_ops->rx_rdy(port))
			keepgoing |= mpc52xx_uart_int_rx_chars(port);

		psc_ops->tx_clr_irq(port);
		if (psc_ops->tx_rdy(port))
			keepgoing |= mpc52xx_uart_int_tx_chars(port);

		status = psc_ops->get_ipcr(port);
		if (status & MPC52xx_PSC_D_DCD)
			uart_handle_dcd_change(port, !(status & MPC52xx_PSC_DCD));

		if (status & MPC52xx_PSC_D_CTS)
			uart_handle_cts_change(port, !(status & MPC52xx_PSC_CTS));

		 
		if (!(--pass))
			keepgoing = false;

	} while (keepgoing);

	return IRQ_HANDLED;
}

static irqreturn_t
mpc52xx_uart_int(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	irqreturn_t ret;

	spin_lock(&port->lock);

	ret = psc_ops->handle_irq(port);

	spin_unlock(&port->lock);

	return ret;
}

 
 
 

#ifdef CONFIG_SERIAL_MPC52xx_CONSOLE

static void __init
mpc52xx_console_get_options(struct uart_port *port,
			    int *baud, int *parity, int *bits, int *flow)
{
	unsigned char mr1;

	pr_debug("mpc52xx_console_get_options(port=%p)\n", port);

	 
	mr1 = psc_ops->get_mr1(port);

	 
	*baud = CONFIG_SERIAL_MPC52xx_CONSOLE_BAUD;

	 
	switch (mr1 & MPC52xx_PSC_MODE_BITS_MASK) {
	case MPC52xx_PSC_MODE_5_BITS:
		*bits = 5;
		break;
	case MPC52xx_PSC_MODE_6_BITS:
		*bits = 6;
		break;
	case MPC52xx_PSC_MODE_7_BITS:
		*bits = 7;
		break;
	case MPC52xx_PSC_MODE_8_BITS:
	default:
		*bits = 8;
	}

	if (mr1 & MPC52xx_PSC_MODE_PARNONE)
		*parity = 'n';
	else
		*parity = mr1 & MPC52xx_PSC_MODE_PARODD ? 'o' : 'e';
}

static void
mpc52xx_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_port *port = &mpc52xx_uart_ports[co->index];
	unsigned int i, j;

	 
	psc_ops->cw_disable_ints(port);

	 
	j = 5000000;	 
	while (!mpc52xx_uart_tx_empty(port) && --j)
		udelay(1);

	 
	for (i = 0; i < count; i++, s++) {
		 
		if (*s == '\n')
			psc_ops->write_char(port, '\r');

		 
		psc_ops->write_char(port, *s);

		 
		j = 20000;	 
		while (!mpc52xx_uart_tx_empty(port) && --j)
			udelay(1);
	}

	 
	psc_ops->cw_restore_ints(port);
}


static int __init
mpc52xx_console_setup(struct console *co, char *options)
{
	struct uart_port *port = &mpc52xx_uart_ports[co->index];
	struct device_node *np = mpc52xx_uart_nodes[co->index];
	unsigned int uartclk;
	struct resource res;
	int ret;

	int baud = CONFIG_SERIAL_MPC52xx_CONSOLE_BAUD;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	pr_debug("mpc52xx_console_setup co=%p, co->index=%i, options=%s\n",
		 co, co->index, options);

	if ((co->index < 0) || (co->index >= MPC52xx_PSC_MAXNUM)) {
		pr_debug("PSC%x out of range\n", co->index);
		return -EINVAL;
	}

	if (!np) {
		pr_debug("PSC%x not found in device tree\n", co->index);
		return -EINVAL;
	}

	pr_debug("Console on ttyPSC%x is %pOF\n",
		 co->index, mpc52xx_uart_nodes[co->index]);

	 
	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		pr_debug("Could not get resources for PSC%x\n", co->index);
		return ret;
	}

	uartclk = mpc5xxx_fwnode_get_bus_frequency(of_fwnode_handle(np));
	if (uartclk == 0) {
		pr_debug("Could not find uart clock frequency!\n");
		return -EINVAL;
	}

	 
	spin_lock_init(&port->lock);
	port->uartclk = uartclk;
	port->ops	= &mpc52xx_uart_ops;
	port->mapbase = res.start;
	port->membase = ioremap(res.start, sizeof(struct mpc52xx_psc));
	port->irq = irq_of_parse_and_map(np, 0);

	if (port->membase == NULL)
		return -EINVAL;

	pr_debug("mpc52xx-psc uart at %p, mapped to %p, irq=%x, freq=%i\n",
		 (void *)port->mapbase, port->membase,
		 port->irq, port->uartclk);

	 
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		mpc52xx_console_get_options(port, &baud, &parity, &bits, &flow);

	pr_debug("Setting console parameters: %i %i%c1 flow=%c\n",
		 baud, bits, parity, flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}


static struct uart_driver mpc52xx_uart_driver;

static struct console mpc52xx_console = {
	.name	= "ttyPSC",
	.write	= mpc52xx_console_write,
	.device	= uart_console_device,
	.setup	= mpc52xx_console_setup,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,	 
	.data	= &mpc52xx_uart_driver,
};


static int __init
mpc52xx_console_init(void)
{
	mpc52xx_uart_of_enumerate();
	register_console(&mpc52xx_console);
	return 0;
}

console_initcall(mpc52xx_console_init);

#define MPC52xx_PSC_CONSOLE &mpc52xx_console
#else
#define MPC52xx_PSC_CONSOLE NULL
#endif


 
 
 

static struct uart_driver mpc52xx_uart_driver = {
	.driver_name	= "mpc52xx_psc_uart",
	.dev_name	= "ttyPSC",
	.major		= SERIAL_PSC_MAJOR,
	.minor		= SERIAL_PSC_MINOR,
	.nr		= MPC52xx_PSC_MAXNUM,
	.cons		= MPC52xx_PSC_CONSOLE,
};

 
 
 

static const struct of_device_id mpc52xx_uart_of_match[] = {
#ifdef CONFIG_PPC_MPC52xx
	{ .compatible = "fsl,mpc5200b-psc-uart", .data = &mpc5200b_psc_ops, },
	{ .compatible = "fsl,mpc5200-psc-uart", .data = &mpc52xx_psc_ops, },
	 
	{ .compatible = "mpc5200-psc-uart", .data = &mpc52xx_psc_ops, },
	 
	{ .compatible = "mpc5200-serial", .data = &mpc52xx_psc_ops, },
#endif
#ifdef CONFIG_PPC_MPC512x
	{ .compatible = "fsl,mpc5121-psc-uart", .data = &mpc512x_psc_ops, },
	{ .compatible = "fsl,mpc5125-psc-uart", .data = &mpc5125_psc_ops, },
#endif
	{},
};

static int mpc52xx_uart_of_probe(struct platform_device *op)
{
	int idx = -1;
	unsigned int uartclk;
	struct uart_port *port = NULL;
	struct resource res;
	int ret;

	 
	for (idx = 0; idx < MPC52xx_PSC_MAXNUM; idx++)
		if (mpc52xx_uart_nodes[idx] == op->dev.of_node)
			break;
	if (idx >= MPC52xx_PSC_MAXNUM)
		return -EINVAL;
	pr_debug("Found %pOF assigned to ttyPSC%x\n",
		 mpc52xx_uart_nodes[idx], idx);

	 
	uartclk = mpc5xxx_get_bus_frequency(&op->dev);
	if (uartclk == 0) {
		dev_dbg(&op->dev, "Could not find uart clock frequency!\n");
		return -EINVAL;
	}

	 
	port = &mpc52xx_uart_ports[idx];

	spin_lock_init(&port->lock);
	port->uartclk = uartclk;
	port->fifosize	= 512;
	port->has_sysrq = IS_ENABLED(CONFIG_SERIAL_MPC52xx_CONSOLE);
	port->iotype	= UPIO_MEM;
	port->flags	= UPF_BOOT_AUTOCONF |
			  (uart_console(port) ? 0 : UPF_IOREMAP);
	port->line	= idx;
	port->ops	= &mpc52xx_uart_ops;
	port->dev	= &op->dev;

	 
	ret = of_address_to_resource(op->dev.of_node, 0, &res);
	if (ret)
		return ret;

	port->mapbase = res.start;
	if (!port->mapbase) {
		dev_dbg(&op->dev, "Could not allocate resources for PSC\n");
		return -EINVAL;
	}

	psc_ops->get_irq(port, op->dev.of_node);
	if (port->irq == 0) {
		dev_dbg(&op->dev, "Could not get irq\n");
		return -EINVAL;
	}

	dev_dbg(&op->dev, "mpc52xx-psc uart at %p, irq=%x, freq=%i\n",
		(void *)port->mapbase, port->irq, port->uartclk);

	 
	ret = uart_add_one_port(&mpc52xx_uart_driver, port);
	if (ret)
		return ret;

	platform_set_drvdata(op, (void *)port);
	return 0;
}

static int
mpc52xx_uart_of_remove(struct platform_device *op)
{
	struct uart_port *port = platform_get_drvdata(op);

	if (port)
		uart_remove_one_port(&mpc52xx_uart_driver, port);

	return 0;
}

#ifdef CONFIG_PM
static int
mpc52xx_uart_of_suspend(struct platform_device *op, pm_message_t state)
{
	struct uart_port *port = platform_get_drvdata(op);

	if (port)
		uart_suspend_port(&mpc52xx_uart_driver, port);

	return 0;
}

static int
mpc52xx_uart_of_resume(struct platform_device *op)
{
	struct uart_port *port = platform_get_drvdata(op);

	if (port)
		uart_resume_port(&mpc52xx_uart_driver, port);

	return 0;
}
#endif

static void
mpc52xx_uart_of_assign(struct device_node *np)
{
	int i;

	 
	for (i = 0; i < MPC52xx_PSC_MAXNUM; i++) {
		if (mpc52xx_uart_nodes[i] == NULL) {
			of_node_get(np);
			mpc52xx_uart_nodes[i] = np;
			return;
		}
	}
}

static void
mpc52xx_uart_of_enumerate(void)
{
	static int enum_done;
	struct device_node *np;
	const struct  of_device_id *match;
	int i;

	if (enum_done)
		return;

	 
	for_each_matching_node(np, mpc52xx_uart_of_match) {
		match = of_match_node(mpc52xx_uart_of_match, np);
		psc_ops = match->data;
		mpc52xx_uart_of_assign(np);
	}

	enum_done = 1;

	for (i = 0; i < MPC52xx_PSC_MAXNUM; i++) {
		if (mpc52xx_uart_nodes[i])
			pr_debug("%pOF assigned to ttyPSC%x\n",
				 mpc52xx_uart_nodes[i], i);
	}
}

MODULE_DEVICE_TABLE(of, mpc52xx_uart_of_match);

static struct platform_driver mpc52xx_uart_of_driver = {
	.probe		= mpc52xx_uart_of_probe,
	.remove		= mpc52xx_uart_of_remove,
#ifdef CONFIG_PM
	.suspend	= mpc52xx_uart_of_suspend,
	.resume		= mpc52xx_uart_of_resume,
#endif
	.driver = {
		.name = "mpc52xx-psc-uart",
		.of_match_table = mpc52xx_uart_of_match,
	},
};


 
 
 

static int __init
mpc52xx_uart_init(void)
{
	int ret;

	printk(KERN_INFO "Serial: MPC52xx PSC UART driver\n");

	ret = uart_register_driver(&mpc52xx_uart_driver);
	if (ret) {
		printk(KERN_ERR "%s: uart_register_driver failed (%i)\n",
		       __FILE__, ret);
		return ret;
	}

	mpc52xx_uart_of_enumerate();

	 
	if (psc_ops && psc_ops->fifoc_init) {
		ret = psc_ops->fifoc_init();
		if (ret)
			goto err_init;
	}

	ret = platform_driver_register(&mpc52xx_uart_of_driver);
	if (ret) {
		printk(KERN_ERR "%s: platform_driver_register failed (%i)\n",
		       __FILE__, ret);
		goto err_reg;
	}

	return 0;
err_reg:
	if (psc_ops && psc_ops->fifoc_uninit)
		psc_ops->fifoc_uninit();
err_init:
	uart_unregister_driver(&mpc52xx_uart_driver);
	return ret;
}

static void __exit
mpc52xx_uart_exit(void)
{
	if (psc_ops->fifoc_uninit)
		psc_ops->fifoc_uninit();

	platform_driver_unregister(&mpc52xx_uart_of_driver);
	uart_unregister_driver(&mpc52xx_uart_driver);
}


module_init(mpc52xx_uart_init);
module_exit(mpc52xx_uart_exit);

MODULE_AUTHOR("Sylvain Munaut <tnt@246tNt.com>");
MODULE_DESCRIPTION("Freescale MPC52xx PSC UART");
MODULE_LICENSE("GPL");
