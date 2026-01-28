#ifndef VPIF_DISPLAY_H
#define VPIF_DISPLAY_H
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-device.h>
#include "vpif.h"
#define VPIF_DISPLAY_VERSION	"0.0.2"
#define VPIF_VALID_FIELD(field) \
	(((V4L2_FIELD_ANY == field) || (V4L2_FIELD_NONE == field)) || \
	(((V4L2_FIELD_INTERLACED == field) || (V4L2_FIELD_SEQ_TB == field)) || \
	(V4L2_FIELD_SEQ_BT == field)))
#define VPIF_DISPLAY_MAX_DEVICES	(2)
#define VPIF_SLICED_BUF_SIZE		(256)
#define VPIF_SLICED_MAX_SERVICES	(3)
#define VPIF_VIDEO_INDEX		(0)
#define VPIF_VBI_INDEX			(1)
#define VPIF_HBI_INDEX			(2)
#define VPIF_NUMOBJECTS	(1)
#define ISALIGNED(a)    (0 == ((a) & 7))
enum vpif_channel_id {
	VPIF_CHANNEL2_VIDEO = 0,	 
	VPIF_CHANNEL3_VIDEO,		 
};
struct video_obj {
	enum v4l2_field buf_field;
	u32 latest_only;		 
	v4l2_std_id stdid;		 
	struct v4l2_dv_timings dv_timings;
};
struct vpif_disp_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};
struct common_obj {
	struct vpif_disp_buffer *cur_frm;	 
	struct vpif_disp_buffer *next_frm;	 
	struct v4l2_format fmt;			 
	struct vb2_queue buffer_queue;		 
	struct list_head dma_queue;		 
	spinlock_t irqlock;			 
	struct mutex lock;			 
	u32 ytop_off;				 
	u32 ybtm_off;				 
	u32 ctop_off;				 
	u32 cbtm_off;				 
	void (*set_addr)(unsigned long, unsigned long,
				unsigned long, unsigned long);
	u32 height;
	u32 width;
};
struct channel_obj {
	struct video_device video_dev;	 
	u32 field_id;			 
	u8 initialized;			 
	u32 output_idx;			 
	struct v4l2_subdev *sd;		 
	enum vpif_channel_id channel_id; 
	struct vpif_params vpifparams;
	struct common_obj common[VPIF_NUMOBJECTS];
	struct video_obj video;
};
struct vpif_device {
	struct v4l2_device v4l2_dev;
	struct channel_obj *dev[VPIF_DISPLAY_NUM_CHANNELS];
	struct v4l2_subdev **sd;
	struct vpif_display_config *config;
};
#endif				 
