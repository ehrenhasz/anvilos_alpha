#ifndef AM437X_VPFE_H
#define AM437X_VPFE_H
#include <linux/am437x-vpfe.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include "am437x-vpfe_regs.h"
enum vpfe_pin_pol {
	VPFE_PINPOL_POSITIVE = 0,
	VPFE_PINPOL_NEGATIVE,
};
enum vpfe_hw_if_type {
	VPFE_RAW_BAYER = 0,
	VPFE_BT656,
	VPFE_BT656_10BIT,
	VPFE_YCBCR_SYNC_8,
	VPFE_YCBCR_SYNC_16,
};
struct vpfe_hw_if_param {
	enum vpfe_hw_if_type if_type;
	enum vpfe_pin_pol hdpol;
	enum vpfe_pin_pol vdpol;
	unsigned int bus_width;
};
#define VPFE_MAX_SUBDEV		1
#define VPFE_MAX_INPUTS		1
struct vpfe_std_info {
	int active_pixels;
	int active_lines;
	int frame_format;
};
struct vpfe_route {
	u32 input;
	u32 output;
};
struct vpfe_subdev_info {
	int grp_id;
	struct v4l2_input inputs[VPFE_MAX_INPUTS];
	struct vpfe_route *routes;
	int can_route;
	struct vpfe_hw_if_param vpfe_param;
	struct v4l2_subdev *sd;
};
struct vpfe_config {
	struct vpfe_subdev_info sub_devs[VPFE_MAX_SUBDEV];
	struct v4l2_async_connection *asd[VPFE_MAX_SUBDEV];
};
struct vpfe_cap_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};
enum ccdc_pixfmt {
	CCDC_PIXFMT_RAW = 0,
	CCDC_PIXFMT_YCBCR_16BIT,
	CCDC_PIXFMT_YCBCR_8BIT,
};
enum ccdc_frmfmt {
	CCDC_FRMFMT_PROGRESSIVE = 0,
	CCDC_FRMFMT_INTERLACED,
};
enum ccdc_pixorder {
	CCDC_PIXORDER_YCBYCR,
	CCDC_PIXORDER_CBYCRY,
};
enum ccdc_buftype {
	CCDC_BUFTYPE_FLD_INTERLEAVED,
	CCDC_BUFTYPE_FLD_SEPARATED
};
static inline u8 ccdc_gamma_width_max_bit(enum vpfe_ccdc_gamma_width width)
{
	return 15 - width;
}
static inline u8 ccdc_data_size_max_bit(enum vpfe_ccdc_data_size sz)
{
	return sz == VPFE_CCDC_DATA_8BITS ? 7 : 15 - sz;
}
struct ccdc_params_raw {
	enum ccdc_pixfmt pix_fmt;
	enum ccdc_frmfmt frm_fmt;
	struct v4l2_rect win;
	unsigned int bytesperpixel;
	unsigned int bytesperline;
	enum vpfe_pin_pol fid_pol;
	enum vpfe_pin_pol vd_pol;
	enum vpfe_pin_pol hd_pol;
	enum ccdc_buftype buf_type;
	unsigned char image_invert_enable;
	struct vpfe_ccdc_config_params_raw config_params;
};
struct ccdc_params_ycbcr {
	enum ccdc_pixfmt pix_fmt;
	enum ccdc_frmfmt frm_fmt;
	struct v4l2_rect win;
	unsigned int bytesperpixel;
	unsigned int bytesperline;
	enum vpfe_pin_pol fid_pol;
	enum vpfe_pin_pol vd_pol;
	enum vpfe_pin_pol hd_pol;
	int bt656_enable;
	enum ccdc_pixorder pix_order;
	enum ccdc_buftype buf_type;
};
struct ccdc_config {
	enum vpfe_hw_if_type if_type;
	struct ccdc_params_raw bayer;
	struct ccdc_params_ycbcr ycbcr;
	void __iomem *base_addr;
};
struct vpfe_ccdc {
	struct ccdc_config ccdc_cfg;
	u32 ccdc_ctx[VPFE_REG_END / sizeof(u32)];
};
struct vpfe_fmt {
	u32 fourcc;
	u32 code;
	u32 bitsperpixel;
};
#define VPFE_NUM_FORMATS	10
struct vpfe_device {
	struct video_device video_dev;
	struct v4l2_subdev **sd;
	struct vpfe_config *cfg;
	struct v4l2_device v4l2_dev;
	struct device *pdev;
	struct v4l2_async_notifier notifier;
	unsigned field;
	unsigned sequence;
	struct vpfe_hw_if_param vpfe_if_params;
	struct vpfe_subdev_info *current_subdev;
	int current_input;
	struct vpfe_std_info std_info;
	int std_index;
	unsigned int irq;
	struct vpfe_cap_buffer *cur_frm;
	struct vpfe_cap_buffer *next_frm;
	struct v4l2_format fmt;
	struct vpfe_fmt *current_vpfe_fmt;
	struct vpfe_fmt	*active_fmt[VPFE_NUM_FORMATS];
	unsigned int num_active_fmt;
	struct v4l2_rect crop;
	struct vb2_queue buffer_queue;
	struct list_head dma_queue;
	spinlock_t dma_queue_lock;
	struct mutex lock;
	u32 field_off;
	struct vpfe_ccdc ccdc;
	int stopping;
	struct completion capture_stop;
};
#endif	 
