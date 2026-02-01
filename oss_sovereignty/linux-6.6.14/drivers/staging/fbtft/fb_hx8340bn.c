
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME		"fb_hx8340bn"
#define WIDTH		176
#define HEIGHT		220
#define TXBUFLEN	(4 * PAGE_SIZE)
#define DEFAULT_GAMMA	"1 3 0E 5 0 2 09 0 6 1 7 1 0 2 2\n" \
			"3 3 17 8 4 7 05 7 6 0 3 1 6 0 0 "

static bool emulate;
module_param(emulate, bool, 0000);
MODULE_PARM_DESC(emulate, "Force emulation in 9-bit mode");

static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	 

	 
	write_reg(par, 0xC1, 0xFF, 0x83, 0x40);

	 
	write_reg(par, 0x11);
	mdelay(150);

	 
	write_reg(par, 0xCA, 0x70, 0x00, 0xD9);

	 
	write_reg(par, 0xB0, 0x01, 0x11);

	 
	write_reg(par, 0xC9, 0x90, 0x49, 0x10, 0x28, 0x28, 0x10, 0x00, 0x06);
	mdelay(20);

	 
	write_reg(par, 0xB5, 0x35, 0x20, 0x45);

	 
	write_reg(par, 0xB4, 0x33, 0x25, 0x4C);
	mdelay(10);

	 
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, MIPI_DCS_PIXEL_FMT_16BIT);

	 
	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	mdelay(10);

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS, 0x00, xs, 0x00, xe);
	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS, 0x00, ys, 0x00, ye);
	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
}

static int set_var(struct fbtft_par *par)
{
	 
	 
#define MY BIT(7)
#define MX BIT(6)
#define MV BIT(5)
	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, par->bgr << 3);
		break;
	case 270:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  MX | MV | (par->bgr << 3));
		break;
	case 180:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  MX | MY | (par->bgr << 3));
		break;
	case 90:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  MY | MV | (par->bgr << 3));
		break;
	}

	return 0;
}

 
#define CURVE(num, idx)  curves[(num) * par->gamma.num_values + (idx)]
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	static const unsigned long mask[] = {
		0x0f, 0x0f, 0x1f, 0x0f, 0x0f, 0x0f, 0x1f, 0x07, 0x07, 0x07,
		0x07, 0x07, 0x07, 0x03, 0x03, 0x0f, 0x0f, 0x1f, 0x0f, 0x0f,
		0x0f, 0x1f, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x00, 0x00,
	};
	int i, j;

	 
	for (i = 0; i < par->gamma.num_curves; i++)
		for (j = 0; j < par->gamma.num_values; j++)
			CURVE(i, j) &= mask[i * par->gamma.num_values + j];

	 
	write_reg(par, MIPI_DCS_SET_GAMMA_CURVE, 1 << CURVE(1, 14));

	if (CURVE(1, 14))
		return 0;  

	write_reg(par, 0xC2,
		  (CURVE(0, 8) << 4) | CURVE(0, 7),
		  (CURVE(0, 10) << 4) | CURVE(0, 9),
		  (CURVE(0, 12) << 4) | CURVE(0, 11),
		  CURVE(0, 2),
		  (CURVE(0, 4) << 4) | CURVE(0, 3),
		  CURVE(0, 5),
		  CURVE(0, 6),
		  (CURVE(0, 1) << 4) | CURVE(0, 0),
		  (CURVE(0, 14) << 2) | CURVE(0, 13));

	write_reg(par, 0xC3,
		  (CURVE(1, 8) << 4) | CURVE(1, 7),
		  (CURVE(1, 10) << 4) | CURVE(1, 9),
		  (CURVE(1, 12) << 4) | CURVE(1, 11),
		  CURVE(1, 2),
		  (CURVE(1, 4) << 4) | CURVE(1, 3),
		  CURVE(1, 5),
		  CURVE(1, 6),
		  (CURVE(1, 1) << 4) | CURVE(1, 0));

	mdelay(10);

	return 0;
}

#undef CURVE

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.txbuflen = TXBUFLEN,
	.gamma_num = 2,
	.gamma_len = 15,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.set_gamma = set_gamma,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "himax,hx8340bn", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:hx8340bn");
MODULE_ALIAS("platform:hx8340bn");

MODULE_DESCRIPTION("FB driver for the HX8340BN LCD Controller");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
