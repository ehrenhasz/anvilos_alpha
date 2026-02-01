
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME		"fb_ili9325"
#define WIDTH		240
#define HEIGHT		320
#define BPP		16
#define FPS		20
#define DEFAULT_GAMMA	"0F 00 7 2 0 0 6 5 4 1\n" \
			"04 16 2 7 6 3 2 1 7 7"

static unsigned int bt = 6;  
module_param(bt, uint, 0000);
MODULE_PARM_DESC(bt, "Sets the factor used in the step-up circuits");

static unsigned int vc = 0x03;  
module_param(vc, uint, 0000);
MODULE_PARM_DESC(vc, "Sets the ratio factor of Vci to generate the reference voltages Vci1");

static unsigned int vrh = 0x0d;  
module_param(vrh, uint, 0000);
MODULE_PARM_DESC(vrh, "Set the amplifying rate (1.6 ~ 1.9) of Vci applied to output the VREG1OUT");

static unsigned int vdv = 0x12;  
module_param(vdv, uint, 0000);
MODULE_PARM_DESC(vdv, "Select the factor of VREG1OUT to set the amplitude of Vcom");

static unsigned int vcm = 0x0a;  
module_param(vcm, uint, 0000);
MODULE_PARM_DESC(vcm, "Set the internal VcomH voltage");

 

static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	bt &= 0x07;
	vc &= 0x07;
	vrh &= 0x0f;
	vdv &= 0x1f;
	vcm &= 0x3f;

	 

	 
	write_reg(par, 0x00E3, 0x3008);  
	write_reg(par, 0x00E7, 0x0012);  
	write_reg(par, 0x00EF, 0x1231);  
	write_reg(par, 0x0001, 0x0100);  
	write_reg(par, 0x0002, 0x0700);  
	write_reg(par, 0x0004, 0x0000);  
	write_reg(par, 0x0008, 0x0207);  
	write_reg(par, 0x0009, 0x0000);  
	write_reg(par, 0x000A, 0x0000);  
	write_reg(par, 0x000C, 0x0000);  
	write_reg(par, 0x000D, 0x0000);  
	write_reg(par, 0x000F, 0x0000);  

	 
	write_reg(par, 0x0010, 0x0000);  
	write_reg(par, 0x0011, 0x0007);  
	write_reg(par, 0x0012, 0x0000);  
	write_reg(par, 0x0013, 0x0000);  
	mdelay(200);  
	write_reg(par, 0x0010,  
		BIT(12) | (bt << 8) | BIT(7) | BIT(4));
	write_reg(par, 0x0011, 0x220 | vc);  
	mdelay(50);  
	write_reg(par, 0x0012, vrh);  
	mdelay(50);  
	write_reg(par, 0x0013, vdv << 8);  
	write_reg(par, 0x0029, vcm);  
	write_reg(par, 0x002B, 0x000C);  
	mdelay(50);  
	write_reg(par, 0x0020, 0x0000);  
	write_reg(par, 0x0021, 0x0000);  

	 
	write_reg(par, 0x0050, 0x0000);  
	write_reg(par, 0x0051, 0x00EF);  
	write_reg(par, 0x0052, 0x0000);  
	write_reg(par, 0x0053, 0x013F);  
	write_reg(par, 0x0060, 0xA700);  
	write_reg(par, 0x0061, 0x0001);  
	write_reg(par, 0x006A, 0x0000);  

	 
	write_reg(par, 0x0080, 0x0000);
	write_reg(par, 0x0081, 0x0000);
	write_reg(par, 0x0082, 0x0000);
	write_reg(par, 0x0083, 0x0000);
	write_reg(par, 0x0084, 0x0000);
	write_reg(par, 0x0085, 0x0000);

	 
	write_reg(par, 0x0090, 0x0010);
	write_reg(par, 0x0092, 0x0600);
	write_reg(par, 0x0007, 0x0133);  

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	switch (par->info->var.rotate) {
	 
	 
	case 0:
		write_reg(par, 0x0020, xs);
		write_reg(par, 0x0021, ys);
		break;
	case 180:
		write_reg(par, 0x0020, WIDTH - 1 - xs);
		write_reg(par, 0x0021, HEIGHT - 1 - ys);
		break;
	case 270:
		write_reg(par, 0x0020, WIDTH - 1 - ys);
		write_reg(par, 0x0021, xs);
		break;
	case 90:
		write_reg(par, 0x0020, ys);
		write_reg(par, 0x0021, HEIGHT - 1 - xs);
		break;
	}
	write_reg(par, 0x0022);  
}

static int set_var(struct fbtft_par *par)
{
	switch (par->info->var.rotate) {
	 
	case 0:
		write_reg(par, 0x03, 0x0030 | (par->bgr << 12));
		break;
	case 180:
		write_reg(par, 0x03, 0x0000 | (par->bgr << 12));
		break;
	case 270:
		write_reg(par, 0x03, 0x0028 | (par->bgr << 12));
		break;
	case 90:
		write_reg(par, 0x03, 0x0018 | (par->bgr << 12));
		break;
	}

	return 0;
}

 
#define CURVE(num, idx)  curves[(num) * par->gamma.num_values + (idx)]
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	static const unsigned long mask[] = {
		0x1f, 0x1f, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
		0x1f, 0x1f, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	};
	int i, j;

	 
	for (i = 0; i < 2; i++)
		for (j = 0; j < 10; j++)
			CURVE(i, j) &= mask[i * par->gamma.num_values + j];

	write_reg(par, 0x0030, CURVE(0, 5) << 8 | CURVE(0, 4));
	write_reg(par, 0x0031, CURVE(0, 7) << 8 | CURVE(0, 6));
	write_reg(par, 0x0032, CURVE(0, 9) << 8 | CURVE(0, 8));
	write_reg(par, 0x0035, CURVE(0, 3) << 8 | CURVE(0, 2));
	write_reg(par, 0x0036, CURVE(0, 1) << 8 | CURVE(0, 0));

	write_reg(par, 0x0037, CURVE(1, 5) << 8 | CURVE(1, 4));
	write_reg(par, 0x0038, CURVE(1, 7) << 8 | CURVE(1, 6));
	write_reg(par, 0x0039, CURVE(1, 9) << 8 | CURVE(1, 8));
	write_reg(par, 0x003C, CURVE(1, 3) << 8 | CURVE(1, 2));
	write_reg(par, 0x003D, CURVE(1, 1) << 8 | CURVE(1, 0));

	return 0;
}

#undef CURVE

static struct fbtft_display display = {
	.regwidth = 16,
	.width = WIDTH,
	.height = HEIGHT,
	.bpp = BPP,
	.fps = FPS,
	.gamma_num = 2,
	.gamma_len = 10,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.set_gamma = set_gamma,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "ilitek,ili9325", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:ili9325");
MODULE_ALIAS("platform:ili9325");

MODULE_DESCRIPTION("FB driver for the ILI9325 LCD Controller");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
