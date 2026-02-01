
 

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/console.h>
#include <linux/gpio/consumer.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/ratelimit.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/nmi.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/ktime.h>

#include <asm/io.h>
#include <asm/irq.h>

#include "8250.h"

 
#define UART_NPCM_TOR          7
#define UART_NPCM_TOIE         BIT(7)   

 
#if 0
#define DEBUG_AUTOCONF(fmt...)	printk(fmt)
#else
#define DEBUG_AUTOCONF(fmt...)	do { } while (0)
#endif

 
static const struct serial8250_config uart_config[] = {
	[PORT_UNKNOWN] = {
		.name		= "unknown",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_8250] = {
		.name		= "8250",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16450] = {
		.name		= "16450",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16550] = {
		.name		= "16550",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16550A] = {
		.name		= "16550A",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.rxtrig_bytes	= {1, 4, 8, 14},
		.flags		= UART_CAP_FIFO,
	},
	[PORT_CIRRUS] = {
		.name		= "Cirrus",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16650] = {
		.name		= "ST16650",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
		.flags		= UART_CAP_FIFO | UART_CAP_EFR | UART_CAP_SLEEP,
	},
	[PORT_16650V2] = {
		.name		= "ST16650V2",
		.fifo_size	= 32,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01 |
				  UART_FCR_T_TRIG_00,
		.rxtrig_bytes	= {8, 16, 24, 28},
		.flags		= UART_CAP_FIFO | UART_CAP_EFR | UART_CAP_SLEEP,
	},
	[PORT_16750] = {
		.name		= "TI16750",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10 |
				  UART_FCR7_64BYTE,
		.rxtrig_bytes	= {1, 16, 32, 56},
		.flags		= UART_CAP_FIFO | UART_CAP_SLEEP | UART_CAP_AFE,
	},
	[PORT_STARTECH] = {
		.name		= "Startech",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16C950] = {
		.name		= "16C950/954",
		.fifo_size	= 128,
		.tx_loadsz	= 128,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01,
		.rxtrig_bytes	= {16, 32, 112, 120},
		 
		.flags		= UART_CAP_FIFO | UART_CAP_SLEEP,
	},
	[PORT_16654] = {
		.name		= "ST16654",
		.fifo_size	= 64,
		.tx_loadsz	= 32,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01 |
				  UART_FCR_T_TRIG_10,
		.rxtrig_bytes	= {8, 16, 56, 60},
		.flags		= UART_CAP_FIFO | UART_CAP_EFR | UART_CAP_SLEEP,
	},
	[PORT_16850] = {
		.name		= "XR16850",
		.fifo_size	= 128,
		.tx_loadsz	= 128,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_EFR | UART_CAP_SLEEP,
	},
	[PORT_RSA] = {
		.name		= "RSA",
		.fifo_size	= 2048,
		.tx_loadsz	= 2048,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_11,
		.flags		= UART_CAP_FIFO,
	},
	[PORT_NS16550A] = {
		.name		= "NS16550A",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_NATSEMI,
	},
	[PORT_XSCALE] = {
		.name		= "XScale",
		.fifo_size	= 32,
		.tx_loadsz	= 32,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_UUE | UART_CAP_RTOIE,
	},
	[PORT_OCTEON] = {
		.name		= "OCTEON",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO,
	},
	[PORT_AR7] = {
		.name		= "AR7",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_00,
		.flags		= UART_CAP_FIFO  ,
	},
	[PORT_U6_16550A] = {
		.name		= "U6_16550A",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_AFE,
	},
	[PORT_TEGRA] = {
		.name		= "Tegra",
		.fifo_size	= 32,
		.tx_loadsz	= 8,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01 |
				  UART_FCR_T_TRIG_01,
		.rxtrig_bytes	= {1, 4, 8, 14},
		.flags		= UART_CAP_FIFO | UART_CAP_RTOIE,
	},
	[PORT_XR17D15X] = {
		.name		= "XR17D15X",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_AFE | UART_CAP_EFR |
				  UART_CAP_SLEEP,
	},
	[PORT_XR17V35X] = {
		.name		= "XR17V35X",
		.fifo_size	= 256,
		.tx_loadsz	= 256,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_11 |
				  UART_FCR_T_TRIG_11,
		.flags		= UART_CAP_FIFO | UART_CAP_AFE | UART_CAP_EFR |
				  UART_CAP_SLEEP,
	},
	[PORT_LPC3220] = {
		.name		= "LPC3220",
		.fifo_size	= 64,
		.tx_loadsz	= 32,
		.fcr		= UART_FCR_DMA_SELECT | UART_FCR_ENABLE_FIFO |
				  UART_FCR_R_TRIG_00 | UART_FCR_T_TRIG_00,
		.flags		= UART_CAP_FIFO,
	},
	[PORT_BRCM_TRUMANAGE] = {
		.name		= "TruManage",
		.fifo_size	= 1,
		.tx_loadsz	= 1024,
		.flags		= UART_CAP_HFIFO,
	},
	[PORT_8250_CIR] = {
		.name		= "CIR port"
	},
	[PORT_ALTR_16550_F32] = {
		.name		= "Altera 16550 FIFO32",
		.fifo_size	= 32,
		.tx_loadsz	= 32,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.rxtrig_bytes	= {1, 8, 16, 30},
		.flags		= UART_CAP_FIFO | UART_CAP_AFE,
	},
	[PORT_ALTR_16550_F64] = {
		.name		= "Altera 16550 FIFO64",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.rxtrig_bytes	= {1, 16, 32, 62},
		.flags		= UART_CAP_FIFO | UART_CAP_AFE,
	},
	[PORT_ALTR_16550_F128] = {
		.name		= "Altera 16550 FIFO128",
		.fifo_size	= 128,
		.tx_loadsz	= 128,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.rxtrig_bytes	= {1, 32, 64, 126},
		.flags		= UART_CAP_FIFO | UART_CAP_AFE,
	},
	 
	[PORT_16550A_FSL64] = {
		.name		= "16550A_FSL64",
		.fifo_size	= 64,
		.tx_loadsz	= 63,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10 |
				  UART_FCR7_64BYTE,
		.flags		= UART_CAP_FIFO | UART_CAP_NOTEMT,
	},
	[PORT_RT2880] = {
		.name		= "Palmchip BK-3103",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.rxtrig_bytes	= {1, 4, 8, 14},
		.flags		= UART_CAP_FIFO,
	},
	[PORT_DA830] = {
		.name		= "TI DA8xx/66AK2x",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_DMA_SELECT | UART_FCR_ENABLE_FIFO |
				  UART_FCR_R_TRIG_10,
		.rxtrig_bytes	= {1, 4, 8, 14},
		.flags		= UART_CAP_FIFO | UART_CAP_AFE,
	},
	[PORT_MTK_BTIF] = {
		.name		= "MediaTek BTIF",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO |
				  UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT,
		.flags		= UART_CAP_FIFO,
	},
	[PORT_NPCM] = {
		.name		= "Nuvoton 16550",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10 |
				  UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT,
		.rxtrig_bytes	= {1, 4, 8, 14},
		.flags		= UART_CAP_FIFO,
	},
	[PORT_SUNIX] = {
		.name		= "Sunix",
		.fifo_size	= 128,
		.tx_loadsz	= 128,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.rxtrig_bytes	= {1, 32, 64, 112},
		.flags		= UART_CAP_FIFO | UART_CAP_SLEEP,
	},
	[PORT_ASPEED_VUART] = {
		.name		= "ASPEED VUART",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_00,
		.rxtrig_bytes	= {1, 4, 8, 14},
		.flags		= UART_CAP_FIFO,
	},
	[PORT_MCHP16550A] = {
		.name           = "MCHP16550A",
		.fifo_size      = 256,
		.tx_loadsz      = 256,
		.fcr            = UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01,
		.rxtrig_bytes   = {2, 66, 130, 194},
		.flags          = UART_CAP_FIFO,
	},
	[PORT_BCM7271] = {
		.name		= "Broadcom BCM7271 UART",
		.fifo_size	= 32,
		.tx_loadsz	= 32,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01,
		.rxtrig_bytes	= {1, 8, 16, 30},
		.flags		= UART_CAP_FIFO | UART_CAP_AFE,
	},
};

 
static u32 default_serial_dl_read(struct uart_8250_port *up)
{
	 
	unsigned char dll = serial_in(up, UART_DLL);
	unsigned char dlm = serial_in(up, UART_DLM);

	return dll | dlm << 8;
}

 
static void default_serial_dl_write(struct uart_8250_port *up, u32 value)
{
	serial_out(up, UART_DLL, value & 0xff);
	serial_out(up, UART_DLM, value >> 8 & 0xff);
}

static unsigned int hub6_serial_in(struct uart_port *p, int offset)
{
	offset = offset << p->regshift;
	outb(p->hub6 - 1 + offset, p->iobase);
	return inb(p->iobase + 1);
}

static void hub6_serial_out(struct uart_port *p, int offset, int value)
{
	offset = offset << p->regshift;
	outb(p->hub6 - 1 + offset, p->iobase);
	outb(value, p->iobase + 1);
}

static unsigned int mem_serial_in(struct uart_port *p, int offset)
{
	offset = offset << p->regshift;
	return readb(p->membase + offset);
}

static void mem_serial_out(struct uart_port *p, int offset, int value)
{
	offset = offset << p->regshift;
	writeb(value, p->membase + offset);
}

static void mem16_serial_out(struct uart_port *p, int offset, int value)
{
	offset = offset << p->regshift;
	writew(value, p->membase + offset);
}

static unsigned int mem16_serial_in(struct uart_port *p, int offset)
{
	offset = offset << p->regshift;
	return readw(p->membase + offset);
}

static void mem32_serial_out(struct uart_port *p, int offset, int value)
{
	offset = offset << p->regshift;
	writel(value, p->membase + offset);
}

static unsigned int mem32_serial_in(struct uart_port *p, int offset)
{
	offset = offset << p->regshift;
	return readl(p->membase + offset);
}

static void mem32be_serial_out(struct uart_port *p, int offset, int value)
{
	offset = offset << p->regshift;
	iowrite32be(value, p->membase + offset);
}

static unsigned int mem32be_serial_in(struct uart_port *p, int offset)
{
	offset = offset << p->regshift;
	return ioread32be(p->membase + offset);
}

static unsigned int io_serial_in(struct uart_port *p, int offset)
{
	offset = offset << p->regshift;
	return inb(p->iobase + offset);
}

static void io_serial_out(struct uart_port *p, int offset, int value)
{
	offset = offset << p->regshift;
	outb(value, p->iobase + offset);
}

static int serial8250_default_handle_irq(struct uart_port *port);

