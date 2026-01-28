#ifndef __AMDGPU_JPEG_H__
#define __AMDGPU_JPEG_H__
#include "amdgpu_ras.h"
#define AMDGPU_MAX_JPEG_INSTANCES	4
#define AMDGPU_MAX_JPEG_RINGS		8
#define AMDGPU_JPEG_HARVEST_JPEG0 (1 << 0)
#define AMDGPU_JPEG_HARVEST_JPEG1 (1 << 1)
struct amdgpu_jpeg_reg{
	unsigned jpeg_pitch[AMDGPU_MAX_JPEG_RINGS];
};
struct amdgpu_jpeg_inst {
	struct amdgpu_ring ring_dec[AMDGPU_MAX_JPEG_RINGS];
	struct amdgpu_irq_src irq;
	struct amdgpu_irq_src ras_poison_irq;
	struct amdgpu_jpeg_reg external;
	uint8_t aid_id;
};
struct amdgpu_jpeg_ras {
	struct amdgpu_ras_block_object ras_block;
};
struct amdgpu_jpeg {
	uint8_t	num_jpeg_inst;
	struct amdgpu_jpeg_inst inst[AMDGPU_MAX_JPEG_INSTANCES];
	unsigned num_jpeg_rings;
	struct amdgpu_jpeg_reg internal;
	unsigned harvest_config;
	struct delayed_work idle_work;
	enum amd_powergating_state cur_state;
	struct mutex jpeg_pg_lock;
	atomic_t total_submission_cnt;
	struct ras_common_if	*ras_if;
	struct amdgpu_jpeg_ras	*ras;
	uint16_t inst_mask;
	uint8_t num_inst_per_aid;
};
int amdgpu_jpeg_sw_init(struct amdgpu_device *adev);
int amdgpu_jpeg_sw_fini(struct amdgpu_device *adev);
int amdgpu_jpeg_suspend(struct amdgpu_device *adev);
int amdgpu_jpeg_resume(struct amdgpu_device *adev);
void amdgpu_jpeg_ring_begin_use(struct amdgpu_ring *ring);
void amdgpu_jpeg_ring_end_use(struct amdgpu_ring *ring);
int amdgpu_jpeg_dec_ring_test_ring(struct amdgpu_ring *ring);
int amdgpu_jpeg_dec_ring_test_ib(struct amdgpu_ring *ring, long timeout);
int amdgpu_jpeg_process_poison_irq(struct amdgpu_device *adev,
				struct amdgpu_irq_src *source,
				struct amdgpu_iv_entry *entry);
int amdgpu_jpeg_ras_late_init(struct amdgpu_device *adev,
				struct ras_common_if *ras_block);
int amdgpu_jpeg_ras_sw_init(struct amdgpu_device *adev);
#endif  
