#ifndef __SOUND_SOC_INTEL_AVS_CTRL_H
#define __SOUND_SOC_INTEL_AVS_CTRL_H
#include <sound/control.h>
struct avs_control_data {
	u32 id;
	long volume;
};
int avs_control_volume_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int avs_control_volume_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
#endif
