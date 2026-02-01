 
 

#ifndef OMAP4_ISS_IPIPEIF_H
#define OMAP4_ISS_IPIPEIF_H

#include "iss_video.h"

enum ipipeif_input_entity {
	IPIPEIF_INPUT_NONE,
	IPIPEIF_INPUT_CSI2A,
	IPIPEIF_INPUT_CSI2B
};

#define IPIPEIF_OUTPUT_MEMORY			BIT(0)
#define IPIPEIF_OUTPUT_VP			BIT(1)

 
#define IPIPEIF_PAD_SINK			0
#define IPIPEIF_PAD_SOURCE_ISIF_SF		1
#define IPIPEIF_PAD_SOURCE_VP			2
#define IPIPEIF_PADS_NUM			3

 
struct iss_ipipeif_device {
	struct v4l2_subdev subdev;
	struct media_pad pads[IPIPEIF_PADS_NUM];
	struct v4l2_mbus_framefmt formats[IPIPEIF_PADS_NUM];

	enum ipipeif_input_entity input;
	unsigned int output;
	struct iss_video video_out;
	unsigned int error;

	enum iss_pipeline_stream_state state;
	wait_queue_head_t wait;
	atomic_t stopping;
};

struct iss_device;

int omap4iss_ipipeif_init(struct iss_device *iss);
int omap4iss_ipipeif_create_links(struct iss_device *iss);
void omap4iss_ipipeif_cleanup(struct iss_device *iss);
int omap4iss_ipipeif_register_entities(struct iss_ipipeif_device *ipipeif,
				       struct v4l2_device *vdev);
void omap4iss_ipipeif_unregister_entities(struct iss_ipipeif_device *ipipeif);

int omap4iss_ipipeif_busy(struct iss_ipipeif_device *ipipeif);
void omap4iss_ipipeif_isr(struct iss_ipipeif_device *ipipeif, u32 events);
void omap4iss_ipipeif_restore_context(struct iss_device *iss);
void omap4iss_ipipeif_max_rate(struct iss_ipipeif_device *ipipeif,
			       unsigned int *max_rate);

#endif	 
