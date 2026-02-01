 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/console.h>
#include <linux/pci.h>
#include <linux/sysrq.h>
#include <linux/vga_switcheroo.h>

#include <drm/drm_atomic.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>

#include "drm_internal.h"

static bool drm_fbdev_emulation = true;
module_param_named(fbdev_emulation, drm_fbdev_emulation, bool, 0600);
MODULE_PARM_DESC(fbdev_emulation,
		 "Enable legacy fbdev emulation [default=true]");

static int drm_fbdev_overalloc = CONFIG_DRM_FBDEV_OVERALLOC;
module_param(drm_fbdev_overalloc, int, 0444);
MODULE_PARM_DESC(drm_fbdev_overalloc,
		 "Overallocation of the fbdev buffer (%) [default="
		 __MODULE_STRING(CONFIG_DRM_FBDEV_OVERALLOC) "]");

 
static bool drm_leak_fbdev_smem;
#if IS_ENABLED(CONFIG_DRM_FBDEV_LEAK_PHYS_SMEM)
module_param_unsafe(drm_leak_fbdev_smem, bool, 0600);
MODULE_PARM_DESC(drm_leak_fbdev_smem,
		 "Allow unsafe leaking fbdev physical smem address [default=false]");
#endif

static LIST_HEAD(kernel_fb_helper_list);
static DEFINE_MUTEX(kernel_fb_helper_lock);

 

static void drm_fb_helper_restore_lut_atomic(struct drm_crtc *crtc)
{
	uint16_t *r_base, *g_base, *b_base;

	if (crtc->funcs->gamma_set == NULL)
		return;

	r_base = crtc->gamma_store;
	g_base = r_base + crtc->gamma_size;
	b_base = g_base + crtc->gamma_size;

	crtc->funcs->gamma_set(crtc, r_base, g_base, b_base,
			       crtc->gamma_size, NULL);
}

 
int drm_fb_helper_debug_enter(struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	const struct drm_crtc_helper_funcs *funcs;
	struct drm_mode_set *mode_set;

	list_for_each_entry(helper, &kernel_fb_helper_list, kernel_fb_list) {
		mutex_lock(&helper->client.modeset_mutex);
		drm_client_for_each_modeset(mode_set, &helper->client) {
			if (!mode_set->crtc->enabled)
				continue;

			funcs =	mode_set->crtc->helper_private;
			if (funcs->mode_set_base_atomic == NULL)
				continue;

			if (drm_drv_uses_atomic_modeset(mode_set->crtc->dev))
				continue;

			funcs->mode_set_base_atomic(mode_set->crtc,
						    mode_set->fb,
						    mode_set->x,
						    mode_set->y,
						    ENTER_ATOMIC_MODE_SET);
		}
		mutex_unlock(&helper->client.modeset_mutex);
	}

	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_debug_enter);

 
int drm_fb_helper_debug_leave(struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct drm_client_dev *client = &helper->client;
	struct drm_device *dev = helper->dev;
	struct drm_crtc *crtc;
	const struct drm_crtc_helper_funcs *funcs;
	struct drm_mode_set *mode_set;
	struct drm_framebuffer *fb;

	mutex_lock(&client->modeset_mutex);
	drm_client_for_each_modeset(mode_set, client) {
		crtc = mode_set->crtc;
		if (drm_drv_uses_atomic_modeset(crtc->dev))
			continue;

		funcs = crtc->helper_private;
		fb = crtc->primary->fb;

		if (!crtc->enabled)
			continue;

		if (!fb) {
			drm_err(dev, "no fb to restore?\n");
			continue;
		}

		if (funcs->mode_set_base_atomic == NULL)
			continue;

		drm_fb_helper_restore_lut_atomic(mode_set->crtc);
		funcs->mode_set_base_atomic(mode_set->crtc, fb, crtc->x,
					    crtc->y, LEAVE_ATOMIC_MODE_SET);
	}
	mutex_unlock(&client->modeset_mutex);

	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_debug_leave);

static int
__drm_fb_helper_restore_fbdev_mode_unlocked(struct drm_fb_helper *fb_helper,
					    bool force)
{
	bool do_delayed;
	int ret;

	if (!drm_fbdev_emulation || !fb_helper)
		return -ENODEV;

	if (READ_ONCE(fb_helper->deferred_setup))
		return 0;

	mutex_lock(&fb_helper->lock);
	if (force) {
		 
		ret = drm_client_modeset_commit_locked(&fb_helper->client);
	} else {
		ret = drm_client_modeset_commit(&fb_helper->client);
	}

	do_delayed = fb_helper->delayed_hotplug;
	if (do_delayed)
		fb_helper->delayed_hotplug = false;
	mutex_unlock(&fb_helper->lock);

	if (do_delayed)
		drm_fb_helper_hotplug_event(fb_helper);

	return ret;
}

 
int drm_fb_helper_restore_fbdev_mode_unlocked(struct drm_fb_helper *fb_helper)
{
	return __drm_fb_helper_restore_fbdev_mode_unlocked(fb_helper, false);
}
EXPORT_SYMBOL(drm_fb_helper_restore_fbdev_mode_unlocked);

#ifdef CONFIG_MAGIC_SYSRQ
 
static void drm_fb_helper_restore_work_fn(struct work_struct *ignored)
{
	struct drm_fb_helper *helper;

	mutex_lock(&kernel_fb_helper_lock);
	list_for_each_entry(helper, &kernel_fb_helper_list, kernel_fb_list) {
		struct drm_device *dev = helper->dev;

		if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
			continue;

		mutex_lock(&helper->lock);
		drm_client_modeset_commit_locked(&helper->client);
		mutex_unlock(&helper->lock);
	}
	mutex_unlock(&kernel_fb_helper_lock);
}

