 

#ifndef __DAL_AUDIO_H__
#define __DAL_AUDIO_H__

#include "audio_types.h"

struct audio;

struct audio_funcs {

	bool (*endpoint_valid)(struct audio *audio);

	void (*hw_init)(struct audio *audio);

	void (*az_enable)(struct audio *audio);

	void (*az_disable)(struct audio *audio);

	void (*az_configure)(struct audio *audio,
		enum signal_type signal,
		const struct audio_crtc_info *crtc_info,
		const struct audio_info *audio_info);

	void (*wall_dto_setup)(struct audio *audio,
		enum signal_type signal,
		const struct audio_crtc_info *crtc_info,
		const struct audio_pll_info *pll_info);

	void (*destroy)(struct audio **audio);
};

struct audio {
	const struct audio_funcs *funcs;
	struct dc_context *ctx;
	unsigned int inst;
	bool enabled;
};

#endif   
