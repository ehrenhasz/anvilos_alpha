 

#ifndef __DRXDRIVER_H__
#define __DRXDRIVER_H__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/i2c.h>

 
struct i2c_device_addr {
	u16 i2c_addr;		 
	u16 i2c_dev_id;		 
	void *user_data;		 
};

 
#define IS_I2C_10BIT(addr) \
	 (((addr) & 0xF8) == 0xF0)

 

 
int drxbsp_i2c_init(void);

 
int drxbsp_i2c_term(void);

 
int drxbsp_i2c_write_read(struct i2c_device_addr *w_dev_addr,
					u16 w_count,
					u8 *wData,
					struct i2c_device_addr *r_dev_addr,
					u16 r_count, u8 *r_data);

 
char *drxbsp_i2c_error_text(void);

 
extern int drx_i2c_error_g;

#define TUNER_MODE_SUB0    0x0001	 
#define TUNER_MODE_SUB1    0x0002	 
#define TUNER_MODE_SUB2    0x0004	 
#define TUNER_MODE_SUB3    0x0008	 
#define TUNER_MODE_SUB4    0x0010	 
#define TUNER_MODE_SUB5    0x0020	 
#define TUNER_MODE_SUB6    0x0040	 
#define TUNER_MODE_SUB7    0x0080	 

#define TUNER_MODE_DIGITAL 0x0100	 
#define TUNER_MODE_ANALOG  0x0200	 
#define TUNER_MODE_SWITCH  0x0400	 
#define TUNER_MODE_LOCK    0x0800	 
#define TUNER_MODE_6MHZ    0x1000	 
#define TUNER_MODE_7MHZ    0x2000	 
#define TUNER_MODE_8MHZ    0x4000	 

#define TUNER_MODE_SUB_MAX 8
#define TUNER_MODE_SUBALL  (TUNER_MODE_SUB0 | TUNER_MODE_SUB1 | \
			      TUNER_MODE_SUB2 | TUNER_MODE_SUB3 | \
			      TUNER_MODE_SUB4 | TUNER_MODE_SUB5 | \
			      TUNER_MODE_SUB6 | TUNER_MODE_SUB7)


enum tuner_lock_status {
	TUNER_LOCKED,
	TUNER_NOT_LOCKED
};

struct tuner_common {
	char *name;	 
	s32 min_freq_rf;	 
	s32 max_freq_rf;	 

	u8 sub_mode;	 
	char ***sub_mode_descriptions;	 
	u8 sub_modes;	 

	 
	void *self_check;	 
	bool programmed;	 
	s32 r_ffrequency;	 
	s32 i_ffrequency;	 

	void *my_user_data;	 
	u16 my_capabilities;	 
};

struct tuner_instance;

typedef int(*tuner_open_func_t) (struct tuner_instance *tuner);
typedef int(*tuner_close_func_t) (struct tuner_instance *tuner);

typedef int(*tuner_set_frequency_func_t) (struct tuner_instance *tuner,
						u32 mode,
						s32
						frequency);

typedef int(*tuner_get_frequency_func_t) (struct tuner_instance *tuner,
						u32 mode,
						s32 *
						r_ffrequency,
						s32 *
						i_ffrequency);

typedef int(*tuner_lock_status_func_t) (struct tuner_instance *tuner,
						enum tuner_lock_status *
						lock_stat);

typedef int(*tune_ri2c_write_read_func_t) (struct tuner_instance *tuner,
						struct i2c_device_addr *
						w_dev_addr, u16 w_count,
						u8 *wData,
						struct i2c_device_addr *
						r_dev_addr, u16 r_count,
						u8 *r_data);

struct tuner_ops {
	tuner_open_func_t open_func;
	tuner_close_func_t close_func;
	tuner_set_frequency_func_t set_frequency_func;
	tuner_get_frequency_func_t get_frequency_func;
	tuner_lock_status_func_t lock_status_func;
	tune_ri2c_write_read_func_t i2c_write_read_func;

};

struct tuner_instance {
	struct i2c_device_addr my_i2c_dev_addr;
	struct tuner_common *my_common_attr;
	void *my_ext_attr;
	struct tuner_ops *my_funct;
};

int drxbsp_tuner_set_frequency(struct tuner_instance *tuner,
					u32 mode,
					s32 frequency);

int drxbsp_tuner_get_frequency(struct tuner_instance *tuner,
					u32 mode,
					s32 *r_ffrequency,
					s32 *i_ffrequency);

int drxbsp_tuner_default_i2c_write_read(struct tuner_instance *tuner,
						struct i2c_device_addr *w_dev_addr,
						u16 w_count,
						u8 *wData,
						struct i2c_device_addr *r_dev_addr,
						u16 r_count, u8 *r_data);

 

 
#ifndef DRXDAP_SINGLE_MASTER
#define DRXDAP_SINGLE_MASTER 1
#endif

 
#ifndef DRXDAP_MAX_WCHUNKSIZE
#define  DRXDAP_MAX_WCHUNKSIZE 60
#endif

 
#ifndef DRXDAP_MAX_RCHUNKSIZE
#define  DRXDAP_MAX_RCHUNKSIZE 60
#endif

 

 
#ifndef DRX_UNKNOWN
#define DRX_UNKNOWN (254)
#endif

 
#ifndef DRX_AUTO
#define DRX_AUTO    (255)
#endif

 

 
#define DRX_CAPABILITY_HAS_LNA           (1UL <<  0)
 
#define DRX_CAPABILITY_HAS_OOBRX         (1UL <<  1)
 
#define DRX_CAPABILITY_HAS_ATV           (1UL <<  2)
 
#define DRX_CAPABILITY_HAS_DVBT          (1UL <<  3)
 
#define DRX_CAPABILITY_HAS_ITUB          (1UL <<  4)
 
#define DRX_CAPABILITY_HAS_AUD           (1UL <<  5)
 
#define DRX_CAPABILITY_HAS_SAWSW         (1UL <<  6)
 
#define DRX_CAPABILITY_HAS_GPIO1         (1UL <<  7)
 
#define DRX_CAPABILITY_HAS_GPIO2         (1UL <<  8)
 
#define DRX_CAPABILITY_HAS_IRQN          (1UL <<  9)
 
#define DRX_CAPABILITY_HAS_8VSB          (1UL << 10)
 
#define DRX_CAPABILITY_HAS_SMATX         (1UL << 11)
 
#define DRX_CAPABILITY_HAS_SMARX         (1UL << 12)
 
#define DRX_CAPABILITY_HAS_ITUAC         (1UL << 13)

 
 
#define DRX_VERSIONSTRING(MAJOR, MINOR, PATCH) \
	 DRX_VERSIONSTRING_HELP(MAJOR)"." \
	 DRX_VERSIONSTRING_HELP(MINOR)"." \
	 DRX_VERSIONSTRING_HELP(PATCH)
#define DRX_VERSIONSTRING_HELP(NUM) #NUM

 
#define DRX_16TO8(x) ((u8) (((u16)x) & 0xFF)), \
			((u8)((((u16)x)>>8)&0xFF))

 
#define DRX_U16TODRXFREQ(x)   ((x & 0x8000) ? \
				 ((s32) \
				    (((u32) x) | 0xFFFF0000)) : \
				 ((s32) x))

 

 
enum drx_standard {
	DRX_STANDARD_DVBT = 0,  
	DRX_STANDARD_8VSB,      
	DRX_STANDARD_NTSC,      
	DRX_STANDARD_PAL_SECAM_BG,
				 
	DRX_STANDARD_PAL_SECAM_DK,
				 
	DRX_STANDARD_PAL_SECAM_I,
				 
	DRX_STANDARD_PAL_SECAM_L,
				 
	DRX_STANDARD_PAL_SECAM_LP,
				 
	DRX_STANDARD_ITU_A,     
	DRX_STANDARD_ITU_B,     
	DRX_STANDARD_ITU_C,     
	DRX_STANDARD_ITU_D,     
	DRX_STANDARD_FM,        
	DRX_STANDARD_DTMB,      
	DRX_STANDARD_UNKNOWN = DRX_UNKNOWN,
				 
