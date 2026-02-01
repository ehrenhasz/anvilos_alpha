
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/unaligned.h>
#include <video/udlfb.h>
#include "edid.h"

#define OUT_EP_NUM	1	 

static const struct fb_fix_screeninfo dlfb_fix = {
	.id =           "udlfb",
	.type =         FB_TYPE_PACKED_PIXELS,
	.visual =       FB_VISUAL_TRUECOLOR,
	.xpanstep =     0,
	.ypanstep =     0,
	.ywrapstep =    0,
	.accel =        FB_ACCEL_NONE,
};

static const u32 udlfb_info_flags = FBINFO_READS_FAST |
		FBINFO_VIRTFB |
		FBINFO_HWACCEL_IMAGEBLIT | FBINFO_HWACCEL_FILLRECT |
		FBINFO_HWACCEL_COPYAREA | FBINFO_MISC_ALWAYS_SETPAR;

 
static const struct usb_device_id id_table[] = {
	{.idVendor = 0x17e9,
	 .bInterfaceClass = 0xff,
	 .bInterfaceSubClass = 0x00,
	 .bInterfaceProtocol = 0x00,
	 .match_flags = USB_DEVICE_ID_MATCH_VENDOR |
		USB_DEVICE_ID_MATCH_INT_CLASS |
		USB_DEVICE_ID_MATCH_INT_SUBCLASS |
		USB_DEVICE_ID_MATCH_INT_PROTOCOL,
	},
	{},
};
MODULE_DEVICE_TABLE(usb, id_table);

 
static bool console = true;  
static bool fb_defio = true;   
static bool shadow = true;  
static int pixel_limit;  

struct dlfb_deferred_free {
	struct list_head list;
	void *mem;
};

static int dlfb_realloc_framebuffer(struct dlfb_data *dlfb, struct fb_info *info, u32 new_len);

 
static void dlfb_urb_completion(struct urb *urb);
static struct urb *dlfb_get_urb(struct dlfb_data *dlfb);
static int dlfb_submit_urb(struct dlfb_data *dlfb, struct urb * urb, size_t len);
static int dlfb_alloc_urb_list(struct dlfb_data *dlfb, int count, size_t size);
static void dlfb_free_urb_list(struct dlfb_data *dlfb);

 
static char *dlfb_set_register(char *buf, u8 reg, u8 val)
{
	*buf++ = 0xAF;
	*buf++ = 0x20;
	*buf++ = reg;
	*buf++ = val;
	return buf;
}

static char *dlfb_vidreg_lock(char *buf)
{
	return dlfb_set_register(buf, 0xFF, 0x00);
}

static char *dlfb_vidreg_unlock(char *buf)
{
	return dlfb_set_register(buf, 0xFF, 0xFF);
}

 
static char *dlfb_blanking(char *buf, int fb_blank)
{
	u8 reg;

	switch (fb_blank) {
	case FB_BLANK_POWERDOWN:
		reg = 0x07;
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		reg = 0x05;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		reg = 0x03;
		break;
	case FB_BLANK_NORMAL:
		reg = 0x01;
		break;
	default:
		reg = 0x00;
	}

	buf = dlfb_set_register(buf, 0x1F, reg);

	return buf;
}

static char *dlfb_set_color_depth(char *buf, u8 selection)
{
	return dlfb_set_register(buf, 0x00, selection);
}

static char *dlfb_set_base16bpp(char *wrptr, u32 base)
{
	 
	wrptr = dlfb_set_register(wrptr, 0x20, base >> 16);
	wrptr = dlfb_set_register(wrptr, 0x21, base >> 8);
	return dlfb_set_register(wrptr, 0x22, base);
}

 
static char *dlfb_set_base8bpp(char *wrptr, u32 base)
{
	wrptr = dlfb_set_register(wrptr, 0x26, base >> 16);
	wrptr = dlfb_set_register(wrptr, 0x27, base >> 8);
	return dlfb_set_register(wrptr, 0x28, base);
}

static char *dlfb_set_register_16(char *wrptr, u8 reg, u16 value)
{
	wrptr = dlfb_set_register(wrptr, reg, value >> 8);
	return dlfb_set_register(wrptr, reg+1, value);
}

 
static char *dlfb_set_register_16be(char *wrptr, u8 reg, u16 value)
{
	wrptr = dlfb_set_register(wrptr, reg, value);
	return dlfb_set_register(wrptr, reg+1, value >> 8);
}

 
static u16 dlfb_lfsr16(u16 actual_count)
{
	u32 lv = 0xFFFF;  

	while (actual_count--) {
		lv =	 ((lv << 1) |
			(((lv >> 15) ^ (lv >> 4) ^ (lv >> 2) ^ (lv >> 1)) & 1))
			& 0xFFFF;
	}

	return (u16) lv;
}

 
static char *dlfb_set_register_lfsr16(char *wrptr, u8 reg, u16 value)
{
	return dlfb_set_register_16(wrptr, reg, dlfb_lfsr16(value));
}

 
static char *dlfb_set_vid_cmds(char *wrptr, struct fb_var_screeninfo *var)
{
	u16 xds, yds;
	u16 xde, yde;
	u16 yec;

	 
	xds = var->left_margin + var->hsync_len;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x01, xds);
	 
	xde = xds + var->xres;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x03, xde);

	 
	yds = var->upper_margin + var->vsync_len;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x05, yds);
	 
	yde = yds + var->yres;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x07, yde);

	 
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x09,
			xde + var->right_margin - 1);

	 
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x0B, 1);

	 
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x0D, var->hsync_len + 1);

	 
	wrptr = dlfb_set_register_16(wrptr, 0x0F, var->xres);

	 
	yec = var->yres + var->upper_margin + var->lower_margin +
			var->vsync_len;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x11, yec);

	 
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x13, 0);

	 
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x15, var->vsync_len);

	 
	wrptr = dlfb_set_register_16(wrptr, 0x17, var->yres);

	 
	wrptr = dlfb_set_register_16be(wrptr, 0x1B,
			200*1000*1000/var->pixclock);

	return wrptr;
}

 
static int dlfb_set_video_mode(struct dlfb_data *dlfb,
				struct fb_var_screeninfo *var)
{
	char *buf;
	char *wrptr;
	int retval;
	int writesize;
	struct urb *urb;

	if (!atomic_read(&dlfb->usb_active))
		return -EPERM;

	urb = dlfb_get_urb(dlfb);
	if (!urb)
		return -ENOMEM;

	buf = (char *) urb->transfer_buffer;

	 
	wrptr = dlfb_vidreg_lock(buf);
	wrptr = dlfb_set_color_depth(wrptr, 0x00);
	 
	wrptr = dlfb_set_base16bpp(wrptr, 0);
	 