static void set_io_from_upio(struct uart_port *p)
{
	struct uart_8250_port *up = up_to_u8250p(p);

	up->dl_read = default_serial_dl_read;
	up->dl_write = default_serial_dl_write;

	switch (p->iotype) {
	case UPIO_HUB6:
		p->serial_in = hub6_serial_in;
		p->serial_out = hub6_serial_out;
		break;

	case UPIO_MEM:
		p->serial_in = mem_serial_in;
		p->serial_out = mem_serial_out;
		break;

	case UPIO_MEM16:
		p->serial_in = mem16_serial_in;
		p->serial_out = mem16_serial_out;
		break;

	case UPIO_MEM32:
		p->serial_in = mem32_serial_in;
		p->serial_out = mem32_serial_out;
		break;

	case UPIO_MEM32BE:
		p->serial_in = mem32be_serial_in;
		p->serial_out = mem32be_serial_out;
		break;

	default:
		p->serial_in = io_serial_in;
		p->serial_out = io_serial_out;
		break;
	}
	 
	up->cur_iotype = p->iotype;
	p->handle_irq = serial8250_default_handle_irq;
}

static void
serial_port_out_sync(struct uart_port *p, int offset, int value)
{
	switch (p->iotype) {
	case UPIO_MEM:
	case UPIO_MEM16:
	case UPIO_MEM32:
	case UPIO_MEM32BE:
	case UPIO_AU:
		p->serial_out(p, offset, value);
		p->serial_in(p, UART_LCR);	 
		break;
	default:
		p->serial_out(p, offset, value);
	}
}

 
static void serial8250_clear_fifos(struct uart_8250_port *p)
{
	if (p->capabilities & UART_CAP_FIFO) {
		serial_out(p, UART_FCR, UART_FCR_ENABLE_FIFO);
		serial_out(p, UART_FCR, UART_FCR_ENABLE_FIFO |
			       UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
		serial_out(p, UART_FCR, 0);
	}
}

static enum hrtimer_restart serial8250_em485_handle_start_tx(struct hrtimer *t);
static enum hrtimer_restart serial8250_em485_handle_stop_tx(struct hrtimer *t);

void serial8250_clear_and_reinit_fifos(struct uart_8250_port *p)
{
	serial8250_clear_fifos(p);
	serial_out(p, UART_FCR, p->fcr);
}
EXPORT_SYMBOL_GPL(serial8250_clear_and_reinit_fifos);

void serial8250_rpm_get(struct uart_8250_port *p)
{
	if (!(p->capabilities & UART_CAP_RPM))
		return;
	pm_runtime_get_sync(p->port.dev);
}
EXPORT_SYMBOL_GPL(serial8250_rpm_get);

void serial8250_rpm_put(struct uart_8250_port *p)
{
	if (!(p->capabilities & UART_CAP_RPM))
		return;
	pm_runtime_mark_last_busy(p->port.dev);
	pm_runtime_put_autosuspend(p->port.dev);
}
EXPORT_SYMBOL_GPL(serial8250_rpm_put);

 
static int serial8250_em485_init(struct uart_8250_port *p)
{
	 
	lockdep_assert_held_once(&p->port.lock);

	if (p->em485)
		goto deassert_rts;

	p->em485 = kmalloc(sizeof(struct uart_8250_em485), GFP_ATOMIC);
	if (!p->em485)
		return -ENOMEM;

	hrtimer_init(&p->em485->stop_tx_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	hrtimer_init(&p->em485->start_tx_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	p->em485->stop_tx_timer.function = &serial8250_em485_handle_stop_tx;
	p->em485->start_tx_timer.function = &serial8250_em485_handle_start_tx;
	p->em485->port = p;
	p->em485->active_timer = NULL;
	p->em485->tx_stopped = true;

deassert_rts:
	if (p->em485->tx_stopped)
		p->rs485_stop_tx(p);

	return 0;
}

 
void serial8250_em485_destroy(struct uart_8250_port *p)
{
	if (!p->em485)
		return;

	hrtimer_cancel(&p->em485->start_tx_timer);
	hrtimer_cancel(&p->em485->stop_tx_timer);

	kfree(p->em485);
	p->em485 = NULL;
}
EXPORT_SYMBOL_GPL(serial8250_em485_destroy);

struct serial_rs485 serial8250_em485_supported = {
	.flags = SER_RS485_ENABLED | SER_RS485_RTS_ON_SEND | SER_RS485_RTS_AFTER_SEND |
		 SER_RS485_TERMINATE_BUS | SER_RS485_RX_DURING_TX,
	.delay_rts_before_send = 1,
	.delay_rts_after_send = 1,
};
EXPORT_SYMBOL_GPL(serial8250_em485_supported);

 
int serial8250_em485_config(struct uart_port *port, struct ktermios *termios,
			    struct serial_rs485 *rs485)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	 
	if (!!(rs485->flags & SER_RS485_RTS_ON_SEND) ==
	    !!(rs485->flags & SER_RS485_RTS_AFTER_SEND)) {
		rs485->flags |= SER_RS485_RTS_ON_SEND;
		rs485->flags &= ~SER_RS485_RTS_AFTER_SEND;
	}

	 
	if (rs485->flags & SER_RS485_ENABLED)
		return serial8250_em485_init(up);

	serial8250_em485_destroy(up);
	return 0;
}
EXPORT_SYMBOL_GPL(serial8250_em485_config);

 
void serial8250_rpm_get_tx(struct uart_8250_port *p)
{
	unsigned char rpm_active;

	if (!(p->capabilities & UART_CAP_RPM))
		return;

	rpm_active = xchg(&p->rpm_tx_active, 1);
	if (rpm_active)
		return;
	pm_runtime_get_sync(p->port.dev);
}
EXPORT_SYMBOL_GPL(serial8250_rpm_get_tx);

void serial8250_rpm_put_tx(struct uart_8250_port *p)
{
	unsigned char rpm_active;

	if (!(p->capabilities & UART_CAP_RPM))
		return;

	rpm_active = xchg(&p->rpm_tx_active, 0);
	if (!rpm_active)
		return;
	pm_runtime_mark_last_busy(p->port.dev);
	pm_runtime_put_autosuspend(p->port.dev);
}
EXPORT_SYMBOL_GPL(serial8250_rpm_put_tx);

 
static void serial8250_set_sleep(struct uart_8250_port *p, int sleep)
{
	unsigned char lcr = 0, efr = 0;

	serial8250_rpm_get(p);

	if (p->capabilities & UART_CAP_SLEEP) {
		 
		spin_lock_irq(&p->port.lock);
		if (p->capabilities & UART_CAP_EFR) {
			lcr = serial_in(p, UART_LCR);
			efr = serial_in(p, UART_EFR);
			serial_out(p, UART_LCR, UART_LCR_CONF_MODE_B);
			serial_out(p, UART_EFR, UART_EFR_ECB);
			serial_out(p, UART_LCR, 0);
		}
		serial_out(p, UART_IER, sleep ? UART_IERX_SLEEP : 0);
		if (p->capabilities & UART_CAP_EFR) {
			serial_out(p, UART_LCR, UART_LCR_CONF_MODE_B);
			serial_out(p, UART_EFR, efr);
			serial_out(p, UART_LCR, lcr);
		}
		spin_unlock_irq(&p->port.lock);
	}

	serial8250_rpm_put(p);
}

static void serial8250_clear_IER(struct uart_8250_port *up)
{
	if (up->capabilities & UART_CAP_UUE)
		serial_out(up, UART_IER, UART_IER_UUE);
	else
		serial_out(up, UART_IER, 0);
}

#ifdef CONFIG_SERIAL_8250_RSA
 
static int __enable_rsa(struct uart_8250_port *up)
{
	unsigned char mode;
	int result;

	mode = serial_in(up, UART_RSA_MSR);
	result = mode & UART_RSA_MSR_FIFO;

	if (!result) {
		serial_out(up, UART_RSA_MSR, mode | UART_RSA_MSR_FIFO);
		mode = serial_in(up, UART_RSA_MSR);
		result = mode & UART_RSA_MSR_FIFO;
	}

	if (result)
		up->port.uartclk = SERIAL_RSA_BAUD_BASE * 16;

	return result;
}

static void enable_rsa(struct uart_8250_port *up)
{
	if (up->port.type == PORT_RSA) {
		if (up->port.uartclk != SERIAL_RSA_BAUD_BASE * 16) {
			spin_lock_irq(&up->port.lock);
			__enable_rsa(up);
			spin_unlock_irq(&up->port.lock);
		}
		if (up->port.uartclk == SERIAL_RSA_BAUD_BASE * 16)
			serial_out(up, UART_RSA_FRR, 0);
	}
}

 
static void disable_rsa(struct uart_8250_port *up)
{
	unsigned char mode;
	int result;

	if (up->port.type == PORT_RSA &&
	    up->port.uartclk == SERIAL_RSA_BAUD_BASE * 16) {
		spin_lock_irq(&up->port.lock);

		mode = serial_in(up, UART_RSA_MSR);
		result = !(mode & UART_RSA_MSR_FIFO);

		if (!result) {
			serial_out(up, UART_RSA_MSR, mode & ~UART_RSA_MSR_FIFO);
			mode = serial_in(up, UART_RSA_MSR);
			result = !(mode & UART_RSA_MSR_FIFO);
		}

		if (result)
			up->port.uartclk = SERIAL_RSA_BAUD_BASE_LO * 16;
		spin_unlock_irq(&up->port.lock);
	}
}
#endif  

 
static int size_fifo(struct uart_8250_port *up)
{
	unsigned char old_fcr, old_mcr, old_lcr;
	u32 old_dl;
	int count;

	old_lcr = serial_in(up, UART_LCR);
	serial_out(up, UART_LCR, 0);
	old_fcr = serial_in(up, UART_FCR);
	old_mcr = serial8250_in_MCR(up);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
		    UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial8250_out_MCR(up, UART_MCR_LOOP);
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_A);
	old_dl = serial_dl_read(up);
	serial_dl_write(up, 0x0001);
	serial_out(up, UART_LCR, UART_LCR_WLEN8);
	for (count = 0; count < 256; count++)
		serial_out(up, UART_TX, count);
	mdelay(20); 
	for (count = 0; (serial_in(up, UART_LSR) & UART_LSR_DR) &&
	     (count < 256); count++)
		serial_in(up, UART_RX);
	serial_out(up, UART_FCR, old_fcr);
	serial8250_out_MCR(up, old_mcr);
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_A);
	serial_dl_write(up, old_dl);
	serial_out(up, UART_LCR, old_lcr);

	return count;
}

 
static unsigned int autoconfig_read_divisor_id(struct uart_8250_port *p)
{
	unsigned char old_lcr;
	unsigned int id, old_dl;

	old_lcr = serial_in(p, UART_LCR);
	serial_out(p, UART_LCR, UART_LCR_CONF_MODE_A);
	old_dl = serial_dl_read(p);
	serial_dl_write(p, 0);
	id = serial_dl_read(p);
	serial_dl_write(p, old_dl);

	serial_out(p, UART_LCR, old_lcr);

	return id;
}

 
static void autoconfig_has_efr(struct uart_8250_port *up)
{
	unsigned int id1, id2, id3, rev;

	 
	up->capabilities |= UART_CAP_EFR | UART_CAP_SLEEP;

	 

	 
	up->acr = 0;
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_out(up, UART_EFR, UART_EFR_ECB);
	serial_out(up, UART_LCR, 0x00);
	id1 = serial_icr_read(up, UART_ID1);
	id2 = serial_icr_read(up, UART_ID2);
	id3 = serial_icr_read(up, UART_ID3);
	rev = serial_icr_read(up, UART_REV);

	DEBUG_AUTOCONF("950id=%02x:%02x:%02x:%02x ", id1, id2, id3, rev);

	if (id1 == 0x16 && id2 == 0xC9 &&
	    (id3 == 0x50 || id3 == 0x52 || id3 == 0x54)) {
		up->port.type = PORT_16C950;

		 
		if (id3 == 0x52 && rev == 0x01)
			up->bugs |= UART_BUG_QUOT;
		return;
	}

	 
	id1 = autoconfig_read_divisor_id(up);
	DEBUG_AUTOCONF("850id=%04x ", id1);

	id2 = id1 >> 8;
	if (id2 == 0x10 || id2 == 0x12 || id2 == 0x14) {
		up->port.type = PORT_16850;
		return;
	}

	 
	if (size_fifo(up) == 64)
		up->port.type = PORT_16654;
	else
		up->port.type = PORT_16650V2;
}

 
static void autoconfig_8250(struct uart_8250_port *up)
{
	unsigned char scratch, status1, status2;

	up->port.type = PORT_8250;

	scratch = serial_in(up, UART_SCR);
	serial_out(up, UART_SCR, 0xa5);
	status1 = serial_in(up, UART_SCR);
	serial_out(up, UART_SCR, 0x5a);
	status2 = serial_in(up, UART_SCR);
	serial_out(up, UART_SCR, scratch);

	if (status1 == 0xa5 && status2 == 0x5a)
		up->port.type = PORT_16450;
}