	DRX_STANDARD_AUTO = DRX_AUTO
				 
};

 
enum drx_substandard {
	DRX_SUBSTANDARD_MAIN = 0,  
	DRX_SUBSTANDARD_ATV_BG_SCANDINAVIA,
	DRX_SUBSTANDARD_ATV_DK_POLAND,
	DRX_SUBSTANDARD_ATV_DK_CHINA,
	DRX_SUBSTANDARD_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_SUBSTANDARD_AUTO = DRX_AUTO
					 
};

 
enum drx_bandwidth {
	DRX_BANDWIDTH_8MHZ = 0,	  
	DRX_BANDWIDTH_7MHZ,	  
	DRX_BANDWIDTH_6MHZ,	  
	DRX_BANDWIDTH_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_BANDWIDTH_AUTO = DRX_AUTO
					 
};

 
enum drx_mirror {
	DRX_MIRROR_NO = 0,    
	DRX_MIRROR_YES,	      
	DRX_MIRROR_UNKNOWN = DRX_UNKNOWN,
				 
	DRX_MIRROR_AUTO = DRX_AUTO
				 
};

 
enum drx_modulation {
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
	DRX_CONSTELLATION_QPSK_NR,    
	DRX_CONSTELLATION_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_CONSTELLATION_AUTO = DRX_AUTO
					 
};

 
enum drx_hierarchy {
	DRX_HIERARCHY_NONE = 0,	 
	DRX_HIERARCHY_ALPHA1,	 
	DRX_HIERARCHY_ALPHA2,	 
	DRX_HIERARCHY_ALPHA4,	 
	DRX_HIERARCHY_UNKNOWN = DRX_UNKNOWN,
				 
	DRX_HIERARCHY_AUTO = DRX_AUTO
				 
};

 
enum drx_priority {
	DRX_PRIORITY_LOW = 0,   
	DRX_PRIORITY_HIGH,      
	DRX_PRIORITY_UNKNOWN = DRX_UNKNOWN
				 
};

 
enum drx_coderate {
		DRX_CODERATE_1DIV2 = 0,	 
		DRX_CODERATE_2DIV3,	 
		DRX_CODERATE_3DIV4,	 
		DRX_CODERATE_5DIV6,	 
		DRX_CODERATE_7DIV8,	 
		DRX_CODERATE_UNKNOWN = DRX_UNKNOWN,
					 
		DRX_CODERATE_AUTO = DRX_AUTO
					 
};

 
enum drx_guard {
	DRX_GUARD_1DIV32 = 0,  
	DRX_GUARD_1DIV16,      
	DRX_GUARD_1DIV8,       
	DRX_GUARD_1DIV4,       
	DRX_GUARD_UNKNOWN = DRX_UNKNOWN,
				 
	DRX_GUARD_AUTO = DRX_AUTO
				 
};

 
enum drx_fft_mode {
	DRX_FFTMODE_2K = 0,     
	DRX_FFTMODE_4K,	        
	DRX_FFTMODE_8K,	        
	DRX_FFTMODE_UNKNOWN = DRX_UNKNOWN,
				 
	DRX_FFTMODE_AUTO = DRX_AUTO
				 
};

 
enum drx_classification {
	DRX_CLASSIFICATION_GAUSS = 0,  
	DRX_CLASSIFICATION_HVY_GAUSS,  
	DRX_CLASSIFICATION_COCHANNEL,  
	DRX_CLASSIFICATION_STATIC,     
	DRX_CLASSIFICATION_MOVING,     
	DRX_CLASSIFICATION_ZERODB,     
	DRX_CLASSIFICATION_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_CLASSIFICATION_AUTO = DRX_AUTO
					 
};

 
enum drx_interleave_mode {
	DRX_INTERLEAVEMODE_I128_J1 = 0,
	DRX_INTERLEAVEMODE_I128_J1_V2,
	DRX_INTERLEAVEMODE_I128_J2,
	DRX_INTERLEAVEMODE_I64_J2,
	DRX_INTERLEAVEMODE_I128_J3,
	DRX_INTERLEAVEMODE_I32_J4,
	DRX_INTERLEAVEMODE_I128_J4,
	DRX_INTERLEAVEMODE_I16_J8,
	DRX_INTERLEAVEMODE_I128_J5,
	DRX_INTERLEAVEMODE_I8_J16,
	DRX_INTERLEAVEMODE_I128_J6,
	DRX_INTERLEAVEMODE_RESERVED_11,
	DRX_INTERLEAVEMODE_I128_J7,
	DRX_INTERLEAVEMODE_RESERVED_13,
	DRX_INTERLEAVEMODE_I128_J8,
	DRX_INTERLEAVEMODE_RESERVED_15,
	DRX_INTERLEAVEMODE_I12_J17,
	DRX_INTERLEAVEMODE_I5_J4,
	DRX_INTERLEAVEMODE_B52_M240,
	DRX_INTERLEAVEMODE_B52_M720,
	DRX_INTERLEAVEMODE_B52_M48,
	DRX_INTERLEAVEMODE_B52_M0,
	DRX_INTERLEAVEMODE_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_INTERLEAVEMODE_AUTO = DRX_AUTO
					 
};

 
enum drx_carrier_mode {
	DRX_CARRIER_MULTI = 0,		 
	DRX_CARRIER_SINGLE,		 
	DRX_CARRIER_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_CARRIER_AUTO = DRX_AUTO	 
};

 
enum drx_frame_mode {
	DRX_FRAMEMODE_420 = 0,	  
	DRX_FRAMEMODE_595,	  
	DRX_FRAMEMODE_945,	  
	DRX_FRAMEMODE_420_FIXED_PN,
					 
	DRX_FRAMEMODE_945_FIXED_PN,
					 
	DRX_FRAMEMODE_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_FRAMEMODE_AUTO = DRX_AUTO
					 
};

 
enum drx_tps_frame {
	DRX_TPS_FRAME1 = 0,	   
	DRX_TPS_FRAME2,		   
	DRX_TPS_FRAME3,		   
	DRX_TPS_FRAME4,		   
	DRX_TPS_FRAME_UNKNOWN = DRX_UNKNOWN
					 
};

 
enum drx_ldpc {
	DRX_LDPC_0_4 = 0,	   
	DRX_LDPC_0_6,		   
	DRX_LDPC_0_8,		   
	DRX_LDPC_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_LDPC_AUTO = DRX_AUTO   
};

 
enum drx_pilot_mode {
	DRX_PILOT_ON = 0,	   
	DRX_PILOT_OFF,		   
	DRX_PILOT_UNKNOWN = DRX_UNKNOWN,
					 
	DRX_PILOT_AUTO = DRX_AUTO  
};

 
enum drxu_code_action {
	UCODE_UPLOAD,
	UCODE_VERIFY
};

 

enum drx_lock_status {
	DRX_NEVER_LOCK = 0,
	DRX_NOT_LOCKED,
	DRX_LOCK_STATE_1,
	DRX_LOCK_STATE_2,
	DRX_LOCK_STATE_3,
	DRX_LOCK_STATE_4,
	DRX_LOCK_STATE_5,
	DRX_LOCK_STATE_6,
	DRX_LOCK_STATE_7,
	DRX_LOCK_STATE_8,
	DRX_LOCK_STATE_9,
	DRX_LOCKED
};

 
enum drx_uio {
	DRX_UIO1,
	DRX_UIO2,
	DRX_UIO3,
	DRX_UIO4,
	DRX_UIO5,
	DRX_UIO6,
	DRX_UIO7,
	DRX_UIO8,
	DRX_UIO9,
	DRX_UIO10,
	DRX_UIO11,
	DRX_UIO12,
	DRX_UIO13,
	DRX_UIO14,
	DRX_UIO15,
	DRX_UIO16,
	DRX_UIO17,
	DRX_UIO18,
	DRX_UIO19,
	DRX_UIO20,
	DRX_UIO21,
	DRX_UIO22,
	DRX_UIO23,
	DRX_UIO24,
	DRX_UIO25,
	DRX_UIO26,
	DRX_UIO27,
	DRX_UIO28,
	DRX_UIO29,
	DRX_UIO30,
	DRX_UIO31,
	DRX_UIO32,
	DRX_UIO_MAX = DRX_UIO32
};

 
enum drxuio_mode {
	DRX_UIO_MODE_DISABLE = 0x01,
			     
