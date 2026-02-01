
 
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/videodev2.h>
#include <linux/atmel-isc-media.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-dma-contig.h>

#include "atmel-isc-regs.h"
#include "atmel-isc.h"

static unsigned int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level (0-2)");

static unsigned int sensor_preferred = 1;
module_param(sensor_preferred, uint, 0644);
MODULE_PARM_DESC(sensor_preferred,
		 "Sensor is preferred to output the specified format (1-on 0-off), default 1");

#define ISC_IS_FORMAT_RAW(mbus_code) \
	(((mbus_code) & 0xf000) == 0x3000)

#define ISC_IS_FORMAT_GREY(mbus_code) \
	(((mbus_code) == MEDIA_BUS_FMT_Y10_1X10) | \
	(((mbus_code) == MEDIA_BUS_FMT_Y8_1X8)))

static inline void isc_update_v4l2_ctrls(struct isc_device *isc)
{
	struct isc_ctrls *ctrls = &isc->ctrls;

	 
	v4l2_ctrl_s_ctrl(isc->r_gain_ctrl, ctrls->gain[ISC_HIS_CFG_MODE_R]);
	v4l2_ctrl_s_ctrl(isc->b_gain_ctrl, ctrls->gain[ISC_HIS_CFG_MODE_B]);
	v4l2_ctrl_s_ctrl(isc->gr_gain_ctrl, ctrls->gain[ISC_HIS_CFG_MODE_GR]);
	v4l2_ctrl_s_ctrl(isc->gb_gain_ctrl, ctrls->gain[ISC_HIS_CFG_MODE_GB]);

	v4l2_ctrl_s_ctrl(isc->r_off_ctrl, ctrls->offset[ISC_HIS_CFG_MODE_R]);
	v4l2_ctrl_s_ctrl(isc->b_off_ctrl, ctrls->offset[ISC_HIS_CFG_MODE_B]);
	v4l2_ctrl_s_ctrl(isc->gr_off_ctrl, ctrls->offset[ISC_HIS_CFG_MODE_GR]);
	v4l2_ctrl_s_ctrl(isc->gb_off_ctrl, ctrls->offset[ISC_HIS_CFG_MODE_GB]);
}

static inline void isc_update_awb_ctrls(struct isc_device *isc)
{
	struct isc_ctrls *ctrls = &isc->ctrls;

	 

	regmap_write(isc->regmap, ISC_WB_O_RGR,
		     ((ctrls->offset[ISC_HIS_CFG_MODE_R])) |
		     ((ctrls->offset[ISC_HIS_CFG_MODE_GR]) << 16));
	regmap_write(isc->regmap, ISC_WB_O_BGB,
		     ((ctrls->offset[ISC_HIS_CFG_MODE_B])) |
		     ((ctrls->offset[ISC_HIS_CFG_MODE_GB]) << 16));
	regmap_write(isc->regmap, ISC_WB_G_RGR,
		     ctrls->gain[ISC_HIS_CFG_MODE_R] |
		     (ctrls->gain[ISC_HIS_CFG_MODE_GR] << 16));
	regmap_write(isc->regmap, ISC_WB_G_BGB,
		     ctrls->gain[ISC_HIS_CFG_MODE_B] |
		     (ctrls->gain[ISC_HIS_CFG_MODE_GB] << 16));
}

static inline void isc_reset_awb_ctrls(struct isc_device *isc)
{
	unsigned int c;

	for (c = ISC_HIS_CFG_MODE_GR; c <= ISC_HIS_CFG_MODE_B; c++) {
		 
		isc->ctrls.gain[c] = 1 << 9;
		 
		isc->ctrls.offset[c] = 0;
	}
}


static int isc_queue_setup(struct vb2_queue *vq,
			    unsigned int *nbuffers, unsigned int *nplanes,
			    unsigned int sizes[], struct device *alloc_devs[])
{
	struct isc_device *isc = vb2_get_drv_priv(vq);
	unsigned int size = isc->fmt.fmt.pix.sizeimage;

	if (*nplanes)
		return sizes[0] < size ? -EINVAL : 0;

	*nplanes = 1;
	sizes[0] = size;

	return 0;
}

