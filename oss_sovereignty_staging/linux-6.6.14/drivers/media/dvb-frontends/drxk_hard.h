 
#include "drxk_map.h"

#define DRXK_VERSION_MAJOR 0
#define DRXK_VERSION_MINOR 9
#define DRXK_VERSION_PATCH 4300

#define HI_I2C_DELAY        42
#define HI_I2C_BRIDGE_DELAY 350
#define DRXK_MAX_RETRIES    100

#define DRIVER_4400 1

#define DRXX_JTAGID   0x039210D9
#define DRXX_J_JTAGID 0x239310D9
#define DRXX_K_JTAGID 0x039210D9

#define DRX_UNKNOWN     254
#define DRX_AUTO        255

#define DRX_SCU_READY   0
#define DRXK_MAX_WAITTIME (200)
#define SCU_RESULT_OK      0
#define SCU_RESULT_SIZE   -4
#define SCU_RESULT_INVPAR -3
#define SCU_RESULT_UNKSTD -2
#define SCU_RESULT_UNKCMD -1

#ifndef DRXK_OFDM_TR_SHUTDOWN_TIMEOUT
#define DRXK_OFDM_TR_SHUTDOWN_TIMEOUT (200)
#endif

#define DRXK_8VSB_MPEG_BIT_RATE     19392658UL   
#define DRXK_DVBT_MPEG_BIT_RATE     32000000UL   
#define DRXK_QAM16_MPEG_BIT_RATE    27000000UL   
#define DRXK_QAM32_MPEG_BIT_RATE    33000000UL   
#define DRXK_QAM64_MPEG_BIT_RATE    40000000UL   
#define DRXK_QAM128_MPEG_BIT_RATE   46000000UL   
#define DRXK_QAM256_MPEG_BIT_RATE   52000000UL   
#define DRXK_MAX_MPEG_BIT_RATE      52000000UL   

#define   IQM_CF_OUT_ENA_OFDM__M                                            0x4
#define     IQM_FS_ADJ_SEL_B_QAM                                            0x1
#define     IQM_FS_ADJ_SEL_B_OFF                                            0x0
#define     IQM_FS_ADJ_SEL_B_VSB                                            0x2
#define     IQM_RC_ADJ_SEL_B_OFF                                            0x0
#define     IQM_RC_ADJ_SEL_B_QAM                                            0x1
#define     IQM_RC_ADJ_SEL_B_VSB                                            0x2

enum operation_mode {
	OM_NONE,
	OM_QAM_ITU_A,
	OM_QAM_ITU_B,
	OM_QAM_ITU_C,
	OM_DVBT
};

enum drx_power_mode {
	DRX_POWER_UP = 0,
	DRX_POWER_MODE_1,
	DRX_POWER_MODE_2,
	DRX_POWER_MODE_3,
	DRX_POWER_MODE_4,
	DRX_POWER_MODE_5,
	DRX_POWER_MODE_6,
	DRX_POWER_MODE_7,
	DRX_POWER_MODE_8,

	DRX_POWER_MODE_9,
	DRX_POWER_MODE_10,
	DRX_POWER_MODE_11,
	DRX_POWER_MODE_12,
	DRX_POWER_MODE_13,
	DRX_POWER_MODE_14,
	DRX_POWER_MODE_15,
	DRX_POWER_MODE_16,
	DRX_POWER_DOWN = 255
};


 
#ifndef DRXK_POWER_DOWN_OFDM
#define DRXK_POWER_DOWN_OFDM        DRX_POWER_MODE_1
#endif

 
#ifndef DRXK_POWER_DOWN_CORE
#define DRXK_POWER_DOWN_CORE        DRX_POWER_MODE_9
#endif

 
#ifndef DRXK_POWER_DOWN_PLL
#define DRXK_POWER_DOWN_PLL         DRX_POWER_MODE_10
#endif


enum agc_ctrl_mode {
	DRXK_AGC_CTRL_AUTO = 0,
	DRXK_AGC_CTRL_USER,
	DRXK_AGC_CTRL_OFF
};

enum e_drxk_state {
	DRXK_UNINITIALIZED = 0,
	DRXK_STOPPED,
	DRXK_DTV_STARTED,
	DRXK_ATV_STARTED,
	DRXK_POWERED_DOWN,
	DRXK_NO_DEV			 
};

