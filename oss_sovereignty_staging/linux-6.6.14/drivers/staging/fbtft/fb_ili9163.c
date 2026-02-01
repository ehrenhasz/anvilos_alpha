
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME		"fb_ili9163"
#define WIDTH		128
#define HEIGHT		128
#define BPP		16
#define FPS		30

#ifdef GAMMA_ADJ
#define GAMMA_LEN	15
#define GAMMA_NUM	1
#define DEFAULT_GAMMA	"36 29 12 22 1C 15 42 B7 2F 13 12 0A 11 0B 06\n"
#endif

 
#define CMD_FRMCTR1	0xB1  
			      
#define CMD_FRMCTR2	0xB2  
#define CMD_FRMCTR3	0xB3  
			      
#define CMD_DINVCTR	0xB4  
#define CMD_RGBBLK	0xB5  
#define CMD_DFUNCTR	0xB6  
#define CMD_SDRVDIR	0xB7  
#define CMD_GDRVDIR	0xB8  

#define CMD_PWCTR1	0xC0  
#define CMD_PWCTR2	0xC1  
#define CMD_PWCTR3	0xC2  
#define CMD_PWCTR4	0xC3  
#define CMD_PWCTR5	0xC4  
#define CMD_VCOMCTR1	0xC5  
#define CMD_VCOMCTR2	0xC6  
#define CMD_VCOMOFFS	0xC7  
#define CMD_PGAMMAC	0xE0  
#define CMD_NGAMMAC	0xE1  
#define CMD_GAMRSEL	0xF2  

 

#ifdef RED
#define __OFFSET		32  
#else
#define __OFFSET		0   
#endif

static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	write_reg(par, MIPI_DCS_SOFT_RESET);  
	mdelay(500);
	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);  
	mdelay(5);
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, MIPI_DCS_PIXEL_FMT_16BIT);
	 
	write_reg(par, MIPI_DCS_SET_GAMMA_CURVE, 0x02);
#ifdef GAMMA_ADJ
	write_reg(par, CMD_GAMRSEL, 0x01);  
#endif
	write_reg(par, MIPI_DCS_ENTER_NORMAL_MODE);
	write_reg(par, CMD_DFUNCTR, 0xff, 0x06);
	 
	write_reg(par, CMD_FRMCTR1, 0x08, 0x02);
	write_reg(par, CMD_DINVCTR, 0x07);  
	 
	write_reg(par, CMD_PWCTR1, 0x0A, 0x02);
	 
	write_reg(par, CMD_PWCTR2, 0x02);
	 
	write_reg(par, CMD_VCOMCTR1, 0x50, 0x63);
	write_reg(par, CMD_VCOMOFFS, 0);

	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS, 0, 0, 0, WIDTH);
	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS, 0, 0, 0, HEIGHT);

	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);  
	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);  

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys,
			 int xe, int ye)
{
	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS,
			  xs >> 8, xs & 0xff, xe >> 8, xe & 0xff);
		write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS,
			  (ys + __OFFSET) >> 8, (ys + __OFFSET) & 0xff,
			  (ye + __OFFSET) >> 8, (ye + __OFFSET) & 0xff);
		break;
	case 90:
		write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS,
			  (xs + __OFFSET) >> 8, (xs + __OFFSET) & 0xff,
			  (xe + __OFFSET) >> 8, (xe + __OFFSET) & 0xff);
		write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS,
			  ys >> 8, ys & 0xff, ye >> 8, ye & 0xff);
		break;
	case 180:
	case 270:
		write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS,
			  xs >> 8, xs & 0xff, xe >> 8, xe & 0xff);
		write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS,
			  ys >> 8, ys & 0xff, ye >> 8, ye & 0xff);
		break;
	default:
		 
		par->info->var.rotate = 0;
	}
	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
}

 
static int set_var(struct fbtft_par *par)
{
	u8 mactrl_data = 0;  

	switch (par->info->var.rotate) {
	case 0:
		mactrl_data = 0x08;
		break;
	case 180:
		mactrl_data = 0xC8;
		break;
	case 270:
		mactrl_data = 0xA8;
		break;
	case 90:
		mactrl_data = 0x68;
		break;
	}

	 
	if (par->bgr)
		mactrl_data |= BIT(2);
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, mactrl_data);
	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
	return 0;
}

#ifdef GAMMA_ADJ
#define CURVE(num, idx)  curves[(num) * par->gamma.num_values + (idx)]
static int gamma_adj(struct fbtft_par *par, u32 *curves)
{
	static const unsigned long mask[] = {
		0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
		0x1f, 0x3f, 0x0f, 0x0f, 0x7f, 0x1f,
		0x3F, 0x3F, 0x3F, 0x3F, 0x3F};
	int i, j;

	for (i = 0; i < GAMMA_NUM; i++)
		for (j = 0; j < GAMMA_LEN; j++)
			CURVE(i, j) &= mask[i * par->gamma.num_values + j];

	write_reg(par, CMD_PGAMMAC,
		  CURVE(0, 0),
		  CURVE(0, 1),
		  CURVE(0, 2),
		  CURVE(0, 3),
		  CURVE(0, 4),
		  CURVE(0, 5),
		  CURVE(0, 6),
		  (CURVE(0, 7) << 4) | CURVE(0, 8),
		  CURVE(0, 9),
		  CURVE(0, 10),
		  CURVE(0, 11),
		  CURVE(0, 12),
		  CURVE(0, 13),
		  CURVE(0, 14),
		  CURVE(0, 15));

	 
	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);

	return 0;
}

#undef CURVE
#endif

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.bpp = BPP,
	.fps = FPS,
#ifdef GAMMA_ADJ
	.gamma_num = GAMMA_NUM,
	.gamma_len = GAMMA_LEN,
	.gamma = DEFAULT_GAMMA,
#endif
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_var = set_var,
#ifdef GAMMA_ADJ
		.set_gamma = gamma_adj,
#endif
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "ilitek,ili9163", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:ili9163");
MODULE_ALIAS("platform:ili9163");

MODULE_DESCRIPTION("FB driver for the ILI9163 LCD Controller");
MODULE_AUTHOR("Kozhevnikov Anatoly");
MODULE_LICENSE("GPL");