static int isc_buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct isc_device *isc = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long size = isc->fmt.fmt.pix.sizeimage;

	if (vb2_plane_size(vb, 0) < size) {
		v4l2_err(&isc->v4l2_dev, "buffer too small (%lu < %lu)\n",
			 vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(vb, 0, size);

	vbuf->field = isc->fmt.fmt.pix.field;

	return 0;
}

static void isc_crop_pfe(struct isc_device *isc)
{
	struct regmap *regmap = isc->regmap;
	u32 h, w;

	h = isc->fmt.fmt.pix.height;
	w = isc->fmt.fmt.pix.width;

	 
	if (!ISC_IS_FORMAT_RAW(isc->config.sd_format->mbus_code)) {
		h <<= 1;
		w <<= 1;
	}

	 
	regmap_write(regmap, ISC_PFE_CFG1,
		     (ISC_PFE_CFG1_COLMIN(0) & ISC_PFE_CFG1_COLMIN_MASK) |
		     (ISC_PFE_CFG1_COLMAX(w - 1) & ISC_PFE_CFG1_COLMAX_MASK));

	regmap_write(regmap, ISC_PFE_CFG2,
		     (ISC_PFE_CFG2_ROWMIN(0) & ISC_PFE_CFG2_ROWMIN_MASK) |
		     (ISC_PFE_CFG2_ROWMAX(h - 1) & ISC_PFE_CFG2_ROWMAX_MASK));

	regmap_update_bits(regmap, ISC_PFE_CFG0,
			   ISC_PFE_CFG0_COLEN | ISC_PFE_CFG0_ROWEN,
			   ISC_PFE_CFG0_COLEN | ISC_PFE_CFG0_ROWEN);
}

static void isc_start_dma(struct isc_device *isc)
{
	struct regmap *regmap = isc->regmap;
	u32 sizeimage = isc->fmt.fmt.pix.sizeimage;
	u32 dctrl_dview;
	dma_addr_t addr0;

	addr0 = vb2_dma_contig_plane_dma_addr(&isc->cur_frm->vb.vb2_buf, 0);
	regmap_write(regmap, ISC_DAD0 + isc->offsets.dma, addr0);

	switch (isc->config.fourcc) {
	case V4L2_PIX_FMT_YUV420:
		regmap_write(regmap, ISC_DAD1 + isc->offsets.dma,
			     addr0 + (sizeimage * 2) / 3);
		regmap_write(regmap, ISC_DAD2 + isc->offsets.dma,
			     addr0 + (sizeimage * 5) / 6);
		break;
	case V4L2_PIX_FMT_YUV422P:
		regmap_write(regmap, ISC_DAD1 + isc->offsets.dma,
			     addr0 + sizeimage / 2);
		regmap_write(regmap, ISC_DAD2 + isc->offsets.dma,
			     addr0 + (sizeimage * 3) / 4);
		break;
	default:
		break;
	}

	dctrl_dview = isc->config.dctrl_dview;

	regmap_write(regmap, ISC_DCTRL + isc->offsets.dma,
		     dctrl_dview | ISC_DCTRL_IE_IS);
	spin_lock(&isc->awb_lock);
	regmap_write(regmap, ISC_CTRLEN, ISC_CTRL_CAPTURE);
	spin_unlock(&isc->awb_lock);
}

static void isc_set_pipeline(struct isc_device *isc, u32 pipeline)
{
	struct regmap *regmap = isc->regmap;
	struct isc_ctrls *ctrls = &isc->ctrls;
	u32 val, bay_cfg;
	const u32 *gamma;
	unsigned int i;

	 
	for (i = 0; i < ISC_PIPE_LINE_NODE_NUM; i++) {
		val = pipeline & BIT(i) ? 1 : 0;
		regmap_field_write(isc->pipeline[i], val);
	}

	if (!pipeline)
		return;

	bay_cfg = isc->config.sd_format->cfa_baycfg;

	regmap_write(regmap, ISC_WB_CFG, bay_cfg);
	isc_update_awb_ctrls(isc);
	isc_update_v4l2_ctrls(isc);

	regmap_write(regmap, ISC_CFA_CFG, bay_cfg | ISC_CFA_CFG_EITPOL);

	gamma = &isc->gamma_table[ctrls->gamma_index][0];
	regmap_bulk_write(regmap, ISC_GAM_BENTRY, gamma, GAMMA_ENTRIES);
	regmap_bulk_write(regmap, ISC_GAM_GENTRY, gamma, GAMMA_ENTRIES);
	regmap_bulk_write(regmap, ISC_GAM_RENTRY, gamma, GAMMA_ENTRIES);

	isc->config_dpc(isc);
	isc->config_csc(isc);
	isc->config_cbc(isc);
	isc->config_cc(isc);
	isc->config_gam(isc);
}

static int isc_update_profile(struct isc_device *isc)
{
	struct regmap *regmap = isc->regmap;
	u32 sr;
	int counter = 100;

	regmap_write(regmap, ISC_CTRLEN, ISC_CTRL_UPPRO);

	regmap_read(regmap, ISC_CTRLSR, &sr);
	while ((sr & ISC_CTRL_UPPRO) && counter--) {
		usleep_range(1000, 2000);
		regmap_read(regmap, ISC_CTRLSR, &sr);
	}

	if (counter < 0) {
		v4l2_warn(&isc->v4l2_dev, "Time out to update profile\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static void isc_set_histogram(struct isc_device *isc, bool enable)
{
	struct regmap *regmap = isc->regmap;
	struct isc_ctrls *ctrls = &isc->ctrls;

	if (enable) {
		regmap_write(regmap, ISC_HIS_CFG + isc->offsets.his,
			     ISC_HIS_CFG_MODE_GR |
			     (isc->config.sd_format->cfa_baycfg
					<< ISC_HIS_CFG_BAYSEL_SHIFT) |
					ISC_HIS_CFG_RAR);
		regmap_write(regmap, ISC_HIS_CTRL + isc->offsets.his,
			     ISC_HIS_CTRL_EN);
		regmap_write(regmap, ISC_INTEN, ISC_INT_HISDONE);
		ctrls->hist_id = ISC_HIS_CFG_MODE_GR;
		isc_update_profile(isc);
		regmap_write(regmap, ISC_CTRLEN, ISC_CTRL_HISREQ);

		ctrls->hist_stat = HIST_ENABLED;
	} else {
		regmap_write(regmap, ISC_INTDIS, ISC_INT_HISDONE);
		regmap_write(regmap, ISC_HIS_CTRL + isc->offsets.his,
			     ISC_HIS_CTRL_DIS);

		ctrls->hist_stat = HIST_DISABLED;
	}
}

static int isc_configure(struct isc_device *isc)
{
	struct regmap *regmap = isc->regmap;
	u32 pfe_cfg0, dcfg, mask, pipeline;
	struct isc_subdev_entity *subdev = isc->current_subdev;

	pfe_cfg0 = isc->config.sd_format->pfe_cfg0_bps;
	pipeline = isc->config.bits_pipeline;

	dcfg = isc->config.dcfg_imode | isc->dcfg;

	pfe_cfg0  |= subdev->pfe_cfg0 | ISC_PFE_CFG0_MODE_PROGRESSIVE;
	mask = ISC_PFE_CFG0_BPS_MASK | ISC_PFE_CFG0_HPOL_LOW |
	       ISC_PFE_CFG0_VPOL_LOW | ISC_PFE_CFG0_PPOL_LOW |
	       ISC_PFE_CFG0_MODE_MASK | ISC_PFE_CFG0_CCIR_CRC |
	       ISC_PFE_CFG0_CCIR656 | ISC_PFE_CFG0_MIPI;

	regmap_update_bits(regmap, ISC_PFE_CFG0, mask, pfe_cfg0);

	isc->config_rlp(isc);

	regmap_write(regmap, ISC_DCFG + isc->offsets.dma, dcfg);

	 
	isc_set_pipeline(isc, pipeline);

	 
	if (isc->ctrls.awb &&
	    ISC_IS_FORMAT_RAW(isc->config.sd_format->mbus_code))
		isc_set_histogram(isc, true);
	else
		isc_set_histogram(isc, false);

	 
	return isc_update_profile(isc);
}

static int isc_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct isc_device *isc = vb2_get_drv_priv(vq);
	struct regmap *regmap = isc->regmap;
	struct isc_buffer *buf;
	unsigned long flags;
	int ret;

	 
	ret = v4l2_subdev_call(isc->current_subdev->sd, video, s_stream, 1);
	if (ret && ret != -ENOIOCTLCMD) {
		v4l2_err(&isc->v4l2_dev, "stream on failed in subdev %d\n",
			 ret);
		goto err_start_stream;
	}

	ret = pm_runtime_resume_and_get(isc->dev);
	if (ret < 0) {
		v4l2_err(&isc->v4l2_dev, "RPM resume failed in subdev %d\n",
			 ret);
		goto err_pm_get;
	}

	ret = isc_configure(isc);
	if (unlikely(ret))
		goto err_configure;

	 
	regmap_write(regmap, ISC_INTEN, ISC_INT_DDONE);

	spin_lock_irqsave(&isc->dma_queue_lock, flags);

	isc->sequence = 0;
	isc->stop = false;
	reinit_completion(&isc->comp);

	isc->cur_frm = list_first_entry(&isc->dma_queue,
					struct isc_buffer, list);
	list_del(&isc->cur_frm->list);

	isc_crop_pfe(isc);
	isc_start_dma(isc);

	spin_unlock_irqrestore(&isc->dma_queue_lock, flags);

	 
	if (ISC_IS_FORMAT_RAW(isc->config.sd_format->mbus_code))
		v4l2_ctrl_activate(isc->do_wb_ctrl, true);

	return 0;

err_configure:
	pm_runtime_put_sync(isc->dev);
err_pm_get:
	v4l2_subdev_call(isc->current_subdev->sd, video, s_stream, 0);

err_start_stream:
	spin_lock_irqsave(&isc->dma_queue_lock, flags);
	list_for_each_entry(buf, &isc->dma_queue, list)
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_QUEUED);
	INIT_LIST_HEAD(&isc->dma_queue);
	spin_unlock_irqrestore(&isc->dma_queue_lock, flags);

	return ret;
}

static void isc_stop_streaming(struct vb2_queue *vq)
{
	struct isc_device *isc = vb2_get_drv_priv(vq);
	unsigned long flags;
	struct isc_buffer *buf;
	int ret;

	mutex_lock(&isc->awb_mutex);
	v4l2_ctrl_activate(isc->do_wb_ctrl, false);

	isc->stop = true;

	 
	if (isc->cur_frm && !wait_for_completion_timeout(&isc->comp, 5 * HZ))
		v4l2_err(&isc->v4l2_dev,
			 "Timeout waiting for end of the capture\n");

	mutex_unlock(&isc->awb_mutex);

	 
	regmap_write(isc->regmap, ISC_INTDIS, ISC_INT_DDONE);

	pm_runtime_put_sync(isc->dev);

	 
	ret = v4l2_subdev_call(isc->current_subdev->sd, video, s_stream, 0);
	if (ret && ret != -ENOIOCTLCMD)
		v4l2_err(&isc->v4l2_dev, "stream off failed in subdev\n");

	 
	spin_lock_irqsave(&isc->dma_queue_lock, flags);
	if (unlikely(isc->cur_frm)) {
		vb2_buffer_done(&isc->cur_frm->vb.vb2_buf,
				VB2_BUF_STATE_ERROR);
		isc->cur_frm = NULL;
	}
	list_for_each_entry(buf, &isc->dma_queue, list)
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	INIT_LIST_HEAD(&isc->dma_queue);
	spin_unlock_irqrestore(&isc->dma_queue_lock, flags);
}

static void isc_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct isc_buffer *buf = container_of(vbuf, struct isc_buffer, vb);
	struct isc_device *isc = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long flags;

	spin_lock_irqsave(&isc->dma_queue_lock, flags);
	if (!isc->cur_frm && list_empty(&isc->dma_queue) &&
		vb2_start_streaming_called(vb->vb2_queue)) {
		isc->cur_frm = buf;
		isc_start_dma(isc);
	} else
		list_add_tail(&buf->list, &isc->dma_queue);
	spin_unlock_irqrestore(&isc->dma_queue_lock, flags);
}

static struct isc_format *find_format_by_fourcc(struct isc_device *isc,
						 unsigned int fourcc)
{
	unsigned int num_formats = isc->num_user_formats;
	struct isc_format *fmt;
	unsigned int i;

	for (i = 0; i < num_formats; i++) {
		fmt = isc->user_formats[i];
		if (fmt->fourcc == fourcc)
			return fmt;
	}

	return NULL;
}

static const struct vb2_ops isc_vb2_ops = {
	.queue_setup		= isc_queue_setup,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
	.buf_prepare		= isc_buffer_prepare,
	.start_streaming	= isc_start_streaming,
	.stop_streaming		= isc_stop_streaming,
	.buf_queue		= isc_buffer_queue,
};

static int isc_querycap(struct file *file, void *priv,
			 struct v4l2_capability *cap)
{
	struct isc_device *isc = video_drvdata(file);

	strscpy(cap->driver, "microchip-isc", sizeof(cap->driver));
	strscpy(cap->card, "Atmel Image Sensor Controller", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", isc->v4l2_dev.name);

	return 0;
}

static int isc_enum_fmt_vid_cap(struct file *file, void *priv,
				 struct v4l2_fmtdesc *f)
{
	struct isc_device *isc = video_drvdata(file);
	u32 index = f->index;
	u32 i, supported_index;

	if (index < isc->controller_formats_size) {
		f->pixelformat = isc->controller_formats[index].fourcc;
		return 0;
	}

	index -= isc->controller_formats_size;

	supported_index = 0;

	for (i = 0; i < isc->formats_list_size; i++) {
		if (!ISC_IS_FORMAT_RAW(isc->formats_list[i].mbus_code) ||
		    !isc->formats_list[i].sd_support)
			continue;
		if (supported_index == index) {
			f->pixelformat = isc->formats_list[i].fourcc;
			return 0;
		}
		supported_index++;
	}

	return -EINVAL;
}

static int isc_g_fmt_vid_cap(struct file *file, void *priv,
			      struct v4l2_format *fmt)
{
	struct isc_device *isc = video_drvdata(file);

	*fmt = isc->fmt;

	return 0;
}

 
static int isc_try_validate_formats(struct isc_device *isc)
{
	int ret;
	bool bayer = false, yuv = false, rgb = false, grey = false;

	 
	switch (isc->try_config.fourcc) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		ret = 0;
		bayer = true;
		break;

	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		ret = 0;
		yuv = true;
		break;

	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_ARGB555:
		ret = 0;
		rgb = true;
		break;
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y16:
		ret = 0;
		grey = true;
		break;
	default:
	 
		ret = -EINVAL;
	}
	v4l2_dbg(1, debug, &isc->v4l2_dev,
		 "Format validation, requested rgb=%u, yuv=%u, grey=%u, bayer=%u\n",
		 rgb, yuv, grey, bayer);

	 
	if ((bayer) && !ISC_IS_FORMAT_RAW(isc->try_config.sd_format->mbus_code))
		return -EINVAL;

	 
	if (grey && !ISC_IS_FORMAT_RAW(isc->try_config.sd_format->mbus_code) &&
	    !ISC_IS_FORMAT_GREY(isc->try_config.sd_format->mbus_code))
		return -EINVAL;

	return ret;
}

 
static int isc_try_configure_rlp_dma(struct isc_device *isc, bool direct_dump)
{
	isc->try_config.rlp_cfg_mode = 0;

	switch (isc->try_config.fourcc) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_DAT8;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED8;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 8;
		isc->try_config.bpp_v4l2 = 8;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_DAT10;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED16;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_DAT12;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED16;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	case V4L2_PIX_FMT_RGB565:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_RGB565;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED16;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	case V4L2_PIX_FMT_ARGB444:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_ARGB444;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED16;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	case V4L2_PIX_FMT_ARGB555:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_ARGB555;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED16;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_XBGR32:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_ARGB32;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED32;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 32;
		isc->try_config.bpp_v4l2 = 32;
		break;
	case V4L2_PIX_FMT_YUV420:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_YYCC;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_YC420P;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PLANAR;
		isc->try_config.bpp = 12;
		isc->try_config.bpp_v4l2 = 8;  
		break;
	case V4L2_PIX_FMT_YUV422P:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_YYCC;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_YC422P;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PLANAR;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 8;  
		break;
	case V4L2_PIX_FMT_YUYV:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_YCYC | ISC_RLP_CFG_YMODE_YUYV;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED32;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	case V4L2_PIX_FMT_UYVY:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_YCYC | ISC_RLP_CFG_YMODE_UYVY;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED32;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	case V4L2_PIX_FMT_VYUY:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_YCYC | ISC_RLP_CFG_YMODE_VYUY;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED32;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	case V4L2_PIX_FMT_GREY:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_DATY8;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED8;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 8;
		isc->try_config.bpp_v4l2 = 8;
		break;
	case V4L2_PIX_FMT_Y16:
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_DATY10 | ISC_RLP_CFG_LSH;
		fallthrough;
	case V4L2_PIX_FMT_Y10:
		isc->try_config.rlp_cfg_mode |= ISC_RLP_CFG_MODE_DATY10;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED16;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		isc->try_config.bpp = 16;
		isc->try_config.bpp_v4l2 = 16;
		break;
	default:
		return -EINVAL;
	}

	if (direct_dump) {
		isc->try_config.rlp_cfg_mode = ISC_RLP_CFG_MODE_DAT8;
		isc->try_config.dcfg_imode = ISC_DCFG_IMODE_PACKED8;
		isc->try_config.dctrl_dview = ISC_DCTRL_DVIEW_PACKED;
		return 0;
	}

	return 0;
}

 
static int isc_try_configure_pipeline(struct isc_device *isc)
{
	switch (isc->try_config.fourcc) {
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_XBGR32:
		 
		if (ISC_IS_FORMAT_RAW(isc->try_config.sd_format->mbus_code)) {
			isc->try_config.bits_pipeline = CFA_ENABLE |
				WB_ENABLE | GAM_ENABLES | DPC_BLCENABLE |
				CC_ENABLE;
		} else {
			isc->try_config.bits_pipeline = 0x0;
		}
		break;
	case V4L2_PIX_FMT_YUV420:
		 
		if (ISC_IS_FORMAT_RAW(isc->try_config.sd_format->mbus_code)) {
			isc->try_config.bits_pipeline = CFA_ENABLE |
				CSC_ENABLE | GAM_ENABLES | WB_ENABLE |
				SUB420_ENABLE | SUB422_ENABLE | CBC_ENABLE |
				DPC_BLCENABLE;
		} else {
			isc->try_config.bits_pipeline = 0x0;
		}
		break;
	case V4L2_PIX_FMT_YUV422P:
		 
		if (ISC_IS_FORMAT_RAW(isc->try_config.sd_format->mbus_code)) {
			isc->try_config.bits_pipeline = CFA_ENABLE |
				CSC_ENABLE | WB_ENABLE | GAM_ENABLES |
				SUB422_ENABLE | CBC_ENABLE | DPC_BLCENABLE;
		} else {
			isc->try_config.bits_pipeline = 0x0;
		}
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		 
		if (ISC_IS_FORMAT_RAW(isc->try_config.sd_format->mbus_code)) {
			isc->try_config.bits_pipeline = CFA_ENABLE |
				CSC_ENABLE | WB_ENABLE | GAM_ENABLES |
				SUB422_ENABLE | CBC_ENABLE | DPC_BLCENABLE;
		} else {
			isc->try_config.bits_pipeline = 0x0;
		}
		break;
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y16:
		 
		if (ISC_IS_FORMAT_RAW(isc->try_config.sd_format->mbus_code)) {
			isc->try_config.bits_pipeline = CFA_ENABLE |
				CSC_ENABLE | WB_ENABLE | GAM_ENABLES |
				CBC_ENABLE | DPC_BLCENABLE;
		} else {
			isc->try_config.bits_pipeline = 0x0;
		}
		break;
	default:
		if (ISC_IS_FORMAT_RAW(isc->try_config.sd_format->mbus_code))
			isc->try_config.bits_pipeline = WB_ENABLE | DPC_BLCENABLE;
		else
			isc->try_config.bits_pipeline = 0x0;
	}

	 
	isc->adapt_pipeline(isc);

	return 0;
}