enum e_drxk_coef_array_index {
	DRXK_COEF_IDX_MN = 0,
	DRXK_COEF_IDX_FM    ,
	DRXK_COEF_IDX_L     ,
	DRXK_COEF_IDX_LP    ,
	DRXK_COEF_IDX_BG    ,
	DRXK_COEF_IDX_DK    ,
	DRXK_COEF_IDX_I     ,
	DRXK_COEF_IDX_MAX
};
enum e_drxk_sif_attenuation {
	DRXK_SIF_ATTENUATION_0DB,
	DRXK_SIF_ATTENUATION_3DB,
	DRXK_SIF_ATTENUATION_6DB,
	DRXK_SIF_ATTENUATION_9DB
};
enum e_drxk_constellation {
	DRX_CONSTELLATION_BPSK = 0,
	DRX_CONSTELLATION_QPSK,
	DRX_CONSTELLATION_PSK8,
	DRX_CONSTELLATION_QAM16,
	DRX_CONSTELLATION_QAM32,
	DRX_CONSTELLATION_QAM64,
	DRX_CONSTELLATION_QAM128,
	DRX_CONSTELLATION_QAM256,
	DRX_CONSTELLATION_QAM512,
	DRX_CONSTELLATION_QAM1024,
	DRX_CONSTELLATION_UNKNOWN = DRX_UNKNOWN,
	DRX_CONSTELLATION_AUTO    = DRX_AUTO
};
enum e_drxk_interleave_mode {
	DRXK_QAM_I12_J17    = 16,
	DRXK_QAM_I_UNKNOWN  = DRX_UNKNOWN
};
enum {
	DRXK_SPIN_A1 = 0,
	DRXK_SPIN_A2,
	DRXK_SPIN_A3,
	DRXK_SPIN_UNKNOWN
};

enum drxk_cfg_dvbt_sqi_speed {
	DRXK_DVBT_SQI_SPEED_FAST = 0,
	DRXK_DVBT_SQI_SPEED_MEDIUM,
	DRXK_DVBT_SQI_SPEED_SLOW,
	DRXK_DVBT_SQI_SPEED_UNKNOWN = DRX_UNKNOWN
} ;

enum drx_fftmode_t {
	DRX_FFTMODE_2K = 0,
	DRX_FFTMODE_4K,
	DRX_FFTMODE_8K,
	DRX_FFTMODE_UNKNOWN = DRX_UNKNOWN,
	DRX_FFTMODE_AUTO    = DRX_AUTO
};

enum drxmpeg_str_width_t {
	DRX_MPEG_STR_WIDTH_1,
	DRX_MPEG_STR_WIDTH_8
};

enum drx_qam_lock_range_t {
	DRX_QAM_LOCKRANGE_NORMAL,
	DRX_QAM_LOCKRANGE_EXTENDED
};

struct drxk_cfg_dvbt_echo_thres_t {
	u16             threshold;
	enum drx_fftmode_t      fft_mode;
} ;

struct s_cfg_agc {
	enum agc_ctrl_mode     ctrl_mode;         
	u16            output_level;      
	u16            min_output_level;   
	u16            max_output_level;   
	u16            speed;            
	u16            top;              
	u16            cut_off_current;    
	u16            ingain_tgt_max;
	u16            fast_clip_ctrl_delay;
};

struct s_cfg_pre_saw {
	u16        reference;  
	bool          use_pre_saw;  
};

struct drxk_ofdm_sc_cmd_t {
	u16 cmd;         
	u16 subcmd;      
	u16 param0;      
	u16 param1;      
	u16 param2;      
	u16 param3;      
	u16 param4;      
};

struct drxk_state {
	struct dvb_frontend frontend;
	struct dtv_frontend_properties props;
	struct device *dev;

	struct i2c_adapter *i2c;
	u8     demod_address;
	void  *priv;

	struct mutex mutex;

	u32    m_instance;            

	int    m_chunk_size;
	u8 chunk[256];

	bool   m_has_lna;
	bool   m_has_dvbt;
	bool   m_has_dvbc;
	bool   m_has_audio;
	bool   m_has_atv;
	bool   m_has_oob;
	bool   m_has_sawsw;          
	bool   m_has_gpio1;          
	bool   m_has_gpio2;          
	bool   m_has_irqn;           
	u16    m_osc_clock_freq;
	u16    m_hi_cfg_timing_div;
	u16    m_hi_cfg_bridge_delay;
	u16    m_hi_cfg_wake_up_key;
	u16    m_hi_cfg_timeout;
	u16    m_hi_cfg_ctrl;
	s32    m_sys_clock_freq;       

