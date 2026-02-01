
 
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pnp.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/property.h>
#include <linux/serial_core.h>
#include <linux/bitops.h>

#include <asm/byteorder.h>

#include "8250.h"

#define UNKNOWN_DEV 0x3000
#define CIR_PORT	0x0800

static const struct pnp_device_id pnp_dev_table[] = {
	 
	 
	{	"AAC000F",		0	},
	 
	 
	{	"ADC0001",		0	},
	 
	{	"ADC0002",		0	},
	 
	{	"AEI0250",		0	},
	 
	{	"AEI1240",		0	},
	 
	{	"AKY1021",		0  	},
	 
	{	"ALI5123",		0	},
	 
	{	"AZT4001",		0	},
	 
	{	"BDP3336",		0	},
	 
	 
	{	"BRI0A49",		0	},
	 
	{	"BRI1400",		0	},
	 
	{	"BRI3400",		0	},
	 
	 
	{	"CPI4050",		0	},
	 
	 
	{	"CTL3001",		0	},
	 
	{	"CTL3011",		0	},
	 
	{	"DAV0336",		0	},
	 
	 
	{	"DMB1032",		0	},
	 
	{	"DMB2001",		0	},
	 
	 
	{	"ETT0002",		0	},
	 
	 
	{	"FUJ0202",		0	},
	 
	{	"FUJ0205",		0	},
	 
	{	"FUJ0206",		0	},
	 
	{	"FUJ0209",		0	},
	 
	 
	{	"GVC000F",		0	},
	 
	{	"GVC0303",		0	},
	 
	 
	{	"HAY0001",		0	},
	 
	{	"HAY000C",		0	},
	 
	{	"HAY000D",		0	},
	 
	{	"HAY5670",		0	},
	 
	{	"HAY5674",		0	},
	 
	{	"HAY5675",		0	},
	 
	{	"HAYF000",		0	},
	 
	{	"HAYF001",		0	},
	 
	 
	{	"IBM0033",		0	},
	 
	 
	{	"PNP4972",		0	},
	 
	 
	{	"IXDC801",		0	},
	 
	{	"IXDC901",		0	},
	 
	{	"IXDD801",		0	},
	 
	{	"IXDD901",		0	},
	 
	{	"IXDF401",		0	},
	 
	{	"IXDF801",		0	},
	 
	{	"IXDF901",		0	},
	 
	 
	{	"KOR4522",		0	},
	 
	{	"KORF661",		0	},
	 
	 
	{	"LAS4040",		0	},
	 
	{	"LAS4540",		0	},
	 
	{	"LAS5440",		0	},
	 
	 
	{	"MNP0281",		0	},
	 
	{	"MNP0336",		0	},
	 
	{	"MNP0339",		0	},
	 
	{	"MNP0342",		0	},
	 
	{	"MNP0500",		0	},
	 
	{	"MNP0501",		0	},
	 
	{	"MNP0502",		0	},
	 
	 
	{	"MOT1105",		0	},
	 
	{	"MOT1111",		0	},
	 
	{	"MOT1114",		0	},
	 
	{	"MOT1115",		0	},
	 
	{	"MOT1190",		0	},
	 
	{	"MOT1501",		0	},
	 
	{	"MOT1502",		0	},
	 
	{	"MOT1505",		0	},
	 
	{	"MOT1509",		0	},
	 
	{	"MOT150A",		0	},
	 
	{	"MOT150F",		0	},
	 
	{	"MOT1510",		0	},
	 
	{	"MOT1550",		0	},
	 
	{	"MOT1560",		0	},
	 
	{	"MOT1580",		0	},
	 
	{	"MOT15B0",		0	},
	 
	{	"MOT15F0",		0	},
	 
	 
	{	"MVX00A1",		0	},
	 
	{	"MVX00F2",		0	},
	 
	{	"nEC8241",		0	},
	 
	{	"PMC2430",		0	},
	 
	 
	{	"PNP0500",		0	},
	 
	{	"PNP0501",		0	},
	 
	{	"PNPC000",		0	},
	 
	{	"PNPC001",		0	},
	 
	{	"PNPC031",		0	},
	 
	{	"PNPC032",		0	},
	 
	{	"PNPC100",		0	},
	 
	{	"PNPC101",		0	},
	 
	{	"PNPC102",		0	},
	 
	{	"PNPC103",		0	},
	 
	{	"PNPC104",		0	},
	 
	{	"PNPC105",		0	},
	 
	{	"PNPC106",		0	},
	 
	{	"PNPC107",		0	},
	 
	{	"PNPC108",		0	},
	 
	{	"PNPC109",		0	},
	 
	{	"PNPC10A",		0	},
	 
	{	"PNPC10B",		0	},
	 
	{	"PNPC10C",		0	},
	 
	{	"PNPC10D",		0	},
	 
	{	"PNPC10E",		0	},
	 
	{	"PNPC10F",		0	},
	 
	{	"PNP2000",		0	},
	 
	 
	 
	 
	{	"ROK0030",		0	},
	 
	 
	{	"ROK0100",		0	},
	 
	{	"ROK4120",		0	},
	 
	 
	{	"ROK4920",		0	},
	 
	 
	 
	 
	 
	 
	{	"RSS00A0",		0	},
	 
	{	"RSS0262",		0	},
	 
	{       "RSS0250",              0       },
	 
	{	"SUP1310",		0	},
	 
	{	"SUP1381",		0	},
	 
	{	"SUP1421",		0	},
	 
	{	"SUP1590",		0	},
	 
	{	"SUP1620",		0	},
	 
	{	"SUP1760",		0	},
	 
	{	"SUP2171",		0	},
	 
	 
	{	"TEX0011",		0	},
	 
	 
	{	"UAC000F",		0	},
	 
	 
	{	"USR0000",		0	},
	 
	{	"USR0002",		0	},
	 
	{	"USR0004",		0	},
	 
	{	"USR0006",		0	},
	 
	{	"USR0007",		0	},
	 
	{	"USR0009",		0	},
	 
	{	"USR2002",		0	},
	 
	{	"USR2070",		0	},
	 
	{	"USR2080",		0	},
	 
	{	"USR3031",		0	},
	 
	{	"USR3050",		0	},
	 
	{	"USR3070",		0	},
	 
	{	"USR3080",		0	},
	 
	{	"USR3090",		0	},
	 
	{	"USR9100",		0	},
	 
	{	"USR9160",		0	},
	 
	{	"USR9170",		0	},
	 
	{	"USR9180",		0	},
	 
	{	"USR9190",		0	},
	 
	{	"WACFXXX",		0	},
	 
	{       "FPI2002",              0 },
	 
	{       "FUJ02B2",              0 },
	{       "FUJ02B3",              0 },
	 
	{       "FUJ02B4",              0 },
	 
	{       "FUJ02B6",              0 },
	{       "FUJ02B7",              0 },
	{       "FUJ02B8",              0 },
	{       "FUJ02B9",              0 },
	{       "FUJ02BC",              0 },
	 
	{	"FUJ02E5",		0	},
	 
	{	"FUJ02E6",		0	},
	 
	{	"FUJ02E7",		0	},
	 
	{	"FUJ02E9",		0	},
	 
	{	"LTS0001",		0       },
	 
	{	"WCI0003",		0	},
	 
	{	"PNPCXXX",		UNKNOWN_DEV	},
	 
	{	"PNPDXXX",		UNKNOWN_DEV	},
	 
	{	"WEC1022",		CIR_PORT	},
	 
	{	"SMCF010",		CIR_PORT	},
	{	"",			0	}
};

