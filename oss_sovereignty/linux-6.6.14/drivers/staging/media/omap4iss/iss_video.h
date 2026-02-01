 
 

#ifndef OMAP4_ISS_VIDEO_H
#define OMAP4_ISS_VIDEO_H

#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

#define ISS_VIDEO_DRIVER_NAME		"issvideo"

struct iss_device;
struct iss_video;
struct v4l2_mbus_framefmt;
struct v4l2_pix_format;

 
struct iss_format_info {
	u32 code;
	u32 truncated;
	u32 uncompressed;
	u32 flavor;
	u32 pixelformat;
	unsigned int bpp;
};

enum iss_pipeline_stream_state {
	ISS_PIPELINE_STREAM_STOPPED = 0,
	ISS_PIPELINE_STREAM_CONTINUOUS = 1,
	ISS_PIPELINE_STREAM_SINGLESHOT = 2,
};

enum iss_pipeline_state {
	 
	ISS_PIPELINE_STREAM_INPUT = BIT(0),
	 
	ISS_PIPELINE_STREAM_OUTPUT = BIT(1),
	 
	ISS_PIPELINE_QUEUE_INPUT = BIT(2),
	 
	ISS_PIPELINE_QUEUE_OUTPUT = BIT(3),
	 
	ISS_PIPELINE_IDLE_INPUT = BIT(4),
	 
	ISS_PIPELINE_IDLE_OUTPUT = BIT(5),
	 
	ISS_PIPELINE_STREAM = BIT(6),
};

 
struct iss_pipeline {
	struct media_pipeline pipe;
	spinlock_t lock;		 
	unsigned int state;
	enum iss_pipeline_stream_state stream_state;
	struct iss_video *input;
	struct iss_video *output;
	struct media_entity_enum ent_enum;
	atomic_t frame_number;
	bool do_propagation;  
	bool error;
	struct v4l2_fract max_timeperframe;
	struct v4l2_subdev *external;
	unsigned int external_rate;
	int external_bpp;
};

static inline struct iss_pipeline *to_iss_pipeline(struct media_entity *entity)
{
	struct media_pipeline *pipe = media_entity_pipeline(entity);

	if (!pipe)
		return NULL;

	return container_of(pipe, struct iss_pipeline, pipe);
}

static inline int iss_pipeline_ready(struct iss_pipeline *pipe)
{
	return pipe->state == (ISS_PIPELINE_STREAM_INPUT |
			       ISS_PIPELINE_STREAM_OUTPUT |
			       ISS_PIPELINE_QUEUE_INPUT |
			       ISS_PIPELINE_QUEUE_OUTPUT |
			       ISS_PIPELINE_IDLE_INPUT |
			       ISS_PIPELINE_IDLE_OUTPUT);
}

 
struct iss_buffer {
	 
	struct vb2_v4l2_buffer	vb;
	struct list_head	list;
	dma_addr_t iss_addr;
};

#define to_iss_buffer(buf)	container_of(buf, struct iss_buffer, vb)

enum iss_video_dmaqueue_flags {
	 
	ISS_VIDEO_DMAQUEUE_UNDERRUN = BIT(0),
	 
	ISS_VIDEO_DMAQUEUE_QUEUED = BIT(1),
};

#define iss_video_dmaqueue_flags_clr(video)	\
			({ (video)->dmaqueue_flags = 0; })

 
struct iss_video_operations {
	int (*queue)(struct iss_video *video, struct iss_buffer *buffer);
};

struct iss_video {
	struct video_device video;
	enum v4l2_buf_type type;
	struct media_pad pad;

	struct mutex mutex;		 
	atomic_t active;

	struct iss_device *iss;

	unsigned int capture_mem;
	unsigned int bpl_alignment;	 
	unsigned int bpl_zero_padding;	 
	unsigned int bpl_max;		 
	unsigned int bpl_value;		 
	unsigned int bpl_padding;	 

	 
	struct iss_pipeline pipe;
	struct mutex stream_lock;	 
	bool error;

	 
	struct vb2_queue *queue;
	spinlock_t qlock;		 
	struct list_head dmaqueue;
	enum iss_video_dmaqueue_flags dmaqueue_flags;

	const struct iss_video_operations *ops;
};

#define to_iss_video(vdev)	container_of(vdev, struct iss_video, video)

struct iss_video_fh {
	struct v4l2_fh vfh;
	struct iss_video *video;
	struct vb2_queue queue;
	struct v4l2_format format;
	struct v4l2_fract timeperframe;
};

#define to_iss_video_fh(fh)	container_of(fh, struct iss_video_fh, vfh)
#define iss_video_queue_to_iss_video_fh(q) \
				container_of(q, struct iss_video_fh, queue)

int omap4iss_video_init(struct iss_video *video, const char *name);
void omap4iss_video_cleanup(struct iss_video *video);
int omap4iss_video_register(struct iss_video *video,
			    struct v4l2_device *vdev);
void omap4iss_video_unregister(struct iss_video *video);
struct iss_buffer *omap4iss_video_buffer_next(struct iss_video *video);
void omap4iss_video_cancel_stream(struct iss_video *video);
struct media_pad *omap4iss_video_remote_pad(struct iss_video *video);

const struct iss_format_info *
omap4iss_video_format_info(u32 code);

#endif  
