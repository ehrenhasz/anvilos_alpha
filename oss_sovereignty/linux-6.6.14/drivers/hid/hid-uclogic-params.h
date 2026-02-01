 
 

 

#ifndef _HID_UCLOGIC_PARAMS_H
#define _HID_UCLOGIC_PARAMS_H

#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/list.h>

#define UCLOGIC_MOUSE_FRAME_QUIRK	BIT(0)
#define UCLOGIC_BATTERY_QUIRK		BIT(1)

 
enum uclogic_params_pen_inrange {
	 
	UCLOGIC_PARAMS_PEN_INRANGE_NORMAL = 0,
	 
	UCLOGIC_PARAMS_PEN_INRANGE_INVERTED,
	 
	UCLOGIC_PARAMS_PEN_INRANGE_NONE,
};

 
enum uclogic_params_frame_type {
	 
	UCLOGIC_PARAMS_FRAME_BUTTONS = 0,
	 
	UCLOGIC_PARAMS_FRAME_DIAL,
	 
	UCLOGIC_PARAMS_FRAME_MOUSE,
};

 
struct uclogic_params_pen_subreport {
	 
	__u8 value;

	 
	__u8 id;
};

 
struct uclogic_params_pen {
	 
	bool usage_invalid;
	 
	__u8 *desc_ptr;
	 
	unsigned int desc_size;
	 
	unsigned int id;
	 
	struct uclogic_params_pen_subreport subreport_list[3];
	 
	enum uclogic_params_pen_inrange inrange;
	 
	bool fragmented_hires;
	 
	bool tilt_y_flipped;
};

 
struct uclogic_params_frame {
	 
	__u8 *desc_ptr;
	 
	unsigned int desc_size;
	 
	unsigned int id;
	 
	const char *suffix;
	 
	unsigned int re_lsb;
	 
	unsigned int dev_id_byte;
	 
	unsigned int touch_byte;
	 
	__s8 touch_flip_at;
	 
	__s8 touch_max;
	 
	unsigned int bitmap_dial_byte;
};

 
struct uclogic_raw_event_hook {
	struct hid_device *hdev;
	__u8 *event;
	size_t size;
	struct work_struct work;
	struct list_head list;
};

 
struct uclogic_params {
	 
	bool invalid;
	 
	__u8 *desc_ptr;
	 
	unsigned int desc_size;
	 
	struct uclogic_params_pen pen;
	 
	struct uclogic_params_frame frame_list[3];
	 
	struct uclogic_raw_event_hook *event_hooks;
};

 
struct uclogic_drvdata {
	 
	struct uclogic_params params;
	 
	__u8 *desc_ptr;
	 
	unsigned int desc_size;
	 
	struct input_dev *pen_input;
	 
	struct timer_list inrange_timer;
	 
	u8 re_state;
	 
	unsigned long quirks;
};

 
extern int uclogic_params_init(struct uclogic_params *params,
				struct hid_device *hdev);

 
extern int uclogic_params_get_desc(const struct uclogic_params *params,
					__u8 **pdesc,
					unsigned int *psize);

 
extern void uclogic_params_cleanup(struct uclogic_params *params);

 
extern void uclogic_params_hid_dbg(const struct hid_device *hdev,
					const struct uclogic_params *params);

#endif  
