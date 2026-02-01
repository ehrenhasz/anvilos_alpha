
 

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/wait.h>

#include "rcar_du_crtc.h"
#include "rcar_du_drv.h"
#include "rcar_du_encoder.h"
#include "rcar_du_kms.h"
#include "rcar_du_regs.h"
#include "rcar_du_vsp.h"
#include "rcar_du_writeback.h"

 

static const struct rcar_du_format_info rcar_du_format_infos[] = {
	{
		.fourcc = DRM_FORMAT_RGB565,
		.v4l2 = V4L2_PIX_FMT_RGB565,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
		.pnmr = PnMR_SPIM_TP | PnMR_DDDF_16BPP,
		.edf = PnDDCR4_EDF_NONE,
	}, {
		.fourcc = DRM_FORMAT_ARGB1555,
		.v4l2 = V4L2_PIX_FMT_ARGB555,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
		.pnmr = PnMR_SPIM_ALP | PnMR_DDDF_ARGB,
		.edf = PnDDCR4_EDF_NONE,
	}, {
		.fourcc = DRM_FORMAT_XRGB1555,
		.v4l2 = V4L2_PIX_FMT_XRGB555,
		.bpp = 16,
		.planes = 1,
		.pnmr = PnMR_SPIM_ALP | PnMR_DDDF_ARGB,
		.edf = PnDDCR4_EDF_NONE,
	}, {
		.fourcc = DRM_FORMAT_XRGB8888,
		.v4l2 = V4L2_PIX_FMT_XBGR32,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
		.pnmr = PnMR_SPIM_TP | PnMR_DDDF_16BPP,
		.edf = PnDDCR4_EDF_RGB888,
	}, {
		.fourcc = DRM_FORMAT_ARGB8888,
		.v4l2 = V4L2_PIX_FMT_ABGR32,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
		.pnmr = PnMR_SPIM_ALP | PnMR_DDDF_16BPP,
		.edf = PnDDCR4_EDF_ARGB8888,
	}, {
		.fourcc = DRM_FORMAT_UYVY,
		.v4l2 = V4L2_PIX_FMT_UYVY,
		.bpp = 16,
		.planes = 1,
		.hsub = 2,
		.pnmr = PnMR_SPIM_TP_OFF | PnMR_DDDF_YC,
		.edf = PnDDCR4_EDF_NONE,
	}, {
		.fourcc = DRM_FORMAT_YUYV,
		.v4l2 = V4L2_PIX_FMT_YUYV,
		.bpp = 16,
		.planes = 1,
		.hsub = 2,
		.pnmr = PnMR_SPIM_TP_OFF | PnMR_DDDF_YC,
		.edf = PnDDCR4_EDF_NONE,
	}, {
		.fourcc = DRM_FORMAT_NV12,
		.v4l2 = V4L2_PIX_FMT_NV12M,
		.bpp = 12,
		.planes = 2,
		.hsub = 2,
		.pnmr = PnMR_SPIM_TP_OFF | PnMR_DDDF_YC,
		.edf = PnDDCR4_EDF_NONE,
	}, {
		.fourcc = DRM_FORMAT_NV21,
		.v4l2 = V4L2_PIX_FMT_NV21M,
		.bpp = 12,
		.planes = 2,
		.hsub = 2,
		.pnmr = PnMR_SPIM_TP_OFF | PnMR_DDDF_YC,
		.edf = PnDDCR4_EDF_NONE,
	}, {
		.fourcc = DRM_FORMAT_NV16,
		.v4l2 = V4L2_PIX_FMT_NV16M,
		.bpp = 16,
		.planes = 2,
		.hsub = 2,
		.pnmr = PnMR_SPIM_TP_OFF | PnMR_DDDF_YC,
		.edf = PnDDCR4_EDF_NONE,
	},
	 
	{
		.fourcc = DRM_FORMAT_RGB332,
		.v4l2 = V4L2_PIX_FMT_RGB332,
		.bpp = 8,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_ARGB4444,
		.v4l2 = V4L2_PIX_FMT_ARGB444,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_XRGB4444,
		.v4l2 = V4L2_PIX_FMT_XRGB444,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGBA4444,
		.v4l2 = V4L2_PIX_FMT_RGBA444,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGBX4444,
		.v4l2 = V4L2_PIX_FMT_RGBX444,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_ABGR4444,
		.v4l2 = V4L2_PIX_FMT_ABGR444,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_XBGR4444,
		.v4l2 = V4L2_PIX_FMT_XBGR444,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_BGRA4444,
		.v4l2 = V4L2_PIX_FMT_BGRA444,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_BGRX4444,
		.v4l2 = V4L2_PIX_FMT_BGRX444,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGBA5551,
		.v4l2 = V4L2_PIX_FMT_RGBA555,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGBX5551,
		.v4l2 = V4L2_PIX_FMT_RGBX555,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_ABGR1555,
		.v4l2 = V4L2_PIX_FMT_ABGR555,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_XBGR1555,
		.v4l2 = V4L2_PIX_FMT_XBGR555,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_BGRA5551,
		.v4l2 = V4L2_PIX_FMT_BGRA555,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_BGRX5551,
		.v4l2 = V4L2_PIX_FMT_BGRX555,
		.bpp = 16,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_BGR888,
		.v4l2 = V4L2_PIX_FMT_RGB24,
		.bpp = 24,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGB888,
		.v4l2 = V4L2_PIX_FMT_BGR24,
		.bpp = 24,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGBA8888,
		.v4l2 = V4L2_PIX_FMT_BGRA32,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGBX8888,
		.v4l2 = V4L2_PIX_FMT_BGRX32,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_ABGR8888,
		.v4l2 = V4L2_PIX_FMT_RGBA32,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_XBGR8888,
		.v4l2 = V4L2_PIX_FMT_RGBX32,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_BGRA8888,
		.v4l2 = V4L2_PIX_FMT_ARGB32,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_BGRX8888,
		.v4l2 = V4L2_PIX_FMT_XRGB32,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGBX1010102,
		.v4l2 = V4L2_PIX_FMT_RGBX1010102,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_RGBA1010102,
		.v4l2 = V4L2_PIX_FMT_RGBA1010102,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_ARGB2101010,
		.v4l2 = V4L2_PIX_FMT_ARGB2101010,
		.bpp = 32,
		.planes = 1,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_YVYU,
		.v4l2 = V4L2_PIX_FMT_YVYU,
		.bpp = 16,
		.planes = 1,
		.hsub = 2,
	}, {
		.fourcc = DRM_FORMAT_NV61,
		.v4l2 = V4L2_PIX_FMT_NV61M,
		.bpp = 16,
		.planes = 2,
		.hsub = 2,
	}, {
		.fourcc = DRM_FORMAT_YUV420,
		.v4l2 = V4L2_PIX_FMT_YUV420M,
		.bpp = 12,
		.planes = 3,
		.hsub = 2,
	}, {
		.fourcc = DRM_FORMAT_YVU420,
		.v4l2 = V4L2_PIX_FMT_YVU420M,
		.bpp = 12,
		.planes = 3,
		.hsub = 2,
	}, {
		.fourcc = DRM_FORMAT_YUV422,
		.v4l2 = V4L2_PIX_FMT_YUV422M,
		.bpp = 16,
		.planes = 3,
		.hsub = 2,
	}, {
		.fourcc = DRM_FORMAT_YVU422,
		.v4l2 = V4L2_PIX_FMT_YVU422M,
		.bpp = 16,
		.planes = 3,
		.hsub = 2,
	}, {
		.fourcc = DRM_FORMAT_YUV444,
		.v4l2 = V4L2_PIX_FMT_YUV444M,
		.bpp = 24,
		.planes = 3,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_YVU444,
		.v4l2 = V4L2_PIX_FMT_YVU444M,
		.bpp = 24,
		.planes = 3,
		.hsub = 1,
	}, {
		.fourcc = DRM_FORMAT_Y210,
		.v4l2 = V4L2_PIX_FMT_Y210,
		.bpp = 32,
		.planes = 1,
		.hsub = 2,
	}, {
		.fourcc = DRM_FORMAT_Y212,
		.v4l2 = V4L2_PIX_FMT_Y212,
		.bpp = 32,
		.planes = 1,
		.hsub = 2,
	},
};

