#ifndef PMF_H
#define PMF_H
#include <linux/acpi.h>
#include <linux/platform_profile.h>
#define APMF_FUNC_VERIFY_INTERFACE			0
#define APMF_FUNC_GET_SYS_PARAMS			1
#define APMF_FUNC_SBIOS_REQUESTS			2
#define APMF_FUNC_SBIOS_HEARTBEAT			4
#define APMF_FUNC_AUTO_MODE					5
#define APMF_FUNC_SET_FAN_IDX				7
#define APMF_FUNC_OS_POWER_SLIDER_UPDATE		8
#define APMF_FUNC_STATIC_SLIDER_GRANULAR       9
#define APMF_FUNC_DYN_SLIDER_AC				11
#define APMF_FUNC_DYN_SLIDER_DC				12
#define SET_SPL				0x03  
#define SET_SPPT			0x05  
#define SET_FPPT			0x07  
#define GET_SPL				0x0B
#define GET_SPPT			0x0D
#define GET_FPPT			0x0F
#define SET_DRAM_ADDR_HIGH	0x14
#define SET_DRAM_ADDR_LOW	0x15
#define SET_TRANSFER_TABLE	0x16
#define SET_STT_MIN_LIMIT	0x18  
#define SET_STT_LIMIT_APU	0x19
#define SET_STT_LIMIT_HS2	0x1A
#define SET_SPPT_APU_ONLY	0x1D
#define GET_SPPT_APU_ONLY	0x1E
#define GET_STT_MIN_LIMIT	0x1F
#define GET_STT_LIMIT_APU	0x20
#define GET_STT_LIMIT_HS2	0x21
#define DC_BEST_PERF		0
#define DC_BETTER_PERF		1
#define DC_BATTERY_SAVER	3
#define AC_BEST_PERF		4
#define AC_BETTER_PERF		5
#define AC_BETTER_BATTERY	6
#define FAN_INDEX_AUTO		0xFFFFFFFF
#define ARG_NONE 0
#define AVG_SAMPLE_SIZE 3
struct apmf_verify_interface {
	u16 size;
	u16 version;
	u32 notification_mask;
	u32 supported_functions;
} __packed;
struct apmf_system_params {
	u16 size;
	u32 valid_mask;
	u32 flags;
	u8 command_code;
	u32 heartbeat_int;
} __packed;
struct apmf_sbios_req {
	u16 size;
	u32 pending_req;
	u8 rsd;
	u8 cql_event;
	u8 amt_event;
	u32 fppt;
	u32 sppt;
	u32 fppt_apu_only;
	u32 spl;
	u32 stt_min_limit;
	u8 skin_temp_apu;
	u8 skin_temp_hs2;
} __packed;
struct apmf_fan_idx {
	u16 size;
	u8 fan_ctl_mode;
	u32 fan_ctl_idx;
} __packed;
struct smu_pmf_metrics {
	u16 gfxclk_freq;  
	u16 socclk_freq;  
	u16 vclk_freq;  
	u16 dclk_freq;  
	u16 memclk_freq;  
	u16 spare;
	u16 gfx_activity;  
	u16 uvd_activity;  
	u16 voltage[2];  
	u16 currents[2];  
	u16 power[2]; 
	u16 core_freq[8];  
	u16 core_power[8];  
	u16 core_temp[8];  
	u16 l3_freq;  
	u16 l3_temp;  
	u16 gfx_temp;  
	u16 soc_temp;  
	u16 throttler_status;
	u16 current_socketpower;  
	u16 stapm_orig_limit;  
	u16 stapm_cur_limit;  
	u32 apu_power;  
	u32 dgpu_power;  
	u16 vdd_tdc_val;  
	u16 soc_tdc_val;  
	u16 vdd_edc_val;  
	u16 soc_edcv_al;  
	u16 infra_cpu_maxfreq;  
	u16 infra_gfx_maxfreq;  
	u16 skin_temp;  
	u16 device_state;
} __packed;
enum amd_stt_skin_temp {
	STT_TEMP_APU,
	STT_TEMP_HS2,
	STT_TEMP_COUNT,
};
enum amd_slider_op {
	SLIDER_OP_GET,
	SLIDER_OP_SET,
};
enum power_source {
	POWER_SOURCE_AC,
	POWER_SOURCE_DC,
	POWER_SOURCE_MAX,
};
enum power_modes {
	POWER_MODE_PERFORMANCE,
	POWER_MODE_BALANCED_POWER,
	POWER_MODE_POWER_SAVER,
	POWER_MODE_MAX,
};
struct amd_pmf_dev {
	void __iomem *regbase;
	void __iomem *smu_virt_addr;
	void *buf;
	u32 base_addr;
	u32 cpu_id;
	struct device *dev;
	struct mutex lock;  
	u32 supported_func;
	enum platform_profile_option current_profile;
	struct platform_profile_handler pprof;
	struct dentry *dbgfs_dir;
	int hb_interval;  
	struct delayed_work heart_beat;
	struct smu_pmf_metrics m_table;
	struct delayed_work work_buffer;
	ktime_t start_time;
	int socket_power_history[AVG_SAMPLE_SIZE];
	int socket_power_history_idx;
	bool amt_enabled;
	struct mutex update_mutex;  
	bool cnqf_enabled;
	bool cnqf_supported;
	struct notifier_block pwr_src_notifier;
};
struct apmf_sps_prop_granular {
	u32 fppt;
	u32 sppt;
	u32 sppt_apu_only;
	u32 spl;
	u32 stt_min;
	u8 stt_skin_temp[STT_TEMP_COUNT];
	u32 fan_id;
} __packed;
struct apmf_static_slider_granular_output {
	u16 size;
	struct apmf_sps_prop_granular prop[POWER_SOURCE_MAX * POWER_MODE_MAX];
} __packed;
struct amd_pmf_static_slider_granular {
	u16 size;
	struct apmf_sps_prop_granular prop[POWER_SOURCE_MAX][POWER_MODE_MAX];
};
struct os_power_slider {
	u16 size;
	u8 slider_event;
} __packed;
struct fan_table_control {
	bool manual;
	unsigned long fan_id;
};
struct power_table_control {
	u32 spl;
	u32 sppt;
	u32 fppt;
	u32 sppt_apu_only;
	u32 stt_min;
	u32 stt_skin_temp[STT_TEMP_COUNT];
	u32 reserved[16];
};
enum auto_mode_transition_priority {
	AUTO_TRANSITION_TO_PERFORMANCE,  
	AUTO_TRANSITION_FROM_QUIET_TO_BALANCE,  
	AUTO_TRANSITION_TO_QUIET,  
	AUTO_TRANSITION_FROM_PERFORMANCE_TO_BALANCE,  
	AUTO_TRANSITION_MAX,
};
enum auto_mode_mode {
	AUTO_QUIET,
	AUTO_BALANCE,
	AUTO_PERFORMANCE_ON_LAP,
	AUTO_PERFORMANCE,
	AUTO_MODE_MAX,
};
struct auto_mode_trans_params {
	u32 time_constant;  
	u32 power_delta;  
	u32 power_threshold;
	u32 timer;  
	u32 applied;
	enum auto_mode_mode target_mode;
	u32 shifting_up;
};
struct auto_mode_mode_settings {
	struct power_table_control power_control;
	struct fan_table_control fan_control;
	u32 power_floor;
};
struct auto_mode_mode_config {
	struct auto_mode_trans_params transition[AUTO_TRANSITION_MAX];
	struct auto_mode_mode_settings mode_set[AUTO_MODE_MAX];
	enum auto_mode_mode current_mode;
};
struct apmf_auto_mode {
	u16 size;
	u32 balanced_to_perf;
	u32 perf_to_balanced;
	u32 quiet_to_balanced;
	u32 balanced_to_quiet;
	u32 pfloor_perf;
	u32 pfloor_balanced;
	u32 pfloor_quiet;
	u32 pd_balanced_to_perf;
	u32 pd_perf_to_balanced;
	u32 pd_quiet_to_balanced;
	u32 pd_balanced_to_quiet;
	u8 stt_apu_perf_on_lap;  
	u8 stt_hs2_perf_on_lap;  
	u8 stt_apu_perf;
	u8 stt_hs2_perf;
	u8 stt_apu_balanced;
	u8 stt_hs2_balanced;
	u8 stt_apu_quiet;
	u8 stt_hs2_quiet;
	u32 stt_min_limit_perf_on_lap;  
	u32 stt_min_limit_perf;
	u32 stt_min_limit_balanced;
	u32 stt_min_limit_quiet;
	u32 fppt_perf_on_lap;  
	u32 sppt_perf_on_lap;  
	u32 spl_perf_on_lap;  
	u32 sppt_apu_only_perf_on_lap;  
	u32 fppt_perf;
	u32 sppt_perf;
	u32 spl_perf;
	u32 sppt_apu_only_perf;
	u32 fppt_balanced;
	u32 sppt_balanced;
	u32 spl_balanced;
	u32 sppt_apu_only_balanced;
	u32 fppt_quiet;
	u32 sppt_quiet;
	u32 spl_quiet;
	u32 sppt_apu_only_quiet;
	u32 fan_id_perf;
	u32 fan_id_balanced;
	u32 fan_id_quiet;
} __packed;
enum cnqf_trans_priority {
	CNQF_TRANSITION_TO_TURBO,  
	CNQF_TRANSITION_FROM_BALANCE_TO_PERFORMANCE,  
	CNQF_TRANSITION_FROM_QUIET_TO_BALANCE,  
	CNQF_TRANSITION_TO_QUIET,  
	CNQF_TRANSITION_FROM_PERFORMANCE_TO_BALANCE,  
	CNQF_TRANSITION_FROM_TURBO_TO_PERFORMANCE,  
	CNQF_TRANSITION_MAX,
};
enum cnqf_mode {
	CNQF_MODE_QUIET,
	CNQF_MODE_BALANCE,
	CNQF_MODE_PERFORMANCE,
	CNQF_MODE_TURBO,
	CNQF_MODE_MAX,
};
enum apmf_cnqf_pos {
	APMF_CNQF_TURBO,
	APMF_CNQF_PERFORMANCE,
	APMF_CNQF_BALANCE,
	APMF_CNQF_QUIET,
	APMF_CNQF_MAX,
};
struct cnqf_mode_settings {
	struct power_table_control power_control;
	struct fan_table_control fan_control;
	u32 power_floor;
};
struct cnqf_tran_params {
	u32 time_constant;  
	u32 power_threshold;
	u32 timer;  
	u32 total_power;
	u32 count;
	bool priority;
	bool shifting_up;
	enum cnqf_mode target_mode;
};
struct cnqf_config {
	struct cnqf_tran_params trans_param[POWER_SOURCE_MAX][CNQF_TRANSITION_MAX];
	struct cnqf_mode_settings mode_set[POWER_SOURCE_MAX][CNQF_MODE_MAX];
	struct power_table_control defaults;
	enum cnqf_mode current_mode;
	u32 power_src;
	u32 avg_power;
};
struct apmf_cnqf_power_set {
	u32 pfloor;
	u32 fppt;
	u32 sppt;
	u32 sppt_apu_only;
	u32 spl;
	u32 stt_min_limit;
	u8 stt_skintemp[STT_TEMP_COUNT];
	u32 fan_id;
} __packed;
struct apmf_dyn_slider_output {
	u16 size;
	u16 flags;
	u32 t_perf_to_turbo;
	u32 t_balanced_to_perf;
	u32 t_quiet_to_balanced;
	u32 t_balanced_to_quiet;
	u32 t_perf_to_balanced;
	u32 t_turbo_to_perf;
	struct apmf_cnqf_power_set ps[APMF_CNQF_MAX];
} __packed;
int apmf_acpi_init(struct amd_pmf_dev *pmf_dev);
void apmf_acpi_deinit(struct amd_pmf_dev *pmf_dev);
int is_apmf_func_supported(struct amd_pmf_dev *pdev, unsigned long index);
int amd_pmf_send_cmd(struct amd_pmf_dev *dev, u8 message, bool get, u32 arg, u32 *data);
int amd_pmf_init_metrics_table(struct amd_pmf_dev *dev);
int amd_pmf_get_power_source(void);
int apmf_install_handler(struct amd_pmf_dev *pmf_dev);
int apmf_os_power_slider_update(struct amd_pmf_dev *dev, u8 flag);
int amd_pmf_get_pprof_modes(struct amd_pmf_dev *pmf);
void amd_pmf_update_slider(struct amd_pmf_dev *dev, bool op, int idx,
			   struct amd_pmf_static_slider_granular *table);