static void isc_try_fse(struct isc_device *isc,
			struct v4l2_subdev_state *sd_state)
{
	struct v4l2_subdev_frame_size_enum fse = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
	};
	int ret;

	 
	if (!isc->try_config.sd_format)
		return;

	fse.code = isc->try_config.sd_format->mbus_code;

	ret = v4l2_subdev_call(isc->current_subdev->sd, pad, enum_frame_size,
			       sd_state, &fse);
	 
	if (ret) {
		sd_state->pads->try_crop.width = isc->max_width;
		sd_state->pads->try_crop.height = isc->max_height;
	} else {
		sd_state->pads->try_crop.width = fse.max_width;
		sd_state->pads->try_crop.height = fse.max_height;
	}
}

static int isc_try_fmt(struct isc_device *isc, struct v4l2_format *f,
			u32 *code)
{
	int i;
	struct isc_format *sd_fmt = NULL, *direct_fmt = NULL;
	struct v4l2_pix_format *pixfmt = &f->fmt.pix;
	struct v4l2_subdev_pad_config pad_cfg = {};
	struct v4l2_subdev_state pad_state = {
		.pads = &pad_cfg,
	};
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
	};
	u32 mbus_code;
	int ret;
	bool rlp_dma_direct_dump = false;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	 
	for (i = 0; i < isc->num_user_formats; i++) {
		if (ISC_IS_FORMAT_RAW(isc->user_formats[i]->mbus_code)) {
			sd_fmt = isc->user_formats[i];
			break;
		}
	}
	 
	direct_fmt = find_format_by_fourcc(isc, pixfmt->pixelformat);

	 
	if (direct_fmt && sd_fmt && sensor_preferred)
		sd_fmt = direct_fmt;

	 
	if (direct_fmt && !sd_fmt)
		sd_fmt = direct_fmt;

	 
	if (sd_fmt == direct_fmt)
		rlp_dma_direct_dump = true;

	 
	if (!sd_fmt && !direct_fmt) {
		sd_fmt = isc->user_formats[isc->num_user_formats - 1];
		v4l2_dbg(1, debug, &isc->v4l2_dev,
			 "Sensor not supporting %.4s, using %.4s\n",
			 (char *)&pixfmt->pixelformat, (char *)&sd_fmt->fourcc);
	}

	if (!sd_fmt) {
		ret = -EINVAL;
		goto isc_try_fmt_err;
	}

	 
	v4l2_dbg(1, debug, &isc->v4l2_dev,
		 "Preferring to have sensor using format %.4s\n",
		 (char *)&sd_fmt->fourcc);

	 
	isc->try_config.sd_format = sd_fmt;

	 
	if (pixfmt->width > isc->max_width)
		pixfmt->width = isc->max_width;
	if (pixfmt->height > isc->max_height)
		pixfmt->height = isc->max_height;

	 
	mbus_code = sd_fmt->mbus_code;

	 

	isc->try_config.fourcc = pixfmt->pixelformat;

	if (isc_try_validate_formats(isc)) {
		pixfmt->pixelformat = isc->try_config.fourcc = sd_fmt->fourcc;
		 
		ret = isc_try_validate_formats(isc);
		if (ret)
			goto isc_try_fmt_err;
	}

	ret = isc_try_configure_rlp_dma(isc, rlp_dma_direct_dump);
	if (ret)
		goto isc_try_fmt_err;

	ret = isc_try_configure_pipeline(isc);
	if (ret)
		goto isc_try_fmt_err;

	 
	isc_try_fse(isc, &pad_state);

	v4l2_fill_mbus_format(&format.format, pixfmt, mbus_code);
	ret = v4l2_subdev_call(isc->current_subdev->sd, pad, set_fmt,
			       &pad_state, &format);
	if (ret < 0)
		goto isc_try_fmt_subdev_err;

	v4l2_fill_pix_format(pixfmt, &format.format);

	 
	if (pixfmt->width > isc->max_width)
		pixfmt->width = isc->max_width;
	if (pixfmt->height > isc->max_height)
		pixfmt->height = isc->max_height;

	pixfmt->field = V4L2_FIELD_NONE;
	pixfmt->bytesperline = (pixfmt->width * isc->try_config.bpp_v4l2) >> 3;
	pixfmt->sizeimage = ((pixfmt->width * isc->try_config.bpp) >> 3) *
			     pixfmt->height;

	if (code)
		*code = mbus_code;

	return 0;