static int broken_efr(struct uart_8250_port *up)
{
	 
	if (autoconfig_read_divisor_id(up) == 0x0201 && size_fifo(up) == 16)
		return 1;

	return 0;
}

 
static void autoconfig_16550a(struct uart_8250_port *up)
{
	unsigned char status1, status2;
	unsigned int iersave;

	 
	lockdep_assert_held_once(&up->port.lock);

	up->port.type = PORT_16550A;
	up->capabilities |= UART_CAP_FIFO;

	if (!IS_ENABLED(CONFIG_SERIAL_8250_16550A_VARIANTS) &&
	    !(up->port.flags & UPF_FULL_PROBE))
		return;

	 
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_A);
	if (serial_in(up, UART_EFR) == 0) {
		serial_out(up, UART_EFR, 0xA8);
		if (serial_in(up, UART_EFR) != 0) {
			DEBUG_AUTOCONF("EFRv1 ");
			up->port.type = PORT_16650;
			up->capabilities |= UART_CAP_EFR | UART_CAP_SLEEP;
		} else {
			serial_out(up, UART_LCR, 0);
			serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
				   UART_FCR7_64BYTE);
			status1 = serial_in(up, UART_IIR) & (UART_IIR_64BYTE_FIFO |
							     UART_IIR_FIFO_ENABLED);
			serial_out(up, UART_FCR, 0);
			serial_out(up, UART_LCR, 0);

			if (status1 == (UART_IIR_64BYTE_FIFO | UART_IIR_FIFO_ENABLED))
				up->port.type = PORT_16550A_FSL64;
			else
				DEBUG_AUTOCONF("Motorola 8xxx DUART ");
		}
		serial_out(up, UART_EFR, 0);
		return;
	}

	 
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	if (serial_in(up, UART_EFR) == 0 && !broken_efr(up)) {
		DEBUG_AUTOCONF("EFRv2 ");
		autoconfig_has_efr(up);
		return;
	}

	 
	serial_out(up, UART_LCR, 0);
	status1 = serial8250_in_MCR(up);
	serial_out(up, UART_LCR, 0xE0);
	status2 = serial_in(up, 0x02);  

	if (!((status2 ^ status1) & UART_MCR_LOOP)) {
		serial_out(up, UART_LCR, 0);
		serial8250_out_MCR(up, status1 ^ UART_MCR_LOOP);
		serial_out(up, UART_LCR, 0xE0);
		status2 = serial_in(up, 0x02);  
		serial_out(up, UART_LCR, 0);
		serial8250_out_MCR(up, status1);

		if ((status2 ^ status1) & UART_MCR_LOOP) {
			unsigned short quot;

			serial_out(up, UART_LCR, 0xE0);

			quot = serial_dl_read(up);
			quot <<= 3;

			if (ns16550a_goto_highspeed(up))
				serial_dl_write(up, quot);

			serial_out(up, UART_LCR, 0);

			up->port.uartclk = 921600*16;
			up->port.type = PORT_NS16550A;
			up->capabilities |= UART_NATSEMI;
			return;
		}
	}

	 
	serial_out(up, UART_LCR, 0);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);
	status1 = serial_in(up, UART_IIR) & (UART_IIR_64BYTE_FIFO | UART_IIR_FIFO_ENABLED);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);

	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_A);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);
	status2 = serial_in(up, UART_IIR) & (UART_IIR_64BYTE_FIFO | UART_IIR_FIFO_ENABLED);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);

	serial_out(up, UART_LCR, 0);

	DEBUG_AUTOCONF("iir1=%d iir2=%d ", status1, status2);

	if (status1 == UART_IIR_FIFO_ENABLED_16550A &&
	    status2 == (UART_IIR_64BYTE_FIFO | UART_IIR_FIFO_ENABLED_16550A)) {
		up->port.type = PORT_16750;
		up->capabilities |= UART_CAP_AFE | UART_CAP_SLEEP;
		return;
	}

	 
	iersave = serial_in(up, UART_IER);
	serial_out(up, UART_IER, iersave & ~UART_IER_UUE);
	if (!(serial_in(up, UART_IER) & UART_IER_UUE)) {
		 
		serial_out(up, UART_IER, iersave | UART_IER_UUE);
		if (serial_in(up, UART_IER) & UART_IER_UUE) {
			 
			DEBUG_AUTOCONF("Xscale ");
			up->port.type = PORT_XSCALE;
			up->capabilities |= UART_CAP_UUE | UART_CAP_RTOIE;
			return;
		}
	} else {
		 
		DEBUG_AUTOCONF("Couldn't force IER_UUE to 0 ");
	}
	serial_out(up, UART_IER, iersave);

	 
	if (up->port.type == PORT_16550A && size_fifo(up) == 64) {
		up->port.type = PORT_U6_16550A;
		up->capabilities |= UART_CAP_AFE;
	}
}

 
static void autoconfig(struct uart_8250_port *up)
{
	unsigned char status1, scratch, scratch2, scratch3;
	unsigned char save_lcr, save_mcr;
	struct uart_port *port = &up->port;
	unsigned long flags;
	unsigned int old_capabilities;

	if (!port->iobase && !port->mapbase && !port->membase)
		return;

	DEBUG_AUTOCONF("%s: autoconf (0x%04lx, 0x%p): ",
		       port->name, port->iobase, port->membase);

	 
	spin_lock_irqsave(&port->lock, flags);

	up->capabilities = 0;
	up->bugs = 0;

	if (!(port->flags & UPF_BUGGY_UART)) {
		 
		scratch = serial_in(up, UART_IER);
		serial_out(up, UART_IER, 0);
#ifdef __i386__
		outb(0xff, 0x080);
#endif
		 
		scratch2 = serial_in(up, UART_IER) & UART_IER_ALL_INTR;
		serial_out(up, UART_IER, UART_IER_ALL_INTR);
#ifdef __i386__
		outb(0, 0x080);
#endif
		scratch3 = serial_in(up, UART_IER) & UART_IER_ALL_INTR;
		serial_out(up, UART_IER, scratch);
		if (scratch2 != 0 || scratch3 != UART_IER_ALL_INTR) {
			 
			spin_unlock_irqrestore(&port->lock, flags);
			DEBUG_AUTOCONF("IER test failed (%02x, %02x) ",
				       scratch2, scratch3);
			goto out;
		}
	}

	save_mcr = serial8250_in_MCR(up);
	save_lcr = serial_in(up, UART_LCR);

	 
	if (!(port->flags & UPF_SKIP_TEST)) {
		serial8250_out_MCR(up, UART_MCR_LOOP | UART_MCR_OUT2 | UART_MCR_RTS);
		status1 = serial_in(up, UART_MSR) & UART_MSR_STATUS_BITS;
		serial8250_out_MCR(up, save_mcr);
		if (status1 != (UART_MSR_DCD | UART_MSR_CTS)) {
			spin_unlock_irqrestore(&port->lock, flags);
			DEBUG_AUTOCONF("LOOP test failed (%02x) ",
				       status1);
			goto out;
		}
	}

	 
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_out(up, UART_EFR, 0);
	serial_out(up, UART_LCR, 0);

	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);

	switch (serial_in(up, UART_IIR) & UART_IIR_FIFO_ENABLED) {
	case UART_IIR_FIFO_ENABLED_8250:
		autoconfig_8250(up);
		break;
	case UART_IIR_FIFO_ENABLED_16550:
		port->type = PORT_16550;
		break;
	case UART_IIR_FIFO_ENABLED_16550A:
		autoconfig_16550a(up);
		break;
	default:
		port->type = PORT_UNKNOWN;
		break;
	}

#ifdef CONFIG_SERIAL_8250_RSA
	 
	if (port->type == PORT_16550A && up->probe & UART_PROBE_RSA &&
	    __enable_rsa(up))
		port->type = PORT_RSA;
#endif

	serial_out(up, UART_LCR, save_lcr);

	port->fifosize = uart_config[up->port.type].fifo_size;
	old_capabilities = up->capabilities;
	up->capabilities = uart_config[port->type].flags;
	up->tx_loadsz = uart_config[port->type].tx_loadsz;

	if (port->type == PORT_UNKNOWN)
		goto out_unlock;

	 
#ifdef CONFIG_SERIAL_8250_RSA
	if (port->type == PORT_RSA)
		serial_out(up, UART_RSA_FRR, 0);
#endif
	serial8250_out_MCR(up, save_mcr);
	serial8250_clear_fifos(up);
	serial_in(up, UART_RX);
	serial8250_clear_IER(up);

out_unlock:
	spin_unlock_irqrestore(&port->lock, flags);

	 
	if (port->type == PORT_16550A && port->iotype == UPIO_PORT)
		fintek_8250_probe(up);

	if (up->capabilities != old_capabilities) {
		dev_warn(port->dev, "detected caps %08x should be %08x\n",
			 old_capabilities, up->capabilities);
	}