	wrptr = dlfb_set_base8bpp(wrptr, dlfb->info->fix.smem_len);

	wrptr = dlfb_set_vid_cmds(wrptr, var);
	wrptr = dlfb_blanking(wrptr, FB_BLANK_UNBLANK);
	wrptr = dlfb_vidreg_unlock(wrptr);

	writesize = wrptr - buf;

	retval = dlfb_submit_urb(dlfb, urb, writesize);

	dlfb->blank_mode = FB_BLANK_UNBLANK;

	return retval;
}

static int dlfb_ops_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;

	if (info->fbdefio)
		return fb_deferred_io_mmap(info, vma);

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	if (size > info->fix.smem_len)
		return -EINVAL;
	if (offset > info->fix.smem_len - size)
		return -EINVAL;

	pos = (unsigned long)info->fix.smem_start + offset;

	dev_dbg(info->dev, "mmap() framebuffer addr:%lu size:%lu\n",
		pos, size);

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	return 0;
}

 
static int dlfb_trim_hline(const u8 *bback, const u8 **bfront, int *width_bytes)
{
	int j, k;
	const unsigned long *back = (const unsigned long *) bback;
	const unsigned long *front = (const unsigned long *) *bfront;
	const int width = *width_bytes / sizeof(unsigned long);
	int identical;
	int start = width;
	int end = width;

	for (j = 0; j < width; j++) {
		if (back[j] != front[j]) {
			start = j;
			break;
		}
	}

	for (k = width - 1; k > j; k--) {
		if (back[k] != front[k]) {
			end = k+1;
			break;
		}
	}

	identical = start + (width - end);
	*bfront = (u8 *) &front[start];
	*width_bytes = (end - start) * sizeof(unsigned long);

	return identical * sizeof(unsigned long);
}

 
static void dlfb_compress_hline(
	const uint16_t **pixel_start_ptr,
	const uint16_t *const pixel_end,
	uint32_t *device_address_ptr,
	uint8_t **command_buffer_ptr,
	const uint8_t *const cmd_buffer_end,
	unsigned long back_buffer_offset,
	int *ident_ptr)
{
	const uint16_t *pixel = *pixel_start_ptr;
	uint32_t dev_addr  = *device_address_ptr;
	uint8_t *cmd = *command_buffer_ptr;

	while ((pixel_end > pixel) &&
	       (cmd_buffer_end - MIN_RLX_CMD_BYTES > cmd)) {
		uint8_t *raw_pixels_count_byte = NULL;
		uint8_t *cmd_pixels_count_byte = NULL;
		const uint16_t *raw_pixel_start = NULL;
		const uint16_t *cmd_pixel_start, *cmd_pixel_end = NULL;

		if (back_buffer_offset &&
		    *pixel == *(u16 *)((u8 *)pixel + back_buffer_offset)) {
			pixel++;
			dev_addr += BPP;
			(*ident_ptr)++;
			continue;
		}

		*cmd++ = 0xAF;
		*cmd++ = 0x6B;
		*cmd++ = dev_addr >> 16;
		*cmd++ = dev_addr >> 8;
		*cmd++ = dev_addr;

		cmd_pixels_count_byte = cmd++;  
		cmd_pixel_start = pixel;

		raw_pixels_count_byte = cmd++;  
		raw_pixel_start = pixel;

		cmd_pixel_end = pixel + min3(MAX_CMD_PIXELS + 1UL,
					(unsigned long)(pixel_end - pixel),
					(unsigned long)(cmd_buffer_end - 1 - cmd) / BPP);

		if (back_buffer_offset) {
			 
			while (cmd_pixel_end - 1 > pixel &&
			       *(cmd_pixel_end - 1) == *(u16 *)((u8 *)(cmd_pixel_end - 1) + back_buffer_offset))
				cmd_pixel_end--;
		}

		while (pixel < cmd_pixel_end) {
			const uint16_t * const repeating_pixel = pixel;
			u16 pixel_value = *pixel;

			put_unaligned_be16(pixel_value, cmd);
			if (back_buffer_offset)
				*(u16 *)((u8 *)pixel + back_buffer_offset) = pixel_value;
			cmd += 2;
			pixel++;

			if (unlikely((pixel < cmd_pixel_end) &&
				     (*pixel == pixel_value))) {
				 
				*raw_pixels_count_byte = ((repeating_pixel -
						raw_pixel_start) + 1) & 0xFF;

				do {
					if (back_buffer_offset)
						*(u16 *)((u8 *)pixel + back_buffer_offset) = pixel_value;
					pixel++;
				} while ((pixel < cmd_pixel_end) &&
					 (*pixel == pixel_value));

				 
				*cmd++ = ((pixel - repeating_pixel) - 1) & 0xFF;

				 
				raw_pixel_start = pixel;
				raw_pixels_count_byte = cmd++;
			}
		}

		if (pixel > raw_pixel_start) {
			 
			*raw_pixels_count_byte = (pixel-raw_pixel_start) & 0xFF;
		} else {
			 
			cmd--;
		}

		*cmd_pixels_count_byte = (pixel - cmd_pixel_start) & 0xFF;
		dev_addr += (u8 *)pixel - (u8 *)cmd_pixel_start;
	}

	if (cmd_buffer_end - MIN_RLX_CMD_BYTES <= cmd) {
		 
		if (cmd_buffer_end > cmd)
			memset(cmd, 0xAF, cmd_buffer_end - cmd);
		cmd = (uint8_t *) cmd_buffer_end;
	}

	*command_buffer_ptr = cmd;
	*pixel_start_ptr = pixel;
	*device_address_ptr = dev_addr;
}

 
static int dlfb_render_hline(struct dlfb_data *dlfb, struct urb **urb_ptr,
			      const char *front, char **urb_buf_ptr,
			      u32 byte_offset, u32 byte_width,
			      int *ident_ptr, int *sent_ptr)
{
	const u8 *line_start, *line_end, *next_pixel;
	u32 dev_addr = dlfb->base16 + byte_offset;
	struct urb *urb = *urb_ptr;
	u8 *cmd = *urb_buf_ptr;
	u8 *cmd_end = (u8 *) urb->transfer_buffer + urb->transfer_buffer_length;
	unsigned long back_buffer_offset = 0;

	line_start = (u8 *) (front + byte_offset);
	next_pixel = line_start;
	line_end = next_pixel + byte_width;

	if (dlfb->backing_buffer) {
		int offset;
		const u8 *back_start = (u8 *) (dlfb->backing_buffer
						+ byte_offset);

		back_buffer_offset = (unsigned long)back_start - (unsigned long)line_start;

		*ident_ptr += dlfb_trim_hline(back_start, &next_pixel,
			&byte_width);

		offset = next_pixel - line_start;
		line_end = next_pixel + byte_width;
		dev_addr += offset;
		back_start += offset;
		line_start += offset;
	}

	while (next_pixel < line_end) {

		dlfb_compress_hline((const uint16_t **) &next_pixel,
			     (const uint16_t *) line_end, &dev_addr,
			(u8 **) &cmd, (u8 *) cmd_end, back_buffer_offset,
			ident_ptr);

		if (cmd >= cmd_end) {
			int len = cmd - (u8 *) urb->transfer_buffer;
			if (dlfb_submit_urb(dlfb, urb, len))
				return 1;  
			*sent_ptr += len;
			urb = dlfb_get_urb(dlfb);
			if (!urb)
				return 1;  
			*urb_ptr = urb;
			cmd = urb->transfer_buffer;
			cmd_end = &cmd[urb->transfer_buffer_length];
		}
	}

	*urb_buf_ptr = cmd;

	return 0;
}

