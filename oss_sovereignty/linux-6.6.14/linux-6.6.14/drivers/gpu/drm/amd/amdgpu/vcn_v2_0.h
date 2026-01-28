#ifndef __VCN_V2_0_H__
#define __VCN_V2_0_H__
extern void vcn_v2_0_dec_ring_insert_start(struct amdgpu_ring *ring);
extern void vcn_v2_0_dec_ring_insert_end(struct amdgpu_ring *ring);
extern void vcn_v2_0_dec_ring_insert_nop(struct amdgpu_ring *ring, uint32_t count);
extern void vcn_v2_0_dec_ring_emit_fence(struct amdgpu_ring *ring, u64 addr, u64 seq,
				unsigned flags);
extern void vcn_v2_0_dec_ring_emit_ib(struct amdgpu_ring *ring, struct amdgpu_job *job,
				struct amdgpu_ib *ib, uint32_t flags);
extern void vcn_v2_0_dec_ring_emit_reg_wait(struct amdgpu_ring *ring, uint32_t reg,
				uint32_t val, uint32_t mask);
extern void vcn_v2_0_dec_ring_emit_vm_flush(struct amdgpu_ring *ring,
				unsigned vmid, uint64_t pd_addr);
extern void vcn_v2_0_dec_ring_emit_wreg(struct amdgpu_ring *ring,
				uint32_t reg, uint32_t val);
extern int vcn_v2_0_dec_ring_test_ring(struct amdgpu_ring *ring);
extern void vcn_v2_0_enc_ring_insert_end(struct amdgpu_ring *ring);
extern void vcn_v2_0_enc_ring_emit_fence(struct amdgpu_ring *ring, u64 addr,
				u64 seq, unsigned flags);
extern void vcn_v2_0_enc_ring_emit_ib(struct amdgpu_ring *ring, struct amdgpu_job *job,
				struct amdgpu_ib *ib, uint32_t flags);
extern void vcn_v2_0_enc_ring_emit_reg_wait(struct amdgpu_ring *ring, uint32_t reg,
				uint32_t val, uint32_t mask);
extern void vcn_v2_0_enc_ring_emit_vm_flush(struct amdgpu_ring *ring,
				unsigned int vmid, uint64_t pd_addr);
extern void vcn_v2_0_enc_ring_emit_wreg(struct amdgpu_ring *ring, uint32_t reg, uint32_t val);
extern const struct amdgpu_ip_block_version vcn_v2_0_ip_block;
#endif  
