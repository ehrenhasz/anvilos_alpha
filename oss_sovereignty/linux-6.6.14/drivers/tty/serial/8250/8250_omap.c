
 

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <linux/tty_flip.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/console.h>
#include <linux/pm_qos.h>
#include <linux/pm_wakeirq.h>
#include <linux/dma-mapping.h>
#include <linux/sys_soc.h>

#include "8250.h"

#define DEFAULT_CLK_SPEED	48000000
#define OMAP_UART_REGSHIFT	2

#define UART_ERRATA_i202_MDR1_ACCESS	(1 << 0)
#define OMAP_UART_WER_HAS_TX_WAKEUP	(1 << 1)
#define OMAP_DMA_TX_KICK		(1 << 2)
 
#define UART_ERRATA_CLOCK_DISABLE	(1 << 3)
#define	UART_HAS_EFR2			BIT(4)
#define UART_HAS_RHR_IT_DIS		BIT(5)
#define UART_RX_TIMEOUT_QUIRK		BIT(6)
#define UART_HAS_NATIVE_RS485		BIT(7)

#define OMAP_UART_FCR_RX_TRIG		6
#define OMAP_UART_FCR_TX_TRIG		4

 
#define OMAP_UART_SCR_RX_TRIG_GRANU1_MASK	(1 << 7)
#define OMAP_UART_SCR_TX_TRIG_GRANU1_MASK	(1 << 6)
#define OMAP_UART_SCR_TX_EMPTY			(1 << 3)
#define OMAP_UART_SCR_DMAMODE_MASK		(3 << 1)
#define OMAP_UART_SCR_DMAMODE_1			(1 << 1)
#define OMAP_UART_SCR_DMAMODE_CTL		(1 << 0)

 
#define OMAP_UART_MVR_SCHEME_SHIFT	30
#define OMAP_UART_LEGACY_MVR_MAJ_MASK	0xf0
#define OMAP_UART_LEGACY_MVR_MAJ_SHIFT	4
#define OMAP_UART_LEGACY_MVR_MIN_MASK	0x0f
#define OMAP_UART_MVR_MAJ_MASK		0x700
#define OMAP_UART_MVR_MAJ_SHIFT		8
#define OMAP_UART_MVR_MIN_MASK		0x3f

 
#define OMAP_UART_SYSC_SOFTRESET	(1 << 1)

 
#define OMAP_UART_SYSS_RESETDONE	(1 << 0)

#define UART_TI752_TLR_TX	0
#define UART_TI752_TLR_RX	4

#define TRIGGER_TLR_MASK(x)	((x & 0x3c) >> 2)
#define TRIGGER_FCR_MASK(x)	(x & 3)

 
#define OMAP_UART_SW_TX		0x08
 
#define OMAP_UART_SW_RX		0x02

#define OMAP_UART_WER_MOD_WKUP	0x7f
#define OMAP_UART_TX_WAKEUP_EN	(1 << 7)

#define TX_TRIGGER	1
#define RX_TRIGGER	48

#define OMAP_UART_TCR_RESTORE(x)	((x / 4) << 4)
#define OMAP_UART_TCR_HALT(x)		((x / 4) << 0)

#define UART_BUILD_REVISION(x, y)	(((x) << 8) | (y))

#define OMAP_UART_REV_46 0x0406
#define OMAP_UART_REV_52 0x0502
#define OMAP_UART_REV_63 0x0603

 
#define UART_OMAP_IER2			0x1B
#define UART_OMAP_IER2_RHR_IT_DIS	BIT(2)

 
#define UART_OMAP_MDR3			0x20
#define UART_OMAP_MDR3_DIR_POL		BIT(3)
#define UART_OMAP_MDR3_DIR_EN		BIT(4)

 
#define UART_OMAP_EFR2			0x23
#define UART_OMAP_EFR2_TIMEOUT_BEHAVE	BIT(6)

 
#define UART_OMAP_RX_LVL		0x19

struct omap8250_priv {
	void __iomem *membase;
	int line;
	u8 habit;
	u8 mdr1;
	u8 mdr3;
	u8 efr;
	u8 scr;
	u8 wer;
	u8 xon;
	u8 xoff;
	u8 delayed_restore;
	u16 quot;

	u8 tx_trigger;
	u8 rx_trigger;
	bool is_suspending;
	int wakeirq;
	int wakeups_enabled;
	u32 latency;
	u32 calc_latency;
	struct pm_qos_request pm_qos_request;
	struct work_struct qos_work;
	struct uart_8250_dma omap8250_dma;
	spinlock_t rx_dma_lock;
	bool rx_dma_broken;
	bool throttled;
};

struct omap8250_dma_params {
	u32 rx_size;
	u8 rx_trigger;
	u8 tx_trigger;
};

struct omap8250_platdata {
	struct omap8250_dma_params *dma_params;
	u8 habit;
};

#ifdef CONFIG_SERIAL_8250_DMA
static void omap_8250_rx_dma_flush(struct uart_8250_port *p);
#else
static inline void omap_8250_rx_dma_flush(struct uart_8250_port *p) { }
#endif

static u32 uart_read(struct omap8250_priv *priv, u32 reg)
{
	return readl(priv->membase + (reg << OMAP_UART_REGSHIFT));
}

 
static void __omap8250_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct omap8250_priv *priv = up->port.private_data;
	u8 lcr;

	serial8250_do_set_mctrl(port, mctrl);

	if (!mctrl_gpio_to_gpiod(up->gpios, UART_GPIO_RTS)) {
		 
		lcr = serial_in(up, UART_LCR);
		serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
		if ((mctrl & TIOCM_RTS) && (port->status & UPSTAT_AUTORTS))
			priv->efr |= UART_EFR_RTS;
		else
			priv->efr &= ~UART_EFR_RTS;
		serial_out(up, UART_EFR, priv->efr);
		serial_out(up, UART_LCR, lcr);
	}
}

static void omap8250_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	int err;

	err = pm_runtime_resume_and_get(port->dev);
	if (err)
		return;

	__omap8250_set_mctrl(port, mctrl);

	pm_runtime_mark_last_busy(port->dev);
	pm_runtime_put_autosuspend(port->dev);
}

 
static void omap_8250_mdr1_errataset(struct uart_8250_port *up,
				     struct omap8250_priv *priv)
{
	serial_out(up, UART_OMAP_MDR1, priv->mdr1);
	udelay(2);
	serial_out(up, UART_FCR, up->fcr | UART_FCR_CLEAR_XMIT |
			UART_FCR_CLEAR_RCVR);
}