static int dlfb_handle_damage(struct dlfb_data *dlfb, int x, int y, int width, int height)
{
	int i, ret;
	char *cmd;
	cycles_t start_cycles, end_cycles;
	int bytes_sent = 0;
	int bytes_identical = 0;
	struct urb *urb;
	int aligned_x;

	start_cycles = get_cycles();

	mutex_lock(&dlfb->render_mutex);

	aligned_x = DL_ALIGN_DOWN(x, sizeof(unsigned long));
	width = DL_ALIGN_UP(width + (x-aligned_x), sizeof(unsigned long));
	x = aligned_x;

	if ((width <= 0) ||
	    (x + width > dlfb->info->var.xres) ||
	    (y + height > dlfb->info->var.yres)) {
		ret = -EINVAL;
		goto unlock_ret;
	}

	if (!atomic_read(&dlfb->usb_active)) {
		ret = 0;
		goto unlock_ret;
	}

	urb = dlfb_get_urb(dlfb);
	if (!urb) {
		ret = 0;
		goto unlock_ret;
	}
	cmd = urb->transfer_buffer;

	for (i = y; i < y + height ; i++) {
		const int line_offset = dlfb->info->fix.line_length * i;
		const int byte_offset = line_offset + (x * BPP);

		if (dlfb_render_hline(dlfb, &urb,
				      (char *) dlfb->info->fix.smem_start,
				      &cmd, byte_offset, width * BPP,
				      &bytes_identical, &bytes_sent))
			goto error;
	}

	if (cmd > (char *) urb->transfer_buffer) {
		int len;
		if (cmd < (char *) urb->transfer_buffer + urb->transfer_buffer_length)
			*cmd++ = 0xAF;
		 
		len = cmd - (char *) urb->transfer_buffer;
		dlfb_submit_urb(dlfb, urb, len);
		bytes_sent += len;
	} else
		dlfb_urb_completion(urb);

error:
	atomic_add(bytes_sent, &dlfb->bytes_sent);
	atomic_add(bytes_identical, &dlfb->bytes_identical);
	atomic_add(width*height*2, &dlfb->bytes_rendered);
	end_cycles = get_cycles();
	atomic_add(((unsigned int) ((end_cycles - start_cycles)
		    >> 10)),  
		   &dlfb->cpu_kcycles_used);

	ret = 0;

unlock_ret:
	mutex_unlock(&dlfb->render_mutex);
	return ret;
}

static void dlfb_init_damage(struct dlfb_data *dlfb)
{
	dlfb->damage_x = INT_MAX;
	dlfb->damage_x2 = 0;
	dlfb->damage_y = INT_MAX;
	dlfb->damage_y2 = 0;
}

static void dlfb_damage_work(struct work_struct *w)
{
	struct dlfb_data *dlfb = container_of(w, struct dlfb_data, damage_work);
	int x, x2, y, y2;

	spin_lock_irq(&dlfb->damage_lock);
	x = dlfb->damage_x;
	x2 = dlfb->damage_x2;
	y = dlfb->damage_y;
	y2 = dlfb->damage_y2;
	dlfb_init_damage(dlfb);
	spin_unlock_irq(&dlfb->damage_lock);

	if (x < x2 && y < y2)
		dlfb_handle_damage(dlfb, x, y, x2 - x, y2 - y);
}

static void dlfb_offload_damage(struct dlfb_data *dlfb, int x, int y, int width, int height)
{
	unsigned long flags;
	int x2 = x + width;
	int y2 = y + height;

	if (x >= x2 || y >= y2)
		return;

	spin_lock_irqsave(&dlfb->damage_lock, flags);
	dlfb->damage_x = min(x, dlfb->damage_x);
	dlfb->damage_x2 = max(x2, dlfb->damage_x2);
	dlfb->damage_y = min(y, dlfb->damage_y);
	dlfb->damage_y2 = max(y2, dlfb->damage_y2);
	spin_unlock_irqrestore(&dlfb->damage_lock, flags);

	schedule_work(&dlfb->damage_work);
}

 
static ssize_t dlfb_ops_write(struct fb_info *info, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	ssize_t result;
	struct dlfb_data *dlfb = info->par;
	u32 offset = (u32) *ppos;

	result = fb_sys_write(info, buf, count, ppos);

	if (result > 0) {
		int start = max((int)(offset / info->fix.line_length), 0);
		int lines = min((u32)((result / info->fix.line_length) + 1),
				(u32)info->var.yres);

		dlfb_handle_damage(dlfb, 0, start, info->var.xres,
			lines);
	}

	return result;
}

 
static void dlfb_ops_copyarea(struct fb_info *info,
				const struct fb_copyarea *area)
{

	struct dlfb_data *dlfb = info->par;

	sys_copyarea(info, area);

	dlfb_offload_damage(dlfb, area->dx, area->dy,
			area->width, area->height);
}

static void dlfb_ops_imageblit(struct fb_info *info,
				const struct fb_image *image)
{
	struct dlfb_data *dlfb = info->par;

	sys_imageblit(info, image);

	dlfb_offload_damage(dlfb, image->dx, image->dy,
			image->width, image->height);
}

