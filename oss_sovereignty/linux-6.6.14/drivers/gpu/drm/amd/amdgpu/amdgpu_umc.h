 
#ifndef __AMDGPU_UMC_H__
#define __AMDGPU_UMC_H__
#include "amdgpu_ras.h"

 
#define ADDR_OF_4KB_BLOCK(addr)			(((addr) & ~0xffULL) << 4)
 
#define ADDR_OF_8KB_BLOCK(addr)			(((addr) & ~0xffULL) << 5)
 
#define ADDR_OF_256B_BLOCK(channel_index)	((channel_index) << 8)
 
#define OFFSET_IN_256B_BLOCK(addr)		((addr) & 0xffULL)

#define LOOP_UMC_INST(umc_inst) for ((umc_inst) = 0; (umc_inst) < adev->umc.umc_inst_num; (umc_inst)++)
#define LOOP_UMC_CH_INST(ch_inst) for ((ch_inst) = 0; (ch_inst) < adev->umc.channel_inst_num; (ch_inst)++)
#define LOOP_UMC_INST_AND_CH(umc_inst, ch_inst) LOOP_UMC_INST((umc_inst)) LOOP_UMC_CH_INST((ch_inst))

#define LOOP_UMC_NODE_INST(node_inst) \
		for_each_set_bit((node_inst), &(adev->umc.active_mask), adev->umc.node_inst_num)

#define LOOP_UMC_EACH_NODE_INST_AND_CH(node_inst, umc_inst, ch_inst) \
		LOOP_UMC_NODE_INST((node_inst)) LOOP_UMC_INST_AND_CH((umc_inst), (ch_inst))


typedef int (*umc_func)(struct amdgpu_device *adev, uint32_t node_inst,
			uint32_t umc_inst, uint32_t ch_inst, void *data);

struct amdgpu_umc_ras {
	struct amdgpu_ras_block_object ras_block;
	void (*err_cnt_init)(struct amdgpu_device *adev);
	bool (*query_ras_poison_mode)(struct amdgpu_device *adev);
	void (*ecc_info_query_ras_error_count)(struct amdgpu_device *adev,
				      void *ras_error_status);
	void (*ecc_info_query_ras_error_address)(struct amdgpu_device *adev,
					void *ras_error_status);
	 
	void (*set_eeprom_table_version)(struct amdgpu_ras_eeprom_table_header *hdr);
};

struct amdgpu_umc_funcs {
	void (*init_registers)(struct amdgpu_device *adev);
};

struct amdgpu_umc {
	 
	uint32_t max_ras_err_cnt_per_query;
	 
	uint32_t channel_inst_num;
	 
	uint32_t umc_inst_num;

	 
	uint32_t node_inst_num;

	 
	uint32_t channel_offs;
	 
	uint32_t retire_unit;
	 
	const uint32_t *channel_idx_tbl;
	struct ras_common_if *ras_if;

	const struct amdgpu_umc_funcs *funcs;
	struct amdgpu_umc_ras *ras;

	 
	unsigned long active_mask;
};

int amdgpu_umc_ras_sw_init(struct amdgpu_device *adev);
int amdgpu_umc_ras_late_init(struct amdgpu_device *adev, struct ras_common_if *ras_block);
int amdgpu_umc_poison_handler(struct amdgpu_device *adev, bool reset);
int amdgpu_umc_process_ecc_irq(struct amdgpu_device *adev,
		struct amdgpu_irq_src *source,
		struct amdgpu_iv_entry *entry);
void amdgpu_umc_fill_error_record(struct ras_err_data *err_data,
		uint64_t err_addr,
		uint64_t retired_page,
		uint32_t channel_index,
		uint32_t umc_inst);

int amdgpu_umc_process_ras_data_cb(struct amdgpu_device *adev,
		void *ras_error_status,
		struct amdgpu_iv_entry *entry);
int amdgpu_umc_page_retirement_mca(struct amdgpu_device *adev,
			uint64_t err_addr, uint32_t ch_inst, uint32_t umc_inst);

int amdgpu_umc_loop_channels(struct amdgpu_device *adev,
			umc_func func, void *data);
#endif
