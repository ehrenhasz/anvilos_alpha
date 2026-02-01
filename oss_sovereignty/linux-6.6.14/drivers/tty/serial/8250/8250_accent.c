
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/serial_8250.h>

#include "8250.h"

static struct plat_serial8250_port accent_data[] = {
	SERIAL8250_PORT(0x330, 4),
	SERIAL8250_PORT(0x338, 4),
	{ },
};

static struct platform_device accent_device = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_ACCENT,
	.dev			= {
		.platform_data	= accent_data,
	},
};

static int __init accent_init(void)
{
	return platform_device_register(&accent_device);
}

module_init(accent_init);

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("8250 serial probe module for Accent Async cards");
MODULE_LICENSE("GPL");