static void dlfb_ops_fillrect(struct fb_info *info,
			  const struct fb_fillrect *rect)
{
	struct dlfb_data *dlfb = info->par;

	sys_fillrect(info, rect);

	dlfb_offload_damage(dlfb, rect->dx, rect->dy, rect->width,
			      rect->height);
}

 
static void dlfb_dpy_deferred_io(struct fb_info *info, struct list_head *pagereflist)
{
	struct fb_deferred_io_pageref *pageref;
	struct dlfb_data *dlfb = info->par;
	struct urb *urb;
	char *cmd;
	cycles_t start_cycles, end_cycles;
	int bytes_sent = 0;
	int bytes_identical = 0;
	int bytes_rendered = 0;

	mutex_lock(&dlfb->render_mutex);

	if (!fb_defio)
		goto unlock_ret;

	if (!atomic_read(&dlfb->usb_active))
		goto unlock_ret;

	start_cycles = get_cycles();

	urb = dlfb_get_urb(dlfb);
	if (!urb)
		goto unlock_ret;

	cmd = urb->transfer_buffer;

	 
	list_for_each_entry(pageref, pagereflist, list) {
		if (dlfb_render_hline(dlfb, &urb, (char *) info->fix.smem_start,
				      &cmd, pageref->offset, PAGE_SIZE,
				      &bytes_identical, &bytes_sent))
			goto error;
		bytes_rendered += PAGE_SIZE;
	}

	if (cmd > (char *) urb->transfer_buffer) {
		int len;
		if (cmd < (char *) urb->transfer_buffer + urb->transfer_buffer_length)
			*cmd++ = 0xAF;
		 
		len = cmd - (char *) urb->transfer_buffer;
		dlfb_submit_urb(dlfb, urb, len);
		bytes_sent += len;
	} else
		dlfb_urb_completion(urb);

error:
	atomic_add(bytes_sent, &dlfb->bytes_sent);
	atomic_add(bytes_identical, &dlfb->bytes_identical);
	atomic_add(bytes_rendered, &dlfb->bytes_rendered);
	end_cycles = get_cycles();
	atomic_add(((unsigned int) ((end_cycles - start_cycles)
		    >> 10)),  
		   &dlfb->cpu_kcycles_used);
unlock_ret:
	mutex_unlock(&dlfb->render_mutex);
}

static int dlfb_get_edid(struct dlfb_data *dlfb, char *edid, int len)
{
	int i, ret;
	char *rbuf;

	rbuf = kmalloc(2, GFP_KERNEL);
	if (!rbuf)
		return 0;

	for (i = 0; i < len; i++) {
		ret = usb_control_msg(dlfb->udev,
				      usb_rcvctrlpipe(dlfb->udev, 0), 0x02,
				      (0x80 | (0x02 << 5)), i << 8, 0xA1,
				      rbuf, 2, USB_CTRL_GET_TIMEOUT);
		if (ret < 2) {
			dev_err(&dlfb->udev->dev,
				"Read EDID byte %d failed: %d\n", i, ret);
			i--;
			break;
		}
		edid[i] = rbuf[1];
	}

	kfree(rbuf);

	return i;
}

static int dlfb_ops_ioctl(struct fb_info *info, unsigned int cmd,
				unsigned long arg)
{

	struct dlfb_data *dlfb = info->par;

	if (!atomic_read(&dlfb->usb_active))
		return 0;

	 
	if (cmd == DLFB_IOCTL_RETURN_EDID) {
		void __user *edid = (void __user *)arg;
		if (copy_to_user(edid, dlfb->edid, dlfb->edid_size))
			return -EFAULT;
		return 0;
	}

	 
	if (cmd == DLFB_IOCTL_REPORT_DAMAGE) {
		struct dloarea area;

		if (copy_from_user(&area, (void __user *)arg,
				  sizeof(struct dloarea)))
			return -EFAULT;

		 
		if (info->fbdefio)
			info->fbdefio->delay = DL_DEFIO_WRITE_DISABLE;

		if (area.x < 0)
			area.x = 0;

		if (area.x > info->var.xres)
			area.x = info->var.xres;

		if (area.y < 0)
			area.y = 0;

		if (area.y > info->var.yres)
			area.y = info->var.yres;

		dlfb_handle_damage(dlfb, area.x, area.y, area.w, area.h);
	}

	return 0;
}

 
static int
dlfb_ops_setcolreg(unsigned regno, unsigned red, unsigned green,
	       unsigned blue, unsigned transp, struct fb_info *info)
{
	int err = 0;

	if (regno >= info->cmap.len)
		return 1;

	if (regno < 16) {
		if (info->var.red.offset == 10) {
			 
			((u32 *) (info->pseudo_palette))[regno] =
			    ((red & 0xf800) >> 1) |
			    ((green & 0xf800) >> 6) | ((blue & 0xf800) >> 11);
		} else {
			 
			((u32 *) (info->pseudo_palette))[regno] =
			    ((red & 0xf800)) |
			    ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
		}
	}

	return err;
}

 
static int dlfb_ops_open(struct fb_info *info, int user)
{
	struct dlfb_data *dlfb = info->par;

	 
	if ((user == 0) && (!console))
		return -EBUSY;

	 
	if (dlfb->virtualized)
		return -ENODEV;

	dlfb->fb_count++;

	if (fb_defio && (info->fbdefio == NULL)) {
		 

		struct fb_deferred_io *fbdefio;

		fbdefio = kzalloc(sizeof(struct fb_deferred_io), GFP_KERNEL);

		if (fbdefio) {
			fbdefio->delay = DL_DEFIO_WRITE_DELAY;
			fbdefio->sort_pagereflist = true;
			fbdefio->deferred_io = dlfb_dpy_deferred_io;
		}

		info->fbdefio = fbdefio;
		fb_deferred_io_init(info);
	}

	dev_dbg(info->dev, "open, user=%d fb_info=%p count=%d\n",
		user, info, dlfb->fb_count);

	return 0;
}

static void dlfb_ops_destroy(struct fb_info *info)
{
	struct dlfb_data *dlfb = info->par;

	cancel_work_sync(&dlfb->damage_work);

	mutex_destroy(&dlfb->render_mutex);

	if (info->cmap.len != 0)
		fb_dealloc_cmap(&info->cmap);
	if (info->monspecs.modedb)
		fb_destroy_modedb(info->monspecs.modedb);
	vfree(info->screen_buffer);

	fb_destroy_modelist(&info->modelist);

	while (!list_empty(&dlfb->deferred_free)) {
		struct dlfb_deferred_free *d = list_entry(dlfb->deferred_free.next, struct dlfb_deferred_free, list);
		list_del(&d->list);
		vfree(d->mem);
		kfree(d);
	}
	vfree(dlfb->backing_buffer);
	kfree(dlfb->edid);
	dlfb_free_urb_list(dlfb);
	usb_put_dev(dlfb->udev);
	kfree(dlfb);

	 
	framebuffer_release(info);
}

 
static int dlfb_ops_release(struct fb_info *info, int user)
{
	struct dlfb_data *dlfb = info->par;

	dlfb->fb_count--;

	if ((dlfb->fb_count == 0) && (info->fbdefio)) {
		fb_deferred_io_cleanup(info);
		kfree(info->fbdefio);
		info->fbdefio = NULL;
	}

	dev_dbg(info->dev, "release, user=%d count=%d\n", user, dlfb->fb_count);

	return 0;
}

 
static int dlfb_is_valid_mode(struct fb_videomode *mode, struct dlfb_data *dlfb)
{
	if (mode->xres * mode->yres > dlfb->sku_pixel_limit)
		return 0;

	return 1;
}