int amd_pmf_init_sps(struct amd_pmf_dev *dev);
void amd_pmf_deinit_sps(struct amd_pmf_dev *dev);
int apmf_get_static_slider_granular(struct amd_pmf_dev *pdev,
				    struct apmf_static_slider_granular_output *output);
bool is_pprof_balanced(struct amd_pmf_dev *pmf);
int amd_pmf_power_slider_update_event(struct amd_pmf_dev *dev);
int apmf_update_fan_idx(struct amd_pmf_dev *pdev, bool manual, u32 idx);
int amd_pmf_set_sps_power_limits(struct amd_pmf_dev *pmf);
int apmf_get_auto_mode_def(struct amd_pmf_dev *pdev, struct apmf_auto_mode *data);
void amd_pmf_init_auto_mode(struct amd_pmf_dev *dev);
void amd_pmf_deinit_auto_mode(struct amd_pmf_dev *dev);
void amd_pmf_trans_automode(struct amd_pmf_dev *dev, int socket_power, ktime_t time_elapsed_ms);
int apmf_get_sbios_requests(struct amd_pmf_dev *pdev, struct apmf_sbios_req *req);
void amd_pmf_update_2_cql(struct amd_pmf_dev *dev, bool is_cql_event);
int amd_pmf_reset_amt(struct amd_pmf_dev *dev);
void amd_pmf_handle_amt(struct amd_pmf_dev *dev);
int apmf_get_dyn_slider_def_ac(struct amd_pmf_dev *pdev, struct apmf_dyn_slider_output *data);
int apmf_get_dyn_slider_def_dc(struct amd_pmf_dev *pdev, struct apmf_dyn_slider_output *data);
int amd_pmf_init_cnqf(struct amd_pmf_dev *dev);
void amd_pmf_deinit_cnqf(struct amd_pmf_dev *dev);
int amd_pmf_trans_cnqf(struct amd_pmf_dev *dev, int socket_power, ktime_t time_lapsed_ms);
extern const struct attribute_group cnqf_feature_attribute_group;
#endif  
