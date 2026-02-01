 
 
#ifndef __VSP1_PIPE_H__
#define __VSP1_PIPE_H__

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <media/media-entity.h>

struct vsp1_dl_list;
struct vsp1_rwpf;

 
struct vsp1_format_info {
	u32 fourcc;
	unsigned int mbus;
	unsigned int hwfmt;
	unsigned int swap;
	unsigned int planes;
	unsigned int bpp[3];
	bool swap_yc;
	bool swap_uv;
	unsigned int hsub;
	unsigned int vsub;
	bool alpha;
};

enum vsp1_pipeline_state {
	VSP1_PIPELINE_STOPPED,
	VSP1_PIPELINE_RUNNING,
	VSP1_PIPELINE_STOPPING,
};

 
struct vsp1_partition_window {
	unsigned int left;
	unsigned int width;
};

 
struct vsp1_partition {
	struct vsp1_partition_window rpf;
	struct vsp1_partition_window uds_sink;
	struct vsp1_partition_window uds_source;
	struct vsp1_partition_window sru;
	struct vsp1_partition_window wpf;
};

 
struct vsp1_pipeline {
	struct media_pipeline pipe;

	spinlock_t irqlock;
	enum vsp1_pipeline_state state;
	wait_queue_head_t wq;

	void (*frame_end)(struct vsp1_pipeline *pipe, unsigned int completion);

	struct mutex lock;
	struct kref kref;
	unsigned int stream_count;
	unsigned int buffers_ready;
	unsigned int sequence;

	unsigned int num_inputs;
	struct vsp1_rwpf *inputs[VSP1_MAX_RPF];
	struct vsp1_rwpf *output;
	struct vsp1_entity *brx;
	struct vsp1_entity *hgo;
	struct vsp1_entity *hgt;
	struct vsp1_entity *lif;
	struct vsp1_entity *uds;
	struct vsp1_entity *uds_input;

	 
	struct list_head entities;

	struct vsp1_dl_body *stream_config;
	bool configured;
	bool interlaced;

	unsigned int partitions;
	struct vsp1_partition *partition;
	struct vsp1_partition *part_table;

	u32 underrun_count;
};

void vsp1_pipeline_reset(struct vsp1_pipeline *pipe);
void vsp1_pipeline_init(struct vsp1_pipeline *pipe);

void vsp1_pipeline_run(struct vsp1_pipeline *pipe);
bool vsp1_pipeline_stopped(struct vsp1_pipeline *pipe);
int vsp1_pipeline_stop(struct vsp1_pipeline *pipe);
bool vsp1_pipeline_ready(struct vsp1_pipeline *pipe);

void vsp1_pipeline_frame_end(struct vsp1_pipeline *pipe);

void vsp1_pipeline_propagate_alpha(struct vsp1_pipeline *pipe,
				   struct vsp1_dl_body *dlb,
				   unsigned int alpha);

void vsp1_pipeline_propagate_partition(struct vsp1_pipeline *pipe,
				       struct vsp1_partition *partition,
				       unsigned int index,
				       struct vsp1_partition_window *window);

const struct vsp1_format_info *vsp1_get_format_info(struct vsp1_device *vsp1,
						    u32 fourcc);

#endif  