static DECLARE_WORK(drm_fb_helper_restore_work, drm_fb_helper_restore_work_fn);

static void drm_fb_helper_sysrq(u8 dummy1)
{
	schedule_work(&drm_fb_helper_restore_work);
}

static const struct sysrq_key_op sysrq_drm_fb_helper_restore_op = {
	.handler = drm_fb_helper_sysrq,
	.help_msg = "force-fb(v)",
	.action_msg = "Restore framebuffer console",
};
#else
static const struct sysrq_key_op sysrq_drm_fb_helper_restore_op = { };
#endif

static void drm_fb_helper_dpms(struct fb_info *info, int dpms_mode)
{
	struct drm_fb_helper *fb_helper = info->par;

	mutex_lock(&fb_helper->lock);
	drm_client_modeset_dpms(&fb_helper->client, dpms_mode);
	mutex_unlock(&fb_helper->lock);
}

 
int drm_fb_helper_blank(int blank, struct fb_info *info)
{
	if (oops_in_progress)
		return -EBUSY;

	switch (blank) {
	 
	case FB_BLANK_UNBLANK:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_ON);
		break;
	 
	case FB_BLANK_NORMAL:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_STANDBY);
		break;
	 
	case FB_BLANK_HSYNC_SUSPEND:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_STANDBY);
		break;
	 
	case FB_BLANK_VSYNC_SUSPEND:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_SUSPEND);
		break;
	 
	case FB_BLANK_POWERDOWN:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_OFF);
		break;
	}
	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_blank);

static void drm_fb_helper_resume_worker(struct work_struct *work)
{
	struct drm_fb_helper *helper = container_of(work, struct drm_fb_helper,
						    resume_work);

	console_lock();
	fb_set_suspend(helper->info, 0);
	console_unlock();
}

static void drm_fb_helper_fb_dirty(struct drm_fb_helper *helper)
{
	struct drm_device *dev = helper->dev;
	struct drm_clip_rect *clip = &helper->damage_clip;
	struct drm_clip_rect clip_copy;
	unsigned long flags;
	int ret;

	if (drm_WARN_ON_ONCE(dev, !helper->funcs->fb_dirty))
		return;

	spin_lock_irqsave(&helper->damage_lock, flags);
	clip_copy = *clip;
	clip->x1 = clip->y1 = ~0;
	clip->x2 = clip->y2 = 0;
	spin_unlock_irqrestore(&helper->damage_lock, flags);

	ret = helper->funcs->fb_dirty(helper, &clip_copy);
	if (ret)
		goto err;

	return;

err:
	 
	spin_lock_irqsave(&helper->damage_lock, flags);
	clip->x1 = min_t(u32, clip->x1, clip_copy.x1);
	clip->y1 = min_t(u32, clip->y1, clip_copy.y1);
	clip->x2 = max_t(u32, clip->x2, clip_copy.x2);
	clip->y2 = max_t(u32, clip->y2, clip_copy.y2);
	spin_unlock_irqrestore(&helper->damage_lock, flags);
}

static void drm_fb_helper_damage_work(struct work_struct *work)
{
	struct drm_fb_helper *helper = container_of(work, struct drm_fb_helper, damage_work);

	drm_fb_helper_fb_dirty(helper);
}

 
void drm_fb_helper_prepare(struct drm_device *dev, struct drm_fb_helper *helper,
			   unsigned int preferred_bpp,
			   const struct drm_fb_helper_funcs *funcs)
{
	 
	if (!preferred_bpp)
		preferred_bpp = 32;

	INIT_LIST_HEAD(&helper->kernel_fb_list);
	spin_lock_init(&helper->damage_lock);
	INIT_WORK(&helper->resume_work, drm_fb_helper_resume_worker);
	INIT_WORK(&helper->damage_work, drm_fb_helper_damage_work);
	helper->damage_clip.x1 = helper->damage_clip.y1 = ~0;
	mutex_init(&helper->lock);
	helper->funcs = funcs;
	helper->dev = dev;
	helper->preferred_bpp = preferred_bpp;
}
EXPORT_SYMBOL(drm_fb_helper_prepare);

 
void drm_fb_helper_unprepare(struct drm_fb_helper *fb_helper)
{
	mutex_destroy(&fb_helper->lock);
}
EXPORT_SYMBOL(drm_fb_helper_unprepare);

 
int drm_fb_helper_init(struct drm_device *dev,
		       struct drm_fb_helper *fb_helper)
{
	int ret;

	 
	if (!fb_helper->client.funcs) {
		ret = drm_client_init(dev, &fb_helper->client, "drm_fb_helper", NULL);
		if (ret)
			return ret;
	}

	dev->fb_helper = fb_helper;

	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_init);

 
struct fb_info *drm_fb_helper_alloc_info(struct drm_fb_helper *fb_helper)
{
	struct device *dev = fb_helper->dev->dev;
	struct fb_info *info;
	int ret;

	info = framebuffer_alloc(0, dev);
	if (!info)
		return ERR_PTR(-ENOMEM);

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret)
		goto err_release;

	fb_helper->info = info;
	info->skip_vt_switch = true;

	return info;

err_release:
	framebuffer_release(info);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(drm_fb_helper_alloc_info);

 
