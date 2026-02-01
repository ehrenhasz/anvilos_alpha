
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME	"fb_uc1701"
#define WIDTH	  102
#define HEIGHT	 64
#define PAGES	  (HEIGHT / 8)

 
#define LCD_DISPLAY_ENABLE    0xAE
 
#define LCD_START_LINE	0x40
 
#define LCD_PAGE_ADDRESS      0xB0
 
#define LCD_COL_ADDRESS       0x10
 
#define LCD_BOTTOMVIEW	0xA0
 
#define LCD_DISPLAY_INVERT    0xA6
 
#define LCD_ALL_PIXEL	 0xA4
 
#define LCD_BIAS	      0xA2
 
#define LCD_RESET_CMD	 0xE2
 
#define LCD_SCAN_DIR	  0xC0
 
#define LCD_POWER_CONTROL     0x28
 
#define LCD_VOLTAGE	   0x20
 
#define LCD_VOLUME_MODE       0x81
 
#define LCD_NO_OP	     0xE3
 
#define LCD_ADV_PROG_CTRL     0xFA
 
#define LCD_ADV_PROG_CTRL2    0x10
#define LCD_TEMPCOMP_HIGH     0x80
 
#define SHIFT_ADDR_NORMAL     0
 
#define SHIFT_ADDR_TOPVIEW    30

static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	 
	write_reg(par, LCD_RESET_CMD);
	mdelay(10);

	 
	write_reg(par, LCD_START_LINE);

	 
	write_reg(par, LCD_BOTTOMVIEW | 1);

	 
	write_reg(par, LCD_SCAN_DIR | 0x00);

	 
	write_reg(par, LCD_ALL_PIXEL | 0);

	 
	write_reg(par, LCD_DISPLAY_INVERT | 0);

	 
	write_reg(par, LCD_BIAS | 0);

	 
	write_reg(par, LCD_POWER_CONTROL | 0x07);

	 
	write_reg(par, LCD_VOLTAGE | 0x07);

	 
	write_reg(par, LCD_VOLUME_MODE);
	write_reg(par, 0x09);
	write_reg(par, LCD_NO_OP);

	 
	write_reg(par, LCD_ADV_PROG_CTRL);
	write_reg(par, LCD_ADV_PROG_CTRL2 | LCD_TEMPCOMP_HIGH);

	 
	write_reg(par, LCD_DISPLAY_ENABLE | 1);

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	 
	write_reg(par, LCD_PAGE_ADDRESS);
	write_reg(par, 0x00);
	write_reg(par, LCD_COL_ADDRESS);
}

static int write_vmem(struct fbtft_par *par, size_t offset, size_t len)
{
	u16 *vmem16 = (u16 *)par->info->screen_buffer;
	u8 *buf;
	int x, y, i;
	int ret = 0;

	for (y = 0; y < PAGES; y++) {
		buf = par->txbuf.buf;
		for (x = 0; x < WIDTH; x++) {
			*buf = 0x00;
			for (i = 0; i < 8; i++)
				*buf |= (vmem16[((y * 8 * WIDTH) +
						 (i * WIDTH)) + x] ?
					 1 : 0) << i;
			buf++;
		}

		write_reg(par, LCD_PAGE_ADDRESS | (u8)y);
		write_reg(par, 0x00);
		write_reg(par, LCD_COL_ADDRESS);
		gpiod_set_value(par->gpio.dc, 1);
		ret = par->fbtftops.write(par, par->txbuf.buf, WIDTH);
		gpiod_set_value(par->gpio.dc, 0);
	}

	if (ret < 0)
		dev_err(par->info->device, "write failed and returned: %d\n",
			ret);

	return ret;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.write_vmem = write_vmem,
	},
	.backlight = 1,
};

FBTFT_REGISTER_DRIVER(DRVNAME, "UltraChip,uc1701", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("spi:uc1701");

MODULE_DESCRIPTION("FB driver for the UC1701 LCD Controller");
MODULE_AUTHOR("Juergen Holzmann");
MODULE_LICENSE("GPL");
