
 

#include <linux/clk.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

 

 
#define SIFIVE_SERIAL_TXDATA_OFFS		0x0
#define SIFIVE_SERIAL_TXDATA_FULL_SHIFT		31
#define SIFIVE_SERIAL_TXDATA_FULL_MASK		(1 << SIFIVE_SERIAL_TXDATA_FULL_SHIFT)
#define SIFIVE_SERIAL_TXDATA_DATA_SHIFT		0
#define SIFIVE_SERIAL_TXDATA_DATA_MASK		(0xff << SIFIVE_SERIAL_TXDATA_DATA_SHIFT)

 
#define SIFIVE_SERIAL_RXDATA_OFFS		0x4
#define SIFIVE_SERIAL_RXDATA_EMPTY_SHIFT	31
#define SIFIVE_SERIAL_RXDATA_EMPTY_MASK		(1 << SIFIVE_SERIAL_RXDATA_EMPTY_SHIFT)
#define SIFIVE_SERIAL_RXDATA_DATA_SHIFT		0
#define SIFIVE_SERIAL_RXDATA_DATA_MASK		(0xff << SIFIVE_SERIAL_RXDATA_DATA_SHIFT)

 
#define SIFIVE_SERIAL_TXCTRL_OFFS		0x8
#define SIFIVE_SERIAL_TXCTRL_TXCNT_SHIFT	16
#define SIFIVE_SERIAL_TXCTRL_TXCNT_MASK		(0x7 << SIFIVE_SERIAL_TXCTRL_TXCNT_SHIFT)
#define SIFIVE_SERIAL_TXCTRL_NSTOP_SHIFT	1
#define SIFIVE_SERIAL_TXCTRL_NSTOP_MASK		(1 << SIFIVE_SERIAL_TXCTRL_NSTOP_SHIFT)
#define SIFIVE_SERIAL_TXCTRL_TXEN_SHIFT		0
#define SIFIVE_SERIAL_TXCTRL_TXEN_MASK		(1 << SIFIVE_SERIAL_TXCTRL_TXEN_SHIFT)

 
#define SIFIVE_SERIAL_RXCTRL_OFFS		0xC
#define SIFIVE_SERIAL_RXCTRL_RXCNT_SHIFT	16
#define SIFIVE_SERIAL_RXCTRL_RXCNT_MASK		(0x7 << SIFIVE_SERIAL_TXCTRL_TXCNT_SHIFT)
#define SIFIVE_SERIAL_RXCTRL_RXEN_SHIFT		0
#define SIFIVE_SERIAL_RXCTRL_RXEN_MASK		(1 << SIFIVE_SERIAL_RXCTRL_RXEN_SHIFT)

 
#define SIFIVE_SERIAL_IE_OFFS			0x10
#define SIFIVE_SERIAL_IE_RXWM_SHIFT		1
#define SIFIVE_SERIAL_IE_RXWM_MASK		(1 << SIFIVE_SERIAL_IE_RXWM_SHIFT)
#define SIFIVE_SERIAL_IE_TXWM_SHIFT		0
#define SIFIVE_SERIAL_IE_TXWM_MASK		(1 << SIFIVE_SERIAL_IE_TXWM_SHIFT)

 
#define SIFIVE_SERIAL_IP_OFFS			0x14
#define SIFIVE_SERIAL_IP_RXWM_SHIFT		1
#define SIFIVE_SERIAL_IP_RXWM_MASK		(1 << SIFIVE_SERIAL_IP_RXWM_SHIFT)
#define SIFIVE_SERIAL_IP_TXWM_SHIFT		0
#define SIFIVE_SERIAL_IP_TXWM_MASK		(1 << SIFIVE_SERIAL_IP_TXWM_SHIFT)

 
#define SIFIVE_SERIAL_DIV_OFFS			0x18
#define SIFIVE_SERIAL_DIV_DIV_SHIFT		0
#define SIFIVE_SERIAL_DIV_DIV_MASK		(0xffff << SIFIVE_SERIAL_IP_DIV_SHIFT)

 

 
#define SIFIVE_SERIAL_MAX_PORTS			8

 
#define SIFIVE_DEFAULT_BAUD_RATE		115200

 
#define SIFIVE_SERIAL_NAME			"sifive-serial"

 
#define SIFIVE_TTY_PREFIX			"ttySIF"

 
#define SIFIVE_TX_FIFO_DEPTH			8

 
#define SIFIVE_RX_FIFO_DEPTH			8