out:
	DEBUG_AUTOCONF("iir=%d ", scratch);
	DEBUG_AUTOCONF("type=%s\n", uart_config[port->type].name);
}

static void autoconfig_irq(struct uart_8250_port *up)
{
	struct uart_port *port = &up->port;
	unsigned char save_mcr, save_ier;
	unsigned char save_ICP = 0;
	unsigned int ICP = 0;
	unsigned long irqs;
	int irq;

	if (port->flags & UPF_FOURPORT) {
		ICP = (port->iobase & 0xfe0) | 0x1f;
		save_ICP = inb_p(ICP);
		outb_p(0x80, ICP);
		inb_p(ICP);
	}

	if (uart_console(port))
		console_lock();

	 
	probe_irq_off(probe_irq_on());
	save_mcr = serial8250_in_MCR(up);
	 
	spin_lock_irq(&port->lock);
	save_ier = serial_in(up, UART_IER);
	spin_unlock_irq(&port->lock);
	serial8250_out_MCR(up, UART_MCR_OUT1 | UART_MCR_OUT2);

	irqs = probe_irq_on();
	serial8250_out_MCR(up, 0);
	udelay(10);
	if (port->flags & UPF_FOURPORT) {
		serial8250_out_MCR(up, UART_MCR_DTR | UART_MCR_RTS);
	} else {
		serial8250_out_MCR(up,
			UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);
	}
	 
	spin_lock_irq(&port->lock);
	serial_out(up, UART_IER, UART_IER_ALL_INTR);
	spin_unlock_irq(&port->lock);
	serial_in(up, UART_LSR);
	serial_in(up, UART_RX);
	serial_in(up, UART_IIR);
	serial_in(up, UART_MSR);
	serial_out(up, UART_TX, 0xFF);
	udelay(20);
	irq = probe_irq_off(irqs);

	serial8250_out_MCR(up, save_mcr);
	 
	spin_lock_irq(&port->lock);
	serial_out(up, UART_IER, save_ier);
	spin_unlock_irq(&port->lock);

	if (port->flags & UPF_FOURPORT)
		outb_p(save_ICP, ICP);

	if (uart_console(port))
		console_unlock();

	port->irq = (irq > 0) ? irq : 0;
}

static void serial8250_stop_rx(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	 
	lockdep_assert_held_once(&port->lock);

	serial8250_rpm_get(up);

	up->ier &= ~(UART_IER_RLSI | UART_IER_RDI);
	up->port.read_status_mask &= ~UART_LSR_DR;
	serial_port_out(port, UART_IER, up->ier);

	serial8250_rpm_put(up);
}

 
void serial8250_em485_stop_tx(struct uart_8250_port *p)
{
	unsigned char mcr = serial8250_in_MCR(p);

	 
	lockdep_assert_held_once(&p->port.lock);

	if (p->port.rs485.flags & SER_RS485_RTS_AFTER_SEND)
		mcr |= UART_MCR_RTS;
	else
		mcr &= ~UART_MCR_RTS;
	serial8250_out_MCR(p, mcr);

	 
	if (!(p->port.rs485.flags & SER_RS485_RX_DURING_TX)) {
		serial8250_clear_and_reinit_fifos(p);

		p->ier |= UART_IER_RLSI | UART_IER_RDI;
		serial_port_out(&p->port, UART_IER, p->ier);
	}
}
EXPORT_SYMBOL_GPL(serial8250_em485_stop_tx);

static enum hrtimer_restart serial8250_em485_handle_stop_tx(struct hrtimer *t)
{
	struct uart_8250_em485 *em485 = container_of(t, struct uart_8250_em485,
			stop_tx_timer);
	struct uart_8250_port *p = em485->port;
	unsigned long flags;

	serial8250_rpm_get(p);
	spin_lock_irqsave(&p->port.lock, flags);
	if (em485->active_timer == &em485->stop_tx_timer) {
		p->rs485_stop_tx(p);
		em485->active_timer = NULL;
		em485->tx_stopped = true;
	}
	spin_unlock_irqrestore(&p->port.lock, flags);
	serial8250_rpm_put(p);

	return HRTIMER_NORESTART;
}

static void start_hrtimer_ms(struct hrtimer *hrt, unsigned long msec)
{
	hrtimer_start(hrt, ms_to_ktime(msec), HRTIMER_MODE_REL);
}

static void __stop_tx_rs485(struct uart_8250_port *p, u64 stop_delay)
{
	struct uart_8250_em485 *em485 = p->em485;

	 
	lockdep_assert_held_once(&p->port.lock);

	stop_delay += (u64)p->port.rs485.delay_rts_after_send * NSEC_PER_MSEC;

	 
	if (stop_delay > 0) {
		em485->active_timer = &em485->stop_tx_timer;
		hrtimer_start(&em485->stop_tx_timer, ns_to_ktime(stop_delay), HRTIMER_MODE_REL);
	} else {
		p->rs485_stop_tx(p);
		em485->active_timer = NULL;
		em485->tx_stopped = true;
	}
}

static inline void __stop_tx(struct uart_8250_port *p)
{
	struct uart_8250_em485 *em485 = p->em485;

	if (em485) {
		u16 lsr = serial_lsr_in(p);
		u64 stop_delay = 0;

		if (!(lsr & UART_LSR_THRE))
			return;
		 
		if (!(lsr & UART_LSR_TEMT)) {
			if (!(p->capabilities & UART_CAP_NOTEMT))
				return;
			 
			stop_delay = p->port.frame_time + DIV_ROUND_UP(p->port.frame_time, 7);
		}

		__stop_tx_rs485(p, stop_delay);
	}

	if (serial8250_clear_THRI(p))
		serial8250_rpm_put_tx(p);
}

static void serial8250_stop_tx(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	serial8250_rpm_get(up);
	__stop_tx(up);

	 
	if (port->type == PORT_16C950) {
		up->acr |= UART_ACR_TXDIS;
		serial_icr_write(up, UART_ACR, up->acr);
	}
	serial8250_rpm_put(up);
}

static inline void __start_tx(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	if (up->dma && !up->dma->tx_dma(up))
		return;

	if (serial8250_set_THRI(up)) {
		if (up->bugs & UART_BUG_TXEN) {
			u16 lsr = serial_lsr_in(up);

			if (lsr & UART_LSR_THRE)
				serial8250_tx_chars(up);
		}
	}

	 
	if (port->type == PORT_16C950 && up->acr & UART_ACR_TXDIS) {
		up->acr &= ~UART_ACR_TXDIS;
		serial_icr_write(up, UART_ACR, up->acr);
	}
}

 
void serial8250_em485_start_tx(struct uart_8250_port *up)
{
	unsigned char mcr = serial8250_in_MCR(up);

	if (!(up->port.rs485.flags & SER_RS485_RX_DURING_TX))
		serial8250_stop_rx(&up->port);

	if (up->port.rs485.flags & SER_RS485_RTS_ON_SEND)
		mcr |= UART_MCR_RTS;
	else
		mcr &= ~UART_MCR_RTS;
	serial8250_out_MCR(up, mcr);
}
EXPORT_SYMBOL_GPL(serial8250_em485_start_tx);

 
static bool start_tx_rs485(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct uart_8250_em485 *em485 = up->em485;

	 
	if (em485->active_timer == &em485->stop_tx_timer)
		hrtimer_try_to_cancel(&em485->stop_tx_timer);

	em485->active_timer = NULL;

	if (em485->tx_stopped) {
		em485->tx_stopped = false;

		up->rs485_start_tx(up);

		if (up->port.rs485.delay_rts_before_send > 0) {
			em485->active_timer = &em485->start_tx_timer;
			start_hrtimer_ms(&em485->start_tx_timer,
					 up->port.rs485.delay_rts_before_send);
			return false;
		}
	}

	return true;
}

static enum hrtimer_restart serial8250_em485_handle_start_tx(struct hrtimer *t)
{
	struct uart_8250_em485 *em485 = container_of(t, struct uart_8250_em485,
			start_tx_timer);
	struct uart_8250_port *p = em485->port;
	unsigned long flags;

	spin_lock_irqsave(&p->port.lock, flags);
	if (em485->active_timer == &em485->start_tx_timer) {
		__start_tx(&p->port);
		em485->active_timer = NULL;
	}
	spin_unlock_irqrestore(&p->port.lock, flags);

	return HRTIMER_NORESTART;
}

static void serial8250_start_tx(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct uart_8250_em485 *em485 = up->em485;

	 
	lockdep_assert_held_once(&port->lock);

	if (!port->x_char && uart_circ_empty(&port->state->xmit))
		return;

	serial8250_rpm_get_tx(up);

	if (em485) {
		if ((em485->active_timer == &em485->start_tx_timer) ||
		    !start_tx_rs485(port))
			return;
	}
	__start_tx(port);
}

static void serial8250_throttle(struct uart_port *port)
{
	port->throttle(port);
}

static void serial8250_unthrottle(struct uart_port *port)
{
	port->unthrottle(port);
}

static void serial8250_disable_ms(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	 
	lockdep_assert_held_once(&port->lock);

	 
	if (up->bugs & UART_BUG_NOMSR)
		return;

	mctrl_gpio_disable_ms(up->gpios);

	up->ier &= ~UART_IER_MSI;
	serial_port_out(port, UART_IER, up->ier);
}

static void serial8250_enable_ms(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	 
	lockdep_assert_held_once(&port->lock);

	 
	if (up->bugs & UART_BUG_NOMSR)
		return;

	mctrl_gpio_enable_ms(up->gpios);

	up->ier |= UART_IER_MSI;

	serial8250_rpm_get(up);
	serial_port_out(port, UART_IER, up->ier);
	serial8250_rpm_put(up);
}

void serial8250_read_char(struct uart_8250_port *up, u16 lsr)
{
	struct uart_port *port = &up->port;
	u8 ch, flag = TTY_NORMAL;

	if (likely(lsr & UART_LSR_DR))
		ch = serial_in(up, UART_RX);
	else
		 
		ch = 0;

	port->icount.rx++;

	lsr |= up->lsr_saved_flags;
	up->lsr_saved_flags = 0;

	if (unlikely(lsr & UART_LSR_BRK_ERROR_BITS)) {
		if (lsr & UART_LSR_BI) {
			lsr &= ~(UART_LSR_FE | UART_LSR_PE);
			port->icount.brk++;
			 
			if (uart_handle_break(port))
				return;
		} else if (lsr & UART_LSR_PE)
			port->icount.parity++;
		else if (lsr & UART_LSR_FE)
			port->icount.frame++;
		if (lsr & UART_LSR_OE)
			port->icount.overrun++;

		 
		lsr &= port->read_status_mask;

		if (lsr & UART_LSR_BI) {
			dev_dbg(port->dev, "handling break\n");
			flag = TTY_BREAK;
		} else if (lsr & UART_LSR_PE)
			flag = TTY_PARITY;
		else if (lsr & UART_LSR_FE)
			flag = TTY_FRAME;
	}
	if (uart_prepare_sysrq_char(port, ch))
		return;

	uart_insert_char(port, lsr, UART_LSR_OE, ch, flag);
}
EXPORT_SYMBOL_GPL(serial8250_read_char);

 
u16 serial8250_rx_chars(struct uart_8250_port *up, u16 lsr)
{
	struct uart_port *port = &up->port;
	int max_count = 256;

	do {
		serial8250_read_char(up, lsr);
		if (--max_count == 0)
			break;
		lsr = serial_in(up, UART_LSR);
	} while (lsr & (UART_LSR_DR | UART_LSR_BI));

	tty_flip_buffer_push(&port->state->port);
	return lsr;
}
EXPORT_SYMBOL_GPL(serial8250_rx_chars);

