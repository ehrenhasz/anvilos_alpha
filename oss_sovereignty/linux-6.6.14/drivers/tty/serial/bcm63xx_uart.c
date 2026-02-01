
 

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/clk.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sysrq.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/serial_bcm63xx.h>
#include <linux/io.h>
#include <linux/of.h>

#define BCM63XX_NR_UARTS	2

static struct uart_port ports[BCM63XX_NR_UARTS];

 
#define UART_RX_INT_MASK	(UART_IR_MASK(UART_IR_RXOVER) |		\
				UART_IR_MASK(UART_IR_RXTHRESH) |	\
				UART_IR_MASK(UART_IR_RXTIMEOUT))

#define UART_RX_INT_STAT	(UART_IR_STAT(UART_IR_RXOVER) |		\
				UART_IR_STAT(UART_IR_RXTHRESH) |	\
				UART_IR_STAT(UART_IR_RXTIMEOUT))

 
#define UART_TX_INT_MASK	(UART_IR_MASK(UART_IR_TXEMPTY) |	\
				UART_IR_MASK(UART_IR_TXTRESH))

#define UART_TX_INT_STAT	(UART_IR_STAT(UART_IR_TXEMPTY) |	\
				UART_IR_STAT(UART_IR_TXTRESH))

 
#define UART_EXTINP_INT_MASK	(UART_EXTINP_IRMASK(UART_EXTINP_IR_CTS) | \
				 UART_EXTINP_IRMASK(UART_EXTINP_IR_DCD))

 
static inline unsigned int bcm_uart_readl(struct uart_port *port,
					 unsigned int offset)
{
	return __raw_readl(port->membase + offset);
}

