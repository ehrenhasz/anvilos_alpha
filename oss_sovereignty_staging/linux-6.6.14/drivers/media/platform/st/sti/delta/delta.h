 
 

#ifndef DELTA_H
#define DELTA_H

#include <linux/rpmsg.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>

#include "delta-cfg.h"

 
enum delta_state {
	DELTA_STATE_WF_FORMAT,
	DELTA_STATE_WF_STREAMINFO,
	DELTA_STATE_READY,
	DELTA_STATE_WF_EOS,
	DELTA_STATE_EOS
};

 
struct delta_streaminfo {
	u32 flags;
	u32 streamformat;
	u32 width;
	u32 height;
	u32 dpb;
	struct v4l2_rect crop;
	struct v4l2_fract pixelaspect;
	enum v4l2_field field;
	u8 profile[32];
	u8 level[32];
	u8 other[32];
	enum v4l2_colorspace colorspace;
	enum v4l2_xfer_func xfer_func;
	enum v4l2_ycbcr_encoding ycbcr_enc;
	enum v4l2_quantization quantization;
};

#define DELTA_STREAMINFO_FLAG_CROP		0x0001
#define DELTA_STREAMINFO_FLAG_PIXELASPECT	0x0002
#define DELTA_STREAMINFO_FLAG_OTHER		0x0004

 
struct delta_au {
	struct vb2_v4l2_buffer vbuf;	 
	struct list_head list;	 

	bool prepared;
	u32 size;
	void *vaddr;
	dma_addr_t paddr;
	u32 flags;
	u64 dts;
};

 
struct delta_frameinfo {
	u32 flags;
	u32 pixelformat;
	u32 width;
	u32 height;
	u32 aligned_width;
	u32 aligned_height;
	u32 size;
	struct v4l2_rect crop;
	struct v4l2_fract pixelaspect;
	enum v4l2_field field;
	enum v4l2_colorspace colorspace;
	enum v4l2_xfer_func xfer_func;
	enum v4l2_ycbcr_encoding ycbcr_enc;
	enum v4l2_quantization quantization;
};

#define DELTA_FRAMEINFO_FLAG_CROP		0x0001
#define DELTA_FRAMEINFO_FLAG_PIXELASPECT	0x0002

 
struct delta_frame {
	struct vb2_v4l2_buffer vbuf;	 
	struct list_head list;	 

	struct delta_frameinfo info;
	bool prepared;
	u32 index;
	void *vaddr;
	dma_addr_t paddr;
	u32 state;
	u32 flags;
	u64 dts;
	enum v4l2_field field;
};

 
#define DELTA_FRAME_FREE	0x00  
#define DELTA_FRAME_REF		0x01  
#define DELTA_FRAME_BSY		0x02  
#define DELTA_FRAME_DEC		0x04  
#define DELTA_FRAME_OUT		0x08  
#define DELTA_FRAME_RDY		0x10  
#define DELTA_FRAME_M2M		0x20  

 
struct delta_dts {
	struct list_head list;
	u64 val;
};

struct delta_buf {
	u32 size;
	void *vaddr;
	dma_addr_t paddr;
	const char *name;
	unsigned long attrs;
};

struct delta_ipc_ctx {
	int cb_err;
	u32 copro_hdl;
	struct completion done;
	struct delta_buf ipc_buf_struct;
	struct delta_buf *ipc_buf;
};

struct delta_ipc_param {
	u32 size;
	void *data;
};

struct delta_ctx;

 
struct delta_dec {
	const char *name;
	u32 streamformat;
	u32 pixelformat;
	u32 max_width;
	u32 max_height;
	bool pm;

	 
	int (*open)(struct delta_ctx *ctx);
	int (*close)(struct delta_ctx *ctx);

	 
	int (*setup_frame)(struct delta_ctx *ctx, struct delta_frame *frame);

	 
	int (*get_streaminfo)(struct delta_ctx *ctx,
			      struct delta_streaminfo *streaminfo);

	 
	int (*get_frameinfo)(struct delta_ctx *ctx,
			     struct delta_frameinfo *frameinfo);

	 
	int (*set_frameinfo)(struct delta_ctx *ctx,
			     struct delta_frameinfo *frameinfo);

	 
	int (*decode)(struct delta_ctx *ctx, struct delta_au *au);

	 
	int (*get_frame)(struct delta_ctx *ctx, struct delta_frame **frame);

	 
	int (*recycle)(struct delta_ctx *ctx, struct delta_frame *frame);

	 
	int (*flush)(struct delta_ctx *ctx);

	 
	int (*drain)(struct delta_ctx *ctx);
};

