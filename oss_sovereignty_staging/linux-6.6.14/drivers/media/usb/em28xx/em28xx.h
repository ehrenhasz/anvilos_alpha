 
 

#ifndef _EM28XX_H
#define _EM28XX_H

#include <linux/bitfield.h>

#define EM28XX_VERSION "0.2.2"
#define DRIVER_DESC    "Empia em28xx device driver"

#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/videodev2.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/i2c/ir-kbd-i2c.h>
#include <media/rc-core.h>
#include "xc2028.h"
#include "xc5000.h"
#include "em28xx-reg.h"

 
#define EM2800_BOARD_UNKNOWN			  0
#define EM2820_BOARD_UNKNOWN			  1
#define EM2820_BOARD_TERRATEC_CINERGY_250	  2
#define EM2820_BOARD_PINNACLE_USB_2		  3
#define EM2820_BOARD_HAUPPAUGE_WINTV_USB_2	  4
#define EM2820_BOARD_MSI_VOX_USB_2		  5
#define EM2800_BOARD_TERRATEC_CINERGY_200	  6
#define EM2800_BOARD_LEADTEK_WINFAST_USBII	  7
#define EM2800_BOARD_KWORLD_USB2800		  8
#define EM2820_BOARD_PINNACLE_DVC_90		  9
#define EM2880_BOARD_HAUPPAUGE_WINTV_HVR_900	  10
#define EM2880_BOARD_TERRATEC_HYBRID_XS		  11
#define EM2820_BOARD_KWORLD_PVRTV2800RF		  12
#define EM2880_BOARD_TERRATEC_PRODIGY_XS	  13
#define EM2820_BOARD_PROLINK_PLAYTV_USB2	  14
#define EM2800_BOARD_VGEAR_POCKETTV		  15
#define EM2883_BOARD_HAUPPAUGE_WINTV_HVR_950	  16
#define EM2880_BOARD_PINNACLE_PCTV_HD_PRO	  17
#define EM2880_BOARD_HAUPPAUGE_WINTV_HVR_900_R2	  18
#define EM2860_BOARD_SAA711X_REFERENCE_DESIGN	  19
#define EM2880_BOARD_AMD_ATI_TV_WONDER_HD_600	  20
#define EM2800_BOARD_GRABBEEX_USB2800		  21
#define EM2750_BOARD_UNKNOWN			  22
#define EM2750_BOARD_DLCW_130			  23
#define EM2820_BOARD_DLINK_USB_TV		  24
#define EM2820_BOARD_GADMEI_UTV310		  25
#define EM2820_BOARD_HERCULES_SMART_TV_USB2	  26
#define EM2820_BOARD_PINNACLE_USB_2_FM1216ME	  27
#define EM2820_BOARD_LEADTEK_WINFAST_USBII_DELUXE 28
#define EM2860_BOARD_TVP5150_REFERENCE_DESIGN	  29
#define EM2820_BOARD_VIDEOLOGY_20K14XUSB	  30
#define EM2821_BOARD_USBGEAR_VD204		  31
#define EM2821_BOARD_SUPERCOMP_USB_2		  32
#define EM2860_BOARD_ELGATO_VIDEO_CAPTURE	  33
#define EM2860_BOARD_TERRATEC_HYBRID_XS		  34
#define EM2860_BOARD_TYPHOON_DVD_MAKER		  35
#define EM2860_BOARD_NETGMBH_CAM		  36
#define EM2860_BOARD_GADMEI_UTV330		  37
#define EM2861_BOARD_YAKUMO_MOVIE_MIXER		  38
#define EM2861_BOARD_KWORLD_PVRTV_300U		  39
#define EM2861_BOARD_PLEXTOR_PX_TV100U		  40
#define EM2870_BOARD_KWORLD_350U		  41
#define EM2870_BOARD_KWORLD_355U		  42
#define EM2870_BOARD_TERRATEC_XS		  43
#define EM2870_BOARD_TERRATEC_XS_MT2060		  44
#define EM2870_BOARD_PINNACLE_PCTV_DVB		  45
#define EM2870_BOARD_COMPRO_VIDEOMATE		  46
#define EM2880_BOARD_KWORLD_DVB_305U		  47
#define EM2880_BOARD_KWORLD_DVB_310U		  48
#define EM2880_BOARD_MSI_DIGIVOX_AD		  49
#define EM2880_BOARD_MSI_DIGIVOX_AD_II		  50
#define EM2880_BOARD_TERRATEC_HYBRID_XS_FR	  51
#define EM2881_BOARD_DNT_DA2_HYBRID		  52
#define EM2881_BOARD_PINNACLE_HYBRID_PRO	  53
#define EM2882_BOARD_KWORLD_VS_DVBT		  54
#define EM2882_BOARD_TERRATEC_HYBRID_XS		  55
#define EM2882_BOARD_PINNACLE_HYBRID_PRO_330E	  56
#define EM2883_BOARD_KWORLD_HYBRID_330U		  57
#define EM2820_BOARD_COMPRO_VIDEOMATE_FORYOU	  58
#define EM2874_BOARD_PCTV_HD_MINI_80E		  59
#define EM2883_BOARD_HAUPPAUGE_WINTV_HVR_850	  60
#define EM2820_BOARD_PROLINK_PLAYTV_BOX4_USB2	  61
#define EM2820_BOARD_GADMEI_TVR200		  62
#define EM2860_BOARD_KAIOMY_TVNPC_U2		  63
#define EM2860_BOARD_EASYCAP			  64
#define EM2820_BOARD_IODATA_GVMVP_SZ		  65
#define EM2880_BOARD_EMPIRE_DUAL_TV		  66
#define EM2860_BOARD_TERRATEC_GRABBY		  67
#define EM2860_BOARD_TERRATEC_AV350		  68
#define EM2882_BOARD_KWORLD_ATSC_315U		  69
#define EM2882_BOARD_EVGA_INDTUBE		  70
#define EM2820_BOARD_SILVERCREST_WEBCAM		  71
#define EM2861_BOARD_GADMEI_UTV330PLUS		  72
#define EM2870_BOARD_REDDO_DVB_C_USB_BOX	  73
#define EM2800_BOARD_VC211A			  74
#define EM2882_BOARD_DIKOM_DK300		  75
#define EM2870_BOARD_KWORLD_A340		  76
#define EM2874_BOARD_LEADERSHIP_ISDBT		  77
#define EM28174_BOARD_PCTV_290E			  78
#define EM2884_BOARD_TERRATEC_H5		  79
#define EM28174_BOARD_PCTV_460E			  80
#define EM2884_BOARD_HAUPPAUGE_WINTV_HVR_930C	  81
#define EM2884_BOARD_CINERGY_HTC_STICK		  82
#define EM2860_BOARD_HT_VIDBOX_NW03		  83
#define EM2874_BOARD_MAXMEDIA_UB425_TC		  84
#define EM2884_BOARD_PCTV_510E			  85
#define EM2884_BOARD_PCTV_520E			  86
#define EM2884_BOARD_TERRATEC_HTC_USB_XS	  87
#define EM2884_BOARD_C3TECH_DIGITAL_DUO		  88
#define EM2874_BOARD_DELOCK_61959		  89
#define EM2874_BOARD_KWORLD_UB435Q_V2		  90
#define EM2765_BOARD_SPEEDLINK_VAD_LAPLACE	  91
#define EM28178_BOARD_PCTV_461E                   92
#define EM2874_BOARD_KWORLD_UB435Q_V3		  93
#define EM28178_BOARD_PCTV_292E                   94
#define EM2861_BOARD_LEADTEK_VC100                95
#define EM28178_BOARD_TERRATEC_T2_STICK_HD        96
#define EM2884_BOARD_ELGATO_EYETV_HYBRID_2008     97
#define EM28178_BOARD_PLEX_PX_BCUD                98
#define EM28174_BOARD_HAUPPAUGE_WINTV_DUALHD_DVB  99
#define EM28174_BOARD_HAUPPAUGE_WINTV_DUALHD_01595 100
#define EM2884_BOARD_TERRATEC_H6		  101
#define EM2882_BOARD_ZOLID_HYBRID_TV_STICK		102
#define EM2861_BOARD_MAGIX_VIDEOWANDLER2          103
#define EM28178_BOARD_PCTV_461E_V2                104
#define EM2860_BOARD_MYGICA_IGRABBER              105
#define EM2874_BOARD_HAUPPAUGE_USB_QUADHD         106

 
#define EM28XX_MIN_BUF 4
#define EM28XX_DEF_BUF 8

 
#define URB_MAX_CTRL_SIZE 80

 
#define EM28XX_BOARD_NOT_VALIDATED 1
#define EM28XX_BOARD_VALIDATED	   0

 
#define EM28XX_START_AUDIO      1
#define EM28XX_STOP_AUDIO       0

 
#define EM28XX_MAXBOARDS DVB_MAX_ADAPTERS  

 
#define EM28XX_NUM_FRAMES 5
 