static inline void bcm_uart_writel(struct uart_port *port,
				  unsigned int value, unsigned int offset)
{
	__raw_writel(value, port->membase + offset);
}

 
static unsigned int bcm_uart_tx_empty(struct uart_port *port)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_IR_REG);
	return (val & UART_IR_STAT(UART_IR_TXEMPTY)) ? 1 : 0;
}

 
static void bcm_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_MCTL_REG);
	val &= ~(UART_MCTL_DTR_MASK | UART_MCTL_RTS_MASK);
	 
	if (!(mctrl & TIOCM_DTR))
		val |= UART_MCTL_DTR_MASK;
	if (!(mctrl & TIOCM_RTS))
		val |= UART_MCTL_RTS_MASK;
	bcm_uart_writel(port, val, UART_MCTL_REG);

	val = bcm_uart_readl(port, UART_CTL_REG);
	if (mctrl & TIOCM_LOOP)
		val |= UART_CTL_LOOPBACK_MASK;
	else
		val &= ~UART_CTL_LOOPBACK_MASK;
	bcm_uart_writel(port, val, UART_CTL_REG);
}

 
static unsigned int bcm_uart_get_mctrl(struct uart_port *port)
{
	unsigned int val, mctrl;

	mctrl = 0;
	val = bcm_uart_readl(port, UART_EXTINP_REG);
	if (val & UART_EXTINP_RI_MASK)
		mctrl |= TIOCM_RI;
	if (val & UART_EXTINP_CTS_MASK)
		mctrl |= TIOCM_CTS;
	if (val & UART_EXTINP_DCD_MASK)
		mctrl |= TIOCM_CD;
	if (val & UART_EXTINP_DSR_MASK)
		mctrl |= TIOCM_DSR;
	return mctrl;
}

 
static void bcm_uart_stop_tx(struct uart_port *port)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_CTL_REG);
	val &= ~(UART_CTL_TXEN_MASK);
	bcm_uart_writel(port, val, UART_CTL_REG);

	val = bcm_uart_readl(port, UART_IR_REG);
	val &= ~UART_TX_INT_MASK;
	bcm_uart_writel(port, val, UART_IR_REG);
}

 
static void bcm_uart_start_tx(struct uart_port *port)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_IR_REG);
	val |= UART_TX_INT_MASK;
	bcm_uart_writel(port, val, UART_IR_REG);

	val = bcm_uart_readl(port, UART_CTL_REG);
	val |= UART_CTL_TXEN_MASK;
	bcm_uart_writel(port, val, UART_CTL_REG);
}

 
static void bcm_uart_stop_rx(struct uart_port *port)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_IR_REG);
	val &= ~UART_RX_INT_MASK;
	bcm_uart_writel(port, val, UART_IR_REG);
}

 
static void bcm_uart_enable_ms(struct uart_port *port)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_IR_REG);
	val |= UART_IR_MASK(UART_IR_EXTIP);
	bcm_uart_writel(port, val, UART_IR_REG);
}

 
static void bcm_uart_break_ctl(struct uart_port *port, int ctl)
{
	unsigned long flags;
	unsigned int val;

	spin_lock_irqsave(&port->lock, flags);

	val = bcm_uart_readl(port, UART_CTL_REG);
	if (ctl)
		val |= UART_CTL_XMITBRK_MASK;
	else
		val &= ~UART_CTL_XMITBRK_MASK;
	bcm_uart_writel(port, val, UART_CTL_REG);

	spin_unlock_irqrestore(&port->lock, flags);
}

 
static const char *bcm_uart_type(struct uart_port *port)
{
	return (port->type == PORT_BCM63XX) ? "bcm63xx_uart" : NULL;
}

 
static void bcm_uart_do_rx(struct uart_port *port)
{
	struct tty_port *tty_port = &port->state->port;
	unsigned int max_count;

	 
	max_count = 32;
	do {
		unsigned int iestat, c, cstat;
		char flag;

		 
		iestat = bcm_uart_readl(port, UART_IR_REG);

		if (unlikely(iestat & UART_IR_STAT(UART_IR_RXOVER))) {
			unsigned int val;

			 
			val = bcm_uart_readl(port, UART_CTL_REG);
			val |= UART_CTL_RSTRXFIFO_MASK;
			bcm_uart_writel(port, val, UART_CTL_REG);

			port->icount.overrun++;
			tty_insert_flip_char(tty_port, 0, TTY_OVERRUN);
		}

		if (!(iestat & UART_IR_STAT(UART_IR_RXNOTEMPTY)))
			break;

		cstat = c = bcm_uart_readl(port, UART_FIFO_REG);
		port->icount.rx++;
		flag = TTY_NORMAL;
		c &= 0xff;

		if (unlikely((cstat & UART_FIFO_ANYERR_MASK))) {
			 
			if (cstat & UART_FIFO_BRKDET_MASK) {
				port->icount.brk++;
				if (uart_handle_break(port))
					continue;
			}

			if (cstat & UART_FIFO_PARERR_MASK)
				port->icount.parity++;
			if (cstat & UART_FIFO_FRAMEERR_MASK)
				port->icount.frame++;

			 
			cstat &= port->read_status_mask;
			if (cstat & UART_FIFO_BRKDET_MASK)
				flag = TTY_BREAK;
			if (cstat & UART_FIFO_FRAMEERR_MASK)
				flag = TTY_FRAME;
			if (cstat & UART_FIFO_PARERR_MASK)
				flag = TTY_PARITY;
		}

		if (uart_handle_sysrq_char(port, c))
			continue;


		if ((cstat & port->ignore_status_mask) == 0)
			tty_insert_flip_char(tty_port, c, flag);

	} while (--max_count);

	tty_flip_buffer_push(tty_port);
}

 
static void bcm_uart_do_tx(struct uart_port *port)
{
	unsigned int val;
	bool pending;
	u8 ch;

	val = bcm_uart_readl(port, UART_MCTL_REG);
	val = (val & UART_MCTL_TXFIFOFILL_MASK) >> UART_MCTL_TXFIFOFILL_SHIFT;

	pending = uart_port_tx_limited(port, ch, port->fifosize - val,
		true,
		bcm_uart_writel(port, ch, UART_FIFO_REG),
		({}));
	if (pending)
		return;

	 
	val = bcm_uart_readl(port, UART_IR_REG);
	val &= ~UART_TX_INT_MASK;
	bcm_uart_writel(port, val, UART_IR_REG);
}

 
static irqreturn_t bcm_uart_interrupt(int irq, void *dev_id)
{
	struct uart_port *port;
	unsigned int irqstat;

	port = dev_id;
	spin_lock(&port->lock);

	irqstat = bcm_uart_readl(port, UART_IR_REG);
	if (irqstat & UART_RX_INT_STAT)
		bcm_uart_do_rx(port);

	if (irqstat & UART_TX_INT_STAT)
		bcm_uart_do_tx(port);

	if (irqstat & UART_IR_MASK(UART_IR_EXTIP)) {
		unsigned int estat;

		estat = bcm_uart_readl(port, UART_EXTINP_REG);
		if (estat & UART_EXTINP_IRSTAT(UART_EXTINP_IR_CTS))
			uart_handle_cts_change(port,
					       estat & UART_EXTINP_CTS_MASK);
		if (estat & UART_EXTINP_IRSTAT(UART_EXTINP_IR_DCD))
			uart_handle_dcd_change(port,
					       estat & UART_EXTINP_DCD_MASK);
	}

	spin_unlock(&port->lock);
	return IRQ_HANDLED;
}

 
static void bcm_uart_enable(struct uart_port *port)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_CTL_REG);
	val |= (UART_CTL_BRGEN_MASK | UART_CTL_TXEN_MASK | UART_CTL_RXEN_MASK);
	bcm_uart_writel(port, val, UART_CTL_REG);
}

 
static void bcm_uart_disable(struct uart_port *port)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_CTL_REG);
	val &= ~(UART_CTL_BRGEN_MASK | UART_CTL_TXEN_MASK |
		 UART_CTL_RXEN_MASK);
	bcm_uart_writel(port, val, UART_CTL_REG);
}

 
static void bcm_uart_flush(struct uart_port *port)
{
	unsigned int val;

	 
	val = bcm_uart_readl(port, UART_CTL_REG);
	val |= UART_CTL_RSTRXFIFO_MASK | UART_CTL_RSTTXFIFO_MASK;
	bcm_uart_writel(port, val, UART_CTL_REG);

	 
	(void)bcm_uart_readl(port, UART_FIFO_REG);
}

 
static int bcm_uart_startup(struct uart_port *port)
{
	unsigned int val;
	int ret;

	 
	bcm_uart_disable(port);
	bcm_uart_writel(port, 0, UART_IR_REG);
	bcm_uart_flush(port);

	 
	(void)bcm_uart_readl(port, UART_EXTINP_REG);

	 
	val = bcm_uart_readl(port, UART_MCTL_REG);
	val &= ~(UART_MCTL_RXFIFOTHRESH_MASK | UART_MCTL_TXFIFOTHRESH_MASK);
	val |= (port->fifosize / 2) << UART_MCTL_RXFIFOTHRESH_SHIFT;
	val |= (port->fifosize / 2) << UART_MCTL_TXFIFOTHRESH_SHIFT;
	bcm_uart_writel(port, val, UART_MCTL_REG);

	 
	val = bcm_uart_readl(port, UART_CTL_REG);
	val &= ~UART_CTL_RXTMOUTCNT_MASK;
	val |= 1 << UART_CTL_RXTMOUTCNT_SHIFT;
	bcm_uart_writel(port, val, UART_CTL_REG);

	 
	val = UART_EXTINP_INT_MASK;
	val |= UART_EXTINP_DCD_NOSENSE_MASK;
	val |= UART_EXTINP_CTS_NOSENSE_MASK;
	bcm_uart_writel(port, val, UART_EXTINP_REG);

	 
	ret = request_irq(port->irq, bcm_uart_interrupt, 0,
			  dev_name(port->dev), port);
	if (ret)
		return ret;
	bcm_uart_writel(port, UART_RX_INT_MASK, UART_IR_REG);
	bcm_uart_enable(port);
	return 0;
}

 
static void bcm_uart_shutdown(struct uart_port *port)
{
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	bcm_uart_writel(port, 0, UART_IR_REG);
	spin_unlock_irqrestore(&port->lock, flags);

	bcm_uart_disable(port);
	bcm_uart_flush(port);
	free_irq(port->irq, port);
}

 
static void bcm_uart_set_termios(struct uart_port *port, struct ktermios *new,
				 const struct ktermios *old)
{
	unsigned int ctl, baud, quot, ier;
	unsigned long flags;
	int tries;

	spin_lock_irqsave(&port->lock, flags);

	 
	for (tries = 3; !bcm_uart_tx_empty(port) && tries; tries--)
		mdelay(10);

	 
	bcm_uart_disable(port);
	bcm_uart_flush(port);

	 
	ctl = bcm_uart_readl(port, UART_CTL_REG);
	ctl &= ~UART_CTL_BITSPERSYM_MASK;

	switch (new->c_cflag & CSIZE) {
	case CS5:
		ctl |= (0 << UART_CTL_BITSPERSYM_SHIFT);
		break;
	case CS6:
		ctl |= (1 << UART_CTL_BITSPERSYM_SHIFT);
		break;
	case CS7:
		ctl |= (2 << UART_CTL_BITSPERSYM_SHIFT);
		break;
	default:
		ctl |= (3 << UART_CTL_BITSPERSYM_SHIFT);
		break;
	}

	ctl &= ~UART_CTL_STOPBITS_MASK;
	if (new->c_cflag & CSTOPB)
		ctl |= UART_CTL_STOPBITS_2;
	else
		ctl |= UART_CTL_STOPBITS_1;

	ctl &= ~(UART_CTL_RXPAREN_MASK | UART_CTL_TXPAREN_MASK);
	if (new->c_cflag & PARENB)
		ctl |= (UART_CTL_RXPAREN_MASK | UART_CTL_TXPAREN_MASK);
	ctl &= ~(UART_CTL_RXPAREVEN_MASK | UART_CTL_TXPAREVEN_MASK);
	if (new->c_cflag & PARODD)
		ctl |= (UART_CTL_RXPAREVEN_MASK | UART_CTL_TXPAREVEN_MASK);
	bcm_uart_writel(port, ctl, UART_CTL_REG);

	 
	baud = uart_get_baud_rate(port, new, old, 0, port->uartclk / 16);
	quot = uart_get_divisor(port, baud) - 1;
	bcm_uart_writel(port, quot, UART_BAUD_REG);

	 
	ier = bcm_uart_readl(port, UART_IR_REG);

	ier &= ~UART_IR_MASK(UART_IR_EXTIP);
	if (UART_ENABLE_MS(port, new->c_cflag))
		ier |= UART_IR_MASK(UART_IR_EXTIP);

	bcm_uart_writel(port, ier, UART_IR_REG);

	 
	port->read_status_mask = UART_FIFO_VALID_MASK;
	if (new->c_iflag & INPCK) {
		port->read_status_mask |= UART_FIFO_FRAMEERR_MASK;
		port->read_status_mask |= UART_FIFO_PARERR_MASK;
	}
	if (new->c_iflag & (IGNBRK | BRKINT))
		port->read_status_mask |= UART_FIFO_BRKDET_MASK;

	port->ignore_status_mask = 0;
	if (new->c_iflag & IGNPAR)
		port->ignore_status_mask |= UART_FIFO_PARERR_MASK;
	if (new->c_iflag & IGNBRK)
		port->ignore_status_mask |= UART_FIFO_BRKDET_MASK;
	if (!(new->c_cflag & CREAD))
		port->ignore_status_mask |= UART_FIFO_VALID_MASK;

	uart_update_timeout(port, new->c_cflag, baud);
	bcm_uart_enable(port);
	spin_unlock_irqrestore(&port->lock, flags);
}

 
static int bcm_uart_request_port(struct uart_port *port)
{
	 
	return 0;
}

 
static void bcm_uart_release_port(struct uart_port *port)
{
	 
}

 
static void bcm_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		if (bcm_uart_request_port(port))
			return;
		port->type = PORT_BCM63XX;
	}
}

 
static int bcm_uart_verify_port(struct uart_port *port,
				struct serial_struct *serinfo)
{
	if (port->type != PORT_BCM63XX)
		return -EINVAL;
	if (port->irq != serinfo->irq)
		return -EINVAL;
	if (port->iotype != serinfo->io_type)
		return -EINVAL;
	if (port->mapbase != (unsigned long)serinfo->iomem_base)
		return -EINVAL;
	return 0;
}