static void omap_8250_get_divisor(struct uart_port *port, unsigned int baud,
				  struct omap8250_priv *priv)
{
	unsigned int uartclk = port->uartclk;
	unsigned int div_13, div_16;
	unsigned int abs_d13, abs_d16;

	 
	if (baud == 38400 && (port->flags & UPF_SPD_MASK) == UPF_SPD_CUST) {
		priv->quot = port->custom_divisor & UART_DIV_MAX;
		 
		if (port->custom_divisor & (1 << 16))
			priv->mdr1 = UART_OMAP_MDR1_13X_MODE;
		else
			priv->mdr1 = UART_OMAP_MDR1_16X_MODE;
		return;
	}
	div_13 = DIV_ROUND_CLOSEST(uartclk, 13 * baud);
	div_16 = DIV_ROUND_CLOSEST(uartclk, 16 * baud);

	if (!div_13)
		div_13 = 1;
	if (!div_16)
		div_16 = 1;

	abs_d13 = abs(baud - uartclk / 13 / div_13);
	abs_d16 = abs(baud - uartclk / 16 / div_16);

	if (abs_d13 >= abs_d16) {
		priv->mdr1 = UART_OMAP_MDR1_16X_MODE;
		priv->quot = div_16;
	} else {
		priv->mdr1 = UART_OMAP_MDR1_13X_MODE;
		priv->quot = div_13;
	}
}

static void omap8250_update_scr(struct uart_8250_port *up,
				struct omap8250_priv *priv)
{
	u8 old_scr;

	old_scr = serial_in(up, UART_OMAP_SCR);
	if (old_scr == priv->scr)
		return;

	 
	if (priv->scr & OMAP_UART_SCR_DMAMODE_MASK)
		serial_out(up, UART_OMAP_SCR,
			   priv->scr & ~OMAP_UART_SCR_DMAMODE_MASK);
	serial_out(up, UART_OMAP_SCR, priv->scr);
}

static void omap8250_update_mdr1(struct uart_8250_port *up,
				 struct omap8250_priv *priv)
{
	if (priv->habit & UART_ERRATA_i202_MDR1_ACCESS)
		omap_8250_mdr1_errataset(up, priv);
	else
		serial_out(up, UART_OMAP_MDR1, priv->mdr1);
}

static void omap8250_restore_regs(struct uart_8250_port *up)
{
	struct omap8250_priv *priv = up->port.private_data;
	struct uart_8250_dma	*dma = up->dma;
	u8 mcr = serial8250_in_MCR(up);

	 
	lockdep_assert_held_once(&up->port.lock);

	if (dma && dma->tx_running) {
		 
		priv->delayed_restore = 1;
		return;
	}

	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_out(up, UART_EFR, UART_EFR_ECB);

	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_A);
	serial8250_out_MCR(up, mcr | UART_MCR_TCRTLR);
	serial_out(up, UART_FCR, up->fcr);

	omap8250_update_scr(up, priv);

	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);

	serial_out(up, UART_TI752_TCR, OMAP_UART_TCR_RESTORE(16) |
			OMAP_UART_TCR_HALT(52));
	serial_out(up, UART_TI752_TLR,
		   TRIGGER_TLR_MASK(priv->tx_trigger) << UART_TI752_TLR_TX |
		   TRIGGER_TLR_MASK(priv->rx_trigger) << UART_TI752_TLR_RX);

	serial_out(up, UART_LCR, 0);

	 
	serial8250_out_MCR(up, mcr);

	serial_out(up, UART_IER, up->ier);

	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_dl_write(up, priv->quot);

	serial_out(up, UART_EFR, priv->efr);

	 
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_out(up, UART_XON1, priv->xon);
	serial_out(up, UART_XOFF1, priv->xoff);

	serial_out(up, UART_LCR, up->lcr);

	omap8250_update_mdr1(up, priv);

	__omap8250_set_mctrl(&up->port, up->port.mctrl);

	serial_out(up, UART_OMAP_MDR3, priv->mdr3);

	if (up->port.rs485.flags & SER_RS485_ENABLED &&
	    up->port.rs485_config == serial8250_em485_config)
		serial8250_em485_stop_tx(up);
}

 
static void omap_8250_set_termios(struct uart_port *port,
				  struct ktermios *termios,
				  const struct ktermios *old)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct omap8250_priv *priv = up->port.private_data;
	unsigned char cval = 0;
	unsigned int baud;

	cval = UART_LCR_WLEN(tty_get_char_size(termios->c_cflag));

	if (termios->c_cflag & CSTOPB)
		cval |= UART_LCR_STOP;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;
	if (termios->c_cflag & CMSPAR)
		cval |= UART_LCR_SPAR;

	 
	baud = uart_get_baud_rate(port, termios, old,
				  port->uartclk / 16 / UART_DIV_MAX,
				  port->uartclk / 13);
	omap_8250_get_divisor(port, baud, priv);

	 
	pm_runtime_get_sync(port->dev);
	spin_lock_irq(&port->lock);

	 
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (IGNBRK | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	 
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		 
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	 
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	 
	up->ier &= ~UART_IER_MSI;
	if (UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;

	up->lcr = cval;
	 

	 
	up->fcr = UART_FCR_ENABLE_FIFO;
	up->fcr |= TRIGGER_FCR_MASK(priv->tx_trigger) << OMAP_UART_FCR_TX_TRIG;
	up->fcr |= TRIGGER_FCR_MASK(priv->rx_trigger) << OMAP_UART_FCR_RX_TRIG;

	priv->scr = OMAP_UART_SCR_RX_TRIG_GRANU1_MASK | OMAP_UART_SCR_TX_EMPTY |
		OMAP_UART_SCR_TX_TRIG_GRANU1_MASK;

	if (up->dma)
		priv->scr |= OMAP_UART_SCR_DMAMODE_1 |
			OMAP_UART_SCR_DMAMODE_CTL;

	priv->xon = termios->c_cc[VSTART];
	priv->xoff = termios->c_cc[VSTOP];

	priv->efr = 0;
	up->port.status &= ~(UPSTAT_AUTOCTS | UPSTAT_AUTORTS | UPSTAT_AUTOXOFF);

	if (termios->c_cflag & CRTSCTS && up->port.flags & UPF_HARD_FLOW &&
	    !mctrl_gpio_to_gpiod(up->gpios, UART_GPIO_RTS) &&
	    !mctrl_gpio_to_gpiod(up->gpios, UART_GPIO_CTS)) {
		 
		up->port.status |= UPSTAT_AUTOCTS | UPSTAT_AUTORTS;
		priv->efr |= UART_EFR_CTS;
	} else	if (up->port.flags & UPF_SOFT_FLOW) {
		 

		 
		if (termios->c_iflag & IXOFF) {
			up->port.status |= UPSTAT_AUTOXOFF;
			priv->efr |= OMAP_UART_SW_TX;
		}
	}
	omap8250_restore_regs(up);

	spin_unlock_irq(&up->port.lock);
	pm_runtime_mark_last_busy(port->dev);
	pm_runtime_put_autosuspend(port->dev);

	 
	priv->calc_latency = USEC_PER_SEC * 64 * 8 / baud;
	priv->latency = priv->calc_latency;

	schedule_work(&priv->qos_work);

	 
	if (tty_termios_baud_rate(termios))
		tty_termios_encode_baud_rate(termios, baud, baud);
}

 
static void omap_8250_pm(struct uart_port *port, unsigned int state,
			 unsigned int oldstate)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	u8 efr;

	pm_runtime_get_sync(port->dev);

	 
	spin_lock_irq(&port->lock);

	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	efr = serial_in(up, UART_EFR);
	serial_out(up, UART_EFR, efr | UART_EFR_ECB);
	serial_out(up, UART_LCR, 0);

	serial_out(up, UART_IER, (state != 0) ? UART_IERX_SLEEP : 0);
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_out(up, UART_EFR, efr);
	serial_out(up, UART_LCR, 0);

	spin_unlock_irq(&port->lock);

	pm_runtime_mark_last_busy(port->dev);
	pm_runtime_put_autosuspend(port->dev);
}