isc_try_fmt_err:
	v4l2_err(&isc->v4l2_dev, "Could not find any possible format for a working pipeline\n");
isc_try_fmt_subdev_err:
	memset(&isc->try_config, 0, sizeof(isc->try_config));

	return ret;
}

static int isc_set_fmt(struct isc_device *isc, struct v4l2_format *f)
{
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	u32 mbus_code = 0;
	int ret;

	ret = isc_try_fmt(isc, f, &mbus_code);
	if (ret)
		return ret;

	v4l2_fill_mbus_format(&format.format, &f->fmt.pix, mbus_code);
	ret = v4l2_subdev_call(isc->current_subdev->sd, pad,
			       set_fmt, NULL, &format);
	if (ret < 0)
		return ret;

	 
	if (f->fmt.pix.width > isc->max_width)
		f->fmt.pix.width = isc->max_width;
	if (f->fmt.pix.height > isc->max_height)
		f->fmt.pix.height = isc->max_height;

	isc->fmt = *f;

	if (isc->try_config.sd_format && isc->config.sd_format &&
	    isc->try_config.sd_format != isc->config.sd_format) {
		isc->ctrls.hist_stat = HIST_INIT;
		isc_reset_awb_ctrls(isc);
		isc_update_v4l2_ctrls(isc);
	}
	 
	isc->config = isc->try_config;

	v4l2_dbg(1, debug, &isc->v4l2_dev, "New ISC configuration in place\n");

	return 0;
}

static int isc_s_fmt_vid_cap(struct file *file, void *priv,
			      struct v4l2_format *f)
{
	struct isc_device *isc = video_drvdata(file);

	if (vb2_is_busy(&isc->vb2_vidq))
		return -EBUSY;

	return isc_set_fmt(isc, f);
}

static int isc_try_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct isc_device *isc = video_drvdata(file);

	return isc_try_fmt(isc, f, NULL);
}