#ifdef CONFIG_CONSOLE_POLL
 
static bool bcm_uart_tx_full(struct uart_port *port)
{
	unsigned int val;

	val = bcm_uart_readl(port, UART_MCTL_REG);
	val = (val & UART_MCTL_TXFIFOFILL_MASK) >> UART_MCTL_TXFIFOFILL_SHIFT;
	return !(port->fifosize - val);
}

static int bcm_uart_poll_get_char(struct uart_port *port)
{
	unsigned int iestat;

	iestat = bcm_uart_readl(port, UART_IR_REG);
	if (!(iestat & UART_IR_STAT(UART_IR_RXNOTEMPTY)))
		return NO_POLL_CHAR;

	return bcm_uart_readl(port, UART_FIFO_REG);
}

static void bcm_uart_poll_put_char(struct uart_port *port, unsigned char c)
{
	while (bcm_uart_tx_full(port)) {
		cpu_relax();
	}

	bcm_uart_writel(port, c, UART_FIFO_REG);
}
#endif

 
static const struct uart_ops bcm_uart_ops = {
	.tx_empty	= bcm_uart_tx_empty,
	.get_mctrl	= bcm_uart_get_mctrl,
	.set_mctrl	= bcm_uart_set_mctrl,
	.start_tx	= bcm_uart_start_tx,
	.stop_tx	= bcm_uart_stop_tx,
	.stop_rx	= bcm_uart_stop_rx,
	.enable_ms	= bcm_uart_enable_ms,
	.break_ctl	= bcm_uart_break_ctl,
	.startup	= bcm_uart_startup,
	.shutdown	= bcm_uart_shutdown,
	.set_termios	= bcm_uart_set_termios,
	.type		= bcm_uart_type,
	.release_port	= bcm_uart_release_port,
	.request_port	= bcm_uart_request_port,
	.config_port	= bcm_uart_config_port,
	.verify_port	= bcm_uart_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char  = bcm_uart_poll_get_char,
	.poll_put_char  = bcm_uart_poll_put_char,
#endif
};