static void dlfb_var_color_format(struct fb_var_screeninfo *var)
{
	const struct fb_bitfield red = { 11, 5, 0 };
	const struct fb_bitfield green = { 5, 6, 0 };
	const struct fb_bitfield blue = { 0, 5, 0 };

	var->bits_per_pixel = 16;
	var->red = red;
	var->green = green;
	var->blue = blue;
}

static int dlfb_ops_check_var(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct fb_videomode mode;
	struct dlfb_data *dlfb = info->par;

	 
	dlfb_var_color_format(var);

	fb_var_to_videomode(&mode, var);

	if (!dlfb_is_valid_mode(&mode, dlfb))
		return -EINVAL;

	return 0;
}

static int dlfb_ops_set_par(struct fb_info *info)
{
	struct dlfb_data *dlfb = info->par;
	int result;
	u16 *pix_framebuffer;
	int i;
	struct fb_var_screeninfo fvs;
	u32 line_length = info->var.xres * (info->var.bits_per_pixel / 8);

	 
	fvs = info->var;
	fvs.activate = 0;
	fvs.vmode &= ~FB_VMODE_SMOOTH_XPAN;

	if (!memcmp(&dlfb->current_mode, &fvs, sizeof(struct fb_var_screeninfo)))
		return 0;

	result = dlfb_realloc_framebuffer(dlfb, info, info->var.yres * line_length);
	if (result)
		return result;

	result = dlfb_set_video_mode(dlfb, &info->var);

	if (result)
		return result;

	dlfb->current_mode = fvs;
	info->fix.line_length = line_length;

	if (dlfb->fb_count == 0) {

		 

		pix_framebuffer = (u16 *)info->screen_buffer;
		for (i = 0; i < info->fix.smem_len / 2; i++)
			pix_framebuffer[i] = 0x37e6;
	}

	dlfb_handle_damage(dlfb, 0, 0, info->var.xres, info->var.yres);

	return 0;
}

 
static char *dlfb_dummy_render(char *buf)
{
	*buf++ = 0xAF;
	*buf++ = 0x6A;  
	*buf++ = 0x00;  
	*buf++ = 0x00;
	*buf++ = 0x00;
	*buf++ = 0x01;  
	*buf++ = 0x00;  
	*buf++ = 0x00;
	*buf++ = 0x00;
	return buf;
}

 
static int dlfb_ops_blank(int blank_mode, struct fb_info *info)
{
	struct dlfb_data *dlfb = info->par;
	char *bufptr;
	struct urb *urb;

	dev_dbg(info->dev, "blank, mode %d --> %d\n",
		dlfb->blank_mode, blank_mode);

	if ((dlfb->blank_mode == FB_BLANK_POWERDOWN) &&
	    (blank_mode != FB_BLANK_POWERDOWN)) {

		 
		dlfb_set_video_mode(dlfb, &info->var);
	}

	urb = dlfb_get_urb(dlfb);
	if (!urb)
		return 0;

	bufptr = (char *) urb->transfer_buffer;
	bufptr = dlfb_vidreg_lock(bufptr);
	bufptr = dlfb_blanking(bufptr, blank_mode);
	bufptr = dlfb_vidreg_unlock(bufptr);

	 
	bufptr = dlfb_dummy_render(bufptr);

	dlfb_submit_urb(dlfb, urb, bufptr -
			(char *) urb->transfer_buffer);

	dlfb->blank_mode = blank_mode;

	return 0;
}

static const struct fb_ops dlfb_ops = {
	.owner = THIS_MODULE,
	.fb_read = fb_sys_read,
	.fb_write = dlfb_ops_write,
	.fb_setcolreg = dlfb_ops_setcolreg,
	.fb_fillrect = dlfb_ops_fillrect,
	.fb_copyarea = dlfb_ops_copyarea,
	.fb_imageblit = dlfb_ops_imageblit,
	.fb_mmap = dlfb_ops_mmap,
	.fb_ioctl = dlfb_ops_ioctl,
	.fb_open = dlfb_ops_open,
	.fb_release = dlfb_ops_release,
	.fb_blank = dlfb_ops_blank,
	.fb_check_var = dlfb_ops_check_var,
	.fb_set_par = dlfb_ops_set_par,
	.fb_destroy = dlfb_ops_destroy,
};


