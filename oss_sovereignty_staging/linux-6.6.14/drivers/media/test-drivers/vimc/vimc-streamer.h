 
 

#ifndef _VIMC_STREAMER_H_
#define _VIMC_STREAMER_H_

#include <media/media-device.h>

#include "vimc-common.h"

#define VIMC_STREAMER_PIPELINE_MAX_SIZE 16

 
struct vimc_stream {
	struct media_pipeline pipe;
	struct vimc_ent_device *ved_pipeline[VIMC_STREAMER_PIPELINE_MAX_SIZE];
	unsigned int pipe_size;
	struct task_struct *kthread;
};

int vimc_streamer_s_stream(struct vimc_stream *stream,
			   struct vimc_ent_device *ved,
			   int enable);

#endif  