void drm_fb_helper_release_info(struct drm_fb_helper *fb_helper)
{
	struct fb_info *info = fb_helper->info;

	if (!info)
		return;

	fb_helper->info = NULL;

	if (info->cmap.len)
		fb_dealloc_cmap(&info->cmap);
	framebuffer_release(info);
}
EXPORT_SYMBOL(drm_fb_helper_release_info);

 
void drm_fb_helper_unregister_info(struct drm_fb_helper *fb_helper)
{
	if (fb_helper && fb_helper->info)
		unregister_framebuffer(fb_helper->info);
}
EXPORT_SYMBOL(drm_fb_helper_unregister_info);

 
void drm_fb_helper_fini(struct drm_fb_helper *fb_helper)
{
	if (!fb_helper)
		return;

	fb_helper->dev->fb_helper = NULL;

	if (!drm_fbdev_emulation)
		return;

	cancel_work_sync(&fb_helper->resume_work);
	cancel_work_sync(&fb_helper->damage_work);

	drm_fb_helper_release_info(fb_helper);

	mutex_lock(&kernel_fb_helper_lock);
	if (!list_empty(&fb_helper->kernel_fb_list)) {
		list_del(&fb_helper->kernel_fb_list);
		if (list_empty(&kernel_fb_helper_list))
			unregister_sysrq_key('v', &sysrq_drm_fb_helper_restore_op);
	}
	mutex_unlock(&kernel_fb_helper_lock);

	if (!fb_helper->client.funcs)
		drm_client_release(&fb_helper->client);
}
EXPORT_SYMBOL(drm_fb_helper_fini);

static void drm_fb_helper_add_damage_clip(struct drm_fb_helper *helper, u32 x, u32 y,
					  u32 width, u32 height)
{
	struct drm_clip_rect *clip = &helper->damage_clip;
	unsigned long flags;

	spin_lock_irqsave(&helper->damage_lock, flags);
	clip->x1 = min_t(u32, clip->x1, x);
	clip->y1 = min_t(u32, clip->y1, y);
	clip->x2 = max_t(u32, clip->x2, x + width);
	clip->y2 = max_t(u32, clip->y2, y + height);
	spin_unlock_irqrestore(&helper->damage_lock, flags);
}

static void drm_fb_helper_damage(struct drm_fb_helper *helper, u32 x, u32 y,
				 u32 width, u32 height)
{
	drm_fb_helper_add_damage_clip(helper, x, y, width, height);

	schedule_work(&helper->damage_work);
}

 
static void drm_fb_helper_memory_range_to_clip(struct fb_info *info, off_t off, size_t len,
					       struct drm_rect *clip)
{
	u32 line_length = info->fix.line_length;
	u32 fb_height = info->var.yres;
	off_t end = off + len;
	u32 x1 = 0;
	u32 y1 = off / line_length;
	u32 x2 = info->var.xres;
	u32 y2 = DIV_ROUND_UP(end, line_length);

	 
	if (y1 > fb_height)
		y1 = fb_height;
	if (y2 > fb_height)
		y2 = fb_height;

	if ((y2 - y1) == 1) {
		 
		off_t bit_off = (off % line_length) * 8;
		off_t bit_end = (end % line_length) * 8;

		x1 = bit_off / info->var.bits_per_pixel;
		x2 = DIV_ROUND_UP(bit_end, info->var.bits_per_pixel);
	}

	drm_rect_init(clip, x1, y1, x2 - x1, y2 - y1);
}

 
void drm_fb_helper_damage_range(struct fb_info *info, off_t off, size_t len)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_rect damage_area;

	drm_fb_helper_memory_range_to_clip(info, off, len, &damage_area);
	drm_fb_helper_damage(fb_helper, damage_area.x1, damage_area.y1,
			     drm_rect_width(&damage_area),
			     drm_rect_height(&damage_area));
}
EXPORT_SYMBOL(drm_fb_helper_damage_range);

 
void drm_fb_helper_damage_area(struct fb_info *info, u32 x, u32 y, u32 width, u32 height)
{
	struct drm_fb_helper *fb_helper = info->par;

	drm_fb_helper_damage(fb_helper, x, y, width, height);
}
EXPORT_SYMBOL(drm_fb_helper_damage_area);

 
void drm_fb_helper_deferred_io(struct fb_info *info, struct list_head *pagereflist)
{
	struct drm_fb_helper *helper = info->par;
	unsigned long start, end, min_off, max_off, total_size;
	struct fb_deferred_io_pageref *pageref;
	struct drm_rect damage_area;

	min_off = ULONG_MAX;
	max_off = 0;
	list_for_each_entry(pageref, pagereflist, list) {
		start = pageref->offset;
		end = start + PAGE_SIZE;
		min_off = min(min_off, start);
		max_off = max(max_off, end);
	}

	 
	if (info->screen_size)
		total_size = info->screen_size;
	else
		total_size = info->fix.smem_len;
	max_off = min(max_off, total_size);

	if (min_off < max_off) {
		drm_fb_helper_memory_range_to_clip(info, min_off, max_off - min_off, &damage_area);
		drm_fb_helper_damage(helper, damage_area.x1, damage_area.y1,
				     drm_rect_width(&damage_area),
				     drm_rect_height(&damage_area));
	}
}
EXPORT_SYMBOL(drm_fb_helper_deferred_io);

 
void drm_fb_helper_set_suspend(struct drm_fb_helper *fb_helper, bool suspend)
{
	if (fb_helper && fb_helper->info)
		fb_set_suspend(fb_helper->info, suspend);
}
EXPORT_SYMBOL(drm_fb_helper_set_suspend);

 
void drm_fb_helper_set_suspend_unlocked(struct drm_fb_helper *fb_helper,
					bool suspend)
{
	if (!fb_helper || !fb_helper->info)
		return;

	 
	flush_work(&fb_helper->resume_work);

	if (suspend) {
		if (fb_helper->info->state != FBINFO_STATE_RUNNING)
			return;

		console_lock();

	} else {
		if (fb_helper->info->state == FBINFO_STATE_RUNNING)
			return;

		if (!console_trylock()) {
			schedule_work(&fb_helper->resume_work);
			return;
		}
	}

	fb_set_suspend(fb_helper->info, suspend);
	console_unlock();
}
EXPORT_SYMBOL(drm_fb_helper_set_suspend_unlocked);