static void omap_serial_fill_features_erratas(struct uart_8250_port *up,
					      struct omap8250_priv *priv)
{
	static const struct soc_device_attribute k3_soc_devices[] = {
		{ .family = "AM65X",  },
		{ .family = "J721E", .revision = "SR1.0" },
		{   }
	};
	u32 mvr, scheme;
	u16 revision, major, minor;

	mvr = uart_read(priv, UART_OMAP_MVER);

	 
	scheme = mvr >> OMAP_UART_MVR_SCHEME_SHIFT;

	switch (scheme) {
	case 0:  
		 
		major = (mvr & OMAP_UART_LEGACY_MVR_MAJ_MASK) >>
			OMAP_UART_LEGACY_MVR_MAJ_SHIFT;
		minor = (mvr & OMAP_UART_LEGACY_MVR_MIN_MASK);
		break;
	case 1:
		 
		 
		major = (mvr & OMAP_UART_MVR_MAJ_MASK) >>
			OMAP_UART_MVR_MAJ_SHIFT;
		minor = (mvr & OMAP_UART_MVR_MIN_MASK);
		break;
	default:
		dev_warn(up->port.dev,
			 "Unknown revision, defaulting to highest\n");
		 
		major = 0xff;
		minor = 0xff;
	}
	 
	revision = UART_BUILD_REVISION(major, minor);

	switch (revision) {
	case OMAP_UART_REV_46:
		priv->habit |= UART_ERRATA_i202_MDR1_ACCESS;
		break;
	case OMAP_UART_REV_52:
		priv->habit |= UART_ERRATA_i202_MDR1_ACCESS |
				OMAP_UART_WER_HAS_TX_WAKEUP;
		break;
	case OMAP_UART_REV_63:
		priv->habit |= UART_ERRATA_i202_MDR1_ACCESS |
			OMAP_UART_WER_HAS_TX_WAKEUP;
		break;
	default:
		break;
	}

	 
	if (soc_device_match(k3_soc_devices))
		priv->habit &= ~UART_HAS_RHR_IT_DIS;
}

static void omap8250_uart_qos_work(struct work_struct *work)
{
	struct omap8250_priv *priv;

	priv = container_of(work, struct omap8250_priv, qos_work);
	cpu_latency_qos_update_request(&priv->pm_qos_request, priv->latency);
}

#ifdef CONFIG_SERIAL_8250_DMA
static int omap_8250_dma_handle_irq(struct uart_port *port);
#endif

static irqreturn_t omap8250_irq(int irq, void *dev_id)
{
	struct omap8250_priv *priv = dev_id;
	struct uart_8250_port *up = serial8250_get_port(priv->line);
	struct uart_port *port = &up->port;
	unsigned int iir, lsr;
	int ret;

#ifdef CONFIG_SERIAL_8250_DMA
	if (up->dma) {
		ret = omap_8250_dma_handle_irq(port);
		return IRQ_RETVAL(ret);
	}
#endif

	serial8250_rpm_get(up);
	lsr = serial_port_in(port, UART_LSR);
	iir = serial_port_in(port, UART_IIR);
	ret = serial8250_handle_irq(port, iir);

	 
	if (priv->habit & UART_RX_TIMEOUT_QUIRK &&
	    (iir & UART_IIR_RX_TIMEOUT) == UART_IIR_RX_TIMEOUT &&
	    serial_port_in(port, UART_OMAP_RX_LVL) == 0) {
		serial_port_in(port, UART_RX);
	}

	 
	if ((lsr & UART_LSR_OE) && up->overrun_backoff_time_ms > 0) {
		unsigned long delay;

		 
		spin_lock(&port->lock);
		up->ier = port->serial_in(port, UART_IER);
		if (up->ier & (UART_IER_RLSI | UART_IER_RDI)) {
			port->ops->stop_rx(port);
		} else {
			 
			cancel_delayed_work(&up->overrun_backoff);
		}
		spin_unlock(&port->lock);

		delay = msecs_to_jiffies(up->overrun_backoff_time_ms);
		schedule_delayed_work(&up->overrun_backoff, delay);
	}

	serial8250_rpm_put(up);

	return IRQ_RETVAL(ret);
}