#define EM28XX_NUM_READ_FRAMES 2

 
#define EM28XX_NUM_BUFS 5
#define EM28XX_DVB_NUM_BUFS 5

 
#define NUM_I2C_BUSES	2

 
#define EM28XX_NUM_ISOC_PACKETS 64
#define EM28XX_DVB_NUM_ISOC_PACKETS 64

 
#define EM28XX_BULK_PACKET_MULTIPLIER 384
#define EM28XX_DVB_BULK_PACKET_MULTIPLIER 94

#define EM28XX_INTERLACED_DEFAULT 1

 
#define EM28XX_AC97_XFER_TIMEOUT	100

 
#define EM28XX_NUM_BUTTON_ADDRESSES_MAX		5

#define PRIMARY_TS	0
#define SECONDARY_TS	1

enum em28xx_mode {
	EM28XX_SUSPEND,
	EM28XX_ANALOG_MODE,
	EM28XX_DIGITAL_MODE,
};

struct em28xx;

 
struct em28xx_usb_bufs {
	int				max_pkt_size;
	int				num_packets;
	int				num_bufs;
	struct urb			**urb;
	char				**buf;
};

 
struct em28xx_usb_ctl {
	struct em28xx_usb_bufs		analog_bufs;
	struct em28xx_usb_bufs		digital_bufs;
	struct em28xx_buffer	*vid_buf;
	struct em28xx_buffer	*vbi_buf;
	int (*urb_data_copy)(struct em28xx *dev, struct urb *urb);
};

 
struct em28xx_fmt {
	u32	fourcc;
	int	depth;
	int	reg;
};

 
struct em28xx_buffer {
	struct vb2_v4l2_buffer	vb;		 

