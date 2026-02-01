 
 

#ifndef _DP_AUDIO_H_
#define _DP_AUDIO_H_

#include <linux/platform_device.h>

#include "dp_panel.h"
#include "dp_catalog.h"
#include <sound/hdmi-codec.h>

 
struct dp_audio {
	u32 lane_count;
	u32 bw_code;
};

 
struct dp_audio *dp_audio_get(struct platform_device *pdev,
			struct dp_panel *panel,
			struct dp_catalog *catalog);

 
int dp_register_audio_driver(struct device *dev,
		struct dp_audio *dp_audio);

void dp_unregister_audio_driver(struct device *dev, struct dp_audio *dp_audio);

 
void dp_audio_put(struct dp_audio *dp_audio);

int dp_audio_hw_params(struct device *dev,
	void *data,
	struct hdmi_codec_daifmt *daifmt,
	struct hdmi_codec_params *params);

#endif  