static int omap_8250_startup(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct omap8250_priv *priv = port->private_data;
	struct uart_8250_dma *dma = &priv->omap8250_dma;
	int ret;

	if (priv->wakeirq) {
		ret = dev_pm_set_dedicated_wake_irq(port->dev, priv->wakeirq);
		if (ret)
			return ret;
	}

	pm_runtime_get_sync(port->dev);

	serial_out(up, UART_FCR, UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);

	serial_out(up, UART_LCR, UART_LCR_WLEN8);

	up->lsr_saved_flags = 0;
	up->msr_saved_flags = 0;

	 
	if (dma->fn && !uart_console(port)) {
		up->dma = &priv->omap8250_dma;
		ret = serial8250_request_dma(up);
		if (ret) {
			dev_warn_ratelimited(port->dev,
					     "failed to request DMA\n");
			up->dma = NULL;
		}
	} else {
		up->dma = NULL;
	}

	 
	spin_lock_irq(&port->lock);
	up->ier = UART_IER_RLSI | UART_IER_RDI;
	serial_out(up, UART_IER, up->ier);
	spin_unlock_irq(&port->lock);

#ifdef CONFIG_PM
	up->capabilities |= UART_CAP_RPM;
#endif

	 
	priv->wer = OMAP_UART_WER_MOD_WKUP;
	if (priv->habit & OMAP_UART_WER_HAS_TX_WAKEUP)
		priv->wer |= OMAP_UART_TX_WAKEUP_EN;
	serial_out(up, UART_OMAP_WER, priv->wer);

	if (up->dma && !(priv->habit & UART_HAS_EFR2)) {
		spin_lock_irq(&port->lock);
		up->dma->rx_dma(up);
		spin_unlock_irq(&port->lock);
	}

	enable_irq(up->port.irq);

	pm_runtime_mark_last_busy(port->dev);
	pm_runtime_put_autosuspend(port->dev);
	return 0;
}

static void omap_8250_shutdown(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct omap8250_priv *priv = port->private_data;

	flush_work(&priv->qos_work);
	if (up->dma)
		omap_8250_rx_dma_flush(up);

	pm_runtime_get_sync(port->dev);

	serial_out(up, UART_OMAP_WER, 0);
	if (priv->habit & UART_HAS_EFR2)
		serial_out(up, UART_OMAP_EFR2, 0x0);

	 
	spin_lock_irq(&port->lock);
	up->ier = 0;
	serial_out(up, UART_IER, 0);
	spin_unlock_irq(&port->lock);
	disable_irq_nosync(up->port.irq);
	dev_pm_clear_wake_irq(port->dev);

	serial8250_release_dma(up);
	up->dma = NULL;

	 
	if (up->lcr & UART_LCR_SBC)
		serial_out(up, UART_LCR, up->lcr & ~UART_LCR_SBC);
	serial_out(up, UART_FCR, UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);

	pm_runtime_mark_last_busy(port->dev);
	pm_runtime_put_autosuspend(port->dev);
}

static void omap_8250_throttle(struct uart_port *port)
{
	struct omap8250_priv *priv = port->private_data;
	unsigned long flags;

	pm_runtime_get_sync(port->dev);

	spin_lock_irqsave(&port->lock, flags);
	port->ops->stop_rx(port);
	priv->throttled = true;
	spin_unlock_irqrestore(&port->lock, flags);

	pm_runtime_mark_last_busy(port->dev);
	pm_runtime_put_autosuspend(port->dev);
}

static void omap_8250_unthrottle(struct uart_port *port)
{
	struct omap8250_priv *priv = port->private_data;
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned long flags;

	pm_runtime_get_sync(port->dev);

	 
	spin_lock_irqsave(&port->lock, flags);
	priv->throttled = false;
	if (up->dma)
		up->dma->rx_dma(up);
	up->ier |= UART_IER_RLSI | UART_IER_RDI;
	port->read_status_mask |= UART_LSR_DR;
	serial_out(up, UART_IER, up->ier);
	spin_unlock_irqrestore(&port->lock, flags);

	pm_runtime_mark_last_busy(port->dev);
	pm_runtime_put_autosuspend(port->dev);
}

static int omap8250_rs485_config(struct uart_port *port,
				 struct ktermios *termios,
				 struct serial_rs485 *rs485)
{
	struct omap8250_priv *priv = port->private_data;
	struct uart_8250_port *up = up_to_u8250p(port);
	u32 fixed_delay_rts_before_send = 0;
	u32 fixed_delay_rts_after_send = 0;
	unsigned int baud;

	 
	if (priv->quot) {
		if (priv->mdr1 == UART_OMAP_MDR1_16X_MODE)
			baud = port->uartclk / (16 * priv->quot);
		else
			baud = port->uartclk / (13 * priv->quot);

		fixed_delay_rts_after_send  = 3 * MSEC_PER_SEC / baud;
		fixed_delay_rts_before_send = 1 * MSEC_PER_SEC / baud;
	}

	 
	if (!(priv->habit & UART_HAS_NATIVE_RS485) ||
	    mctrl_gpio_to_gpiod(up->gpios, UART_GPIO_RTS) ||
	    rs485->delay_rts_after_send  > fixed_delay_rts_after_send ||
	    rs485->delay_rts_before_send > fixed_delay_rts_before_send) {
		priv->mdr3 &= ~UART_OMAP_MDR3_DIR_EN;
		serial_out(up, UART_OMAP_MDR3, priv->mdr3);

		port->rs485_config = serial8250_em485_config;
		return serial8250_em485_config(port, termios, rs485);
	}

	rs485->delay_rts_after_send  = fixed_delay_rts_after_send;
	rs485->delay_rts_before_send = fixed_delay_rts_before_send;

	if (rs485->flags & SER_RS485_ENABLED)
		priv->mdr3 |= UART_OMAP_MDR3_DIR_EN;
	else
		priv->mdr3 &= ~UART_OMAP_MDR3_DIR_EN;

	 
	if (rs485->flags & SER_RS485_RTS_ON_SEND)
		priv->mdr3 &= ~UART_OMAP_MDR3_DIR_POL;
	else
		priv->mdr3 |= UART_OMAP_MDR3_DIR_POL;

	serial_out(up, UART_OMAP_MDR3, priv->mdr3);

	return 0;
}

