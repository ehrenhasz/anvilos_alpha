 
 

#ifndef OMAP3_ISP_VIDEO_H
#define OMAP3_ISP_VIDEO_H

#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-v4l2.h>

#define ISP_VIDEO_DRIVER_NAME		"ispvideo"
#define ISP_VIDEO_DRIVER_VERSION	"0.0.2"

struct isp_device;
struct isp_video;
struct v4l2_mbus_framefmt;
struct v4l2_pix_format;

 
struct isp_format_info {
	u32 code;
	u32 truncated;
	u32 uncompressed;
	u32 flavor;
	u32 pixelformat;
	unsigned int width;
	unsigned int bpp;
};

enum isp_pipeline_stream_state {
	ISP_PIPELINE_STREAM_STOPPED = 0,
	ISP_PIPELINE_STREAM_CONTINUOUS = 1,
	ISP_PIPELINE_STREAM_SINGLESHOT = 2,
};

enum isp_pipeline_state {
	 
	ISP_PIPELINE_STREAM_INPUT = 1,
	 
	ISP_PIPELINE_STREAM_OUTPUT = 2,
	 
	ISP_PIPELINE_QUEUE_INPUT = 4,
	 
	ISP_PIPELINE_QUEUE_OUTPUT = 8,
	 
	ISP_PIPELINE_IDLE_INPUT = 16,
	 
	ISP_PIPELINE_IDLE_OUTPUT = 32,
	 
	ISP_PIPELINE_STREAM = 64,
};

 
struct isp_pipeline {
	struct media_pipeline pipe;
	spinlock_t lock;		 
	unsigned int state;
	enum isp_pipeline_stream_state stream_state;
	struct isp_video *input;
	struct isp_video *output;
	struct media_entity_enum ent_enum;
	unsigned long l3_ick;
	unsigned int max_rate;
	enum v4l2_field field;
	atomic_t frame_number;
	bool do_propagation;  
	bool error;
	struct v4l2_fract max_timeperframe;
	struct v4l2_subdev *external;
	unsigned int external_rate;
	unsigned int external_width;
};

static inline struct isp_pipeline *to_isp_pipeline(struct media_entity *entity)
{
	struct media_pipeline *pipe = media_entity_pipeline(entity);

	if (!pipe)
		return NULL;

	return container_of(pipe, struct isp_pipeline, pipe);
}

static inline int isp_pipeline_ready(struct isp_pipeline *pipe)
{
	return pipe->state == (ISP_PIPELINE_STREAM_INPUT |
			       ISP_PIPELINE_STREAM_OUTPUT |
			       ISP_PIPELINE_QUEUE_INPUT |
			       ISP_PIPELINE_QUEUE_OUTPUT |
			       ISP_PIPELINE_IDLE_INPUT |
			       ISP_PIPELINE_IDLE_OUTPUT);
}

 
struct isp_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head irqlist;
	dma_addr_t dma;
};

#define to_isp_buffer(buf)	container_of(buf, struct isp_buffer, vb)

enum isp_video_dmaqueue_flags {
	 
	ISP_VIDEO_DMAQUEUE_UNDERRUN = (1 << 0),
	 
	ISP_VIDEO_DMAQUEUE_QUEUED = (1 << 1),
};

#define isp_video_dmaqueue_flags_clr(video)	\
			({ (video)->dmaqueue_flags = 0; })

 
struct isp_video_operations {
	int(*queue)(struct isp_video *video, struct isp_buffer *buffer);
};

struct isp_video {
	struct video_device video;
	enum v4l2_buf_type type;
	struct media_pad pad;

	struct mutex mutex;		 
	atomic_t active;

	struct isp_device *isp;

	unsigned int capture_mem;
	unsigned int bpl_alignment;	 
	unsigned int bpl_zero_padding;	 
	unsigned int bpl_max;		 
	unsigned int bpl_value;		 
	unsigned int bpl_padding;	 

	 
	struct isp_pipeline pipe;
	struct mutex stream_lock;	 
	bool error;

	 
	struct vb2_queue *queue;
	struct mutex queue_lock;	 
	spinlock_t irqlock;		 
	struct list_head dmaqueue;
	enum isp_video_dmaqueue_flags dmaqueue_flags;

	const struct isp_video_operations *ops;
};

#define to_isp_video(vdev)	container_of(vdev, struct isp_video, video)

struct isp_video_fh {
	struct v4l2_fh vfh;
	struct isp_video *video;
	struct vb2_queue queue;
	struct v4l2_format format;
	struct v4l2_fract timeperframe;
};

#define to_isp_video_fh(fh)	container_of(fh, struct isp_video_fh, vfh)
#define isp_video_queue_to_isp_video_fh(q) \
				container_of(q, struct isp_video_fh, queue)

int omap3isp_video_init(struct isp_video *video, const char *name);
void omap3isp_video_cleanup(struct isp_video *video);
int omap3isp_video_register(struct isp_video *video,
			    struct v4l2_device *vdev);
void omap3isp_video_unregister(struct isp_video *video);
struct isp_buffer *omap3isp_video_buffer_next(struct isp_video *video);
void omap3isp_video_cancel_stream(struct isp_video *video);
void omap3isp_video_resume(struct isp_video *video, int continuous);
struct media_pad *omap3isp_video_remote_pad(struct isp_video *video);

const struct isp_format_info *
omap3isp_video_format_info(u32 code);

#endif  