#if (SIFIVE_TX_FIFO_DEPTH != SIFIVE_RX_FIFO_DEPTH)
#error Driver does not support configurations with different TX, RX FIFO sizes
#endif

 

 
struct sifive_serial_port {
	struct uart_port	port;
	struct device		*dev;
	unsigned char		ier;
	unsigned long		baud_rate;
	struct clk		*clk;
	struct notifier_block	clk_notifier;
};

 

#define port_to_sifive_serial_port(p) (container_of((p), \
						    struct sifive_serial_port, \
						    port))

#define notifier_to_sifive_serial_port(nb) (container_of((nb), \
							 struct sifive_serial_port, \
							 clk_notifier))

 
static void sifive_serial_stop_tx(struct uart_port *port);

 

 
static void __ssp_early_writel(u32 v, u16 offs, struct uart_port *port)
{
	writel_relaxed(v, port->membase + offs);
}

 
static u32 __ssp_early_readl(struct uart_port *port, u16 offs)
{
	return readl_relaxed(port->membase + offs);
}

 
static void __ssp_writel(u32 v, u16 offs, struct sifive_serial_port *ssp)
{
	__ssp_early_writel(v, offs, &ssp->port);
}

 
static u32 __ssp_readl(struct sifive_serial_port *ssp, u16 offs)
{
	return __ssp_early_readl(&ssp->port, offs);
}

 
static int sifive_serial_is_txfifo_full(struct sifive_serial_port *ssp)
{
	return __ssp_readl(ssp, SIFIVE_SERIAL_TXDATA_OFFS) &
		SIFIVE_SERIAL_TXDATA_FULL_MASK;
}

 
static void __ssp_transmit_char(struct sifive_serial_port *ssp, int ch)
{
	__ssp_writel(ch, SIFIVE_SERIAL_TXDATA_OFFS, ssp);
}

 
static void __ssp_transmit_chars(struct sifive_serial_port *ssp)
{
	u8 ch;

	uart_port_tx_limited(&ssp->port, ch, SIFIVE_TX_FIFO_DEPTH,
		true,
		__ssp_transmit_char(ssp, ch),
		({}));
}

 
static void __ssp_enable_txwm(struct sifive_serial_port *ssp)
{
	if (ssp->ier & SIFIVE_SERIAL_IE_TXWM_MASK)
		return;

	ssp->ier |= SIFIVE_SERIAL_IE_TXWM_MASK;
	__ssp_writel(ssp->ier, SIFIVE_SERIAL_IE_OFFS, ssp);
}

 
static void __ssp_enable_rxwm(struct sifive_serial_port *ssp)
{
	if (ssp->ier & SIFIVE_SERIAL_IE_RXWM_MASK)
		return;

	ssp->ier |= SIFIVE_SERIAL_IE_RXWM_MASK;
	__ssp_writel(ssp->ier, SIFIVE_SERIAL_IE_OFFS, ssp);
}

 
static void __ssp_disable_txwm(struct sifive_serial_port *ssp)
{
	if (!(ssp->ier & SIFIVE_SERIAL_IE_TXWM_MASK))
		return;

	ssp->ier &= ~SIFIVE_SERIAL_IE_TXWM_MASK;
	__ssp_writel(ssp->ier, SIFIVE_SERIAL_IE_OFFS, ssp);
}

 
static void __ssp_disable_rxwm(struct sifive_serial_port *ssp)
{
	if (!(ssp->ier & SIFIVE_SERIAL_IE_RXWM_MASK))
		return;

	ssp->ier &= ~SIFIVE_SERIAL_IE_RXWM_MASK;
	__ssp_writel(ssp->ier, SIFIVE_SERIAL_IE_OFFS, ssp);
}

 
static char __ssp_receive_char(struct sifive_serial_port *ssp, char *is_empty)
{
	u32 v;
	u8 ch;

	v = __ssp_readl(ssp, SIFIVE_SERIAL_RXDATA_OFFS);

	if (!is_empty)
		WARN_ON(1);
	else
		*is_empty = (v & SIFIVE_SERIAL_RXDATA_EMPTY_MASK) >>
			SIFIVE_SERIAL_RXDATA_EMPTY_SHIFT;

	ch = (v & SIFIVE_SERIAL_RXDATA_DATA_MASK) >>
		SIFIVE_SERIAL_RXDATA_DATA_SHIFT;

	return ch;
}

 
static void __ssp_receive_chars(struct sifive_serial_port *ssp)
{
	char is_empty;
	int c;
	u8 ch;

	for (c = SIFIVE_RX_FIFO_DEPTH; c > 0; --c) {
		ch = __ssp_receive_char(ssp, &is_empty);
		if (is_empty)
			break;

		ssp->port.icount.rx++;
		uart_insert_char(&ssp->port, 0, 0, ch, TTY_NORMAL);
	}

	tty_flip_buffer_push(&ssp->port.state->port);
}

 
static void __ssp_update_div(struct sifive_serial_port *ssp)
{
	u16 div;

	div = DIV_ROUND_UP(ssp->port.uartclk, ssp->baud_rate) - 1;

	__ssp_writel(div, SIFIVE_SERIAL_DIV_OFFS, ssp);
}

 
static void __ssp_update_baud_rate(struct sifive_serial_port *ssp,
				   unsigned int rate)
{
	if (ssp->baud_rate == rate)
		return;

	ssp->baud_rate = rate;
	__ssp_update_div(ssp);
}

 
static void __ssp_set_stop_bits(struct sifive_serial_port *ssp, char nstop)
{
	u32 v;

	if (nstop < 1 || nstop > 2) {
		WARN_ON(1);
		return;
	}

	v = __ssp_readl(ssp, SIFIVE_SERIAL_TXCTRL_OFFS);
	v &= ~SIFIVE_SERIAL_TXCTRL_NSTOP_MASK;
	v |= (nstop - 1) << SIFIVE_SERIAL_TXCTRL_NSTOP_SHIFT;
	__ssp_writel(v, SIFIVE_SERIAL_TXCTRL_OFFS, ssp);
}

 
static void __maybe_unused __ssp_wait_for_xmitr(struct sifive_serial_port *ssp)
{
	while (sifive_serial_is_txfifo_full(ssp))
		udelay(1);  
}

 

