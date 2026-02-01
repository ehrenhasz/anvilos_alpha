 

#ifndef __AMDGPU_PSP_TA_H__
#define __AMDGPU_PSP_TA_H__

 
#define psp_fn_ta_initialize(psp) ((psp)->ta_funcs->fn_ta_initialize((psp)))
#define psp_fn_ta_invoke(psp, ta_cmd_id) ((psp)->ta_funcs->fn_ta_invoke((psp), (ta_cmd_id)))
#define psp_fn_ta_terminate(psp) ((psp)->ta_funcs->fn_ta_terminate((psp)))

void amdgpu_ta_if_debugfs_init(struct amdgpu_device *adev);

#endif