	struct list_head	list;

	void			*mem;
	unsigned int		length;
	int			top_field;

	unsigned int		pos;

	char			*vb_buf;
};

struct em28xx_dmaqueue {
	struct list_head       active;

	wait_queue_head_t          wq;
};

 

#define MAX_EM28XX_INPUT 4
enum enum28xx_itype {
	EM28XX_VMUX_COMPOSITE = 1,
	EM28XX_VMUX_SVIDEO,
	EM28XX_VMUX_TELEVISION,
	EM28XX_RADIO,
};

enum em28xx_ac97_mode {
	EM28XX_NO_AC97 = 0,
	EM28XX_AC97_EM202,
	EM28XX_AC97_SIGMATEL,
	EM28XX_AC97_OTHER,
};

struct em28xx_audio_mode {
	enum em28xx_ac97_mode ac97;
};

enum em28xx_int_audio_type {
	EM28XX_INT_AUDIO_NONE = 0,
	EM28XX_INT_AUDIO_AC97,
	EM28XX_INT_AUDIO_I2S,
};

enum em28xx_usb_audio_type {
	EM28XX_USB_AUDIO_NONE = 0,
	EM28XX_USB_AUDIO_CLASS,
	EM28XX_USB_AUDIO_VENDOR,
};

 
enum em28xx_amux {
	EM28XX_AMUX_UNUSED = -1,
	EM28XX_AMUX_VIDEO = 0,
	EM28XX_AMUX_LINE_IN,

	 
	EM28XX_AMUX_VIDEO2,
	EM28XX_AMUX_PHONE,
	EM28XX_AMUX_MIC,
	EM28XX_AMUX_CD,
	EM28XX_AMUX_AUX,
	EM28XX_AMUX_PCM_OUT,
};

