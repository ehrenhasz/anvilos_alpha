#ifndef __IPU3_H
#define __IPU3_H
#include <linux/iova.h>
#include <linux/pci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-dma-sg.h>
#include "ipu3-css.h"
#define IMGU_NAME			"ipu3-imgu"
#define IMGU_QUEUE_MASTER		IPU3_CSS_QUEUE_IN
#define IMGU_QUEUE_FIRST_INPUT		IPU3_CSS_QUEUE_OUT
#define IMGU_MAX_QUEUE_DEPTH		(2 + 2)
#define IMGU_NODE_IN			0  
#define IMGU_NODE_PARAMS		1  
#define IMGU_NODE_OUT			2  
#define IMGU_NODE_VF			3  
#define IMGU_NODE_STAT_3A		4  
#define IMGU_NODE_NUM			5
#define file_to_intel_imgu_node(__file) \
	container_of(video_devdata(__file), struct imgu_video_device, vdev)
#define IPU3_INPUT_MIN_WIDTH		0U
#define IPU3_INPUT_MIN_HEIGHT		0U
#define IPU3_INPUT_MAX_WIDTH		5120U
#define IPU3_INPUT_MAX_HEIGHT		38404U
#define IPU3_OUTPUT_MIN_WIDTH		2U
#define IPU3_OUTPUT_MIN_HEIGHT		2U
#define IPU3_OUTPUT_MAX_WIDTH		4480U
#define IPU3_OUTPUT_MAX_HEIGHT		34004U
struct imgu_vb2_buffer {
	struct vb2_v4l2_buffer vbb;	 
	struct list_head list;
};
struct imgu_buffer {
	struct imgu_vb2_buffer vid_buf;	 
	struct imgu_css_buffer css_buf;
	struct imgu_css_map map;
};
struct imgu_node_mapping {
	unsigned int css_queue;
	const char *name;
};
struct imgu_video_device {
	const char *name;
	bool output;
	bool enabled;
	struct v4l2_format vdev_fmt;	 
	struct video_device vdev;
	struct media_pad vdev_pad;
	struct v4l2_mbus_framefmt pad_fmt;
	struct vb2_queue vbq;
	struct list_head buffers;
	struct mutex lock;
	atomic_t sequence;
	unsigned int id;
	unsigned int pipe;
};
struct imgu_v4l2_subdev {
	unsigned int pipe;
	struct v4l2_subdev subdev;
	struct media_pad subdev_pads[IMGU_NODE_NUM];
	struct {
		struct v4l2_rect eff;  
		struct v4l2_rect bds;  
		struct v4l2_rect gdc;  
	} rect;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *ctrl;
	atomic_t running_mode;
	bool active;
};
struct imgu_media_pipe {
	unsigned int pipe;
	struct {
		struct imgu_css_map dmap;
		struct imgu_css_buffer dummybufs[IMGU_MAX_QUEUE_DEPTH];
	} queues[IPU3_CSS_QUEUES];
	struct imgu_video_device nodes[IMGU_NODE_NUM];
	bool queue_enabled[IMGU_NODE_NUM];
	struct media_pipeline pipeline;
	struct imgu_v4l2_subdev imgu_sd;
};
struct imgu_device {
	struct pci_dev *pci_dev;
	void __iomem *base;
	unsigned int buf_struct_size;
	bool streaming;		 
	struct imgu_media_pipe imgu_pipe[IMGU_MAX_PIPE_NUM];
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct v4l2_file_operations v4l2_file_ops;
	struct imgu_mmu_info *mmu;
	struct iova_domain iova_domain;
	struct imgu_css css;
	struct mutex lock;
	struct mutex streaming_lock;
	atomic_t qbuf_barrier;
	bool suspend_in_stream;
	wait_queue_head_t buf_drain_wq;
};
unsigned int imgu_node_to_queue(unsigned int node);
unsigned int imgu_map_node(struct imgu_device *imgu, unsigned int css_queue);
int imgu_queue_buffers(struct imgu_device *imgu, bool initial,
		       unsigned int pipe);
int imgu_v4l2_register(struct imgu_device *dev);
int imgu_v4l2_unregister(struct imgu_device *dev);
void imgu_v4l2_buffer_done(struct vb2_buffer *vb, enum vb2_buffer_state state);
int imgu_s_stream(struct imgu_device *imgu, int enable);
static inline u32 imgu_bytesperline(const unsigned int width,
				    enum imgu_abi_frame_format frame_format)
{
	if (frame_format == IMGU_ABI_FRAME_FORMAT_NV12)
		return ALIGN(width, IPU3_UAPI_ISP_VEC_ELEMS);
	return DIV_ROUND_UP(width, 50) * 64;
}
#endif
