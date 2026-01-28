#ifndef _HID_UCLOGIC_RDESC_H
#define _HID_UCLOGIC_RDESC_H
#include <linux/usb.h>
#define UCLOGIC_RDESC_WPXXXXU_ORIG_SIZE		212
extern __u8 uclogic_rdesc_wp4030u_fixed_arr[];
extern const size_t uclogic_rdesc_wp4030u_fixed_size;
extern __u8 uclogic_rdesc_wp5540u_fixed_arr[];
extern const size_t uclogic_rdesc_wp5540u_fixed_size;
extern __u8 uclogic_rdesc_wp8060u_fixed_arr[];
extern const size_t uclogic_rdesc_wp8060u_fixed_size;
#define UCLOGIC_RDESC_WP5540U_V2_ORIG_SIZE	232
#define UCLOGIC_RDESC_WP1062_ORIG_SIZE		254
extern __u8 uclogic_rdesc_wp1062_fixed_arr[];
extern const size_t uclogic_rdesc_wp1062_fixed_size;
#define UCLOGIC_RDESC_PF1209_ORIG_SIZE		234
extern __u8 uclogic_rdesc_pf1209_fixed_arr[];
extern const size_t uclogic_rdesc_pf1209_fixed_size;
#define UCLOGIC_RDESC_TWHL850_ORIG0_SIZE	182
#define UCLOGIC_RDESC_TWHL850_ORIG1_SIZE	161
#define UCLOGIC_RDESC_TWHL850_ORIG2_SIZE	92
extern __u8 uclogic_rdesc_twhl850_fixed0_arr[];
extern const size_t uclogic_rdesc_twhl850_fixed0_size;
extern __u8 uclogic_rdesc_twhl850_fixed1_arr[];
extern const size_t uclogic_rdesc_twhl850_fixed1_size;
extern __u8 uclogic_rdesc_twhl850_fixed2_arr[];
extern const size_t uclogic_rdesc_twhl850_fixed2_size;
#define UCLOGIC_RDESC_TWHA60_ORIG0_SIZE		254
#define UCLOGIC_RDESC_TWHA60_ORIG1_SIZE		139
extern __u8 uclogic_rdesc_twha60_fixed0_arr[];
extern const size_t uclogic_rdesc_twha60_fixed0_size;
extern __u8 uclogic_rdesc_twha60_fixed1_arr[];
extern const size_t uclogic_rdesc_twha60_fixed1_size;
#define UCLOGIC_RDESC_PEN_PH_HEAD	0xFE, 0xED, 0x1D
#define UCLOGIC_RDESC_FRAME_PH_BTN_HEAD	0xFE, 0xED
extern __u8 *uclogic_rdesc_template_apply(const __u8 *template_ptr,
					  size_t template_size,
					  const s32 *param_list,
					  size_t param_num);
enum uclogic_rdesc_ph_id {
	UCLOGIC_RDESC_PEN_PH_ID_X_LM,
	UCLOGIC_RDESC_PEN_PH_ID_X_PM,
	UCLOGIC_RDESC_PEN_PH_ID_Y_LM,
	UCLOGIC_RDESC_PEN_PH_ID_Y_PM,
	UCLOGIC_RDESC_PEN_PH_ID_PRESSURE_LM,
	UCLOGIC_RDESC_FRAME_PH_ID_UM,
	UCLOGIC_RDESC_PH_ID_NUM
};
#define UCLOGIC_RDESC_PEN_PH(_ID) \
	UCLOGIC_RDESC_PEN_PH_HEAD, UCLOGIC_RDESC_PEN_PH_ID_##_ID
#define UCLOGIC_RDESC_FRAME_PH_BTN \
	UCLOGIC_RDESC_FRAME_PH_BTN_HEAD, UCLOGIC_RDESC_FRAME_PH_ID_UM
#define UCLOGIC_RDESC_V1_PEN_ID	0x07
extern const __u8 uclogic_rdesc_v1_pen_template_arr[];
extern const size_t uclogic_rdesc_v1_pen_template_size;
#define UCLOGIC_RDESC_V2_PEN_ID	0x08
extern const __u8 uclogic_rdesc_v2_pen_template_arr[];
extern const size_t uclogic_rdesc_v2_pen_template_size;
#define UCLOGIC_RDESC_V1_FRAME_ID 0xf7
extern const __u8 uclogic_rdesc_v1_frame_arr[];
extern const size_t uclogic_rdesc_v1_frame_size;
#define UCLOGIC_RDESC_V2_FRAME_BUTTONS_ID 0xf7
extern const __u8 uclogic_rdesc_v2_frame_buttons_arr[];
extern const size_t uclogic_rdesc_v2_frame_buttons_size;
#define UCLOGIC_RDESC_V2_FRAME_TOUCH_ID 0xf8
extern const __u8 uclogic_rdesc_v2_frame_touch_ring_arr[];
extern const size_t uclogic_rdesc_v2_frame_touch_ring_size;
extern const __u8 uclogic_rdesc_v2_frame_touch_strip_arr[];
extern const size_t uclogic_rdesc_v2_frame_touch_strip_size;
#define UCLOGIC_RDESC_V2_FRAME_TOUCH_DEV_ID_BYTE	0x4
#define UCLOGIC_RDESC_V2_FRAME_DIAL_ID 0xf9
extern const __u8 uclogic_rdesc_v2_frame_dial_arr[];
extern const size_t uclogic_rdesc_v2_frame_dial_size;
#define UCLOGIC_RDESC_V2_FRAME_DIAL_DEV_ID_BYTE	0x4
#define UCLOGIC_RDESC_UGEE_V2_BATTERY_ID 0xba
extern const __u8 uclogic_ugee_v2_probe_arr[];
extern const size_t uclogic_ugee_v2_probe_size;
extern const int uclogic_ugee_v2_probe_endpoint;
extern const __u8 uclogic_rdesc_ugee_v2_pen_template_arr[];
extern const size_t uclogic_rdesc_ugee_v2_pen_template_size;
extern const __u8 uclogic_rdesc_ugee_v2_frame_btn_template_arr[];
extern const size_t uclogic_rdesc_ugee_v2_frame_btn_template_size;
extern const __u8 uclogic_rdesc_ugee_v2_frame_dial_template_arr[];
extern const size_t uclogic_rdesc_ugee_v2_frame_dial_template_size;
extern const __u8 uclogic_rdesc_ugee_v2_frame_mouse_template_arr[];
extern const size_t uclogic_rdesc_ugee_v2_frame_mouse_template_size;
extern const __u8 uclogic_rdesc_ugee_v2_battery_template_arr[];
extern const size_t uclogic_rdesc_ugee_v2_battery_template_size;
extern const __u8 uclogic_rdesc_ugee_ex07_frame_arr[];
extern const size_t uclogic_rdesc_ugee_ex07_frame_size;
extern const __u8 uclogic_rdesc_xppen_deco01_frame_arr[];
extern const size_t uclogic_rdesc_xppen_deco01_frame_size;
extern const __u8 uclogic_rdesc_ugee_g5_frame_arr[];
extern const size_t uclogic_rdesc_ugee_g5_frame_size;
#define UCLOGIC_RDESC_UGEE_G5_FRAME_ID 0x06
#define UCLOGIC_RDESC_UGEE_G5_FRAME_DEV_ID_BYTE	0x2
#define UCLOGIC_RDESC_UGEE_G5_FRAME_RE_LSB 38
#endif  
