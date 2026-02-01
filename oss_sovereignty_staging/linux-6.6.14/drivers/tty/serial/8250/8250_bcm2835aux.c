
 

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#include "8250.h"

#define BCM2835_AUX_UART_CNTL		8
#define BCM2835_AUX_UART_CNTL_RXEN	0x01  
#define BCM2835_AUX_UART_CNTL_TXEN	0x02  
#define BCM2835_AUX_UART_CNTL_AUTORTS	0x04  
#define BCM2835_AUX_UART_CNTL_AUTOCTS	0x08  
#define BCM2835_AUX_UART_CNTL_RTS3	0x00  
#define BCM2835_AUX_UART_CNTL_RTS2	0x10  
#define BCM2835_AUX_UART_CNTL_RTS1	0x20  
#define BCM2835_AUX_UART_CNTL_RTS4	0x30  
#define BCM2835_AUX_UART_CNTL_RTSINV	0x40  
#define BCM2835_AUX_UART_CNTL_CTSINV	0x80  

 
struct bcm2835aux_data {
	struct clk *clk;
	int line;
	u32 cntl;
};

struct bcm2835_aux_serial_driver_data {
	resource_size_t offset;
};

static void bcm2835aux_rs485_start_tx(struct uart_8250_port *up)
{
	if (!(up->port.rs485.flags & SER_RS485_RX_DURING_TX)) {
		struct bcm2835aux_data *data = dev_get_drvdata(up->port.dev);

		data->cntl &= ~BCM2835_AUX_UART_CNTL_RXEN;
		serial_out(up, BCM2835_AUX_UART_CNTL, data->cntl);
	}

	 
	if (up->port.rs485.flags & SER_RS485_RTS_ON_SEND)
		serial8250_out_MCR(up, 0);
	else
		serial8250_out_MCR(up, UART_MCR_RTS);
}

static void bcm2835aux_rs485_stop_tx(struct uart_8250_port *up)
{
	if (up->port.rs485.flags & SER_RS485_RTS_AFTER_SEND)
		serial8250_out_MCR(up, 0);
	else
		serial8250_out_MCR(up, UART_MCR_RTS);

	if (!(up->port.rs485.flags & SER_RS485_RX_DURING_TX)) {
		struct bcm2835aux_data *data = dev_get_drvdata(up->port.dev);

		data->cntl |= BCM2835_AUX_UART_CNTL_RXEN;
		serial_out(up, BCM2835_AUX_UART_CNTL, data->cntl);
	}
}

static int bcm2835aux_serial_probe(struct platform_device *pdev)
{
	const struct bcm2835_aux_serial_driver_data *bcm_data;
	struct uart_8250_port up = { };
	struct bcm2835aux_data *data;
	resource_size_t offset = 0;
	struct resource *res;
	unsigned int uartclk;
	int ret;

	 
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	 
	up.capabilities = UART_CAP_FIFO | UART_CAP_MINI;
	up.port.dev = &pdev->dev;
	up.port.regshift = 2;
	up.port.type = PORT_16550;
	up.port.iotype = UPIO_MEM;
	up.port.fifosize = 8;
	up.port.flags = UPF_SHARE_IRQ | UPF_FIXED_PORT | UPF_FIXED_TYPE |
			UPF_SKIP_TEST | UPF_IOREMAP;
	up.port.rs485_config = serial8250_em485_config;
	up.port.rs485_supported = serial8250_em485_supported;
	up.rs485_start_tx = bcm2835aux_rs485_start_tx;
	up.rs485_stop_tx = bcm2835aux_rs485_stop_tx;

	 
	data->cntl = BCM2835_AUX_UART_CNTL_RXEN | BCM2835_AUX_UART_CNTL_TXEN;

	platform_set_drvdata(pdev, data);

	 
	data->clk = devm_clk_get_optional(&pdev->dev, NULL);
	if (IS_ERR(data->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(data->clk), "could not get clk\n");

	 
	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		return ret;
	up.port.irq = ret;

	 
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "memory resource not found");
		return -EINVAL;
	}

	bcm_data = device_get_match_data(&pdev->dev);

	 
	if (bcm_data)
		offset = bcm_data->offset;

	up.port.mapbase = res->start + offset;
	up.port.mapsize = resource_size(res) - offset;

	 
	ret = of_alias_get_id(pdev->dev.of_node, "serial");
	if (ret >= 0)
		up.port.line = ret;

	 
	ret = clk_prepare_enable(data->clk);
	if (ret) {
		dev_err(&pdev->dev, "unable to enable uart clock - %d\n",
			ret);
		return ret;
	}

	uartclk = clk_get_rate(data->clk);
	if (!uartclk) {
		ret = device_property_read_u32(&pdev->dev, "clock-frequency", &uartclk);
		if (ret) {
			dev_err_probe(&pdev->dev, ret, "could not get clk rate\n");
			goto dis_clk;
		}
	}

	 
	up.port.uartclk = uartclk * 2;

	 
	ret = serial8250_register_8250_port(&up);
	if (ret < 0) {
		dev_err_probe(&pdev->dev, ret, "unable to register 8250 port\n");
		goto dis_clk;
	}
	data->line = ret;

	return 0;

dis_clk:
	clk_disable_unprepare(data->clk);
	return ret;
}

static int bcm2835aux_serial_remove(struct platform_device *pdev)
{
	struct bcm2835aux_data *data = platform_get_drvdata(pdev);

	serial8250_unregister_port(data->line);
	clk_disable_unprepare(data->clk);

	return 0;
}

static const struct bcm2835_aux_serial_driver_data bcm2835_acpi_data = {
	.offset = 0x40,
};

static const struct of_device_id bcm2835aux_serial_match[] = {
	{ .compatible = "brcm,bcm2835-aux-uart" },
	{ },
};
MODULE_DEVICE_TABLE(of, bcm2835aux_serial_match);

static const struct acpi_device_id bcm2835aux_serial_acpi_match[] = {
	{ "BCM2836", (kernel_ulong_t)&bcm2835_acpi_data },
	{ }
};
MODULE_DEVICE_TABLE(acpi, bcm2835aux_serial_acpi_match);

static struct platform_driver bcm2835aux_serial_driver = {
	.driver = {
		.name = "bcm2835-aux-uart",
		.of_match_table = bcm2835aux_serial_match,
		.acpi_match_table = bcm2835aux_serial_acpi_match,
	},
	.probe  = bcm2835aux_serial_probe,
	.remove = bcm2835aux_serial_remove,
};
module_platform_driver(bcm2835aux_serial_driver);

#ifdef CONFIG_SERIAL_8250_CONSOLE

static int __init early_bcm2835aux_setup(struct earlycon_device *device,
					const char *options)
{
	if (!device->port.membase)
		return -ENODEV;

	device->port.iotype = UPIO_MEM32;
	device->port.regshift = 2;

	return early_serial8250_setup(device, NULL);
}

OF_EARLYCON_DECLARE(bcm2835aux, "brcm,bcm2835-aux-uart",
		    early_bcm2835aux_setup);
#endif

MODULE_DESCRIPTION("BCM2835 auxiliar UART driver");
MODULE_AUTHOR("Martin Sperl <kernel@martin.sperl.org>");
MODULE_LICENSE("GPL v2");
