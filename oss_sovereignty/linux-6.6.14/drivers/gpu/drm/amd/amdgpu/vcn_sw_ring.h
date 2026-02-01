 

#ifndef __VCN_SW_RING_H__
#define __VCN_SW_RING_H__

#define VCN_SW_RING_EMIT_FRAME_SIZE \
		(4 +   \
		5 + 5 +   \
		1)  

void vcn_dec_sw_ring_emit_fence(struct amdgpu_ring *ring, u64 addr,
	u64 seq, uint32_t flags);
void vcn_dec_sw_ring_insert_end(struct amdgpu_ring *ring);
void vcn_dec_sw_ring_emit_ib(struct amdgpu_ring *ring, struct amdgpu_job *job,
	struct amdgpu_ib *ib, uint32_t flags);
void vcn_dec_sw_ring_emit_reg_wait(struct amdgpu_ring *ring, uint32_t reg,
	uint32_t val, uint32_t mask);
void vcn_dec_sw_ring_emit_vm_flush(struct amdgpu_ring *ring,
	uint32_t vmid, uint64_t pd_addr);
void vcn_dec_sw_ring_emit_wreg(struct amdgpu_ring *ring, uint32_t reg,
	uint32_t val);

#endif  