MODULE_DEVICE_TABLE(pnp, pnp_dev_table);

static const char *modem_names[] = {
	"MODEM", "Modem", "modem", "FAX", "Fax", "fax",
	"56K", "56k", "K56", "33.6", "28.8", "14.4",
	"33,600", "28,800", "14,400", "33.600", "28.800", "14.400",
	"33600", "28800", "14400", "V.90", "V.34", "V.32", NULL
};

static bool check_name(const char *name)
{
	const char **tmp;

	for (tmp = modem_names; *tmp; tmp++)
		if (strstr(name, *tmp))
			return true;

	return false;
}

static bool check_resources(struct pnp_dev *dev)
{
	static const resource_size_t base[] = {0x2f8, 0x3f8, 0x2e8, 0x3e8};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(base); i++) {
		if (pnp_possible_config(dev, IORESOURCE_IO, base[i], 8))
			return true;
	}

	return false;
}

 
static int serial_pnp_guess_board(struct pnp_dev *dev)
{
	if (!(check_name(pnp_dev_name(dev)) ||
	    (dev->card && check_name(dev->card->name))))
		return -ENODEV;

	if (check_resources(dev))
		return 0;

	return -ENODEV;
}

static int
serial_pnp_probe(struct pnp_dev *dev, const struct pnp_device_id *dev_id)
{
	struct uart_8250_port uart, *port;
	int ret, line, flags = dev_id->driver_data;

	if (flags & UNKNOWN_DEV) {
		ret = serial_pnp_guess_board(dev);
		if (ret < 0)
			return ret;
	}

	memset(&uart, 0, sizeof(uart));
	if (pnp_irq_valid(dev, 0))
		uart.port.irq = pnp_irq(dev, 0);
	if ((flags & CIR_PORT) && pnp_port_valid(dev, 2)) {
		uart.port.iobase = pnp_port_start(dev, 2);
		uart.port.iotype = UPIO_PORT;
	} else if (pnp_port_valid(dev, 0)) {
		uart.port.iobase = pnp_port_start(dev, 0);
		uart.port.iotype = UPIO_PORT;
	} else if (pnp_mem_valid(dev, 0)) {
		uart.port.mapbase = pnp_mem_start(dev, 0);
		uart.port.iotype = UPIO_MEM;
		uart.port.flags = UPF_IOREMAP;
	} else
		return -ENODEV;

	dev_dbg(&dev->dev,
		 "Setup PNP port: port %#lx, mem %#llx, irq %u, type %u\n",
		 uart.port.iobase, (unsigned long long)uart.port.mapbase,
		 uart.port.irq, uart.port.iotype);

	if (flags & CIR_PORT) {
		uart.port.flags |= UPF_FIXED_PORT | UPF_FIXED_TYPE;
		uart.port.type = PORT_8250_CIR;
	}

	uart.port.flags |= UPF_SKIP_TEST | UPF_BOOT_AUTOCONF;
	if (pnp_irq_flags(dev, 0) & IORESOURCE_IRQ_SHAREABLE)
		uart.port.flags |= UPF_SHARE_IRQ;
	uart.port.uartclk = 1843200;
	device_property_read_u32(&dev->dev, "clock-frequency", &uart.port.uartclk);
	uart.port.dev = &dev->dev;

	line = serial8250_register_8250_port(&uart);
	if (line < 0 || (flags & CIR_PORT))
		return -ENODEV;

	port = serial8250_get_port(line);
	if (uart_console(&port->port))
		dev->capabilities |= PNP_CONSOLE;

	pnp_set_drvdata(dev, (void *)((long)line + 1));
	return 0;
}

