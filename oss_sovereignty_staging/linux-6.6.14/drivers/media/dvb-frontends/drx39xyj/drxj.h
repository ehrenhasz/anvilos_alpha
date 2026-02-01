
 

#ifndef __DRXJ_H__
#define __DRXJ_H__
 

#include "drx_driver.h"
#include "drx_dap_fasi.h"

 
 
#if ((DRXDAP_SINGLE_MASTER == 0) && (DRXDAPFASI_LONG_ADDR_ALLOWED == 0))
#error "Multi master mode and short addressing only is an illegal combination"
	*;			 
#endif

 
 
 
 
 
 

 
 
 
 
 

	struct drxjscu_cmd {
		u16 command;
			 
		u16 parameter_len;
			 
		u16 result_len;
			 
		u16 *parameter;
			 
		u16 *result;
			 };

 
 
 
 
 

 
#define DRXJ_DEMOD_LOCK       (DRX_LOCK_STATE_1)

 
#define DRXJ_OOB_AGC_LOCK     (DRX_LOCK_STATE_1)	 
#define DRXJ_OOB_SYNC_LOCK    (DRX_LOCK_STATE_2)	 

 
#define DRXJ_POWER_DOWN_MAIN_PATH   DRX_POWER_MODE_8
#define DRXJ_POWER_DOWN_CORE        DRX_POWER_MODE_9
#define DRXJ_POWER_DOWN_PLL         DRX_POWER_MODE_10

 
#define APP_O                 (0x0000)

 

#define DRXJ_CTRL_CFG_BASE    (0x1000)
	enum drxj_cfg_type {
		DRXJ_CFG_AGC_RF = DRXJ_CTRL_CFG_BASE,
		DRXJ_CFG_AGC_IF,
		DRXJ_CFG_AGC_INTERNAL,
		DRXJ_CFG_PRE_SAW,
		DRXJ_CFG_AFE_GAIN,
		DRXJ_CFG_SYMBOL_CLK_OFFSET,
		DRXJ_CFG_ACCUM_CR_RS_CW_ERR,
		DRXJ_CFG_FEC_MERS_SEQ_COUNT,
		DRXJ_CFG_OOB_MISC,
		DRXJ_CFG_SMART_ANT,
		DRXJ_CFG_OOB_PRE_SAW,
		DRXJ_CFG_VSB_MISC,
		DRXJ_CFG_RESET_PACKET_ERR,

		 
		DRXJ_CFG_ATV_OUTPUT,	 
		DRXJ_CFG_ATV_MISC,
		DRXJ_CFG_ATV_EQU_COEF,
		DRXJ_CFG_ATV_AGC_STATUS,	 

		DRXJ_CFG_MPEG_OUTPUT_MISC,
		DRXJ_CFG_HW_CFG,
		DRXJ_CFG_OOB_LO_POW,

		DRXJ_CFG_MAX	 };

 
enum drxj_cfg_smart_ant_io {
	DRXJ_SMT_ANT_OUTPUT = 0,
	DRXJ_SMT_ANT_INPUT
};

 
	struct drxj_cfg_smart_ant {
		enum drxj_cfg_smart_ant_io io;
		u16 ctrl_data;
	};

 
struct drxj_agc_status {
	u16 IFAGC;
	u16 RFAGC;
	u16 digital_agc;
};

 

 
	enum drxj_agc_ctrl_mode {
		DRX_AGC_CTRL_AUTO = 0,
		DRX_AGC_CTRL_USER,
		DRX_AGC_CTRL_OFF};

 
	struct drxj_cfg_agc {
		enum drx_standard standard;	 
		enum drxj_agc_ctrl_mode ctrl_mode;	 
		u16 output_level;	 
		u16 min_output_level;	 
		u16 max_output_level;	 
		u16 speed;	 
		u16 top;	 
		u16 cut_off_current;	 };

 

 
	struct drxj_cfg_pre_saw {
		enum drx_standard standard;	 
		u16 reference;	 
		bool use_pre_saw;	 };

 

 
	struct drxj_cfg_afe_gain {
		enum drx_standard standard;	 
		u16 gain;	 };

 
	struct drxjrs_errors {
		u16 nr_bit_errors;
				 
		u16 nr_symbol_errors;
				 
		u16 nr_packet_errors;
				 
		u16 nr_failures;
				 
		u16 nr_snc_par_fail_count;
				 
	};

 
	struct drxj_cfg_vsb_misc {
		u32 symb_error;
			       };

 
	enum drxj_mpeg_start_width {
		DRXJ_MPEG_START_WIDTH_1CLKCYC,
		DRXJ_MPEG_START_WIDTH_8CLKCYC};

 
	enum drxj_mpeg_output_clock_rate {
		DRXJ_MPEGOUTPUT_CLOCK_RATE_AUTO,
		DRXJ_MPEGOUTPUT_CLOCK_RATE_75973K,
		DRXJ_MPEGOUTPUT_CLOCK_RATE_50625K,
		DRXJ_MPEGOUTPUT_CLOCK_RATE_37968K,
		DRXJ_MPEGOUTPUT_CLOCK_RATE_30375K,
		DRXJ_MPEGOUTPUT_CLOCK_RATE_25313K,
		DRXJ_MPEGOUTPUT_CLOCK_RATE_21696K};

 
	struct drxj_cfg_mpeg_output_misc {
		bool disable_tei_handling;	       
		bool bit_reverse_mpeg_outout;	       
		enum drxj_mpeg_output_clock_rate mpeg_output_clock_rate;
						       
		enum drxj_mpeg_start_width mpeg_start_width;   };

 
	enum drxj_xtal_freq {
		DRXJ_XTAL_FREQ_RSVD,
		DRXJ_XTAL_FREQ_27MHZ,
		DRXJ_XTAL_FREQ_20P25MHZ,
		DRXJ_XTAL_FREQ_4MHZ};

 
	enum drxji2c_speed {
		DRXJ_I2C_SPEED_400KBPS,
		DRXJ_I2C_SPEED_100KBPS};

 
	struct drxj_cfg_hw_cfg {
		enum drxj_xtal_freq xtal_freq;
				    
		enum drxji2c_speed i2c_speed;
				    };

 
	struct drxj_cfg_atv_misc {
		s16 peak_filter;	 
		u16 noise_filter;	 };

 
