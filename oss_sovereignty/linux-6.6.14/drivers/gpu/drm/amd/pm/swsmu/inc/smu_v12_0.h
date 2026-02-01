 
#ifndef __SMU_V12_0_H__
#define __SMU_V12_0_H__

#include "amdgpu_smu.h"

 
#define MP0_Public			0x03800000
#define MP0_SRAM			0x03900000
#define MP1_Public			0x03b00000
#define MP1_SRAM			0x03c00004

#if defined(SWSMU_CODE_LAYER_L2) || defined(SWSMU_CODE_LAYER_L3)

int smu_v12_0_check_fw_status(struct smu_context *smu);

int smu_v12_0_check_fw_version(struct smu_context *smu);

int smu_v12_0_powergate_sdma(struct smu_context *smu, bool gate);

int smu_v12_0_powergate_vcn(struct smu_context *smu, bool gate);

int smu_v12_0_powergate_jpeg(struct smu_context *smu, bool gate);

int smu_v12_0_set_gfx_cgpg(struct smu_context *smu, bool enable);

uint32_t smu_v12_0_get_gfxoff_status(struct smu_context *smu);

int smu_v12_0_gfx_off_control(struct smu_context *smu, bool enable);

int smu_v12_0_fini_smc_tables(struct smu_context *smu);

int smu_v12_0_set_default_dpm_tables(struct smu_context *smu);

int smu_v12_0_mode2_reset(struct smu_context *smu);

int smu_v12_0_set_soft_freq_limited_range(struct smu_context *smu, enum smu_clk_type clk_type,
			    uint32_t min, uint32_t max);

int smu_v12_0_set_driver_table_location(struct smu_context *smu);

int smu_v12_0_get_vbios_bootup_values(struct smu_context *smu);

#endif
#endif