static void serial_pnp_remove(struct pnp_dev *dev)
{
	long line = (long)pnp_get_drvdata(dev);

	dev->capabilities &= ~PNP_CONSOLE;
	if (line)
		serial8250_unregister_port(line - 1);
}

static int __maybe_unused serial_pnp_suspend(struct device *dev)
{
	long line = (long)dev_get_drvdata(dev);

	if (!line)
		return -ENODEV;
	serial8250_suspend_port(line - 1);
	return 0;
}

static int __maybe_unused serial_pnp_resume(struct device *dev)
{
	long line = (long)dev_get_drvdata(dev);

	if (!line)
		return -ENODEV;
	serial8250_resume_port(line - 1);
	return 0;
}

static SIMPLE_DEV_PM_OPS(serial_pnp_pm_ops, serial_pnp_suspend, serial_pnp_resume);

static struct pnp_driver serial_pnp_driver = {
	.name		= "serial",
	.probe		= serial_pnp_probe,
	.remove		= serial_pnp_remove,
	.driver         = {
		.pm     = &serial_pnp_pm_ops,
	},
	.id_table	= pnp_dev_table,
};

int serial8250_pnp_init(void)
{
	return pnp_register_driver(&serial_pnp_driver);
}

void serial8250_pnp_exit(void)
{
	pnp_unregister_driver(&serial_pnp_driver);
}