const struct rcar_du_format_info *rcar_du_format_info(u32 fourcc)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(rcar_du_format_infos); ++i) {
		if (rcar_du_format_infos[i].fourcc == fourcc)
			return &rcar_du_format_infos[i];
	}

	return NULL;
}

 

static const struct drm_gem_object_funcs rcar_du_gem_funcs = {
	.free = drm_gem_dma_object_free,
	.print_info = drm_gem_dma_object_print_info,
	.get_sg_table = drm_gem_dma_object_get_sg_table,
	.vmap = drm_gem_dma_object_vmap,
	.mmap = drm_gem_dma_object_mmap,
	.vm_ops = &drm_gem_dma_vm_ops,
};

struct drm_gem_object *rcar_du_gem_prime_import_sg_table(struct drm_device *dev,
				struct dma_buf_attachment *attach,
				struct sg_table *sgt)
{
	struct rcar_du_device *rcdu = to_rcar_du_device(dev);
	struct drm_gem_dma_object *dma_obj;
	struct drm_gem_object *gem_obj;
	int ret;

	if (!rcar_du_has(rcdu, RCAR_DU_FEATURE_VSP1_SOURCE))
		return drm_gem_dma_prime_import_sg_table(dev, attach, sgt);

	 
	dma_obj = kzalloc(sizeof(*dma_obj), GFP_KERNEL);
	if (!dma_obj)
		return ERR_PTR(-ENOMEM);