static void sifive_serial_stop_tx(struct uart_port *port)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);

	__ssp_disable_txwm(ssp);
}

static void sifive_serial_stop_rx(struct uart_port *port)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);

	__ssp_disable_rxwm(ssp);
}

static void sifive_serial_start_tx(struct uart_port *port)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);

	__ssp_enable_txwm(ssp);
}

static irqreturn_t sifive_serial_irq(int irq, void *dev_id)
{
	struct sifive_serial_port *ssp = dev_id;
	u32 ip;

	spin_lock(&ssp->port.lock);

	ip = __ssp_readl(ssp, SIFIVE_SERIAL_IP_OFFS);
	if (!ip) {
		spin_unlock(&ssp->port.lock);
		return IRQ_NONE;
	}

	if (ip & SIFIVE_SERIAL_IP_RXWM_MASK)
		__ssp_receive_chars(ssp);
	if (ip & SIFIVE_SERIAL_IP_TXWM_MASK)
		__ssp_transmit_chars(ssp);

	spin_unlock(&ssp->port.lock);

	return IRQ_HANDLED;
}

static unsigned int sifive_serial_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT;
}

static unsigned int sifive_serial_get_mctrl(struct uart_port *port)
{
	return TIOCM_CAR | TIOCM_CTS | TIOCM_DSR;
}

