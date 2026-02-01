
 

 

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/platform_data/simplefb.h>
#include <linux/platform_device.h>
#include <linux/screen_info.h>
#include <linux/sysfb.h>

static struct platform_device *pd;
static DEFINE_MUTEX(disable_lock);
static bool disabled;

static bool sysfb_unregister(void)
{
	if (IS_ERR_OR_NULL(pd))
		return false;

	platform_device_unregister(pd);
	pd = NULL;

	return true;
}

 
void sysfb_disable(void)
{
	mutex_lock(&disable_lock);
	sysfb_unregister();
	disabled = true;
	mutex_unlock(&disable_lock);
}
EXPORT_SYMBOL_GPL(sysfb_disable);

static __init int sysfb_init(void)
{
	struct screen_info *si = &screen_info;
	struct simplefb_platform_data mode;
	const char *name;
	bool compatible;
	int ret = 0;

	mutex_lock(&disable_lock);
	if (disabled)
		goto unlock_mutex;

	sysfb_apply_efi_quirks();

	 
	compatible = sysfb_parse_mode(si, &mode);
	if (compatible) {
		pd = sysfb_create_simplefb(si, &mode);
		if (!IS_ERR(pd))
			goto unlock_mutex;
	}

	 
	if (si->orig_video_isVGA == VIDEO_TYPE_EFI)
		name = "efi-framebuffer";
	else if (si->orig_video_isVGA == VIDEO_TYPE_VLFB)
		name = "vesa-framebuffer";
	else if (si->orig_video_isVGA == VIDEO_TYPE_VGAC)
		name = "vga-framebuffer";
	else if (si->orig_video_isVGA == VIDEO_TYPE_EGAC)
		name = "ega-framebuffer";
	else
		name = "platform-framebuffer";

	pd = platform_device_alloc(name, 0);
	if (!pd) {
		ret = -ENOMEM;
		goto unlock_mutex;
	}

	sysfb_set_efifb_fwnode(pd);

	ret = platform_device_add_data(pd, si, sizeof(*si));
	if (ret)
		goto err;

	ret = platform_device_add(pd);
	if (ret)
		goto err;

	goto unlock_mutex;
err:
	platform_device_put(pd);
unlock_mutex:
	mutex_unlock(&disable_lock);
	return ret;
}

 
subsys_initcall_sync(sysfb_init);