static int isc_enum_input(struct file *file, void *priv,
			   struct v4l2_input *inp)
{
	if (inp->index != 0)
		return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = 0;
	strscpy(inp->name, "Camera", sizeof(inp->name));

	return 0;
}

static int isc_g_input(struct file *file, void *priv, unsigned int *i)
{
	*i = 0;

	return 0;
}

static int isc_s_input(struct file *file, void *priv, unsigned int i)
{
	if (i > 0)
		return -EINVAL;

	return 0;
}

static int isc_g_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct isc_device *isc = video_drvdata(file);

	return v4l2_g_parm_cap(video_devdata(file), isc->current_subdev->sd, a);
}

static int isc_s_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct isc_device *isc = video_drvdata(file);

	return v4l2_s_parm_cap(video_devdata(file), isc->current_subdev->sd, a);
}

static int isc_enum_framesizes(struct file *file, void *fh,
			       struct v4l2_frmsizeenum *fsize)
{
	struct isc_device *isc = video_drvdata(file);
	int ret = -EINVAL;
	int i;

	if (fsize->index)
		return -EINVAL;

	for (i = 0; i < isc->num_user_formats; i++)
		if (isc->user_formats[i]->fourcc == fsize->pixel_format)
			ret = 0;

	for (i = 0; i < isc->controller_formats_size; i++)
		if (isc->controller_formats[i].fourcc == fsize->pixel_format)
			ret = 0;

	if (ret)
		return ret;

	fsize->type = V4L2_FRMSIZE_TYPE_CONTINUOUS;

	fsize->stepwise.min_width = 16;
	fsize->stepwise.max_width = isc->max_width;
	fsize->stepwise.min_height = 16;
	fsize->stepwise.max_height = isc->max_height;
	fsize->stepwise.step_width = 1;
	fsize->stepwise.step_height = 1;

	return 0;
}

static const struct v4l2_ioctl_ops isc_ioctl_ops = {
	.vidioc_querycap		= isc_querycap,
	.vidioc_enum_fmt_vid_cap	= isc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= isc_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= isc_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap		= isc_try_fmt_vid_cap,

	.vidioc_enum_input		= isc_enum_input,
	.vidioc_g_input			= isc_g_input,
	.vidioc_s_input			= isc_s_input,

	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_prepare_buf		= vb2_ioctl_prepare_buf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,

	.vidioc_g_parm			= isc_g_parm,
	.vidioc_s_parm			= isc_s_parm,
	.vidioc_enum_framesizes		= isc_enum_framesizes,

	.vidioc_log_status		= v4l2_ctrl_log_status,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static int isc_open(struct file *file)
{
	struct isc_device *isc = video_drvdata(file);
	struct v4l2_subdev *sd = isc->current_subdev->sd;
	int ret;

	if (mutex_lock_interruptible(&isc->lock))
		return -ERESTARTSYS;

	ret = v4l2_fh_open(file);
	if (ret < 0)
		goto unlock;

	if (!v4l2_fh_is_singular_file(file))
		goto unlock;

	ret = v4l2_subdev_call(sd, core, s_power, 1);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		v4l2_fh_release(file);
		goto unlock;
	}

	ret = isc_set_fmt(isc, &isc->fmt);
	if (ret) {
		v4l2_subdev_call(sd, core, s_power, 0);
		v4l2_fh_release(file);
	}

unlock:
	mutex_unlock(&isc->lock);
	return ret;
}

static int isc_release(struct file *file)
{
	struct isc_device *isc = video_drvdata(file);
	struct v4l2_subdev *sd = isc->current_subdev->sd;
	bool fh_singular;
	int ret;

	mutex_lock(&isc->lock);

	fh_singular = v4l2_fh_is_singular_file(file);

	ret = _vb2_fop_release(file, NULL);

	if (fh_singular)
		v4l2_subdev_call(sd, core, s_power, 0);

	mutex_unlock(&isc->lock);

	return ret;
}

static const struct v4l2_file_operations isc_fops = {
	.owner		= THIS_MODULE,
	.open		= isc_open,
	.release	= isc_release,
	.unlocked_ioctl	= video_ioctl2,
	.read		= vb2_fop_read,
	.mmap		= vb2_fop_mmap,
	.poll		= vb2_fop_poll,
};

