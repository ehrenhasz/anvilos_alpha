#ifndef DM_PP_SMU_IF__H
#define DM_PP_SMU_IF__H
enum pp_smu_ver {
	PP_SMU_UNSUPPORTED,
	PP_SMU_VER_RV,
	PP_SMU_VER_NV,
	PP_SMU_VER_RN,
	PP_SMU_VER_MAX
};
struct pp_smu {
	enum pp_smu_ver ver;
	const void *pp;
	const void *dm;
};
enum pp_smu_status {
	PP_SMU_RESULT_UNDEFINED = 0,
	PP_SMU_RESULT_OK = 1,
	PP_SMU_RESULT_FAIL,
	PP_SMU_RESULT_UNSUPPORTED
};
#define PP_SMU_WM_SET_RANGE_CLK_UNCONSTRAINED_MIN 0x0
#define PP_SMU_WM_SET_RANGE_CLK_UNCONSTRAINED_MAX 0xFFFF
enum wm_type {
	WM_TYPE_PSTATE_CHG = 0,
	WM_TYPE_RETRAINING = 1,
};
struct pp_smu_wm_set_range {
	uint16_t min_fill_clk_mhz;
	uint16_t max_fill_clk_mhz;
	uint16_t min_drain_clk_mhz;
	uint16_t max_drain_clk_mhz;
	uint8_t wm_inst;
	uint8_t wm_type;
};
#define MAX_WATERMARK_SETS 4
struct pp_smu_wm_range_sets {
	unsigned int num_reader_wm_sets;
	struct pp_smu_wm_set_range reader_wm_sets[MAX_WATERMARK_SETS];
	unsigned int num_writer_wm_sets;
	struct pp_smu_wm_set_range writer_wm_sets[MAX_WATERMARK_SETS];
};
struct pp_smu_funcs_rv {
	struct pp_smu pp_smu;
	void (*set_display_count)(struct pp_smu *pp, int count);
	void (*set_wm_ranges)(struct pp_smu *pp,
			struct pp_smu_wm_range_sets *ranges);
	void (*set_hard_min_dcfclk_by_freq)(struct pp_smu *pp, int mhz);
	void (*set_min_deep_sleep_dcfclk)(struct pp_smu *pp, int mhz);
	void (*set_hard_min_fclk_by_freq)(struct pp_smu *pp, int mhz);
	void (*set_hard_min_socclk_by_freq)(struct pp_smu *pp, int mhz);
	void (*set_pme_wa_enable)(struct pp_smu *pp);
};
enum pp_smu_nv_clock_id {
	PP_SMU_NV_DISPCLK,
	PP_SMU_NV_PHYCLK,
	PP_SMU_NV_PIXELCLK
};
struct pp_smu_nv_clock_table {
	unsigned int    displayClockInKhz;
	unsigned int	dppClockInKhz;
	unsigned int    phyClockInKhz;
	unsigned int    pixelClockInKhz;
	unsigned int	dscClockInKhz;
	unsigned int	fabricClockInKhz;
	unsigned int	socClockInKhz;
	unsigned int    dcfClockInKhz;
	unsigned int    uClockInKhz;
};
struct pp_smu_funcs_nv {
	struct pp_smu pp_smu;
	enum pp_smu_status (*set_display_count)(struct pp_smu *pp, int count);
	enum pp_smu_status (*set_hard_min_dcfclk_by_freq)(struct pp_smu *pp, int Mhz);
	enum pp_smu_status (*set_min_deep_sleep_dcfclk)(struct pp_smu *pp, int Mhz);
	enum pp_smu_status (*set_hard_min_uclk_by_freq)(struct pp_smu *pp, int Mhz);
	enum pp_smu_status (*set_hard_min_socclk_by_freq)(struct pp_smu *pp, int Mhz);
	enum pp_smu_status (*set_pme_wa_enable)(struct pp_smu *pp);
	enum pp_smu_status (*set_voltage_by_freq)(struct pp_smu *pp,
			enum pp_smu_nv_clock_id clock_id, int Mhz);
	enum pp_smu_status (*set_wm_ranges)(struct pp_smu *pp,
			struct pp_smu_wm_range_sets *ranges);
	enum pp_smu_status (*get_maximum_sustainable_clocks)(struct pp_smu *pp,
			struct pp_smu_nv_clock_table *max_clocks);
	enum pp_smu_status (*get_uclk_dpm_states)(struct pp_smu *pp,
			unsigned int *clock_values_in_khz, unsigned int *num_states);
	enum pp_smu_status (*set_pstate_handshake_support)(struct pp_smu *pp,
			bool pstate_handshake_supported);
};
#define PP_SMU_NUM_SOCCLK_DPM_LEVELS  8
#define PP_SMU_NUM_DCFCLK_DPM_LEVELS  8
#define PP_SMU_NUM_FCLK_DPM_LEVELS    4
#define PP_SMU_NUM_MEMCLK_DPM_LEVELS  4
#define PP_SMU_NUM_DCLK_DPM_LEVELS    8
#define PP_SMU_NUM_VCLK_DPM_LEVELS    8
struct dpm_clock {
  uint32_t  Freq;     
  uint32_t  Vol;      
};
struct dpm_clocks {
	struct dpm_clock DcfClocks[PP_SMU_NUM_DCFCLK_DPM_LEVELS];
	struct dpm_clock SocClocks[PP_SMU_NUM_SOCCLK_DPM_LEVELS];
	struct dpm_clock FClocks[PP_SMU_NUM_FCLK_DPM_LEVELS];
	struct dpm_clock MemClocks[PP_SMU_NUM_MEMCLK_DPM_LEVELS];
	struct dpm_clock VClocks[PP_SMU_NUM_VCLK_DPM_LEVELS];
	struct dpm_clock DClocks[PP_SMU_NUM_DCLK_DPM_LEVELS];
};
struct pp_smu_funcs_rn {
	struct pp_smu pp_smu;
	enum pp_smu_status (*set_wm_ranges)(struct pp_smu *pp,
			struct pp_smu_wm_range_sets *ranges);
	enum pp_smu_status (*get_dpm_clock_table) (struct pp_smu *pp,
			struct dpm_clocks *clock_table);
};
struct pp_smu_funcs_vgh {
	struct pp_smu pp_smu;
	enum pp_smu_status (*set_wm_ranges)(struct pp_smu *pp,
			struct pp_smu_wm_range_sets *ranges);
	enum pp_smu_status (*get_dpm_clock_table) (struct pp_smu *pp,
			struct dpm_clocks *clock_table);
	enum pp_smu_status (*notify_smu_timeout) (struct pp_smu *pp);
};
struct pp_smu_funcs {
	struct pp_smu ctx;
	union {
		struct pp_smu_funcs_rv rv_funcs;
		struct pp_smu_funcs_nv nv_funcs;
		struct pp_smu_funcs_rn rn_funcs;
		struct pp_smu_funcs_vgh vgh_funcs;
	};
};
#endif  
