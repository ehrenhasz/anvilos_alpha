 

#include <drm/amdgpu_drm.h>
#include "amdgpu.h"
#include "amdgpu_i2c.h"
#include "atom.h"
#include "amdgpu_connectors.h"
#include "amdgpu_display.h"
#include "soc15_common.h"
#include "gc/gc_11_0_0_offset.h"
#include "gc/gc_11_0_0_sh_mask.h"
#include <asm/div64.h>

#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_vblank.h>

 
void amdgpu_display_hotplug_work_func(struct work_struct *work)
{
	struct amdgpu_device *adev = container_of(work, struct amdgpu_device,
						  hotplug_work.work);
	struct drm_device *dev = adev_to_drm(adev);
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct drm_connector *connector;
	struct drm_connector_list_iter iter;

	mutex_lock(&mode_config->mutex);
	drm_connector_list_iter_begin(dev, &iter);
	drm_for_each_connector_iter(connector, &iter)
		amdgpu_connector_hotplug(connector);
	drm_connector_list_iter_end(&iter);
	mutex_unlock(&mode_config->mutex);
	 
	drm_helper_hpd_irq_event(dev);
}

static int amdgpu_display_framebuffer_init(struct drm_device *dev,
					   struct amdgpu_framebuffer *rfb,
					   const struct drm_mode_fb_cmd2 *mode_cmd,
					   struct drm_gem_object *obj);

static void amdgpu_display_flip_callback(struct dma_fence *f,
					 struct dma_fence_cb *cb)
{
	struct amdgpu_flip_work *work =
		container_of(cb, struct amdgpu_flip_work, cb);

	dma_fence_put(f);
	schedule_work(&work->flip_work.work);
}

static bool amdgpu_display_flip_handle_fence(struct amdgpu_flip_work *work,
					     struct dma_fence **f)
{
	struct dma_fence *fence = *f;

	if (fence == NULL)
		return false;

	*f = NULL;

	if (!dma_fence_add_callback(fence, &work->cb,
				    amdgpu_display_flip_callback))
		return true;

	dma_fence_put(fence);
	return false;
}

static void amdgpu_display_flip_work_func(struct work_struct *__work)
{
	struct delayed_work *delayed_work =
		container_of(__work, struct delayed_work, work);
	struct amdgpu_flip_work *work =
		container_of(delayed_work, struct amdgpu_flip_work, flip_work);
	struct amdgpu_device *adev = work->adev;
	struct amdgpu_crtc *amdgpu_crtc = adev->mode_info.crtcs[work->crtc_id];

	struct drm_crtc *crtc = &amdgpu_crtc->base;
	unsigned long flags;
	unsigned int i;
	int vpos, hpos;

	for (i = 0; i < work->shared_count; ++i)
		if (amdgpu_display_flip_handle_fence(work, &work->shared[i]))
			return;

	 
	if (amdgpu_crtc->enabled &&
	    (amdgpu_display_get_crtc_scanoutpos(adev_to_drm(adev), work->crtc_id, 0,
						&vpos, &hpos, NULL, NULL,
						&crtc->hwmode)
	     & (DRM_SCANOUTPOS_VALID | DRM_SCANOUTPOS_IN_VBLANK)) ==
	    (DRM_SCANOUTPOS_VALID | DRM_SCANOUTPOS_IN_VBLANK) &&
	    (int)(work->target_vblank -
		  amdgpu_get_vblank_counter_kms(crtc)) > 0) {
		schedule_delayed_work(&work->flip_work, usecs_to_jiffies(1000));
		return;
	}

	 
	spin_lock_irqsave(&crtc->dev->event_lock, flags);

	 
	adev->mode_info.funcs->page_flip(adev, work->crtc_id, work->base, work->async);

	 
	amdgpu_crtc->pflip_status = AMDGPU_FLIP_SUBMITTED;
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);


	drm_dbg_vbl(adev_to_drm(adev),
		    "crtc:%d[%p], pflip_stat:AMDGPU_FLIP_SUBMITTED, work: %p,\n",
		    amdgpu_crtc->crtc_id, amdgpu_crtc, work);

}

 
static void amdgpu_display_unpin_work_func(struct work_struct *__work)
{
	struct amdgpu_flip_work *work =
		container_of(__work, struct amdgpu_flip_work, unpin_work);
	int r;

	 
	r = amdgpu_bo_reserve(work->old_abo, true);
	if (likely(r == 0)) {
		amdgpu_bo_unpin(work->old_abo);
		amdgpu_bo_unreserve(work->old_abo);
	} else
		DRM_ERROR("failed to reserve buffer after flip\n");

	amdgpu_bo_unref(&work->old_abo);
	kfree(work->shared);
	kfree(work);
}

int amdgpu_display_crtc_page_flip_target(struct drm_crtc *crtc,
				struct drm_framebuffer *fb,
				struct drm_pending_vblank_event *event,
				uint32_t page_flip_flags, uint32_t target,
				struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_device *dev = crtc->dev;
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct amdgpu_crtc *amdgpu_crtc = to_amdgpu_crtc(crtc);
	struct drm_gem_object *obj;
	struct amdgpu_flip_work *work;
	struct amdgpu_bo *new_abo;
	unsigned long flags;
	u64 tiling_flags;
	int i, r;

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (work == NULL)
		return -ENOMEM;

	INIT_DELAYED_WORK(&work->flip_work, amdgpu_display_flip_work_func);
	INIT_WORK(&work->unpin_work, amdgpu_display_unpin_work_func);

	work->event = event;
	work->adev = adev;
	work->crtc_id = amdgpu_crtc->crtc_id;
	work->async = (page_flip_flags & DRM_MODE_PAGE_FLIP_ASYNC) != 0;

	 
	obj = crtc->primary->fb->obj[0];

	 
	work->old_abo = gem_to_amdgpu_bo(obj);
	amdgpu_bo_ref(work->old_abo);

	obj = fb->obj[0];
	new_abo = gem_to_amdgpu_bo(obj);

	 
	r = amdgpu_bo_reserve(new_abo, false);
	if (unlikely(r != 0)) {
		DRM_ERROR("failed to reserve new abo buffer before flip\n");
		goto cleanup;
	}

	if (!adev->enable_virtual_display) {
		r = amdgpu_bo_pin(new_abo,
				  amdgpu_display_supported_domains(adev, new_abo->flags));
		if (unlikely(r != 0)) {
			DRM_ERROR("failed to pin new abo buffer before flip\n");
			goto unreserve;
		}
	}

	r = amdgpu_ttm_alloc_gart(&new_abo->tbo);
	if (unlikely(r != 0)) {
		DRM_ERROR("%p bind failed\n", new_abo);
		goto unpin;
	}

	r = dma_resv_get_fences(new_abo->tbo.base.resv, DMA_RESV_USAGE_WRITE,
				&work->shared_count,
				&work->shared);
	if (unlikely(r != 0)) {
		DRM_ERROR("failed to get fences for buffer\n");
		goto unpin;
	}

	amdgpu_bo_get_tiling_flags(new_abo, &tiling_flags);
	amdgpu_bo_unreserve(new_abo);

	if (!adev->enable_virtual_display)
		work->base = amdgpu_bo_gpu_offset(new_abo);
	work->target_vblank = target - (uint32_t)drm_crtc_vblank_count(crtc) +
		amdgpu_get_vblank_counter_kms(crtc);

	 
	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (amdgpu_crtc->pflip_status != AMDGPU_FLIP_NONE) {
		DRM_DEBUG_DRIVER("flip queue: crtc already busy\n");
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
		r = -EBUSY;
		goto pflip_cleanup;
	}

	amdgpu_crtc->pflip_status = AMDGPU_FLIP_PENDING;
	amdgpu_crtc->pflip_works = work;


	DRM_DEBUG_DRIVER("crtc:%d[%p], pflip_stat:AMDGPU_FLIP_PENDING, work: %p,\n",
					 amdgpu_crtc->crtc_id, amdgpu_crtc, work);
	 
	crtc->primary->fb = fb;
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	amdgpu_display_flip_work_func(&work->flip_work.work);
	return 0;

pflip_cleanup:
	if (unlikely(amdgpu_bo_reserve(new_abo, false) != 0)) {
		DRM_ERROR("failed to reserve new abo in error path\n");
		goto cleanup;
	}
unpin:
	if (!adev->enable_virtual_display)
		amdgpu_bo_unpin(new_abo);

unreserve:
	amdgpu_bo_unreserve(new_abo);

cleanup:
	amdgpu_bo_unref(&work->old_abo);
	for (i = 0; i < work->shared_count; ++i)
		dma_fence_put(work->shared[i]);
	kfree(work->shared);
	kfree(work);

	return r;
}

