
 

#include <linux/backlight.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

#include <drm/drm_connector.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_format_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_mipi_dbi.h>
#include <drm/drm_modes.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_rect.h>
#include <video/mipi_display.h>

#define MIPI_DBI_MAX_SPI_READ_SPEED 2000000  

#define DCS_POWER_MODE_DISPLAY			BIT(2)
#define DCS_POWER_MODE_DISPLAY_NORMAL_MODE	BIT(3)
#define DCS_POWER_MODE_SLEEP_MODE		BIT(4)
#define DCS_POWER_MODE_PARTIAL_MODE		BIT(5)
#define DCS_POWER_MODE_IDLE_MODE		BIT(6)
#define DCS_POWER_MODE_RESERVED_MASK		(BIT(0) | BIT(1) | BIT(7))

 

#define MIPI_DBI_DEBUG_COMMAND(cmd, data, len) \
({ \
	if (!len) \
		DRM_DEBUG_DRIVER("cmd=%02x\n", cmd); \
	else if (len <= 32) \
		DRM_DEBUG_DRIVER("cmd=%02x, par=%*ph\n", cmd, (int)len, data);\
	else \
		DRM_DEBUG_DRIVER("cmd=%02x, len=%zu\n", cmd, len); \
})

static const u8 mipi_dbi_dcs_read_commands[] = {
	MIPI_DCS_GET_DISPLAY_ID,
	MIPI_DCS_GET_RED_CHANNEL,
	MIPI_DCS_GET_GREEN_CHANNEL,
	MIPI_DCS_GET_BLUE_CHANNEL,
	MIPI_DCS_GET_DISPLAY_STATUS,
	MIPI_DCS_GET_POWER_MODE,
	MIPI_DCS_GET_ADDRESS_MODE,
	MIPI_DCS_GET_PIXEL_FORMAT,
	MIPI_DCS_GET_DISPLAY_MODE,
	MIPI_DCS_GET_SIGNAL_MODE,
	MIPI_DCS_GET_DIAGNOSTIC_RESULT,
	MIPI_DCS_READ_MEMORY_START,
	MIPI_DCS_READ_MEMORY_CONTINUE,
	MIPI_DCS_GET_SCANLINE,
	MIPI_DCS_GET_DISPLAY_BRIGHTNESS,
	MIPI_DCS_GET_CONTROL_DISPLAY,
	MIPI_DCS_GET_POWER_SAVE,
	MIPI_DCS_GET_CABC_MIN_BRIGHTNESS,
	MIPI_DCS_READ_DDB_START,
	MIPI_DCS_READ_DDB_CONTINUE,
	0,  
};

