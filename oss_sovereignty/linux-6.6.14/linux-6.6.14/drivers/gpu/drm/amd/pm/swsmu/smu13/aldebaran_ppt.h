#ifndef __ALDEBARAN_PPT_H__
#define __ALDEBARAN_PPT_H__
#define ALDEBARAN_UMD_PSTATE_GFXCLK_LEVEL         0x3
#define ALDEBARAN_UMD_PSTATE_SOCCLK_LEVEL         0x3
#define ALDEBARAN_UMD_PSTATE_MCLK_LEVEL           0x2
#define MAX_DPM_NUMBER 16
#define ALDEBARAN_MAX_PCIE_CONF 2
struct aldebaran_dpm_level {
	bool            enabled;
	uint32_t        value;
	uint32_t        param1;
};
struct aldebaran_dpm_state {
	uint32_t  soft_min_level;
	uint32_t  soft_max_level;
	uint32_t  hard_min_level;
	uint32_t  hard_max_level;
};
struct aldebaran_single_dpm_table {
	uint32_t                count;
	struct aldebaran_dpm_state dpm_state;
	struct aldebaran_dpm_level dpm_levels[MAX_DPM_NUMBER];
};
struct aldebaran_pcie_table {
	uint16_t count;
	uint8_t  pcie_gen[ALDEBARAN_MAX_PCIE_CONF];
	uint8_t  pcie_lane[ALDEBARAN_MAX_PCIE_CONF];
	uint32_t lclk[ALDEBARAN_MAX_PCIE_CONF];
};
struct aldebaran_dpm_table {
	struct aldebaran_single_dpm_table  soc_table;
	struct aldebaran_single_dpm_table  gfx_table;
	struct aldebaran_single_dpm_table  mem_table;
	struct aldebaran_single_dpm_table  eclk_table;
	struct aldebaran_single_dpm_table  vclk_table;
	struct aldebaran_single_dpm_table  dclk_table;
	struct aldebaran_single_dpm_table  fclk_table;
	struct aldebaran_pcie_table        pcie_table;
};
extern void aldebaran_set_ppt_funcs(struct smu_context *smu);
#endif
