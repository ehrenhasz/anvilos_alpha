 
 
#ifndef __PVRUSB2_DEVATTR_H
#define __PVRUSB2_DEVATTR_H

#include <linux/mod_devicetable.h>
#include <linux/videodev2.h>
#ifdef CONFIG_VIDEO_PVRUSB2_DVB
#include "pvrusb2-dvb.h"
#endif

 


#define PVR2_CLIENT_ID_NULL 0
#define PVR2_CLIENT_ID_MSP3400 1
#define PVR2_CLIENT_ID_CX25840 2
#define PVR2_CLIENT_ID_SAA7115 3
#define PVR2_CLIENT_ID_TUNER 4
#define PVR2_CLIENT_ID_CS53L32A 5
#define PVR2_CLIENT_ID_WM8775 6
#define PVR2_CLIENT_ID_DEMOD 7

struct pvr2_device_client_desc {
	 
	unsigned char module_id;

	 
	unsigned char *i2c_address_list;
};

struct pvr2_device_client_table {
	const struct pvr2_device_client_desc *lst;
	unsigned char cnt;
};


struct pvr2_string_table {
	const char **lst;
	unsigned int cnt;
};

#define PVR2_ROUTING_SCHEME_HAUPPAUGE 0
#define PVR2_ROUTING_SCHEME_GOTVIEW 1
#define PVR2_ROUTING_SCHEME_ONAIR 2
#define PVR2_ROUTING_SCHEME_AV400 3
#define PVR2_ROUTING_SCHEME_HAUP160XXX 4

#define PVR2_DIGITAL_SCHEME_NONE 0
#define PVR2_DIGITAL_SCHEME_HAUPPAUGE 1
#define PVR2_DIGITAL_SCHEME_ONAIR 2

#define PVR2_LED_SCHEME_NONE 0
#define PVR2_LED_SCHEME_HAUPPAUGE 1

#define PVR2_IR_SCHEME_NONE 0
#define PVR2_IR_SCHEME_24XXX 1  
#define PVR2_IR_SCHEME_ZILOG 2  
#define PVR2_IR_SCHEME_24XXX_MCE 3  
#define PVR2_IR_SCHEME_29XXX 4  

 
struct pvr2_device_desc {
	 
	const char *description;

	 
	const char *shortname;

	 
	struct pvr2_string_table client_modules;

	 
	struct pvr2_device_client_table client_table;

	 
	struct pvr2_string_table fx2_firmware;

#ifdef CONFIG_VIDEO_PVRUSB2_DVB
	 
	const struct pvr2_dvb_props *dvb_props;

#endif
	 
	v4l2_std_id default_std_mask;

	 
	int default_tuner_type;

	 
	unsigned char signal_routing_scheme;

	 
	unsigned char led_scheme;

	 
	unsigned char digital_control_scheme;

	 
	unsigned int flag_skip_cx23416_firmware:1;

	 
	unsigned int flag_digital_requires_cx23416:1;

	 
	unsigned int flag_has_hauppauge_rom:1;

	 
	unsigned int flag_no_powerup:1;

	 
	unsigned int flag_has_cx25840:1;

	 
	unsigned int flag_has_wm8775:1;

	 
	unsigned int ir_scheme:3;

	 
	unsigned int flag_has_fmradio:1;        
	unsigned int flag_has_analogtuner:1;    
	unsigned int flag_has_composite:1;      
	unsigned int flag_has_svideo:1;         
	unsigned int flag_fx2_16kb:1;           

	 
	unsigned int flag_is_experimental:1;
};

extern struct usb_device_id pvr2_device_table[];

#endif  
