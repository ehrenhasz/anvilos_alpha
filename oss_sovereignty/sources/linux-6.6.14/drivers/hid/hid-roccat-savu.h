
#ifndef __HID_ROCCAT_SAVU_H
#define __HID_ROCCAT_SAVU_H





#include <linux/types.h>

struct savu_mouse_report_special {
	uint8_t report_number; 
	uint8_t zero;
	uint8_t type;
	uint8_t data[2];
} __packed;

enum {
	SAVU_MOUSE_REPORT_NUMBER_SPECIAL = 3,
};

enum savu_mouse_report_button_types {
	
	SAVU_MOUSE_REPORT_BUTTON_TYPE_PROFILE = 0x20,

	
	SAVU_MOUSE_REPORT_BUTTON_TYPE_QUICKLAUNCH = 0x60,

	
	SAVU_MOUSE_REPORT_BUTTON_TYPE_TIMER = 0x80,

	
	SAVU_MOUSE_REPORT_BUTTON_TYPE_CPI = 0xb0,

	
	SAVU_MOUSE_REPORT_BUTTON_TYPE_SENSITIVITY = 0xc0,

	
	SAVU_MOUSE_REPORT_BUTTON_TYPE_MULTIMEDIA = 0xf0,
};

struct savu_roccat_report {
	uint8_t type;
	uint8_t data[2];
} __packed;

#endif