static void sifive_serial_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	 
}

static void sifive_serial_break_ctl(struct uart_port *port, int break_state)
{
	 
}

static int sifive_serial_startup(struct uart_port *port)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);

	__ssp_enable_rxwm(ssp);

	return 0;
}

static void sifive_serial_shutdown(struct uart_port *port)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);

	__ssp_disable_rxwm(ssp);
	__ssp_disable_txwm(ssp);
}

 
static int sifive_serial_clk_notifier(struct notifier_block *nb,
				      unsigned long event, void *data)
{
	struct clk_notifier_data *cnd = data;
	struct sifive_serial_port *ssp = notifier_to_sifive_serial_port(nb);

	if (event == PRE_RATE_CHANGE) {
		 
		__ssp_wait_for_xmitr(ssp);
		 
		udelay(DIV_ROUND_UP(12 * 1000 * 1000, ssp->baud_rate));
	}

	if (event == POST_RATE_CHANGE && ssp->port.uartclk != cnd->new_rate) {
		ssp->port.uartclk = cnd->new_rate;
		__ssp_update_div(ssp);
	}

	return NOTIFY_OK;
}

static void sifive_serial_set_termios(struct uart_port *port,
				      struct ktermios *termios,
				      const struct ktermios *old)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);
	unsigned long flags;
	u32 v, old_v;
	int rate;
	char nstop;

	if ((termios->c_cflag & CSIZE) != CS8) {
		dev_err_once(ssp->port.dev, "only 8-bit words supported\n");
		termios->c_cflag &= ~CSIZE;
		termios->c_cflag |= CS8;
	}
	if (termios->c_iflag & (INPCK | PARMRK))
		dev_err_once(ssp->port.dev, "parity checking not supported\n");
	if (termios->c_iflag & BRKINT)
		dev_err_once(ssp->port.dev, "BREAK detection not supported\n");
	termios->c_iflag &= ~(INPCK|PARMRK|BRKINT);

	 
	nstop = (termios->c_cflag & CSTOPB) ? 2 : 1;
	__ssp_set_stop_bits(ssp, nstop);

	 
	rate = uart_get_baud_rate(port, termios, old, 0,
				  ssp->port.uartclk / 16);
	__ssp_update_baud_rate(ssp, rate);

	spin_lock_irqsave(&ssp->port.lock, flags);

	 
	uart_update_timeout(port, termios->c_cflag, rate);

	ssp->port.read_status_mask = 0;

	 
	v = __ssp_readl(ssp, SIFIVE_SERIAL_RXCTRL_OFFS);
	old_v = v;
	if ((termios->c_cflag & CREAD) == 0)
		v &= SIFIVE_SERIAL_RXCTRL_RXEN_MASK;
	else
		v |= SIFIVE_SERIAL_RXCTRL_RXEN_MASK;
	if (v != old_v)
		__ssp_writel(v, SIFIVE_SERIAL_RXCTRL_OFFS, ssp);

	spin_unlock_irqrestore(&ssp->port.lock, flags);
}

static void sifive_serial_release_port(struct uart_port *port)
{
}

static int sifive_serial_request_port(struct uart_port *port)
{
	return 0;
}

static void sifive_serial_config_port(struct uart_port *port, int flags)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);

	ssp->port.type = PORT_SIFIVE_V0;
}

static int sifive_serial_verify_port(struct uart_port *port,
				     struct serial_struct *ser)
{
	return -EINVAL;
}

static const char *sifive_serial_type(struct uart_port *port)
{
	return port->type == PORT_SIFIVE_V0 ? "SiFive UART v0" : NULL;
}