#ifdef CONFIG_SERIAL_8250_DMA
static int omap_8250_rx_dma(struct uart_8250_port *p);

 
static void __dma_rx_do_complete(struct uart_8250_port *p)
{
	struct uart_8250_dma    *dma = p->dma;
	struct tty_port         *tty_port = &p->port.state->port;
	struct omap8250_priv	*priv = p->port.private_data;
	struct dma_chan		*rxchan = dma->rxchan;
	dma_cookie_t		cookie;
	struct dma_tx_state     state;
	int                     count;
	int			ret;
	u32			reg;

	if (!dma->rx_running)
		goto out;

	cookie = dma->rx_cookie;
	dma->rx_running = 0;

	 
	if (priv->habit & UART_HAS_RHR_IT_DIS) {
		reg = serial_in(p, UART_OMAP_IER2);
		reg &= ~UART_OMAP_IER2_RHR_IT_DIS;
		serial_out(p, UART_OMAP_IER2, reg);
	}

	dmaengine_tx_status(rxchan, cookie, &state);

	count = dma->rx_size - state.residue + state.in_flight_bytes;
	if (count < dma->rx_size) {
		dmaengine_terminate_async(rxchan);

		 
		if (state.in_flight_bytes) {
			int poll_count = 25;

			while (dmaengine_tx_status(rxchan, cookie, NULL) &&
			       poll_count--)
				cpu_relax();

			if (poll_count == -1)
				dev_err(p->port.dev, "teardown incomplete\n");
		}
	}
	if (!count)
		goto out;
	ret = tty_insert_flip_string(tty_port, dma->rx_buf, count);

	p->port.icount.rx += ret;
	p->port.icount.buf_overrun += count - ret;
out:

	tty_flip_buffer_push(tty_port);
}

static void __dma_rx_complete(void *param)
{
	struct uart_8250_port *p = param;
	struct omap8250_priv *priv = p->port.private_data;
	struct uart_8250_dma *dma = p->dma;
	struct dma_tx_state     state;
	unsigned long flags;

	 
	spin_lock_irqsave(&p->port.lock, flags);

	 
	if (dmaengine_tx_status(dma->rxchan, dma->rx_cookie, &state) !=
			DMA_COMPLETE) {
		spin_unlock_irqrestore(&p->port.lock, flags);
		return;
	}
	__dma_rx_do_complete(p);
	if (!priv->throttled) {
		p->ier |= UART_IER_RLSI | UART_IER_RDI;
		serial_out(p, UART_IER, p->ier);
		if (!(priv->habit & UART_HAS_EFR2))
			omap_8250_rx_dma(p);
	}

	spin_unlock_irqrestore(&p->port.lock, flags);
}

static void omap_8250_rx_dma_flush(struct uart_8250_port *p)
{
	struct omap8250_priv	*priv = p->port.private_data;
	struct uart_8250_dma	*dma = p->dma;
	struct dma_tx_state     state;
	unsigned long		flags;
	int ret;

	spin_lock_irqsave(&priv->rx_dma_lock, flags);

	if (!dma->rx_running) {
		spin_unlock_irqrestore(&priv->rx_dma_lock, flags);
		return;
	}

	ret = dmaengine_tx_status(dma->rxchan, dma->rx_cookie, &state);
	if (ret == DMA_IN_PROGRESS) {
		ret = dmaengine_pause(dma->rxchan);
		if (WARN_ON_ONCE(ret))
			priv->rx_dma_broken = true;
	}
	__dma_rx_do_complete(p);
	spin_unlock_irqrestore(&priv->rx_dma_lock, flags);
}

static int omap_8250_rx_dma(struct uart_8250_port *p)
{
	struct omap8250_priv		*priv = p->port.private_data;
	struct uart_8250_dma            *dma = p->dma;
	int				err = 0;
	struct dma_async_tx_descriptor  *desc;
	unsigned long			flags;
	u32				reg;

	 
	lockdep_assert_held_once(&p->port.lock);

	if (priv->rx_dma_broken)
		return -EINVAL;

	spin_lock_irqsave(&priv->rx_dma_lock, flags);

	if (dma->rx_running) {
		enum dma_status state;

		state = dmaengine_tx_status(dma->rxchan, dma->rx_cookie, NULL);
		if (state == DMA_COMPLETE) {
			 
			p->ier &= ~(UART_IER_RLSI | UART_IER_RDI);
			serial_out(p, UART_IER, p->ier);
		}
		goto out;
	}

	desc = dmaengine_prep_slave_single(dma->rxchan, dma->rx_addr,
					   dma->rx_size, DMA_DEV_TO_MEM,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		err = -EBUSY;
		goto out;
	}

	dma->rx_running = 1;
	desc->callback = __dma_rx_complete;
	desc->callback_param = p;

	dma->rx_cookie = dmaengine_submit(desc);

	 
	if (priv->habit & UART_HAS_RHR_IT_DIS) {
		reg = serial_in(p, UART_OMAP_IER2);
		reg |= UART_OMAP_IER2_RHR_IT_DIS;
		serial_out(p, UART_OMAP_IER2, reg);
	}

	dma_async_issue_pending(dma->rxchan);
out:
	spin_unlock_irqrestore(&priv->rx_dma_lock, flags);
	return err;
}

static int omap_8250_tx_dma(struct uart_8250_port *p);

static void omap_8250_dma_tx_complete(void *param)
{
	struct uart_8250_port	*p = param;
	struct uart_8250_dma	*dma = p->dma;
	struct circ_buf		*xmit = &p->port.state->xmit;
	unsigned long		flags;
	bool			en_thri = false;
	struct omap8250_priv	*priv = p->port.private_data;

	dma_sync_single_for_cpu(dma->txchan->device->dev, dma->tx_addr,
				UART_XMIT_SIZE, DMA_TO_DEVICE);

	spin_lock_irqsave(&p->port.lock, flags);

	dma->tx_running = 0;

	uart_xmit_advance(&p->port, dma->tx_size);

	if (priv->delayed_restore) {
		priv->delayed_restore = 0;
		omap8250_restore_regs(p);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&p->port);

	if (!uart_circ_empty(xmit) && !uart_tx_stopped(&p->port)) {
		int ret;

		ret = omap_8250_tx_dma(p);
		if (ret)
			en_thri = true;
	} else if (p->capabilities & UART_CAP_RPM) {
		en_thri = true;
	}

	if (en_thri) {
		dma->tx_err = 1;
		serial8250_set_THRI(p);
	}

	spin_unlock_irqrestore(&p->port.lock, flags);
}