enum em28xx_aout {
	 
	EM28XX_AOUT_MASTER = BIT(0),
	EM28XX_AOUT_LINE   = BIT(1),
	EM28XX_AOUT_MONO   = BIT(2),
	EM28XX_AOUT_LFE    = BIT(3),
	EM28XX_AOUT_SURR   = BIT(4),

	 
	EM28XX_AOUT_PCM_IN = BIT(7),

	 
	EM28XX_AOUT_PCM_MIC_PCM = 0 << 8,
	EM28XX_AOUT_PCM_CD	= 1 << 8,
	EM28XX_AOUT_PCM_VIDEO	= 2 << 8,
	EM28XX_AOUT_PCM_AUX	= 3 << 8,
	EM28XX_AOUT_PCM_LINE	= 4 << 8,
	EM28XX_AOUT_PCM_STEREO	= 5 << 8,
	EM28XX_AOUT_PCM_MONO	= 6 << 8,
	EM28XX_AOUT_PCM_PHONE	= 7 << 8,
};

static inline int ac97_return_record_select(int a_out)
{
	return (a_out & 0x700) >> 8;
}

struct em28xx_reg_seq {
	int reg;
	unsigned char val, mask;
	int sleep;
};

struct em28xx_input {
	enum enum28xx_itype type;
	unsigned int vmux;
	enum em28xx_amux amux;
	enum em28xx_aout aout;
	const struct em28xx_reg_seq *gpio;
};

#define INPUT(nr) (&em28xx_boards[dev->model].input[nr])

enum em28xx_decoder {
	EM28XX_NODECODER = 0,
	EM28XX_TVP5150,
	EM28XX_SAA711X,
};

enum em28xx_sensor {
	EM28XX_NOSENSOR = 0,
	EM28XX_MT9V011,
	EM28XX_MT9M001,
	EM28XX_MT9M111,
	EM28XX_OV2640,
};

enum em28xx_adecoder {
	EM28XX_NOADECODER = 0,
	EM28XX_TVAUDIO,
};

enum em28xx_led_role {
	EM28XX_LED_ANALOG_CAPTURING = 0,
	EM28XX_LED_DIGITAL_CAPTURING,
	EM28XX_LED_DIGITAL_CAPTURING_TS2,
	EM28XX_LED_ILLUMINATION,
	EM28XX_NUM_LED_ROLES,  
};

struct em28xx_led {
	enum em28xx_led_role role;
	u8 gpio_reg;
	u8 gpio_mask;
	bool inverted;
};

enum em28xx_button_role {
	EM28XX_BUTTON_SNAPSHOT = 0,
	EM28XX_BUTTON_ILLUMINATION,
	EM28XX_NUM_BUTTON_ROLES,  
};

struct em28xx_button {
	enum em28xx_button_role role;
	u8 reg_r;
	u8 reg_clearing;
	u8 mask;
	bool inverted;
};

