
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME		"fb_tls8204"
#define WIDTH		84
#define HEIGHT		48
#define TXBUFLEN	WIDTH

 
#define DEFAULT_GAMMA	"40"

static unsigned int bs = 4;
module_param(bs, uint, 0000);
MODULE_PARM_DESC(bs, "BS[2:0] Bias voltage level: 0-7 (default: 4)");

static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	 
	write_reg(par, 0x21);	 

	 
	write_reg(par, 0x10 | (bs & 0x7));
				 

	 
	write_reg(par, 0x04 | (64 >> 6));
	write_reg(par, 0x40 | (64 & 0x3F));

	 
	write_reg(par, 0x20);

	 
	write_reg(par, 0x08 | 4);
				 

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	 
	write_reg(par, 0x80);	 

	 
	write_reg(par, 0x40);	 
}

static int write_vmem(struct fbtft_par *par, size_t offset, size_t len)
{
	u16 *vmem16 = (u16 *)par->info->screen_buffer;
	int x, y, i;
	int ret = 0;

	for (y = 0; y < HEIGHT / 8; y++) {
		u8 *buf = par->txbuf.buf;
		 
		gpiod_set_value(par->gpio.dc, 0);
		write_reg(par, 0x80 | 0);
		write_reg(par, 0x40 | y);

		for (x = 0; x < WIDTH; x++) {
			u8 ch = 0;

			for (i = 0; i < 8 * WIDTH; i += WIDTH) {
				ch >>= 1;
				if (vmem16[(y * 8 * WIDTH) + i + x])
					ch |= 0x80;
			}
			*buf++ = ch;
		}
		 
		gpiod_set_value(par->gpio.dc, 1);
		ret = par->fbtftops.write(par, par->txbuf.buf, WIDTH);
		if (ret < 0) {
			dev_err(par->info->device,
				"write failed and returned: %d\n", ret);
			break;
		}
	}

	return ret;
}

static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	 
	curves[0] &= 0x7F;

	write_reg(par, 0x21);  
	write_reg(par, 0x80 | curves[0]);
	write_reg(par, 0x20);  

	return 0;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.txbuflen = TXBUFLEN,
	.gamma_num = 1,
	.gamma_len = 1,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.write_vmem = write_vmem,
		.set_gamma = set_gamma,
	},
	.backlight = 1,
};

FBTFT_REGISTER_DRIVER(DRVNAME, "teralane,tls8204", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("spi:tls8204");

MODULE_DESCRIPTION("FB driver for the TLS8204 LCD Controller");
MODULE_AUTHOR("Michael Hope");
MODULE_LICENSE("GPL");