#ifdef CONFIG_SERIAL_BCM63XX_CONSOLE
static void wait_for_xmitr(struct uart_port *port)
{
	unsigned int tmout;

	 
	tmout = 10000;
	while (--tmout) {
		unsigned int val;

		val = bcm_uart_readl(port, UART_IR_REG);
		if (val & UART_IR_STAT(UART_IR_TXEMPTY))
			break;
		udelay(1);
	}

	 
	if (port->flags & UPF_CONS_FLOW) {
		tmout = 1000000;
		while (--tmout) {
			unsigned int val;

			val = bcm_uart_readl(port, UART_EXTINP_REG);
			if (val & UART_EXTINP_CTS_MASK)
				break;
			udelay(1);
		}
	}
}

 
static void bcm_console_putchar(struct uart_port *port, unsigned char ch)
{
	wait_for_xmitr(port);
	bcm_uart_writel(port, ch, UART_FIFO_REG);
}

 
static void bcm_console_write(struct console *co, const char *s,
			      unsigned int count)
{
	struct uart_port *port;
	unsigned long flags;
	int locked;

	port = &ports[co->index];

	local_irq_save(flags);
	if (port->sysrq) {
		 
		locked = 0;
	} else if (oops_in_progress) {
		locked = spin_trylock(&port->lock);
	} else {
		spin_lock(&port->lock);
		locked = 1;
	}

	 
	uart_console_write(port, s, count, bcm_console_putchar);

	 
	wait_for_xmitr(port);

	if (locked)
		spin_unlock(&port->lock);
	local_irq_restore(flags);
}

 
static int bcm_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index < 0 || co->index >= BCM63XX_NR_UARTS)
		return -EINVAL;
	port = &ports[co->index];
	if (!port->membase)
		return -ENODEV;
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver bcm_uart_driver;