static int omap_8250_tx_dma(struct uart_8250_port *p)
{
	struct uart_8250_dma		*dma = p->dma;
	struct omap8250_priv		*priv = p->port.private_data;
	struct circ_buf			*xmit = &p->port.state->xmit;
	struct dma_async_tx_descriptor	*desc;
	unsigned int	skip_byte = 0;
	int ret;

	if (dma->tx_running)
		return 0;
	if (uart_tx_stopped(&p->port) || uart_circ_empty(xmit)) {

		 
		if (dma->tx_err || p->capabilities & UART_CAP_RPM) {
			ret = -EBUSY;
			goto err;
		}
		serial8250_clear_THRI(p);
		return 0;
	}

	dma->tx_size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
	if (priv->habit & OMAP_DMA_TX_KICK) {
		u8 tx_lvl;

		 
		tx_lvl = serial_in(p, UART_OMAP_TX_LVL);
		if (tx_lvl == p->tx_loadsz) {
			ret = -EBUSY;
			goto err;
		}
		if (dma->tx_size < 4) {
			ret = -EINVAL;
			goto err;
		}
		skip_byte = 1;
	}

	desc = dmaengine_prep_slave_single(dma->txchan,
			dma->tx_addr + xmit->tail + skip_byte,
			dma->tx_size - skip_byte, DMA_MEM_TO_DEV,
			DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		ret = -EBUSY;
		goto err;
	}

	dma->tx_running = 1;

	desc->callback = omap_8250_dma_tx_complete;
	desc->callback_param = p;

	dma->tx_cookie = dmaengine_submit(desc);

	dma_sync_single_for_device(dma->txchan->device->dev, dma->tx_addr,
				   UART_XMIT_SIZE, DMA_TO_DEVICE);

	dma_async_issue_pending(dma->txchan);
	if (dma->tx_err)
		dma->tx_err = 0;

	serial8250_clear_THRI(p);
	if (skip_byte)
		serial_out(p, UART_TX, xmit->buf[xmit->tail]);
	return 0;
err:
	dma->tx_err = 1;
	return ret;
}

static bool handle_rx_dma(struct uart_8250_port *up, unsigned int iir)
{
	switch (iir & 0x3f) {
	case UART_IIR_RLSI:
	case UART_IIR_RX_TIMEOUT:
	case UART_IIR_RDI:
		omap_8250_rx_dma_flush(up);
		return true;
	}
	return omap_8250_rx_dma(up);
}

static u16 omap_8250_handle_rx_dma(struct uart_8250_port *up, u8 iir, u16 status)
{
	if ((status & (UART_LSR_DR | UART_LSR_BI)) &&
	    (iir & UART_IIR_RDI)) {
		if (handle_rx_dma(up, iir)) {
			status = serial8250_rx_chars(up, status);
			omap_8250_rx_dma(up);
		}
	}

	return status;
}

static void am654_8250_handle_rx_dma(struct uart_8250_port *up, u8 iir,
				     u16 status)
{
	 
	lockdep_assert_held_once(&up->port.lock);

	 
	if ((status & (UART_LSR_DR | UART_LSR_BI)) &&
	    (up->ier & UART_IER_RDI)) {
		omap_8250_rx_dma(up);
		serial_out(up, UART_OMAP_EFR2, UART_OMAP_EFR2_TIMEOUT_BEHAVE);
	} else if ((iir & 0x3f) == UART_IIR_RX_TIMEOUT) {
		 
		up->ier &= ~(UART_IER_RLSI | UART_IER_RDI);
		serial_out(up, UART_IER, up->ier);
		omap_8250_rx_dma_flush(up);
		serial_in(up, UART_IIR);
		serial_out(up, UART_OMAP_EFR2, 0x0);
		up->ier |= UART_IER_RLSI | UART_IER_RDI;
		serial_out(up, UART_IER, up->ier);
	}
}

 
static int omap_8250_dma_handle_irq(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct omap8250_priv *priv = up->port.private_data;
	u16 status;
	u8 iir;

	serial8250_rpm_get(up);

	iir = serial_port_in(port, UART_IIR);
	if (iir & UART_IIR_NO_INT) {
		serial8250_rpm_put(up);
		return IRQ_HANDLED;
	}

	spin_lock(&port->lock);

	status = serial_port_in(port, UART_LSR);

	if ((iir & 0x3f) != UART_IIR_THRI) {
		if (priv->habit & UART_HAS_EFR2)
			am654_8250_handle_rx_dma(up, iir, status);
		else
			status = omap_8250_handle_rx_dma(up, iir, status);
	}

	serial8250_modem_status(up);
	if (status & UART_LSR_THRE && up->dma->tx_err) {
		if (uart_tx_stopped(&up->port) ||
		    uart_circ_empty(&up->port.state->xmit)) {
			up->dma->tx_err = 0;
			serial8250_tx_chars(up);
		} else  {
			 
			if (omap_8250_tx_dma(up))
				serial8250_tx_chars(up);
		}
	}

	uart_unlock_and_check_sysrq(port);

	serial8250_rpm_put(up);
	return 1;
}

static bool the_no_dma_filter_fn(struct dma_chan *chan, void *param)
{
	return false;
}

#else

static inline int omap_8250_rx_dma(struct uart_8250_port *p)
{
	return -EINVAL;
}
#endif

static int omap8250_no_handle_irq(struct uart_port *port)
{
	 
	WARN_ONCE(1, "Unexpected irq handling before port startup\n");
	return 0;
}

static struct omap8250_dma_params am654_dma = {
	.rx_size = SZ_2K,
	.rx_trigger = 1,
	.tx_trigger = TX_TRIGGER,
};

static struct omap8250_dma_params am33xx_dma = {
	.rx_size = RX_TRIGGER,
	.rx_trigger = RX_TRIGGER,
	.tx_trigger = TX_TRIGGER,
};

static struct omap8250_platdata am654_platdata = {
	.dma_params	= &am654_dma,
	.habit		= UART_HAS_EFR2 | UART_HAS_RHR_IT_DIS |
			  UART_RX_TIMEOUT_QUIRK | UART_HAS_NATIVE_RS485,
};

static struct omap8250_platdata am33xx_platdata = {
	.dma_params	= &am33xx_dma,
	.habit		= OMAP_DMA_TX_KICK | UART_ERRATA_CLOCK_DISABLE,
};

static struct omap8250_platdata omap4_platdata = {
	.dma_params	= &am33xx_dma,
	.habit		= UART_ERRATA_CLOCK_DISABLE,
};

static const struct of_device_id omap8250_dt_ids[] = {
	{ .compatible = "ti,am654-uart", .data = &am654_platdata, },
	{ .compatible = "ti,omap2-uart" },
	{ .compatible = "ti,omap3-uart" },
	{ .compatible = "ti,omap4-uart", .data = &omap4_platdata, },
	{ .compatible = "ti,am3352-uart", .data = &am33xx_platdata, },
	{ .compatible = "ti,am4372-uart", .data = &am33xx_platdata, },
	{ .compatible = "ti,dra742-uart", .data = &omap4_platdata, },
	{},
};
MODULE_DEVICE_TABLE(of, omap8250_dt_ids);

