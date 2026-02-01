

#include <linux/export.h>
#include <linux/fb.h>
#include <linux/mutex.h>
#include <linux/slab.h>

 
struct fb_info *framebuffer_alloc(size_t size, struct device *dev)
{
#define BYTES_PER_LONG (BITS_PER_LONG/8)
#define PADDING (BYTES_PER_LONG - (sizeof(struct fb_info) % BYTES_PER_LONG))
	int fb_info_size = sizeof(struct fb_info);
	struct fb_info *info;
	char *p;

	if (size)
		fb_info_size += PADDING;

	p = kzalloc(fb_info_size + size, GFP_KERNEL);

	if (!p)
		return NULL;

	info = (struct fb_info *) p;

	if (size)
		info->par = p + fb_info_size;

	info->device = dev;
	info->fbcon_rotate_hint = -1;

#if IS_ENABLED(CONFIG_FB_BACKLIGHT)
	mutex_init(&info->bl_curve_mutex);
#endif

	return info;
#undef PADDING
#undef BYTES_PER_LONG
}
EXPORT_SYMBOL(framebuffer_alloc);

 
void framebuffer_release(struct fb_info *info)
{
	if (!info)
		return;

	if (WARN_ON(refcount_read(&info->count)))
		return;

#if IS_ENABLED(CONFIG_FB_BACKLIGHT)
	mutex_destroy(&info->bl_curve_mutex);
#endif

	kfree(info);
}
EXPORT_SYMBOL(framebuffer_release);
