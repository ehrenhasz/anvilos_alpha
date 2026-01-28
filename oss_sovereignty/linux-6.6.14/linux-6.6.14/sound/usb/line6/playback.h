#ifndef PLAYBACK_H
#define PLAYBACK_H
#include <sound/pcm.h>
#include "driver.h"
#define USE_CLEAR_BUFFER_WORKAROUND 1
extern const struct snd_pcm_ops snd_line6_playback_ops;
extern int line6_create_audio_out_urbs(struct snd_line6_pcm *line6pcm);
extern int line6_submit_audio_out_all_urbs(struct snd_line6_pcm *line6pcm);
#endif
