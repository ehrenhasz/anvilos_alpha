#ifndef __TEGRA_VI_H__
#define __TEGRA_VI_H__
#include <linux/host1x.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-v4l2.h>
#include "csi.h"
#define V4L2_CID_TEGRA_SYNCPT_TIMEOUT_RETRY	(V4L2_CTRL_CLASS_CAMERA | 0x1001)
#define TEGRA_DEF_WIDTH		1920
#define TEGRA_DEF_HEIGHT	1080
#define TEGRA_IMAGE_FORMAT_DEF	32
#define MAX_FORMAT_NUM		64
enum tegra_vi_pg_mode {
	TEGRA_VI_PG_DISABLED = 0,
	TEGRA_VI_PG_DIRECT,
	TEGRA_VI_PG_PATCH,
};
struct tegra_vi;
struct tegra_vi_channel;
struct tegra_vi_ops {
	int (*vi_enable)(struct tegra_vi *vi, bool on);
	int (*channel_host1x_syncpt_init)(struct tegra_vi_channel *chan);
	void (*channel_host1x_syncpt_free)(struct tegra_vi_channel *chan);
	void (*vi_fmt_align)(struct v4l2_pix_format *pix, unsigned int bpp);
	void (*channel_queue_setup)(struct tegra_vi_channel *chan);
	int (*vi_start_streaming)(struct vb2_queue *vq, u32 count);
	void (*vi_stop_streaming)(struct vb2_queue *vq);
};
struct tegra_vi_soc {
	const struct tegra_video_format *video_formats;
	const unsigned int nformats;
	const struct tegra_video_format *default_video_format;
	const struct tegra_vi_ops *ops;
	u32 hw_revision;
	unsigned int vi_max_channels;
	unsigned int vi_max_clk_hz;
	bool has_h_v_flip:1;
};
struct tegra_vi {
	struct device *dev;
	struct host1x_client client;
	void __iomem *iomem;
	struct clk *clk;
	struct regulator *vdd;
	const struct tegra_vi_soc *soc;
	const struct tegra_vi_ops *ops;
	struct list_head vi_chans;
};
struct tegra_vi_channel {
	struct list_head list;
	struct video_device video;
	struct mutex video_lock;
	struct media_pad pad;
	struct tegra_vi *vi;
	struct host1x_syncpt *frame_start_sp[GANG_PORTS_MAX];
	struct host1x_syncpt *mw_ack_sp[GANG_PORTS_MAX];
	spinlock_t sp_incr_lock[GANG_PORTS_MAX];
	u32 next_out_sp_idx;
	struct task_struct *kthread_start_capture;
	wait_queue_head_t start_wait;
	struct task_struct *kthread_finish_capture;
	wait_queue_head_t done_wait;
	struct v4l2_pix_format format;
	const struct tegra_video_format *fmtinfo;
	struct vb2_queue queue;
	u32 sequence;
	unsigned int addr_offset_u;
	unsigned int addr_offset_v;
	unsigned int start_offset;
	unsigned int start_offset_u;
	unsigned int start_offset_v;
	struct list_head capture;
	spinlock_t start_lock;
	struct list_head done;
	spinlock_t done_lock;
	unsigned char portnos[GANG_PORTS_MAX];
	u8 totalports;
	u8 numgangports;
	struct device_node *of_node;
	struct v4l2_ctrl_handler ctrl_handler;
	unsigned int syncpt_timeout_retry;
	DECLARE_BITMAP(fmts_bitmap, MAX_FORMAT_NUM);
	DECLARE_BITMAP(tpg_fmts_bitmap, MAX_FORMAT_NUM);
	enum tegra_vi_pg_mode pg_mode;
	struct v4l2_async_notifier notifier;
	bool hflip:1;
	bool vflip:1;
};
struct tegra_channel_buffer {
	struct vb2_v4l2_buffer buf;
	struct list_head queue;
	struct tegra_vi_channel *chan;
	dma_addr_t addr;
	u32 mw_ack_sp_thresh[GANG_PORTS_MAX];
};
enum tegra_image_dt {
	TEGRA_IMAGE_DT_YUV420_8 = 24,
	TEGRA_IMAGE_DT_YUV420_10,
	TEGRA_IMAGE_DT_YUV420CSPS_8 = 28,
	TEGRA_IMAGE_DT_YUV420CSPS_10,
	TEGRA_IMAGE_DT_YUV422_8,
	TEGRA_IMAGE_DT_YUV422_10,
	TEGRA_IMAGE_DT_RGB444,
	TEGRA_IMAGE_DT_RGB555,
	TEGRA_IMAGE_DT_RGB565,
	TEGRA_IMAGE_DT_RGB666,
	TEGRA_IMAGE_DT_RGB888,
	TEGRA_IMAGE_DT_RAW6 = 40,
	TEGRA_IMAGE_DT_RAW7,
	TEGRA_IMAGE_DT_RAW8,
	TEGRA_IMAGE_DT_RAW10,
	TEGRA_IMAGE_DT_RAW12,
	TEGRA_IMAGE_DT_RAW14,
};
struct tegra_video_format {
	enum tegra_image_dt img_dt;
	unsigned int bit_width;
	unsigned int code;
	unsigned int bpp;
	u32 img_fmt;
	u32 fourcc;
};
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
extern const struct tegra_vi_soc tegra20_vi_soc;
#endif
#if defined(CONFIG_ARCH_TEGRA_210_SOC)
extern const struct tegra_vi_soc tegra210_vi_soc;
#endif
struct v4l2_subdev *
tegra_channel_get_remote_csi_subdev(struct tegra_vi_channel *chan);
struct v4l2_subdev *
tegra_channel_get_remote_source_subdev(struct tegra_vi_channel *chan);
int tegra_channel_set_stream(struct tegra_vi_channel *chan, bool on);
void tegra_channel_release_buffers(struct tegra_vi_channel *chan,
				   enum vb2_buffer_state state);
void tegra_channels_cleanup(struct tegra_vi *vi);
#endif
