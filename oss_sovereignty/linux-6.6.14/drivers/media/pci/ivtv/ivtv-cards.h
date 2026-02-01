 
 

#ifndef IVTV_CARDS_H
#define IVTV_CARDS_H

 
#define IVTV_CARD_PVR_250	      0	 
#define IVTV_CARD_PVR_350	      1	 
#define IVTV_CARD_PVR_150	      2	 
#define IVTV_CARD_M179		      3	 
#define IVTV_CARD_MPG600	      4	 
#define IVTV_CARD_MPG160	      5	 
#define IVTV_CARD_PG600		      6	 
#define IVTV_CARD_AVC2410	      7	 
#define IVTV_CARD_AVC2010	      8	 
#define IVTV_CARD_TG5000TV	      9  
#define IVTV_CARD_VA2000MAX_SNT6     10  
#define IVTV_CARD_CX23416GYC	     11  
#define IVTV_CARD_GV_MVPRX	     12  
#define IVTV_CARD_GV_MVPRX2E	     13  
#define IVTV_CARD_GOTVIEW_PCI_DVD    14	 
#define IVTV_CARD_GOTVIEW_PCI_DVD2   15	 
#define IVTV_CARD_YUAN_MPC622        16	 
#define IVTV_CARD_DCTMTVP1	     17  
#define IVTV_CARD_PG600V2	     18  
#define IVTV_CARD_CLUB3D	     19  
#define IVTV_CARD_AVERTV_MCE116	     20  
#define IVTV_CARD_ASUS_FALCON2	     21  
#define IVTV_CARD_AVER_PVR150PLUS    22  
#define IVTV_CARD_AVER_EZMAKER       23  
#define IVTV_CARD_AVER_M104          24  
#define IVTV_CARD_BUFFALO_MV5L       25  
#define IVTV_CARD_AVER_ULTRA1500MCE  26  
#define IVTV_CARD_KIKYOU             27  
#define IVTV_CARD_LAST		     27

 

 
#define IVTV_CARD_PVR_350_V1	     (IVTV_CARD_LAST+1)
 
#define IVTV_CARD_CX23416GYC_NOGR    (IVTV_CARD_LAST+2)
#define IVTV_CARD_CX23416GYC_NOGRYCS (IVTV_CARD_LAST+3)

 
#define PCI_VENDOR_ID_ICOMP  0x4444
#define PCI_DEVICE_ID_IVTV15 0x0803
#define PCI_DEVICE_ID_IVTV16 0x0016

 
#define IVTV_PCI_ID_HAUPPAUGE		0x0070
#define IVTV_PCI_ID_HAUPPAUGE_ALT1	0x0270
#define IVTV_PCI_ID_HAUPPAUGE_ALT2	0x4070
#define IVTV_PCI_ID_ADAPTEC		0x9005
#define IVTV_PCI_ID_ASUSTEK		0x1043
#define IVTV_PCI_ID_AVERMEDIA		0x1461
#define IVTV_PCI_ID_YUAN1		0x12ab
#define IVTV_PCI_ID_YUAN2		0xff01
#define IVTV_PCI_ID_YUAN3		0xffab
#define IVTV_PCI_ID_YUAN4		0xfbab
#define IVTV_PCI_ID_DIAMONDMM		0xff92
#define IVTV_PCI_ID_IODATA		0x10fc
#define IVTV_PCI_ID_MELCO		0x1154
#define IVTV_PCI_ID_GOTVIEW1		0xffac
#define IVTV_PCI_ID_GOTVIEW2		0xffad
#define IVTV_PCI_ID_SONY		0x104d

 
enum ivtv_hw_bits {
	IVTV_HW_BIT_CX25840,
	IVTV_HW_BIT_SAA7115,
	IVTV_HW_BIT_SAA7127,
	IVTV_HW_BIT_MSP34XX,
	IVTV_HW_BIT_TUNER,
	IVTV_HW_BIT_WM8775,
	IVTV_HW_BIT_CS53L32A,
	IVTV_HW_BIT_TVEEPROM,
	IVTV_HW_BIT_SAA7114,
	IVTV_HW_BIT_UPD64031A,
	IVTV_HW_BIT_UPD6408X,
	IVTV_HW_BIT_SAA717X,
	IVTV_HW_BIT_WM8739,
	IVTV_HW_BIT_VP27SMPX,
	IVTV_HW_BIT_M52790,
	IVTV_HW_BIT_GPIO,
	IVTV_HW_BIT_I2C_IR_RX_AVER,
	IVTV_HW_BIT_I2C_IR_RX_HAUP_EXT,		  
	IVTV_HW_BIT_I2C_IR_RX_HAUP_INT,
	IVTV_HW_BIT_Z8F0811_IR_HAUP,
	IVTV_HW_BIT_I2C_IR_RX_ADAPTEC,