irqreturn_t atmel_isc_interrupt(int irq, void *dev_id)
{
	struct isc_device *isc = (struct isc_device *)dev_id;
	struct regmap *regmap = isc->regmap;
	u32 isc_intsr, isc_intmask, pending;
	irqreturn_t ret = IRQ_NONE;

	regmap_read(regmap, ISC_INTSR, &isc_intsr);
	regmap_read(regmap, ISC_INTMASK, &isc_intmask);

	pending = isc_intsr & isc_intmask;

	if (likely(pending & ISC_INT_DDONE)) {
		spin_lock(&isc->dma_queue_lock);
		if (isc->cur_frm) {
			struct vb2_v4l2_buffer *vbuf = &isc->cur_frm->vb;
			struct vb2_buffer *vb = &vbuf->vb2_buf;

			vb->timestamp = ktime_get_ns();
			vbuf->sequence = isc->sequence++;
			vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			isc->cur_frm = NULL;
		}

		if (!list_empty(&isc->dma_queue) && !isc->stop) {
			isc->cur_frm = list_first_entry(&isc->dma_queue,
						     struct isc_buffer, list);
			list_del(&isc->cur_frm->list);

			isc_start_dma(isc);
		}

		if (isc->stop)
			complete(&isc->comp);

		ret = IRQ_HANDLED;
		spin_unlock(&isc->dma_queue_lock);
	}

	if (pending & ISC_INT_HISDONE) {
		schedule_work(&isc->awb_work);
		ret = IRQ_HANDLED;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(atmel_isc_interrupt);

static void isc_hist_count(struct isc_device *isc, u32 *min, u32 *max)
{
	struct regmap *regmap = isc->regmap;
	struct isc_ctrls *ctrls = &isc->ctrls;
	u32 *hist_count = &ctrls->hist_count[ctrls->hist_id];
	u32 *hist_entry = &ctrls->hist_entry[0];
	u32 i;

	*min = 0;
	*max = HIST_ENTRIES;

	regmap_bulk_read(regmap, ISC_HIS_ENTRY + isc->offsets.his_entry,
			 hist_entry, HIST_ENTRIES);

	*hist_count = 0;
	 
	for (i = 1; i < HIST_ENTRIES; i++) {
		if (*hist_entry && !*min)
			*min = i;
		if (*hist_entry)
			*max = i;
		*hist_count += i * (*hist_entry++);
	}

	if (!*min)
		*min = 1;

	v4l2_dbg(1, debug, &isc->v4l2_dev,
		 "isc wb: hist_id %u, hist_count %u",
		 ctrls->hist_id, *hist_count);
}

static void isc_wb_update(struct isc_ctrls *ctrls)
{
	struct isc_device *isc = container_of(ctrls, struct isc_device, ctrls);
	u32 *hist_count = &ctrls->hist_count[0];
	u32 c, offset[4];
	u64 avg = 0;
	 
	u32 s_gain[4], gw_gain[4];

	 
	avg = (u64)hist_count[ISC_HIS_CFG_MODE_GR] +
		(u64)hist_count[ISC_HIS_CFG_MODE_GB];
	avg >>= 1;

	v4l2_dbg(1, debug, &isc->v4l2_dev,
		 "isc wb: green components average %llu\n", avg);

	 
	if (!avg)
		return;

	for (c = ISC_HIS_CFG_MODE_GR; c <= ISC_HIS_CFG_MODE_B; c++) {
		 
		offset[c] = ctrls->hist_minmax[c][HIST_MIN_INDEX];
		 
		ctrls->offset[c] = (offset[c] - 1) << 3;

		 
		ctrls->offset[c] = -ctrls->offset[c];

		 
		s_gain[c] = (HIST_ENTRIES << 9) /
			(ctrls->hist_minmax[c][HIST_MAX_INDEX] -
			ctrls->hist_minmax[c][HIST_MIN_INDEX] + 1);

		 
		if (hist_count[c])
			gw_gain[c] = div_u64(avg << 9, hist_count[c]);
		else
			gw_gain[c] = 1 << 9;

		v4l2_dbg(1, debug, &isc->v4l2_dev,
			 "isc wb: component %d, s_gain %u, gw_gain %u\n",
			 c, s_gain[c], gw_gain[c]);
		 
		ctrls->gain[c] = s_gain[c] * gw_gain[c];
		ctrls->gain[c] >>= 9;

		 
		ctrls->gain[c] = clamp_val(ctrls->gain[c], 0, GENMASK(12, 0));

		v4l2_dbg(1, debug, &isc->v4l2_dev,
			 "isc wb: component %d, final gain %u\n",
			 c, ctrls->gain[c]);
	}
}

static void isc_awb_work(struct work_struct *w)
{
	struct isc_device *isc =
		container_of(w, struct isc_device, awb_work);
	struct regmap *regmap = isc->regmap;
	struct isc_ctrls *ctrls = &isc->ctrls;
	u32 hist_id = ctrls->hist_id;
	u32 baysel;
	unsigned long flags;
	u32 min, max;
	int ret;

	if (ctrls->hist_stat != HIST_ENABLED)
		return;

	isc_hist_count(isc, &min, &max);

	v4l2_dbg(1, debug, &isc->v4l2_dev,
		 "isc wb mode %d: hist min %u , max %u\n", hist_id, min, max);

	ctrls->hist_minmax[hist_id][HIST_MIN_INDEX] = min;
	ctrls->hist_minmax[hist_id][HIST_MAX_INDEX] = max;

	if (hist_id != ISC_HIS_CFG_MODE_B) {
		hist_id++;
	} else {
		isc_wb_update(ctrls);
		hist_id = ISC_HIS_CFG_MODE_GR;
	}

	ctrls->hist_id = hist_id;
	baysel = isc->config.sd_format->cfa_baycfg << ISC_HIS_CFG_BAYSEL_SHIFT;

	ret = pm_runtime_resume_and_get(isc->dev);
	if (ret < 0)
		return;

	 
	if (hist_id == ISC_HIS_CFG_MODE_GR || ctrls->awb == ISC_WB_NONE) {
		 
		spin_lock_irqsave(&isc->awb_lock, flags);
		isc_update_awb_ctrls(isc);
		spin_unlock_irqrestore(&isc->awb_lock, flags);

		 
		if (ctrls->awb == ISC_WB_ONETIME) {
			v4l2_info(&isc->v4l2_dev,
				  "Completed one time white-balance adjustment.\n");
			 
			isc_update_v4l2_ctrls(isc);
			ctrls->awb = ISC_WB_NONE;
		}
	}
	regmap_write(regmap, ISC_HIS_CFG + isc->offsets.his,
		     hist_id | baysel | ISC_HIS_CFG_RAR);

	 
	mutex_lock(&isc->awb_mutex);

	 
	if (isc->stop) {
		mutex_unlock(&isc->awb_mutex);
		return;
	}

	isc_update_profile(isc);

	mutex_unlock(&isc->awb_mutex);

	 
	if (ctrls->awb)
		regmap_write(regmap, ISC_CTRLEN, ISC_CTRL_HISREQ);

	pm_runtime_put_sync(isc->dev);
}

static int isc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct isc_device *isc = container_of(ctrl->handler,
					     struct isc_device, ctrls.handler);
	struct isc_ctrls *ctrls = &isc->ctrls;

	if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ctrls->brightness = ctrl->val & ISC_CBC_BRIGHT_MASK;
		break;
	case V4L2_CID_CONTRAST:
		ctrls->contrast = ctrl->val & ISC_CBC_CONTRAST_MASK;
		break;
	case V4L2_CID_GAMMA:
		ctrls->gamma_index = ctrl->val;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops isc_ctrl_ops = {
	.s_ctrl	= isc_s_ctrl,
};

static int isc_s_awb_ctrl(struct v4l2_ctrl *ctrl)
{
	struct isc_device *isc = container_of(ctrl->handler,
					     struct isc_device, ctrls.handler);
	struct isc_ctrls *ctrls = &isc->ctrls;

	if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_AUTO_WHITE_BALANCE:
		if (ctrl->val == 1)
			ctrls->awb = ISC_WB_AUTO;
		else
			ctrls->awb = ISC_WB_NONE;

		 
		if (ctrl->cluster[ISC_CTRL_R_GAIN]->is_new)
			ctrls->gain[ISC_HIS_CFG_MODE_R] = isc->r_gain_ctrl->val;
		if (ctrl->cluster[ISC_CTRL_B_GAIN]->is_new)
			ctrls->gain[ISC_HIS_CFG_MODE_B] = isc->b_gain_ctrl->val;
		if (ctrl->cluster[ISC_CTRL_GR_GAIN]->is_new)
			ctrls->gain[ISC_HIS_CFG_MODE_GR] = isc->gr_gain_ctrl->val;
		if (ctrl->cluster[ISC_CTRL_GB_GAIN]->is_new)
			ctrls->gain[ISC_HIS_CFG_MODE_GB] = isc->gb_gain_ctrl->val;

		if (ctrl->cluster[ISC_CTRL_R_OFF]->is_new)
			ctrls->offset[ISC_HIS_CFG_MODE_R] = isc->r_off_ctrl->val;
		if (ctrl->cluster[ISC_CTRL_B_OFF]->is_new)
			ctrls->offset[ISC_HIS_CFG_MODE_B] = isc->b_off_ctrl->val;
		if (ctrl->cluster[ISC_CTRL_GR_OFF]->is_new)
			ctrls->offset[ISC_HIS_CFG_MODE_GR] = isc->gr_off_ctrl->val;
		if (ctrl->cluster[ISC_CTRL_GB_OFF]->is_new)
			ctrls->offset[ISC_HIS_CFG_MODE_GB] = isc->gb_off_ctrl->val;

		isc_update_awb_ctrls(isc);

		mutex_lock(&isc->awb_mutex);
		if (vb2_is_streaming(&isc->vb2_vidq)) {
			 
			isc_update_profile(isc);
		} else {
			 
			v4l2_ctrl_activate(isc->do_wb_ctrl, false);
		}
		mutex_unlock(&isc->awb_mutex);

		 
		if (ctrls->awb == ISC_WB_AUTO &&
		    vb2_is_streaming(&isc->vb2_vidq) &&
		    ISC_IS_FORMAT_RAW(isc->config.sd_format->mbus_code))
			isc_set_histogram(isc, true);

		 
		if (ctrls->awb == ISC_WB_NONE &&
		    ctrl->cluster[ISC_CTRL_DO_WB]->is_new &&
		    !(ctrl->cluster[ISC_CTRL_DO_WB]->flags &
		    V4L2_CTRL_FLAG_INACTIVE)) {
			ctrls->awb = ISC_WB_ONETIME;
			isc_set_histogram(isc, true);
			v4l2_dbg(1, debug, &isc->v4l2_dev,
				 "One time white-balance started.\n");
		}
		return 0;
	}
	return 0;
}

static int isc_g_volatile_awb_ctrl(struct v4l2_ctrl *ctrl)
{
	struct isc_device *isc = container_of(ctrl->handler,
					     struct isc_device, ctrls.handler);
	struct isc_ctrls *ctrls = &isc->ctrls;

	switch (ctrl->id) {
	 
	case V4L2_CID_AUTO_WHITE_BALANCE:
		ctrl->cluster[ISC_CTRL_R_GAIN]->val =
					ctrls->gain[ISC_HIS_CFG_MODE_R];
		ctrl->cluster[ISC_CTRL_B_GAIN]->val =
					ctrls->gain[ISC_HIS_CFG_MODE_B];
		ctrl->cluster[ISC_CTRL_GR_GAIN]->val =
					ctrls->gain[ISC_HIS_CFG_MODE_GR];
		ctrl->cluster[ISC_CTRL_GB_GAIN]->val =
					ctrls->gain[ISC_HIS_CFG_MODE_GB];

		ctrl->cluster[ISC_CTRL_R_OFF]->val =
			ctrls->offset[ISC_HIS_CFG_MODE_R];
		ctrl->cluster[ISC_CTRL_B_OFF]->val =
			ctrls->offset[ISC_HIS_CFG_MODE_B];
		ctrl->cluster[ISC_CTRL_GR_OFF]->val =
			ctrls->offset[ISC_HIS_CFG_MODE_GR];
		ctrl->cluster[ISC_CTRL_GB_OFF]->val =
			ctrls->offset[ISC_HIS_CFG_MODE_GB];
		break;
	}
	return 0;
}

static const struct v4l2_ctrl_ops isc_awb_ops = {
	.s_ctrl = isc_s_awb_ctrl,
	.g_volatile_ctrl = isc_g_volatile_awb_ctrl,
};

#define ISC_CTRL_OFF(_name, _id, _name_str) \
	static const struct v4l2_ctrl_config _name = { \
		.ops = &isc_awb_ops, \
		.id = _id, \
		.name = _name_str, \
		.type = V4L2_CTRL_TYPE_INTEGER, \
		.flags = V4L2_CTRL_FLAG_SLIDER, \
		.min = -4095, \
		.max = 4095, \
		.step = 1, \
		.def = 0, \
	}

ISC_CTRL_OFF(isc_r_off_ctrl, ISC_CID_R_OFFSET, "Red Component Offset");
ISC_CTRL_OFF(isc_b_off_ctrl, ISC_CID_B_OFFSET, "Blue Component Offset");
ISC_CTRL_OFF(isc_gr_off_ctrl, ISC_CID_GR_OFFSET, "Green Red Component Offset");
ISC_CTRL_OFF(isc_gb_off_ctrl, ISC_CID_GB_OFFSET, "Green Blue Component Offset");

#define ISC_CTRL_GAIN(_name, _id, _name_str) \
	static const struct v4l2_ctrl_config _name = { \
		.ops = &isc_awb_ops, \
		.id = _id, \
		.name = _name_str, \
		.type = V4L2_CTRL_TYPE_INTEGER, \
		.flags = V4L2_CTRL_FLAG_SLIDER, \
		.min = 0, \
		.max = 8191, \
		.step = 1, \
		.def = 512, \
	}

ISC_CTRL_GAIN(isc_r_gain_ctrl, ISC_CID_R_GAIN, "Red Component Gain");
ISC_CTRL_GAIN(isc_b_gain_ctrl, ISC_CID_B_GAIN, "Blue Component Gain");
ISC_CTRL_GAIN(isc_gr_gain_ctrl, ISC_CID_GR_GAIN, "Green Red Component Gain");
ISC_CTRL_GAIN(isc_gb_gain_ctrl, ISC_CID_GB_GAIN, "Green Blue Component Gain");

static int isc_ctrl_init(struct isc_device *isc)
{
	const struct v4l2_ctrl_ops *ops = &isc_ctrl_ops;
	struct isc_ctrls *ctrls = &isc->ctrls;
	struct v4l2_ctrl_handler *hdl = &ctrls->handler;
	int ret;

	ctrls->hist_stat = HIST_INIT;
	isc_reset_awb_ctrls(isc);

	ret = v4l2_ctrl_handler_init(hdl, 13);
	if (ret < 0)
		return ret;

	 
	isc->config_ctrls(isc, ops);

	ctrls->brightness = 0;

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_BRIGHTNESS, -1024, 1023, 1, 0);
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_GAMMA, 0, isc->gamma_max, 1,
			  isc->gamma_max);
	isc->awb_ctrl = v4l2_ctrl_new_std(hdl, &isc_awb_ops,
					  V4L2_CID_AUTO_WHITE_BALANCE,
					  0, 1, 1, 1);

	 
	isc->do_wb_ctrl = v4l2_ctrl_new_std(hdl, &isc_awb_ops,
					    V4L2_CID_DO_WHITE_BALANCE,
					    0, 0, 0, 0);

	if (!isc->do_wb_ctrl) {
		ret = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		return ret;
	}

	v4l2_ctrl_activate(isc->do_wb_ctrl, false);

	isc->r_gain_ctrl = v4l2_ctrl_new_custom(hdl, &isc_r_gain_ctrl, NULL);
	isc->b_gain_ctrl = v4l2_ctrl_new_custom(hdl, &isc_b_gain_ctrl, NULL);
	isc->gr_gain_ctrl = v4l2_ctrl_new_custom(hdl, &isc_gr_gain_ctrl, NULL);
	isc->gb_gain_ctrl = v4l2_ctrl_new_custom(hdl, &isc_gb_gain_ctrl, NULL);
	isc->r_off_ctrl = v4l2_ctrl_new_custom(hdl, &isc_r_off_ctrl, NULL);
	isc->b_off_ctrl = v4l2_ctrl_new_custom(hdl, &isc_b_off_ctrl, NULL);
	isc->gr_off_ctrl = v4l2_ctrl_new_custom(hdl, &isc_gr_off_ctrl, NULL);
	isc->gb_off_ctrl = v4l2_ctrl_new_custom(hdl, &isc_gb_off_ctrl, NULL);

	 
	v4l2_ctrl_auto_cluster(10, &isc->awb_ctrl, 0, true);

	v4l2_ctrl_handler_setup(hdl);

	return 0;
}