	DRX_UIO_MODE_READWRITE = 0x02,
			     
	DRX_UIO_MODE_FIRMWARE = 0x04,
			     
	DRX_UIO_MODE_FIRMWARE0 = DRX_UIO_MODE_FIRMWARE,
					     
	DRX_UIO_MODE_FIRMWARE1 = 0x08,
			     
	DRX_UIO_MODE_FIRMWARE2 = 0x10,
			     
	DRX_UIO_MODE_FIRMWARE3 = 0x20,
			     
	DRX_UIO_MODE_FIRMWARE4 = 0x40,
			     
	DRX_UIO_MODE_FIRMWARE5 = 0x80
			     
};

 
enum drxoob_downstream_standard {
	DRX_OOB_MODE_A = 0,
		        
	DRX_OOB_MODE_B_GRADE_A,
		        
	DRX_OOB_MODE_B_GRADE_B
		        
};

 

 
 
 
 
 

#ifndef DRX_CFG_BASE
#define DRX_CFG_BASE          0
#endif

#define DRX_CFG_MPEG_OUTPUT         (DRX_CFG_BASE +  0)	 
#define DRX_CFG_PKTERR              (DRX_CFG_BASE +  1)	 
#define DRX_CFG_SYMCLK_OFFS         (DRX_CFG_BASE +  2)	 
#define DRX_CFG_SMA                 (DRX_CFG_BASE +  3)	 
#define DRX_CFG_PINSAFE             (DRX_CFG_BASE +  4)	 
#define DRX_CFG_SUBSTANDARD         (DRX_CFG_BASE +  5)	 
#define DRX_CFG_AUD_VOLUME          (DRX_CFG_BASE +  6)	 
#define DRX_CFG_AUD_RDS             (DRX_CFG_BASE +  7)	 
#define DRX_CFG_AUD_AUTOSOUND       (DRX_CFG_BASE +  8)	 
#define DRX_CFG_AUD_ASS_THRES       (DRX_CFG_BASE +  9)	 
#define DRX_CFG_AUD_DEVIATION       (DRX_CFG_BASE + 10)	 
#define DRX_CFG_AUD_PRESCALE        (DRX_CFG_BASE + 11)	 
#define DRX_CFG_AUD_MIXER           (DRX_CFG_BASE + 12)	 
#define DRX_CFG_AUD_AVSYNC          (DRX_CFG_BASE + 13)	 
#define DRX_CFG_AUD_CARRIER         (DRX_CFG_BASE + 14)	 
#define DRX_CFG_I2S_OUTPUT          (DRX_CFG_BASE + 15)	 
#define DRX_CFG_ATV_STANDARD        (DRX_CFG_BASE + 16)	 
#define DRX_CFG_SQI_SPEED           (DRX_CFG_BASE + 17)	 
#define DRX_CTRL_CFG_MAX            (DRX_CFG_BASE + 18)	 

#define DRX_CFG_PINS_SAFE_MODE      DRX_CFG_PINSAFE
 
 
 
 
 

 
struct drxu_code_info {
	char			*mc_file;
};

 
#define AUX_VER_RECORD 0x8000

struct drx_mc_version_rec {
	u16 aux_type;	 
	u32 mc_dev_type;	 
	u32 mc_version;	 
	u32 mc_base_version;	 
};

 

 
struct drx_filter_info {
	u8 *data_re;
	       
	u8 *data_im;
	       
	u16 size_re;
	       
	u16 size_im;
	       
};

 

 
struct drx_channel {
	s32 frequency;
				 
	enum drx_bandwidth bandwidth;
				 
	enum drx_mirror mirror;	 
	enum drx_modulation constellation;
				 
	enum drx_hierarchy hierarchy;
				 
	enum drx_priority priority;	 
	enum drx_coderate coderate;	 
	enum drx_guard guard;	 
	enum drx_fft_mode fftmode;	 
	enum drx_classification classification;
				 
	u32 symbolrate;
				 
	enum drx_interleave_mode interleavemode;
				 
	enum drx_ldpc ldpc;		 
	enum drx_carrier_mode carrier;	 
	enum drx_frame_mode framemode;
				 
	enum drx_pilot_mode pilot;	 
};

 

enum drx_cfg_sqi_speed {
	DRX_SQI_SPEED_FAST = 0,
	DRX_SQI_SPEED_MEDIUM,
	DRX_SQI_SPEED_SLOW,
	DRX_SQI_SPEED_UNKNOWN = DRX_UNKNOWN
};

 

 
struct drx_complex {
	s16 im;
      
	s16 re;
      
};

 

 
struct drx_frequency_plan {
	s32 first;
		      
	s32 last;
		      
	s32 step;
		      
	enum drx_bandwidth bandwidth;
		      
	u16 ch_number;
		      
	char **ch_names;
		      
};

 

 
struct drx_scan_param {
	struct drx_frequency_plan *frequency_plan;
				   