#ifdef CONFIG_CONSOLE_POLL
static int sifive_serial_poll_get_char(struct uart_port *port)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);
	char is_empty, ch;

	ch = __ssp_receive_char(ssp, &is_empty);
	if (is_empty)
		return NO_POLL_CHAR;

	return ch;
}

static void sifive_serial_poll_put_char(struct uart_port *port,
					unsigned char c)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);

	__ssp_wait_for_xmitr(ssp);
	__ssp_transmit_char(ssp, c);
}
#endif  

 

#ifdef CONFIG_SERIAL_EARLYCON
static void early_sifive_serial_putc(struct uart_port *port, unsigned char c)
{
	while (__ssp_early_readl(port, SIFIVE_SERIAL_TXDATA_OFFS) &
	       SIFIVE_SERIAL_TXDATA_FULL_MASK)
		cpu_relax();

	__ssp_early_writel(c, SIFIVE_SERIAL_TXDATA_OFFS, port);
}

static void early_sifive_serial_write(struct console *con, const char *s,
				      unsigned int n)
{
	struct earlycon_device *dev = con->data;
	struct uart_port *port = &dev->port;

	uart_console_write(port, s, n, early_sifive_serial_putc);
}

static int __init early_sifive_serial_setup(struct earlycon_device *dev,
					    const char *options)
{
	struct uart_port *port = &dev->port;

	if (!port->membase)
		return -ENODEV;

	dev->con->write = early_sifive_serial_write;

	return 0;
}

OF_EARLYCON_DECLARE(sifive, "sifive,uart0", early_sifive_serial_setup);
OF_EARLYCON_DECLARE(sifive, "sifive,fu540-c000-uart0",
		    early_sifive_serial_setup);
#endif  

 

#ifdef CONFIG_SERIAL_SIFIVE_CONSOLE

static struct sifive_serial_port *sifive_serial_console_ports[SIFIVE_SERIAL_MAX_PORTS];

static void sifive_serial_console_putchar(struct uart_port *port, unsigned char ch)
{
	struct sifive_serial_port *ssp = port_to_sifive_serial_port(port);

	__ssp_wait_for_xmitr(ssp);
	__ssp_transmit_char(ssp, ch);
}

static void sifive_serial_console_write(struct console *co, const char *s,
					unsigned int count)
{
	struct sifive_serial_port *ssp = sifive_serial_console_ports[co->index];
	unsigned long flags;
	unsigned int ier;
	int locked = 1;

	if (!ssp)
		return;

	local_irq_save(flags);
	if (ssp->port.sysrq)
		locked = 0;
	else if (oops_in_progress)
		locked = spin_trylock(&ssp->port.lock);
	else
		spin_lock(&ssp->port.lock);

	ier = __ssp_readl(ssp, SIFIVE_SERIAL_IE_OFFS);
	__ssp_writel(0, SIFIVE_SERIAL_IE_OFFS, ssp);

	uart_console_write(&ssp->port, s, count, sifive_serial_console_putchar);

	__ssp_writel(ier, SIFIVE_SERIAL_IE_OFFS, ssp);

	if (locked)
		spin_unlock(&ssp->port.lock);
	local_irq_restore(flags);
}

static int sifive_serial_console_setup(struct console *co, char *options)
{
	struct sifive_serial_port *ssp;
	int baud = SIFIVE_DEFAULT_BAUD_RATE;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index < 0 || co->index >= SIFIVE_SERIAL_MAX_PORTS)
		return -ENODEV;

	ssp = sifive_serial_console_ports[co->index];
	if (!ssp)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&ssp->port, co, baud, parity, bits, flow);
}

static struct uart_driver sifive_serial_uart_driver;

static struct console sifive_serial_console = {
	.name		= SIFIVE_TTY_PREFIX,
	.write		= sifive_serial_console_write,
	.device		= uart_console_device,
	.setup		= sifive_serial_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &sifive_serial_uart_driver,
};

static int __init sifive_console_init(void)
{
	register_console(&sifive_serial_console);
	return 0;
}