#define   DRXJ_OOB_STATE_RESET                                        0x0
#define   DRXJ_OOB_STATE_AGN_HUNT                                     0x1
#define   DRXJ_OOB_STATE_DGN_HUNT                                     0x2
#define   DRXJ_OOB_STATE_AGC_HUNT                                     0x3
#define   DRXJ_OOB_STATE_FRQ_HUNT                                     0x4
#define   DRXJ_OOB_STATE_PHA_HUNT                                     0x8
#define   DRXJ_OOB_STATE_TIM_HUNT                                     0x10
#define   DRXJ_OOB_STATE_EQU_HUNT                                     0x20
#define   DRXJ_OOB_STATE_EQT_HUNT                                     0x30
#define   DRXJ_OOB_STATE_SYNC                                         0x40

struct drxj_cfg_oob_misc {
	struct drxj_agc_status agc;
	bool eq_lock;
	bool sym_timing_lock;
	bool phase_lock;
	bool freq_lock;
	bool dig_gain_lock;
	bool ana_gain_lock;
	u8 state;
};

 
	enum drxj_cfg_oob_lo_power {
		DRXJ_OOB_LO_POW_MINUS0DB = 0,
		DRXJ_OOB_LO_POW_MINUS5DB,
		DRXJ_OOB_LO_POW_MINUS10DB,
		DRXJ_OOB_LO_POW_MINUS15DB,
		DRXJ_OOB_LO_POW_MAX};

 
	struct drxj_cfg_atv_equ_coef {
		s16 coef0;	 
		s16 coef1;	 
		s16 coef2;	 
		s16 coef3;	 };

 
	enum drxj_coef_array_index {
		DRXJ_COEF_IDX_MN = 0,
		DRXJ_COEF_IDX_FM,
		DRXJ_COEF_IDX_L,
		DRXJ_COEF_IDX_LP,
		DRXJ_COEF_IDX_BG,
		DRXJ_COEF_IDX_DK,
		DRXJ_COEF_IDX_I,
		DRXJ_COEF_IDX_MAX};

 

 
	enum drxjsif_attenuation {
		DRXJ_SIF_ATTENUATION_0DB,
		DRXJ_SIF_ATTENUATION_3DB,
		DRXJ_SIF_ATTENUATION_6DB,
		DRXJ_SIF_ATTENUATION_9DB};

 