struct em28xx_board {
	char *name;
	int vchannels;
	int tuner_type;
	int tuner_addr;
	unsigned int def_i2c_bus;	 

	 
	unsigned int tda9887_conf;

	 
	const struct em28xx_reg_seq *dvb_gpio;
	const struct em28xx_reg_seq *suspend_gpio;
	const struct em28xx_reg_seq *tuner_gpio;
	const struct em28xx_reg_seq *mute_gpio;

	unsigned int is_em2800:1;
	unsigned int has_msp34xx:1;
	unsigned int mts_firmware:1;
	unsigned int max_range_640_480:1;
	unsigned int has_dvb:1;
	unsigned int has_dual_ts:1;
	unsigned int is_webcam:1;
	unsigned int valid:1;
	unsigned int has_ir_i2c:1;

	unsigned char xclk, i2c_speed;
	unsigned char radio_addr;
	unsigned short tvaudio_addr;

	enum em28xx_decoder decoder;
	enum em28xx_adecoder adecoder;

	struct em28xx_input       input[MAX_EM28XX_INPUT];
	struct em28xx_input	  radio;
	char			  *ir_codes;

	 
	struct em28xx_led	  *leds;

	 
	const struct em28xx_button *buttons;
};

struct em28xx_eeprom {
	u8 id[4];			 
	__le16 vendor_ID;
	__le16 product_ID;

	__le16 chip_conf;

	__le16 board_conf;

	__le16 string1, string2, string3;

	u8 string_idx_table;
};

#define EM28XX_CAPTURE_STREAM_EN 1

 
#define EM28XX_AUDIO   0x10
#define EM28XX_DVB     0x20
#define EM28XX_RC      0x30
#define EM28XX_V4L2    0x40

 
#define EM28XX_RESOURCE_VIDEO 0x01
#define EM28XX_RESOURCE_VBI   0x02

struct em28xx_v4l2 {
	struct kref ref;
	struct em28xx *dev;

	struct v4l2_device v4l2_dev;
	struct v4l2_ctrl_handler ctrl_handler;

	struct video_device vdev;
	struct video_device vbi_dev;
	struct video_device radio_dev;

	 
	struct vb2_queue vb_vidq;
	struct vb2_queue vb_vbiq;
	struct mutex vb_queue_lock;	 
	struct mutex vb_vbi_queue_lock;	 

	u8 vinmode;
	u8 vinctl;

	 
	int sensor_xres;
	int sensor_yres;
	int sensor_xtal;

	int users;		 
	int streaming_users;     

	u32 frequency;		 

	struct em28xx_fmt *format;
	v4l2_std_id norm;	 

	 
	bool progressive;
	int interlaced_fieldmode;  
	 

	 
	int width;		 
	int height;		 
	unsigned int hscale;	 
	unsigned int vscale;	 
	unsigned int vbi_width;
	unsigned int vbi_height;  

	 
	int capture_type;
	bool top_field;
	int vbi_read;
	unsigned int field_count;

#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_pad video_pad, vbi_pad;
	struct media_entity *decoder;
#endif
};

struct em28xx_audio {
	char name[50];
	unsigned int num_urb;
	char **transfer_buffer;
	struct urb **urb;
	struct usb_device *udev;
	unsigned int capture_transfer_done;
	struct snd_pcm_substream   *capture_pcm_substream;

	unsigned int hwptr_done_capture;
	struct snd_card            *sndcard;

	size_t period;

	int users;
	spinlock_t slock;		 

	 
	struct work_struct wq_trigger;	 
	atomic_t       stream_started;	 
};

enum em28xx_i2c_algo_type {
	EM28XX_I2C_ALGO_EM28XX = 0,
	EM28XX_I2C_ALGO_EM2800,
	EM28XX_I2C_ALGO_EM25XX_BUS_B,
};

struct em28xx_i2c_bus {
	struct em28xx *dev;

