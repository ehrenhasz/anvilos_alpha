 
 

#ifndef _RKISP1_COMMON_H
#define _RKISP1_COMMON_H

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/rkisp1-config.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>

#include "rkisp1-regs.h"

struct dentry;

 
#define RKISP1_ISP_SD_SRC			BIT(0)
#define RKISP1_ISP_SD_SINK			BIT(1)

 
#define RKISP1_ISP_MAX_WIDTH			4032
#define RKISP1_ISP_MAX_HEIGHT			3024
#define RKISP1_ISP_MIN_WIDTH			32
#define RKISP1_ISP_MIN_HEIGHT			32

#define RKISP1_RSZ_MP_SRC_MAX_WIDTH		4416
#define RKISP1_RSZ_MP_SRC_MAX_HEIGHT		3312
#define RKISP1_RSZ_SP_SRC_MAX_WIDTH		1920
#define RKISP1_RSZ_SP_SRC_MAX_HEIGHT		1920
#define RKISP1_RSZ_SRC_MIN_WIDTH		32
#define RKISP1_RSZ_SRC_MIN_HEIGHT		16

 
#define RKISP1_DEFAULT_WIDTH			800
#define RKISP1_DEFAULT_HEIGHT			600

#define RKISP1_DRIVER_NAME			"rkisp1"
#define RKISP1_BUS_INFO				"platform:" RKISP1_DRIVER_NAME

 
#define RKISP1_MAX_BUS_CLK			8

 
#define RKISP1_STATS_MEAS_MASK			(RKISP1_CIF_ISP_AWB_DONE |	\
						 RKISP1_CIF_ISP_AFM_FIN |	\
						 RKISP1_CIF_ISP_EXP_END |	\
						 RKISP1_CIF_ISP_HIST_MEASURE_RDY)

 
enum rkisp1_rsz_pad {
	RKISP1_RSZ_PAD_SINK,
	RKISP1_RSZ_PAD_SRC,
	RKISP1_RSZ_PAD_MAX
};

 
enum rkisp1_csi_pad {
	RKISP1_CSI_PAD_SINK,
	RKISP1_CSI_PAD_SRC,
	RKISP1_CSI_PAD_NUM
};

 
enum rkisp1_stream_id {
	RKISP1_MAINPATH,
	RKISP1_SELFPATH,
};

 
enum rkisp1_fmt_raw_pat_type {
	RKISP1_RAW_RGGB = 0,
	RKISP1_RAW_GRBG,
	RKISP1_RAW_GBRG,
	RKISP1_RAW_BGGR,
};

 
enum rkisp1_isp_pad {
	RKISP1_ISP_PAD_SINK_VIDEO,
	RKISP1_ISP_PAD_SINK_PARAMS,
	RKISP1_ISP_PAD_SOURCE_VIDEO,
	RKISP1_ISP_PAD_SOURCE_STATS,
	RKISP1_ISP_PAD_MAX
};

 
enum rkisp1_feature {
	RKISP1_FEATURE_MIPI_CSI2 = BIT(0),
};

 
struct rkisp1_info {
	const char * const *clks;
	unsigned int clk_size;
	const struct rkisp1_isr_data *isrs;
	unsigned int isr_size;
	enum rkisp1_cif_isp_version isp_ver;
	unsigned int features;
};

 
struct rkisp1_sensor_async {
	struct v4l2_async_connection asd;
	unsigned int index;
	struct fwnode_handle *source_ep;
	unsigned int lanes;
	enum v4l2_mbus_type mbus_type;
	unsigned int mbus_flags;
	struct v4l2_subdev *sd;
	struct v4l2_ctrl *pixel_rate_ctrl;
	unsigned int port;
};

 
struct rkisp1_csi {
	struct rkisp1_device *rkisp1;
	struct phy *dphy;
	bool is_dphy_errctrl_disabled;
	struct v4l2_subdev sd;
	struct media_pad pads[RKISP1_CSI_PAD_NUM];
	struct v4l2_subdev_pad_config pad_cfg[RKISP1_CSI_PAD_NUM];
	const struct rkisp1_mbus_info *sink_fmt;
	struct mutex lock;
	struct v4l2_subdev *source;
};

 
struct rkisp1_isp {
	struct v4l2_subdev sd;
	struct rkisp1_device *rkisp1;
	struct media_pad pads[RKISP1_ISP_PAD_MAX];
	struct v4l2_subdev_pad_config pad_cfg[RKISP1_ISP_PAD_MAX];
	const struct rkisp1_mbus_info *sink_fmt;
	const struct rkisp1_mbus_info *src_fmt;
	struct mutex ops_lock;  
	__u32 frame_sequence;
};

 
struct rkisp1_vdev_node {
	struct vb2_queue buf_queue;
	struct mutex vlock;  
	struct video_device vdev;
	struct media_pad pad;
};

 
struct rkisp1_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head queue;
	u32 buff_addr[VIDEO_MAX_PLANES];
};

 
struct rkisp1_dummy_buffer {
	void *vaddr;
	dma_addr_t dma_addr;
	u32 size;
};

