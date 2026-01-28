#ifndef AMDGPU_DM_AMDGPU_DM_PSR_H_
#define AMDGPU_DM_AMDGPU_DM_PSR_H_
#include "amdgpu.h"
#define AMDGPU_DM_PSR_ENTRY_DELAY 5
void amdgpu_dm_set_psr_caps(struct dc_link *link);
bool amdgpu_dm_psr_enable(struct dc_stream_state *stream);
bool amdgpu_dm_link_setup_psr(struct dc_stream_state *stream);
bool amdgpu_dm_psr_disable(struct dc_stream_state *stream);
bool amdgpu_dm_psr_disable_all(struct amdgpu_display_manager *dm);
#endif  
