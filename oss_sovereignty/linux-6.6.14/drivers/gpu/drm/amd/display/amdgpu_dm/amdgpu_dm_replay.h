 

#ifndef AMDGPU_DM_AMDGPU_DM_REPLAY_H_
#define AMDGPU_DM_AMDGPU_DM_REPLAY_H_

#include "amdgpu.h"

enum replay_enable_option {
	pr_enable_option_static_screen = 0x1,
	pr_enable_option_mpo_video = 0x2,
	pr_enable_option_full_screen_video = 0x4,
	pr_enable_option_general_ui = 0x8,
	pr_enable_option_static_screen_coasting = 0x10000,
	pr_enable_option_mpo_video_coasting = 0x20000,
	pr_enable_option_full_screen_video_coasting = 0x40000,
};


bool amdgpu_dm_replay_enable(struct dc_stream_state *stream, bool enable);
bool amdgpu_dm_setup_replay(struct dc_link *link, struct amdgpu_dm_connector *aconnector);
bool amdgpu_dm_replay_disable(struct dc_stream_state *stream);

#endif  