static int setcmap_pseudo_palette(struct fb_cmap *cmap, struct fb_info *info)
{
	u32 *palette = (u32 *)info->pseudo_palette;
	int i;

	if (cmap->start + cmap->len > 16)
		return -EINVAL;

	for (i = 0; i < cmap->len; ++i) {
		u16 red = cmap->red[i];
		u16 green = cmap->green[i];
		u16 blue = cmap->blue[i];
		u32 value;

		red >>= 16 - info->var.red.length;
		green >>= 16 - info->var.green.length;
		blue >>= 16 - info->var.blue.length;
		value = (red << info->var.red.offset) |
			(green << info->var.green.offset) |
			(blue << info->var.blue.offset);
		if (info->var.transp.length > 0) {
			u32 mask = (1 << info->var.transp.length) - 1;

			mask <<= info->var.transp.offset;
			value |= mask;
		}
		palette[cmap->start + i] = value;
	}

	return 0;
}

static int setcmap_legacy(struct fb_cmap *cmap, struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_mode_set *modeset;
	struct drm_crtc *crtc;
	u16 *r, *g, *b;
	int ret = 0;

	drm_modeset_lock_all(fb_helper->dev);
	drm_client_for_each_modeset(modeset, &fb_helper->client) {
		crtc = modeset->crtc;
		if (!crtc->funcs->gamma_set || !crtc->gamma_size) {
			ret = -EINVAL;
			goto out;
		}

		if (cmap->start + cmap->len > crtc->gamma_size) {
			ret = -EINVAL;
			goto out;
		}

		r = crtc->gamma_store;
		g = r + crtc->gamma_size;
		b = g + crtc->gamma_size;

		memcpy(r + cmap->start, cmap->red, cmap->len * sizeof(*r));
		memcpy(g + cmap->start, cmap->green, cmap->len * sizeof(*g));
		memcpy(b + cmap->start, cmap->blue, cmap->len * sizeof(*b));

		ret = crtc->funcs->gamma_set(crtc, r, g, b,
					     crtc->gamma_size, NULL);
		if (ret)
			goto out;
	}
out:
	drm_modeset_unlock_all(fb_helper->dev);

	return ret;
}

static struct drm_property_blob *setcmap_new_gamma_lut(struct drm_crtc *crtc,
						       struct fb_cmap *cmap)
{
	struct drm_device *dev = crtc->dev;
	struct drm_property_blob *gamma_lut;
	struct drm_color_lut *lut;
	int size = crtc->gamma_size;
	int i;

	if (!size || cmap->start + cmap->len > size)
		return ERR_PTR(-EINVAL);

	gamma_lut = drm_property_create_blob(dev, sizeof(*lut) * size, NULL);
	if (IS_ERR(gamma_lut))
		return gamma_lut;

	lut = gamma_lut->data;
	if (cmap->start || cmap->len != size) {
		u16 *r = crtc->gamma_store;
		u16 *g = r + crtc->gamma_size;
		u16 *b = g + crtc->gamma_size;

		for (i = 0; i < cmap->start; i++) {
			lut[i].red = r[i];
			lut[i].green = g[i];
			lut[i].blue = b[i];
		}
		for (i = cmap->start + cmap->len; i < size; i++) {
			lut[i].red = r[i];
			lut[i].green = g[i];
			lut[i].blue = b[i];
		}
	}

	for (i = 0; i < cmap->len; i++) {
		lut[cmap->start + i].red = cmap->red[i];
		lut[cmap->start + i].green = cmap->green[i];
		lut[cmap->start + i].blue = cmap->blue[i];
	}

	return gamma_lut;
}

static int setcmap_atomic(struct fb_cmap *cmap, struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	struct drm_property_blob *gamma_lut = NULL;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_crtc_state *crtc_state;
	struct drm_atomic_state *state;
	struct drm_mode_set *modeset;
	struct drm_crtc *crtc;
	u16 *r, *g, *b;
	bool replaced;
	int ret = 0;

	drm_modeset_acquire_init(&ctx, 0);

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		ret = -ENOMEM;
		goto out_ctx;
	}

	state->acquire_ctx = &ctx;
retry:
	drm_client_for_each_modeset(modeset, &fb_helper->client) {
		crtc = modeset->crtc;

		if (!gamma_lut)
			gamma_lut = setcmap_new_gamma_lut(crtc, cmap);
		if (IS_ERR(gamma_lut)) {
			ret = PTR_ERR(gamma_lut);
			gamma_lut = NULL;
			goto out_state;
		}

		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state)) {
			ret = PTR_ERR(crtc_state);
			goto out_state;
		}

		 
		replaced  = drm_property_replace_blob(&crtc_state->degamma_lut,
						      NULL);
		replaced |= drm_property_replace_blob(&crtc_state->ctm, NULL);
		replaced |= drm_property_replace_blob(&crtc_state->gamma_lut,
						      gamma_lut);
		crtc_state->color_mgmt_changed |= replaced;
	}

	ret = drm_atomic_commit(state);
	if (ret)
		goto out_state;

	drm_client_for_each_modeset(modeset, &fb_helper->client) {
		crtc = modeset->crtc;

		r = crtc->gamma_store;
		g = r + crtc->gamma_size;
		b = g + crtc->gamma_size;

		memcpy(r + cmap->start, cmap->red, cmap->len * sizeof(*r));
		memcpy(g + cmap->start, cmap->green, cmap->len * sizeof(*g));
		memcpy(b + cmap->start, cmap->blue, cmap->len * sizeof(*b));
	}

out_state:
	if (ret == -EDEADLK)
		goto backoff;

	drm_property_blob_put(gamma_lut);
	drm_atomic_state_put(state);
out_ctx:
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return ret;