static int isc_async_bound(struct v4l2_async_notifier *notifier,
			    struct v4l2_subdev *subdev,
			    struct v4l2_async_connection *asd)
{
	struct isc_device *isc = container_of(notifier->v4l2_dev,
					      struct isc_device, v4l2_dev);
	struct isc_subdev_entity *subdev_entity =
		container_of(notifier, struct isc_subdev_entity, notifier);

	if (video_is_registered(&isc->video_dev)) {
		v4l2_err(&isc->v4l2_dev, "only supports one sub-device.\n");
		return -EBUSY;
	}

	subdev_entity->sd = subdev;

	return 0;
}

static void isc_async_unbind(struct v4l2_async_notifier *notifier,
			      struct v4l2_subdev *subdev,
			      struct v4l2_async_connection *asd)
{
	struct isc_device *isc = container_of(notifier->v4l2_dev,
					      struct isc_device, v4l2_dev);
	mutex_destroy(&isc->awb_mutex);
	cancel_work_sync(&isc->awb_work);
	video_unregister_device(&isc->video_dev);
	v4l2_ctrl_handler_free(&isc->ctrls.handler);
}

static struct isc_format *find_format_by_code(struct isc_device *isc,
					      unsigned int code, int *index)
{
	struct isc_format *fmt = &isc->formats_list[0];
	unsigned int i;

	for (i = 0; i < isc->formats_list_size; i++) {
		if (fmt->mbus_code == code) {
			*index = i;
			return fmt;
		}

		fmt++;
	}

	return NULL;
}