static void dlfb_deferred_vfree(struct dlfb_data *dlfb, void *mem)
{
	struct dlfb_deferred_free *d = kmalloc(sizeof(struct dlfb_deferred_free), GFP_KERNEL);
	if (!d)
		return;
	d->mem = mem;
	list_add(&d->list, &dlfb->deferred_free);
}

 
static int dlfb_realloc_framebuffer(struct dlfb_data *dlfb, struct fb_info *info, u32 new_len)
{
	u32 old_len = info->fix.smem_len;
	const void *old_fb = info->screen_buffer;
	unsigned char *new_fb;
	unsigned char *new_back = NULL;

	new_len = PAGE_ALIGN(new_len);

	if (new_len > old_len) {
		 
		new_fb = vmalloc(new_len);
		if (!new_fb) {
			dev_err(info->dev, "Virtual framebuffer alloc failed\n");
			return -ENOMEM;
		}
		memset(new_fb, 0xff, new_len);

		if (info->screen_buffer) {
			memcpy(new_fb, old_fb, old_len);
			dlfb_deferred_vfree(dlfb, info->screen_buffer);
		}

		info->screen_buffer = new_fb;
		info->fix.smem_len = new_len;
		info->fix.smem_start = (unsigned long) new_fb;
		info->flags = udlfb_info_flags;

		 
		if (shadow)
			new_back = vzalloc(new_len);
		if (!new_back)
			dev_info(info->dev,
				 "No shadow/backing buffer allocated\n");
		else {
			dlfb_deferred_vfree(dlfb, dlfb->backing_buffer);
			dlfb->backing_buffer = new_back;
		}
	}
	return 0;
}

 
static int dlfb_setup_modes(struct dlfb_data *dlfb,
			   struct fb_info *info,
			   char *default_edid, size_t default_edid_size)
{
	char *edid;
	int i, result = 0, tries = 3;
	struct device *dev = info->device;
	struct fb_videomode *mode;
	const struct fb_videomode *default_vmode = NULL;

	if (info->dev) {
		 
		mutex_lock(&info->lock);
		 
		dev = info->dev;
	}

	edid = kmalloc(EDID_LENGTH, GFP_KERNEL);
	if (!edid) {
		result = -ENOMEM;
		goto error;
	}

	fb_destroy_modelist(&info->modelist);
	memset(&info->monspecs, 0, sizeof(info->monspecs));

	 
	while (tries--) {

		i = dlfb_get_edid(dlfb, edid, EDID_LENGTH);

		if (i >= EDID_LENGTH)
			fb_edid_to_monspecs(edid, &info->monspecs);

		if (info->monspecs.modedb_len > 0) {
			dlfb->edid = edid;
			dlfb->edid_size = i;
			break;
		}
	}

	 
	if (info->monspecs.modedb_len == 0) {
		dev_err(dev, "Unable to get valid EDID from device/display\n");

		if (dlfb->edid) {
			fb_edid_to_monspecs(dlfb->edid, &info->monspecs);
			if (info->monspecs.modedb_len > 0)
				dev_err(dev, "Using previously queried EDID\n");
		}
	}

	 
	if (info->monspecs.modedb_len == 0) {
		if (default_edid_size >= EDID_LENGTH) {
			fb_edid_to_monspecs(default_edid, &info->monspecs);
			if (info->monspecs.modedb_len > 0) {
				memcpy(edid, default_edid, default_edid_size);
				dlfb->edid = edid;
				dlfb->edid_size = default_edid_size;
				dev_err(dev, "Using default/backup EDID\n");
			}
		}
	}

	 
	if (info->monspecs.modedb_len > 0) {

		for (i = 0; i < info->monspecs.modedb_len; i++) {
			mode = &info->monspecs.modedb[i];
			if (dlfb_is_valid_mode(mode, dlfb)) {
				fb_add_videomode(mode, &info->modelist);
			} else {
				dev_dbg(dev, "Specified mode %dx%d too big\n",
					mode->xres, mode->yres);
				if (i == 0)
					 
					info->monspecs.misc
						&= ~FB_MISC_1ST_DETAIL;
			}
		}

		default_vmode = fb_find_best_display(&info->monspecs,
						     &info->modelist);
	}

	 
	if (default_vmode == NULL) {

		struct fb_videomode fb_vmode = {0};

		 
		for (i = 0; i < VESA_MODEDB_SIZE; i++) {
			mode = (struct fb_videomode *)&vesa_modes[i];
			if (dlfb_is_valid_mode(mode, dlfb))
				fb_add_videomode(mode, &info->modelist);
			else
				dev_dbg(dev, "VESA mode %dx%d too big\n",
					mode->xres, mode->yres);
		}

		 
		fb_vmode.xres = 800;
		fb_vmode.yres = 600;
		fb_vmode.refresh = 60;
		default_vmode = fb_find_nearest_mode(&fb_vmode,
						     &info->modelist);
	}

	 
	if ((default_vmode != NULL) && (dlfb->fb_count == 0)) {

		fb_videomode_to_var(&info->var, default_vmode);
		dlfb_var_color_format(&info->var);

		 
		memcpy(&info->fix, &dlfb_fix, sizeof(dlfb_fix));
	} else
		result = -EINVAL;

error:
	if (edid && (dlfb->edid != edid))
		kfree(edid);

	if (info->dev)
		mutex_unlock(&info->lock);

	return result;
}

static ssize_t metrics_bytes_rendered_show(struct device *fbdev,
				   struct device_attribute *a, char *buf) {
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dlfb = fb_info->par;
	return sysfs_emit(buf, "%u\n",
			atomic_read(&dlfb->bytes_rendered));
}

static ssize_t metrics_bytes_identical_show(struct device *fbdev,
				   struct device_attribute *a, char *buf) {
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dlfb = fb_info->par;
	return sysfs_emit(buf, "%u\n",
			atomic_read(&dlfb->bytes_identical));
}

static ssize_t metrics_bytes_sent_show(struct device *fbdev,
				   struct device_attribute *a, char *buf) {
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dlfb = fb_info->par;
	return sysfs_emit(buf, "%u\n",
			atomic_read(&dlfb->bytes_sent));
}

static ssize_t metrics_cpu_kcycles_used_show(struct device *fbdev,
				   struct device_attribute *a, char *buf) {
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dlfb = fb_info->par;
	return sysfs_emit(buf, "%u\n",
			atomic_read(&dlfb->cpu_kcycles_used));
}

static ssize_t edid_show(
			struct file *filp,
			struct kobject *kobj, struct bin_attribute *a,
			 char *buf, loff_t off, size_t count) {
	struct device *fbdev = kobj_to_dev(kobj);
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dlfb = fb_info->par;

	if (dlfb->edid == NULL)
		return 0;

	if ((off >= dlfb->edid_size) || (count > dlfb->edid_size))
		return 0;

	if (off + count > dlfb->edid_size)
		count = dlfb->edid_size - off;

	memcpy(buf, dlfb->edid, count);

	return count;
}

static ssize_t edid_store(
			struct file *filp,
			struct kobject *kobj, struct bin_attribute *a,
			char *src, loff_t src_off, size_t src_size) {
	struct device *fbdev = kobj_to_dev(kobj);
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dlfb = fb_info->par;
	int ret;

	 
	if ((src_size != EDID_LENGTH) || (src_off != 0))
		return -EINVAL;

	ret = dlfb_setup_modes(dlfb, fb_info, src, src_size);
	if (ret)
		return ret;

	if (!dlfb->edid || memcmp(src, dlfb->edid, src_size))
		return -EINVAL;

	ret = dlfb_ops_set_par(fb_info);
	if (ret)
		return ret;

	return src_size;
}

static ssize_t metrics_reset_store(struct device *fbdev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dlfb = fb_info->par;

	atomic_set(&dlfb->bytes_rendered, 0);
	atomic_set(&dlfb->bytes_identical, 0);
	atomic_set(&dlfb->bytes_sent, 0);
	atomic_set(&dlfb->cpu_kcycles_used, 0);

	return count;
}

static const struct bin_attribute edid_attr = {
	.attr.name = "edid",
	.attr.mode = 0666,
	.size = EDID_LENGTH,
	.read = edid_show,
	.write = edid_store
};

static const struct device_attribute fb_device_attrs[] = {
	__ATTR_RO(metrics_bytes_rendered),
	__ATTR_RO(metrics_bytes_identical),
	__ATTR_RO(metrics_bytes_sent),
	__ATTR_RO(metrics_cpu_kcycles_used),
	__ATTR(metrics_reset, S_IWUSR, NULL, metrics_reset_store),
};

 
static int dlfb_select_std_channel(struct dlfb_data *dlfb)
{
	int ret;
	static const u8 set_def_chn[] = {
				0x57, 0xCD, 0xDC, 0xA7,
				0x1C, 0x88, 0x5E, 0x15,
				0x60, 0xFE, 0xC6, 0x97,
				0x16, 0x3D, 0x47, 0xF2  };

	ret = usb_control_msg_send(dlfb->udev, 0, NR_USB_REQUEST_CHANNEL,
			(USB_DIR_OUT | USB_TYPE_VENDOR), 0, 0,
			&set_def_chn, sizeof(set_def_chn), USB_CTRL_SET_TIMEOUT,
			GFP_KERNEL);

	return ret;
}