	IVTV_HW_MAX_BITS	 
};

#define IVTV_HW_CX25840			BIT(IVTV_HW_BIT_CX25840)
#define IVTV_HW_SAA7115			BIT(IVTV_HW_BIT_SAA7115)
#define IVTV_HW_SAA7127			BIT(IVTV_HW_BIT_SAA7127)
#define IVTV_HW_MSP34XX			BIT(IVTV_HW_BIT_MSP34XX)
#define IVTV_HW_TUNER			BIT(IVTV_HW_BIT_TUNER)
#define IVTV_HW_WM8775			BIT(IVTV_HW_BIT_WM8775)
#define IVTV_HW_CS53L32A		BIT(IVTV_HW_BIT_CS53L32A)
#define IVTV_HW_TVEEPROM		BIT(IVTV_HW_BIT_TVEEPROM)
#define IVTV_HW_SAA7114			BIT(IVTV_HW_BIT_SAA7114)
#define IVTV_HW_UPD64031A		BIT(IVTV_HW_BIT_UPD64031A)
#define IVTV_HW_UPD6408X		BIT(IVTV_HW_BIT_UPD6408X)
#define IVTV_HW_SAA717X			BIT(IVTV_HW_BIT_SAA717X)
#define IVTV_HW_WM8739			BIT(IVTV_HW_BIT_WM8739)
#define IVTV_HW_VP27SMPX		BIT(IVTV_HW_BIT_VP27SMPX)
#define IVTV_HW_M52790			BIT(IVTV_HW_BIT_M52790)
#define IVTV_HW_GPIO			BIT(IVTV_HW_BIT_GPIO)
#define IVTV_HW_I2C_IR_RX_AVER		BIT(IVTV_HW_BIT_I2C_IR_RX_AVER)
#define IVTV_HW_I2C_IR_RX_HAUP_EXT	BIT(IVTV_HW_BIT_I2C_IR_RX_HAUP_EXT)
#define IVTV_HW_I2C_IR_RX_HAUP_INT	BIT(IVTV_HW_BIT_I2C_IR_RX_HAUP_INT)
#define IVTV_HW_Z8F0811_IR_HAUP		BIT(IVTV_HW_BIT_Z8F0811_IR_HAUP)
#define IVTV_HW_I2C_IR_RX_ADAPTEC	BIT(IVTV_HW_BIT_I2C_IR_RX_ADAPTEC)

#define IVTV_HW_SAA711X   (IVTV_HW_SAA7115 | IVTV_HW_SAA7114)

#define IVTV_HW_IR_ANY (IVTV_HW_I2C_IR_RX_AVER | \
			IVTV_HW_I2C_IR_RX_HAUP_EXT | \
			IVTV_HW_I2C_IR_RX_HAUP_INT | \
			IVTV_HW_Z8F0811_IR_HAUP | \
			IVTV_HW_I2C_IR_RX_ADAPTEC)

 
#define	IVTV_CARD_INPUT_VID_TUNER	1
#define	IVTV_CARD_INPUT_SVIDEO1		2
#define	IVTV_CARD_INPUT_SVIDEO2		3
#define	IVTV_CARD_INPUT_COMPOSITE1	4
#define	IVTV_CARD_INPUT_COMPOSITE2	5
#define	IVTV_CARD_INPUT_COMPOSITE3	6

 
#define	IVTV_CARD_INPUT_AUD_TUNER	1
#define	IVTV_CARD_INPUT_LINE_IN1	2
#define	IVTV_CARD_INPUT_LINE_IN2	3