static struct console bcm63xx_console = {
	.name		= "ttyS",
	.write		= bcm_console_write,
	.device		= uart_console_device,
	.setup		= bcm_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &bcm_uart_driver,
};

static int __init bcm63xx_console_init(void)
{
	register_console(&bcm63xx_console);
	return 0;
}

console_initcall(bcm63xx_console_init);

static void bcm_early_write(struct console *con, const char *s, unsigned n)
{
	struct earlycon_device *dev = con->data;

	uart_console_write(&dev->port, s, n, bcm_console_putchar);
	wait_for_xmitr(&dev->port);
}

static int __init bcm_early_console_setup(struct earlycon_device *device,
					  const char *opt)
{
	if (!device->port.membase)
		return -ENODEV;

	device->con->write = bcm_early_write;
	return 0;
}

OF_EARLYCON_DECLARE(bcm63xx_uart, "brcm,bcm6345-uart", bcm_early_console_setup);

#define BCM63XX_CONSOLE	(&bcm63xx_console)
#else
#define BCM63XX_CONSOLE	NULL
#endif  

static struct uart_driver bcm_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "bcm63xx_uart",
	.dev_name	= "ttyS",
	.major		= TTY_MAJOR,
	.minor		= 64,
	.nr		= BCM63XX_NR_UARTS,
	.cons		= BCM63XX_CONSOLE,
};

 
static int bcm_uart_probe(struct platform_device *pdev)
{
	struct resource *res_mem;
	struct uart_port *port;
	struct clk *clk;
	int ret;

	if (pdev->dev.of_node) {
		pdev->id = of_alias_get_id(pdev->dev.of_node, "serial");

		if (pdev->id < 0)
			pdev->id = of_alias_get_id(pdev->dev.of_node, "uart");
	}

	if (pdev->id < 0 || pdev->id >= BCM63XX_NR_UARTS)
		return -EINVAL;

	port = &ports[pdev->id];
	if (port->membase)
		return -EBUSY;
	memset(port, 0, sizeof(*port));

	port->membase = devm_platform_get_and_ioremap_resource(pdev, 0, &res_mem);
	if (IS_ERR(port->membase))
		return PTR_ERR(port->membase);
	port->mapbase = res_mem->start;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		return ret;
	port->irq = ret;

	clk = clk_get(&pdev->dev, "refclk");
	if (IS_ERR(clk) && pdev->dev.of_node)
		clk = of_clk_get(pdev->dev.of_node, 0);

	if (IS_ERR(clk))
		return -ENODEV;

	port->iotype = UPIO_MEM;
	port->ops = &bcm_uart_ops;
	port->flags = UPF_BOOT_AUTOCONF;
	port->dev = &pdev->dev;
	port->fifosize = 16;
	port->uartclk = clk_get_rate(clk) / 2;
	port->line = pdev->id;
	port->has_sysrq = IS_ENABLED(CONFIG_SERIAL_BCM63XX_CONSOLE);
	clk_put(clk);

	ret = uart_add_one_port(&bcm_uart_driver, port);
	if (ret) {
		ports[pdev->id].membase = NULL;
		return ret;
	}
	platform_set_drvdata(pdev, port);
	return 0;
}