struct delta_dev;

 
struct delta_ctx {
	u32 flags;
	struct v4l2_fh fh;
	struct delta_dev *dev;
	const struct delta_dec *dec;
	struct delta_ipc_ctx ipc_ctx;

	enum delta_state state;
	u32 frame_num;
	u32 au_num;
	size_t max_au_size;
	struct delta_streaminfo streaminfo;
	struct delta_frameinfo frameinfo;
	u32 nb_of_frames;
	struct delta_frame *frames[DELTA_MAX_FRAMES];
	u32 decoded_frames;
	u32 output_frames;
	u32 dropped_frames;
	u32 stream_errors;
	u32 decode_errors;
	u32 sys_errors;
	struct list_head dts;
	char name[100];
	struct work_struct run_work;
	struct mutex lock;
	bool aborting;
	void *priv;
};

#define DELTA_FLAG_STREAMINFO 0x0001
#define DELTA_FLAG_FRAMEINFO 0x0002

#define DELTA_MAX_FORMATS  DELTA_MAX_DECODERS

 
struct delta_dev {
	struct v4l2_device v4l2_dev;
	struct video_device *vdev;
	struct platform_device *pdev;
	struct device *dev;
	struct v4l2_m2m_dev *m2m_dev;
	struct mutex lock;
	struct clk *clk_delta;
	struct clk *clk_st231;
	struct clk *clk_flash_promip;
	const struct delta_dec *decoders[DELTA_MAX_DECODERS];
	u32 nb_of_decoders;
	u32 pixelformats[DELTA_MAX_FORMATS];
	u32 nb_of_pixelformats;
	u32 streamformats[DELTA_MAX_FORMATS];
	u32 nb_of_streamformats;
	u8 instance_id;
	struct workqueue_struct *work_queue;
	struct rpmsg_driver rpmsg_driver;
	struct rpmsg_device *rpmsg_device;
};

static inline char *frame_type_str(u32 flags)
{
	if (flags & V4L2_BUF_FLAG_KEYFRAME)
		return "I";
	if (flags & V4L2_BUF_FLAG_PFRAME)
		return "P";
	if (flags & V4L2_BUF_FLAG_BFRAME)
		return "B";
	if (flags & V4L2_BUF_FLAG_LAST)
		return "EOS";
	return "?";
}

static inline char *frame_field_str(enum v4l2_field field)
{
	if (field == V4L2_FIELD_NONE)
		return "-";
	if (field == V4L2_FIELD_TOP)
		return "T";
	if (field == V4L2_FIELD_BOTTOM)
		return "B";
	if (field == V4L2_FIELD_INTERLACED)
		return "I";
	if (field == V4L2_FIELD_INTERLACED_TB)
		return "TB";
	if (field == V4L2_FIELD_INTERLACED_BT)
		return "BT";
	return "?";
}

static inline char *frame_state_str(u32 state, char *str, unsigned int len)
{
	snprintf(str, len, "%s %s %s %s %s %s",
		 (state & DELTA_FRAME_REF)  ? "ref" : "   ",
		 (state & DELTA_FRAME_BSY)  ? "bsy" : "   ",
		 (state & DELTA_FRAME_DEC)  ? "dec" : "   ",
		 (state & DELTA_FRAME_OUT)  ? "out" : "   ",
		 (state & DELTA_FRAME_M2M)  ? "m2m" : "   ",
		 (state & DELTA_FRAME_RDY)  ? "rdy" : "   ");
	return str;
}

int delta_get_frameinfo_default(struct delta_ctx *ctx,
				struct delta_frameinfo *frameinfo);
int delta_recycle_default(struct delta_ctx *pctx,
			  struct delta_frame *frame);

int delta_get_free_frame(struct delta_ctx *ctx,
			 struct delta_frame **pframe);

int delta_get_sync(struct delta_ctx *ctx);
void delta_put_autosuspend(struct delta_ctx *ctx);

#endif  
