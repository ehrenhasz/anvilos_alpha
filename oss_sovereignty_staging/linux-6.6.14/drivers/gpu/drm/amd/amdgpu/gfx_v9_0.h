 

#ifndef __GFX_V9_0_H__
#define __GFX_V9_0_H__

extern const struct amdgpu_ip_block_version gfx_v9_0_ip_block;

void gfx_v9_0_select_se_sh(struct amdgpu_device *adev, u32 se_num, u32 sh_num,
			   u32 instance, int xcc_id);

#endif
