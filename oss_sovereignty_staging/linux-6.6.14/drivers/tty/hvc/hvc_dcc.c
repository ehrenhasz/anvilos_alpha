
 

#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

#include <asm/dcc.h>
#include <asm/processor.h>

#include "hvc_console.h"

 
#define DCC_STATUS_RX		(1 << 30)
#define DCC_STATUS_TX		(1 << 29)

#define DCC_INBUF_SIZE		128
#define DCC_OUTBUF_SIZE		1024

 
static DEFINE_SPINLOCK(dcc_lock);

static DEFINE_KFIFO(inbuf, unsigned char, DCC_INBUF_SIZE);
static DEFINE_KFIFO(outbuf, unsigned char, DCC_OUTBUF_SIZE);

static void dcc_uart_console_putchar(struct uart_port *port, unsigned char ch)
{
	while (__dcc_getstatus() & DCC_STATUS_TX)
		cpu_relax();

	__dcc_putchar(ch);
}

static void dcc_early_write(struct console *con, const char *s, unsigned n)
{
	struct earlycon_device *dev = con->data;

	uart_console_write(&dev->port, s, n, dcc_uart_console_putchar);
}

static int __init dcc_early_console_setup(struct earlycon_device *device,
					  const char *opt)
{
	device->con->write = dcc_early_write;

	return 0;
}

EARLYCON_DECLARE(dcc, dcc_early_console_setup);

static int hvc_dcc_put_chars(uint32_t vt, const char *buf, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		while (__dcc_getstatus() & DCC_STATUS_TX)
			cpu_relax();

		__dcc_putchar(buf[i]);
	}

	return count;
}

static int hvc_dcc_get_chars(uint32_t vt, char *buf, int count)
{
	int i;

	for (i = 0; i < count; ++i)
		if (__dcc_getstatus() & DCC_STATUS_RX)
			buf[i] = __dcc_getchar();
		else
			break;

	return i;
}

 
static bool hvc_dcc_check(void)
{
	unsigned long time = jiffies + (HZ / 10);
	static bool dcc_core0_available;

	 
	int cpu = get_cpu();

	if (IS_ENABLED(CONFIG_HVC_DCC_SERIALIZE_SMP) && cpu && dcc_core0_available) {
		put_cpu();
		return true;
	}

	put_cpu();

	 
	__dcc_putchar('\n');

	while (time_is_after_jiffies(time)) {
		if (!(__dcc_getstatus() & DCC_STATUS_TX)) {
			dcc_core0_available = true;
			return true;
		}
	}

	return false;
}

 
static void dcc_put_work(struct work_struct *work)
{
	unsigned char ch;
	unsigned long irqflags;

	spin_lock_irqsave(&dcc_lock, irqflags);

	 
	while (kfifo_get(&outbuf, &ch))
		hvc_dcc_put_chars(0, &ch, 1);

	 
	while (!kfifo_is_full(&inbuf)) {
		if (!hvc_dcc_get_chars(0, &ch, 1))
			break;
		kfifo_put(&inbuf, ch);
	}

	spin_unlock_irqrestore(&dcc_lock, irqflags);
}

static DECLARE_WORK(dcc_pwork, dcc_put_work);

 
static void dcc_get_work(struct work_struct *work)
{
	unsigned char ch;
	unsigned long irqflags;

	 
	spin_lock_irqsave(&dcc_lock, irqflags);

	while (!kfifo_is_full(&inbuf)) {
		if (!hvc_dcc_get_chars(0, &ch, 1))
			break;
		kfifo_put(&inbuf, ch);
	}
	spin_unlock_irqrestore(&dcc_lock, irqflags);
}

static DECLARE_WORK(dcc_gwork, dcc_get_work);

 
static int hvc_dcc0_put_chars(u32 vt, const char *buf, int count)
{
	int len;
	unsigned long irqflags;

	if (!IS_ENABLED(CONFIG_HVC_DCC_SERIALIZE_SMP))
		return hvc_dcc_put_chars(vt, buf, count);

	spin_lock_irqsave(&dcc_lock, irqflags);
	if (smp_processor_id() || (!kfifo_is_empty(&outbuf))) {
		len = kfifo_in(&outbuf, buf, count);
		spin_unlock_irqrestore(&dcc_lock, irqflags);

		 
		if (cpu_online(0))
			schedule_work_on(0, &dcc_pwork);

		return len;
	}

	 
	len = hvc_dcc_put_chars(vt, buf, count);
	spin_unlock_irqrestore(&dcc_lock, irqflags);

	return len;
}

 
static int hvc_dcc0_get_chars(u32 vt, char *buf, int count)
{
	int len;
	unsigned long irqflags;

	if (!IS_ENABLED(CONFIG_HVC_DCC_SERIALIZE_SMP))
		return hvc_dcc_get_chars(vt, buf, count);

	spin_lock_irqsave(&dcc_lock, irqflags);

	if (smp_processor_id() || (!kfifo_is_empty(&inbuf))) {
		len = kfifo_out(&inbuf, buf, count);
		spin_unlock_irqrestore(&dcc_lock, irqflags);

		 
		if (!len && cpu_online(0))
			schedule_work_on(0, &dcc_gwork);

		return len;
	}

	 
	len = hvc_dcc_get_chars(vt, buf, count);
	spin_unlock_irqrestore(&dcc_lock, irqflags);

	return len;
}

static const struct hv_ops hvc_dcc_get_put_ops = {
	.get_chars = hvc_dcc0_get_chars,
	.put_chars = hvc_dcc0_put_chars,
};

static int __init hvc_dcc_console_init(void)
{
	int ret;

	if (!hvc_dcc_check())
		return -ENODEV;

	 
	ret = hvc_instantiate(0, 0, &hvc_dcc_get_put_ops);

	return ret < 0 ? -ENODEV : 0;
}
console_initcall(hvc_dcc_console_init);

static int __init hvc_dcc_init(void)
{
	struct hvc_struct *p;

	if (!hvc_dcc_check())
		return -ENODEV;

	if (IS_ENABLED(CONFIG_HVC_DCC_SERIALIZE_SMP)) {
		pr_warn("\n");
		pr_warn("********************************************************************\n");
		pr_warn("**     NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE           **\n");
		pr_warn("**                                                                **\n");
		pr_warn("**  HVC_DCC_SERIALIZE_SMP SUPPORT HAS BEEN ENABLED IN THIS KERNEL **\n");
		pr_warn("**                                                                **\n");
		pr_warn("** This means that this is a DEBUG kernel and unsafe for          **\n");
		pr_warn("** production use and has important feature like CPU hotplug      **\n");
		pr_warn("** disabled.                                                      **\n");
		pr_warn("**                                                                **\n");
		pr_warn("** If you see this message and you are not debugging the          **\n");
		pr_warn("** kernel, report this immediately to your vendor!                **\n");
		pr_warn("**                                                                **\n");
		pr_warn("**     NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE           **\n");
		pr_warn("********************************************************************\n");

		cpu_hotplug_disable();
	}

	p = hvc_alloc(0, 0, &hvc_dcc_get_put_ops, 128);

	return PTR_ERR_OR_ZERO(p);
}
device_initcall(hvc_dcc_init);