backoff:
	drm_atomic_state_clear(state);
	drm_modeset_backoff(&ctx);
	goto retry;
}

 
int drm_fb_helper_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	int ret;

	if (oops_in_progress)
		return -EBUSY;

	mutex_lock(&fb_helper->lock);

	if (!drm_master_internal_acquire(dev)) {
		ret = -EBUSY;
		goto unlock;
	}

	mutex_lock(&fb_helper->client.modeset_mutex);
	if (info->fix.visual == FB_VISUAL_TRUECOLOR)
		ret = setcmap_pseudo_palette(cmap, info);
	else if (drm_drv_uses_atomic_modeset(fb_helper->dev))
		ret = setcmap_atomic(cmap, info);
	else
		ret = setcmap_legacy(cmap, info);
	mutex_unlock(&fb_helper->client.modeset_mutex);

	drm_master_internal_release(dev);
unlock:
	mutex_unlock(&fb_helper->lock);

	return ret;
}
EXPORT_SYMBOL(drm_fb_helper_setcmap);

 
int drm_fb_helper_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	struct drm_crtc *crtc;
	int ret = 0;

	mutex_lock(&fb_helper->lock);
	if (!drm_master_internal_acquire(dev)) {
		ret = -EBUSY;
		goto unlock;
	}

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		 
		crtc = fb_helper->client.modesets[0].crtc;

		 
		ret = drm_crtc_vblank_get(crtc);
		if (!ret) {
			drm_crtc_wait_one_vblank(crtc);
			drm_crtc_vblank_put(crtc);
		}

		ret = 0;
		break;
	default:
		ret = -ENOTTY;
	}

	drm_master_internal_release(dev);
unlock:
	mutex_unlock(&fb_helper->lock);
	return ret;
}
EXPORT_SYMBOL(drm_fb_helper_ioctl);

static bool drm_fb_pixel_format_equal(const struct fb_var_screeninfo *var_1,
				      const struct fb_var_screeninfo *var_2)
{
	return var_1->bits_per_pixel == var_2->bits_per_pixel &&
	       var_1->grayscale == var_2->grayscale &&
	       var_1->red.offset == var_2->red.offset &&
	       var_1->red.length == var_2->red.length &&
	       var_1->red.msb_right == var_2->red.msb_right &&
	       var_1->green.offset == var_2->green.offset &&
	       var_1->green.length == var_2->green.length &&
	       var_1->green.msb_right == var_2->green.msb_right &&
	       var_1->blue.offset == var_2->blue.offset &&
	       var_1->blue.length == var_2->blue.length &&
	       var_1->blue.msb_right == var_2->blue.msb_right &&
	       var_1->transp.offset == var_2->transp.offset &&
	       var_1->transp.length == var_2->transp.length &&
	       var_1->transp.msb_right == var_2->transp.msb_right;
}

static void drm_fb_helper_fill_pixel_fmt(struct fb_var_screeninfo *var,
					 const struct drm_format_info *format)
{
	u8 depth = format->depth;

	if (format->is_color_indexed) {
		var->red.offset = 0;
		var->green.offset = 0;
		var->blue.offset = 0;
		var->red.length = depth;
		var->green.length = depth;
		var->blue.length = depth;
		var->transp.offset = 0;
		var->transp.length = 0;
		return;
	}

	switch (depth) {
	case 15:
		var->red.offset = 10;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.offset = 15;
		var->transp.length = 1;
		break;
	case 16:
		var->red.offset = 11;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->transp.offset = 0;
		break;
	case 24:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	default:
		break;
	}
}

static void __fill_var(struct fb_var_screeninfo *var, struct fb_info *info,
		       struct drm_framebuffer *fb)
{
	int i;

	var->xres_virtual = fb->width;
	var->yres_virtual = fb->height;
	var->accel_flags = 0;
	var->bits_per_pixel = drm_format_info_bpp(fb->format, 0);

	var->height = info->var.height;
	var->width = info->var.width;

