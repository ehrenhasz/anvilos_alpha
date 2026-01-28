#ifndef OMAP4_ISS_IPIPE_H
#define OMAP4_ISS_IPIPE_H
#include "iss_video.h"
enum ipipe_input_entity {
	IPIPE_INPUT_NONE,
	IPIPE_INPUT_IPIPEIF,
};
#define IPIPE_OUTPUT_VP				BIT(0)
#define IPIPE_PAD_SINK				0
#define IPIPE_PAD_SOURCE_VP			1
#define IPIPE_PADS_NUM				2
struct iss_ipipe_device {
	struct v4l2_subdev subdev;
	struct media_pad pads[IPIPE_PADS_NUM];
	struct v4l2_mbus_framefmt formats[IPIPE_PADS_NUM];
	enum ipipe_input_entity input;
	unsigned int output;
	unsigned int error;
	enum iss_pipeline_stream_state state;
	wait_queue_head_t wait;
	atomic_t stopping;
};
struct iss_device;
int omap4iss_ipipe_register_entities(struct iss_ipipe_device *ipipe,
				     struct v4l2_device *vdev);
void omap4iss_ipipe_unregister_entities(struct iss_ipipe_device *ipipe);
int omap4iss_ipipe_init(struct iss_device *iss);
void omap4iss_ipipe_cleanup(struct iss_device *iss);
#endif	 
