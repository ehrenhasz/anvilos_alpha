#ifndef __AMDGPU_DM_IRQ_PARAMS_H__
#define __AMDGPU_DM_IRQ_PARAMS_H__
#include "amdgpu_dm_crc.h"
struct dm_irq_params {
	u32 last_flip_vblank;
	struct mod_vrr_params vrr_params;
	struct dc_stream_state *stream;
	int active_planes;
	bool allow_psr_entry;
	struct mod_freesync_config freesync_config;
#ifdef CONFIG_DEBUG_FS
	enum amdgpu_dm_pipe_crc_source crc_src;
#ifdef CONFIG_DRM_AMD_SECURE_DISPLAY
	struct crc_window_param window_param;
#endif
#endif
};
#endif  