struct rkisp1_device;

 
struct rkisp1_capture {
	struct rkisp1_vdev_node vnode;
	struct rkisp1_device *rkisp1;
	enum rkisp1_stream_id id;
	const struct rkisp1_capture_ops *ops;
	const struct rkisp1_capture_config *config;
	bool is_streaming;
	bool is_stopping;
	wait_queue_head_t done;
	unsigned int sp_y_stride;
	struct {
		 
		spinlock_t lock;
		struct list_head queue;
		struct rkisp1_dummy_buffer dummy;
		struct rkisp1_buffer *curr;
		struct rkisp1_buffer *next;
	} buf;
	struct {
		const struct rkisp1_capture_fmt_cfg *cfg;
		const struct v4l2_format_info *info;
		struct v4l2_pix_format_mplane fmt;
	} pix;
};

struct rkisp1_stats;
struct rkisp1_stats_ops {
	void (*get_awb_meas)(struct rkisp1_stats *stats,
			     struct rkisp1_stat_buffer *pbuf);
	void (*get_aec_meas)(struct rkisp1_stats *stats,
			     struct rkisp1_stat_buffer *pbuf);
	void (*get_hst_meas)(struct rkisp1_stats *stats,
			     struct rkisp1_stat_buffer *pbuf);
};

 
struct rkisp1_stats {
	struct rkisp1_vdev_node vnode;
	struct rkisp1_device *rkisp1;
	const struct rkisp1_stats_ops *ops;

	spinlock_t lock;  
	struct list_head stat;
	struct v4l2_format vdev_fmt;
};

struct rkisp1_params;
struct rkisp1_params_ops {
	void (*lsc_matrix_config)(struct rkisp1_params *params,
				  const struct rkisp1_cif_isp_lsc_config *pconfig);
	void (*goc_config)(struct rkisp1_params *params,
			   const struct rkisp1_cif_isp_goc_config *arg);
	void (*awb_meas_config)(struct rkisp1_params *params,
				const struct rkisp1_cif_isp_awb_meas_config *arg);
	void (*awb_meas_enable)(struct rkisp1_params *params,
				const struct rkisp1_cif_isp_awb_meas_config *arg,
				bool en);
	void (*awb_gain_config)(struct rkisp1_params *params,
				const struct rkisp1_cif_isp_awb_gain_config *arg);
	void (*aec_config)(struct rkisp1_params *params,
			   const struct rkisp1_cif_isp_aec_config *arg);
	void (*hst_config)(struct rkisp1_params *params,
			   const struct rkisp1_cif_isp_hst_config *arg);
	void (*hst_enable)(struct rkisp1_params *params,
			   const struct rkisp1_cif_isp_hst_config *arg, bool en);
	void (*afm_config)(struct rkisp1_params *params,
			   const struct rkisp1_cif_isp_afc_config *arg);
};

 
struct rkisp1_params {
	struct rkisp1_vdev_node vnode;
	struct rkisp1_device *rkisp1;
	const struct rkisp1_params_ops *ops;

	spinlock_t config_lock;  
	struct list_head params;
	struct v4l2_format vdev_fmt;