	var->left_margin = var->right_margin = 0;
	var->upper_margin = var->lower_margin = 0;
	var->hsync_len = var->vsync_len = 0;
	var->sync = var->vmode = 0;
	var->rotate = 0;
	var->colorspace = 0;
	for (i = 0; i < 4; i++)
		var->reserved[i] = 0;
}

 
int drm_fb_helper_check_var(struct fb_var_screeninfo *var,
			    struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_framebuffer *fb = fb_helper->fb;
	const struct drm_format_info *format = fb->format;
	struct drm_device *dev = fb_helper->dev;
	unsigned int bpp;

	if (in_dbg_master())
		return -EINVAL;

	if (var->pixclock != 0) {
		drm_dbg_kms(dev, "fbdev emulation doesn't support changing the pixel clock, value of pixclock is ignored\n");
		var->pixclock = 0;
	}

	switch (format->format) {
	case DRM_FORMAT_C1:
	case DRM_FORMAT_C2:
	case DRM_FORMAT_C4:
		 
		break;

	default:
		if ((drm_format_info_block_width(format, 0) > 1) ||
		    (drm_format_info_block_height(format, 0) > 1))
			return -EINVAL;
		break;
	}

	 
	bpp = drm_format_info_bpp(format, 0);
	if (var->bits_per_pixel > bpp ||
	    var->xres > fb->width || var->yres > fb->height ||
	    var->xres_virtual > fb->width || var->yres_virtual > fb->height) {
		drm_dbg_kms(dev, "fb requested width/height/bpp can't fit in current fb "
			  "request %dx%d-%d (virtual %dx%d) > %dx%d-%d\n",
			  var->xres, var->yres, var->bits_per_pixel,
			  var->xres_virtual, var->yres_virtual,
			  fb->width, fb->height, bpp);
		return -EINVAL;
	}

	__fill_var(var, info, fb);

	 
	if (var->yoffset > var->yres_virtual - var->yres ||
	    var->xoffset > var->xres_virtual - var->xres)
		return -EINVAL;

	 
	if (var->grayscale > 0)
		return -EINVAL;

	if (var->nonstd)
		return -EINVAL;

	 
	if (!var->red.offset     && !var->green.offset    &&
	    !var->blue.offset    && !var->transp.offset   &&
	    !var->red.length     && !var->green.length    &&
	    !var->blue.length    && !var->transp.length   &&
	    !var->red.msb_right  && !var->green.msb_right &&
	    !var->blue.msb_right && !var->transp.msb_right) {
		drm_fb_helper_fill_pixel_fmt(var, format);
	}

	 
	if (!drm_fb_pixel_format_equal(var, &info->var)) {
		drm_dbg_kms(dev, "fbdev emulation doesn't support changing the pixel format\n");
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_check_var);

 
int drm_fb_helper_set_par(struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct fb_var_screeninfo *var = &info->var;
	bool force;

	if (oops_in_progress)
		return -EBUSY;

	 
	force = var->activate & FB_ACTIVATE_KD_TEXT;

	__drm_fb_helper_restore_fbdev_mode_unlocked(fb_helper, force);

	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_set_par);

static void pan_set(struct drm_fb_helper *fb_helper, int x, int y)
{
	struct drm_mode_set *mode_set;

	mutex_lock(&fb_helper->client.modeset_mutex);
	drm_client_for_each_modeset(mode_set, &fb_helper->client) {
		mode_set->x = x;
		mode_set->y = y;
	}
	mutex_unlock(&fb_helper->client.modeset_mutex);
}

static int pan_display_atomic(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	int ret;

	pan_set(fb_helper, var->xoffset, var->yoffset);

	ret = drm_client_modeset_commit_locked(&fb_helper->client);
	if (!ret) {
		info->var.xoffset = var->xoffset;
		info->var.yoffset = var->yoffset;
	} else
		pan_set(fb_helper, info->var.xoffset, info->var.yoffset);

	return ret;
}

static int pan_display_legacy(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_client_dev *client = &fb_helper->client;
	struct drm_mode_set *modeset;
	int ret = 0;

	mutex_lock(&client->modeset_mutex);
	drm_modeset_lock_all(fb_helper->dev);
	drm_client_for_each_modeset(modeset, client) {
		modeset->x = var->xoffset;
		modeset->y = var->yoffset;

		if (modeset->num_connectors) {
			ret = drm_mode_set_config_internal(modeset);
			if (!ret) {
				info->var.xoffset = var->xoffset;
				info->var.yoffset = var->yoffset;
			}
		}
	}
	drm_modeset_unlock_all(fb_helper->dev);
	mutex_unlock(&client->modeset_mutex);

	return ret;
}

 
int drm_fb_helper_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	int ret;

	if (oops_in_progress)
		return -EBUSY;

	mutex_lock(&fb_helper->lock);
	if (!drm_master_internal_acquire(dev)) {
		ret = -EBUSY;
		goto unlock;
	}

	if (drm_drv_uses_atomic_modeset(dev))
		ret = pan_display_atomic(var, info);
	else
		ret = pan_display_legacy(var, info);

	drm_master_internal_release(dev);
unlock:
	mutex_unlock(&fb_helper->lock);

	return ret;
}
EXPORT_SYMBOL(drm_fb_helper_pan_display);

static uint32_t drm_fb_helper_find_format(struct drm_fb_helper *fb_helper, const uint32_t *formats,
					  size_t format_count, uint32_t bpp, uint32_t depth)
{
	struct drm_device *dev = fb_helper->dev;
	uint32_t format;
	size_t i;

	 
	format = drm_mode_legacy_fb_format(bpp, depth);
	if (!format)
		goto err;

	for (i = 0; i < format_count; ++i) {
		if (formats[i] == format)
			return format;
	}

err:
	 
	drm_warn(dev, "bpp/depth value of %u/%u not supported\n", bpp, depth);

	return DRM_FORMAT_INVALID;
}

static uint32_t drm_fb_helper_find_color_mode_format(struct drm_fb_helper *fb_helper,
						     const uint32_t *formats, size_t format_count,
						     unsigned int color_mode)
{
	struct drm_device *dev = fb_helper->dev;
	uint32_t bpp, depth;

	switch (color_mode) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
	case 24:
		bpp = depth = color_mode;
		break;
	case 15:
		bpp = 16;
		depth = 15;
		break;
	case 32:
		bpp = 32;
		depth = 24;
		break;
	default:
		drm_info(dev, "unsupported color mode of %d\n", color_mode);
		return DRM_FORMAT_INVALID;
	}

	return drm_fb_helper_find_format(fb_helper, formats, format_count, bpp, depth);
}

static int __drm_fb_helper_find_sizes(struct drm_fb_helper *fb_helper,
				      struct drm_fb_helper_surface_size *sizes)
{
	struct drm_client_dev *client = &fb_helper->client;
	struct drm_device *dev = fb_helper->dev;
	int crtc_count = 0;
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *connector;
	struct drm_mode_set *mode_set;
	uint32_t surface_format = DRM_FORMAT_INVALID;
	const struct drm_format_info *info;

	memset(sizes, 0, sizeof(*sizes));
	sizes->fb_width = (u32)-1;
	sizes->fb_height = (u32)-1;