	enum e_drxk_state    m_drxk_state;       
	enum operation_mode m_operation_mode;   
	struct s_cfg_agc     m_vsb_rf_agc_cfg;     
	struct s_cfg_agc     m_vsb_if_agc_cfg;     
	u16                m_vsb_pga_cfg;       
	struct s_cfg_pre_saw  m_vsb_pre_saw_cfg;    
	s32    m_Quality83percent;   
	s32    m_Quality93percent;   
	bool   m_smart_ant_inverted;
	bool   m_b_debug_enable_bridge;
	bool   m_b_p_down_open_bridge;   
	bool   m_b_power_down;         

	u32    m_iqm_fs_rate_ofs;       

	bool   m_enable_mpeg_output;   
	bool   m_insert_rs_byte;       
	bool   m_enable_parallel;     
	bool   m_invert_data;         
	bool   m_invert_err;          
	bool   m_invert_str;          
	bool   m_invert_val;          
	bool   m_invert_clk;          
	bool   m_dvbc_static_clk;
	bool   m_dvbt_static_clk;      
	u32    m_dvbt_bitrate;
	u32    m_dvbc_bitrate;

	u8     m_ts_data_strength;
	u8     m_ts_clockk_strength;

	bool   m_itut_annex_c;       

	enum drxmpeg_str_width_t  m_width_str;     
	u32    m_mpeg_ts_static_bitrate;           

	       
	s32    m_mpeg_lock_time_out;       
	s32    m_demod_lock_time_out;      

	bool   m_disable_te_ihandling;

	bool   m_rf_agc_pol;
	bool   m_if_agc_pol;

	struct s_cfg_agc    m_atv_rf_agc_cfg;   
	struct s_cfg_agc    m_atv_if_agc_cfg;   
	struct s_cfg_pre_saw m_atv_pre_saw_cfg;  
	bool              m_phase_correction_bypass;
	s16               m_atv_top_vid_peak;
	u16               m_atv_top_noise_th;
	enum e_drxk_sif_attenuation m_sif_attenuation;
	bool              m_enable_cvbs_output;
	bool              m_enable_sif_output;
	bool              m_b_mirror_freq_spect;
	enum e_drxk_constellation  m_constellation;  
	u32               m_curr_symbol_rate;        
	struct s_cfg_agc    m_qam_rf_agc_cfg;           
	struct s_cfg_agc    m_qam_if_agc_cfg;           
	u16               m_qam_pga_cfg;             
	struct s_cfg_pre_saw m_qam_pre_saw_cfg;          
	enum e_drxk_interleave_mode m_qam_interleave_mode;  
	u16               m_fec_rs_plen;
	u16               m_fec_rs_prescale;

	enum drxk_cfg_dvbt_sqi_speed m_sqi_speed;

	u16               m_gpio;
	u16               m_gpio_cfg;

	struct s_cfg_agc    m_dvbt_rf_agc_cfg;      
	struct s_cfg_agc    m_dvbt_if_agc_cfg;      
	struct s_cfg_pre_saw m_dvbt_pre_saw_cfg;     

	u16               m_agcfast_clip_ctrl_delay;
	bool              m_adc_comp_passed;
	u16               m_adcCompCoef[64];
	u16               m_adc_state;

	u8               *m_microcode;
	int               m_microcode_length;
	bool		  m_drxk_a3_rom_code;
	bool              m_drxk_a3_patch_code;

	bool              m_rfmirror;
	u8                m_device_spin;
	u32               m_iqm_rc_rate;

	enum drx_power_mode m_current_power_mode;

	 
	bool		  drxk_i2c_exclusive_lock;

	 

	u16	uio_mask;	 

	bool	enable_merr_cfg;
	bool	single_master;
	bool	no_i2c_bridge;
	bool	antenna_dvbt;
	u16	antenna_gpio;

	enum fe_status fe_status;

	 
	const char *microcode_name;
	struct completion fw_wait_load;
	const struct firmware *fw;
	int qam_demod_parameter_count;
};

#define NEVER_LOCK 0
#define NOT_LOCKED 1
#define DEMOD_LOCK 2
#define FEC_LOCK   3
#define MPEG_LOCK  4