int amdgpu_display_crtc_set_config(struct drm_mode_set *set,
				   struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_device *dev;
	struct amdgpu_device *adev;
	struct drm_crtc *crtc;
	bool active = false;
	int ret;

	if (!set || !set->crtc)
		return -EINVAL;

	dev = set->crtc->dev;

	ret = pm_runtime_get_sync(dev->dev);
	if (ret < 0)
		goto out;

	ret = drm_crtc_helper_set_config(set, ctx);

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		if (crtc->enabled)
			active = true;

	pm_runtime_mark_last_busy(dev->dev);

	adev = drm_to_adev(dev);
	 
	if (active && !adev->have_disp_power_ref) {
		adev->have_disp_power_ref = true;
		return ret;
	}
	 
	if (!active && adev->have_disp_power_ref)
		adev->have_disp_power_ref = false;
out:
	 
	pm_runtime_put_autosuspend(dev->dev);
	return ret;
}

static const char *encoder_names[41] = {
	"NONE",
	"INTERNAL_LVDS",
	"INTERNAL_TMDS1",
	"INTERNAL_TMDS2",
	"INTERNAL_DAC1",
	"INTERNAL_DAC2",
	"INTERNAL_SDVOA",
	"INTERNAL_SDVOB",
	"SI170B",
	"CH7303",
	"CH7301",
	"INTERNAL_DVO1",
	"EXTERNAL_SDVOA",
	"EXTERNAL_SDVOB",
	"TITFP513",
	"INTERNAL_LVTM1",
	"VT1623",
	"HDMI_SI1930",
	"HDMI_INTERNAL",
	"INTERNAL_KLDSCP_TMDS1",
	"INTERNAL_KLDSCP_DVO1",
	"INTERNAL_KLDSCP_DAC1",
	"INTERNAL_KLDSCP_DAC2",
	"SI178",
	"MVPU_FPGA",
	"INTERNAL_DDI",
	"VT1625",
	"HDMI_SI1932",
	"DP_AN9801",
	"DP_DP501",
	"INTERNAL_UNIPHY",
	"INTERNAL_KLDSCP_LVTMA",
	"INTERNAL_UNIPHY1",
	"INTERNAL_UNIPHY2",
	"NUTMEG",
	"TRAVIS",
	"INTERNAL_VCE",
	"INTERNAL_UNIPHY3",
	"HDMI_ANX9805",
	"INTERNAL_AMCLK",
	"VIRTUAL",
};

static const char *hpd_names[6] = {
	"HPD1",
	"HPD2",
	"HPD3",
	"HPD4",
	"HPD5",
	"HPD6",
};