static int bcm_uart_remove(struct platform_device *pdev)
{
	struct uart_port *port;

	port = platform_get_drvdata(pdev);
	uart_remove_one_port(&bcm_uart_driver, port);
	 
	ports[pdev->id].membase = NULL;
	return 0;
}

static const struct of_device_id bcm63xx_of_match[] = {
	{ .compatible = "brcm,bcm6345-uart" },
	{   }
};
MODULE_DEVICE_TABLE(of, bcm63xx_of_match);

 
static struct platform_driver bcm_uart_platform_driver = {
	.probe	= bcm_uart_probe,
	.remove	= bcm_uart_remove,
	.driver	= {
		.name  = "bcm63xx_uart",
		.of_match_table = bcm63xx_of_match,
	},
};

static int __init bcm_uart_init(void)
{
	int ret;

	ret = uart_register_driver(&bcm_uart_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&bcm_uart_platform_driver);
	if (ret)
		uart_unregister_driver(&bcm_uart_driver);

	return ret;
}

static void __exit bcm_uart_exit(void)
{
	platform_driver_unregister(&bcm_uart_platform_driver);
	uart_unregister_driver(&bcm_uart_driver);
}

module_init(bcm_uart_init);
module_exit(bcm_uart_exit);

MODULE_AUTHOR("Maxime Bizon <mbizon@freebox.fr>");
MODULE_DESCRIPTION("Broadcom 63xx integrated uart driver");
MODULE_LICENSE("GPL");
