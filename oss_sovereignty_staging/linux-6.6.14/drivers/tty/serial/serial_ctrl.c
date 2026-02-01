
 

#include <linux/device.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/serial_core.h>
#include <linux/spinlock.h>

#include "serial_base.h"

static int serial_ctrl_probe(struct device *dev)
{
	pm_runtime_enable(dev);

	return 0;
}

static int serial_ctrl_remove(struct device *dev)
{
	pm_runtime_disable(dev);

	return 0;
}

 
int serial_ctrl_register_port(struct uart_driver *drv, struct uart_port *port)
{
	return serial_core_register_port(drv, port);
}

void serial_ctrl_unregister_port(struct uart_driver *drv, struct uart_port *port)
{
	serial_core_unregister_port(drv, port);
}

static struct device_driver serial_ctrl_driver = {
	.name = "ctrl",
	.suppress_bind_attrs = true,
	.probe = serial_ctrl_probe,
	.remove = serial_ctrl_remove,
};

int serial_base_ctrl_init(void)
{
	return serial_base_driver_register(&serial_ctrl_driver);
}

void serial_base_ctrl_exit(void)
{
	serial_base_driver_unregister(&serial_ctrl_driver);
}

MODULE_AUTHOR("Tony Lindgren <tony@atomide.com>");
MODULE_DESCRIPTION("Serial core controller driver");
MODULE_LICENSE("GPL");
