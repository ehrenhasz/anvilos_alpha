#ifndef OMAP4_ISS_RESIZER_H
#define OMAP4_ISS_RESIZER_H
#include "iss_video.h"
enum resizer_input_entity {
	RESIZER_INPUT_NONE,
	RESIZER_INPUT_IPIPE,
	RESIZER_INPUT_IPIPEIF
};
#define RESIZER_OUTPUT_MEMORY			BIT(0)
#define RESIZER_PAD_SINK			0
#define RESIZER_PAD_SOURCE_MEM			1
#define RESIZER_PADS_NUM			2
struct iss_resizer_device {
	struct v4l2_subdev subdev;
	struct media_pad pads[RESIZER_PADS_NUM];
	struct v4l2_mbus_framefmt formats[RESIZER_PADS_NUM];
	enum resizer_input_entity input;
	unsigned int output;
	struct iss_video video_out;
	unsigned int error;
	enum iss_pipeline_stream_state state;
	wait_queue_head_t wait;
	atomic_t stopping;
};
struct iss_device;
int omap4iss_resizer_init(struct iss_device *iss);
int omap4iss_resizer_create_links(struct iss_device *iss);
void omap4iss_resizer_cleanup(struct iss_device *iss);
int omap4iss_resizer_register_entities(struct iss_resizer_device *resizer,
				       struct v4l2_device *vdev);
void omap4iss_resizer_unregister_entities(struct iss_resizer_device *resizer);
int omap4iss_resizer_busy(struct iss_resizer_device *resizer);
void omap4iss_resizer_isr(struct iss_resizer_device *resizer, u32 events);
void omap4iss_resizer_restore_context(struct iss_device *iss);
void omap4iss_resizer_max_rate(struct iss_resizer_device *resizer,
			       unsigned int *max_rate);
#endif	 