static int omap8250_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct omap8250_priv *priv;
	const struct omap8250_platdata *pdata;
	struct uart_8250_port up;
	struct resource *regs;
	void __iomem *membase;
	int irq, ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "missing registers\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	membase = devm_ioremap(&pdev->dev, regs->start,
				       resource_size(regs));
	if (!membase)
		return -ENODEV;

	memset(&up, 0, sizeof(up));
	up.port.dev = &pdev->dev;
	up.port.mapbase = regs->start;
	up.port.membase = membase;
	up.port.irq = irq;
	 
	up.port.type = PORT_8250;
	up.port.iotype = UPIO_MEM;
	up.port.flags = UPF_FIXED_PORT | UPF_FIXED_TYPE | UPF_SOFT_FLOW |
		UPF_HARD_FLOW;
	up.port.private_data = priv;

	up.port.regshift = OMAP_UART_REGSHIFT;
	up.port.fifosize = 64;
	up.tx_loadsz = 64;
	up.capabilities = UART_CAP_FIFO;
#ifdef CONFIG_PM
	 
	up.capabilities |= UART_CAP_RPM;
#endif
	up.port.set_termios = omap_8250_set_termios;
	up.port.set_mctrl = omap8250_set_mctrl;
	up.port.pm = omap_8250_pm;
	up.port.startup = omap_8250_startup;
	up.port.shutdown = omap_8250_shutdown;
	up.port.throttle = omap_8250_throttle;
	up.port.unthrottle = omap_8250_unthrottle;
	up.port.rs485_config = omap8250_rs485_config;
	 
	up.port.rs485_supported = serial8250_em485_supported;
	up.rs485_start_tx = serial8250_em485_start_tx;
	up.rs485_stop_tx = serial8250_em485_stop_tx;
	up.port.has_sysrq = IS_ENABLED(CONFIG_SERIAL_8250_CONSOLE);

	ret = of_alias_get_id(np, "serial");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias\n");
		return ret;
	}
	up.port.line = ret;

	if (of_property_read_u32(np, "clock-frequency", &up.port.uartclk)) {
		struct clk *clk;

		clk = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(clk)) {
			if (PTR_ERR(clk) == -EPROBE_DEFER)
				return -EPROBE_DEFER;
		} else {
			up.port.uartclk = clk_get_rate(clk);
		}
	}

	if (of_property_read_u32(np, "overrun-throttle-ms",
				 &up.overrun_backoff_time_ms) != 0)
		up.overrun_backoff_time_ms = 0;

	pdata = of_device_get_match_data(&pdev->dev);
	if (pdata)
		priv->habit |= pdata->habit;

	if (!up.port.uartclk) {
		up.port.uartclk = DEFAULT_CLK_SPEED;
		dev_warn(&pdev->dev,
			 "No clock speed specified: using default: %d\n",
			 DEFAULT_CLK_SPEED);
	}

	priv->membase = membase;
	priv->line = -ENODEV;
	priv->latency = PM_QOS_CPU_LATENCY_DEFAULT_VALUE;
	priv->calc_latency = PM_QOS_CPU_LATENCY_DEFAULT_VALUE;
	cpu_latency_qos_add_request(&priv->pm_qos_request, priv->latency);
	INIT_WORK(&priv->qos_work, omap8250_uart_qos_work);

	spin_lock_init(&priv->rx_dma_lock);

	platform_set_drvdata(pdev, priv);

	device_init_wakeup(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_use_autosuspend(&pdev->dev);

	 
	if (!of_get_available_child_count(pdev->dev.of_node))
		pm_runtime_set_autosuspend_delay(&pdev->dev, -1);

	pm_runtime_irq_safe(&pdev->dev);

	pm_runtime_get_sync(&pdev->dev);

	omap_serial_fill_features_erratas(&up, priv);
	up.port.handle_irq = omap8250_no_handle_irq;
	priv->rx_trigger = RX_TRIGGER;
	priv->tx_trigger = TX_TRIGGER;
#ifdef CONFIG_SERIAL_8250_DMA
	 
	ret = of_property_count_strings(np, "dma-names");
	if (ret == 2) {
		struct omap8250_dma_params *dma_params = NULL;
		struct uart_8250_dma *dma = &priv->omap8250_dma;

		dma->fn = the_no_dma_filter_fn;
		dma->tx_dma = omap_8250_tx_dma;
		dma->rx_dma = omap_8250_rx_dma;
		if (pdata)
			dma_params = pdata->dma_params;

		if (dma_params) {
			dma->rx_size = dma_params->rx_size;
			dma->rxconf.src_maxburst = dma_params->rx_trigger;
			dma->txconf.dst_maxburst = dma_params->tx_trigger;
			priv->rx_trigger = dma_params->rx_trigger;
			priv->tx_trigger = dma_params->tx_trigger;
		} else {
			dma->rx_size = RX_TRIGGER;
			dma->rxconf.src_maxburst = RX_TRIGGER;
			dma->txconf.dst_maxburst = TX_TRIGGER;
		}
	}
#endif

	irq_set_status_flags(irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(&pdev->dev, irq, omap8250_irq, 0,
			       dev_name(&pdev->dev), priv);
	if (ret < 0)
		return ret;

	priv->wakeirq = irq_of_parse_and_map(np, 1);

	ret = serial8250_register_8250_port(&up);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to register 8250 port\n");
		goto err;
	}
	priv->line = ret;
	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);
	return 0;
err:
	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_put_sync(&pdev->dev);
	flush_work(&priv->qos_work);
	pm_runtime_disable(&pdev->dev);
	cpu_latency_qos_remove_request(&priv->pm_qos_request);
	return ret;
}

static int omap8250_remove(struct platform_device *pdev)
{
	struct omap8250_priv *priv = platform_get_drvdata(pdev);
	struct uart_8250_port *up;
	int err;

	err = pm_runtime_resume_and_get(&pdev->dev);
	if (err)
		dev_err(&pdev->dev, "Failed to resume hardware\n");

	up = serial8250_get_port(priv->line);
	omap_8250_shutdown(&up->port);
	serial8250_unregister_port(priv->line);
	priv->line = -ENODEV;
	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_put_sync(&pdev->dev);
	flush_work(&priv->qos_work);
	pm_runtime_disable(&pdev->dev);
	cpu_latency_qos_remove_request(&priv->pm_qos_request);
	device_init_wakeup(&pdev->dev, false);
	return 0;
}

