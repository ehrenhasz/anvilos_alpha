 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/uaccess.h>

#include <video/hecubafb.h>

 
#define DPY_W 600
#define DPY_H 800

static const struct fb_fix_screeninfo hecubafb_fix = {
	.id =		"hecubafb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_MONO01,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0,
	.line_length =	DPY_W,
	.accel =	FB_ACCEL_NONE,
};

static const struct fb_var_screeninfo hecubafb_var = {
	.xres		= DPY_W,
	.yres		= DPY_H,
	.xres_virtual	= DPY_W,
	.yres_virtual	= DPY_H,
	.bits_per_pixel	= 1,
	.nonstd		= 1,
};

 

static void apollo_send_data(struct hecubafb_par *par, unsigned char data)
{
	 
	par->board->set_data(par, data);

	 
	par->board->set_ctl(par, HCB_DS_BIT, 0);

	 
	par->board->wait_for_ack(par, 0);

	 
	par->board->set_ctl(par, HCB_DS_BIT, 1);

	 
	par->board->wait_for_ack(par, 1);
}

static void apollo_send_command(struct hecubafb_par *par, unsigned char data)
{
	 
	par->board->set_ctl(par, HCB_CD_BIT, 1);

	 
	apollo_send_data(par, data);

	 
	par->board->set_ctl(par, HCB_CD_BIT, 0);
}

static void hecubafb_dpy_update(struct hecubafb_par *par)
{
	int i;
	unsigned char *buf = par->info->screen_buffer;

	apollo_send_command(par, APOLLO_START_NEW_IMG);

	for (i=0; i < (DPY_W*DPY_H/8); i++) {
		apollo_send_data(par, *(buf++));
	}

	apollo_send_command(par, APOLLO_STOP_IMG_DATA);
	apollo_send_command(par, APOLLO_DISPLAY_IMG);
}

 
static void hecubafb_dpy_deferred_io(struct fb_info *info, struct list_head *pagereflist)
{
	hecubafb_dpy_update(info->par);
}

static void hecubafb_defio_damage_range(struct fb_info *info, off_t off, size_t len)
{
	struct hecubafb_par *par = info->par;

	hecubafb_dpy_update(par);
}

static void hecubafb_defio_damage_area(struct fb_info *info, u32 x, u32 y,
				       u32 width, u32 height)
{
	struct hecubafb_par *par = info->par;

	hecubafb_dpy_update(par);
}

FB_GEN_DEFAULT_DEFERRED_SYSMEM_OPS(hecubafb,
				   hecubafb_defio_damage_range,
				   hecubafb_defio_damage_area)

static const struct fb_ops hecubafb_ops = {
	.owner	= THIS_MODULE,
	FB_DEFAULT_DEFERRED_OPS(hecubafb),
};

static struct fb_deferred_io hecubafb_defio = {
	.delay		= HZ,
	.deferred_io	= hecubafb_dpy_deferred_io,
};

static int hecubafb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	struct hecuba_board *board;
	int retval = -ENOMEM;
	int videomemorysize;
	unsigned char *videomemory;
	struct hecubafb_par *par;

	 
	board = dev->dev.platform_data;
	if (!board)
		return -EINVAL;

	 
	if (!try_module_get(board->owner))
		return -ENODEV;

	videomemorysize = (DPY_W*DPY_H)/8;

	videomemory = vzalloc(videomemorysize);
	if (!videomemory)
		goto err_videomem_alloc;

	info = framebuffer_alloc(sizeof(struct hecubafb_par), &dev->dev);
	if (!info)
		goto err_fballoc;

	info->screen_buffer = videomemory;
	info->fbops = &hecubafb_ops;

	info->var = hecubafb_var;
	info->fix = hecubafb_fix;
	info->fix.smem_len = videomemorysize;
	par = info->par;
	par->info = info;
	par->board = board;
	par->send_command = apollo_send_command;
	par->send_data = apollo_send_data;

	info->flags = FBINFO_VIRTFB;

	info->fbdefio = &hecubafb_defio;
	fb_deferred_io_init(info);

	retval = register_framebuffer(info);
	if (retval < 0)
		goto err_fbreg;
	platform_set_drvdata(dev, info);

	fb_info(info, "Hecuba frame buffer device, using %dK of video memory\n",
		videomemorysize >> 10);

	 
	retval = par->board->init(par);
	if (retval < 0)
		goto err_fbreg;

	return 0;
err_fbreg:
	framebuffer_release(info);
err_fballoc:
	vfree(videomemory);
err_videomem_alloc:
	module_put(board->owner);
	return retval;
}

static void hecubafb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		struct hecubafb_par *par = info->par;
		fb_deferred_io_cleanup(info);
		unregister_framebuffer(info);
		vfree(info->screen_buffer);
		if (par->board->remove)
			par->board->remove(par);
		module_put(par->board->owner);
		framebuffer_release(info);
	}
}

static struct platform_driver hecubafb_driver = {
	.probe	= hecubafb_probe,
	.remove_new = hecubafb_remove,
	.driver	= {
		.name	= "hecubafb",
	},
};
module_platform_driver(hecubafb_driver);

MODULE_DESCRIPTION("fbdev driver for Hecuba/Apollo controller");
MODULE_AUTHOR("Jaya Kumar");
MODULE_LICENSE("GPL");