	u16 frequency_plan_size;   
	u32 num_tries;		   
	s32 skip;	   
	void *ext_params;	   
};

 

 
enum drx_scan_command {
		DRX_SCAN_COMMAND_INIT = 0, 
		DRX_SCAN_COMMAND_NEXT,	   
		DRX_SCAN_COMMAND_STOP	   
};

 

 
typedef int(*drx_scan_func_t) (void *scan_context,
				     enum drx_scan_command scan_command,
				     struct drx_channel *scan_channel,
				     bool *get_next_channel);

 

 
	struct drxtps_info {
		enum drx_fft_mode fftmode;	 
		enum drx_guard guard;	 
		enum drx_modulation constellation;
					 
		enum drx_hierarchy hierarchy;
					 
		enum drx_coderate high_coderate;
					 
		enum drx_coderate low_coderate;
					 
		enum drx_tps_frame frame;	 
		u8 length;		 
		u16 cell_id;		 
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

 

 
	enum drx_module {
		DRX_MODULE_DEVICE,
		DRX_MODULE_MICROCODE,
		DRX_MODULE_DRIVERCORE,
		DRX_MODULE_DEVICEDRIVER,
		DRX_MODULE_DAP,
		DRX_MODULE_BSP_I2C,
		DRX_MODULE_BSP_TUNER,
		DRX_MODULE_BSP_HOST,
		DRX_MODULE_UNKNOWN
	};

 
	struct drx_version {
		enum drx_module module_type;
			        
		char *module_name;
			        
		u16 v_major;   
		u16 v_minor;   
		u16 v_patch;   
		char *v_string;  
	};

 
struct drx_version_list {
	struct drx_version *version; 
	struct drx_version_list *next;
			       
};

 

 
	struct drxuio_cfg {
		enum drx_uio uio;
		        
		enum drxuio_mode mode;
		        
	};

 

 
	struct drxuio_data {
		enum drx_uio uio;
		    
		bool value;
		    
	};

 

 
	struct drxoob {
		s32 frequency;	    
		enum drxoob_downstream_standard standard;
						    
		bool spectrum_inverted;	    
	};

 

 
	struct drxoob_status {
		s32 frequency;  
		enum drx_lock_status lock;	   
		u32 mer;		   
		s32 symbol_rate_offset;	   
	};

 

 
	struct drx_cfg {
		u32 cfg_type;
			   
		void *cfg_data;
			   
	};

 

 

	enum drxmpeg_str_width {
		DRX_MPEG_STR_WIDTH_1,
		DRX_MPEG_STR_WIDTH_8
	};

 
 

	struct drx_cfg_mpeg_output {
		bool enable_mpeg_output; 
		bool insert_rs_byte;	 
		bool enable_parallel;	 
		bool invert_data;	 
		bool invert_err;	 
		bool invert_str;	 
		bool invert_val;	 
		bool invert_clk;	 
		bool static_clk;	 
		u32 bitrate;		 
		enum drxmpeg_str_width width_str;
					 
	};


 

 
	struct drxi2c_data {
		u16 port_nr;	 
		struct i2c_device_addr *w_dev_addr;
				 
		u16 w_count;	 
		u8 *wData;	 
		struct i2c_device_addr *r_dev_addr;
				 
		u16 r_count;	 
		u8 *r_data;	 
	};

 

 
	enum drx_aud_standard {
		DRX_AUD_STANDARD_BTSC,	    
		DRX_AUD_STANDARD_A2,	    
		DRX_AUD_STANDARD_EIAJ,	    
		DRX_AUD_STANDARD_FM_STEREO, 
		DRX_AUD_STANDARD_M_MONO,    
		DRX_AUD_STANDARD_D_K_MONO,  
		DRX_AUD_STANDARD_BG_FM,	    
		DRX_AUD_STANDARD_D_K1,	    
		DRX_AUD_STANDARD_D_K2,	    
		DRX_AUD_STANDARD_D_K3,	    
		DRX_AUD_STANDARD_BG_NICAM_FM,
					    
		DRX_AUD_STANDARD_L_NICAM_AM,
					    
		DRX_AUD_STANDARD_I_NICAM_FM,
					    
		DRX_AUD_STANDARD_D_K_NICAM_FM,
					    
		DRX_AUD_STANDARD_NOT_READY, 
		DRX_AUD_STANDARD_AUTO = DRX_AUTO,
					    
		DRX_AUD_STANDARD_UNKNOWN = DRX_UNKNOWN
					    
	};

 
 
	enum drx_aud_nicam_status {
		DRX_AUD_NICAM_DETECTED = 0,
					   
		DRX_AUD_NICAM_NOT_DETECTED,
					   
		DRX_AUD_NICAM_BAD	   
	};

 
	struct drx_aud_status {
		bool stereo;		   
		bool carrier_a;	   
		bool carrier_b;	   
		bool sap;		   
		bool rds;		   
		enum drx_aud_nicam_status nicam_status;
					   
		s8 fm_ident;		   
	};

 

 
	struct drx_cfg_aud_rds {
		bool valid;		   
		u16 data[18];		   
	};

 
 
	enum drx_aud_avc_mode {
		DRX_AUD_AVC_OFF,	   
		DRX_AUD_AVC_DECAYTIME_8S,  
		DRX_AUD_AVC_DECAYTIME_4S,  
		DRX_AUD_AVC_DECAYTIME_2S,  
		DRX_AUD_AVC_DECAYTIME_20MS 
	};

 
	enum drx_aud_avc_max_gain {
		DRX_AUD_AVC_MAX_GAIN_0DB,  
		DRX_AUD_AVC_MAX_GAIN_6DB,  
		DRX_AUD_AVC_MAX_GAIN_12DB  
	};

 
	enum drx_aud_avc_max_atten {
		DRX_AUD_AVC_MAX_ATTEN_12DB,
					   
		DRX_AUD_AVC_MAX_ATTEN_18DB,
					   
		DRX_AUD_AVC_MAX_ATTEN_24DB 
	};
 
	struct drx_cfg_aud_volume {
		bool mute;		   
		s16 volume;		   
		enum drx_aud_avc_mode avc_mode;   
		u16 avc_ref_level;	   
		enum drx_aud_avc_max_gain avc_max_gain;
					   
		enum drx_aud_avc_max_atten avc_max_atten;
					   
		s16 strength_left;	   
		s16 strength_right;	   
	};

 
 
	enum drxi2s_mode {
		DRX_I2S_MODE_MASTER,	   
		DRX_I2S_MODE_SLAVE	   
	};

 
	enum drxi2s_word_length {
		DRX_I2S_WORDLENGTH_32 = 0, 
		DRX_I2S_WORDLENGTH_16 = 1  
	};

 
	enum drxi2s_format {
		DRX_I2S_FORMAT_WS_WITH_DATA,
				     
		DRX_I2S_FORMAT_WS_ADVANCED
				     
	};

 
	enum drxi2s_polarity {
		DRX_I2S_POLARITY_RIGHT, 
		DRX_I2S_POLARITY_LEFT   
	};

 
	struct drx_cfg_i2s_output {
		bool output_enable;	   
		u32 frequency;	   
		enum drxi2s_mode mode;	   
		enum drxi2s_word_length word_length;
					   
		enum drxi2s_polarity polarity; 
		enum drxi2s_format format;	   
	};

 
 
	enum drx_aud_fm_deemphasis {
		DRX_AUD_FM_DEEMPH_50US,
		DRX_AUD_FM_DEEMPH_75US,
		DRX_AUD_FM_DEEMPH_OFF
	};

 
	enum drx_cfg_aud_deviation {
		DRX_AUD_DEVIATION_NORMAL,
		DRX_AUD_DEVIATION_HIGH
	};

 
	enum drx_no_carrier_option {
		DRX_NO_CARRIER_MUTE,
		DRX_NO_CARRIER_NOISE
	};

 
	enum drx_cfg_aud_auto_sound {
		DRX_AUD_AUTO_SOUND_OFF = 0,
		DRX_AUD_AUTO_SOUND_SELECT_ON_CHANGE_ON,
		DRX_AUD_AUTO_SOUND_SELECT_ON_CHANGE_OFF
	};

 
	struct drx_cfg_aud_ass_thres {
		u16 a2;	 
		u16 btsc;	 
		u16 nicam;	 
	};

 
	struct drx_aud_carrier {
		u16 thres;	 
		enum drx_no_carrier_option opt;	 
		s32 shift;	 
		s32 dco;	 
	};

 
	struct drx_cfg_aud_carriers {
		struct drx_aud_carrier a;
		struct drx_aud_carrier b;
	};

 
	enum drx_aud_i2s_src {
		DRX_AUD_SRC_MONO,
		DRX_AUD_SRC_STEREO_OR_AB,
		DRX_AUD_SRC_STEREO_OR_A,
		DRX_AUD_SRC_STEREO_OR_B};

 
	enum drx_aud_i2s_matrix {
		DRX_AUD_I2S_MATRIX_A_MONO,
					 
		DRX_AUD_I2S_MATRIX_B_MONO,
					 
		DRX_AUD_I2S_MATRIX_STEREO,
					 
		DRX_AUD_I2S_MATRIX_MONO	 };

 
	enum drx_aud_fm_matrix {
		DRX_AUD_FM_MATRIX_NO_MATRIX,
		DRX_AUD_FM_MATRIX_GERMAN,
		DRX_AUD_FM_MATRIX_KOREAN,
		DRX_AUD_FM_MATRIX_SOUND_A,
		DRX_AUD_FM_MATRIX_SOUND_B};

 
struct drx_cfg_aud_mixer {
	enum drx_aud_i2s_src source_i2s;
	enum drx_aud_i2s_matrix matrix_i2s;
	enum drx_aud_fm_matrix matrix_fm;
};

 
	enum drx_cfg_aud_av_sync {
		DRX_AUD_AVSYNC_OFF, 
		DRX_AUD_AVSYNC_NTSC,
				    
		DRX_AUD_AVSYNC_MONOCHROME,
				    
		DRX_AUD_AVSYNC_PAL_SECAM
				    };

 
struct drx_cfg_aud_prescale {
	u16 fm_deviation;
	s16 nicam_gain;
};

 
struct drx_aud_beep {
	s16 volume;	 
	u16 frequency;	 
	bool mute;
};

 
	enum drx_aud_btsc_detect {
		DRX_BTSC_STEREO,
		DRX_BTSC_MONO_AND_SAP};

 
struct drx_aud_data {
	 
	bool audio_is_active;
	enum drx_aud_standard audio_standard;
	struct drx_cfg_i2s_output i2sdata;
	struct drx_cfg_aud_volume volume;
	enum drx_cfg_aud_auto_sound auto_sound;
	struct drx_cfg_aud_ass_thres ass_thresholds;
	struct drx_cfg_aud_carriers carriers;
	struct drx_cfg_aud_mixer mixer;
	enum drx_cfg_aud_deviation deviation;
	enum drx_cfg_aud_av_sync av_sync;
	struct drx_cfg_aud_prescale prescale;
	enum drx_aud_fm_deemphasis deemph;
	enum drx_aud_btsc_detect btsc_detect;
	 
	u16 rds_data_counter;
	bool rds_data_present;
};

 
	enum drx_qam_lock_range {
		DRX_QAM_LOCKRANGE_NORMAL,
		DRX_QAM_LOCKRANGE_EXTENDED};

 
 
 
 
 

 
	typedef u32 dr_xaddr_t, *pdr_xaddr_t;

 
	typedef u32 dr_xflags_t, *pdr_xflags_t;

 
	typedef int(*drx_write_block_func_t) (struct i2c_device_addr *dev_addr,	 
						   u32 addr,	 
						   u16 datasize,	 
						   u8 *data,	 
						   u32 flags);

 
	typedef int(*drx_read_block_func_t) (struct i2c_device_addr *dev_addr,	 
						  u32 addr,	 
						  u16 datasize,	 
						  u8 *data,	 
						  u32 flags);

 
	typedef int(*drx_write_reg8func_t) (struct i2c_device_addr *dev_addr,	 
						  u32 addr,	 
						  u8 data,	 
						  u32 flags);

 
	typedef int(*drx_read_reg8func_t) (struct i2c_device_addr *dev_addr,	 
						 u32 addr,	 
						 u8 *data,	 
						 u32 flags);

 
	typedef int(*drx_read_modify_write_reg8func_t) (struct i2c_device_addr *dev_addr,	 
							    u32 waddr,	 
							    u32 raddr,	 
							    u8 wdata,	 
							    u8 *rdata);	 

 
	typedef int(*drx_write_reg16func_t) (struct i2c_device_addr *dev_addr,	 
						   u32 addr,	 
						   u16 data,	 
						   u32 flags);

 
	typedef int(*drx_read_reg16func_t) (struct i2c_device_addr *dev_addr,	 
						  u32 addr,	 
						  u16 *data,	 
						  u32 flags);

 
	typedef int(*drx_read_modify_write_reg16func_t) (struct i2c_device_addr *dev_addr,	 
							     u32 waddr,	 
							     u32 raddr,	 
							     u16 wdata,	 
							     u16 *rdata);	 

 
	typedef int(*drx_write_reg32func_t) (struct i2c_device_addr *dev_addr,	 
						   u32 addr,	 
						   u32 data,	 
						   u32 flags);

 
	typedef int(*drx_read_reg32func_t) (struct i2c_device_addr *dev_addr,	 
						  u32 addr,	 
						  u32 *data,	 
						  u32 flags);

 
	typedef int(*drx_read_modify_write_reg32func_t) (struct i2c_device_addr *dev_addr,	 
							     u32 waddr,	 
							     u32 raddr,	 
							     u32 wdata,	 
							     u32 *rdata);	 

 
struct drx_access_func {
	drx_write_block_func_t write_block_func;
	drx_read_block_func_t read_block_func;
	drx_write_reg8func_t write_reg8func;
	drx_read_reg8func_t read_reg8func;
	drx_read_modify_write_reg8func_t read_modify_write_reg8func;
	drx_write_reg16func_t write_reg16func;
	drx_read_reg16func_t read_reg16func;
	drx_read_modify_write_reg16func_t read_modify_write_reg16func;
	drx_write_reg32func_t write_reg32func;
	drx_read_reg32func_t read_reg32func;
	drx_read_modify_write_reg32func_t read_modify_write_reg32func;
};

 
struct drx_reg_dump {
	u32 address;
	u32 data;
};

 
 
 
 
 

 
	struct drx_common_attr {
		 
		char *microcode_file;    
		bool verify_microcode;
				    
		struct drx_mc_version_rec mcversion;
				    

		 
		s32 intermediate_freq;
				      
		s32 sys_clock_freq;
				      
		s32 osc_clock_freq;
				      
		s16 osc_clock_deviation;
				      
		bool mirror_freq_spect;
				      

		 
		struct drx_cfg_mpeg_output mpeg_cfg;
				      

		bool is_opened;      

		 
		struct drx_scan_param *scan_param;
				       
		u16 scan_freq_plan_index;
				       
		s32 scan_next_frequency;
				       
		bool scan_ready;      
		u32 scan_max_channels; 
		u32 scan_channels_scanned;
					 
		 
		drx_scan_func_t scan_function;
				       
		 
		void *scan_context;     
		 
		u16 scan_demod_lock_timeout;
					  
		enum drx_lock_status scan_desired_lock;
				       
		 
		bool scan_active;     

		 
		enum drx_power_mode current_power_mode;
				       

		 
		u8 tuner_port_nr;      
		s32 tuner_min_freq_rf;
				       
		s32 tuner_max_freq_rf;
				       
		bool tuner_rf_agc_pol;  
		bool tuner_if_agc_pol;  
		bool tuner_slow_mode;  

		struct drx_channel current_channel;
				       
		enum drx_standard current_standard;
				       
		enum drx_standard prev_standard;
				       
		enum drx_standard di_cache_standard;
				       
		bool use_bootloader;  
		u32 capabilities;    
		u32 product_id;       };

 

struct drx_demod_instance;

 
struct drx_demod_instance {
				 
	struct i2c_device_addr *my_i2c_dev_addr;
				 
	struct drx_common_attr *my_common_attr;
				 
	void *my_ext_attr;     
	 

	struct i2c_adapter	*i2c;
	const struct firmware	*firmware;
};

 

 

#define DRX_STR_STANDARD(x) ( \
	(x == DRX_STANDARD_DVBT)  ? "DVB-T"            : \
	(x == DRX_STANDARD_8VSB)  ? "8VSB"             : \
	(x == DRX_STANDARD_NTSC)  ? "NTSC"             : \
	(x == DRX_STANDARD_PAL_SECAM_BG)  ? "PAL/SECAM B/G"    : \
	(x == DRX_STANDARD_PAL_SECAM_DK)  ? "PAL/SECAM D/K"    : \
	(x == DRX_STANDARD_PAL_SECAM_I)  ? "PAL/SECAM I"      : \
	(x == DRX_STANDARD_PAL_SECAM_L)  ? "PAL/SECAM L"      : \
	(x == DRX_STANDARD_PAL_SECAM_LP)  ? "PAL/SECAM LP"     : \
	(x == DRX_STANDARD_ITU_A)  ? "ITU-A"            : \
	(x == DRX_STANDARD_ITU_B)  ? "ITU-B"            : \
	(x == DRX_STANDARD_ITU_C)  ? "ITU-C"            : \
	(x == DRX_STANDARD_ITU_D)  ? "ITU-D"            : \
	(x == DRX_STANDARD_FM)  ? "FM"               : \
	(x == DRX_STANDARD_DTMB)  ? "DTMB"             : \
	(x == DRX_STANDARD_AUTO)  ? "Auto"             : \
	(x == DRX_STANDARD_UNKNOWN)  ? "Unknown"          : \
	"(Invalid)")

 