static bool mipi_dbi_command_is_read(struct mipi_dbi *dbi, u8 cmd)
{
	unsigned int i;

	if (!dbi->read_commands)
		return false;

	for (i = 0; i < 0xff; i++) {
		if (!dbi->read_commands[i])
			return false;
		if (cmd == dbi->read_commands[i])
			return true;
	}

	return false;
}

 
int mipi_dbi_command_read(struct mipi_dbi *dbi, u8 cmd, u8 *val)
{
	if (!dbi->read_commands)
		return -EACCES;

	if (!mipi_dbi_command_is_read(dbi, cmd))
		return -EINVAL;

	return mipi_dbi_command_buf(dbi, cmd, val, 1);
}
EXPORT_SYMBOL(mipi_dbi_command_read);

 
int mipi_dbi_command_buf(struct mipi_dbi *dbi, u8 cmd, u8 *data, size_t len)
{
	u8 *cmdbuf;
	int ret;

	 
	cmdbuf = kmemdup(&cmd, 1, GFP_KERNEL);
	if (!cmdbuf)
		return -ENOMEM;

	mutex_lock(&dbi->cmdlock);
	ret = dbi->command(dbi, cmdbuf, data, len);
	mutex_unlock(&dbi->cmdlock);

	kfree(cmdbuf);

	return ret;
}
EXPORT_SYMBOL(mipi_dbi_command_buf);

 
int mipi_dbi_command_stackbuf(struct mipi_dbi *dbi, u8 cmd, const u8 *data,
			      size_t len)
{
	u8 *buf;
	int ret;

	buf = kmemdup(data, len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = mipi_dbi_command_buf(dbi, cmd, buf, len);

	kfree(buf);

	return ret;
}
EXPORT_SYMBOL(mipi_dbi_command_stackbuf);

 
int mipi_dbi_buf_copy(void *dst, struct iosys_map *src, struct drm_framebuffer *fb,
		      struct drm_rect *clip, bool swap)
{
	struct drm_gem_object *gem = drm_gem_fb_get_obj(fb, 0);
	struct iosys_map dst_map = IOSYS_MAP_INIT_VADDR(dst);
	int ret;

	ret = drm_gem_fb_begin_cpu_access(fb, DMA_FROM_DEVICE);
	if (ret)
		return ret;

	switch (fb->format->format) {
	case DRM_FORMAT_RGB565:
		if (swap)
			drm_fb_swab(&dst_map, NULL, src, fb, clip, !gem->import_attach);
		else
			drm_fb_memcpy(&dst_map, NULL, src, fb, clip);
		break;
	case DRM_FORMAT_XRGB8888:
		drm_fb_xrgb8888_to_rgb565(&dst_map, NULL, src, fb, clip, swap);
		break;
	default:
		drm_err_once(fb->dev, "Format is not supported: %p4cc\n",
			     &fb->format->format);
		ret = -EINVAL;
	}

	drm_gem_fb_end_cpu_access(fb, DMA_FROM_DEVICE);

	return ret;
}
EXPORT_SYMBOL(mipi_dbi_buf_copy);

static void mipi_dbi_set_window_address(struct mipi_dbi_dev *dbidev,
					unsigned int xs, unsigned int xe,
					unsigned int ys, unsigned int ye)
{
	struct mipi_dbi *dbi = &dbidev->dbi;

	xs += dbidev->left_offset;
	xe += dbidev->left_offset;
	ys += dbidev->top_offset;
	ye += dbidev->top_offset;

	mipi_dbi_command(dbi, MIPI_DCS_SET_COLUMN_ADDRESS, (xs >> 8) & 0xff,
			 xs & 0xff, (xe >> 8) & 0xff, xe & 0xff);
	mipi_dbi_command(dbi, MIPI_DCS_SET_PAGE_ADDRESS, (ys >> 8) & 0xff,
			 ys & 0xff, (ye >> 8) & 0xff, ye & 0xff);
}

static void mipi_dbi_fb_dirty(struct iosys_map *src, struct drm_framebuffer *fb,
			      struct drm_rect *rect)
{
	struct mipi_dbi_dev *dbidev = drm_to_mipi_dbi_dev(fb->dev);
	unsigned int height = rect->y2 - rect->y1;
	unsigned int width = rect->x2 - rect->x1;
	struct mipi_dbi *dbi = &dbidev->dbi;
	bool swap = dbi->swap_bytes;
	int ret = 0;
	bool full;
	void *tr;

	full = width == fb->width && height == fb->height;

	DRM_DEBUG_KMS("Flushing [FB:%d] " DRM_RECT_FMT "\n", fb->base.id, DRM_RECT_ARG(rect));

	if (!dbi->dc || !full || swap ||
	    fb->format->format == DRM_FORMAT_XRGB8888) {
		tr = dbidev->tx_buf;
		ret = mipi_dbi_buf_copy(tr, src, fb, rect, swap);
		if (ret)
			goto err_msg;
	} else {
		tr = src->vaddr;  
	}

	mipi_dbi_set_window_address(dbidev, rect->x1, rect->x2 - 1, rect->y1,
				    rect->y2 - 1);

	ret = mipi_dbi_command_buf(dbi, MIPI_DCS_WRITE_MEMORY_START, tr,
				   width * height * 2);
err_msg:
	if (ret)
		drm_err_once(fb->dev, "Failed to update display %d\n", ret);
}

 
enum drm_mode_status mipi_dbi_pipe_mode_valid(struct drm_simple_display_pipe *pipe,
					      const struct drm_display_mode *mode)
{
	struct mipi_dbi_dev *dbidev = drm_to_mipi_dbi_dev(pipe->crtc.dev);

	return drm_crtc_helper_mode_valid_fixed(&pipe->crtc, mode, &dbidev->mode);
}
EXPORT_SYMBOL(mipi_dbi_pipe_mode_valid);

 
void mipi_dbi_pipe_update(struct drm_simple_display_pipe *pipe,
			  struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = pipe->plane.state;
	struct drm_shadow_plane_state *shadow_plane_state = to_drm_shadow_plane_state(state);
	struct drm_framebuffer *fb = state->fb;
	struct drm_rect rect;
	int idx;

	if (!pipe->crtc.state->active)
		return;

	if (WARN_ON(!fb))
		return;

	if (!drm_dev_enter(fb->dev, &idx))
		return;

	if (drm_atomic_helper_damage_merged(old_state, state, &rect))
		mipi_dbi_fb_dirty(&shadow_plane_state->data[0], fb, &rect);

	drm_dev_exit(idx);
}
EXPORT_SYMBOL(mipi_dbi_pipe_update);

 
void mipi_dbi_enable_flush(struct mipi_dbi_dev *dbidev,
			   struct drm_crtc_state *crtc_state,
			   struct drm_plane_state *plane_state)
{
	struct drm_shadow_plane_state *shadow_plane_state = to_drm_shadow_plane_state(plane_state);
	struct drm_framebuffer *fb = plane_state->fb;
	struct drm_rect rect = {
		.x1 = 0,
		.x2 = fb->width,
		.y1 = 0,
		.y2 = fb->height,
	};
	int idx;

	if (!drm_dev_enter(&dbidev->drm, &idx))
		return;

	mipi_dbi_fb_dirty(&shadow_plane_state->data[0], fb, &rect);
	backlight_enable(dbidev->backlight);

	drm_dev_exit(idx);
}
EXPORT_SYMBOL(mipi_dbi_enable_flush);

static void mipi_dbi_blank(struct mipi_dbi_dev *dbidev)
{
	struct drm_device *drm = &dbidev->drm;
	u16 height = drm->mode_config.min_height;
	u16 width = drm->mode_config.min_width;
	struct mipi_dbi *dbi = &dbidev->dbi;
	size_t len = width * height * 2;
	int idx;

	if (!drm_dev_enter(drm, &idx))
		return;

	memset(dbidev->tx_buf, 0, len);

	mipi_dbi_set_window_address(dbidev, 0, width - 1, 0, height - 1);
	mipi_dbi_command_buf(dbi, MIPI_DCS_WRITE_MEMORY_START,
			     (u8 *)dbidev->tx_buf, len);

	drm_dev_exit(idx);
}

 
void mipi_dbi_pipe_disable(struct drm_simple_display_pipe *pipe)
{
	struct mipi_dbi_dev *dbidev = drm_to_mipi_dbi_dev(pipe->crtc.dev);

	DRM_DEBUG_KMS("\n");

	if (dbidev->backlight)
		backlight_disable(dbidev->backlight);
	else
		mipi_dbi_blank(dbidev);

	if (dbidev->regulator)
		regulator_disable(dbidev->regulator);
	if (dbidev->io_regulator)
		regulator_disable(dbidev->io_regulator);
}
EXPORT_SYMBOL(mipi_dbi_pipe_disable);

 
int mipi_dbi_pipe_begin_fb_access(struct drm_simple_display_pipe *pipe,
				  struct drm_plane_state *plane_state)
{
	return drm_gem_begin_shadow_fb_access(&pipe->plane, plane_state);
}
EXPORT_SYMBOL(mipi_dbi_pipe_begin_fb_access);

 
void mipi_dbi_pipe_end_fb_access(struct drm_simple_display_pipe *pipe,
				 struct drm_plane_state *plane_state)
{
	drm_gem_end_shadow_fb_access(&pipe->plane, plane_state);
}
EXPORT_SYMBOL(mipi_dbi_pipe_end_fb_access);

 
void mipi_dbi_pipe_reset_plane(struct drm_simple_display_pipe *pipe)
{
	drm_gem_reset_shadow_plane(&pipe->plane);
}
EXPORT_SYMBOL(mipi_dbi_pipe_reset_plane);

 
struct drm_plane_state *mipi_dbi_pipe_duplicate_plane_state(struct drm_simple_display_pipe *pipe)
{
	return drm_gem_duplicate_shadow_plane_state(&pipe->plane);
}
EXPORT_SYMBOL(mipi_dbi_pipe_duplicate_plane_state);

 
void mipi_dbi_pipe_destroy_plane_state(struct drm_simple_display_pipe *pipe,
				       struct drm_plane_state *plane_state)
{
	drm_gem_destroy_shadow_plane_state(&pipe->plane, plane_state);
}
EXPORT_SYMBOL(mipi_dbi_pipe_destroy_plane_state);

static int mipi_dbi_connector_get_modes(struct drm_connector *connector)
{
	struct mipi_dbi_dev *dbidev = drm_to_mipi_dbi_dev(connector->dev);

	return drm_connector_helper_get_modes_fixed(connector, &dbidev->mode);
}

static const struct drm_connector_helper_funcs mipi_dbi_connector_hfuncs = {
	.get_modes = mipi_dbi_connector_get_modes,
};

static const struct drm_connector_funcs mipi_dbi_connector_funcs = {
	.reset = drm_atomic_helper_connector_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int mipi_dbi_rotate_mode(struct drm_display_mode *mode,
				unsigned int rotation)
{
	if (rotation == 0 || rotation == 180) {
		return 0;
	} else if (rotation == 90 || rotation == 270) {
		swap(mode->hdisplay, mode->vdisplay);
		swap(mode->hsync_start, mode->vsync_start);
		swap(mode->hsync_end, mode->vsync_end);
		swap(mode->htotal, mode->vtotal);
		swap(mode->width_mm, mode->height_mm);
		return 0;
	} else {
		return -EINVAL;
	}
}

static const struct drm_mode_config_funcs mipi_dbi_mode_config_funcs = {
	.fb_create = drm_gem_fb_create_with_dirty,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static const uint32_t mipi_dbi_formats[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
};

 
int mipi_dbi_dev_init_with_formats(struct mipi_dbi_dev *dbidev,
				   const struct drm_simple_display_pipe_funcs *funcs,
				   const uint32_t *formats, unsigned int format_count,
				   const struct drm_display_mode *mode,
				   unsigned int rotation, size_t tx_buf_size)
{
	static const uint64_t modifiers[] = {
		DRM_FORMAT_MOD_LINEAR,
		DRM_FORMAT_MOD_INVALID
	};
	struct drm_device *drm = &dbidev->drm;
	int ret;

	if (!dbidev->dbi.command)
		return -EINVAL;

	ret = drmm_mode_config_init(drm);
	if (ret)
		return ret;

	dbidev->tx_buf = devm_kmalloc(drm->dev, tx_buf_size, GFP_KERNEL);
	if (!dbidev->tx_buf)
		return -ENOMEM;

	drm_mode_copy(&dbidev->mode, mode);
	ret = mipi_dbi_rotate_mode(&dbidev->mode, rotation);
	if (ret) {
		DRM_ERROR("Illegal rotation value %u\n", rotation);
		return -EINVAL;
	}

	drm_connector_helper_add(&dbidev->connector, &mipi_dbi_connector_hfuncs);
	ret = drm_connector_init(drm, &dbidev->connector, &mipi_dbi_connector_funcs,
				 DRM_MODE_CONNECTOR_SPI);
	if (ret)
		return ret;

	ret = drm_simple_display_pipe_init(drm, &dbidev->pipe, funcs, formats, format_count,
					   modifiers, &dbidev->connector);
	if (ret)
		return ret;

	drm_plane_enable_fb_damage_clips(&dbidev->pipe.plane);

	drm->mode_config.funcs = &mipi_dbi_mode_config_funcs;
	drm->mode_config.min_width = dbidev->mode.hdisplay;
	drm->mode_config.max_width = dbidev->mode.hdisplay;
	drm->mode_config.min_height = dbidev->mode.vdisplay;
	drm->mode_config.max_height = dbidev->mode.vdisplay;
	dbidev->rotation = rotation;

	DRM_DEBUG_KMS("rotation = %u\n", rotation);

	return 0;
}
EXPORT_SYMBOL(mipi_dbi_dev_init_with_formats);

 
int mipi_dbi_dev_init(struct mipi_dbi_dev *dbidev,
		      const struct drm_simple_display_pipe_funcs *funcs,
		      const struct drm_display_mode *mode, unsigned int rotation)
{
	size_t bufsize = mode->vdisplay * mode->hdisplay * sizeof(u16);

	dbidev->drm.mode_config.preferred_depth = 16;

	return mipi_dbi_dev_init_with_formats(dbidev, funcs, mipi_dbi_formats,
					      ARRAY_SIZE(mipi_dbi_formats), mode,
					      rotation, bufsize);
}
EXPORT_SYMBOL(mipi_dbi_dev_init);

 
void mipi_dbi_hw_reset(struct mipi_dbi *dbi)
{
	if (!dbi->reset)
		return;

	gpiod_set_value_cansleep(dbi->reset, 0);
	usleep_range(20, 1000);
	gpiod_set_value_cansleep(dbi->reset, 1);
	msleep(120);
}
EXPORT_SYMBOL(mipi_dbi_hw_reset);

 
bool mipi_dbi_display_is_on(struct mipi_dbi *dbi)
{
	u8 val;

	if (mipi_dbi_command_read(dbi, MIPI_DCS_GET_POWER_MODE, &val))
		return false;

	val &= ~DCS_POWER_MODE_RESERVED_MASK;

	 
	if (val != (DCS_POWER_MODE_DISPLAY |
	    DCS_POWER_MODE_DISPLAY_NORMAL_MODE | DCS_POWER_MODE_SLEEP_MODE))
		return false;

	DRM_DEBUG_DRIVER("Display is ON\n");

	return true;
}
EXPORT_SYMBOL(mipi_dbi_display_is_on);

static int mipi_dbi_poweron_reset_conditional(struct mipi_dbi_dev *dbidev, bool cond)
{
	struct device *dev = dbidev->drm.dev;
	struct mipi_dbi *dbi = &dbidev->dbi;
	int ret;

	if (dbidev->regulator) {
		ret = regulator_enable(dbidev->regulator);
		if (ret) {
			DRM_DEV_ERROR(dev, "Failed to enable regulator (%d)\n", ret);
			return ret;
		}
	}

	if (dbidev->io_regulator) {
		ret = regulator_enable(dbidev->io_regulator);
		if (ret) {
			DRM_DEV_ERROR(dev, "Failed to enable I/O regulator (%d)\n", ret);
			if (dbidev->regulator)
				regulator_disable(dbidev->regulator);
			return ret;
		}
	}

	if (cond && mipi_dbi_display_is_on(dbi))
		return 1;

	mipi_dbi_hw_reset(dbi);
	ret = mipi_dbi_command(dbi, MIPI_DCS_SOFT_RESET);
	if (ret) {
		DRM_DEV_ERROR(dev, "Failed to send reset command (%d)\n", ret);
		if (dbidev->regulator)
			regulator_disable(dbidev->regulator);
		if (dbidev->io_regulator)
			regulator_disable(dbidev->io_regulator);
		return ret;
	}

	 
	if (dbi->reset)
		usleep_range(5000, 20000);
	else
		msleep(120);

	return 0;
}

 
int mipi_dbi_poweron_reset(struct mipi_dbi_dev *dbidev)
{
	return mipi_dbi_poweron_reset_conditional(dbidev, false);
}
EXPORT_SYMBOL(mipi_dbi_poweron_reset);

 
int mipi_dbi_poweron_conditional_reset(struct mipi_dbi_dev *dbidev)
{
	return mipi_dbi_poweron_reset_conditional(dbidev, true);
}
EXPORT_SYMBOL(mipi_dbi_poweron_conditional_reset);

#if IS_ENABLED(CONFIG_SPI)

 
u32 mipi_dbi_spi_cmd_max_speed(struct spi_device *spi, size_t len)
{
	if (len > 64)
		return 0;  

	return min_t(u32, 10000000, spi->max_speed_hz);
}
EXPORT_SYMBOL(mipi_dbi_spi_cmd_max_speed);

static bool mipi_dbi_machine_little_endian(void)
{
#if defined(__LITTLE_ENDIAN)
	return true;
#else
	return false;
#endif
}

 

static int mipi_dbi_spi1e_transfer(struct mipi_dbi *dbi, int dc,
				   const void *buf, size_t len,
				   unsigned int bpw)
{
	bool swap_bytes = (bpw == 16 && mipi_dbi_machine_little_endian());
	size_t chunk, max_chunk = dbi->tx_buf9_len;
	struct spi_device *spi = dbi->spi;
	struct spi_transfer tr = {
		.tx_buf = dbi->tx_buf9,
		.bits_per_word = 8,
	};
	struct spi_message m;
	const u8 *src = buf;
	int i, ret;
	u8 *dst;

	if (drm_debug_enabled(DRM_UT_DRIVER))
		pr_debug("[drm:%s] dc=%d, max_chunk=%zu, transfers:\n",
			 __func__, dc, max_chunk);

	tr.speed_hz = mipi_dbi_spi_cmd_max_speed(spi, len);
	spi_message_init_with_transfers(&m, &tr, 1);

	if (!dc) {
		if (WARN_ON_ONCE(len != 1))
			return -EINVAL;

		 
		dst = dbi->tx_buf9;
		memset(dst, 0, 9);
		dst[8] = *src;
		tr.len = 9;

		return spi_sync(spi, &m);
	}

	 
	max_chunk = max_chunk / 9 * 8;
	 
	max_chunk = min(max_chunk, len);
	 
	max_chunk = max_t(size_t, 8, max_chunk & ~0x7);

	while (len) {
		size_t added = 0;

		chunk = min(len, max_chunk);
		len -= chunk;
		dst = dbi->tx_buf9;

		if (chunk < 8) {
			u8 val, carry = 0;

			 
			memset(dst, 0, 9);

			if (swap_bytes) {
				for (i = 1; i < (chunk + 1); i++) {
					val = src[1];
					*dst++ = carry | BIT(8 - i) | (val >> i);
					carry = val << (8 - i);
					i++;
					val = src[0];
					*dst++ = carry | BIT(8 - i) | (val >> i);
					carry = val << (8 - i);
					src += 2;
				}
				*dst++ = carry;
			} else {
				for (i = 1; i < (chunk + 1); i++) {
					val = *src++;
					*dst++ = carry | BIT(8 - i) | (val >> i);
					carry = val << (8 - i);
				}
				*dst++ = carry;
			}

			chunk = 8;
			added = 1;
		} else {
			for (i = 0; i < chunk; i += 8) {
				if (swap_bytes) {
					*dst++ =                 BIT(7) | (src[1] >> 1);
					*dst++ = (src[1] << 7) | BIT(6) | (src[0] >> 2);
					*dst++ = (src[0] << 6) | BIT(5) | (src[3] >> 3);
					*dst++ = (src[3] << 5) | BIT(4) | (src[2] >> 4);
					*dst++ = (src[2] << 4) | BIT(3) | (src[5] >> 5);
					*dst++ = (src[5] << 3) | BIT(2) | (src[4] >> 6);
					*dst++ = (src[4] << 2) | BIT(1) | (src[7] >> 7);
					*dst++ = (src[7] << 1) | BIT(0);
					*dst++ = src[6];
				} else {
					*dst++ =                 BIT(7) | (src[0] >> 1);
					*dst++ = (src[0] << 7) | BIT(6) | (src[1] >> 2);
					*dst++ = (src[1] << 6) | BIT(5) | (src[2] >> 3);
					*dst++ = (src[2] << 5) | BIT(4) | (src[3] >> 4);
					*dst++ = (src[3] << 4) | BIT(3) | (src[4] >> 5);
					*dst++ = (src[4] << 3) | BIT(2) | (src[5] >> 6);
					*dst++ = (src[5] << 2) | BIT(1) | (src[6] >> 7);
					*dst++ = (src[6] << 1) | BIT(0);
					*dst++ = src[7];
				}

				src += 8;
				added++;
			}
		}

		tr.len = chunk + added;

		ret = spi_sync(spi, &m);
		if (ret)
			return ret;
	}

	return 0;
}

static int mipi_dbi_spi1_transfer(struct mipi_dbi *dbi, int dc,
				  const void *buf, size_t len,
				  unsigned int bpw)
{
	struct spi_device *spi = dbi->spi;
	struct spi_transfer tr = {
		.bits_per_word = 9,
	};
	const u16 *src16 = buf;
	const u8 *src8 = buf;
	struct spi_message m;
	size_t max_chunk;
	u16 *dst16;
	int ret;

	if (!spi_is_bpw_supported(spi, 9))
		return mipi_dbi_spi1e_transfer(dbi, dc, buf, len, bpw);

	tr.speed_hz = mipi_dbi_spi_cmd_max_speed(spi, len);
	max_chunk = dbi->tx_buf9_len;
	dst16 = dbi->tx_buf9;

	if (drm_debug_enabled(DRM_UT_DRIVER))
		pr_debug("[drm:%s] dc=%d, max_chunk=%zu, transfers:\n",
			 __func__, dc, max_chunk);

	max_chunk = min(max_chunk / 2, len);

	spi_message_init_with_transfers(&m, &tr, 1);
	tr.tx_buf = dst16;

	while (len) {
		size_t chunk = min(len, max_chunk);
		unsigned int i;

		if (bpw == 16 && mipi_dbi_machine_little_endian()) {
			for (i = 0; i < (chunk * 2); i += 2) {
				dst16[i]     = *src16 >> 8;
				dst16[i + 1] = *src16++ & 0xFF;
				if (dc) {
					dst16[i]     |= 0x0100;
					dst16[i + 1] |= 0x0100;
				}
			}
		} else {
			for (i = 0; i < chunk; i++) {
				dst16[i] = *src8++;
				if (dc)
					dst16[i] |= 0x0100;
			}
		}

		tr.len = chunk * 2;
		len -= chunk;

		ret = spi_sync(spi, &m);
		if (ret)
			return ret;
	}

	return 0;
}

static int mipi_dbi_typec1_command_read(struct mipi_dbi *dbi, u8 *cmd,
					u8 *data, size_t len)
{
	struct spi_device *spi = dbi->spi;
	u32 speed_hz = min_t(u32, MIPI_DBI_MAX_SPI_READ_SPEED,
			     spi->max_speed_hz / 2);
	struct spi_transfer tr[2] = {
		{
			.speed_hz = speed_hz,
			.bits_per_word = 9,
			.tx_buf = dbi->tx_buf9,
			.len = 2,
		}, {
			.speed_hz = speed_hz,
			.bits_per_word = 8,
			.len = len,
			.rx_buf = data,
		},
	};
	struct spi_message m;
	u16 *dst16;
	int ret;

	if (!len)
		return -EINVAL;

	if (!spi_is_bpw_supported(spi, 9)) {
		 
		dev_err(&spi->dev,
			"reading on host not supporting 9 bpw not yet implemented\n");
		return -EOPNOTSUPP;
	}

	 
	dst16 = dbi->tx_buf9;
	dst16[0] = *cmd;

	spi_message_init_with_transfers(&m, tr, ARRAY_SIZE(tr));
	ret = spi_sync(spi, &m);

	if (!ret)
		MIPI_DBI_DEBUG_COMMAND(*cmd, data, len);

	return ret;
}

static int mipi_dbi_typec1_command(struct mipi_dbi *dbi, u8 *cmd,
				   u8 *parameters, size_t num)
{
	unsigned int bpw = (*cmd == MIPI_DCS_WRITE_MEMORY_START) ? 16 : 8;
	int ret;

	if (mipi_dbi_command_is_read(dbi, *cmd))
		return mipi_dbi_typec1_command_read(dbi, cmd, parameters, num);

	MIPI_DBI_DEBUG_COMMAND(*cmd, parameters, num);

	ret = mipi_dbi_spi1_transfer(dbi, 0, cmd, 1, 8);
	if (ret || !num)
		return ret;

	return mipi_dbi_spi1_transfer(dbi, 1, parameters, num, bpw);
}

 

static int mipi_dbi_typec3_command_read(struct mipi_dbi *dbi, u8 *cmd,
					u8 *data, size_t len)
{
	struct spi_device *spi = dbi->spi;
	u32 speed_hz = min_t(u32, MIPI_DBI_MAX_SPI_READ_SPEED,
			     spi->max_speed_hz / 2);
	struct spi_transfer tr[2] = {
		{
			.speed_hz = speed_hz,
			.tx_buf = cmd,
			.len = 1,
		}, {
			.speed_hz = speed_hz,
			.len = len,
		},
	};
	struct spi_message m;
	u8 *buf;
	int ret;

	if (!len)
		return -EINVAL;

	 
	if (*cmd == MIPI_DCS_GET_DISPLAY_ID ||
	    *cmd == MIPI_DCS_GET_DISPLAY_STATUS) {
		if (!(len == 3 || len == 4))
			return -EINVAL;

		tr[1].len = len + 1;
	}

	buf = kmalloc(tr[1].len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	tr[1].rx_buf = buf;

	spi_bus_lock(spi->controller);
	gpiod_set_value_cansleep(dbi->dc, 0);

	spi_message_init_with_transfers(&m, tr, ARRAY_SIZE(tr));
	ret = spi_sync_locked(spi, &m);
	spi_bus_unlock(spi->controller);
	if (ret)
		goto err_free;

	if (tr[1].len == len) {
		memcpy(data, buf, len);
	} else {
		unsigned int i;

		for (i = 0; i < len; i++)
			data[i] = (buf[i] << 1) | (buf[i + 1] >> 7);
	}

	MIPI_DBI_DEBUG_COMMAND(*cmd, data, len);

err_free:
	kfree(buf);

	return ret;
}

static int mipi_dbi_typec3_command(struct mipi_dbi *dbi, u8 *cmd,
				   u8 *par, size_t num)
{
	struct spi_device *spi = dbi->spi;
	unsigned int bpw = 8;
	u32 speed_hz;
	int ret;

	if (mipi_dbi_command_is_read(dbi, *cmd))
		return mipi_dbi_typec3_command_read(dbi, cmd, par, num);

	MIPI_DBI_DEBUG_COMMAND(*cmd, par, num);

	spi_bus_lock(spi->controller);
	gpiod_set_value_cansleep(dbi->dc, 0);
	speed_hz = mipi_dbi_spi_cmd_max_speed(spi, 1);
	ret = mipi_dbi_spi_transfer(spi, speed_hz, 8, cmd, 1);
	spi_bus_unlock(spi->controller);
	if (ret || !num)
		return ret;

	if (*cmd == MIPI_DCS_WRITE_MEMORY_START && !dbi->swap_bytes)
		bpw = 16;

	spi_bus_lock(spi->controller);
	gpiod_set_value_cansleep(dbi->dc, 1);
	speed_hz = mipi_dbi_spi_cmd_max_speed(spi, num);
	ret = mipi_dbi_spi_transfer(spi, speed_hz, bpw, par, num);
	spi_bus_unlock(spi->controller);

	return ret;
}

 
int mipi_dbi_spi_init(struct spi_device *spi, struct mipi_dbi *dbi,
		      struct gpio_desc *dc)
{
	struct device *dev = &spi->dev;
	int ret;

	 
	if (!dev->coherent_dma_mask) {
		ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(32));
		if (ret) {
			dev_warn(dev, "Failed to set dma mask %d\n", ret);
			return ret;
		}
	}

	dbi->spi = spi;
	dbi->read_commands = mipi_dbi_dcs_read_commands;

	if (dc) {
		dbi->command = mipi_dbi_typec3_command;
		dbi->dc = dc;
		if (mipi_dbi_machine_little_endian() && !spi_is_bpw_supported(spi, 16))
			dbi->swap_bytes = true;
	} else {
		dbi->command = mipi_dbi_typec1_command;
		dbi->tx_buf9_len = SZ_16K;
		dbi->tx_buf9 = devm_kmalloc(dev, dbi->tx_buf9_len, GFP_KERNEL);
		if (!dbi->tx_buf9)
			return -ENOMEM;
	}

	mutex_init(&dbi->cmdlock);

	DRM_DEBUG_DRIVER("SPI speed: %uMHz\n", spi->max_speed_hz / 1000000);

	return 0;
}
EXPORT_SYMBOL(mipi_dbi_spi_init);

 
int mipi_dbi_spi_transfer(struct spi_device *spi, u32 speed_hz,
			  u8 bpw, const void *buf, size_t len)
{
	size_t max_chunk = spi_max_transfer_size(spi);
	struct spi_transfer tr = {
		.bits_per_word = bpw,
		.speed_hz = speed_hz,
	};
	struct spi_message m;
	size_t chunk;
	int ret;

	 
	max_chunk = ALIGN_DOWN(max_chunk, 2);

	spi_message_init_with_transfers(&m, &tr, 1);

	while (len) {
		chunk = min(len, max_chunk);

		tr.tx_buf = buf;
		tr.len = chunk;
		buf += chunk;
		len -= chunk;

		ret = spi_sync_locked(spi, &m);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(mipi_dbi_spi_transfer);

#endif  

#ifdef CONFIG_DEBUG_FS

static ssize_t mipi_dbi_debugfs_command_write(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct mipi_dbi_dev *dbidev = m->private;
	u8 val, cmd = 0, parameters[64];
	char *buf, *pos, *token;
	int i, ret, idx;

	if (!drm_dev_enter(&dbidev->drm, &idx))
		return -ENODEV;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto err_exit;
	}

	 
	for (i = count - 1; i > 0; i--)
		if (isspace(buf[i]))
			buf[i] = '\0';
		else
			break;
	i = 0;
	pos = buf;
	while (pos) {
		token = strsep(&pos, " ");
		if (!token) {
			ret = -EINVAL;
			goto err_free;
		}

		ret = kstrtou8(token, 16, &val);
		if (ret < 0)
			goto err_free;

		if (token == buf)
			cmd = val;
		else
			parameters[i++] = val;

		if (i == 64) {
			ret = -E2BIG;
			goto err_free;
		}
	}

	ret = mipi_dbi_command_buf(&dbidev->dbi, cmd, parameters, i);

err_free:
	kfree(buf);
err_exit:
	drm_dev_exit(idx);

	return ret < 0 ? ret : count;
}

static int mipi_dbi_debugfs_command_show(struct seq_file *m, void *unused)
{
	struct mipi_dbi_dev *dbidev = m->private;
	struct mipi_dbi *dbi = &dbidev->dbi;
	u8 cmd, val[4];
	int ret, idx;
	size_t len;

	if (!drm_dev_enter(&dbidev->drm, &idx))
		return -ENODEV;

	for (cmd = 0; cmd < 255; cmd++) {
		if (!mipi_dbi_command_is_read(dbi, cmd))
			continue;

		switch (cmd) {
		case MIPI_DCS_READ_MEMORY_START:
		case MIPI_DCS_READ_MEMORY_CONTINUE:
			len = 2;
			break;
		case MIPI_DCS_GET_DISPLAY_ID:
			len = 3;
			break;
		case MIPI_DCS_GET_DISPLAY_STATUS:
			len = 4;
			break;
		default:
			len = 1;
			break;
		}

		seq_printf(m, "%02x: ", cmd);
		ret = mipi_dbi_command_buf(dbi, cmd, val, len);
		if (ret) {
			seq_puts(m, "XX\n");
			continue;
		}
		seq_printf(m, "%*phN\n", (int)len, val);
	}

	drm_dev_exit(idx);

	return 0;
}

static int mipi_dbi_debugfs_command_open(struct inode *inode,
					 struct file *file)
{
	return single_open(file, mipi_dbi_debugfs_command_show,
			   inode->i_private);
}

static const struct file_operations mipi_dbi_debugfs_command_fops = {
	.owner = THIS_MODULE,
	.open = mipi_dbi_debugfs_command_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mipi_dbi_debugfs_command_write,
};

 
void mipi_dbi_debugfs_init(struct drm_minor *minor)
{
	struct mipi_dbi_dev *dbidev = drm_to_mipi_dbi_dev(minor->dev);
	umode_t mode = S_IFREG | S_IWUSR;

	if (dbidev->dbi.read_commands)
		mode |= S_IRUGO;
	debugfs_create_file("command", mode, minor->debugfs_root, dbidev,
			    &mipi_dbi_debugfs_command_fops);
}
EXPORT_SYMBOL(mipi_dbi_debugfs_init);

#endif

MODULE_LICENSE("GPL");