	unsigned int bus;
	enum em28xx_i2c_algo_type algo_type;
};

 
struct em28xx {
	struct kref ref;

	
	struct em28xx_v4l2 *v4l2;
	struct em28xx_dvb *dvb;
	struct em28xx_audio adev;
	struct em28xx_IR *ir;

	
	int model;		
	int devno;		
	enum em28xx_chip_id chip_id;

	unsigned int is_em25xx:1;	
	unsigned int disconnected:1;	
	unsigned int has_video:1;
	unsigned int is_audio_only:1;
	unsigned int is_webcam:1;
	unsigned int has_msp34xx:1;
	unsigned int i2c_speed:2;
	enum em28xx_int_audio_type int_audio_type;
	enum em28xx_usb_audio_type usb_audio_type;
	unsigned char name[32];

	struct em28xx_board board;

	enum em28xx_sensor em28xx_sensor;	

	
	unsigned int wait_after_write;

	struct list_head	devlist;

	u32 i2s_speed;		

	struct em28xx_audio_mode audio_mode;

	int tuner_type;		

	
	struct i2c_adapter i2c_adap[NUM_I2C_BUSES];
	struct i2c_client i2c_client[NUM_I2C_BUSES];
	struct em28xx_i2c_bus i2c_bus[NUM_I2C_BUSES];

	unsigned char eeprom_addrwidth_16bit:1;
	unsigned int def_i2c_bus;	
	unsigned int cur_i2c_bus;	
	struct rt_mutex i2c_bus_lock;

	
	unsigned int ctl_input;	
	unsigned int ctl_ainput;
	unsigned int ctl_aoutput;
	enum em28xx_amux amux_map[MAX_EM28XX_INPUT];

	int mute;
	int volume;

	unsigned long hash;	
	unsigned long i2c_hash;	
				

	struct work_struct         request_module_wk;

	
	struct mutex lock;		 
	struct mutex ctrl_urb_lock;	 

	
	unsigned int resources;

	
	u8 *eedata;
	u16 eedata_len;

	
	struct em28xx_dmaqueue vidq;
	struct em28xx_dmaqueue vbiq;
	struct em28xx_usb_ctl usb_ctl;

	spinlock_t slock;  

	
	struct usb_interface *intf;	
	u8 ifnum;		
	u8 analog_ep_isoc;	
	u8 analog_ep_bulk;	
	u8 dvb_ep_isoc_ts2;	
	u8 dvb_ep_bulk_ts2;	
	u8 dvb_ep_isoc;		
	u8 dvb_ep_bulk;		
	int alt;		
	int max_pkt_size;	
	int packet_multiplier;	
				
	int num_alt;		
	unsigned int *alt_max_pkt_size_isoc; 
	unsigned int analog_xfer_bulk:1;	
						
	int dvb_alt_isoc;	
	unsigned int dvb_max_pkt_size_isoc;	
						
	unsigned int dvb_max_pkt_size_isoc_ts2;	
						
	unsigned int dvb_xfer_bulk:1;		
						
	char urb_buf[URB_MAX_CTRL_SIZE];	

	
	int (*em28xx_write_regs)(struct em28xx *dev, u16 reg,
				 char *buf, int len);
	int (*em28xx_read_reg)(struct em28xx *dev, u16 reg);
	int (*em28xx_read_reg_req_len)(struct em28xx *dev, u8 req, u16 reg,
				       char *buf, int len);
	int (*em28xx_write_regs_req)(struct em28xx *dev, u8 req, u16 reg,
				     char *buf, int len);
	int (*em28xx_read_reg_req)(struct em28xx *dev, u8 req, u16 reg);

	enum em28xx_mode mode;

	
	struct delayed_work buttons_query_work;
	u8 button_polling_addresses[EM28XX_NUM_BUTTON_ADDRESSES_MAX];
	u8 button_polling_last_values[EM28XX_NUM_BUTTON_ADDRESSES_MAX];
	u8 num_button_polling_addresses;
	u16 button_polling_interval; 
	
