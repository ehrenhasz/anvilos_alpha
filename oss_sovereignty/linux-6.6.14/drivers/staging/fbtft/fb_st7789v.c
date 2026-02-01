
 

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/module.h>

#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME "fb_st7789v"

#define DEFAULT_GAMMA \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25\n" \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25"

#define HSD20_IPS_GAMMA \
	"D0 05 0A 09 08 05 2E 44 45 0F 17 16 2B 33\n" \
	"D0 05 0A 09 08 05 2E 43 45 0F 16 16 2B 33"

#define HSD20_IPS 1

 
enum st7789v_command {
	PORCTRL = 0xB2,
	GCTRL = 0xB7,
	VCOMS = 0xBB,
	VDVVRHEN = 0xC2,
	VRHS = 0xC3,
	VDVS = 0xC4,
	VCMOFSET = 0xC5,
	PWCTRL1 = 0xD0,
	PVGAMCTRL = 0xE0,
	NVGAMCTRL = 0xE1,
};

#define MADCTL_BGR BIT(3)  
#define MADCTL_MV BIT(5)  
#define MADCTL_MX BIT(6)  
#define MADCTL_MY BIT(7)  

 
#define PANEL_TE_TIMEOUT_MS  33

static struct completion panel_te;  
static int irq_te;  

static irqreturn_t panel_te_handler(int irq, void *data)
{
	complete(&panel_te);
	return IRQ_HANDLED;
}

 
static int init_tearing_effect_line(struct fbtft_par *par)
{
	struct device *dev = par->info->device;
	struct gpio_desc *te;
	int rc, irq;

	te = gpiod_get_optional(dev, "te", GPIOD_IN);
	if (IS_ERR(te))
		return dev_err_probe(dev, PTR_ERR(te), "Failed to request te GPIO\n");

	 
	if (!te) {
		irq_te = 0;
		return 0;
	}

	irq = gpiod_to_irq(te);

	 
	gpiod_put(te);

	if (irq < 0)
		return irq;

	irq_te = irq;
	init_completion(&panel_te);

	 
	rc = devm_request_irq(dev, irq_te, panel_te_handler,
			      IRQF_TRIGGER_RISING, "TE_GPIO", par);
	if (rc)
		return dev_err_probe(dev, rc, "TE IRQ request failed.\n");

	disable_irq_nosync(irq_te);

	return 0;
}

 
static int init_display(struct fbtft_par *par)
{
	int rc;

	par->fbtftops.reset(par);

	rc = init_tearing_effect_line(par);
	if (rc)
		return rc;

	 
	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	 
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, MIPI_DCS_PIXEL_FMT_16BIT);
	if (HSD20_IPS)
		write_reg(par, PORCTRL, 0x05, 0x05, 0x00, 0x33, 0x33);

	else
		write_reg(par, PORCTRL, 0x08, 0x08, 0x00, 0x22, 0x22);

	 
	if (HSD20_IPS)
		write_reg(par, GCTRL, 0x75);
	else
		write_reg(par, GCTRL, 0x35);

	 
	write_reg(par, VDVVRHEN, 0x01, 0xFF);

	 
	if (HSD20_IPS)
		write_reg(par, VRHS, 0x13);
	else
		write_reg(par, VRHS, 0x0B);

	 
	write_reg(par, VDVS, 0x20);

	 
	if (HSD20_IPS)
		write_reg(par, VCOMS, 0x22);
	else
		write_reg(par, VCOMS, 0x20);

	 
	write_reg(par, VCMOFSET, 0x20);

	 
	write_reg(par, PWCTRL1, 0xA4, 0xA1);

	 
	if (irq_te)
		write_reg(par, MIPI_DCS_SET_TEAR_ON, 0x00);

	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);

	if (HSD20_IPS)
		write_reg(par, MIPI_DCS_ENTER_INVERT_MODE);

	return 0;
}

 
static int write_vmem(struct fbtft_par *par, size_t offset, size_t len)
{
	struct device *dev = par->info->device;
	int ret;

	if (irq_te) {
		enable_irq(irq_te);
		reinit_completion(&panel_te);
		ret = wait_for_completion_timeout(&panel_te,
						  msecs_to_jiffies(PANEL_TE_TIMEOUT_MS));
		if (ret == 0)
			dev_err(dev, "wait panel TE timeout\n");

		disable_irq(irq_te);
	}

	switch (par->pdata->display.buswidth) {
	case 8:
		ret = fbtft_write_vmem16_bus8(par, offset, len);
		break;
	case 9:
		ret = fbtft_write_vmem16_bus9(par, offset, len);
		break;
	case 16:
		ret = fbtft_write_vmem16_bus16(par, offset, len);
		break;
	default:
		dev_err(dev, "Unsupported buswidth %d\n",
			par->pdata->display.buswidth);
		ret = 0;
		break;
	}

	return ret;
}

 
static int set_var(struct fbtft_par *par)
{
	u8 madctl_par = 0;

	if (par->bgr)
		madctl_par |= MADCTL_BGR;
	switch (par->info->var.rotate) {
	case 0:
		break;
	case 90:
		madctl_par |= (MADCTL_MV | MADCTL_MY);
		break;
	case 180:
		madctl_par |= (MADCTL_MX | MADCTL_MY);
		break;
	case 270:
		madctl_par |= (MADCTL_MV | MADCTL_MX);
		break;
	default:
		return -EINVAL;
	}
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctl_par);
	return 0;
}

 
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	int i;
	int j;
	int c;  

	 
	static const u8 gamma_par_mask[] = {
		0xFF,  
		0x3F,  
		0x3F,  
		0x1F,  
		0x1F,  
		0x3F,  
		0x7F,  
		0x77,  
		0x7F,  
		0x3F,  
		0x1F,  
		0x1F,  
		0x3F,  
		0x3F,  
	};

	for (i = 0; i < par->gamma.num_curves; i++) {
		c = i * par->gamma.num_values;
		for (j = 0; j < par->gamma.num_values; j++)
			curves[c + j] &= gamma_par_mask[j];
		write_reg(par, PVGAMCTRL + i,
			  curves[c + 0],  curves[c + 1],  curves[c + 2],
			  curves[c + 3],  curves[c + 4],  curves[c + 5],
			  curves[c + 6],  curves[c + 7],  curves[c + 8],
			  curves[c + 9],  curves[c + 10], curves[c + 11],
			  curves[c + 12], curves[c + 13]);
	}
	return 0;
}

 
static int blank(struct fbtft_par *par, bool on)
{
	if (on)
		write_reg(par, MIPI_DCS_SET_DISPLAY_OFF);
	else
		write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	return 0;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = 240,
	.height = 320,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = HSD20_IPS_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.write_vmem = write_vmem,
		.set_var = set_var,
		.set_gamma = set_gamma,
		.blank = blank,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7789v", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7789v");
MODULE_ALIAS("platform:st7789v");

MODULE_DESCRIPTION("FB driver for the ST7789V LCD Controller");
MODULE_AUTHOR("Dennis Menschel");
MODULE_LICENSE("GPL");