	drm_client_for_each_modeset(mode_set, client) {
		struct drm_crtc *crtc = mode_set->crtc;
		struct drm_plane *plane = crtc->primary;

		drm_dbg_kms(dev, "test CRTC %u primary plane\n", drm_crtc_index(crtc));

		drm_connector_list_iter_begin(fb_helper->dev, &conn_iter);
		drm_client_for_each_connector_iter(connector, &conn_iter) {
			struct drm_cmdline_mode *cmdline_mode = &connector->cmdline_mode;

			if (!cmdline_mode->bpp_specified)
				continue;

			surface_format = drm_fb_helper_find_color_mode_format(fb_helper,
									      plane->format_types,
									      plane->format_count,
									      cmdline_mode->bpp);
			if (surface_format != DRM_FORMAT_INVALID)
				break;  
		}
		drm_connector_list_iter_end(&conn_iter);

		if (surface_format != DRM_FORMAT_INVALID)
			break;  

		 
		surface_format = drm_fb_helper_find_color_mode_format(fb_helper,
								      plane->format_types,
								      plane->format_count,
								      fb_helper->preferred_bpp);
		if (surface_format != DRM_FORMAT_INVALID)
			break;  
	}

	if (surface_format == DRM_FORMAT_INVALID) {
		 
		drm_warn(dev, "No compatible format found\n");
		surface_format = drm_driver_legacy_fb_format(dev, 32, 24);
	}

	info = drm_format_info(surface_format);
	sizes->surface_bpp = drm_format_info_bpp(info, 0);
	sizes->surface_depth = info->depth;

	 
	crtc_count = 0;
	drm_client_for_each_modeset(mode_set, client) {
		struct drm_display_mode *desired_mode;
		int x, y, j;
		 
		bool lastv = true, lasth = true;

		desired_mode = mode_set->mode;

		if (!desired_mode)
			continue;

		crtc_count++;

		x = mode_set->x;
		y = mode_set->y;

		sizes->surface_width  =
			max_t(u32, desired_mode->hdisplay + x, sizes->surface_width);
		sizes->surface_height =
			max_t(u32, desired_mode->vdisplay + y, sizes->surface_height);

		for (j = 0; j < mode_set->num_connectors; j++) {
			struct drm_connector *connector = mode_set->connectors[j];

			if (connector->has_tile &&
			    desired_mode->hdisplay == connector->tile_h_size &&
			    desired_mode->vdisplay == connector->tile_v_size) {
				lasth = (connector->tile_h_loc == (connector->num_h_tile - 1));
				lastv = (connector->tile_v_loc == (connector->num_v_tile - 1));
				 
				break;
			}
		}

		if (lasth)
			sizes->fb_width  = min_t(u32, desired_mode->hdisplay + x, sizes->fb_width);
		if (lastv)
			sizes->fb_height = min_t(u32, desired_mode->vdisplay + y, sizes->fb_height);
	}

	if (crtc_count == 0 || sizes->fb_width == -1 || sizes->fb_height == -1) {
		drm_info(dev, "Cannot find any crtc or sizes\n");
		return -EAGAIN;
	}

	return 0;
}

static int drm_fb_helper_find_sizes(struct drm_fb_helper *fb_helper,
				    struct drm_fb_helper_surface_size *sizes)
{
	struct drm_client_dev *client = &fb_helper->client;
	struct drm_device *dev = fb_helper->dev;
	struct drm_mode_config *config = &dev->mode_config;
	int ret;

	mutex_lock(&client->modeset_mutex);
	ret = __drm_fb_helper_find_sizes(fb_helper, sizes);
	mutex_unlock(&client->modeset_mutex);

	if (ret)
		return ret;

	 
	sizes->surface_height *= drm_fbdev_overalloc;
	sizes->surface_height /= 100;
	if (sizes->surface_height > config->max_height) {
		drm_dbg_kms(dev, "Fbdev over-allocation too large; clamping height to %d\n",
			    config->max_height);
		sizes->surface_height = config->max_height;
	}

	return 0;
}

 
static int drm_fb_helper_single_fb_probe(struct drm_fb_helper *fb_helper)
{
	struct drm_client_dev *client = &fb_helper->client;
	struct drm_device *dev = fb_helper->dev;
	struct drm_fb_helper_surface_size sizes;
	int ret;

	ret = drm_fb_helper_find_sizes(fb_helper, &sizes);
	if (ret) {
		 
		if (!fb_helper->deferred_setup)
			drm_client_modeset_commit(client);
		return ret;
	}

	 
	ret = (*fb_helper->funcs->fb_probe)(fb_helper, &sizes);
	if (ret < 0)
		return ret;

	strcpy(fb_helper->fb->comm, "[fbcon]");

	 
	if (dev_is_pci(dev->dev))
		vga_switcheroo_client_fb_set(to_pci_dev(dev->dev), fb_helper->info);

	return 0;
}

static void drm_fb_helper_fill_fix(struct fb_info *info, uint32_t pitch,
				   bool is_color_indexed)
{
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = is_color_indexed ? FB_VISUAL_PSEUDOCOLOR
					    : FB_VISUAL_TRUECOLOR;
	info->fix.mmio_start = 0;
	info->fix.mmio_len = 0;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 1;  
	info->fix.ypanstep = 1;  
	info->fix.ywrapstep = 0;
	info->fix.accel = FB_ACCEL_NONE;

	info->fix.line_length = pitch;
}

static void drm_fb_helper_fill_var(struct fb_info *info,
				   struct drm_fb_helper *fb_helper,
				   uint32_t fb_width, uint32_t fb_height)
{
	struct drm_framebuffer *fb = fb_helper->fb;
	const struct drm_format_info *format = fb->format;

	switch (format->format) {
	case DRM_FORMAT_C1:
	case DRM_FORMAT_C2:
	case DRM_FORMAT_C4:
		 
		break;

	default:
		WARN_ON((drm_format_info_block_width(format, 0) > 1) ||
			(drm_format_info_block_height(format, 0) > 1));
		break;
	}

	info->pseudo_palette = fb_helper->pseudo_palette;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	__fill_var(&info->var, info, fb);
	info->var.activate = FB_ACTIVATE_NOW;

	drm_fb_helper_fill_pixel_fmt(&info->var, format);

	info->var.xres = fb_width;
	info->var.yres = fb_height;
}

 
void drm_fb_helper_fill_info(struct fb_info *info,
			     struct drm_fb_helper *fb_helper,
			     struct drm_fb_helper_surface_size *sizes)
{
	struct drm_framebuffer *fb = fb_helper->fb;

