#ifndef _SMU8_SMUMGR_H_
#define _SMU8_SMUMGR_H_
#define MAX_NUM_FIRMWARE                        8
#define MAX_NUM_SCRATCH                         11
#define SMU8_SCRATCH_SIZE_NONGFX_CLOCKGATING      1024
#define SMU8_SCRATCH_SIZE_NONGFX_GOLDENSETTING    2048
#define SMU8_SCRATCH_SIZE_SDMA_METADATA           1024
#define SMU8_SCRATCH_SIZE_IH                      ((2*256+1)*4)
#define SMU_EnabledFeatureScoreboard_SclkDpmOn    0x00200000
enum smu8_scratch_entry {
	SMU8_SCRATCH_ENTRY_UCODE_ID_SDMA0 = 0,
	SMU8_SCRATCH_ENTRY_UCODE_ID_SDMA1,
	SMU8_SCRATCH_ENTRY_UCODE_ID_CP_CE,
	SMU8_SCRATCH_ENTRY_UCODE_ID_CP_PFP,
	SMU8_SCRATCH_ENTRY_UCODE_ID_CP_ME,
	SMU8_SCRATCH_ENTRY_UCODE_ID_CP_MEC_JT1,
	SMU8_SCRATCH_ENTRY_UCODE_ID_CP_MEC_JT2,
	SMU8_SCRATCH_ENTRY_UCODE_ID_GMCON_RENG,
	SMU8_SCRATCH_ENTRY_UCODE_ID_RLC_G,
	SMU8_SCRATCH_ENTRY_UCODE_ID_RLC_SCRATCH,
	SMU8_SCRATCH_ENTRY_UCODE_ID_RLC_SRM_ARAM,
	SMU8_SCRATCH_ENTRY_UCODE_ID_RLC_SRM_DRAM,
	SMU8_SCRATCH_ENTRY_UCODE_ID_DMCU_ERAM,
	SMU8_SCRATCH_ENTRY_UCODE_ID_DMCU_IRAM,
	SMU8_SCRATCH_ENTRY_UCODE_ID_POWER_PROFILING,
	SMU8_SCRATCH_ENTRY_DATA_ID_SDMA_HALT,
	SMU8_SCRATCH_ENTRY_DATA_ID_SYS_CLOCKGATING,
	SMU8_SCRATCH_ENTRY_DATA_ID_SDMA_RING_REGS,
	SMU8_SCRATCH_ENTRY_DATA_ID_NONGFX_REINIT,
	SMU8_SCRATCH_ENTRY_DATA_ID_SDMA_START,
	SMU8_SCRATCH_ENTRY_DATA_ID_IH_REGISTERS,
	SMU8_SCRATCH_ENTRY_SMU8_FUSION_CLKTABLE
};
struct smu8_buffer_entry {
	uint32_t data_size;
	uint64_t mc_addr;
	void *kaddr;
	enum smu8_scratch_entry firmware_ID;
	struct amdgpu_bo *handle;  
};
struct smu8_register_index_data_pair {
	uint32_t offset;
	uint32_t value;
};
struct smu8_ih_meta_data {
	uint32_t command;
	struct smu8_register_index_data_pair register_index_value_pair[1];
};
struct smu8_smumgr {
	uint8_t driver_buffer_length;
	uint8_t scratch_buffer_length;
	uint16_t toc_entry_used_count;
	uint16_t toc_entry_initialize_index;
	uint16_t toc_entry_power_profiling_index;
	uint16_t toc_entry_aram;
	uint16_t toc_entry_ih_register_restore_task_index;
	uint16_t toc_entry_clock_table;
	uint16_t ih_register_restore_task_size;
	uint16_t smu_buffer_used_bytes;
	struct smu8_buffer_entry toc_buffer;
	struct smu8_buffer_entry smu_buffer;
	struct smu8_buffer_entry firmware_buffer;
	struct smu8_buffer_entry driver_buffer[MAX_NUM_FIRMWARE];
	struct smu8_buffer_entry meta_data_buffer[MAX_NUM_FIRMWARE];
	struct smu8_buffer_entry scratch_buffer[MAX_NUM_SCRATCH];
};
#endif