void amdgpu_display_print_display_setup(struct drm_device *dev)
{
	struct drm_connector *connector;
	struct amdgpu_connector *amdgpu_connector;
	struct drm_encoder *encoder;
	struct amdgpu_encoder *amdgpu_encoder;
	struct drm_connector_list_iter iter;
	uint32_t devices;
	int i = 0;

	drm_connector_list_iter_begin(dev, &iter);
	DRM_INFO("AMDGPU Display Connectors\n");
	drm_for_each_connector_iter(connector, &iter) {
		amdgpu_connector = to_amdgpu_connector(connector);
		DRM_INFO("Connector %d:\n", i);
		DRM_INFO("  %s\n", connector->name);
		if (amdgpu_connector->hpd.hpd != AMDGPU_HPD_NONE)
			DRM_INFO("  %s\n", hpd_names[amdgpu_connector->hpd.hpd]);
		if (amdgpu_connector->ddc_bus) {
			DRM_INFO("  DDC: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				 amdgpu_connector->ddc_bus->rec.mask_clk_reg,
				 amdgpu_connector->ddc_bus->rec.mask_data_reg,
				 amdgpu_connector->ddc_bus->rec.a_clk_reg,
				 amdgpu_connector->ddc_bus->rec.a_data_reg,
				 amdgpu_connector->ddc_bus->rec.en_clk_reg,
				 amdgpu_connector->ddc_bus->rec.en_data_reg,
				 amdgpu_connector->ddc_bus->rec.y_clk_reg,
				 amdgpu_connector->ddc_bus->rec.y_data_reg);
			if (amdgpu_connector->router.ddc_valid)
				DRM_INFO("  DDC Router 0x%x/0x%x\n",
					 amdgpu_connector->router.ddc_mux_control_pin,
					 amdgpu_connector->router.ddc_mux_state);
			if (amdgpu_connector->router.cd_valid)
				DRM_INFO("  Clock/Data Router 0x%x/0x%x\n",
					 amdgpu_connector->router.cd_mux_control_pin,
					 amdgpu_connector->router.cd_mux_state);
		} else {
			if (connector->connector_type == DRM_MODE_CONNECTOR_VGA ||
			    connector->connector_type == DRM_MODE_CONNECTOR_DVII ||
			    connector->connector_type == DRM_MODE_CONNECTOR_DVID ||
			    connector->connector_type == DRM_MODE_CONNECTOR_DVIA ||
			    connector->connector_type == DRM_MODE_CONNECTOR_HDMIA ||
			    connector->connector_type == DRM_MODE_CONNECTOR_HDMIB)
				DRM_INFO("  DDC: no ddc bus - possible BIOS bug - please report to xorg-driver-ati@lists.x.org\n");
		}
		DRM_INFO("  Encoders:\n");
		list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
			amdgpu_encoder = to_amdgpu_encoder(encoder);
			devices = amdgpu_encoder->devices & amdgpu_connector->devices;
			if (devices) {
				if (devices & ATOM_DEVICE_CRT1_SUPPORT)
					DRM_INFO("    CRT1: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_CRT2_SUPPORT)
					DRM_INFO("    CRT2: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_LCD1_SUPPORT)
					DRM_INFO("    LCD1: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_DFP1_SUPPORT)
					DRM_INFO("    DFP1: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_DFP2_SUPPORT)
					DRM_INFO("    DFP2: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_DFP3_SUPPORT)
					DRM_INFO("    DFP3: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_DFP4_SUPPORT)
					DRM_INFO("    DFP4: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_DFP5_SUPPORT)
					DRM_INFO("    DFP5: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_DFP6_SUPPORT)
					DRM_INFO("    DFP6: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_TV1_SUPPORT)
					DRM_INFO("    TV1: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
				if (devices & ATOM_DEVICE_CV_SUPPORT)
					DRM_INFO("    CV: %s\n", encoder_names[amdgpu_encoder->encoder_id]);
			}
		}
		i++;
	}
	drm_connector_list_iter_end(&iter);
}

bool amdgpu_display_ddc_probe(struct amdgpu_connector *amdgpu_connector,
			      bool use_aux)
{
	u8 out = 0x0;
	u8 buf[8];
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr = DDC_ADDR,
			.flags = 0,
			.len = 1,
			.buf = &out,
		},
		{
			.addr = DDC_ADDR,
			.flags = I2C_M_RD,
			.len = 8,
			.buf = buf,
		}
	};

	 
	if (amdgpu_connector->router.ddc_valid)
		amdgpu_i2c_router_select_ddc_port(amdgpu_connector);

	if (use_aux)
		ret = i2c_transfer(&amdgpu_connector->ddc_bus->aux.ddc, msgs, 2);
	else
		ret = i2c_transfer(&amdgpu_connector->ddc_bus->adapter, msgs, 2);

	if (ret != 2)
		 
		return false;
	 
	if (drm_edid_header_is_valid(buf) < 6) {
		 
		return false;
	}
	return true;
}

static int amdgpu_dirtyfb(struct drm_framebuffer *fb, struct drm_file *file,
			  unsigned int flags, unsigned int color,
			  struct drm_clip_rect *clips, unsigned int num_clips)
{

	if (file)
		return -ENOSYS;

	return drm_atomic_helper_dirtyfb(fb, file, flags, color, clips,
					 num_clips);
}

static const struct drm_framebuffer_funcs amdgpu_fb_funcs = {
	.destroy = drm_gem_fb_destroy,
	.create_handle = drm_gem_fb_create_handle,
};

static const struct drm_framebuffer_funcs amdgpu_fb_funcs_atomic = {
	.destroy = drm_gem_fb_destroy,
	.create_handle = drm_gem_fb_create_handle,
	.dirty = amdgpu_dirtyfb
};

uint32_t amdgpu_display_supported_domains(struct amdgpu_device *adev,
					  uint64_t bo_flags)
{
	uint32_t domain = AMDGPU_GEM_DOMAIN_VRAM;

#if defined(CONFIG_DRM_AMD_DC)
	 
	if ((bo_flags & AMDGPU_GEM_CREATE_CPU_GTT_USWC) &&
	    amdgpu_bo_support_uswc(bo_flags) &&
	    adev->dc_enabled &&
	    adev->mode_info.gpu_vm_support)
		domain |= AMDGPU_GEM_DOMAIN_GTT;
#endif

	return domain;
}

static const struct drm_format_info dcc_formats[] = {
	{ .format = DRM_FORMAT_XRGB8888, .depth = 24, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
	 { .format = DRM_FORMAT_XBGR8888, .depth = 24, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
	{ .format = DRM_FORMAT_ARGB8888, .depth = 32, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	   .has_alpha = true, },
	{ .format = DRM_FORMAT_ABGR8888, .depth = 32, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	  .has_alpha = true, },
	{ .format = DRM_FORMAT_BGRA8888, .depth = 32, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	  .has_alpha = true, },
	{ .format = DRM_FORMAT_XRGB2101010, .depth = 30, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
	{ .format = DRM_FORMAT_XBGR2101010, .depth = 30, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
	{ .format = DRM_FORMAT_ARGB2101010, .depth = 30, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	  .has_alpha = true, },
	{ .format = DRM_FORMAT_ABGR2101010, .depth = 30, .num_planes = 2,
	  .cpp = { 4, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	  .has_alpha = true, },
	{ .format = DRM_FORMAT_RGB565, .depth = 16, .num_planes = 2,
	  .cpp = { 2, 0, }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
};

static const struct drm_format_info dcc_retile_formats[] = {
	{ .format = DRM_FORMAT_XRGB8888, .depth = 24, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
	 { .format = DRM_FORMAT_XBGR8888, .depth = 24, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
	{ .format = DRM_FORMAT_ARGB8888, .depth = 32, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	   .has_alpha = true, },
	{ .format = DRM_FORMAT_ABGR8888, .depth = 32, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	  .has_alpha = true, },
	{ .format = DRM_FORMAT_BGRA8888, .depth = 32, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	  .has_alpha = true, },
	{ .format = DRM_FORMAT_XRGB2101010, .depth = 30, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
	{ .format = DRM_FORMAT_XBGR2101010, .depth = 30, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
	{ .format = DRM_FORMAT_ARGB2101010, .depth = 30, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	  .has_alpha = true, },
	{ .format = DRM_FORMAT_ABGR2101010, .depth = 30, .num_planes = 3,
	  .cpp = { 4, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1,
	  .has_alpha = true, },
	{ .format = DRM_FORMAT_RGB565, .depth = 16, .num_planes = 3,
	  .cpp = { 2, 0, 0 }, .block_w = {1, 1, 1}, .block_h = {1, 1, 1}, .hsub = 1, .vsub = 1, },
};

static const struct drm_format_info *
lookup_format_info(const struct drm_format_info formats[],
		  int num_formats, u32 format)
{
	int i;

	for (i = 0; i < num_formats; i++) {
		if (formats[i].format == format)
			return &formats[i];
	}

	return NULL;
}

const struct drm_format_info *
amdgpu_lookup_format_info(u32 format, uint64_t modifier)
{
	if (!IS_AMD_FMT_MOD(modifier))
		return NULL;

	if (AMD_FMT_MOD_GET(DCC_RETILE, modifier))
		return lookup_format_info(dcc_retile_formats,
					  ARRAY_SIZE(dcc_retile_formats),
					  format);

	if (AMD_FMT_MOD_GET(DCC, modifier))
		return lookup_format_info(dcc_formats, ARRAY_SIZE(dcc_formats),
					  format);

	 
	return NULL;
}


 
static int
extract_render_dcc_offset(struct amdgpu_device *adev,
			  struct drm_gem_object *obj,
			  uint64_t *offset)
{
	struct amdgpu_bo *rbo;
	int r = 0;
	uint32_t metadata[10];  
	uint32_t size;

	rbo = gem_to_amdgpu_bo(obj);
	r = amdgpu_bo_reserve(rbo, false);

	if (unlikely(r)) {
		 
		if (r != -ERESTARTSYS)
			DRM_ERROR("Unable to reserve buffer: %d\n", r);
		return r;
	}

	r = amdgpu_bo_get_metadata(rbo, metadata, sizeof(metadata), &size, NULL);
	amdgpu_bo_unreserve(rbo);

	if (r)
		return r;

	 
	if (size < 40  || metadata[0] != 1)
		return -EINVAL;

	if (adev->family >= AMDGPU_FAMILY_NV) {
		 
		*offset = ((u64)metadata[9] << 16u) |
			  ((metadata[8] & 0xFF000000u) >> 16);
	} else {
		 
		*offset = ((u64)metadata[9] << 8u) |
			  ((u64)(metadata[7] & 0x1FE0000u) << 23);
	}

	return 0;
}

static int convert_tiling_flags_to_modifier(struct amdgpu_framebuffer *afb)
{
	struct amdgpu_device *adev = drm_to_adev(afb->base.dev);
	uint64_t modifier = 0;
	int num_pipes = 0;
	int num_pkrs = 0;

	num_pkrs = adev->gfx.config.gb_addr_config_fields.num_pkrs;
	num_pipes = adev->gfx.config.gb_addr_config_fields.num_pipes;

	if (!afb->tiling_flags || !AMDGPU_TILING_GET(afb->tiling_flags, SWIZZLE_MODE)) {
		modifier = DRM_FORMAT_MOD_LINEAR;
	} else {
		int swizzle = AMDGPU_TILING_GET(afb->tiling_flags, SWIZZLE_MODE);
		bool has_xor = swizzle >= 16;
		int block_size_bits;
		int version;
		int pipe_xor_bits = 0;
		int bank_xor_bits = 0;
		int packers = 0;
		int rb = 0;
		int pipes = ilog2(num_pipes);
		uint32_t dcc_offset = AMDGPU_TILING_GET(afb->tiling_flags, DCC_OFFSET_256B);

		switch (swizzle >> 2) {
		case 0:  
			block_size_bits = 8;
			break;
		case 1:  
		case 5:  
			block_size_bits = 12;
			break;
		case 2:  
		case 4:  
		case 6:  
			block_size_bits = 16;
			break;
		case 7:  
			block_size_bits = 18;
			break;
		default:
			 
			return -EINVAL;
		}

		if (adev->ip_versions[GC_HWIP][0] >= IP_VERSION(11, 0, 0))
			version = AMD_FMT_MOD_TILE_VER_GFX11;
		else if (adev->ip_versions[GC_HWIP][0] >= IP_VERSION(10, 3, 0))
			version = AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS;
		else if (adev->ip_versions[GC_HWIP][0] >= IP_VERSION(10, 0, 0))
			version = AMD_FMT_MOD_TILE_VER_GFX10;
		else
			version = AMD_FMT_MOD_TILE_VER_GFX9;

		switch (swizzle & 3) {
		case 0:  
			return -EINVAL;
		case 1:  
			if (adev->ip_versions[GC_HWIP][0] < IP_VERSION(11, 0, 0)) {
				if (!has_xor)
					version = AMD_FMT_MOD_TILE_VER_GFX9;
			}
			break;
		case 2:
			if (adev->ip_versions[GC_HWIP][0] < IP_VERSION(11, 0, 0)) {
				if (!has_xor && afb->base.format->cpp[0] != 4)
					version = AMD_FMT_MOD_TILE_VER_GFX9;
			}
			break;
		case 3:
			break;
		}

		if (has_xor) {
			if (num_pipes == num_pkrs && num_pkrs == 0) {
				DRM_ERROR("invalid number of pipes and packers\n");
				return -EINVAL;
			}

			switch (version) {
			case AMD_FMT_MOD_TILE_VER_GFX11:
				pipe_xor_bits = min(block_size_bits - 8, pipes);
				packers = ilog2(adev->gfx.config.gb_addr_config_fields.num_pkrs);
				break;
			case AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS:
				pipe_xor_bits = min(block_size_bits - 8, pipes);
				packers = min(block_size_bits - 8 - pipe_xor_bits,
					      ilog2(adev->gfx.config.gb_addr_config_fields.num_pkrs));
				break;
			case AMD_FMT_MOD_TILE_VER_GFX10:
				pipe_xor_bits = min(block_size_bits - 8, pipes);
				break;
			case AMD_FMT_MOD_TILE_VER_GFX9:
				rb = ilog2(adev->gfx.config.gb_addr_config_fields.num_se) +
				     ilog2(adev->gfx.config.gb_addr_config_fields.num_rb_per_se);
				pipe_xor_bits = min(block_size_bits - 8, pipes +
						    ilog2(adev->gfx.config.gb_addr_config_fields.num_se));
				bank_xor_bits = min(block_size_bits - 8 - pipe_xor_bits,
						    ilog2(adev->gfx.config.gb_addr_config_fields.num_banks));
				break;
			}
		}

		modifier = AMD_FMT_MOD |
			   AMD_FMT_MOD_SET(TILE, AMDGPU_TILING_GET(afb->tiling_flags, SWIZZLE_MODE)) |
			   AMD_FMT_MOD_SET(TILE_VERSION, version) |
			   AMD_FMT_MOD_SET(PIPE_XOR_BITS, pipe_xor_bits) |
			   AMD_FMT_MOD_SET(BANK_XOR_BITS, bank_xor_bits) |
			   AMD_FMT_MOD_SET(PACKERS, packers);

		if (dcc_offset != 0) {
			bool dcc_i64b = AMDGPU_TILING_GET(afb->tiling_flags, DCC_INDEPENDENT_64B) != 0;
			bool dcc_i128b = version >= AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS;
			const struct drm_format_info *format_info;
			u64 render_dcc_offset;

			 
			bool dcc_constant_encode = (adev->asic_type > CHIP_RAVEN ||
						   (adev->asic_type == CHIP_RAVEN &&
						    adev->external_rev_id >= 0x81)) &&
						    adev->ip_versions[GC_HWIP][0] < IP_VERSION(11, 0, 0);

			int max_cblock_size = dcc_i64b ? AMD_FMT_MOD_DCC_BLOCK_64B :
					      dcc_i128b ? AMD_FMT_MOD_DCC_BLOCK_128B :
					      AMD_FMT_MOD_DCC_BLOCK_256B;

			modifier |= AMD_FMT_MOD_SET(DCC, 1) |
				    AMD_FMT_MOD_SET(DCC_CONSTANT_ENCODE, dcc_constant_encode) |
				    AMD_FMT_MOD_SET(DCC_INDEPENDENT_64B, dcc_i64b) |
				    AMD_FMT_MOD_SET(DCC_INDEPENDENT_128B, dcc_i128b) |
				    AMD_FMT_MOD_SET(DCC_MAX_COMPRESSED_BLOCK, max_cblock_size);

			afb->base.offsets[1] = dcc_offset * 256 + afb->base.offsets[0];
			afb->base.pitches[1] =
				AMDGPU_TILING_GET(afb->tiling_flags, DCC_PITCH_MAX) + 1;

			 
			if (extract_render_dcc_offset(adev, afb->base.obj[0],
						      &render_dcc_offset) == 0 &&
			    render_dcc_offset != 0 &&
			    render_dcc_offset != afb->base.offsets[1] &&
			    render_dcc_offset < UINT_MAX) {
				uint32_t dcc_block_bits;   

				modifier |= AMD_FMT_MOD_SET(DCC_RETILE, 1);
				afb->base.offsets[2] = render_dcc_offset;

				if (adev->family >= AMDGPU_FAMILY_NV) {
					int extra_pipe = 0;

					if ((adev->ip_versions[GC_HWIP][0] >= IP_VERSION(10, 3, 0)) &&
					    pipes == packers && pipes > 1)
						extra_pipe = 1;

					dcc_block_bits = max(20, 16 + pipes + extra_pipe);
				} else {
					modifier |= AMD_FMT_MOD_SET(RB, rb) |
						    AMD_FMT_MOD_SET(PIPE, pipes);
					dcc_block_bits = max(20, 18 + rb);
				}

				dcc_block_bits -= ilog2(afb->base.format->cpp[0]);
				afb->base.pitches[2] = ALIGN(afb->base.width,
							     1u << ((dcc_block_bits + 1) / 2));
			}
			format_info = amdgpu_lookup_format_info(afb->base.format->format,
								modifier);
			if (!format_info)
				return -EINVAL;

			afb->base.format = format_info;
		}
	}

	afb->base.modifier = modifier;
	afb->base.flags |= DRM_MODE_FB_MODIFIERS;
	return 0;
}

 
static int check_tiling_flags_gfx6(struct amdgpu_framebuffer *afb)
{
	u64 micro_tile_mode;

	 
	if (AMDGPU_TILING_GET(afb->tiling_flags, SWIZZLE_MODE) == 0)
		return 0;

	micro_tile_mode = AMDGPU_TILING_GET(afb->tiling_flags, MICRO_TILE_MODE);
	switch (micro_tile_mode) {
	case 0:  
	case 3:  
		return 0;
	default:
		drm_dbg_kms(afb->base.dev,
			    "Micro tile mode %llu not supported for scanout\n",
			    micro_tile_mode);
		return -EINVAL;
	}
}

static void get_block_dimensions(unsigned int block_log2, unsigned int cpp,
				 unsigned int *width, unsigned int *height)
{
	unsigned int cpp_log2 = ilog2(cpp);
	unsigned int pixel_log2 = block_log2 - cpp_log2;
	unsigned int width_log2 = (pixel_log2 + 1) / 2;
	unsigned int height_log2 = pixel_log2 - width_log2;

	*width = 1 << width_log2;
	*height = 1 << height_log2;
}

static unsigned int get_dcc_block_size(uint64_t modifier, bool rb_aligned,
				       bool pipe_aligned)
{
	unsigned int ver = AMD_FMT_MOD_GET(TILE_VERSION, modifier);

	switch (ver) {
	case AMD_FMT_MOD_TILE_VER_GFX9: {
		 
		return max(10 + (rb_aligned ? (int)AMD_FMT_MOD_GET(RB, modifier) : 0), 12);
	}
	case AMD_FMT_MOD_TILE_VER_GFX10:
	case AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS:
	case AMD_FMT_MOD_TILE_VER_GFX11: {
		int pipes_log2 = AMD_FMT_MOD_GET(PIPE_XOR_BITS, modifier);

		if (ver >= AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS && pipes_log2 > 1 &&
		    AMD_FMT_MOD_GET(PACKERS, modifier) == pipes_log2)
			++pipes_log2;

		return max(8 + (pipe_aligned ? pipes_log2 : 0), 12);
	}
	default:
		return 0;
	}
}

static int amdgpu_display_verify_plane(struct amdgpu_framebuffer *rfb, int plane,
				       const struct drm_format_info *format,
				       unsigned int block_width, unsigned int block_height,
				       unsigned int block_size_log2)
{
	unsigned int width = rfb->base.width /
		((plane && plane < format->num_planes) ? format->hsub : 1);
	unsigned int height = rfb->base.height /
		((plane && plane < format->num_planes) ? format->vsub : 1);
	unsigned int cpp = plane < format->num_planes ? format->cpp[plane] : 1;
	unsigned int block_pitch = block_width * cpp;
	unsigned int min_pitch = ALIGN(width * cpp, block_pitch);
	unsigned int block_size = 1 << block_size_log2;
	uint64_t size;

	if (rfb->base.pitches[plane] % block_pitch) {
		drm_dbg_kms(rfb->base.dev,
			    "pitch %d for plane %d is not a multiple of block pitch %d\n",
			    rfb->base.pitches[plane], plane, block_pitch);
		return -EINVAL;
	}
	if (rfb->base.pitches[plane] < min_pitch) {
		drm_dbg_kms(rfb->base.dev,
			    "pitch %d for plane %d is less than minimum pitch %d\n",
			    rfb->base.pitches[plane], plane, min_pitch);
		return -EINVAL;
	}

	 
	if (rfb->base.offsets[plane] % block_size) {
		drm_dbg_kms(rfb->base.dev,
			    "offset 0x%x for plane %d is not a multiple of block pitch 0x%x\n",
			    rfb->base.offsets[plane], plane, block_size);
		return -EINVAL;
	}

	size = rfb->base.offsets[plane] +
		(uint64_t)rfb->base.pitches[plane] / block_pitch *
		block_size * DIV_ROUND_UP(height, block_height);

	if (rfb->base.obj[0]->size < size) {
		drm_dbg_kms(rfb->base.dev,
			    "BO size 0x%zx is less than 0x%llx required for plane %d\n",
			    rfb->base.obj[0]->size, size, plane);
		return -EINVAL;
	}

	return 0;
}


static int amdgpu_display_verify_sizes(struct amdgpu_framebuffer *rfb)
{
	const struct drm_format_info *format_info = drm_format_info(rfb->base.format->format);
	uint64_t modifier = rfb->base.modifier;
	int ret;
	unsigned int i, block_width, block_height, block_size_log2;

	if (rfb->base.dev->mode_config.fb_modifiers_not_supported)
		return 0;

	for (i = 0; i < format_info->num_planes; ++i) {
		if (modifier == DRM_FORMAT_MOD_LINEAR) {
			block_width = 256 / format_info->cpp[i];
			block_height = 1;
			block_size_log2 = 8;
		} else {
			int swizzle = AMD_FMT_MOD_GET(TILE, modifier);

			switch ((swizzle & ~3) + 1) {
			case DC_SW_256B_S:
				block_size_log2 = 8;
				break;
			case DC_SW_4KB_S:
			case DC_SW_4KB_S_X:
				block_size_log2 = 12;
				break;
			case DC_SW_64KB_S:
			case DC_SW_64KB_S_T:
			case DC_SW_64KB_S_X:
				block_size_log2 = 16;
				break;
			case DC_SW_VAR_S_X:
				block_size_log2 = 18;
				break;
			default:
				drm_dbg_kms(rfb->base.dev,
					    "Swizzle mode with unknown block size: %d\n", swizzle);
				return -EINVAL;
			}

			get_block_dimensions(block_size_log2, format_info->cpp[i],
					     &block_width, &block_height);
		}

		ret = amdgpu_display_verify_plane(rfb, i, format_info,
						  block_width, block_height, block_size_log2);
		if (ret)
			return ret;
	}

	if (AMD_FMT_MOD_GET(DCC, modifier)) {
		if (AMD_FMT_MOD_GET(DCC_RETILE, modifier)) {
			block_size_log2 = get_dcc_block_size(modifier, false, false);
			get_block_dimensions(block_size_log2 + 8, format_info->cpp[0],
					     &block_width, &block_height);
			ret = amdgpu_display_verify_plane(rfb, i, format_info,
							  block_width, block_height,
							  block_size_log2);
			if (ret)
				return ret;

			++i;
			block_size_log2 = get_dcc_block_size(modifier, true, true);
		} else {
			bool pipe_aligned = AMD_FMT_MOD_GET(DCC_PIPE_ALIGN, modifier);

			block_size_log2 = get_dcc_block_size(modifier, true, pipe_aligned);
		}
		get_block_dimensions(block_size_log2 + 8, format_info->cpp[0],
				     &block_width, &block_height);
		ret = amdgpu_display_verify_plane(rfb, i, format_info,
						  block_width, block_height, block_size_log2);
		if (ret)
			return ret;
	}

	return 0;
}

static int amdgpu_display_get_fb_info(const struct amdgpu_framebuffer *amdgpu_fb,
				      uint64_t *tiling_flags, bool *tmz_surface)
{
	struct amdgpu_bo *rbo;
	int r;

	if (!amdgpu_fb) {
		*tiling_flags = 0;
		*tmz_surface = false;
		return 0;
	}

	rbo = gem_to_amdgpu_bo(amdgpu_fb->base.obj[0]);
	r = amdgpu_bo_reserve(rbo, false);

	if (unlikely(r)) {
		 
		if (r != -ERESTARTSYS)
			DRM_ERROR("Unable to reserve buffer: %d\n", r);
		return r;
	}

	if (tiling_flags)
		amdgpu_bo_get_tiling_flags(rbo, tiling_flags);

	if (tmz_surface)
		*tmz_surface = amdgpu_bo_encrypted(rbo);

	amdgpu_bo_unreserve(rbo);

	return r;
}

static int amdgpu_display_gem_fb_verify_and_init(struct drm_device *dev,
						 struct amdgpu_framebuffer *rfb,
						 struct drm_file *file_priv,
						 const struct drm_mode_fb_cmd2 *mode_cmd,
						 struct drm_gem_object *obj)
{
	int ret;

	rfb->base.obj[0] = obj;
	drm_helper_mode_fill_fb_struct(dev, &rfb->base, mode_cmd);
	 
	if (!drm_any_plane_has_format(dev, mode_cmd->pixel_format,
				      mode_cmd->modifier[0])) {
		drm_dbg_kms(dev,
			    "unsupported pixel format %p4cc / modifier 0x%llx\n",
			    &mode_cmd->pixel_format, mode_cmd->modifier[0]);

		ret = -EINVAL;
		goto err;
	}

	ret = amdgpu_display_framebuffer_init(dev, rfb, mode_cmd, obj);
	if (ret)
		goto err;

	if (drm_drv_uses_atomic_modeset(dev))
		ret = drm_framebuffer_init(dev, &rfb->base,
					   &amdgpu_fb_funcs_atomic);
	else
		ret = drm_framebuffer_init(dev, &rfb->base, &amdgpu_fb_funcs);

	if (ret)
		goto err;

	return 0;
err:
	drm_dbg_kms(dev, "Failed to verify and init gem fb: %d\n", ret);
	rfb->base.obj[0] = NULL;
	return ret;
}

static int amdgpu_display_framebuffer_init(struct drm_device *dev,
					   struct amdgpu_framebuffer *rfb,
					   const struct drm_mode_fb_cmd2 *mode_cmd,
					   struct drm_gem_object *obj)
{
	struct amdgpu_device *adev = drm_to_adev(dev);
	int ret, i;

	 
	for (i = 1; i < rfb->base.format->num_planes; ++i) {
		if (mode_cmd->handles[i] != mode_cmd->handles[0]) {
			drm_dbg_kms(dev, "Plane 0 and %d have different BOs: %u vs. %u\n",
				    i, mode_cmd->handles[0], mode_cmd->handles[i]);
			ret = -EINVAL;
			return ret;
		}
	}

	ret = amdgpu_display_get_fb_info(rfb, &rfb->tiling_flags, &rfb->tmz_surface);
	if (ret)
		return ret;

	if (dev->mode_config.fb_modifiers_not_supported && !adev->enable_virtual_display) {
		drm_WARN_ONCE(dev, adev->family >= AMDGPU_FAMILY_AI,
			      "GFX9+ requires FB check based on format modifier\n");
		ret = check_tiling_flags_gfx6(rfb);
		if (ret)
			return ret;
	}

	if (!dev->mode_config.fb_modifiers_not_supported &&
	    !(rfb->base.flags & DRM_MODE_FB_MODIFIERS)) {
		ret = convert_tiling_flags_to_modifier(rfb);
		if (ret) {
			drm_dbg_kms(dev, "Failed to convert tiling flags 0x%llX to a modifier",
				    rfb->tiling_flags);
			return ret;
		}
	}

	ret = amdgpu_display_verify_sizes(rfb);
	if (ret)
		return ret;

	for (i = 0; i < rfb->base.format->num_planes; ++i) {
		drm_gem_object_get(rfb->base.obj[0]);
		rfb->base.obj[i] = rfb->base.obj[0];
	}

	return 0;
}

struct drm_framebuffer *
amdgpu_display_user_framebuffer_create(struct drm_device *dev,
				       struct drm_file *file_priv,
				       const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct amdgpu_framebuffer *amdgpu_fb;
	struct drm_gem_object *obj;
	struct amdgpu_bo *bo;
	uint32_t domains;
	int ret;

	obj = drm_gem_object_lookup(file_priv, mode_cmd->handles[0]);
	if (obj ==  NULL) {
		drm_dbg_kms(dev,
			    "No GEM object associated to handle 0x%08X, can't create framebuffer\n",
			    mode_cmd->handles[0]);

		return ERR_PTR(-ENOENT);
	}

	 
	bo = gem_to_amdgpu_bo(obj);
	domains = amdgpu_display_supported_domains(drm_to_adev(dev), bo->flags);
	if (obj->import_attach && !(domains & AMDGPU_GEM_DOMAIN_GTT)) {
		drm_dbg_kms(dev, "Cannot create framebuffer from imported dma_buf\n");
		drm_gem_object_put(obj);
		return ERR_PTR(-EINVAL);
	}

	amdgpu_fb = kzalloc(sizeof(*amdgpu_fb), GFP_KERNEL);
	if (amdgpu_fb == NULL) {
		drm_gem_object_put(obj);
		return ERR_PTR(-ENOMEM);
	}

	ret = amdgpu_display_gem_fb_verify_and_init(dev, amdgpu_fb, file_priv,
						    mode_cmd, obj);
	if (ret) {
		kfree(amdgpu_fb);
		drm_gem_object_put(obj);
		return ERR_PTR(ret);
	}

	drm_gem_object_put(obj);
	return &amdgpu_fb->base;
}

const struct drm_mode_config_funcs amdgpu_mode_funcs = {
	.fb_create = amdgpu_display_user_framebuffer_create,
};

static const struct drm_prop_enum_list amdgpu_underscan_enum_list[] = {
	{ UNDERSCAN_OFF, "off" },
	{ UNDERSCAN_ON, "on" },
	{ UNDERSCAN_AUTO, "auto" },
};

static const struct drm_prop_enum_list amdgpu_audio_enum_list[] = {
	{ AMDGPU_AUDIO_DISABLE, "off" },
	{ AMDGPU_AUDIO_ENABLE, "on" },
	{ AMDGPU_AUDIO_AUTO, "auto" },
};

 
static const struct drm_prop_enum_list amdgpu_dither_enum_list[] = {
	{ AMDGPU_FMT_DITHER_DISABLE, "off" },
	{ AMDGPU_FMT_DITHER_ENABLE, "on" },
};

int amdgpu_display_modeset_create_props(struct amdgpu_device *adev)
{
	int sz;

	adev->mode_info.coherent_mode_property =
		drm_property_create_range(adev_to_drm(adev), 0, "coherent", 0, 1);
	if (!adev->mode_info.coherent_mode_property)
		return -ENOMEM;

	adev->mode_info.load_detect_property =
		drm_property_create_range(adev_to_drm(adev), 0, "load detection", 0, 1);
	if (!adev->mode_info.load_detect_property)
		return -ENOMEM;

	drm_mode_create_scaling_mode_property(adev_to_drm(adev));

	sz = ARRAY_SIZE(amdgpu_underscan_enum_list);
	adev->mode_info.underscan_property =
		drm_property_create_enum(adev_to_drm(adev), 0,
					 "underscan",
					 amdgpu_underscan_enum_list, sz);

	adev->mode_info.underscan_hborder_property =
		drm_property_create_range(adev_to_drm(adev), 0,
					  "underscan hborder", 0, 128);
	if (!adev->mode_info.underscan_hborder_property)
		return -ENOMEM;

	adev->mode_info.underscan_vborder_property =
		drm_property_create_range(adev_to_drm(adev), 0,
					  "underscan vborder", 0, 128);
	if (!adev->mode_info.underscan_vborder_property)
		return -ENOMEM;

	sz = ARRAY_SIZE(amdgpu_audio_enum_list);
	adev->mode_info.audio_property =
		drm_property_create_enum(adev_to_drm(adev), 0,
					 "audio",
					 amdgpu_audio_enum_list, sz);

	sz = ARRAY_SIZE(amdgpu_dither_enum_list);
	adev->mode_info.dither_property =
		drm_property_create_enum(adev_to_drm(adev), 0,
					 "dither",
					 amdgpu_dither_enum_list, sz);

	if (adev->dc_enabled) {
		adev->mode_info.abm_level_property =
			drm_property_create_range(adev_to_drm(adev), 0,
						  "abm level", 0, 4);
		if (!adev->mode_info.abm_level_property)
			return -ENOMEM;
	}

	return 0;
}

void amdgpu_display_update_priority(struct amdgpu_device *adev)
{
	 
	if ((amdgpu_disp_priority == 0) || (amdgpu_disp_priority > 2))
		adev->mode_info.disp_priority = 0;
	else
		adev->mode_info.disp_priority = amdgpu_disp_priority;

}

static bool amdgpu_display_is_hdtv_mode(const struct drm_display_mode *mode)
{
	 
	if ((mode->vdisplay == 480 && mode->hdisplay == 720) ||  
	    (mode->vdisplay == 576) ||  
	    (mode->vdisplay == 720) ||  
	    (mode->vdisplay == 1080))  
		return true;
	else
		return false;
}

bool amdgpu_display_crtc_scaling_mode_fixup(struct drm_crtc *crtc,
					const struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = crtc->dev;
	struct drm_encoder *encoder;
	struct amdgpu_crtc *amdgpu_crtc = to_amdgpu_crtc(crtc);
	struct amdgpu_encoder *amdgpu_encoder;
	struct drm_connector *connector;
	u32 src_v = 1, dst_v = 1;
	u32 src_h = 1, dst_h = 1;

	amdgpu_crtc->h_border = 0;
	amdgpu_crtc->v_border = 0;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;
		amdgpu_encoder = to_amdgpu_encoder(encoder);
		connector = amdgpu_get_connector_for_encoder(encoder);

		 
		if (amdgpu_encoder->rmx_type == RMX_OFF)
			amdgpu_crtc->rmx_type = RMX_OFF;
		else if (mode->hdisplay < amdgpu_encoder->native_mode.hdisplay ||
			 mode->vdisplay < amdgpu_encoder->native_mode.vdisplay)
			amdgpu_crtc->rmx_type = amdgpu_encoder->rmx_type;
		else
			amdgpu_crtc->rmx_type = RMX_OFF;
		 
		memcpy(&amdgpu_crtc->native_mode,
		       &amdgpu_encoder->native_mode,
		       sizeof(struct drm_display_mode));
		src_v = crtc->mode.vdisplay;
		dst_v = amdgpu_crtc->native_mode.vdisplay;
		src_h = crtc->mode.hdisplay;
		dst_h = amdgpu_crtc->native_mode.hdisplay;

		 
		if ((!(mode->flags & DRM_MODE_FLAG_INTERLACE)) &&
		    ((amdgpu_encoder->underscan_type == UNDERSCAN_ON) ||
		     ((amdgpu_encoder->underscan_type == UNDERSCAN_AUTO) &&
		      connector->display_info.is_hdmi &&
		      amdgpu_display_is_hdtv_mode(mode)))) {
			if (amdgpu_encoder->underscan_hborder != 0)
				amdgpu_crtc->h_border = amdgpu_encoder->underscan_hborder;
			else
				amdgpu_crtc->h_border = (mode->hdisplay >> 5) + 16;
			if (amdgpu_encoder->underscan_vborder != 0)
				amdgpu_crtc->v_border = amdgpu_encoder->underscan_vborder;
			else
				amdgpu_crtc->v_border = (mode->vdisplay >> 5) + 16;
			amdgpu_crtc->rmx_type = RMX_FULL;
			src_v = crtc->mode.vdisplay;
			dst_v = crtc->mode.vdisplay - (amdgpu_crtc->v_border * 2);
			src_h = crtc->mode.hdisplay;
			dst_h = crtc->mode.hdisplay - (amdgpu_crtc->h_border * 2);
		}
	}
	if (amdgpu_crtc->rmx_type != RMX_OFF) {
		fixed20_12 a, b;

		a.full = dfixed_const(src_v);
		b.full = dfixed_const(dst_v);
		amdgpu_crtc->vsc.full = dfixed_div(a, b);
		a.full = dfixed_const(src_h);
		b.full = dfixed_const(dst_h);
		amdgpu_crtc->hsc.full = dfixed_div(a, b);
	} else {
		amdgpu_crtc->vsc.full = dfixed_const(1);
		amdgpu_crtc->hsc.full = dfixed_const(1);
	}
	return true;
}

 
int amdgpu_display_get_crtc_scanoutpos(struct drm_device *dev,
			unsigned int pipe, unsigned int flags, int *vpos,
			int *hpos, ktime_t *stime, ktime_t *etime,
			const struct drm_display_mode *mode)
{
	u32 vbl = 0, position = 0;
	int vbl_start, vbl_end, vtotal, ret = 0;
	bool in_vbl = true;

	struct amdgpu_device *adev = drm_to_adev(dev);

	 

	 
	if (stime)
		*stime = ktime_get();

	if (amdgpu_display_page_flip_get_scanoutpos(adev, pipe, &vbl, &position) == 0)
		ret |= DRM_SCANOUTPOS_VALID;

	 
	if (etime)
		*etime = ktime_get();

	 

	 
	*vpos = position & 0x1fff;
	*hpos = (position >> 16) & 0x1fff;

	 
	if (vbl > 0) {
		 
		ret |= DRM_SCANOUTPOS_ACCURATE;
		vbl_start = vbl & 0x1fff;
		vbl_end = (vbl >> 16) & 0x1fff;
	} else {
		 
		vbl_start = mode->crtc_vdisplay;
		vbl_end = 0;
	}

	 
	if (flags & GET_DISTANCE_TO_VBLANKSTART) {
		 
		*hpos = *vpos - vbl_start;
	}

	 
	if (!(flags & USE_REAL_VBLANKSTART))
		vbl_start -= adev->mode_info.crtcs[pipe]->lb_vblank_lead_lines;

	 
	if ((*vpos < vbl_start) && (*vpos >= vbl_end))
		in_vbl = false;

	 
	if (in_vbl)
		ret |= DRM_SCANOUTPOS_IN_VBLANK;

	 
	if (flags & GET_DISTANCE_TO_VBLANKSTART) {
		 
		*vpos -= vbl_start;
		return ret;
	}

	 

	 
	if (in_vbl && (*vpos >= vbl_start)) {
		vtotal = mode->crtc_vtotal;

		 
		*vpos = (*vpos < vtotal) ? (*vpos - vtotal) : 0;
	}

	 
	*vpos = *vpos - vbl_end;

	return ret;
}

int amdgpu_display_crtc_idx_to_irq_type(struct amdgpu_device *adev, int crtc)
{
	if (crtc < 0 || crtc >= adev->mode_info.num_crtc)
		return AMDGPU_CRTC_IRQ_NONE;

	switch (crtc) {
	case 0:
		return AMDGPU_CRTC_IRQ_VBLANK1;
	case 1:
		return AMDGPU_CRTC_IRQ_VBLANK2;
	case 2:
		return AMDGPU_CRTC_IRQ_VBLANK3;
	case 3:
		return AMDGPU_CRTC_IRQ_VBLANK4;
	case 4:
		return AMDGPU_CRTC_IRQ_VBLANK5;
	case 5:
		return AMDGPU_CRTC_IRQ_VBLANK6;
	default:
		return AMDGPU_CRTC_IRQ_NONE;
	}
}

bool amdgpu_crtc_get_scanout_position(struct drm_crtc *crtc,
			bool in_vblank_irq, int *vpos,
			int *hpos, ktime_t *stime, ktime_t *etime,
			const struct drm_display_mode *mode)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = crtc->index;

	return amdgpu_display_get_crtc_scanoutpos(dev, pipe, 0, vpos, hpos,
						  stime, etime, mode);
}

static bool
amdgpu_display_robj_is_fb(struct amdgpu_device *adev, struct amdgpu_bo *robj)
{
	struct drm_device *dev = adev_to_drm(adev);
	struct drm_fb_helper *fb_helper = dev->fb_helper;

	if (!fb_helper || !fb_helper->buffer)
		return false;

	if (gem_to_amdgpu_bo(fb_helper->buffer->gem) != robj)
		return false;

	return true;
}

int amdgpu_display_suspend_helper(struct amdgpu_device *adev)
{
	struct drm_device *dev = adev_to_drm(adev);
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct drm_connector_list_iter iter;
	int r;

	drm_kms_helper_poll_disable(dev);

	 
	drm_modeset_lock_all(dev);
	drm_connector_list_iter_begin(dev, &iter);
	drm_for_each_connector_iter(connector, &iter)
		drm_helper_connector_dpms(connector,
					  DRM_MODE_DPMS_OFF);
	drm_connector_list_iter_end(&iter);
	drm_modeset_unlock_all(dev);
	 
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct amdgpu_crtc *amdgpu_crtc = to_amdgpu_crtc(crtc);
		struct drm_framebuffer *fb = crtc->primary->fb;
		struct amdgpu_bo *robj;

		if (amdgpu_crtc->cursor_bo && !adev->enable_virtual_display) {
			struct amdgpu_bo *aobj = gem_to_amdgpu_bo(amdgpu_crtc->cursor_bo);

			r = amdgpu_bo_reserve(aobj, true);
			if (r == 0) {
				amdgpu_bo_unpin(aobj);
				amdgpu_bo_unreserve(aobj);
			}
		}

		if (!fb || !fb->obj[0])
			continue;

		robj = gem_to_amdgpu_bo(fb->obj[0]);
		if (!amdgpu_display_robj_is_fb(adev, robj)) {
			r = amdgpu_bo_reserve(robj, true);
			if (r == 0) {
				amdgpu_bo_unpin(robj);
				amdgpu_bo_unreserve(robj);
			}
		}
	}
	return 0;
}

int amdgpu_display_resume_helper(struct amdgpu_device *adev)
{
	struct drm_device *dev = adev_to_drm(adev);
	struct drm_connector *connector;
	struct drm_connector_list_iter iter;
	struct drm_crtc *crtc;
	int r;

	 
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct amdgpu_crtc *amdgpu_crtc = to_amdgpu_crtc(crtc);

		if (amdgpu_crtc->cursor_bo && !adev->enable_virtual_display) {
			struct amdgpu_bo *aobj = gem_to_amdgpu_bo(amdgpu_crtc->cursor_bo);

			r = amdgpu_bo_reserve(aobj, true);
			if (r == 0) {
				r = amdgpu_bo_pin(aobj, AMDGPU_GEM_DOMAIN_VRAM);
				if (r != 0)
					dev_err(adev->dev, "Failed to pin cursor BO (%d)\n", r);
				amdgpu_crtc->cursor_addr = amdgpu_bo_gpu_offset(aobj);
				amdgpu_bo_unreserve(aobj);
			}
		}
	}

	drm_helper_resume_force_mode(dev);

	 
	drm_modeset_lock_all(dev);

	drm_connector_list_iter_begin(dev, &iter);
	drm_for_each_connector_iter(connector, &iter)
		drm_helper_connector_dpms(connector,
					  DRM_MODE_DPMS_ON);
	drm_connector_list_iter_end(&iter);

	drm_modeset_unlock_all(dev);

	drm_kms_helper_poll_enable(dev);

	return 0;
}