#define DRX_STR_BANDWIDTH(x) ( \
	(x == DRX_BANDWIDTH_8MHZ)  ?  "8 MHz"            : \
	(x == DRX_BANDWIDTH_7MHZ)  ?  "7 MHz"            : \
	(x == DRX_BANDWIDTH_6MHZ)  ?  "6 MHz"            : \
	(x == DRX_BANDWIDTH_AUTO)  ?  "Auto"             : \
	(x == DRX_BANDWIDTH_UNKNOWN)  ?  "Unknown"          : \
	"(Invalid)")
#define DRX_STR_FFTMODE(x) ( \
	(x == DRX_FFTMODE_2K)  ?  "2k"               : \
	(x == DRX_FFTMODE_4K)  ?  "4k"               : \
	(x == DRX_FFTMODE_8K)  ?  "8k"               : \
	(x == DRX_FFTMODE_AUTO)  ?  "Auto"             : \
	(x == DRX_FFTMODE_UNKNOWN)  ?  "Unknown"          : \
	"(Invalid)")
#define DRX_STR_GUARD(x) ( \
	(x == DRX_GUARD_1DIV32)  ?  "1/32nd"           : \
	(x == DRX_GUARD_1DIV16)  ?  "1/16th"           : \
	(x == DRX_GUARD_1DIV8)  ?  "1/8th"            : \
	(x == DRX_GUARD_1DIV4)  ?  "1/4th"            : \
	(x == DRX_GUARD_AUTO)  ?  "Auto"             : \
	(x == DRX_GUARD_UNKNOWN)  ?  "Unknown"          : \
	"(Invalid)")