void serial8250_tx_chars(struct uart_8250_port *up)
{
	struct uart_port *port = &up->port;
	struct circ_buf *xmit = &port->state->xmit;
	int count;

	if (port->x_char) {
		uart_xchar_out(port, UART_TX);
		return;
	}
	if (uart_tx_stopped(port)) {
		serial8250_stop_tx(port);
		return;
	}
	if (uart_circ_empty(xmit)) {
		__stop_tx(up);
		return;
	}

	count = up->tx_loadsz;
	do {
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		if (up->bugs & UART_BUG_TXRACE) {
			 
			serial_in(up, UART_SCR);
		}
		uart_xmit_advance(port, 1);
		if (uart_circ_empty(xmit))
			break;
		if ((up->capabilities & UART_CAP_HFIFO) &&
		    !uart_lsr_tx_empty(serial_in(up, UART_LSR)))
			break;
		 
		if ((up->capabilities & UART_CAP_MINI) &&
		    !(serial_in(up, UART_LSR) & UART_LSR_THRE))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	 
	if (uart_circ_empty(xmit) && !(up->capabilities & UART_CAP_RPM))
		__stop_tx(up);
}
EXPORT_SYMBOL_GPL(serial8250_tx_chars);

 
unsigned int serial8250_modem_status(struct uart_8250_port *up)
{
	struct uart_port *port = &up->port;
	unsigned int status = serial_in(up, UART_MSR);

	status |= up->msr_saved_flags;
	up->msr_saved_flags = 0;
	if (status & UART_MSR_ANY_DELTA && up->ier & UART_IER_MSI &&
	    port->state != NULL) {
		if (status & UART_MSR_TERI)
			port->icount.rng++;
		if (status & UART_MSR_DDSR)
			port->icount.dsr++;
		if (status & UART_MSR_DDCD)
			uart_handle_dcd_change(port, status & UART_MSR_DCD);
		if (status & UART_MSR_DCTS)
			uart_handle_cts_change(port, status & UART_MSR_CTS);

		wake_up_interruptible(&port->state->port.delta_msr_wait);
	}

	return status;
}
EXPORT_SYMBOL_GPL(serial8250_modem_status);

static bool handle_rx_dma(struct uart_8250_port *up, unsigned int iir)
{
	switch (iir & 0x3f) {
	case UART_IIR_THRI:
		 
		return false;
	case UART_IIR_RDI:
		if (!up->dma->rx_running)
			break;
		fallthrough;
	case UART_IIR_RLSI:
	case UART_IIR_RX_TIMEOUT:
		serial8250_rx_dma_flush(up);
		return true;
	}
	return up->dma->rx_dma(up);
}

 
int serial8250_handle_irq(struct uart_port *port, unsigned int iir)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct tty_port *tport = &port->state->port;
	bool skip_rx = false;
	unsigned long flags;
	u16 status;

	if (iir & UART_IIR_NO_INT)
		return 0;

	spin_lock_irqsave(&port->lock, flags);

	status = serial_lsr_in(up);

	 
	if (!(status & (UART_LSR_FIFOE | UART_LSR_BRK_ERROR_BITS)) &&
	    (port->status & (UPSTAT_AUTOCTS | UPSTAT_AUTORTS)) &&
	    !(port->read_status_mask & UART_LSR_DR))
		skip_rx = true;

	if (status & (UART_LSR_DR | UART_LSR_BI) && !skip_rx) {
		struct irq_data *d;

		d = irq_get_irq_data(port->irq);
		if (d && irqd_is_wakeup_set(d))
			pm_wakeup_event(tport->tty->dev, 0);
		if (!up->dma || handle_rx_dma(up, iir))
			status = serial8250_rx_chars(up, status);
	}
	serial8250_modem_status(up);
	if ((status & UART_LSR_THRE) && (up->ier & UART_IER_THRI)) {
		if (!up->dma || up->dma->tx_err)
			serial8250_tx_chars(up);
		else if (!up->dma->tx_running)
			__stop_tx(up);
	}

	uart_unlock_and_check_sysrq_irqrestore(port, flags);

	return 1;
}
EXPORT_SYMBOL_GPL(serial8250_handle_irq);

static int serial8250_default_handle_irq(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned int iir;
	int ret;

	serial8250_rpm_get(up);

	iir = serial_port_in(port, UART_IIR);
	ret = serial8250_handle_irq(port, iir);

	serial8250_rpm_put(up);
	return ret;
}

 
static int serial8250_tx_threshold_handle_irq(struct uart_port *port)
{
	unsigned long flags;
	unsigned int iir = serial_port_in(port, UART_IIR);

	 
	if ((iir & UART_IIR_ID) == UART_IIR_THRI) {
		struct uart_8250_port *up = up_to_u8250p(port);

		spin_lock_irqsave(&port->lock, flags);
		serial8250_tx_chars(up);
		spin_unlock_irqrestore(&port->lock, flags);
	}

	iir = serial_port_in(port, UART_IIR);
	return serial8250_handle_irq(port, iir);
}

static unsigned int serial8250_tx_empty(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned int result = 0;
	unsigned long flags;

	serial8250_rpm_get(up);

	spin_lock_irqsave(&port->lock, flags);
	if (!serial8250_tx_dma_running(up) && uart_lsr_tx_empty(serial_lsr_in(up)))
		result = TIOCSER_TEMT;
	spin_unlock_irqrestore(&port->lock, flags);

	serial8250_rpm_put(up);

	return result;
}

unsigned int serial8250_do_get_mctrl(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned int status;
	unsigned int val;

	serial8250_rpm_get(up);
	status = serial8250_modem_status(up);
	serial8250_rpm_put(up);

	val = serial8250_MSR_to_TIOCM(status);
	if (up->gpios)
		return mctrl_gpio_get(up->gpios, &val);

	return val;
}
EXPORT_SYMBOL_GPL(serial8250_do_get_mctrl);

static unsigned int serial8250_get_mctrl(struct uart_port *port)
{
	if (port->get_mctrl)
		return port->get_mctrl(port);
	return serial8250_do_get_mctrl(port);
}

void serial8250_do_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned char mcr;

	mcr = serial8250_TIOCM_to_MCR(mctrl);

	mcr |= up->mcr;

	serial8250_out_MCR(up, mcr);
}
EXPORT_SYMBOL_GPL(serial8250_do_set_mctrl);

static void serial8250_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	if (port->rs485.flags & SER_RS485_ENABLED)
		return;

	if (port->set_mctrl)
		port->set_mctrl(port, mctrl);
	else
		serial8250_do_set_mctrl(port, mctrl);
}

static void serial8250_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned long flags;

	serial8250_rpm_get(up);
	spin_lock_irqsave(&port->lock, flags);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_port_out(port, UART_LCR, up->lcr);
	spin_unlock_irqrestore(&port->lock, flags);
	serial8250_rpm_put(up);
}

static void wait_for_lsr(struct uart_8250_port *up, int bits)
{
	unsigned int status, tmout = 10000;

	 
	for (;;) {
		status = serial_lsr_in(up);

		if ((status & bits) == bits)
			break;
		if (--tmout == 0)
			break;
		udelay(1);
		touch_nmi_watchdog();
	}
}

 
static void wait_for_xmitr(struct uart_8250_port *up, int bits)
{
	unsigned int tmout;

	wait_for_lsr(up, bits);

	 
	if (up->port.flags & UPF_CONS_FLOW) {
		for (tmout = 1000000; tmout; tmout--) {
			unsigned int msr = serial_in(up, UART_MSR);
			up->msr_saved_flags |= msr & MSR_SAVE_FLAGS;
			if (msr & UART_MSR_CTS)
				break;
			udelay(1);
			touch_nmi_watchdog();
		}
	}
}

#ifdef CONFIG_CONSOLE_POLL
 

static int serial8250_get_poll_char(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	int status;
	u16 lsr;

	serial8250_rpm_get(up);

	lsr = serial_port_in(port, UART_LSR);

	if (!(lsr & UART_LSR_DR)) {
		status = NO_POLL_CHAR;
		goto out;
	}

	status = serial_port_in(port, UART_RX);
out:
	serial8250_rpm_put(up);
	return status;
}


static void serial8250_put_poll_char(struct uart_port *port,
			 unsigned char c)
{
	unsigned int ier;
	struct uart_8250_port *up = up_to_u8250p(port);

	 

	serial8250_rpm_get(up);
	 
	ier = serial_port_in(port, UART_IER);
	serial8250_clear_IER(up);

	wait_for_xmitr(up, UART_LSR_BOTH_EMPTY);
	 
	serial_port_out(port, UART_TX, c);

	 
	wait_for_xmitr(up, UART_LSR_BOTH_EMPTY);
	serial_port_out(port, UART_IER, ier);
	serial8250_rpm_put(up);
}

#endif  

