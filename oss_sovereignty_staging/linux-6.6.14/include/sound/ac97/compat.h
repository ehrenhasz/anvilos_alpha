 

#ifndef AC97_COMPAT_H
#define AC97_COMPAT_H

#include <sound/ac97_codec.h>

struct snd_ac97 *snd_ac97_compat_alloc(struct ac97_codec_device *adev);
void snd_ac97_compat_release(struct snd_ac97 *ac97);

#endif