#define DRX_STR_CONSTELLATION(x) ( \
	(x == DRX_CONSTELLATION_BPSK)  ?  "BPSK"            : \
	(x == DRX_CONSTELLATION_QPSK)  ?  "QPSK"            : \
	(x == DRX_CONSTELLATION_PSK8)  ?  "PSK8"            : \
	(x == DRX_CONSTELLATION_QAM16)  ?  "QAM16"           : \
	(x == DRX_CONSTELLATION_QAM32)  ?  "QAM32"           : \
	(x == DRX_CONSTELLATION_QAM64)  ?  "QAM64"           : \
	(x == DRX_CONSTELLATION_QAM128)  ?  "QAM128"          : \
	(x == DRX_CONSTELLATION_QAM256)  ?  "QAM256"          : \
	(x == DRX_CONSTELLATION_QAM512)  ?  "QAM512"          : \
	(x == DRX_CONSTELLATION_QAM1024)  ?  "QAM1024"         : \
	(x == DRX_CONSTELLATION_QPSK_NR)  ?  "QPSK_NR"            : \
	(x == DRX_CONSTELLATION_AUTO)  ?  "Auto"            : \
	(x == DRX_CONSTELLATION_UNKNOWN)  ?  "Unknown"         : \
	"(Invalid)")
#define DRX_STR_CODERATE(x) ( \
	(x == DRX_CODERATE_1DIV2)  ?  "1/2nd"           : \
	(x == DRX_CODERATE_2DIV3)  ?  "2/3rd"           : \
	(x == DRX_CODERATE_3DIV4)  ?  "3/4th"           : \
	(x == DRX_CODERATE_5DIV6)  ?  "5/6th"           : \
	(x == DRX_CODERATE_7DIV8)  ?  "7/8th"           : \
	(x == DRX_CODERATE_AUTO)  ?  "Auto"            : \
	(x == DRX_CODERATE_UNKNOWN)  ?  "Unknown"         : \
	"(Invalid)")
#define DRX_STR_HIERARCHY(x) ( \
	(x == DRX_HIERARCHY_NONE)  ?  "None"            : \
	(x == DRX_HIERARCHY_ALPHA1)  ?  "Alpha=1"         : \
	(x == DRX_HIERARCHY_ALPHA2)  ?  "Alpha=2"         : \
	(x == DRX_HIERARCHY_ALPHA4)  ?  "Alpha=4"         : \
	(x == DRX_HIERARCHY_AUTO)  ?  "Auto"            : \
	(x == DRX_HIERARCHY_UNKNOWN)  ?  "Unknown"         : \
	"(Invalid)")
#define DRX_STR_PRIORITY(x) ( \
	(x == DRX_PRIORITY_LOW)  ?  "Low"             : \
	(x == DRX_PRIORITY_HIGH)  ?  "High"            : \
	(x == DRX_PRIORITY_UNKNOWN)  ?  "Unknown"         : \
	"(Invalid)")
#define DRX_STR_MIRROR(x) ( \
	(x == DRX_MIRROR_NO)  ?  "Normal"          : \
	(x == DRX_MIRROR_YES)  ?  "Mirrored"        : \
	(x == DRX_MIRROR_AUTO)  ?  "Auto"            : \
	(x == DRX_MIRROR_UNKNOWN)  ?  "Unknown"         : \
	"(Invalid)")
#define DRX_STR_CLASSIFICATION(x) ( \
	(x == DRX_CLASSIFICATION_GAUSS)  ?  "Gaussion"        : \
	(x == DRX_CLASSIFICATION_HVY_GAUSS)  ?  "Heavy Gaussion"  : \
	(x == DRX_CLASSIFICATION_COCHANNEL)  ?  "Co-channel"      : \
	(x == DRX_CLASSIFICATION_STATIC)  ?  "Static echo"     : \
	(x == DRX_CLASSIFICATION_MOVING)  ?  "Moving echo"     : \
	(x == DRX_CLASSIFICATION_ZERODB)  ?  "Zero dB echo"    : \
	(x == DRX_CLASSIFICATION_UNKNOWN)  ?  "Unknown"         : \
	(x == DRX_CLASSIFICATION_AUTO)  ?  "Auto"            : \
	"(Invalid)")

#define DRX_STR_INTERLEAVEMODE(x) ( \
	(x == DRX_INTERLEAVEMODE_I128_J1) ? "I128_J1"         : \
	(x == DRX_INTERLEAVEMODE_I128_J1_V2) ? "I128_J1_V2"      : \
	(x == DRX_INTERLEAVEMODE_I128_J2) ? "I128_J2"         : \
	(x == DRX_INTERLEAVEMODE_I64_J2) ? "I64_J2"          : \
	(x == DRX_INTERLEAVEMODE_I128_J3) ? "I128_J3"         : \
	(x == DRX_INTERLEAVEMODE_I32_J4) ? "I32_J4"          : \
	(x == DRX_INTERLEAVEMODE_I128_J4) ? "I128_J4"         : \
	(x == DRX_INTERLEAVEMODE_I16_J8) ? "I16_J8"          : \
	(x == DRX_INTERLEAVEMODE_I128_J5) ? "I128_J5"         : \
	(x == DRX_INTERLEAVEMODE_I8_J16) ? "I8_J16"          : \
	(x == DRX_INTERLEAVEMODE_I128_J6) ? "I128_J6"         : \
	(x == DRX_INTERLEAVEMODE_RESERVED_11) ? "Reserved 11"     : \
	(x == DRX_INTERLEAVEMODE_I128_J7) ? "I128_J7"         : \
	(x == DRX_INTERLEAVEMODE_RESERVED_13) ? "Reserved 13"     : \
	(x == DRX_INTERLEAVEMODE_I128_J8) ? "I128_J8"         : \
	(x == DRX_INTERLEAVEMODE_RESERVED_15) ? "Reserved 15"     : \
	(x == DRX_INTERLEAVEMODE_I12_J17) ? "I12_J17"         : \
	(x == DRX_INTERLEAVEMODE_I5_J4) ? "I5_J4"           : \
	(x == DRX_INTERLEAVEMODE_B52_M240) ? "B52_M240"        : \
	(x == DRX_INTERLEAVEMODE_B52_M720) ? "B52_M720"        : \
	(x == DRX_INTERLEAVEMODE_B52_M48) ? "B52_M48"         : \
	(x == DRX_INTERLEAVEMODE_B52_M0) ? "B52_M0"          : \
	(x == DRX_INTERLEAVEMODE_UNKNOWN) ? "Unknown"         : \
	(x == DRX_INTERLEAVEMODE_AUTO) ? "Auto"            : \
	"(Invalid)")

