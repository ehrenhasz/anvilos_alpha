
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/parport.h>
#include <linux/ks0108.h>

#define KS0108_NAME "ks0108"

 

static unsigned int ks0108_port = CONFIG_KS0108_PORT;
module_param(ks0108_port, uint, 0444);
MODULE_PARM_DESC(ks0108_port, "Parallel port where the LCD is connected");

static unsigned int ks0108_delay = CONFIG_KS0108_DELAY;
module_param(ks0108_delay, uint, 0444);
MODULE_PARM_DESC(ks0108_delay, "Delay between each control writing (microseconds)");

 

static struct parport *ks0108_parport;
static struct pardevice *ks0108_pardevice;

 

#define bit(n) (((unsigned char)1)<<(n))

void ks0108_writedata(unsigned char byte)
{
	parport_write_data(ks0108_parport, byte);
}

void ks0108_writecontrol(unsigned char byte)
{
	udelay(ks0108_delay);
	parport_write_control(ks0108_parport, byte ^ (bit(0) | bit(1) | bit(3)));
}

void ks0108_displaystate(unsigned char state)
{
	ks0108_writedata((state ? bit(0) : 0) | bit(1) | bit(2) | bit(3) | bit(4) | bit(5));
}

void ks0108_startline(unsigned char startline)
{
	ks0108_writedata(min_t(unsigned char, startline, 63) | bit(6) |
			 bit(7));
}

void ks0108_address(unsigned char address)
{
	ks0108_writedata(min_t(unsigned char, address, 63) | bit(6));
}

void ks0108_page(unsigned char page)
{
	ks0108_writedata(min_t(unsigned char, page, 7) | bit(3) | bit(4) |
			 bit(5) | bit(7));
}

EXPORT_SYMBOL_GPL(ks0108_writedata);
EXPORT_SYMBOL_GPL(ks0108_writecontrol);
EXPORT_SYMBOL_GPL(ks0108_displaystate);
EXPORT_SYMBOL_GPL(ks0108_startline);
EXPORT_SYMBOL_GPL(ks0108_address);
EXPORT_SYMBOL_GPL(ks0108_page);

 

static unsigned char ks0108_inited;
unsigned char ks0108_isinited(void)
{
	return ks0108_inited;
}
EXPORT_SYMBOL_GPL(ks0108_isinited);

static void ks0108_parport_attach(struct parport *port)
{
	struct pardev_cb ks0108_cb;

	if (port->base != ks0108_port)
		return;

	memset(&ks0108_cb, 0, sizeof(ks0108_cb));
	ks0108_cb.flags = PARPORT_DEV_EXCL;
	ks0108_pardevice = parport_register_dev_model(port, KS0108_NAME,
						      &ks0108_cb, 0);
	if (!ks0108_pardevice) {
		pr_err("ERROR: parport didn't register new device\n");
		return;
	}
	if (parport_claim(ks0108_pardevice)) {
		pr_err("could not claim access to parport %i. Aborting.\n",
		       ks0108_port);
		goto err_unreg_device;
	}

	ks0108_parport = port;
	ks0108_inited = 1;
	return;

err_unreg_device:
	parport_unregister_device(ks0108_pardevice);
	ks0108_pardevice = NULL;
}

static void ks0108_parport_detach(struct parport *port)
{
	if (port->base != ks0108_port)
		return;

	if (!ks0108_pardevice) {
		pr_err("%s: already unregistered.\n", KS0108_NAME);
		return;
	}

	parport_release(ks0108_pardevice);
	parport_unregister_device(ks0108_pardevice);
	ks0108_pardevice = NULL;
	ks0108_parport = NULL;
}

 

static struct parport_driver ks0108_parport_driver = {
	.name = "ks0108",
	.match_port = ks0108_parport_attach,
	.detach = ks0108_parport_detach,
	.devmodel = true,
};
module_parport_driver(ks0108_parport_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Miguel Ojeda <ojeda@kernel.org>");
MODULE_DESCRIPTION("ks0108 LCD Controller driver");