	gem_obj = &dma_obj->base;
	gem_obj->funcs = &rcar_du_gem_funcs;

	drm_gem_private_object_init(dev, gem_obj, attach->dmabuf->size);
	dma_obj->map_noncoherent = false;

	ret = drm_gem_create_mmap_offset(gem_obj);
	if (ret) {
		drm_gem_object_release(gem_obj);
		kfree(dma_obj);
		return ERR_PTR(ret);
	}

	dma_obj->dma_addr = 0;
	dma_obj->sgt = sgt;

	return gem_obj;
}

int rcar_du_dumb_create(struct drm_file *file, struct drm_device *dev,
			struct drm_mode_create_dumb *args)
{
	struct rcar_du_device *rcdu = to_rcar_du_device(dev);
	unsigned int min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	unsigned int align;

	 
	if (rcar_du_needs(rcdu, RCAR_DU_QUIRK_ALIGN_128B))
		align = 128;
	else
		align = 16 * args->bpp / 8;

	args->pitch = roundup(min_pitch, align);

	return drm_gem_dma_dumb_create_internal(file, dev, args);
}

static struct drm_framebuffer *
rcar_du_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		  const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct rcar_du_device *rcdu = to_rcar_du_device(dev);
	const struct rcar_du_format_info *format;
	unsigned int chroma_pitch;
	unsigned int max_pitch;
	unsigned int align;
	unsigned int i;

	format = rcar_du_format_info(mode_cmd->pixel_format);
	if (format == NULL) {
		dev_dbg(dev->dev, "unsupported pixel format %p4cc\n",
			&mode_cmd->pixel_format);
		return ERR_PTR(-EINVAL);
	}

	if (rcdu->info->gen < 3) {
		 
		unsigned int bpp = format->planes == 1 ? format->bpp / 8 : 1;

		max_pitch = 4095 * bpp;

		if (rcar_du_needs(rcdu, RCAR_DU_QUIRK_ALIGN_128B))
			align = 128;
		else
			align = 16 * bpp;
	} else {
		 
		max_pitch = 65535;
		align = 1;
	}

	if (mode_cmd->pitches[0] & (align - 1) ||
	    mode_cmd->pitches[0] > max_pitch) {
		dev_dbg(dev->dev, "invalid pitch value %u\n",
			mode_cmd->pitches[0]);
		return ERR_PTR(-EINVAL);
	}

	 
	chroma_pitch = mode_cmd->pitches[0] / format->hsub;
	if (format->planes == 2)
		chroma_pitch *= 2;

	for (i = 1; i < format->planes; ++i) {
		if (mode_cmd->pitches[i] != chroma_pitch) {
			dev_dbg(dev->dev,
				"luma and chroma pitches are not compatible\n");
			return ERR_PTR(-EINVAL);
		}
	}

	return drm_gem_fb_create(dev, file_priv, mode_cmd);
}

 

static int rcar_du_atomic_check(struct drm_device *dev,
				struct drm_atomic_state *state)
{
	struct rcar_du_device *rcdu = to_rcar_du_device(dev);
	int ret;