struct drxj_cfg_atv_output {
	bool enable_cvbs_output;	 
	bool enable_sif_output;	 
	enum drxjsif_attenuation sif_attenuation;
};

 
 
	struct drxj_cfg_atv_agc_status {
		u16 rf_agc_gain;	 
		u16 if_agc_gain;	 
		s16 video_agc_gain;	 
		s16 audio_agc_gain;	 
		u16 rf_agc_loop_gain;	 
		u16 if_agc_loop_gain;	 
		u16 video_agc_loop_gain;	 };

 
 
 
 
 

 

 
 

 
 
	struct drxj_data {
		 
		bool has_lna;		   
		bool has_oob;		   
		bool has_ntsc;		   
		bool has_btsc;		   
		bool has_smatx;	   
		bool has_smarx;	   
		bool has_gpio;		   
		bool has_irqn;		   
		 
		u8 mfx;		   

		 
		bool mirror_freq_spect_oob; 

		 
		enum drx_standard standard;	   
		enum drx_modulation constellation;
					   
		s32 frequency;  
		enum drx_bandwidth curr_bandwidth;
					   
		enum drx_mirror mirror;	   

		 
		u32 fec_bits_desired;	   
		u16 fec_vd_plen;	   
		u16 qam_vd_prescale;	   
		u16 qam_vd_period;	   
		u16 fec_rs_plen;	   
		u16 fec_rs_prescale;	   
		u16 fec_rs_period;	   
		bool reset_pkt_err_acc;	   
		u16 pkt_err_acc_start;	   

		 
		u16 hi_cfg_timing_div;	   
		u16 hi_cfg_bridge_delay;	   
		u16 hi_cfg_wake_up_key;	   
		u16 hi_cfg_ctrl;	   
		u16 hi_cfg_transmit;	   

		 
		enum drxuio_mode uio_sma_rx_mode; 
		enum drxuio_mode uio_sma_tx_mode; 
		enum drxuio_mode uio_gpio_mode;  
		enum drxuio_mode uio_irqn_mode;  

		 
		u32 iqm_fs_rate_ofs;	    
		bool pos_image;	    
		 
		u32 iqm_rc_rate_ofs;	    

		 
		u32 atv_cfg_changed_flags;  
		s16 atv_top_equ0[DRXJ_COEF_IDX_MAX];	      
		s16 atv_top_equ1[DRXJ_COEF_IDX_MAX];	      
		s16 atv_top_equ2[DRXJ_COEF_IDX_MAX];	      
		s16 atv_top_equ3[DRXJ_COEF_IDX_MAX];	      
		bool phase_correction_bypass; 
		s16 atv_top_vid_peak;	   
		u16 atv_top_noise_th;	   
		bool enable_cvbs_output;   
		bool enable_sif_output;	   
		 enum drxjsif_attenuation sif_attenuation;
					   
		 
		struct drxj_cfg_agc qam_rf_agc_cfg;  
		struct drxj_cfg_agc qam_if_agc_cfg;  
		struct drxj_cfg_agc vsb_rf_agc_cfg;  
		struct drxj_cfg_agc vsb_if_agc_cfg;  

		 
		u16 qam_pga_cfg;	   
		u16 vsb_pga_cfg;	   

		 
		struct drxj_cfg_pre_saw qam_pre_saw_cfg;
					   
		struct drxj_cfg_pre_saw vsb_pre_saw_cfg;
					   

		 
		char v_text[2][12];	   
		struct drx_version v_version[2];  
		struct drx_version_list v_list_elements[2];
					   

		 
		bool smart_ant_inverted;

		 
		u16 oob_trk_filter_cfg[8];
		bool oob_power_on;

		 
		u32 mpeg_ts_static_bitrate;   
		bool disable_te_ihandling;   
		bool bit_reverse_mpeg_outout; 
		 enum drxj_mpeg_output_clock_rate mpeg_output_clock_rate;
					     
		 enum drxj_mpeg_start_width mpeg_start_width;
					     

		 
		struct drxj_cfg_pre_saw atv_pre_saw_cfg;
					   
		struct drxj_cfg_agc atv_rf_agc_cfg;  
		struct drxj_cfg_agc atv_if_agc_cfg;  
		u16 atv_pga_cfg;	   

		u32 curr_symbol_rate;

		 
		bool pdr_safe_mode;	     
		u16 pdr_safe_restore_val_gpio;
		u16 pdr_safe_restore_val_v_sync;
		u16 pdr_safe_restore_val_sma_rx;
		u16 pdr_safe_restore_val_sma_tx;

		 
		u16 oob_pre_saw;
		enum drxj_cfg_oob_lo_power oob_lo_pow;

		struct drx_aud_data aud_data;
				     };

 
 

#define DRXJ_ATTR_BTSC_DETECT(d)                       \
			(((struct drxj_data *)(d)->my_ext_attr)->aud_data.btsc_detect)

 

 
#define DRXJ_NTSC_CARRIER_FREQ_OFFSET           ((s32)(1750))

 
#define DRXJ_PAL_SECAM_BG_CARRIER_FREQ_OFFSET   ((s32)(2375))

 
#define DRXJ_PAL_SECAM_DKIL_CARRIER_FREQ_OFFSET ((s32)(2775))

 
#define DRXJ_PAL_SECAM_LP_CARRIER_FREQ_OFFSET   ((s32)(-3255))

 
#define DRXJ_FM_CARRIER_FREQ_OFFSET             ((s32)(-3000))

 

#define DRXJ_TYPE_ID (0x3946000DUL)

 

 
#define DRXJ_STR_OOB_LOCKSTATUS(x) ( \
	(x == DRX_NEVER_LOCK) ? "Never" : \
	(x == DRX_NOT_LOCKED) ? "No" : \
	(x == DRX_LOCKED) ? "Locked" : \
	(x == DRX_LOCK_STATE_1) ? "AGC lock" : \
	(x == DRX_LOCK_STATE_2) ? "sync lock" : \
	"(Invalid)")

#endif				 
