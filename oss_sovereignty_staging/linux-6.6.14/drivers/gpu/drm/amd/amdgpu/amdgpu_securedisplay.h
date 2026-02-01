 
#ifndef _AMDGPU_SECUREDISPLAY_H
#define _AMDGPU_SECUREDISPLAY_H

#include "amdgpu.h"
#include "ta_secureDisplay_if.h"

void amdgpu_securedisplay_debugfs_init(struct amdgpu_device *adev);
void psp_securedisplay_parse_resp_status(struct psp_context *psp,
		enum ta_securedisplay_status status);
void psp_prep_securedisplay_cmd_buf(struct psp_context *psp, struct ta_securedisplay_cmd **cmd,
		enum ta_securedisplay_command command_id);

#endif