#define DRX_STR_LDPC(x) ( \
	(x == DRX_LDPC_0_4) ? "0.4"             : \
	(x == DRX_LDPC_0_6) ? "0.6"             : \
	(x == DRX_LDPC_0_8) ? "0.8"             : \
	(x == DRX_LDPC_AUTO) ? "Auto"            : \
	(x == DRX_LDPC_UNKNOWN) ? "Unknown"         : \
	"(Invalid)")

#define DRX_STR_CARRIER(x) ( \
	(x == DRX_CARRIER_MULTI) ? "Multi"           : \
	(x == DRX_CARRIER_SINGLE) ? "Single"          : \
	(x == DRX_CARRIER_AUTO) ? "Auto"            : \
	(x == DRX_CARRIER_UNKNOWN) ? "Unknown"         : \
	"(Invalid)")

#define DRX_STR_FRAMEMODE(x) ( \
	(x == DRX_FRAMEMODE_420)  ? "420"                : \
	(x == DRX_FRAMEMODE_595)  ? "595"                : \
	(x == DRX_FRAMEMODE_945)  ? "945"                : \
	(x == DRX_FRAMEMODE_420_FIXED_PN)  ? "420 with fixed PN"  : \
	(x == DRX_FRAMEMODE_945_FIXED_PN)  ? "945 with fixed PN"  : \
	(x == DRX_FRAMEMODE_AUTO)  ? "Auto"               : \
	(x == DRX_FRAMEMODE_UNKNOWN)  ? "Unknown"            : \
	"(Invalid)")

#define DRX_STR_PILOT(x) ( \
	(x == DRX_PILOT_ON) ?   "On"              : \
	(x == DRX_PILOT_OFF) ?   "Off"             : \
	(x == DRX_PILOT_AUTO) ?   "Auto"            : \
	(x == DRX_PILOT_UNKNOWN) ?   "Unknown"         : \
	"(Invalid)")
 

#define DRX_STR_TPS_FRAME(x)  ( \
	(x == DRX_TPS_FRAME1)  ?  "Frame1"          : \
	(x == DRX_TPS_FRAME2)  ?  "Frame2"          : \
	(x == DRX_TPS_FRAME3)  ?  "Frame3"          : \
	(x == DRX_TPS_FRAME4)  ?  "Frame4"          : \
	(x == DRX_TPS_FRAME_UNKNOWN)  ?  "Unknown"         : \
	"(Invalid)")

 

#define DRX_STR_LOCKSTATUS(x) ( \
	(x == DRX_NEVER_LOCK)  ?  "Never"           : \
	(x == DRX_NOT_LOCKED)  ?  "No"              : \
	(x == DRX_LOCKED)  ?  "Locked"          : \
	(x == DRX_LOCK_STATE_1)  ?  "Lock state 1"    : \
	(x == DRX_LOCK_STATE_2)  ?  "Lock state 2"    : \
	(x == DRX_LOCK_STATE_3)  ?  "Lock state 3"    : \
	(x == DRX_LOCK_STATE_4)  ?  "Lock state 4"    : \
	(x == DRX_LOCK_STATE_5)  ?  "Lock state 5"    : \
	(x == DRX_LOCK_STATE_6)  ?  "Lock state 6"    : \
	(x == DRX_LOCK_STATE_7)  ?  "Lock state 7"    : \
	(x == DRX_LOCK_STATE_8)  ?  "Lock state 8"    : \
	(x == DRX_LOCK_STATE_9)  ?  "Lock state 9"    : \
	"(Invalid)")

 
#define DRX_STR_MODULE(x) ( \
	(x == DRX_MODULE_DEVICE)  ?  "Device"                : \
	(x == DRX_MODULE_MICROCODE)  ?  "Microcode"             : \
	(x == DRX_MODULE_DRIVERCORE)  ?  "CoreDriver"            : \
	(x == DRX_MODULE_DEVICEDRIVER)  ?  "DeviceDriver"          : \
	(x == DRX_MODULE_BSP_I2C)  ?  "BSP I2C"               : \
	(x == DRX_MODULE_BSP_TUNER)  ?  "BSP Tuner"             : \
	(x == DRX_MODULE_BSP_HOST)  ?  "BSP Host"              : \
	(x == DRX_MODULE_DAP)  ?  "Data Access Protocol"  : \
	(x == DRX_MODULE_UNKNOWN)  ?  "Unknown"               : \
	"(Invalid)")

#define DRX_STR_POWER_MODE(x) ( \
	(x == DRX_POWER_UP)  ?  "DRX_POWER_UP    "  : \
	(x == DRX_POWER_MODE_1)  ?  "DRX_POWER_MODE_1"  : \
	(x == DRX_POWER_MODE_2)  ?  "DRX_POWER_MODE_2"  : \
	(x == DRX_POWER_MODE_3)  ?  "DRX_POWER_MODE_3"  : \
	(x == DRX_POWER_MODE_4)  ?  "DRX_POWER_MODE_4"  : \
	(x == DRX_POWER_MODE_5)  ?  "DRX_POWER_MODE_5"  : \
	(x == DRX_POWER_MODE_6)  ?  "DRX_POWER_MODE_6"  : \
	(x == DRX_POWER_MODE_7)  ?  "DRX_POWER_MODE_7"  : \
	(x == DRX_POWER_MODE_8)  ?  "DRX_POWER_MODE_8"  : \
	(x == DRX_POWER_MODE_9)  ?  "DRX_POWER_MODE_9"  : \
	(x == DRX_POWER_MODE_10)  ?  "DRX_POWER_MODE_10" : \
	(x == DRX_POWER_MODE_11)  ?  "DRX_POWER_MODE_11" : \
	(x == DRX_POWER_MODE_12)  ?  "DRX_POWER_MODE_12" : \
	(x == DRX_POWER_MODE_13)  ?  "DRX_POWER_MODE_13" : \
	(x == DRX_POWER_MODE_14)  ?  "DRX_POWER_MODE_14" : \
	(x == DRX_POWER_MODE_15)  ?  "DRX_POWER_MODE_15" : \
	(x == DRX_POWER_MODE_16)  ?  "DRX_POWER_MODE_16" : \
	(x == DRX_POWER_DOWN)  ?  "DRX_POWER_DOWN  " : \
	"(Invalid)")

#define DRX_STR_OOB_STANDARD(x) ( \
	(x == DRX_OOB_MODE_A)  ?  "ANSI 55-1  " : \
	(x == DRX_OOB_MODE_B_GRADE_A)  ?  "ANSI 55-2 A" : \
	(x == DRX_OOB_MODE_B_GRADE_B)  ?  "ANSI 55-2 B" : \
	"(Invalid)")

#define DRX_STR_AUD_STANDARD(x) ( \
	(x == DRX_AUD_STANDARD_BTSC)  ? "BTSC"                     : \
	(x == DRX_AUD_STANDARD_A2)  ? "A2"                       : \
	(x == DRX_AUD_STANDARD_EIAJ)  ? "EIAJ"                     : \
	(x == DRX_AUD_STANDARD_FM_STEREO)  ? "FM Stereo"                : \
	(x == DRX_AUD_STANDARD_AUTO)  ? "Auto"                     : \
	(x == DRX_AUD_STANDARD_M_MONO)  ? "M-Standard Mono"          : \
	(x == DRX_AUD_STANDARD_D_K_MONO)  ? "D/K Mono FM"              : \
	(x == DRX_AUD_STANDARD_BG_FM)  ? "B/G-Dual Carrier FM (A2)" : \
	(x == DRX_AUD_STANDARD_D_K1)  ? "D/K1-Dual Carrier FM"     : \
	(x == DRX_AUD_STANDARD_D_K2)  ? "D/K2-Dual Carrier FM"     : \
	(x == DRX_AUD_STANDARD_D_K3)  ? "D/K3-Dual Carrier FM"     : \
	(x == DRX_AUD_STANDARD_BG_NICAM_FM)  ? "B/G-NICAM-FM"             : \
	(x == DRX_AUD_STANDARD_L_NICAM_AM)  ? "L-NICAM-AM"               : \
	(x == DRX_AUD_STANDARD_I_NICAM_FM)  ? "I-NICAM-FM"               : \
	(x == DRX_AUD_STANDARD_D_K_NICAM_FM)  ? "D/K-NICAM-FM"             : \
	(x == DRX_AUD_STANDARD_UNKNOWN)  ? "Unknown"                  : \
	"(Invalid)")
