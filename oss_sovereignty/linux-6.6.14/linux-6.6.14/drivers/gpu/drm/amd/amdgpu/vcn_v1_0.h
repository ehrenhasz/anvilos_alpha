#ifndef __VCN_V1_0_H__
#define __VCN_V1_0_H__
void vcn_v1_0_ring_end_use(struct amdgpu_ring *ring);
void vcn_v1_0_set_pg_for_begin_use(struct amdgpu_ring *ring, bool set_clocks);
extern const struct amdgpu_ip_block_version vcn_v1_0_ip_block;
#endif