static int omap8250_prepare(struct device *dev)
{
	struct omap8250_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return 0;
	priv->is_suspending = true;
	return 0;
}

static void omap8250_complete(struct device *dev)
{
	struct omap8250_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return;
	priv->is_suspending = false;
}

static int omap8250_suspend(struct device *dev)
{
	struct omap8250_priv *priv = dev_get_drvdata(dev);
	struct uart_8250_port *up = serial8250_get_port(priv->line);
	int err = 0;

	serial8250_suspend_port(priv->line);

	err = pm_runtime_resume_and_get(dev);
	if (err)
		return err;
	if (!device_may_wakeup(dev))
		priv->wer = 0;
	serial_out(up, UART_OMAP_WER, priv->wer);
	if (uart_console(&up->port) && console_suspend_enabled)
		err = pm_runtime_force_suspend(dev);
	flush_work(&priv->qos_work);

	return err;
}

static int omap8250_resume(struct device *dev)
{
	struct omap8250_priv *priv = dev_get_drvdata(dev);
	struct uart_8250_port *up = serial8250_get_port(priv->line);
	int err;

	if (uart_console(&up->port) && console_suspend_enabled) {
		err = pm_runtime_force_resume(dev);
		if (err)
			return err;
	}

	serial8250_resume_port(priv->line);
	 
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;
}

static int omap8250_lost_context(struct uart_8250_port *up)
{
	u32 val;

	val = serial_in(up, UART_OMAP_SCR);
	 
	if (!val)
		return 1;
	return 0;
}

static void uart_write(struct omap8250_priv *priv, u32 reg, u32 val)
{
	writel(val, priv->membase + (reg << OMAP_UART_REGSHIFT));
}

 
static int omap8250_soft_reset(struct device *dev)
{
	struct omap8250_priv *priv = dev_get_drvdata(dev);
	int timeout = 100;
	int sysc;
	int syss;

	 
	uart_write(priv, UART_OMAP_SCR, OMAP_UART_SCR_DMAMODE_1);
	uart_write(priv, UART_OMAP_SCR,
		   OMAP_UART_SCR_DMAMODE_1 | OMAP_UART_SCR_DMAMODE_CTL);

	sysc = uart_read(priv, UART_OMAP_SYSC);

	 
	sysc |= OMAP_UART_SYSC_SOFTRESET;
	uart_write(priv, UART_OMAP_SYSC, sysc);

	 
	do {
		udelay(1);
		syss = uart_read(priv, UART_OMAP_SYSS);
	} while (--timeout && !(syss & OMAP_UART_SYSS_RESETDONE));

	if (!timeout) {
		dev_err(dev, "timed out waiting for reset done\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int omap8250_runtime_suspend(struct device *dev)
{
	struct omap8250_priv *priv = dev_get_drvdata(dev);
	struct uart_8250_port *up = NULL;

	if (priv->line >= 0)
		up = serial8250_get_port(priv->line);

	if (priv->habit & UART_ERRATA_CLOCK_DISABLE) {
		int ret;

		ret = omap8250_soft_reset(dev);
		if (ret)
			return ret;

		if (up) {
			 
			omap8250_update_mdr1(up, priv);
			 
			serial_out(up, UART_OMAP_WER, priv->wer);
		}
	}

	if (up && up->dma && up->dma->rxchan)
		omap_8250_rx_dma_flush(up);

	priv->latency = PM_QOS_CPU_LATENCY_DEFAULT_VALUE;
	schedule_work(&priv->qos_work);

	return 0;
}

static int omap8250_runtime_resume(struct device *dev)
{
	struct omap8250_priv *priv = dev_get_drvdata(dev);
	struct uart_8250_port *up = NULL;

	if (priv->line >= 0)
		up = serial8250_get_port(priv->line);

	if (up && omap8250_lost_context(up)) {
		spin_lock_irq(&up->port.lock);
		omap8250_restore_regs(up);
		spin_unlock_irq(&up->port.lock);
	}

	if (up && up->dma && up->dma->rxchan && !(priv->habit & UART_HAS_EFR2)) {
		spin_lock_irq(&up->port.lock);
		omap_8250_rx_dma(up);
		spin_unlock_irq(&up->port.lock);
	}

	priv->latency = priv->calc_latency;
	schedule_work(&priv->qos_work);
	return 0;
}

#ifdef CONFIG_SERIAL_8250_OMAP_TTYO_FIXUP
static int __init omap8250_console_fixup(void)
{
	char *omap_str;
	char *options;
	u8 idx;

	if (strstr(boot_command_line, "console=ttyS"))
		 
		return 0;

	omap_str = strstr(boot_command_line, "console=ttyO");
	if (!omap_str)
		 
		return 0;

	omap_str += 12;
	if ('0' <= *omap_str && *omap_str <= '9')
		idx = *omap_str - '0';
	else
		return 0;

	omap_str++;
	if (omap_str[0] == ',') {
		omap_str++;
		options = omap_str;
	} else {
		options = NULL;
	}

	add_preferred_console("ttyS", idx, options);
	pr_err("WARNING: Your 'console=ttyO%d' has been replaced by 'ttyS%d'\n",
	       idx, idx);
	pr_err("This ensures that you still see kernel messages. Please\n");
	pr_err("update your kernel commandline.\n");
	return 0;
}
console_initcall(omap8250_console_fixup);
#endif

static const struct dev_pm_ops omap8250_dev_pm_ops = {
	SYSTEM_SLEEP_PM_OPS(omap8250_suspend, omap8250_resume)
	RUNTIME_PM_OPS(omap8250_runtime_suspend,
			   omap8250_runtime_resume, NULL)
	.prepare        = pm_sleep_ptr(omap8250_prepare),
	.complete       = pm_sleep_ptr(omap8250_complete),
};

static struct platform_driver omap8250_platform_driver = {
	.driver = {
		.name		= "omap8250",
		.pm		= pm_ptr(&omap8250_dev_pm_ops),
		.of_match_table = omap8250_dt_ids,
	},
	.probe			= omap8250_probe,
	.remove			= omap8250_remove,
};
module_platform_driver(omap8250_platform_driver);

MODULE_AUTHOR("Sebastian Andrzej Siewior");
MODULE_DESCRIPTION("OMAP 8250 Driver");
MODULE_LICENSE("GPL v2");