	char snapshot_button_path[30];	
	struct input_dev *sbutton_input_dev;

#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_device *media_dev;
	struct media_entity input_ent[MAX_EM28XX_INPUT];
	struct media_pad input_pad[MAX_EM28XX_INPUT];
#endif

	struct em28xx	*dev_next;
	int ts;
};

#define kref_to_dev(d) container_of(d, struct em28xx, ref)

struct em28xx_ops {
	struct list_head next;
	char *name;
	int id;
	int (*init)(struct em28xx *dev);
	int (*fini)(struct em28xx *dev);
	int (*suspend)(struct em28xx *dev);
	int (*resume)(struct em28xx *dev);
};

 
void em28xx_do_i2c_scan(struct em28xx *dev, unsigned int bus);
int  em28xx_i2c_register(struct em28xx *dev, unsigned int bus,
			 enum em28xx_i2c_algo_type algo_type);
int  em28xx_i2c_unregister(struct em28xx *dev, unsigned int bus);

 
int em28xx_read_reg_req_len(struct em28xx *dev, u8 req, u16 reg,
			    char *buf, int len);
int em28xx_read_reg_req(struct em28xx *dev, u8 req, u16 reg);
int em28xx_read_reg(struct em28xx *dev, u16 reg);
int em28xx_write_regs_req(struct em28xx *dev, u8 req, u16 reg, char *buf,
			  int len);
int em28xx_write_regs(struct em28xx *dev, u16 reg, char *buf, int len);
int em28xx_write_reg(struct em28xx *dev, u16 reg, u8 val);
int em28xx_write_reg_bits(struct em28xx *dev, u16 reg, u8 val,
			  u8 bitmask);
int em28xx_toggle_reg_bits(struct em28xx *dev, u16 reg, u8 bitmask);

int em28xx_read_ac97(struct em28xx *dev, u8 reg);
int em28xx_write_ac97(struct em28xx *dev, u8 reg, u16 val);

int em28xx_audio_analog_set(struct em28xx *dev);
int em28xx_audio_setup(struct em28xx *dev);

const struct em28xx_led *em28xx_find_led(struct em28xx *dev,
					 enum em28xx_led_role role);
int em28xx_capture_start(struct em28xx *dev, int start);
int em28xx_alloc_urbs(struct em28xx *dev, enum em28xx_mode mode, int xfer_bulk,
		      int num_bufs, int max_pkt_size, int packet_multiplier);
int em28xx_init_usb_xfer(struct em28xx *dev, enum em28xx_mode mode,
			 int xfer_bulk,
			 int num_bufs, int max_pkt_size, int packet_multiplier,
			 int (*urb_data_copy)
					(struct em28xx *dev, struct urb *urb));
void em28xx_uninit_usb_xfer(struct em28xx *dev, enum em28xx_mode mode);
void em28xx_stop_urbs(struct em28xx *dev);
int em28xx_set_mode(struct em28xx *dev, enum em28xx_mode set_mode);
int em28xx_gpio_set(struct em28xx *dev, const struct em28xx_reg_seq *gpio);
int em28xx_register_extension(struct em28xx_ops *dev);
void em28xx_unregister_extension(struct em28xx_ops *dev);
void em28xx_init_extension(struct em28xx *dev);
void em28xx_close_extension(struct em28xx *dev);
int em28xx_suspend_extension(struct em28xx *dev);
int em28xx_resume_extension(struct em28xx *dev);

 
extern const struct em28xx_board em28xx_boards[];
extern struct usb_device_id em28xx_id_table[];
int em28xx_tuner_callback(void *ptr, int component, int command, int arg);
void em28xx_setup_xc3028(struct em28xx *dev, struct xc2028_ctrl *ctl);
void em28xx_free_device(struct kref *ref);

 
int em28xx_detect_sensor(struct em28xx *dev);
int em28xx_init_camera(struct em28xx *dev);

#endif
