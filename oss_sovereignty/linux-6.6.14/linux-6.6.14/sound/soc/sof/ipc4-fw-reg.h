#ifndef __IPC4_FW_REG_H__
#define __IPC4_FW_REG_H__
#define SOF_IPC4_INVALID_STREAM_POSITION	ULLONG_MAX
struct sof_ipc4_pipeline_registers {
	u64 stream_start_offset;
	u64 stream_end_offset;
} __packed __aligned(4);
#define SOF_IPC4_PV_MAX_SUPPORTED_CHANNELS 8
struct sof_ipc4_peak_volume_regs {
	u32 peak_meter[SOF_IPC4_PV_MAX_SUPPORTED_CHANNELS];
	u32 current_volume[SOF_IPC4_PV_MAX_SUPPORTED_CHANNELS];
	u32 target_volume[SOF_IPC4_PV_MAX_SUPPORTED_CHANNELS];
} __packed __aligned(4);
struct sof_ipc4_llp_reading {
	u32 llp_l;
	u32 llp_u;
	u32 wclk_l;
	u32 wclk_u;
} __packed __aligned(4);
struct sof_ipc4_llp_reading_extended {
	struct sof_ipc4_llp_reading llp_reading;
	u32 tpd_low;
	u32 tpd_high;
} __packed __aligned(4);
struct sof_ipc4_llp_reading_slot {
	u32 node_id;
	struct sof_ipc4_llp_reading reading;
} __packed __aligned(4);
#define SOF_IPC4_FW_FUSE_VALUE_MASK		GENMASK(7, 0)
#define SOF_IPC4_FW_LOAD_METHOD_MASK		BIT(8)
#define SOF_IPC4_FW_DOWNLINK_IPC_USE_DMA_MASK	BIT(9)
#define SOF_IPC4_FW_LOAD_METHOD_REV_MASK	GENMASK(11, 10)
#define SOF_IPC4_FW_REVISION_MIN_MASK		GENMASK(15, 12)
#define SOF_IPC4_FW_REVISION_MAJ_MASK		GENMASK(19, 16)
#define SOF_IPC4_FW_VERSION_MIN_MASK		GENMASK(23, 20)
#define SOF_IPC4_FW_VERSION_MAJ_MASK		GENMASK(27, 24)
#define SOF_IPC4_MAX_SUPPORTED_ADSP_CORES	8
#define SOF_IPC4_MAX_PIPELINE_REG_SLOTS		16
#define SOF_IPC4_MAX_PEAK_VOL_REG_SLOTS		16
#define SOF_IPC4_MAX_LLP_GPDMA_READING_SLOTS	24
#define SOF_IPC4_MAX_LLP_SNDW_READING_SLOTS	15
#define SOF_IPC4_FW_REGS_ABI_VER		1
struct sof_ipc4_fw_registers {
	u32 fw_status;
	u32 lec;
	u32 fps;
	u32 lnec;
	u32 ltr;
	u32 rsvd0;
	u32 rom_info;
	u32 abi_ver;
	u8 slave_core_sts[SOF_IPC4_MAX_SUPPORTED_ADSP_CORES];
	u32 rsvd2[6];
	struct sof_ipc4_pipeline_registers
		pipeline_regs[SOF_IPC4_MAX_PIPELINE_REG_SLOTS];
	struct sof_ipc4_peak_volume_regs
		peak_vol_regs[SOF_IPC4_MAX_PEAK_VOL_REG_SLOTS];
	struct sof_ipc4_llp_reading_slot
		llp_gpdma_reading_slots[SOF_IPC4_MAX_LLP_GPDMA_READING_SLOTS];
	struct sof_ipc4_llp_reading_slot
		llp_sndw_reading_slots[SOF_IPC4_MAX_LLP_SNDW_READING_SLOTS];
	struct sof_ipc4_llp_reading_slot llp_evad_reading_slot;
} __packed __aligned(4);
#endif
