
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME		"fb_hx8347d"
#define WIDTH		320
#define HEIGHT		240
#define DEFAULT_GAMMA	"0 0 0 0 0 0 0 0 0 0 0 0 0 0\n" \
			"0 0 0 0 0 0 0 0 0 0 0 0 0 0"

static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	 
	write_reg(par, 0xEA, 0x00);
	write_reg(par, 0xEB, 0x20);
	write_reg(par, 0xEC, 0x0C);
	write_reg(par, 0xED, 0xC4);
	write_reg(par, 0xE8, 0x40);
	write_reg(par, 0xE9, 0x38);
	write_reg(par, 0xF1, 0x01);
	write_reg(par, 0xF2, 0x10);
	write_reg(par, 0x27, 0xA3);

	 
	write_reg(par, 0x1B, 0x1B);
	write_reg(par, 0x1A, 0x01);
	write_reg(par, 0x24, 0x2F);
	write_reg(par, 0x25, 0x57);

	 
	write_reg(par, 0x23, 0x8D);  

	 
	write_reg(par, 0x18, 0x36);
	write_reg(par, 0x19, 0x01);  
	write_reg(par, 0x01, 0x00);  
	write_reg(par, 0x1F, 0x88);
	mdelay(5);
	write_reg(par, 0x1F, 0x80);
	mdelay(5);
	write_reg(par, 0x1F, 0x90);
	mdelay(5);
	write_reg(par, 0x1F, 0xD0);
	mdelay(5);

	 
	write_reg(par, 0x17, 0x05);  

	 
	write_reg(par, 0x36, 0x00);

	 
	write_reg(par, 0x28, 0x38);
	mdelay(40);
	write_reg(par, 0x28, 0x3C);

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	write_reg(par, 0x02, (xs >> 8) & 0xFF);
	write_reg(par, 0x03, xs & 0xFF);
	write_reg(par, 0x04, (xe >> 8) & 0xFF);
	write_reg(par, 0x05, xe & 0xFF);
	write_reg(par, 0x06, (ys >> 8) & 0xFF);
	write_reg(par, 0x07, ys & 0xFF);
	write_reg(par, 0x08, (ye >> 8) & 0xFF);
	write_reg(par, 0x09, ye & 0xFF);
	write_reg(par, 0x22);
}

#define MEM_Y   BIT(7)  
#define MEM_X   BIT(6)  
#define MEM_V   BIT(5)  
#define MEM_L   BIT(4)  
#define MEM_BGR (3)  
static int set_var(struct fbtft_par *par)
{
	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, 0x16, MEM_V | MEM_X | (par->bgr << MEM_BGR));
		break;
	case 270:
		write_reg(par, 0x16, par->bgr << MEM_BGR);
		break;
	case 180:
		write_reg(par, 0x16, MEM_V | MEM_Y | (par->bgr << MEM_BGR));
		break;
	case 90:
		write_reg(par, 0x16, MEM_X | MEM_Y | (par->bgr << MEM_BGR));
		break;
	}

	return 0;
}

 
#define CURVE(num, idx)  curves[(num) * par->gamma.num_values + (idx)]
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	static const unsigned long mask[] = {
		0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x7f, 0x7f, 0x1f, 0x1f,
		0x1f, 0x1f, 0x1f, 0x0f,
	};
	int i, j;
	int acc = 0;

	 
	for (i = 0; i < par->gamma.num_curves; i++)
		for (j = 0; j < par->gamma.num_values; j++) {
			acc += CURVE(i, j);
			CURVE(i, j) &= mask[j];
		}

	if (acc == 0)  
		return 0;

	for (i = 0; i < par->gamma.num_curves; i++) {
		write_reg(par, 0x40 + (i * 0x10), CURVE(i, 0));
		write_reg(par, 0x41 + (i * 0x10), CURVE(i, 1));
		write_reg(par, 0x42 + (i * 0x10), CURVE(i, 2));
		write_reg(par, 0x43 + (i * 0x10), CURVE(i, 3));
		write_reg(par, 0x44 + (i * 0x10), CURVE(i, 4));
		write_reg(par, 0x45 + (i * 0x10), CURVE(i, 5));
		write_reg(par, 0x46 + (i * 0x10), CURVE(i, 6));
		write_reg(par, 0x47 + (i * 0x10), CURVE(i, 7));
		write_reg(par, 0x48 + (i * 0x10), CURVE(i, 8));
		write_reg(par, 0x49 + (i * 0x10), CURVE(i, 9));
		write_reg(par, 0x4A + (i * 0x10), CURVE(i, 10));
		write_reg(par, 0x4B + (i * 0x10), CURVE(i, 11));
		write_reg(par, 0x4C + (i * 0x10), CURVE(i, 12));
	}
	write_reg(par, 0x5D, (CURVE(1, 0) << 4) | CURVE(0, 0));

	return 0;
}

#undef CURVE

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.set_gamma = set_gamma,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "himax,hx8347d", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:hx8347d");
MODULE_ALIAS("platform:hx8347d");

MODULE_DESCRIPTION("FB driver for the HX8347D LCD Controller");
MODULE_AUTHOR("Christian Vogelgsang");
MODULE_LICENSE("GPL");
