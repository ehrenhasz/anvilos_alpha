#ifndef __AMDGPU_GDS_H__
#define __AMDGPU_GDS_H__
struct amdgpu_ring;
struct amdgpu_bo;
struct amdgpu_gds {
	uint32_t gds_size;
	uint32_t gws_size;
	uint32_t oa_size;
	uint32_t gds_compute_max_wave_id;
};
struct amdgpu_gds_reg_offset {
	uint32_t	mem_base;
	uint32_t	mem_size;
	uint32_t	gws;
	uint32_t	oa;
};
#endif  
