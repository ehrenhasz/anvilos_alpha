#ifndef FIMC_ISP_H_
#define FIMC_ISP_H_
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/drv-intf/exynos-fimc.h>
extern int fimc_isp_debug;
#define isp_dbg(level, dev, fmt, arg...) \
	v4l2_dbg(level, fimc_isp_debug, dev, fmt, ## arg)
#define FIMC_ISP_SINK_WIDTH_MIN		(16 + 8)
#define FIMC_ISP_SINK_HEIGHT_MIN	(12 + 8)
#define FIMC_ISP_SOURCE_WIDTH_MIN	8
#define FIMC_ISP_SOURCE_HEIGHT_MIN	8
#define FIMC_ISP_CAC_MARGIN_WIDTH	16
#define FIMC_ISP_CAC_MARGIN_HEIGHT	12
#define FIMC_ISP_SINK_WIDTH_MAX		(4000 - 16)
#define FIMC_ISP_SINK_HEIGHT_MAX	(4000 + 12)
#define FIMC_ISP_SOURCE_WIDTH_MAX	4000
#define FIMC_ISP_SOURCE_HEIGHT_MAX	4000
#define FIMC_ISP_NUM_FORMATS		3
#define FIMC_ISP_REQ_BUFS_MIN		2
#define FIMC_ISP_REQ_BUFS_MAX		32
#define FIMC_ISP_SD_PAD_SINK		0
#define FIMC_ISP_SD_PAD_SRC_FIFO	1
#define FIMC_ISP_SD_PAD_SRC_DMA		2
#define FIMC_ISP_SD_PADS_NUM		3
#define FIMC_ISP_MAX_PLANES		1
struct fimc_isp_frame {
	u16 width;
	u16 height;
	struct v4l2_rect rect;
};
struct fimc_isp_ctrls {
	struct v4l2_ctrl_handler handler;
	struct v4l2_ctrl *auto_wb;
	struct {
		struct v4l2_ctrl *auto_iso;
		struct v4l2_ctrl *iso;
	};
	struct v4l2_ctrl *contrast;
	struct v4l2_ctrl *saturation;
	struct v4l2_ctrl *sharpness;
	struct v4l2_ctrl *brightness;
	struct v4l2_ctrl *hue;
	struct v4l2_ctrl *auto_exp;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *aewb_lock;
	struct v4l2_ctrl *exp_metering;
	struct v4l2_ctrl *afc;
	struct v4l2_ctrl *colorfx;
};
struct isp_video_buf {
	struct vb2_v4l2_buffer vb;
	dma_addr_t dma_addr[FIMC_ISP_MAX_PLANES];
	unsigned int index;
};
#define to_isp_video_buf(_b) container_of(_b, struct isp_video_buf, vb)
#define FIMC_ISP_MAX_BUFS	4
struct fimc_is_video {
	struct exynos_video_entity ve;
	enum v4l2_buf_type	type;
	struct media_pad	pad;
	struct list_head	pending_buf_q;
	struct list_head	active_buf_q;
	struct vb2_queue	vb_queue;
	unsigned int		reqbufs_count;
	unsigned int		buf_count;
	unsigned int		buf_mask;
	unsigned int		frame_count;
	int			streaming;
	struct isp_video_buf	*buffers[FIMC_ISP_MAX_BUFS];
	const struct fimc_fmt	*format;
	struct v4l2_pix_format_mplane pixfmt;
};
#define ST_ISP_VID_CAP_BUF_PREP		0
#define ST_ISP_VID_CAP_STREAMING	1
struct fimc_isp {
	struct platform_device		*pdev;
	struct v4l2_subdev		subdev;
	struct media_pad		subdev_pads[FIMC_ISP_SD_PADS_NUM];
	struct v4l2_mbus_framefmt	src_fmt;
	struct v4l2_mbus_framefmt	sink_fmt;
	struct v4l2_ctrl		*test_pattern;
	struct fimc_isp_ctrls		ctrls;
	struct mutex			video_lock;
	struct mutex			subdev_lock;
	unsigned int			cac_margin_x;
	unsigned int			cac_margin_y;
	unsigned long			state;
	struct fimc_is_video		video_capture;
};
#define ctrl_to_fimc_isp(_ctrl) \
	container_of(ctrl->handler, struct fimc_isp, ctrls.handler)
struct fimc_is;
int fimc_isp_subdev_create(struct fimc_isp *isp);
void fimc_isp_subdev_destroy(struct fimc_isp *isp);
void fimc_isp_irq_handler(struct fimc_is *is);
int fimc_is_create_controls(struct fimc_isp *isp);
int fimc_is_delete_controls(struct fimc_isp *isp);
const struct fimc_fmt *fimc_isp_find_format(const u32 *pixelformat,
					const u32 *mbus_code, int index);
#endif  
