 

#ifndef __JPEG_V2_5_H__
#define __JPEG_V2_5_H__

enum amdgpu_jpeg_v2_6_sub_block {
	AMDGPU_JPEG_V2_6_JPEG0 = 0,
	AMDGPU_JPEG_V2_6_JPEG1,

	AMDGPU_JPEG_V2_6_MAX_SUB_BLOCK,
};

extern const struct amdgpu_ip_block_version jpeg_v2_5_ip_block;
extern const struct amdgpu_ip_block_version jpeg_v2_6_ip_block;

#endif  
