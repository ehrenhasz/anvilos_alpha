#ifndef HVA_H
#define HVA_H
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-mem2mem.h>
#define fh_to_ctx(f)    (container_of(f, struct hva_ctx, fh))
#define hva_to_dev(h)   (h->dev)
#define ctx_to_dev(c)   (c->hva_dev->dev)
#define ctx_to_hdev(c)  (c->hva_dev)
#define HVA_NAME	"st-hva"
#define HVA_PREFIX	"[---:----]"
extern const struct hva_enc nv12h264enc;
extern const struct hva_enc nv21h264enc;
struct hva_frameinfo {
	u32	pixelformat;
	u32	width;
	u32	height;
	u32	aligned_width;
	u32	aligned_height;
	u32	size;
};
struct hva_streaminfo {
	u32	streamformat;
	u32	width;
	u32	height;
	u8	profile[32];
	u8	level[32];
};
struct hva_controls {
	struct v4l2_fract					time_per_frame;
	enum v4l2_mpeg_video_bitrate_mode			bitrate_mode;
	u32							gop_size;
	u32							bitrate;
	enum v4l2_mpeg_video_aspect				aspect;
	enum v4l2_mpeg_video_h264_profile			profile;
	enum v4l2_mpeg_video_h264_level				level;
	enum v4l2_mpeg_video_h264_entropy_mode			entropy_mode;
	u32							cpb_size;
	bool							dct8x8;
	u32							qpmin;
	u32							qpmax;
	bool							vui_sar;
	enum v4l2_mpeg_video_h264_vui_sar_idc			vui_sar_idc;
	bool							sei_fp;
	enum v4l2_mpeg_video_h264_sei_fp_arrangement_type	sei_fp_type;
};
struct hva_frame {
	struct vb2_v4l2_buffer	vbuf;
	struct list_head	list;
	struct hva_frameinfo	info;
	dma_addr_t		paddr;
	void			*vaddr;
	bool			prepared;
};
#define to_hva_frame(vb) \
	container_of(vb, struct hva_frame, vbuf)
struct hva_stream {
	struct vb2_v4l2_buffer	vbuf;
	struct list_head	list;
	dma_addr_t		paddr;
	void			*vaddr;
	bool			prepared;
	unsigned int		size;
	unsigned int		bytesused;
};
#define to_hva_stream(vb) \
	container_of(vb, struct hva_stream, vbuf)
#ifdef CONFIG_VIDEO_STI_HVA_DEBUGFS
struct hva_ctx_dbg {
	struct dentry	*debugfs_entry;
	bool		is_valid_period;
	ktime_t		begin;
	u32		total_duration;
	u32		cnt_duration;
	u32		min_duration;
	u32		max_duration;
	u32		avg_duration;
	u32		max_fps;
	u32		total_period;
	u32		cnt_period;
	u32		min_period;
	u32		max_period;
	u32		avg_period;
	u32		total_stream_size;
	u32		avg_fps;
	u32		window_duration;
	u32		cnt_window;
	u32		window_stream_size;
	u32		last_bitrate;
	u32		min_bitrate;
	u32		max_bitrate;
	u32		avg_bitrate;
};
#endif
struct hva_dev;
struct hva_enc;
struct hva_ctx {
	struct hva_dev			*hva_dev;
	struct v4l2_fh			fh;
	struct v4l2_ctrl_handler	ctrl_handler;
	struct hva_controls		ctrls;
	u8				id;
	bool				aborting;
	char				name[100];
	struct work_struct		run_work;
	struct mutex			lock;
	u32				flags;
	u32				frame_num;
	u32				stream_num;
	u32				max_stream_size;
	enum v4l2_colorspace		colorspace;
	enum v4l2_xfer_func		xfer_func;
	enum v4l2_ycbcr_encoding	ycbcr_enc;
	enum v4l2_quantization		quantization;
	struct hva_streaminfo		streaminfo;
	struct hva_frameinfo		frameinfo;
	struct hva_enc			*enc;
	void				*priv;
	bool				hw_err;
	u32				encoded_frames;
	u32				sys_errors;
	u32				encode_errors;
	u32				frame_errors;
#ifdef CONFIG_VIDEO_STI_HVA_DEBUGFS
	struct hva_ctx_dbg		dbg;
#endif
};
#define HVA_FLAG_STREAMINFO	0x0001
#define HVA_FLAG_FRAMEINFO	0x0002
#ifdef CONFIG_VIDEO_STI_HVA_DEBUGFS
struct hva_dev_dbg {
	struct dentry	*debugfs_entry;
	struct hva_ctx	last_ctx;
};
#endif
#define HVA_MAX_INSTANCES	16
#define HVA_MAX_ENCODERS	10
#define HVA_MAX_FORMATS		HVA_MAX_ENCODERS
struct hva_dev {
	struct v4l2_device	v4l2_dev;
	struct video_device	*vdev;
	struct platform_device	*pdev;
	struct device		*dev;
	struct mutex		lock;
	struct v4l2_m2m_dev	*m2m_dev;
	struct hva_ctx		*instances[HVA_MAX_INSTANCES];
	unsigned int		nb_of_instances;
	unsigned int		instance_id;
	void __iomem		*regs;
	u32			esram_addr;
	u32			esram_size;
	struct clk		*clk;
	int			irq_its;
	int			irq_err;
	struct workqueue_struct *work_queue;
	struct mutex		protect_mutex;
	struct completion	interrupt;
	unsigned long int	ip_version;
	const struct hva_enc	*encoders[HVA_MAX_ENCODERS];
	u32			nb_of_encoders;
	u32			pixelformats[HVA_MAX_FORMATS];
	u32			nb_of_pixelformats;
	u32			streamformats[HVA_MAX_FORMATS];
	u32			nb_of_streamformats;
	u32			sfl_reg;
	u32			sts_reg;
	u32			lmi_err_reg;
	u32			emi_err_reg;
	u32			hec_mif_err_reg;
#ifdef CONFIG_VIDEO_STI_HVA_DEBUGFS
	struct hva_dev_dbg	dbg;
#endif
};
struct hva_enc {
	const char	*name;
	u32		streamformat;
	u32		pixelformat;
	u32		max_width;
	u32		max_height;
	int		(*open)(struct hva_ctx *ctx);
	int		(*close)(struct hva_ctx *ctx);
	int		(*encode)(struct hva_ctx *ctx, struct hva_frame *frame,
				  struct hva_stream *stream);
};
#ifdef CONFIG_VIDEO_STI_HVA_DEBUGFS
void hva_debugfs_create(struct hva_dev *hva);
void hva_debugfs_remove(struct hva_dev *hva);
void hva_dbg_ctx_create(struct hva_ctx *ctx);
void hva_dbg_ctx_remove(struct hva_ctx *ctx);
void hva_dbg_perf_begin(struct hva_ctx *ctx);
void hva_dbg_perf_end(struct hva_ctx *ctx, struct hva_stream *stream);
#endif
#endif  