static int dlfb_parse_vendor_descriptor(struct dlfb_data *dlfb,
					struct usb_interface *intf)
{
	char *desc;
	char *buf;
	char *desc_end;
	int total_len;

	buf = kzalloc(MAX_VENDOR_DESCRIPTOR_SIZE, GFP_KERNEL);
	if (!buf)
		return false;
	desc = buf;

	total_len = usb_get_descriptor(interface_to_usbdev(intf),
					0x5f,  
					0, desc, MAX_VENDOR_DESCRIPTOR_SIZE);

	 
	if (total_len < 0) {
		if (0 == usb_get_extra_descriptor(intf->cur_altsetting,
			0x5f, &desc))
			total_len = (int) desc[0];
	}

	if (total_len > 5) {
		dev_info(&intf->dev,
			 "vendor descriptor length: %d data: %11ph\n",
			 total_len, desc);

		if ((desc[0] != total_len) ||  
		    (desc[1] != 0x5f) ||    
		    (desc[2] != 0x01) ||    
		    (desc[3] != 0x00) ||
		    (desc[4] != total_len - 2))  
			goto unrecognized;

		desc_end = desc + total_len;
		desc += 5;  

		while (desc < desc_end) {
			u8 length;
			u16 key;

			key = *desc++;
			key |= (u16)*desc++ << 8;
			length = *desc++;

			switch (key) {
			case 0x0200: {  
				u32 max_area = *desc++;
				max_area |= (u32)*desc++ << 8;
				max_area |= (u32)*desc++ << 16;
				max_area |= (u32)*desc++ << 24;
				dev_warn(&intf->dev,
					 "DL chip limited to %d pixel modes\n",
					 max_area);
				dlfb->sku_pixel_limit = max_area;
				break;
			}
			default:
				break;
			}
			desc += length;
		}
	} else {
		dev_info(&intf->dev, "vendor descriptor not available (%d)\n",
			 total_len);
	}

	goto success;

unrecognized:
	 
	dev_err(&intf->dev, "Unrecognized vendor firmware descriptor\n");

success:
	kfree(buf);
	return true;
}

