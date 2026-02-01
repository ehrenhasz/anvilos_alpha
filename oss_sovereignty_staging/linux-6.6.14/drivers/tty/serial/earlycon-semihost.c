
 
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <asm/semihost.h>

static void smh_write(struct console *con, const char *s, unsigned n)
{
	struct earlycon_device *dev = con->data;
	uart_console_write(&dev->port, s, n, smh_putc);
}

static int
__init early_smh_setup(struct earlycon_device *device, const char *opt)
{
	device->con->write = smh_write;
	return 0;
}
EARLYCON_DECLARE(smh, early_smh_setup);