int serial8250_do_startup(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned long flags;
	unsigned char iir;
	int retval;
	u16 lsr;

	if (!port->fifosize)
		port->fifosize = uart_config[port->type].fifo_size;
	if (!up->tx_loadsz)
		up->tx_loadsz = uart_config[port->type].tx_loadsz;
	if (!up->capabilities)
		up->capabilities = uart_config[port->type].flags;
	up->mcr = 0;

	if (port->iotype != up->cur_iotype)
		set_io_from_upio(port);

	serial8250_rpm_get(up);
	if (port->type == PORT_16C950) {
		 
		spin_lock_irqsave(&port->lock, flags);
		up->acr = 0;
		serial_port_out(port, UART_LCR, UART_LCR_CONF_MODE_B);
		serial_port_out(port, UART_EFR, UART_EFR_ECB);
		serial_port_out(port, UART_IER, 0);
		serial_port_out(port, UART_LCR, 0);
		serial_icr_write(up, UART_CSR, 0);  
		serial_port_out(port, UART_LCR, UART_LCR_CONF_MODE_B);
		serial_port_out(port, UART_EFR, UART_EFR_ECB);
		serial_port_out(port, UART_LCR, 0);
		spin_unlock_irqrestore(&port->lock, flags);
	}

	if (port->type == PORT_DA830) {
		 
		spin_lock_irqsave(&port->lock, flags);
		serial_port_out(port, UART_IER, 0);
		serial_port_out(port, UART_DA830_PWREMU_MGMT, 0);
		spin_unlock_irqrestore(&port->lock, flags);
		mdelay(10);

		 
		serial_port_out(port, UART_DA830_PWREMU_MGMT,
				UART_DA830_PWREMU_MGMT_UTRST |
				UART_DA830_PWREMU_MGMT_URRST |
				UART_DA830_PWREMU_MGMT_FREE);
	}

	if (port->type == PORT_NPCM) {
		 
		serial_port_out(port, UART_NPCM_TOR, UART_NPCM_TOIE | 0x20);
	}

#ifdef CONFIG_SERIAL_8250_RSA
	 
	enable_rsa(up);
#endif

	 
	serial8250_clear_fifos(up);

	 
	serial_port_in(port, UART_LSR);
	serial_port_in(port, UART_RX);
	serial_port_in(port, UART_IIR);
	serial_port_in(port, UART_MSR);

	 
	if (!(port->flags & UPF_BUGGY_UART) &&
	    (serial_port_in(port, UART_LSR) == 0xff)) {
		dev_info_ratelimited(port->dev, "LSR safety check engaged!\n");
		retval = -ENODEV;
		goto out;
	}

	 
	if (port->type == PORT_16850) {
		unsigned char fctr;

		serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);

		fctr = serial_in(up, UART_FCTR) & ~(UART_FCTR_RX|UART_FCTR_TX);
		serial_port_out(port, UART_FCTR,
				fctr | UART_FCTR_TRGD | UART_FCTR_RX);
		serial_port_out(port, UART_TRG, UART_TRG_96);
		serial_port_out(port, UART_FCTR,
				fctr | UART_FCTR_TRGD | UART_FCTR_TX);
		serial_port_out(port, UART_TRG, UART_TRG_96);

		serial_port_out(port, UART_LCR, 0);
	}

	 
	if (((port->type == PORT_ALTR_16550_F32) ||
	     (port->type == PORT_ALTR_16550_F64) ||
	     (port->type == PORT_ALTR_16550_F128)) && (port->fifosize > 1)) {
		 
		if ((up->tx_loadsz < 2) || (up->tx_loadsz > port->fifosize)) {
			dev_err(port->dev, "TX FIFO Threshold errors, skipping\n");
		} else {
			serial_port_out(port, UART_ALTR_AFR,
					UART_ALTR_EN_TXFIFO_LW);
			serial_port_out(port, UART_ALTR_TX_LOW,
					port->fifosize - up->tx_loadsz);
			port->handle_irq = serial8250_tx_threshold_handle_irq;
		}
	}

	 
	if (port->irq && (up->port.flags & UPF_SHARE_IRQ))
		up->port.irqflags |= IRQF_SHARED;

	retval = up->ops->setup_irq(up);
	if (retval)
		goto out;

	if (port->irq && !(up->port.flags & UPF_NO_THRE_TEST)) {
		unsigned char iir1;

		if (port->irqflags & IRQF_SHARED)
			disable_irq_nosync(port->irq);

		 
		spin_lock_irqsave(&port->lock, flags);

		wait_for_xmitr(up, UART_LSR_THRE);
		serial_port_out_sync(port, UART_IER, UART_IER_THRI);
		udelay(1);  
		iir1 = serial_port_in(port, UART_IIR);
		serial_port_out(port, UART_IER, 0);
		serial_port_out_sync(port, UART_IER, UART_IER_THRI);
		udelay(1);  
		iir = serial_port_in(port, UART_IIR);
		serial_port_out(port, UART_IER, 0);

		spin_unlock_irqrestore(&port->lock, flags);

		if (port->irqflags & IRQF_SHARED)
			enable_irq(port->irq);

		 
		if ((!(iir1 & UART_IIR_NO_INT) && (iir & UART_IIR_NO_INT)) ||
		    up->port.flags & UPF_BUG_THRE) {
			up->bugs |= UART_BUG_THRE;
		}
	}

	up->ops->setup_timer(up);

	 
	serial_port_out(port, UART_LCR, UART_LCR_WLEN8);

	spin_lock_irqsave(&port->lock, flags);
	if (up->port.flags & UPF_FOURPORT) {
		if (!up->port.irq)
			up->port.mctrl |= TIOCM_OUT1;
	} else
		 
		if (port->irq)
			up->port.mctrl |= TIOCM_OUT2;

	serial8250_set_mctrl(port, port->mctrl);

	 
	if (up->port.quirks & UPQ_NO_TXEN_TEST)
		goto dont_test_tx_en;

	 
	serial_port_out(port, UART_IER, UART_IER_THRI);
	lsr = serial_port_in(port, UART_LSR);
	iir = serial_port_in(port, UART_IIR);
	serial_port_out(port, UART_IER, 0);

	if (lsr & UART_LSR_TEMT && iir & UART_IIR_NO_INT) {
		if (!(up->bugs & UART_BUG_TXEN)) {
			up->bugs |= UART_BUG_TXEN;
			dev_dbg(port->dev, "enabling bad tx status workarounds\n");
		}
	} else {
		up->bugs &= ~UART_BUG_TXEN;
	}

dont_test_tx_en:
	spin_unlock_irqrestore(&port->lock, flags);

	 
	serial_port_in(port, UART_LSR);
	serial_port_in(port, UART_RX);
	serial_port_in(port, UART_IIR);
	serial_port_in(port, UART_MSR);
	up->lsr_saved_flags = 0;
	up->msr_saved_flags = 0;

	 
	if (up->dma) {
		const char *msg = NULL;

		if (uart_console(port))
			msg = "forbid DMA for kernel console";
		else if (serial8250_request_dma(up))
			msg = "failed to request DMA";
		if (msg) {
			dev_warn_ratelimited(port->dev, "%s\n", msg);
			up->dma = NULL;
		}
	}

	 
	up->ier = UART_IER_RLSI | UART_IER_RDI;

	if (port->flags & UPF_FOURPORT) {
		unsigned int icp;
		 
		icp = (port->iobase & 0xfe0) | 0x01f;
		outb_p(0x80, icp);
		inb_p(icp);
	}
	retval = 0;
out:
	serial8250_rpm_put(up);
	return retval;
}
EXPORT_SYMBOL_GPL(serial8250_do_startup);

static int serial8250_startup(struct uart_port *port)
{
	if (port->startup)
		return port->startup(port);
	return serial8250_do_startup(port);
}

void serial8250_do_shutdown(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned long flags;

	serial8250_rpm_get(up);
	 
	spin_lock_irqsave(&port->lock, flags);
	up->ier = 0;
	serial_port_out(port, UART_IER, 0);
	spin_unlock_irqrestore(&port->lock, flags);

	synchronize_irq(port->irq);

	if (up->dma)
		serial8250_release_dma(up);

	spin_lock_irqsave(&port->lock, flags);
	if (port->flags & UPF_FOURPORT) {
		 
		inb((port->iobase & 0xfe0) | 0x1f);
		port->mctrl |= TIOCM_OUT1;
	} else
		port->mctrl &= ~TIOCM_OUT2;

	serial8250_set_mctrl(port, port->mctrl);
	spin_unlock_irqrestore(&port->lock, flags);

	 
	serial_port_out(port, UART_LCR,
			serial_port_in(port, UART_LCR) & ~UART_LCR_SBC);
	serial8250_clear_fifos(up);

#ifdef CONFIG_SERIAL_8250_RSA
	 
	disable_rsa(up);
#endif

	 
	serial_port_in(port, UART_RX);
	serial8250_rpm_put(up);

	up->ops->release_irq(up);
}
EXPORT_SYMBOL_GPL(serial8250_do_shutdown);

static void serial8250_shutdown(struct uart_port *port)
{
	if (port->shutdown)
		port->shutdown(port);
	else
		serial8250_do_shutdown(port);
}

 
static unsigned int npcm_get_divisor(struct uart_8250_port *up,
		unsigned int baud)
{
	struct uart_port *port = &up->port;

	return DIV_ROUND_CLOSEST(port->uartclk, 16 * baud + 2) - 2;
}

static unsigned int serial8250_do_get_divisor(struct uart_port *port,
					      unsigned int baud,
					      unsigned int *frac)
{
	upf_t magic_multiplier = port->flags & UPF_MAGIC_MULTIPLIER;
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned int quot;

	 
	if (magic_multiplier && baud >= port->uartclk / 6)
		quot = 0x8001;
	else if (magic_multiplier && baud >= port->uartclk / 12)
		quot = 0x8002;
	else if (up->port.type == PORT_NPCM)
		quot = npcm_get_divisor(up, baud);
	else
		quot = uart_get_divisor(port, baud);

	 
	if (up->bugs & UART_BUG_QUOT && (quot & 0xff) == 0)
		quot++;

	return quot;
}

static unsigned int serial8250_get_divisor(struct uart_port *port,
					   unsigned int baud,
					   unsigned int *frac)
{
	if (port->get_divisor)
		return port->get_divisor(port, baud, frac);

	return serial8250_do_get_divisor(port, baud, frac);
}

static unsigned char serial8250_compute_lcr(struct uart_8250_port *up,
					    tcflag_t c_cflag)
{
	unsigned char cval;

	cval = UART_LCR_WLEN(tty_get_char_size(c_cflag));

	if (c_cflag & CSTOPB)
		cval |= UART_LCR_STOP;
	if (c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(c_cflag & PARODD))
		cval |= UART_LCR_EPAR;
	if (c_cflag & CMSPAR)
		cval |= UART_LCR_SPAR;

	return cval;
}

void serial8250_do_set_divisor(struct uart_port *port, unsigned int baud,
			       unsigned int quot, unsigned int quot_frac)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	 
	if (is_omap1510_8250(up)) {
		if (baud == 115200) {
			quot = 1;
			serial_port_out(port, UART_OMAP_OSC_12M_SEL, 1);
		} else
			serial_port_out(port, UART_OMAP_OSC_12M_SEL, 0);
	}

	 
	if (up->capabilities & UART_NATSEMI)
		serial_port_out(port, UART_LCR, 0xe0);
	else
		serial_port_out(port, UART_LCR, up->lcr | UART_LCR_DLAB);

	serial_dl_write(up, quot);
}
EXPORT_SYMBOL_GPL(serial8250_do_set_divisor);

static void serial8250_set_divisor(struct uart_port *port, unsigned int baud,
				   unsigned int quot, unsigned int quot_frac)
{
	if (port->set_divisor)
		port->set_divisor(port, baud, quot, quot_frac);
	else
		serial8250_do_set_divisor(port, baud, quot, quot_frac);
}

