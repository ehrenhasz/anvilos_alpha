 
 

#ifndef __XILINX_VIP_DMA_H__
#define __XILINX_VIP_DMA_H__

#include <linux/dmaengine.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-v4l2.h>

struct dma_chan;
struct xvip_composite_device;
struct xvip_video_format;

 
struct xvip_pipeline {
	struct media_pipeline pipe;

	struct mutex lock;
	unsigned int use_count;
	unsigned int stream_count;

	unsigned int num_dmas;
	struct xvip_dma *output;
};

static inline struct xvip_pipeline *to_xvip_pipeline(struct video_device *vdev)
{
	struct media_pipeline *pipe = video_device_pipeline(vdev);

	if (!pipe)
		return NULL;

	return container_of(pipe, struct xvip_pipeline, pipe);
}

 
struct xvip_dma {
	struct list_head list;
	struct video_device video;
	struct media_pad pad;

	struct xvip_composite_device *xdev;
	struct xvip_pipeline pipe;
	unsigned int port;

	struct mutex lock;
	struct v4l2_pix_format format;
	const struct xvip_video_format *fmtinfo;

	struct vb2_queue queue;
	unsigned int sequence;

	struct list_head queued_bufs;
	spinlock_t queued_lock;

	struct dma_chan *dma;
	unsigned int align;
	struct dma_interleaved_template xt;
	struct data_chunk sgl[1];
};

#define to_xvip_dma(vdev)	container_of(vdev, struct xvip_dma, video)

int xvip_dma_init(struct xvip_composite_device *xdev, struct xvip_dma *dma,
		  enum v4l2_buf_type type, unsigned int port);
void xvip_dma_cleanup(struct xvip_dma *dma);

#endif  