	ret = drm_atomic_helper_check(dev, state);
	if (ret)
		return ret;

	if (rcar_du_has(rcdu, RCAR_DU_FEATURE_VSP1_SOURCE))
		return 0;

	return rcar_du_atomic_check_planes(dev, state);
}

static void rcar_du_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	struct rcar_du_device *rcdu = to_rcar_du_device(dev);
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	unsigned int i;

	 
	rcdu->dpad1_source = -1;

	for_each_new_crtc_in_state(old_state, crtc, crtc_state, i) {
		struct rcar_du_crtc_state *rcrtc_state =
			to_rcar_crtc_state(crtc_state);
		struct rcar_du_crtc *rcrtc = to_rcar_crtc(crtc);

		if (rcrtc_state->outputs & BIT(RCAR_DU_OUTPUT_DPAD0))
			rcdu->dpad0_source = rcrtc->index;

		if (rcrtc_state->outputs & BIT(RCAR_DU_OUTPUT_DPAD1))
			rcdu->dpad1_source = rcrtc->index;
	}

	 
	drm_atomic_helper_commit_modeset_disables(dev, old_state);
	drm_atomic_helper_commit_planes(dev, old_state,
					DRM_PLANE_COMMIT_ACTIVE_ONLY);
	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_commit_hw_done(old_state);
	drm_atomic_helper_wait_for_flip_done(dev, old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);
}

 

static const struct drm_mode_config_helper_funcs rcar_du_mode_config_helper = {
	.atomic_commit_tail = rcar_du_atomic_commit_tail,
};