static int dlfb_usb_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
	int i;
	const struct device_attribute *attr;
	struct dlfb_data *dlfb;
	struct fb_info *info;
	int retval;
	struct usb_device *usbdev = interface_to_usbdev(intf);
	static u8 out_ep[] = {OUT_EP_NUM + USB_DIR_OUT, 0};

	 
	dlfb = kzalloc(sizeof(*dlfb), GFP_KERNEL);
	if (!dlfb) {
		dev_err(&intf->dev, "%s: failed to allocate dlfb\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&dlfb->deferred_free);

	dlfb->udev = usb_get_dev(usbdev);
	usb_set_intfdata(intf, dlfb);

	if (!usb_check_bulk_endpoints(intf, out_ep)) {
		dev_err(&intf->dev, "Invalid DisplayLink device!\n");
		retval = -EINVAL;
		goto error;
	}

	dev_dbg(&intf->dev, "console enable=%d\n", console);
	dev_dbg(&intf->dev, "fb_defio enable=%d\n", fb_defio);
	dev_dbg(&intf->dev, "shadow enable=%d\n", shadow);

	dlfb->sku_pixel_limit = 2048 * 1152;  

	if (!dlfb_parse_vendor_descriptor(dlfb, intf)) {
		dev_err(&intf->dev,
			"firmware not recognized, incompatible device?\n");
		retval = -ENODEV;
		goto error;
	}

	if (pixel_limit) {
		dev_warn(&intf->dev,
			 "DL chip limit of %d overridden to %d\n",
			 dlfb->sku_pixel_limit, pixel_limit);
		dlfb->sku_pixel_limit = pixel_limit;
	}


	 
	info = framebuffer_alloc(0, &dlfb->udev->dev);
	if (!info) {
		retval = -ENOMEM;
		goto error;
	}

	dlfb->info = info;
	info->par = dlfb;
	info->pseudo_palette = dlfb->pseudo_palette;
	dlfb->ops = dlfb_ops;
	info->fbops = &dlfb->ops;

	mutex_init(&dlfb->render_mutex);
	dlfb_init_damage(dlfb);
	spin_lock_init(&dlfb->damage_lock);
	INIT_WORK(&dlfb->damage_work, dlfb_damage_work);

	INIT_LIST_HEAD(&info->modelist);

	if (!dlfb_alloc_urb_list(dlfb, WRITES_IN_FLIGHT, MAX_TRANSFER)) {
		retval = -ENOMEM;
		dev_err(&intf->dev, "unable to allocate urb list\n");
		goto error;
	}

	 

	retval = fb_alloc_cmap(&info->cmap, 256, 0);
	if (retval < 0) {
		dev_err(info->device, "cmap allocation failed: %d\n", retval);
		goto error;
	}

	retval = dlfb_setup_modes(dlfb, info, NULL, 0);
	if (retval != 0) {
		dev_err(info->device,
			"unable to find common mode for display and adapter\n");
		goto error;
	}

	 

	atomic_set(&dlfb->usb_active, 1);
	dlfb_select_std_channel(dlfb);

	dlfb_ops_check_var(&info->var, info);
	retval = dlfb_ops_set_par(info);
	if (retval)
		goto error;

	retval = register_framebuffer(info);
	if (retval < 0) {
		dev_err(info->device, "unable to register framebuffer: %d\n",
			retval);
		goto error;
	}

	for (i = 0; i < ARRAY_SIZE(fb_device_attrs); i++) {
		attr = &fb_device_attrs[i];
		retval = device_create_file(info->dev, attr);
		if (retval)
			dev_warn(info->device,
				 "failed to create '%s' attribute: %d\n",
				 attr->attr.name, retval);
	}

	retval = device_create_bin_file(info->dev, &edid_attr);
	if (retval)
		dev_warn(info->device, "failed to create '%s' attribute: %d\n",
			 edid_attr.attr.name, retval);

	dev_info(info->device,
		 "%s is DisplayLink USB device (%dx%d, %dK framebuffer memory)\n",
		 dev_name(info->dev), info->var.xres, info->var.yres,
		 ((dlfb->backing_buffer) ?
		 info->fix.smem_len * 2 : info->fix.smem_len) >> 10);
	return 0;

error:
	if (dlfb->info) {
		dlfb_ops_destroy(dlfb->info);
	} else {
		usb_put_dev(dlfb->udev);
		kfree(dlfb);
	}
	return retval;
}

static void dlfb_usb_disconnect(struct usb_interface *intf)
{
	struct dlfb_data *dlfb;
	struct fb_info *info;
	int i;

	dlfb = usb_get_intfdata(intf);
	info = dlfb->info;

	dev_dbg(&intf->dev, "USB disconnect starting\n");

	 
	dlfb->virtualized = true;

	 
	atomic_set(&dlfb->usb_active, 0);

	 
	dlfb_free_urb_list(dlfb);

	 
	for (i = 0; i < ARRAY_SIZE(fb_device_attrs); i++)
		device_remove_file(info->dev, &fb_device_attrs[i]);
	device_remove_bin_file(info->dev, &edid_attr);

	unregister_framebuffer(info);
}

static struct usb_driver dlfb_driver = {
	.name = "udlfb",
	.probe = dlfb_usb_probe,
	.disconnect = dlfb_usb_disconnect,
	.id_table = id_table,
};

module_usb_driver(dlfb_driver);

static void dlfb_urb_completion(struct urb *urb)
{
	struct urb_node *unode = urb->context;
	struct dlfb_data *dlfb = unode->dlfb;
	unsigned long flags;

	switch (urb->status) {
	case 0:
		 
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		 
		break;
	default:
		dev_err(&dlfb->udev->dev,
			"%s - nonzero write bulk status received: %d\n",
			__func__, urb->status);
		atomic_set(&dlfb->lost_pixels, 1);
		break;
	}

	urb->transfer_buffer_length = dlfb->urbs.size;  

	spin_lock_irqsave(&dlfb->urbs.lock, flags);
	list_add_tail(&unode->entry, &dlfb->urbs.list);
	dlfb->urbs.available++;
	spin_unlock_irqrestore(&dlfb->urbs.lock, flags);

	up(&dlfb->urbs.limit_sem);
}

static void dlfb_free_urb_list(struct dlfb_data *dlfb)
{
	int count = dlfb->urbs.count;
	struct list_head *node;
	struct urb_node *unode;
	struct urb *urb;

	 
	while (count--) {
		down(&dlfb->urbs.limit_sem);

		spin_lock_irq(&dlfb->urbs.lock);

		node = dlfb->urbs.list.next;  
		list_del_init(node);

		spin_unlock_irq(&dlfb->urbs.lock);

		unode = list_entry(node, struct urb_node, entry);
		urb = unode->urb;

		 
		usb_free_coherent(urb->dev, dlfb->urbs.size,
				  urb->transfer_buffer, urb->transfer_dma);
		usb_free_urb(urb);
		kfree(node);
	}

	dlfb->urbs.count = 0;
}

static int dlfb_alloc_urb_list(struct dlfb_data *dlfb, int count, size_t size)
{
	struct urb *urb;
	struct urb_node *unode;
	char *buf;
	size_t wanted_size = count * size;

	spin_lock_init(&dlfb->urbs.lock);

retry:
	dlfb->urbs.size = size;
	INIT_LIST_HEAD(&dlfb->urbs.list);

	sema_init(&dlfb->urbs.limit_sem, 0);
	dlfb->urbs.count = 0;
	dlfb->urbs.available = 0;

	while (dlfb->urbs.count * size < wanted_size) {
		unode = kzalloc(sizeof(*unode), GFP_KERNEL);
		if (!unode)
			break;
		unode->dlfb = dlfb;

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			kfree(unode);
			break;
		}
		unode->urb = urb;

		buf = usb_alloc_coherent(dlfb->udev, size, GFP_KERNEL,
					 &urb->transfer_dma);
		if (!buf) {
			kfree(unode);
			usb_free_urb(urb);
			if (size > PAGE_SIZE) {
				size /= 2;
				dlfb_free_urb_list(dlfb);
				goto retry;
			}
			break;
		}

		 
		usb_fill_bulk_urb(urb, dlfb->udev,
			usb_sndbulkpipe(dlfb->udev, OUT_EP_NUM),
			buf, size, dlfb_urb_completion, unode);
		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		list_add_tail(&unode->entry, &dlfb->urbs.list);

		up(&dlfb->urbs.limit_sem);
		dlfb->urbs.count++;
		dlfb->urbs.available++;
	}

	return dlfb->urbs.count;
}

static struct urb *dlfb_get_urb(struct dlfb_data *dlfb)
{
	int ret;
	struct list_head *entry;
	struct urb_node *unode;

	 
	ret = down_timeout(&dlfb->urbs.limit_sem, GET_URB_TIMEOUT);
	if (ret) {
		atomic_set(&dlfb->lost_pixels, 1);
		dev_warn(&dlfb->udev->dev,
			 "wait for urb interrupted: %d available: %d\n",
			 ret, dlfb->urbs.available);
		return NULL;
	}

	spin_lock_irq(&dlfb->urbs.lock);

	BUG_ON(list_empty(&dlfb->urbs.list));  
	entry = dlfb->urbs.list.next;
	list_del_init(entry);
	dlfb->urbs.available--;

	spin_unlock_irq(&dlfb->urbs.lock);

	unode = list_entry(entry, struct urb_node, entry);
	return unode->urb;
}

static int dlfb_submit_urb(struct dlfb_data *dlfb, struct urb *urb, size_t len)
{
	int ret;

	BUG_ON(len > dlfb->urbs.size);

	urb->transfer_buffer_length = len;  
	ret = usb_submit_urb(urb, GFP_KERNEL);
	if (ret) {
		dlfb_urb_completion(urb);  
		atomic_set(&dlfb->lost_pixels, 1);
		dev_err(&dlfb->udev->dev, "submit urb error: %d\n", ret);
	}
	return ret;
}

module_param(console, bool, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP);
MODULE_PARM_DESC(console, "Allow fbcon to open framebuffer");

module_param(fb_defio, bool, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP);
MODULE_PARM_DESC(fb_defio, "Page fault detection of mmap writes");

module_param(shadow, bool, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP);
MODULE_PARM_DESC(shadow, "Shadow vid mem. Disable to save mem but lose perf");

module_param(pixel_limit, int, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP);
MODULE_PARM_DESC(pixel_limit, "Force limit on max mode (in x*y pixels)");

MODULE_AUTHOR("Roberto De Ioris <roberto@unbit.it>, "
	      "Jaya Kumar <jayakumar.lkml@gmail.com>, "
	      "Bernie Thompson <bernie@plugable.com>");
MODULE_DESCRIPTION("DisplayLink kernel framebuffer driver");
MODULE_LICENSE("GPL");