	enum v4l2_quantization quantization;
	enum v4l2_ycbcr_encoding ycbcr_encoding;
	enum rkisp1_fmt_raw_pat_type raw_type;
};

 
struct rkisp1_resizer {
	struct v4l2_subdev sd;
	u32 regs_base;
	enum rkisp1_stream_id id;
	struct rkisp1_device *rkisp1;
	struct media_pad pads[RKISP1_RSZ_PAD_MAX];
	struct v4l2_subdev_pad_config pad_cfg[RKISP1_RSZ_PAD_MAX];
	const struct rkisp1_rsz_config *config;
	enum v4l2_pixel_encoding pixel_enc;
	struct mutex ops_lock;  
};

 
struct rkisp1_debug {
	struct dentry *debugfs_dir;
	unsigned long data_loss;
	unsigned long outform_size_error;
	unsigned long img_stabilization_size_error;
	unsigned long inform_size_error;
	unsigned long irq_delay;
	unsigned long mipi_error;
	unsigned long stats_error;
	unsigned long stop_timeout[2];
	unsigned long frame_drop[2];
};

 
struct rkisp1_device {
	void __iomem *base_addr;
	struct device *dev;
	unsigned int clk_size;
	struct clk_bulk_data clks[RKISP1_MAX_BUS_CLK];
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct v4l2_async_notifier notifier;
	struct v4l2_subdev *source;
	struct rkisp1_csi csi;
	struct rkisp1_isp isp;
	struct rkisp1_resizer resizer_devs[2];
	struct rkisp1_capture capture_devs[2];
	struct rkisp1_stats stats;
	struct rkisp1_params params;
	struct media_pipeline pipe;
	struct mutex stream_lock;  
	struct rkisp1_debug debug;
	const struct rkisp1_info *info;
};

 
struct rkisp1_mbus_info {
	u32 mbus_code;
	enum v4l2_pixel_encoding pixel_enc;
	u32 mipi_dt;
	u32 yuv_seq;
	u8 bus_width;
	enum rkisp1_fmt_raw_pat_type bayer_pat;
	unsigned int direction;
};

static inline void
rkisp1_write(struct rkisp1_device *rkisp1, unsigned int addr, u32 val)
{
	writel(val, rkisp1->base_addr + addr);
}

static inline u32 rkisp1_read(struct rkisp1_device *rkisp1, unsigned int addr)
{
	return readl(rkisp1->base_addr + addr);
}

 
int rkisp1_cap_enum_mbus_codes(struct rkisp1_capture *cap,
			       struct v4l2_subdev_mbus_code_enum *code);

 
const struct rkisp1_mbus_info *rkisp1_mbus_info_get_by_index(unsigned int index);

 
void rkisp1_sd_adjust_crop_rect(struct v4l2_rect *crop,
				const struct v4l2_rect *bounds);

 
void rkisp1_sd_adjust_crop(struct v4l2_rect *crop,
			   const struct v4l2_mbus_framefmt *bounds);

 
const struct rkisp1_mbus_info *rkisp1_mbus_info_get_by_code(u32 mbus_code);

 
void rkisp1_params_pre_configure(struct rkisp1_params *params,
				 enum rkisp1_fmt_raw_pat_type bayer_pat,
				 enum v4l2_quantization quantization,
				 enum v4l2_ycbcr_encoding ycbcr_encoding);

 
void rkisp1_params_post_configure(struct rkisp1_params *params);

 
void rkisp1_params_disable(struct rkisp1_params *params);

 
irqreturn_t rkisp1_isp_isr(int irq, void *ctx);
irqreturn_t rkisp1_csi_isr(int irq, void *ctx);
irqreturn_t rkisp1_capture_isr(int irq, void *ctx);
void rkisp1_stats_isr(struct rkisp1_stats *stats, u32 isp_ris);
void rkisp1_params_isr(struct rkisp1_device *rkisp1);

 
int rkisp1_capture_devs_register(struct rkisp1_device *rkisp1);
void rkisp1_capture_devs_unregister(struct rkisp1_device *rkisp1);

int rkisp1_isp_register(struct rkisp1_device *rkisp1);
void rkisp1_isp_unregister(struct rkisp1_device *rkisp1);

int rkisp1_resizer_devs_register(struct rkisp1_device *rkisp1);
void rkisp1_resizer_devs_unregister(struct rkisp1_device *rkisp1);

int rkisp1_stats_register(struct rkisp1_device *rkisp1);
void rkisp1_stats_unregister(struct rkisp1_device *rkisp1);

int rkisp1_params_register(struct rkisp1_device *rkisp1);
void rkisp1_params_unregister(struct rkisp1_device *rkisp1);

#if IS_ENABLED(CONFIG_DEBUG_FS)
void rkisp1_debug_init(struct rkisp1_device *rkisp1);
void rkisp1_debug_cleanup(struct rkisp1_device *rkisp1);
#else
static inline void rkisp1_debug_init(struct rkisp1_device *rkisp1)
{
}
static inline void rkisp1_debug_cleanup(struct rkisp1_device *rkisp1)
{
}
#endif

#endif  