	drm_fb_helper_fill_fix(info, fb->pitches[0],
			       fb->format->is_color_indexed);
	drm_fb_helper_fill_var(info, fb_helper,
			       sizes->fb_width, sizes->fb_height);

	info->par = fb_helper;
	 
	snprintf(info->fix.id, sizeof(info->fix.id), "%sdrmfb",
		 fb_helper->dev->driver->name);

}
EXPORT_SYMBOL(drm_fb_helper_fill_info);

 
static void drm_setup_crtcs_fb(struct drm_fb_helper *fb_helper)
{
	struct drm_client_dev *client = &fb_helper->client;
	struct drm_connector_list_iter conn_iter;
	struct fb_info *info = fb_helper->info;
	unsigned int rotation, sw_rotations = 0;
	struct drm_connector *connector;
	struct drm_mode_set *modeset;

	mutex_lock(&client->modeset_mutex);
	drm_client_for_each_modeset(modeset, client) {
		if (!modeset->num_connectors)
			continue;

		modeset->fb = fb_helper->fb;

		if (drm_client_rotation(modeset, &rotation))
			 
			sw_rotations |= DRM_MODE_ROTATE_0;
		else
			sw_rotations |= rotation;
	}
	mutex_unlock(&client->modeset_mutex);

	drm_connector_list_iter_begin(fb_helper->dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {

		 
		if (connector->status == connector_status_connected) {
			info->var.width = connector->display_info.width_mm;
			info->var.height = connector->display_info.height_mm;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	switch (sw_rotations) {
	case DRM_MODE_ROTATE_0:
		info->fbcon_rotate_hint = FB_ROTATE_UR;
		break;
	case DRM_MODE_ROTATE_90:
		info->fbcon_rotate_hint = FB_ROTATE_CCW;
		break;
	case DRM_MODE_ROTATE_180:
		info->fbcon_rotate_hint = FB_ROTATE_UD;
		break;
	case DRM_MODE_ROTATE_270:
		info->fbcon_rotate_hint = FB_ROTATE_CW;
		break;
	default:
		 
		info->fbcon_rotate_hint = FB_ROTATE_UR;
	}
}

 
static int
__drm_fb_helper_initial_config_and_unlock(struct drm_fb_helper *fb_helper)
{
	struct drm_device *dev = fb_helper->dev;
	struct fb_info *info;
	unsigned int width, height;
	int ret;

	width = dev->mode_config.max_width;
	height = dev->mode_config.max_height;

	drm_client_modeset_probe(&fb_helper->client, width, height);
	ret = drm_fb_helper_single_fb_probe(fb_helper);
	if (ret < 0) {
		if (ret == -EAGAIN) {
			fb_helper->deferred_setup = true;
			ret = 0;
		}
		mutex_unlock(&fb_helper->lock);

		return ret;
	}
	drm_setup_crtcs_fb(fb_helper);

	fb_helper->deferred_setup = false;

	info = fb_helper->info;
	info->var.pixclock = 0;

	if (!drm_leak_fbdev_smem)
		info->flags |= FBINFO_HIDE_SMEM_START;

	 
	mutex_unlock(&fb_helper->lock);

	ret = register_framebuffer(info);
	if (ret < 0)
		return ret;

	drm_info(dev, "fb%d: %s frame buffer device\n",
		 info->node, info->fix.id);

	mutex_lock(&kernel_fb_helper_lock);
	if (list_empty(&kernel_fb_helper_list))
		register_sysrq_key('v', &sysrq_drm_fb_helper_restore_op);

	list_add(&fb_helper->kernel_fb_list, &kernel_fb_helper_list);
	mutex_unlock(&kernel_fb_helper_lock);

	return 0;
}

 
int drm_fb_helper_initial_config(struct drm_fb_helper *fb_helper)
{
	int ret;

	if (!drm_fbdev_emulation)
		return 0;

	mutex_lock(&fb_helper->lock);
	ret = __drm_fb_helper_initial_config_and_unlock(fb_helper);

	return ret;
}
EXPORT_SYMBOL(drm_fb_helper_initial_config);

 
int drm_fb_helper_hotplug_event(struct drm_fb_helper *fb_helper)
{
	int err = 0;

	if (!drm_fbdev_emulation || !fb_helper)
		return 0;

	mutex_lock(&fb_helper->lock);
	if (fb_helper->deferred_setup) {
		err = __drm_fb_helper_initial_config_and_unlock(fb_helper);
		return err;
	}

	if (!fb_helper->fb || !drm_master_internal_acquire(fb_helper->dev)) {
		fb_helper->delayed_hotplug = true;
		mutex_unlock(&fb_helper->lock);
		return err;
	}

	drm_master_internal_release(fb_helper->dev);

	drm_dbg_kms(fb_helper->dev, "\n");

	drm_client_modeset_probe(&fb_helper->client, fb_helper->fb->width, fb_helper->fb->height);
	drm_setup_crtcs_fb(fb_helper);
	mutex_unlock(&fb_helper->lock);

	drm_fb_helper_set_par(fb_helper->info);

	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_hotplug_event);

 
void drm_fb_helper_lastclose(struct drm_device *dev)
{
	drm_fb_helper_restore_fbdev_mode_unlocked(dev->fb_helper);
}
EXPORT_SYMBOL(drm_fb_helper_lastclose);

 
void drm_fb_helper_output_poll_changed(struct drm_device *dev)
{
	drm_fb_helper_hotplug_event(dev->fb_helper);
}
EXPORT_SYMBOL(drm_fb_helper_output_poll_changed);
