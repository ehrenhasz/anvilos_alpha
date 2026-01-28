#ifndef __ARCTURUS_PPT_H__
#define __ARCTURUS_PPT_H__
#define ARCTURUS_UMD_PSTATE_GFXCLK_LEVEL         0x3
#define ARCTURUS_UMD_PSTATE_SOCCLK_LEVEL         0x3
#define ARCTURUS_UMD_PSTATE_MCLK_LEVEL           0x2
#define MAX_DPM_NUMBER 16
#define MAX_PCIE_CONF 2
struct arcturus_dpm_level {
        bool            enabled;
        uint32_t        value;
        uint32_t        param1;
};
struct arcturus_dpm_state {
        uint32_t  soft_min_level;
        uint32_t  soft_max_level;
        uint32_t  hard_min_level;
        uint32_t  hard_max_level;
};
struct arcturus_single_dpm_table {
        uint32_t                count;
        struct arcturus_dpm_state dpm_state;
        struct arcturus_dpm_level dpm_levels[MAX_DPM_NUMBER];
};
struct arcturus_pcie_table {
        uint16_t count;
        uint8_t  pcie_gen[MAX_PCIE_CONF];
        uint8_t  pcie_lane[MAX_PCIE_CONF];
        uint32_t lclk[MAX_PCIE_CONF];
};
struct arcturus_dpm_table {
        struct arcturus_single_dpm_table  soc_table;
        struct arcturus_single_dpm_table  gfx_table;
        struct arcturus_single_dpm_table  mem_table;
        struct arcturus_single_dpm_table  eclk_table;
        struct arcturus_single_dpm_table  vclk_table;
        struct arcturus_single_dpm_table  dclk_table;
        struct arcturus_single_dpm_table  fclk_table;
        struct arcturus_pcie_table        pcie_table;
};
extern void arcturus_set_ppt_funcs(struct smu_context *smu);
#endif