static unsigned int serial8250_get_baud_rate(struct uart_port *port,
					     struct ktermios *termios,
					     const struct ktermios *old)
{
	unsigned int tolerance = port->uartclk / 100;
	unsigned int min;
	unsigned int max;

	 
	if (port->flags & UPF_MAGIC_MULTIPLIER) {
		min = port->uartclk / 16 / UART_DIV_MAX >> 1;
		max = (port->uartclk + tolerance) / 4;
	} else {
		min = port->uartclk / 16 / UART_DIV_MAX;
		max = (port->uartclk + tolerance) / 16;
	}

	 
	return uart_get_baud_rate(port, termios, old, min, max);
}

 
void serial8250_update_uartclk(struct uart_port *port, unsigned int uartclk)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	struct tty_port *tport = &port->state->port;
	unsigned int baud, quot, frac = 0;
	struct ktermios *termios;
	struct tty_struct *tty;
	unsigned long flags;

	tty = tty_port_tty_get(tport);
	if (!tty) {
		mutex_lock(&tport->mutex);
		port->uartclk = uartclk;
		mutex_unlock(&tport->mutex);
		return;
	}

	down_write(&tty->termios_rwsem);
	mutex_lock(&tport->mutex);

	if (port->uartclk == uartclk)
		goto out_unlock;

	port->uartclk = uartclk;

	if (!tty_port_initialized(tport))
		goto out_unlock;

	termios = &tty->termios;

	baud = serial8250_get_baud_rate(port, termios, NULL);
	quot = serial8250_get_divisor(port, baud, &frac);

	serial8250_rpm_get(up);
	spin_lock_irqsave(&port->lock, flags);

	uart_update_timeout(port, termios->c_cflag, baud);

	serial8250_set_divisor(port, baud, quot, frac);
	serial_port_out(port, UART_LCR, up->lcr);

	spin_unlock_irqrestore(&port->lock, flags);
	serial8250_rpm_put(up);

out_unlock:
	mutex_unlock(&tport->mutex);
	up_write(&tty->termios_rwsem);
	tty_kref_put(tty);
}
EXPORT_SYMBOL_GPL(serial8250_update_uartclk);

void
serial8250_do_set_termios(struct uart_port *port, struct ktermios *termios,
		          const struct ktermios *old)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	unsigned char cval;
	unsigned long flags;
	unsigned int baud, quot, frac = 0;

	if (up->capabilities & UART_CAP_MINI) {
		termios->c_cflag &= ~(CSTOPB | PARENB | PARODD | CMSPAR);
		if ((termios->c_cflag & CSIZE) == CS5 ||
		    (termios->c_cflag & CSIZE) == CS6)
			termios->c_cflag = (termios->c_cflag & ~CSIZE) | CS7;
	}
	cval = serial8250_compute_lcr(up, termios->c_cflag);

	baud = serial8250_get_baud_rate(port, termios, old);
	quot = serial8250_get_divisor(port, baud, &frac);

	 
	serial8250_rpm_get(up);
	spin_lock_irqsave(&port->lock, flags);

	up->lcr = cval;					 

	if (up->capabilities & UART_CAP_FIFO && port->fifosize > 1) {
		if (baud < 2400 && !up->dma) {
			up->fcr &= ~UART_FCR_TRIGGER_MASK;
			up->fcr |= UART_FCR_TRIGGER_1;
		}
	}

	 
	if (up->capabilities & UART_CAP_AFE) {
		up->mcr &= ~UART_MCR_AFE;
		if (termios->c_cflag & CRTSCTS)
			up->mcr |= UART_MCR_AFE;
	}

	 
	uart_update_timeout(port, termios->c_cflag, baud);

	port->read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (IGNBRK | BRKINT | PARMRK))
		port->read_status_mask |= UART_LSR_BI;

	 
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= UART_LSR_BI;
		 
		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= UART_LSR_OE;
	}

	 
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= UART_LSR_DR;

	 
	up->ier &= ~UART_IER_MSI;
	if (!(up->bugs & UART_BUG_NOMSR) &&
			UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;
	if (up->capabilities & UART_CAP_UUE)
		up->ier |= UART_IER_UUE;
	if (up->capabilities & UART_CAP_RTOIE)
		up->ier |= UART_IER_RTOIE;

	serial_port_out(port, UART_IER, up->ier);

	if (up->capabilities & UART_CAP_EFR) {
		unsigned char efr = 0;
		 
		if (termios->c_cflag & CRTSCTS)
			efr |= UART_EFR_CTS;

		serial_port_out(port, UART_LCR, UART_LCR_CONF_MODE_B);
		if (port->flags & UPF_EXAR_EFR)
			serial_port_out(port, UART_XR_EFR, efr);
		else
			serial_port_out(port, UART_EFR, efr);
	}

	serial8250_set_divisor(port, baud, quot, frac);

	 
	if (port->type == PORT_16750)
		serial_port_out(port, UART_FCR, up->fcr);

	serial_port_out(port, UART_LCR, up->lcr);	 
	if (port->type != PORT_16750) {
		 
		if (up->fcr & UART_FCR_ENABLE_FIFO)
			serial_port_out(port, UART_FCR, UART_FCR_ENABLE_FIFO);
		serial_port_out(port, UART_FCR, up->fcr);	 
	}
	serial8250_set_mctrl(port, port->mctrl);
	spin_unlock_irqrestore(&port->lock, flags);
	serial8250_rpm_put(up);

	 
	if (tty_termios_baud_rate(termios))
		tty_termios_encode_baud_rate(termios, baud, baud);
}
EXPORT_SYMBOL(serial8250_do_set_termios);

static void
serial8250_set_termios(struct uart_port *port, struct ktermios *termios,
		       const struct ktermios *old)
{
	if (port->set_termios)
		port->set_termios(port, termios, old);
	else
		serial8250_do_set_termios(port, termios, old);
}

void serial8250_do_set_ldisc(struct uart_port *port, struct ktermios *termios)
{
	if (termios->c_line == N_PPS) {
		port->flags |= UPF_HARDPPS_CD;
		spin_lock_irq(&port->lock);
		serial8250_enable_ms(port);
		spin_unlock_irq(&port->lock);
	} else {
		port->flags &= ~UPF_HARDPPS_CD;
		if (!UART_ENABLE_MS(port, termios->c_cflag)) {
			spin_lock_irq(&port->lock);
			serial8250_disable_ms(port);
			spin_unlock_irq(&port->lock);
		}
	}
}
EXPORT_SYMBOL_GPL(serial8250_do_set_ldisc);

static void
serial8250_set_ldisc(struct uart_port *port, struct ktermios *termios)
{
	if (port->set_ldisc)
		port->set_ldisc(port, termios);
	else
		serial8250_do_set_ldisc(port, termios);
}

void serial8250_do_pm(struct uart_port *port, unsigned int state,
		      unsigned int oldstate)
{
	struct uart_8250_port *p = up_to_u8250p(port);

	serial8250_set_sleep(p, state != 0);
}
EXPORT_SYMBOL(serial8250_do_pm);

static void
serial8250_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
	if (port->pm)
		port->pm(port, state, oldstate);
	else
		serial8250_do_pm(port, state, oldstate);
}

static unsigned int serial8250_port_size(struct uart_8250_port *pt)
{
	if (pt->port.mapsize)
		return pt->port.mapsize;
	if (is_omap1_8250(pt))
		return 0x16 << pt->port.regshift;

	return 8 << pt->port.regshift;
}

 
static int serial8250_request_std_resource(struct uart_8250_port *up)
{
	unsigned int size = serial8250_port_size(up);
	struct uart_port *port = &up->port;
	int ret = 0;

	switch (port->iotype) {
	case UPIO_AU:
	case UPIO_TSI:
	case UPIO_MEM32:
	case UPIO_MEM32BE:
	case UPIO_MEM16:
	case UPIO_MEM:
		if (!port->mapbase) {
			ret = -EINVAL;
			break;
		}

		if (!request_mem_region(port->mapbase, size, "serial")) {
			ret = -EBUSY;
			break;
		}

		if (port->flags & UPF_IOREMAP) {
			port->membase = ioremap(port->mapbase, size);
			if (!port->membase) {
				release_mem_region(port->mapbase, size);
				ret = -ENOMEM;
			}
		}
		break;

	case UPIO_HUB6:
	case UPIO_PORT:
		if (!request_region(port->iobase, size, "serial"))
			ret = -EBUSY;
		break;
	}
	return ret;
}

static void serial8250_release_std_resource(struct uart_8250_port *up)
{
	unsigned int size = serial8250_port_size(up);
	struct uart_port *port = &up->port;

	switch (port->iotype) {
	case UPIO_AU:
	case UPIO_TSI:
	case UPIO_MEM32:
	case UPIO_MEM32BE:
	case UPIO_MEM16:
	case UPIO_MEM:
		if (!port->mapbase)
			break;

		if (port->flags & UPF_IOREMAP) {
			iounmap(port->membase);
			port->membase = NULL;
		}

		release_mem_region(port->mapbase, size);
		break;

	case UPIO_HUB6:
	case UPIO_PORT:
		release_region(port->iobase, size);
		break;
	}
}

static void serial8250_release_port(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	serial8250_release_std_resource(up);
}

static int serial8250_request_port(struct uart_port *port)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	return serial8250_request_std_resource(up);
}

static int fcr_get_rxtrig_bytes(struct uart_8250_port *up)
{
	const struct serial8250_config *conf_type = &uart_config[up->port.type];
	unsigned char bytes;

	bytes = conf_type->rxtrig_bytes[UART_FCR_R_TRIG_BITS(up->fcr)];

	return bytes ? bytes : -EOPNOTSUPP;
}

static int bytes_to_fcr_rxtrig(struct uart_8250_port *up, unsigned char bytes)
{
	const struct serial8250_config *conf_type = &uart_config[up->port.type];
	int i;

	if (!conf_type->rxtrig_bytes[UART_FCR_R_TRIG_BITS(UART_FCR_R_TRIG_00)])
		return -EOPNOTSUPP;

	for (i = 1; i < UART_FCR_R_TRIG_MAX_STATE; i++) {
		if (bytes < conf_type->rxtrig_bytes[i])
			 
			return (--i) << UART_FCR_R_TRIG_SHIFT;
	}

	return UART_FCR_R_TRIG_11;
}

static int do_get_rxtrig(struct tty_port *port)
{
	struct uart_state *state = container_of(port, struct uart_state, port);
	struct uart_port *uport = state->uart_port;
	struct uart_8250_port *up = up_to_u8250p(uport);

	if (!(up->capabilities & UART_CAP_FIFO) || uport->fifosize <= 1)
		return -EINVAL;

	return fcr_get_rxtrig_bytes(up);
}

static int do_serial8250_get_rxtrig(struct tty_port *port)
{
	int rxtrig_bytes;

	mutex_lock(&port->mutex);
	rxtrig_bytes = do_get_rxtrig(port);
	mutex_unlock(&port->mutex);

	return rxtrig_bytes;
}