console_initcall(sifive_console_init);

static void __ssp_add_console_port(struct sifive_serial_port *ssp)
{
	sifive_serial_console_ports[ssp->port.line] = ssp;
}

static void __ssp_remove_console_port(struct sifive_serial_port *ssp)
{
	sifive_serial_console_ports[ssp->port.line] = NULL;
}

#define SIFIVE_SERIAL_CONSOLE	(&sifive_serial_console)

#else

#define SIFIVE_SERIAL_CONSOLE	NULL

static void __ssp_add_console_port(struct sifive_serial_port *ssp)
{}
static void __ssp_remove_console_port(struct sifive_serial_port *ssp)
{}

#endif

static const struct uart_ops sifive_serial_uops = {
	.tx_empty	= sifive_serial_tx_empty,
	.set_mctrl	= sifive_serial_set_mctrl,
	.get_mctrl	= sifive_serial_get_mctrl,
	.stop_tx	= sifive_serial_stop_tx,
	.start_tx	= sifive_serial_start_tx,
	.stop_rx	= sifive_serial_stop_rx,
	.break_ctl	= sifive_serial_break_ctl,
	.startup	= sifive_serial_startup,
	.shutdown	= sifive_serial_shutdown,
	.set_termios	= sifive_serial_set_termios,
	.type		= sifive_serial_type,
	.release_port	= sifive_serial_release_port,
	.request_port	= sifive_serial_request_port,
	.config_port	= sifive_serial_config_port,
	.verify_port	= sifive_serial_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char	= sifive_serial_poll_get_char,
	.poll_put_char	= sifive_serial_poll_put_char,
#endif
};

static struct uart_driver sifive_serial_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= SIFIVE_SERIAL_NAME,
	.dev_name	= SIFIVE_TTY_PREFIX,
	.nr		= SIFIVE_SERIAL_MAX_PORTS,
	.cons		= SIFIVE_SERIAL_CONSOLE,
};

static int sifive_serial_probe(struct platform_device *pdev)
{
	struct sifive_serial_port *ssp;
	struct resource *mem;
	struct clk *clk;
	void __iomem *base;
	int irq, id, r;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -EPROBE_DEFER;

	base = devm_platform_get_and_ioremap_resource(pdev, 0, &mem);
	if (IS_ERR(base))
		return PTR_ERR(base);

	clk = devm_clk_get_enabled(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "unable to find controller clock\n");
		return PTR_ERR(clk);
	}

	id = of_alias_get_id(pdev->dev.of_node, "serial");
	if (id < 0) {
		dev_err(&pdev->dev, "missing aliases entry\n");
		return id;
	}

#ifdef CONFIG_SERIAL_SIFIVE_CONSOLE
	if (id > SIFIVE_SERIAL_MAX_PORTS) {
		dev_err(&pdev->dev, "too many UARTs (%d)\n", id);
		return -EINVAL;
	}
