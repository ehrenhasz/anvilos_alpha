 

#ifndef __AMDGPU_SDMA_H__
#define __AMDGPU_SDMA_H__
#include "amdgpu_ras.h"

 
#define AMDGPU_MAX_SDMA_INSTANCES		16

enum amdgpu_sdma_irq {
	AMDGPU_SDMA_IRQ_INSTANCE0  = 0,
	AMDGPU_SDMA_IRQ_INSTANCE1,
	AMDGPU_SDMA_IRQ_INSTANCE2,
	AMDGPU_SDMA_IRQ_INSTANCE3,
	AMDGPU_SDMA_IRQ_INSTANCE4,
	AMDGPU_SDMA_IRQ_INSTANCE5,
	AMDGPU_SDMA_IRQ_INSTANCE6,
	AMDGPU_SDMA_IRQ_INSTANCE7,
	AMDGPU_SDMA_IRQ_INSTANCE8,
	AMDGPU_SDMA_IRQ_INSTANCE9,
	AMDGPU_SDMA_IRQ_INSTANCE10,
	AMDGPU_SDMA_IRQ_INSTANCE11,
	AMDGPU_SDMA_IRQ_INSTANCE12,
	AMDGPU_SDMA_IRQ_INSTANCE13,
	AMDGPU_SDMA_IRQ_INSTANCE14,
	AMDGPU_SDMA_IRQ_INSTANCE15,
	AMDGPU_SDMA_IRQ_LAST
};

#define NUM_SDMA(x) hweight32(x)

struct amdgpu_sdma_instance {
	 
	const struct firmware	*fw;
	uint32_t		fw_version;
	uint32_t		feature_version;

	struct amdgpu_ring	ring;
	struct amdgpu_ring	page;
	bool			burst_nop;
	uint32_t		aid_id;
};

enum amdgpu_sdma_ras_memory_id {
	AMDGPU_SDMA_MBANK_DATA_BUF0 = 1,
	AMDGPU_SDMA_MBANK_DATA_BUF1 = 2,
	AMDGPU_SDMA_MBANK_DATA_BUF2 = 3,
	AMDGPU_SDMA_MBANK_DATA_BUF3 = 4,
	AMDGPU_SDMA_MBANK_DATA_BUF4 = 5,
	AMDGPU_SDMA_MBANK_DATA_BUF5 = 6,
	AMDGPU_SDMA_MBANK_DATA_BUF6 = 7,
	AMDGPU_SDMA_MBANK_DATA_BUF7 = 8,
	AMDGPU_SDMA_MBANK_DATA_BUF8 = 9,
	AMDGPU_SDMA_MBANK_DATA_BUF9 = 10,
	AMDGPU_SDMA_MBANK_DATA_BUF10 = 11,
	AMDGPU_SDMA_MBANK_DATA_BUF11 = 12,
	AMDGPU_SDMA_MBANK_DATA_BUF12 = 13,
	AMDGPU_SDMA_MBANK_DATA_BUF13 = 14,
	AMDGPU_SDMA_MBANK_DATA_BUF14 = 15,
	AMDGPU_SDMA_MBANK_DATA_BUF15 = 16,
	AMDGPU_SDMA_UCODE_BUF = 17,
	AMDGPU_SDMA_RB_CMD_BUF = 18,
	AMDGPU_SDMA_IB_CMD_BUF = 19,
	AMDGPU_SDMA_UTCL1_RD_FIFO = 20,
	AMDGPU_SDMA_UTCL1_RDBST_FIFO = 21,
	AMDGPU_SDMA_UTCL1_WR_FIFO = 22,
	AMDGPU_SDMA_DATA_LUT_FIFO = 23,
	AMDGPU_SDMA_SPLIT_DAT_BUF = 24,
	AMDGPU_SDMA_MEMORY_BLOCK_LAST,
};

struct amdgpu_sdma_ras {
	struct amdgpu_ras_block_object ras_block;
};

struct amdgpu_sdma {
	struct amdgpu_sdma_instance instance[AMDGPU_MAX_SDMA_INSTANCES];
	struct amdgpu_irq_src	trap_irq;
	struct amdgpu_irq_src	illegal_inst_irq;
	struct amdgpu_irq_src	ecc_irq;
	struct amdgpu_irq_src	vm_hole_irq;
	struct amdgpu_irq_src	doorbell_invalid_irq;
	struct amdgpu_irq_src	pool_timeout_irq;
	struct amdgpu_irq_src	srbm_write_irq;

	int			num_instances;
	uint32_t 		sdma_mask;
	int			num_inst_per_aid;
	uint32_t                    srbm_soft_reset;
	bool			has_page_queue;
	struct ras_common_if	*ras_if;
	struct amdgpu_sdma_ras	*ras;
};

 
struct amdgpu_buffer_funcs {
	 
	uint32_t	copy_max_bytes;

	 
	unsigned	copy_num_dw;

	 
	void (*emit_copy_buffer)(struct amdgpu_ib *ib,
				  
				 uint64_t src_offset,
				  
				 uint64_t dst_offset,
				  
				 uint32_t byte_count,
				 bool tmz);

	 
	uint32_t	fill_max_bytes;

	 
	unsigned	fill_num_dw;

	 
	void (*emit_fill_buffer)(struct amdgpu_ib *ib,
				  
				 uint32_t src_data,
				  
				 uint64_t dst_offset,
				  
				 uint32_t byte_count);
};

#define amdgpu_emit_copy_buffer(adev, ib, s, d, b, t) (adev)->mman.buffer_funcs->emit_copy_buffer((ib),  (s), (d), (b), (t))
#define amdgpu_emit_fill_buffer(adev, ib, s, d, b) (adev)->mman.buffer_funcs->emit_fill_buffer((ib), (s), (d), (b))

struct amdgpu_sdma_instance *
amdgpu_sdma_get_instance_from_ring(struct amdgpu_ring *ring);
int amdgpu_sdma_get_index_from_ring(struct amdgpu_ring *ring, uint32_t *index);
uint64_t amdgpu_sdma_get_csa_mc_addr(struct amdgpu_ring *ring, unsigned vmid);
int amdgpu_sdma_ras_late_init(struct amdgpu_device *adev,
			      struct ras_common_if *ras_block);
int amdgpu_sdma_process_ras_data_cb(struct amdgpu_device *adev,
		void *err_data,
		struct amdgpu_iv_entry *entry);
int amdgpu_sdma_process_ecc_irq(struct amdgpu_device *adev,
				      struct amdgpu_irq_src *source,
				      struct amdgpu_iv_entry *entry);
int amdgpu_sdma_init_microcode(struct amdgpu_device *adev, u32 instance,
			       bool duplicate);
void amdgpu_sdma_destroy_inst_ctx(struct amdgpu_device *adev,
        bool duplicate);
void amdgpu_sdma_unset_buffer_funcs_helper(struct amdgpu_device *adev);
int amdgpu_sdma_ras_sw_init(struct amdgpu_device *adev);

#endif