#define IVTV_CARD_MAX_VIDEO_INPUTS 6
#define IVTV_CARD_MAX_AUDIO_INPUTS 3
#define IVTV_CARD_MAX_TUNERS	   3

 
#define IVTV_SAA71XX_COMPOSITE0 0
#define IVTV_SAA71XX_COMPOSITE1 1
#define IVTV_SAA71XX_COMPOSITE2 2
#define IVTV_SAA71XX_COMPOSITE3 3
#define IVTV_SAA71XX_COMPOSITE4 4
#define IVTV_SAA71XX_COMPOSITE5 5
#define IVTV_SAA71XX_SVIDEO0    6
#define IVTV_SAA71XX_SVIDEO1    7
#define IVTV_SAA71XX_SVIDEO2    8
#define IVTV_SAA71XX_SVIDEO3    9

 
#define IVTV_SAA717X_TUNER_FLAG 0x80

 
#define IVTV_DUMMY_AUDIO        0

 
#define IVTV_GPIO_TUNER   0
#define IVTV_GPIO_LINE_IN 1

 
#define IVTV_SAA717X_IN0 0
#define IVTV_SAA717X_IN1 1
#define IVTV_SAA717X_IN2 2

 
#define IVTV_CAP_ENCODER (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_TUNER | \
			  V4L2_CAP_AUDIO | V4L2_CAP_READWRITE | V4L2_CAP_VBI_CAPTURE | \
			  V4L2_CAP_SLICED_VBI_CAPTURE)
#define IVTV_CAP_DECODER (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_SLICED_VBI_OUTPUT)

struct ivtv_card_video_input {
	u8  video_type;		 
	u8  audio_index;	 
	u16 video_input;	 
};

struct ivtv_card_audio_input {
	u8  audio_type;		 
	u32 audio_input;	 
	u16 muxer_input;	 
};

struct ivtv_card_output {
	u8  name[32];
	u16 video_output;   
};

struct ivtv_card_pci_info {
	u16 device;
	u16 subsystem_vendor;
	u16 subsystem_device;
};

 

 

struct ivtv_gpio_init {		 
	u16 direction;		 
	u16 initial_value;
};

struct ivtv_gpio_video_input {	 
	u16 mask;		 
	u16 tuner;
	u16 composite;
	u16 svideo;
};

struct ivtv_gpio_audio_input {	 
	u16 mask;		 
	u16 tuner;
	u16 linein;
	u16 radio;
};

struct ivtv_gpio_audio_mute {
	u16 mask;		 
	u16 mute;		 
};

struct ivtv_gpio_audio_mode {
	u16 mask;		 
	u16 mono;		 
	u16 stereo;		 
	u16 lang1;		 
	u16 lang2;		 
	u16 both;		 
};

struct ivtv_gpio_audio_freq {
	u16 mask;		 
	u16 f32000;
	u16 f44100;
	u16 f48000;
};

struct ivtv_gpio_audio_detect {
	u16 mask;		 
	u16 stereo;		 
};

struct ivtv_card_tuner {
	v4l2_std_id std;	 
	int	    tuner;	 
};

struct ivtv_card_tuner_i2c {
	unsigned short radio[2]; 
	unsigned short demod[2]; 
	unsigned short tv[4];	 
};

 
struct ivtv_card {
	int type;
	char *name;
	char *comment;
	u32 v4l2_capabilities;
	u32 hw_video;		 
	u32 hw_audio;		 
	u32 hw_audio_ctrl;	 
	u32 hw_muxer;		 
	u32 hw_all;		 
	struct ivtv_card_video_input video_inputs[IVTV_CARD_MAX_VIDEO_INPUTS];
	struct ivtv_card_audio_input audio_inputs[IVTV_CARD_MAX_AUDIO_INPUTS];
	struct ivtv_card_audio_input radio_input;
	int nof_outputs;
	const struct ivtv_card_output *video_outputs;
	u8 gr_config;		 
	u8 xceive_pin;		 

	 
	struct ivtv_gpio_init		gpio_init;
	struct ivtv_gpio_video_input	gpio_video_input;
	struct ivtv_gpio_audio_input	gpio_audio_input;
	struct ivtv_gpio_audio_mute	gpio_audio_mute;
	struct ivtv_gpio_audio_mode	gpio_audio_mode;
	struct ivtv_gpio_audio_freq	gpio_audio_freq;
	struct ivtv_gpio_audio_detect	gpio_audio_detect;

	struct ivtv_card_tuner tuners[IVTV_CARD_MAX_TUNERS];
	struct ivtv_card_tuner_i2c *i2c;

	 
	const struct ivtv_card_pci_info *pci_list;
};

int ivtv_get_input(struct ivtv *itv, u16 index, struct v4l2_input *input);
int ivtv_get_output(struct ivtv *itv, u16 index, struct v4l2_output *output);
int ivtv_get_audio_input(struct ivtv *itv, u16 index, struct v4l2_audio *input);
int ivtv_get_audio_output(struct ivtv *itv, u16 index, struct v4l2_audioout *output);
const struct ivtv_card *ivtv_get_card(u16 index);

#endif