static int isc_formats_init(struct isc_device *isc)
{
	struct isc_format *fmt;
	struct v4l2_subdev *subdev = isc->current_subdev->sd;
	unsigned int num_fmts, i, j;
	u32 list_size = isc->formats_list_size;
	struct v4l2_subdev_mbus_code_enum mbus_code = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};

	num_fmts = 0;
	while (!v4l2_subdev_call(subdev, pad, enum_mbus_code,
	       NULL, &mbus_code)) {
		mbus_code.index++;

		fmt = find_format_by_code(isc, mbus_code.code, &i);
		if (!fmt) {
			v4l2_warn(&isc->v4l2_dev, "Mbus code %x not supported\n",
				  mbus_code.code);
			continue;
		}

		fmt->sd_support = true;
		num_fmts++;
	}

	if (!num_fmts)
		return -ENXIO;

	isc->num_user_formats = num_fmts;
	isc->user_formats = devm_kcalloc(isc->dev,
					 num_fmts, sizeof(*isc->user_formats),
					 GFP_KERNEL);
	if (!isc->user_formats)
		return -ENOMEM;

	fmt = &isc->formats_list[0];
	for (i = 0, j = 0; i < list_size; i++) {
		if (fmt->sd_support)
			isc->user_formats[j++] = fmt;
		fmt++;
	}

	return 0;
}

static int isc_set_default_fmt(struct isc_device *isc)
{
	struct v4l2_format f = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.fmt.pix = {
			.width		= VGA_WIDTH,
			.height		= VGA_HEIGHT,
			.field		= V4L2_FIELD_NONE,
			.pixelformat	= isc->user_formats[0]->fourcc,
		},
	};
	int ret;

	ret = isc_try_fmt(isc, &f, NULL);
	if (ret)
		return ret;

	isc->fmt = f;
	return 0;
}

static int isc_async_complete(struct v4l2_async_notifier *notifier)
{
	struct isc_device *isc = container_of(notifier->v4l2_dev,
					      struct isc_device, v4l2_dev);
	struct video_device *vdev = &isc->video_dev;
	struct vb2_queue *q = &isc->vb2_vidq;
	int ret = 0;

	INIT_WORK(&isc->awb_work, isc_awb_work);

	ret = v4l2_device_register_subdev_nodes(&isc->v4l2_dev);
	if (ret < 0) {
		v4l2_err(&isc->v4l2_dev, "Failed to register subdev nodes\n");
		return ret;
	}

	isc->current_subdev = container_of(notifier,
					   struct isc_subdev_entity, notifier);
	mutex_init(&isc->lock);
	mutex_init(&isc->awb_mutex);

	init_completion(&isc->comp);

	 
	q->type			= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes		= VB2_MMAP | VB2_DMABUF | VB2_READ;
	q->drv_priv		= isc;
	q->buf_struct_size	= sizeof(struct isc_buffer);
	q->ops			= &isc_vb2_ops;
	q->mem_ops		= &vb2_dma_contig_memops;
	q->timestamp_flags	= V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock			= &isc->lock;
	q->min_buffers_needed	= 1;
	q->dev			= isc->dev;

	ret = vb2_queue_init(q);
	if (ret < 0) {
		v4l2_err(&isc->v4l2_dev,
			 "vb2_queue_init() failed: %d\n", ret);
		goto isc_async_complete_err;
	}

	 
	INIT_LIST_HEAD(&isc->dma_queue);
	spin_lock_init(&isc->dma_queue_lock);
	spin_lock_init(&isc->awb_lock);

	ret = isc_formats_init(isc);
	if (ret < 0) {
		v4l2_err(&isc->v4l2_dev,
			 "Init format failed: %d\n", ret);
		goto isc_async_complete_err;
	}

	ret = isc_set_default_fmt(isc);
	if (ret) {
		v4l2_err(&isc->v4l2_dev, "Could not set default format\n");
		goto isc_async_complete_err;
	}

	ret = isc_ctrl_init(isc);
	if (ret) {
		v4l2_err(&isc->v4l2_dev, "Init isc ctrols failed: %d\n", ret);
		goto isc_async_complete_err;
	}

	 
	strscpy(vdev->name, KBUILD_MODNAME, sizeof(vdev->name));
	vdev->release		= video_device_release_empty;
	vdev->fops		= &isc_fops;
	vdev->ioctl_ops		= &isc_ioctl_ops;
	vdev->v4l2_dev		= &isc->v4l2_dev;
	vdev->vfl_dir		= VFL_DIR_RX;
	vdev->queue		= q;
	vdev->lock		= &isc->lock;
	vdev->ctrl_handler	= &isc->ctrls.handler;
	vdev->device_caps	= V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE;
	video_set_drvdata(vdev, isc);

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		v4l2_err(&isc->v4l2_dev,
			 "video_register_device failed: %d\n", ret);
		goto isc_async_complete_err;
	}

	return 0;

isc_async_complete_err:
	mutex_destroy(&isc->awb_mutex);
	mutex_destroy(&isc->lock);
	return ret;
}

const struct v4l2_async_notifier_operations atmel_isc_async_ops = {
	.bound = isc_async_bound,
	.unbind = isc_async_unbind,
	.complete = isc_async_complete,
};
EXPORT_SYMBOL_GPL(atmel_isc_async_ops);

void atmel_isc_subdev_cleanup(struct isc_device *isc)
{
	struct isc_subdev_entity *subdev_entity;

	list_for_each_entry(subdev_entity, &isc->subdev_entities, list) {
		v4l2_async_nf_unregister(&subdev_entity->notifier);
		v4l2_async_nf_cleanup(&subdev_entity->notifier);
	}

	INIT_LIST_HEAD(&isc->subdev_entities);
}
EXPORT_SYMBOL_GPL(atmel_isc_subdev_cleanup);

int atmel_isc_pipeline_init(struct isc_device *isc)
{
	struct device *dev = isc->dev;
	struct regmap *regmap = isc->regmap;
	struct regmap_field *regs;
	unsigned int i;

	 
	const struct reg_field regfields[ISC_PIPE_LINE_NODE_NUM] = {
		REG_FIELD(ISC_DPC_CTRL, 0, 0),
		REG_FIELD(ISC_DPC_CTRL, 1, 1),
		REG_FIELD(ISC_DPC_CTRL, 2, 2),
		REG_FIELD(ISC_WB_CTRL, 0, 0),
		REG_FIELD(ISC_CFA_CTRL, 0, 0),
		REG_FIELD(ISC_CC_CTRL, 0, 0),
		REG_FIELD(ISC_GAM_CTRL, 0, 0),
		REG_FIELD(ISC_GAM_CTRL, 1, 1),
		REG_FIELD(ISC_GAM_CTRL, 2, 2),
		REG_FIELD(ISC_GAM_CTRL, 3, 3),
		REG_FIELD(ISC_VHXS_CTRL, 0, 0),
		REG_FIELD(ISC_CSC_CTRL + isc->offsets.csc, 0, 0),
		REG_FIELD(ISC_CBC_CTRL + isc->offsets.cbc, 0, 0),
		REG_FIELD(ISC_SUB422_CTRL + isc->offsets.sub422, 0, 0),
		REG_FIELD(ISC_SUB420_CTRL + isc->offsets.sub420, 0, 0),
	};

	for (i = 0; i < ISC_PIPE_LINE_NODE_NUM; i++) {
		regs = devm_regmap_field_alloc(dev, regmap, regfields[i]);
		if (IS_ERR(regs))
			return PTR_ERR(regs);

		isc->pipeline[i] =  regs;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(atmel_isc_pipeline_init);

 
#define ATMEL_ISC_REG_MAX    0xd5c
const struct regmap_config atmel_isc_regmap_config = {
	.reg_bits       = 32,
	.reg_stride     = 4,
	.val_bits       = 32,
	.max_register	= ATMEL_ISC_REG_MAX,
};
EXPORT_SYMBOL_GPL(atmel_isc_regmap_config);

MODULE_AUTHOR("Songjun Wu");
MODULE_AUTHOR("Eugen Hristev");
MODULE_DESCRIPTION("Atmel ISC common code base");
MODULE_LICENSE("GPL v2");