#endif

	ssp = devm_kzalloc(&pdev->dev, sizeof(*ssp), GFP_KERNEL);
	if (!ssp)
		return -ENOMEM;

	ssp->port.dev = &pdev->dev;
	ssp->port.type = PORT_SIFIVE_V0;
	ssp->port.iotype = UPIO_MEM;
	ssp->port.irq = irq;
	ssp->port.fifosize = SIFIVE_TX_FIFO_DEPTH;
	ssp->port.ops = &sifive_serial_uops;
	ssp->port.line = id;
	ssp->port.mapbase = mem->start;
	ssp->port.membase = base;
	ssp->dev = &pdev->dev;
	ssp->clk = clk;
	ssp->clk_notifier.notifier_call = sifive_serial_clk_notifier;

	r = clk_notifier_register(ssp->clk, &ssp->clk_notifier);
	if (r) {
		dev_err(&pdev->dev, "could not register clock notifier: %d\n",
			r);
		goto probe_out1;
	}

	 
	ssp->port.uartclk = clk_get_rate(ssp->clk);
	ssp->baud_rate = SIFIVE_DEFAULT_BAUD_RATE;
	__ssp_update_div(ssp);

	platform_set_drvdata(pdev, ssp);

	 
	__ssp_writel((1 << SIFIVE_SERIAL_TXCTRL_TXCNT_SHIFT) |
		     SIFIVE_SERIAL_TXCTRL_TXEN_MASK,
		     SIFIVE_SERIAL_TXCTRL_OFFS, ssp);

	 
	__ssp_writel((0 << SIFIVE_SERIAL_RXCTRL_RXCNT_SHIFT) |
		     SIFIVE_SERIAL_RXCTRL_RXEN_MASK,
		     SIFIVE_SERIAL_RXCTRL_OFFS, ssp);

	r = request_irq(ssp->port.irq, sifive_serial_irq, ssp->port.irqflags,
			dev_name(&pdev->dev), ssp);
	if (r) {
		dev_err(&pdev->dev, "could not attach interrupt: %d\n", r);
		goto probe_out2;
	}

	__ssp_add_console_port(ssp);

	r = uart_add_one_port(&sifive_serial_uart_driver, &ssp->port);
	if (r != 0) {
		dev_err(&pdev->dev, "could not add uart: %d\n", r);
		goto probe_out3;
	}

	return 0;

probe_out3:
	__ssp_remove_console_port(ssp);
	free_irq(ssp->port.irq, ssp);
probe_out2:
	clk_notifier_unregister(ssp->clk, &ssp->clk_notifier);
probe_out1:
	return r;
}

static int sifive_serial_remove(struct platform_device *dev)
{
	struct sifive_serial_port *ssp = platform_get_drvdata(dev);

	__ssp_remove_console_port(ssp);
	uart_remove_one_port(&sifive_serial_uart_driver, &ssp->port);
	free_irq(ssp->port.irq, ssp);
	clk_notifier_unregister(ssp->clk, &ssp->clk_notifier);

	return 0;
}

static int sifive_serial_suspend(struct device *dev)
{
	struct sifive_serial_port *ssp = dev_get_drvdata(dev);

	return uart_suspend_port(&sifive_serial_uart_driver, &ssp->port);
}

static int sifive_serial_resume(struct device *dev)
{
	struct sifive_serial_port *ssp = dev_get_drvdata(dev);

	return uart_resume_port(&sifive_serial_uart_driver, &ssp->port);
}

DEFINE_SIMPLE_DEV_PM_OPS(sifive_uart_pm_ops, sifive_serial_suspend,
			 sifive_serial_resume);

static const struct of_device_id sifive_serial_of_match[] = {
	{ .compatible = "sifive,fu540-c000-uart0" },
	{ .compatible = "sifive,uart0" },
	{},
};
MODULE_DEVICE_TABLE(of, sifive_serial_of_match);

static struct platform_driver sifive_serial_platform_driver = {
	.probe		= sifive_serial_probe,
	.remove		= sifive_serial_remove,
	.driver		= {
		.name	= SIFIVE_SERIAL_NAME,
		.pm = pm_sleep_ptr(&sifive_uart_pm_ops),
		.of_match_table = sifive_serial_of_match,
	},
};

static int __init sifive_serial_init(void)
{
	int r;

	r = uart_register_driver(&sifive_serial_uart_driver);
	if (r)
		goto init_out1;

	r = platform_driver_register(&sifive_serial_platform_driver);
	if (r)
		goto init_out2;

	return 0;

init_out2:
	uart_unregister_driver(&sifive_serial_uart_driver);
init_out1:
	return r;
}

static void __exit sifive_serial_exit(void)
{
	platform_driver_unregister(&sifive_serial_platform_driver);
	uart_unregister_driver(&sifive_serial_uart_driver);
}

module_init(sifive_serial_init);
module_exit(sifive_serial_exit);

MODULE_DESCRIPTION("SiFive UART serial driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Paul Walmsley <paul@pwsan.com>");
