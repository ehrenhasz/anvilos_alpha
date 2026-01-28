#ifndef __HDA_DSP_COMMON_H
#define __HDA_DSP_COMMON_H
#include <sound/hda_codec.h>
#include <sound/hda_i915.h>
#include "../../codecs/hdac_hda.h"
#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA_AUDIO_CODEC)
int hda_dsp_hdmi_build_controls(struct snd_soc_card *card,
				struct snd_soc_component *comp);
#else
static inline int hda_dsp_hdmi_build_controls(struct snd_soc_card *card,
					      struct snd_soc_component *comp)
{
	return -EINVAL;
}
#endif
#endif  