static const struct drm_mode_config_funcs rcar_du_mode_config_funcs = {
	.fb_create = rcar_du_fb_create,
	.atomic_check = rcar_du_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static int rcar_du_encoders_init_one(struct rcar_du_device *rcdu,
				     enum rcar_du_output output,
				     struct of_endpoint *ep)
{
	struct device_node *entity;
	int ret;

	 
	entity = of_graph_get_remote_port_parent(ep->local_node);
	if (!entity) {
		dev_dbg(rcdu->dev, "unconnected endpoint %pOF, skipping\n",
			ep->local_node);
		return -ENODEV;
	}

	if (!of_device_is_available(entity)) {
		dev_dbg(rcdu->dev,
			"connected entity %pOF is disabled, skipping\n",
			entity);
		of_node_put(entity);
		return -ENODEV;
	}

	ret = rcar_du_encoder_init(rcdu, output, entity);
	if (ret && ret != -EPROBE_DEFER && ret != -ENOLINK)
		dev_warn(rcdu->dev,
			 "failed to initialize encoder %pOF on output %s (%d), skipping\n",
			 entity, rcar_du_output_name(output), ret);

	of_node_put(entity);

	return ret;
}

static int rcar_du_encoders_init(struct rcar_du_device *rcdu)
{
	struct device_node *np = rcdu->dev->of_node;
	struct device_node *ep_node;
	unsigned int num_encoders = 0;

	 
	for_each_endpoint_of_node(np, ep_node) {
		enum rcar_du_output output;
		struct of_endpoint ep;
		unsigned int i;
		int ret;

		ret = of_graph_parse_endpoint(ep_node, &ep);
		if (ret < 0) {
			of_node_put(ep_node);
			return ret;
		}

		 
		for (i = 0; i < RCAR_DU_OUTPUT_MAX; ++i) {
			if (rcdu->info->routes[i].possible_crtcs &&
			    rcdu->info->routes[i].port == ep.port) {
				output = i;
				break;
			}
		}

		if (i == RCAR_DU_OUTPUT_MAX) {
			dev_warn(rcdu->dev,
				 "port %u references unexisting output, skipping\n",
				 ep.port);
			continue;
		}

		 
		ret = rcar_du_encoders_init_one(rcdu, output, &ep);
		if (ret < 0) {
			if (ret == -EPROBE_DEFER) {
				of_node_put(ep_node);
				return ret;
			}

			continue;
		}

		num_encoders++;
	}

	return num_encoders;
}

static int rcar_du_properties_init(struct rcar_du_device *rcdu)
{
	 
	rcdu->props.colorkey =
		drm_property_create_range(&rcdu->ddev, 0, "colorkey",
					  0, 0x01ffffff);
	if (rcdu->props.colorkey == NULL)
		return -ENOMEM;

	return 0;
}

static int rcar_du_vsps_init(struct rcar_du_device *rcdu)
{
	const struct device_node *np = rcdu->dev->of_node;
	const char *vsps_prop_name = "renesas,vsps";
	struct of_phandle_args args;
	struct {
		struct device_node *np;
		unsigned int crtcs_mask;
	} vsps[RCAR_DU_MAX_VSPS] = { { NULL, }, };
	unsigned int vsps_count = 0;
	unsigned int cells;
	unsigned int i;
	int ret;

	 
	ret = of_property_count_u32_elems(np, vsps_prop_name);
	if (ret < 0) {
		 
		vsps_prop_name = "vsps";
		ret = of_property_count_u32_elems(np, vsps_prop_name);
	}
	cells = ret / rcdu->num_crtcs - 1;
	if (cells > 1)
		return -EINVAL;

	for (i = 0; i < rcdu->num_crtcs; ++i) {
		unsigned int j;

		ret = of_parse_phandle_with_fixed_args(np, vsps_prop_name,
						       cells, i, &args);
		if (ret < 0)
			goto error;

		 
		for (j = 0; j < vsps_count; ++j) {
			if (vsps[j].np == args.np)
				break;
		}

		if (j < vsps_count)
			of_node_put(args.np);
		else
			vsps[vsps_count++].np = args.np;

		vsps[j].crtcs_mask |= BIT(i);

		 
		rcdu->crtcs[i].vsp = &rcdu->vsps[j];
		rcdu->crtcs[i].vsp_pipe = cells >= 1 ? args.args[0] : 0;
	}

	 
	for (i = 0; i < vsps_count; ++i) {
		struct rcar_du_vsp *vsp = &rcdu->vsps[i];

		vsp->index = i;
		vsp->dev = rcdu;

		ret = rcar_du_vsp_init(vsp, vsps[i].np, vsps[i].crtcs_mask);
		if (ret < 0)
			goto error;
	}

	return 0;

error:
	for (i = 0; i < ARRAY_SIZE(vsps); ++i)
		of_node_put(vsps[i].np);

	return ret;
}

static int rcar_du_cmm_init(struct rcar_du_device *rcdu)
{
	const struct device_node *np = rcdu->dev->of_node;
	unsigned int i;
	int cells;

	cells = of_property_count_u32_elems(np, "renesas,cmms");
	if (cells == -EINVAL)
		return 0;

	if (cells > rcdu->num_crtcs) {
		dev_err(rcdu->dev,
			"Invalid number of entries in 'renesas,cmms'\n");
		return -EINVAL;
	}

	for (i = 0; i < cells; ++i) {
		struct platform_device *pdev;
		struct device_link *link;
		struct device_node *cmm;
		int ret;

		cmm = of_parse_phandle(np, "renesas,cmms", i);
		if (!cmm) {
			dev_err(rcdu->dev,
				"Failed to parse 'renesas,cmms' property\n");
			return -EINVAL;
		}

		if (!of_device_is_available(cmm)) {
			 
			of_node_put(cmm);
			continue;
		}

		pdev = of_find_device_by_node(cmm);
		if (!pdev) {
			dev_err(rcdu->dev, "No device found for CMM%u\n", i);
			of_node_put(cmm);
			return -EINVAL;
		}

		of_node_put(cmm);

		 
		ret = rcar_cmm_init(pdev);
		if (ret) {
			platform_device_put(pdev);
			return ret == -ENODEV ? 0 : ret;
		}

		rcdu->cmms[i] = pdev;

		 
		link = device_link_add(rcdu->dev, &pdev->dev, DL_FLAG_STATELESS);
		if (!link) {
			dev_err(rcdu->dev,
				"Failed to create device link to CMM%u\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

static void rcar_du_modeset_cleanup(struct drm_device *dev, void *res)
{
	struct rcar_du_device *rcdu = to_rcar_du_device(dev);
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(rcdu->cmms); ++i)
		platform_device_put(rcdu->cmms[i]);
}

int rcar_du_modeset_init(struct rcar_du_device *rcdu)
{
	static const unsigned int mmio_offsets[] = {
		DU0_REG_OFFSET, DU2_REG_OFFSET
	};

	struct drm_device *dev = &rcdu->ddev;
	struct drm_encoder *encoder;
	unsigned int dpad0_sources;
	unsigned int num_encoders;
	unsigned int num_groups;
	unsigned int swindex;
	unsigned int hwindex;
	unsigned int i;
	int ret;

	ret = drmm_mode_config_init(dev);
	if (ret)
		return ret;

	ret = drmm_add_action(&rcdu->ddev, rcar_du_modeset_cleanup, NULL);
	if (ret)
		return ret;

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.normalize_zpos = true;
	dev->mode_config.funcs = &rcar_du_mode_config_funcs;
	dev->mode_config.helper_private = &rcar_du_mode_config_helper;

	if (rcdu->info->gen < 3) {
		dev->mode_config.max_width = 4095;
		dev->mode_config.max_height = 2047;
	} else {
		 
		dev->mode_config.max_width = 8190;
		dev->mode_config.max_height = 8190;
	}

	rcdu->num_crtcs = hweight8(rcdu->info->channels_mask);

	ret = rcar_du_properties_init(rcdu);
	if (ret < 0)
		return ret;

	 
	ret = drm_vblank_init(dev, rcdu->num_crtcs);
	if (ret < 0)
		return ret;

	 
	num_groups = DIV_ROUND_UP(rcdu->num_crtcs, 2);

	for (i = 0; i < num_groups; ++i) {
		struct rcar_du_group *rgrp = &rcdu->groups[i];

		mutex_init(&rgrp->lock);

		rgrp->dev = rcdu;
		rgrp->mmio_offset = mmio_offsets[i];
		rgrp->index = i;
		 
		rgrp->channels_mask = (rcdu->info->channels_mask >> (2 * i))
				   & GENMASK(1, 0);
		rgrp->num_crtcs = hweight8(rgrp->channels_mask);

		 
		rgrp->dptsr_planes = rgrp->num_crtcs > 1
				   ? (rcdu->info->gen >= 3 ? 0x04 : 0xf0)
				   : 0;

		if (!rcar_du_has(rcdu, RCAR_DU_FEATURE_VSP1_SOURCE)) {
			ret = rcar_du_planes_init(rgrp);
			if (ret < 0)
				return ret;
		}
	}

	 
	if (rcar_du_has(rcdu, RCAR_DU_FEATURE_VSP1_SOURCE)) {
		ret = rcar_du_vsps_init(rcdu);
		if (ret < 0)
			return ret;
	}

	 
	ret = rcar_du_cmm_init(rcdu);
	if (ret)
		return dev_err_probe(rcdu->dev, ret,
				     "failed to initialize CMM\n");

	 
	for (swindex = 0, hwindex = 0; swindex < rcdu->num_crtcs; ++hwindex) {
		struct rcar_du_group *rgrp;

		 
		if (!(rcdu->info->channels_mask & BIT(hwindex)))
			continue;

		rgrp = &rcdu->groups[hwindex / 2];

		ret = rcar_du_crtc_create(rgrp, swindex++, hwindex);
		if (ret < 0)
			return ret;
	}

	 
	ret = rcar_du_encoders_init(rcdu);
	if (ret < 0)
		return dev_err_probe(rcdu->dev, ret,
				     "failed to initialize encoders\n");

	if (ret == 0) {
		dev_err(rcdu->dev, "error: no encoder could be initialized\n");
		return -EINVAL;
	}

	num_encoders = ret;

	 
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct rcar_du_encoder *renc = to_rcar_encoder(encoder);
		const struct rcar_du_output_routing *route =
			&rcdu->info->routes[renc->output];

		encoder->possible_crtcs = route->possible_crtcs;
		encoder->possible_clones = (1 << num_encoders) - 1;
	}

	 
	if (rcdu->info->gen >= 3) {
		for (i = 0; i < rcdu->num_crtcs; ++i) {
			struct rcar_du_crtc *rcrtc = &rcdu->crtcs[i];

			ret = rcar_du_writeback_init(rcdu, rcrtc);
			if (ret < 0)
				return ret;
		}
	}

	 
	dpad0_sources = rcdu->info->routes[RCAR_DU_OUTPUT_DPAD0].possible_crtcs;
	rcdu->dpad0_source = ffs(dpad0_sources) - 1;

	drm_mode_config_reset(dev);

	drm_kms_helper_poll_init(dev);

	return 0;
}