#define DRX_STR_AUD_STEREO(x) ( \
	(x == true)  ? "Stereo"           : \
	(x == false)  ? "Mono"             : \
	"(Invalid)")

#define DRX_STR_AUD_SAP(x) ( \
	(x == true)  ? "Present"          : \
	(x == false)  ? "Not present"      : \
	"(Invalid)")

#define DRX_STR_AUD_CARRIER(x) ( \
	(x == true)  ? "Present"          : \
	(x == false)  ? "Not present"      : \
	"(Invalid)")

#define DRX_STR_AUD_RDS(x) ( \
	(x == true)  ? "Available"        : \
	(x == false)  ? "Not Available"    : \
	"(Invalid)")

#define DRX_STR_AUD_NICAM_STATUS(x) ( \
	(x == DRX_AUD_NICAM_DETECTED)  ? "Detected"         : \
	(x == DRX_AUD_NICAM_NOT_DETECTED)  ? "Not detected"     : \
	(x == DRX_AUD_NICAM_BAD)  ? "Bad"              : \
	"(Invalid)")

#define DRX_STR_RDS_VALID(x) ( \
	(x == true)  ? "Valid"            : \
	(x == false)  ? "Not Valid"        : \
	"(Invalid)")

 

 

#define DRX_ATTR_MCRECORD(d)        ((d)->my_common_attr->mcversion)
#define DRX_ATTR_MIRRORFREQSPECT(d) ((d)->my_common_attr->mirror_freq_spect)
#define DRX_ATTR_CURRENTPOWERMODE(d)((d)->my_common_attr->current_power_mode)
#define DRX_ATTR_ISOPENED(d)        ((d)->my_common_attr->is_opened)
#define DRX_ATTR_USEBOOTLOADER(d)   ((d)->my_common_attr->use_bootloader)
#define DRX_ATTR_CURRENTSTANDARD(d) ((d)->my_common_attr->current_standard)
#define DRX_ATTR_PREVSTANDARD(d)    ((d)->my_common_attr->prev_standard)
#define DRX_ATTR_CACHESTANDARD(d)   ((d)->my_common_attr->di_cache_standard)
#define DRX_ATTR_CURRENTCHANNEL(d)  ((d)->my_common_attr->current_channel)
#define DRX_ATTR_MICROCODE(d)       ((d)->my_common_attr->microcode)
#define DRX_ATTR_VERIFYMICROCODE(d) ((d)->my_common_attr->verify_microcode)
#define DRX_ATTR_CAPABILITIES(d)    ((d)->my_common_attr->capabilities)
#define DRX_ATTR_PRODUCTID(d)       ((d)->my_common_attr->product_id)
#define DRX_ATTR_INTERMEDIATEFREQ(d) ((d)->my_common_attr->intermediate_freq)
#define DRX_ATTR_SYSCLOCKFREQ(d)     ((d)->my_common_attr->sys_clock_freq)
#define DRX_ATTR_TUNERRFAGCPOL(d)   ((d)->my_common_attr->tuner_rf_agc_pol)
#define DRX_ATTR_TUNERIFAGCPOL(d)    ((d)->my_common_attr->tuner_if_agc_pol)
#define DRX_ATTR_TUNERSLOWMODE(d)    ((d)->my_common_attr->tuner_slow_mode)
#define DRX_ATTR_TUNERSPORTNR(d)     ((d)->my_common_attr->tuner_port_nr)
#define DRX_ATTR_I2CADDR(d)         ((d)->my_i2c_dev_addr->i2c_addr)
#define DRX_ATTR_I2CDEVID(d)        ((d)->my_i2c_dev_addr->i2c_dev_id)
#define DRX_ISMCVERTYPE(x) ((x) == AUX_VER_RECORD)

 

 

#define DRX_ACCESSMACRO_SET(demod, value, cfg_name, data_type)             \
	do {                                                               \
		struct drx_cfg config;                                     \
		data_type cfg_data;                                        \
		config.cfg_type = cfg_name;                                \
		config.cfg_data = &cfg_data;                               \
		cfg_data = value;                                          \
		drx_ctrl(demod, DRX_CTRL_SET_CFG, &config);                \
	} while (0)

#define DRX_ACCESSMACRO_GET(demod, value, cfg_name, data_type, error_value) \
	do {                                                                \
		int cfg_status;                                             \
		struct drx_cfg config;                                      \
		data_type    cfg_data;                                      \
		config.cfg_type = cfg_name;                                 \
		config.cfg_data = &cfg_data;                                \
		cfg_status = drx_ctrl(demod, DRX_CTRL_GET_CFG, &config);    \
		if (cfg_status == 0) {                                      \
			value = cfg_data;                                   \
		} else {                                                    \
			value = (data_type)error_value;                     \
		}                                                           \
	} while (0)

 

#ifndef DRX_XS_CFG_BASE
#define DRX_XS_CFG_BASE (500)
#endif

#define DRX_XS_CFG_PRESET          (DRX_XS_CFG_BASE + 0)
#define DRX_XS_CFG_AUD_BTSC_DETECT (DRX_XS_CFG_BASE + 1)
#define DRX_XS_CFG_QAM_LOCKRANGE   (DRX_XS_CFG_BASE + 2)

 

#define DRX_SET_PRESET(d, x) \
	DRX_ACCESSMACRO_SET((d), (x), DRX_XS_CFG_PRESET, char*)
#define DRX_GET_PRESET(d, x) \
	DRX_ACCESSMACRO_GET((d), (x), DRX_XS_CFG_PRESET, char*, "ERROR")

#define DRX_SET_AUD_BTSC_DETECT(d, x) DRX_ACCESSMACRO_SET((d), (x), \
	 DRX_XS_CFG_AUD_BTSC_DETECT, enum drx_aud_btsc_detect)
#define DRX_GET_AUD_BTSC_DETECT(d, x) DRX_ACCESSMACRO_GET((d), (x), \
	 DRX_XS_CFG_AUD_BTSC_DETECT, enum drx_aud_btsc_detect, DRX_UNKNOWN)

#define DRX_SET_QAM_LOCKRANGE(d, x) DRX_ACCESSMACRO_SET((d), (x), \
	 DRX_XS_CFG_QAM_LOCKRANGE, enum drx_qam_lock_range)
#define DRX_GET_QAM_LOCKRANGE(d, x) DRX_ACCESSMACRO_GET((d), (x), \
	 DRX_XS_CFG_QAM_LOCKRANGE, enum drx_qam_lock_range, DRX_UNKNOWN)

 
#define DRX_ISATVSTD(std) (((std) == DRX_STANDARD_PAL_SECAM_BG) || \
			      ((std) == DRX_STANDARD_PAL_SECAM_DK) || \
			      ((std) == DRX_STANDARD_PAL_SECAM_I) || \
			      ((std) == DRX_STANDARD_PAL_SECAM_L) || \
			      ((std) == DRX_STANDARD_PAL_SECAM_LP) || \
			      ((std) == DRX_STANDARD_NTSC) || \
			      ((std) == DRX_STANDARD_FM))

 
#define DRX_ISQAMSTD(std) (((std) == DRX_STANDARD_ITU_A) || \
			      ((std) == DRX_STANDARD_ITU_B) || \
			      ((std) == DRX_STANDARD_ITU_C) || \
			      ((std) == DRX_STANDARD_ITU_D))

 
#define DRX_ISVSBSTD(std) ((std) == DRX_STANDARD_8VSB)

 
#define DRX_ISDVBTSTD(std) ((std) == DRX_STANDARD_DVBT)

 
#endif				 