static ssize_t rx_trig_bytes_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tty_port *port = dev_get_drvdata(dev);
	int rxtrig_bytes;

	rxtrig_bytes = do_serial8250_get_rxtrig(port);
	if (rxtrig_bytes < 0)
		return rxtrig_bytes;

	return sysfs_emit(buf, "%d\n", rxtrig_bytes);
}

static int do_set_rxtrig(struct tty_port *port, unsigned char bytes)
{
	struct uart_state *state = container_of(port, struct uart_state, port);
	struct uart_port *uport = state->uart_port;
	struct uart_8250_port *up = up_to_u8250p(uport);
	int rxtrig;

	if (!(up->capabilities & UART_CAP_FIFO) || uport->fifosize <= 1)
		return -EINVAL;

	rxtrig = bytes_to_fcr_rxtrig(up, bytes);
	if (rxtrig < 0)
		return rxtrig;

	serial8250_clear_fifos(up);
	up->fcr &= ~UART_FCR_TRIGGER_MASK;
	up->fcr |= (unsigned char)rxtrig;
	serial_out(up, UART_FCR, up->fcr);
	return 0;
}

static int do_serial8250_set_rxtrig(struct tty_port *port, unsigned char bytes)
{
	int ret;

	mutex_lock(&port->mutex);
	ret = do_set_rxtrig(port, bytes);
	mutex_unlock(&port->mutex);

	return ret;
}

static ssize_t rx_trig_bytes_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tty_port *port = dev_get_drvdata(dev);
	unsigned char bytes;
	int ret;

	if (!count)
		return -EINVAL;

	ret = kstrtou8(buf, 10, &bytes);
	if (ret < 0)
		return ret;

	ret = do_serial8250_set_rxtrig(port, bytes);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR_RW(rx_trig_bytes);

static struct attribute *serial8250_dev_attrs[] = {
	&dev_attr_rx_trig_bytes.attr,
	NULL
};

static struct attribute_group serial8250_dev_attr_group = {
	.attrs = serial8250_dev_attrs,
};

static void register_dev_spec_attr_grp(struct uart_8250_port *up)
{
	const struct serial8250_config *conf_type = &uart_config[up->port.type];

	if (conf_type->rxtrig_bytes[0])
		up->port.attr_group = &serial8250_dev_attr_group;
}

static void serial8250_config_port(struct uart_port *port, int flags)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	int ret;

	 
	ret = serial8250_request_std_resource(up);
	if (ret < 0)
		return;

	if (port->iotype != up->cur_iotype)
		set_io_from_upio(port);

	if (flags & UART_CONFIG_TYPE)
		autoconfig(up);

	 
	if (port->type == PORT_TEGRA)
		up->bugs |= UART_BUG_NOMSR;

	if (port->type != PORT_UNKNOWN && flags & UART_CONFIG_IRQ)
		autoconfig_irq(up);

	if (port->type == PORT_UNKNOWN)
		serial8250_release_std_resource(up);

	register_dev_spec_attr_grp(up);
	up->fcr = uart_config[up->port.type].fcr;
}

static int
serial8250_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	if (ser->irq >= nr_irqs || ser->irq < 0 ||
	    ser->baud_base < 9600 || ser->type < PORT_UNKNOWN ||
	    ser->type >= ARRAY_SIZE(uart_config) || ser->type == PORT_CIRRUS ||
	    ser->type == PORT_STARTECH)
		return -EINVAL;
	return 0;
}

static const char *serial8250_type(struct uart_port *port)
{
	int type = port->type;

	if (type >= ARRAY_SIZE(uart_config))
		type = 0;
	return uart_config[type].name;
}

static const struct uart_ops serial8250_pops = {
	.tx_empty	= serial8250_tx_empty,
	.set_mctrl	= serial8250_set_mctrl,
	.get_mctrl	= serial8250_get_mctrl,
	.stop_tx	= serial8250_stop_tx,
	.start_tx	= serial8250_start_tx,
	.throttle	= serial8250_throttle,
	.unthrottle	= serial8250_unthrottle,
	.stop_rx	= serial8250_stop_rx,
	.enable_ms	= serial8250_enable_ms,
	.break_ctl	= serial8250_break_ctl,
	.startup	= serial8250_startup,
	.shutdown	= serial8250_shutdown,
	.set_termios	= serial8250_set_termios,
	.set_ldisc	= serial8250_set_ldisc,
	.pm		= serial8250_pm,
	.type		= serial8250_type,
	.release_port	= serial8250_release_port,
	.request_port	= serial8250_request_port,
	.config_port	= serial8250_config_port,
	.verify_port	= serial8250_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = serial8250_get_poll_char,
	.poll_put_char = serial8250_put_poll_char,
#endif
};

void serial8250_init_port(struct uart_8250_port *up)
{
	struct uart_port *port = &up->port;

	spin_lock_init(&port->lock);
	port->ctrl_id = 0;
	port->pm = NULL;
	port->ops = &serial8250_pops;
	port->has_sysrq = IS_ENABLED(CONFIG_SERIAL_8250_CONSOLE);

	up->cur_iotype = 0xFF;
}
EXPORT_SYMBOL_GPL(serial8250_init_port);

void serial8250_set_defaults(struct uart_8250_port *up)
{
	struct uart_port *port = &up->port;

	if (up->port.flags & UPF_FIXED_TYPE) {
		unsigned int type = up->port.type;

		if (!up->port.fifosize)
			up->port.fifosize = uart_config[type].fifo_size;
		if (!up->tx_loadsz)
			up->tx_loadsz = uart_config[type].tx_loadsz;
		if (!up->capabilities)
			up->capabilities = uart_config[type].flags;
	}

	set_io_from_upio(port);

	 
	if (up->dma) {
		if (!up->dma->tx_dma)
			up->dma->tx_dma = serial8250_tx_dma;
		if (!up->dma->rx_dma)
			up->dma->rx_dma = serial8250_rx_dma;
	}
}
EXPORT_SYMBOL_GPL(serial8250_set_defaults);

#ifdef CONFIG_SERIAL_8250_CONSOLE

static void serial8250_console_putchar(struct uart_port *port, unsigned char ch)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	wait_for_xmitr(up, UART_LSR_THRE);
	serial_port_out(port, UART_TX, ch);
}

 
static void serial8250_console_restore(struct uart_8250_port *up)
{
	struct uart_port *port = &up->port;
	struct ktermios termios;
	unsigned int baud, quot, frac = 0;

	termios.c_cflag = port->cons->cflag;
	termios.c_ispeed = port->cons->ispeed;
	termios.c_ospeed = port->cons->ospeed;
	if (port->state->port.tty && termios.c_cflag == 0) {
		termios.c_cflag = port->state->port.tty->termios.c_cflag;
		termios.c_ispeed = port->state->port.tty->termios.c_ispeed;
		termios.c_ospeed = port->state->port.tty->termios.c_ospeed;
	}

	baud = serial8250_get_baud_rate(port, &termios, NULL);
	quot = serial8250_get_divisor(port, baud, &frac);

	serial8250_set_divisor(port, baud, quot, frac);
	serial_port_out(port, UART_LCR, up->lcr);
	serial8250_out_MCR(up, up->mcr | UART_MCR_DTR | UART_MCR_RTS);
}

 
static void serial8250_console_fifo_write(struct uart_8250_port *up,
					  const char *s, unsigned int count)
{
	int i;
	const char *end = s + count;
	unsigned int fifosize = up->tx_loadsz;
	bool cr_sent = false;

	while (s != end) {
		wait_for_lsr(up, UART_LSR_THRE);

		for (i = 0; i < fifosize && s != end; ++i) {
			if (*s == '\n' && !cr_sent) {
				serial_out(up, UART_TX, '\r');
				cr_sent = true;
			} else {
				serial_out(up, UART_TX, *s++);
				cr_sent = false;
			}
		}
	}
}

 
void serial8250_console_write(struct uart_8250_port *up, const char *s,
			      unsigned int count)
{
	struct uart_8250_em485 *em485 = up->em485;
	struct uart_port *port = &up->port;
	unsigned long flags;
	unsigned int ier, use_fifo;
	int locked = 1;

	touch_nmi_watchdog();

	if (oops_in_progress)
		locked = spin_trylock_irqsave(&port->lock, flags);
	else
		spin_lock_irqsave(&port->lock, flags);

	 
	ier = serial_port_in(port, UART_IER);
	serial8250_clear_IER(up);

	 
	if (up->canary && (up->canary != serial_port_in(port, UART_SCR))) {
		serial8250_console_restore(up);
		up->canary = 0;
	}

	if (em485) {
		if (em485->tx_stopped)
			up->rs485_start_tx(up);
		mdelay(port->rs485.delay_rts_before_send);
	}

	use_fifo = (up->capabilities & UART_CAP_FIFO) &&
		 
		!(up->capabilities & UART_CAP_MINI) &&
		 
		up->tx_loadsz > 1 &&
		(up->fcr & UART_FCR_ENABLE_FIFO) &&
		port->state &&
		test_bit(TTY_PORT_INITIALIZED, &port->state->port.iflags) &&
		 
		!(up->port.flags & UPF_CONS_FLOW);

	if (likely(use_fifo))
		serial8250_console_fifo_write(up, s, count);
	else
		uart_console_write(port, s, count, serial8250_console_putchar);

	 
	wait_for_xmitr(up, UART_LSR_BOTH_EMPTY);

	if (em485) {
		mdelay(port->rs485.delay_rts_after_send);
		if (em485->tx_stopped)
			up->rs485_stop_tx(up);
	}

	serial_port_out(port, UART_IER, ier);

	 
	if (up->msr_saved_flags)
		serial8250_modem_status(up);

	if (locked)
		spin_unlock_irqrestore(&port->lock, flags);
}

static unsigned int probe_baud(struct uart_port *port)
{
	unsigned char lcr, dll, dlm;
	unsigned int quot;

	lcr = serial_port_in(port, UART_LCR);
	serial_port_out(port, UART_LCR, lcr | UART_LCR_DLAB);
	dll = serial_port_in(port, UART_DLL);
	dlm = serial_port_in(port, UART_DLM);
	serial_port_out(port, UART_LCR, lcr);

	quot = (dlm << 8) | dll;
	return (port->uartclk / 16) / quot;
}

int serial8250_console_setup(struct uart_port *port, char *options, bool probe)
{
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	int ret;

	if (!port->iobase && !port->membase)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else if (probe)
		baud = probe_baud(port);

	ret = uart_set_options(port, port->cons, baud, parity, bits, flow);
	if (ret)
		return ret;

	if (port->dev)
		pm_runtime_get_sync(port->dev);

	return 0;
}

int serial8250_console_exit(struct uart_port *port)
{
	if (port->dev)
		pm_runtime_put_sync(port->dev);

	return 0;
}

#endif  

MODULE_LICENSE("GPL");
