
 

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/rmi.h>
#include <linux/slab.h>
#include <linux/of.h>
#include "rmi_driver.h"
#include "rmi_2d_sensor.h"

#define F11_MAX_NUM_OF_FINGERS		10
#define F11_MAX_NUM_OF_TOUCH_SHAPES	16

#define FINGER_STATE_MASK	0x03

#define F11_CTRL_SENSOR_MAX_X_POS_OFFSET	6
#define F11_CTRL_SENSOR_MAX_Y_POS_OFFSET	8

#define DEFAULT_XY_MAX 9999
#define DEFAULT_MAX_ABS_MT_PRESSURE 255
#define DEFAULT_MAX_ABS_MT_TOUCH 15
#define DEFAULT_MAX_ABS_MT_ORIENTATION 1
#define DEFAULT_MIN_ABS_MT_TRACKING_ID 1
#define DEFAULT_MAX_ABS_MT_TRACKING_ID 10

 

 
#define DMAX 10

 
#define RMI_F11_REZERO  0x01

#define RMI_F11_HAS_QUERY9              (1 << 3)
#define RMI_F11_HAS_QUERY11             (1 << 4)
#define RMI_F11_HAS_QUERY12             (1 << 5)
#define RMI_F11_HAS_QUERY27             (1 << 6)
#define RMI_F11_HAS_QUERY28             (1 << 7)

 

#define RMI_F11_NR_FINGERS_MASK 0x07
#define RMI_F11_HAS_REL                 (1 << 3)
#define RMI_F11_HAS_ABS                 (1 << 4)
#define RMI_F11_HAS_GESTURES            (1 << 5)
#define RMI_F11_HAS_SENSITIVITY_ADJ     (1 << 6)
#define RMI_F11_CONFIGURABLE            (1 << 7)

 
#define RMI_F11_NR_ELECTRODES_MASK      0x7F

 

#define RMI_F11_ABS_DATA_SIZE_MASK      0x03
#define RMI_F11_HAS_ANCHORED_FINGER     (1 << 2)
#define RMI_F11_HAS_ADJ_HYST            (1 << 3)
#define RMI_F11_HAS_DRIBBLE             (1 << 4)
#define RMI_F11_HAS_BENDING_CORRECTION  (1 << 5)
#define RMI_F11_HAS_LARGE_OBJECT_SUPPRESSION    (1 << 6)
#define RMI_F11_HAS_JITTER_FILTER       (1 << 7)

 
#define RMI_F11_HAS_SINGLE_TAP                  (1 << 0)
#define RMI_F11_HAS_TAP_AND_HOLD                (1 << 1)
#define RMI_F11_HAS_DOUBLE_TAP                  (1 << 2)
#define RMI_F11_HAS_EARLY_TAP                   (1 << 3)
#define RMI_F11_HAS_FLICK                       (1 << 4)
#define RMI_F11_HAS_PRESS                       (1 << 5)
#define RMI_F11_HAS_PINCH                       (1 << 6)
#define RMI_F11_HAS_CHIRAL                      (1 << 7)

 
#define RMI_F11_HAS_PALM_DET                    (1 << 0)
#define RMI_F11_HAS_ROTATE                      (1 << 1)
#define RMI_F11_HAS_TOUCH_SHAPES                (1 << 2)
#define RMI_F11_HAS_SCROLL_ZONES                (1 << 3)
#define RMI_F11_HAS_INDIVIDUAL_SCROLL_ZONES     (1 << 4)
#define RMI_F11_HAS_MF_SCROLL                   (1 << 5)
#define RMI_F11_HAS_MF_EDGE_MOTION              (1 << 6)
#define RMI_F11_HAS_MF_SCROLL_INERTIA           (1 << 7)

 
#define RMI_F11_HAS_PEN                         (1 << 0)
#define RMI_F11_HAS_PROXIMITY                   (1 << 1)
#define RMI_F11_HAS_PALM_DET_SENSITIVITY        (1 << 2)
#define RMI_F11_HAS_SUPPRESS_ON_PALM_DETECT     (1 << 3)
#define RMI_F11_HAS_TWO_PEN_THRESHOLDS          (1 << 4)
#define RMI_F11_HAS_CONTACT_GEOMETRY            (1 << 5)
#define RMI_F11_HAS_PEN_HOVER_DISCRIMINATION    (1 << 6)
#define RMI_F11_HAS_PEN_FILTERS                 (1 << 7)

 
#define RMI_F11_NR_TOUCH_SHAPES_MASK            0x1F

 

#define RMI_F11_HAS_Z_TUNING                    (1 << 0)
#define RMI_F11_HAS_ALGORITHM_SELECTION         (1 << 1)
#define RMI_F11_HAS_W_TUNING                    (1 << 2)
#define RMI_F11_HAS_PITCH_INFO                  (1 << 3)
#define RMI_F11_HAS_FINGER_SIZE                 (1 << 4)
#define RMI_F11_HAS_SEGMENTATION_AGGRESSIVENESS (1 << 5)
#define RMI_F11_HAS_XY_CLIP                     (1 << 6)
#define RMI_F11_HAS_DRUMMING_FILTER             (1 << 7)

 

#define RMI_F11_HAS_GAPLESS_FINGER              (1 << 0)
#define RMI_F11_HAS_GAPLESS_FINGER_TUNING       (1 << 1)
#define RMI_F11_HAS_8BIT_W                      (1 << 2)
#define RMI_F11_HAS_ADJUSTABLE_MAPPING          (1 << 3)
#define RMI_F11_HAS_INFO2                       (1 << 4)
#define RMI_F11_HAS_PHYSICAL_PROPS              (1 << 5)
#define RMI_F11_HAS_FINGER_LIMIT                (1 << 6)
#define RMI_F11_HAS_LINEAR_COEFF                (1 << 7)

 

#define RMI_F11_JITTER_WINDOW_MASK              0x1F
#define RMI_F11_JITTER_FILTER_MASK              0x60
#define RMI_F11_JITTER_FILTER_SHIFT             5

 
#define RMI_F11_LIGHT_CONTROL_MASK              0x03
#define RMI_F11_IS_CLEAR                        (1 << 2)
#define RMI_F11_CLICKPAD_PROPS_MASK             0x18
#define RMI_F11_CLICKPAD_PROPS_SHIFT            3
#define RMI_F11_MOUSE_BUTTONS_MASK              0x60
#define RMI_F11_MOUSE_BUTTONS_SHIFT             5
#define RMI_F11_HAS_ADVANCED_GESTURES           (1 << 7)

#define RMI_F11_QUERY_SIZE                      4
#define RMI_F11_QUERY_GESTURE_SIZE              2

#define F11_LIGHT_CTL_NONE 0x00
#define F11_LUXPAD	   0x01
#define F11_DUAL_MODE      0x02

#define F11_NOT_CLICKPAD     0x00
#define F11_HINGED_CLICKPAD  0x01
#define F11_UNIFORM_CLICKPAD 0x02

 
struct f11_2d_sensor_queries {
	 
	u8 nr_fingers;
	bool has_rel;
	bool has_abs;
	bool has_gestures;
	bool has_sensitivity_adjust;
	bool configurable;

	 
	u8 nr_x_electrodes;

	 
	u8 nr_y_electrodes;

	 
	u8 max_electrodes;

	 
	u8 abs_data_size;
	bool has_anchored_finger;
	bool has_adj_hyst;
	bool has_dribble;
	bool has_bending_correction;
	bool has_large_object_suppression;
	bool has_jitter_filter;

	u8 f11_2d_query6;

	 
	bool has_single_tap;
	bool has_tap_n_hold;
	bool has_double_tap;
	bool has_early_tap;
	bool has_flick;
	bool has_press;
	bool has_pinch;
	bool has_chiral;

	bool query7_nonzero;

	 
	bool has_palm_det;
	bool has_rotate;
	bool has_touch_shapes;
	bool has_scroll_zones;
	bool has_individual_scroll_zones;
	bool has_mf_scroll;
	bool has_mf_edge_motion;
	bool has_mf_scroll_inertia;

	bool query8_nonzero;

	 
	bool has_pen;
	bool has_proximity;
	bool has_palm_det_sensitivity;
	bool has_suppress_on_palm_detect;
	bool has_two_pen_thresholds;
	bool has_contact_geometry;
	bool has_pen_hover_discrimination;
	bool has_pen_filters;

	 
	u8 nr_touch_shapes;

	 
	bool has_z_tuning;
	bool has_algorithm_selection;
	bool has_w_tuning;
	bool has_pitch_info;
	bool has_finger_size;
	bool has_segmentation_aggressiveness;
	bool has_XY_clip;
	bool has_drumming_filter;

	 
	bool has_gapless_finger;
	bool has_gapless_finger_tuning;
	bool has_8bit_w;
	bool has_adjustable_mapping;
	bool has_info2;
	bool has_physical_props;
	bool has_finger_limit;
	bool has_linear_coeff_2;

	 
	u8 jitter_window_size;
	u8 jitter_filter_type;

	 
	u8 light_control;
	bool is_clear;
	u8 clickpad_props;
	u8 mouse_buttons;
	bool has_advanced_gestures;

	 
	u16 x_sensor_size_mm;
	u16 y_sensor_size_mm;
};

 
#define RMI_F11_REPORT_MODE_MASK        0x07
#define RMI_F11_REPORT_MODE_CONTINUOUS  (0 << 0)
#define RMI_F11_REPORT_MODE_REDUCED     (1 << 0)
#define RMI_F11_REPORT_MODE_FS_CHANGE   (2 << 0)
#define RMI_F11_REPORT_MODE_FP_CHANGE   (3 << 0)
#define RMI_F11_ABS_POS_FILT            (1 << 3)
#define RMI_F11_REL_POS_FILT            (1 << 4)
#define RMI_F11_REL_BALLISTICS          (1 << 5)
#define RMI_F11_DRIBBLE                 (1 << 6)
#define RMI_F11_REPORT_BEYOND_CLIP      (1 << 7)

 
#define RMI_F11_PALM_DETECT_THRESH_MASK 0x0F
#define RMI_F11_MOTION_SENSITIVITY_MASK 0x30
#define RMI_F11_MANUAL_TRACKING         (1 << 6)
#define RMI_F11_MANUAL_TRACKED_FINGER   (1 << 7)

#define RMI_F11_DELTA_X_THRESHOLD       2
#define RMI_F11_DELTA_Y_THRESHOLD       3

#define RMI_F11_CTRL_REG_COUNT          12

struct f11_2d_ctrl {
	u8              ctrl0_11[RMI_F11_CTRL_REG_COUNT];
	u16             ctrl0_11_address;
};

#define RMI_F11_ABS_BYTES 5
#define RMI_F11_REL_BYTES 2

 

#define RMI_F11_SINGLE_TAP              (1 << 0)
#define RMI_F11_TAP_AND_HOLD            (1 << 1)
#define RMI_F11_DOUBLE_TAP              (1 << 2)
#define RMI_F11_EARLY_TAP               (1 << 3)
#define RMI_F11_FLICK                   (1 << 4)
#define RMI_F11_PRESS                   (1 << 5)
#define RMI_F11_PINCH                   (1 << 6)

 

#define RMI_F11_PALM_DETECT                     (1 << 0)
#define RMI_F11_ROTATE                          (1 << 1)
#define RMI_F11_SHAPE                           (1 << 2)
#define RMI_F11_SCROLLZONE                      (1 << 3)
#define RMI_F11_GESTURE_FINGER_COUNT_MASK       0x70

 
struct f11_2d_data {
	u8	*f_state;
	u8	*abs_pos;
	s8	*rel_pos;
	u8	*gest_1;
	u8	*gest_2;
	s8	*pinch;
	u8	*flick;
	u8	*rotate;
	u8	*shapes;
	s8	*multi_scroll;
	s8	*scroll_zones;
};

 
struct f11_data {
	bool has_query9;
	bool has_query11;
	bool has_query12;
	bool has_query27;
	bool has_query28;
	bool has_acm;
	struct f11_2d_ctrl dev_controls;
	struct mutex dev_controls_mutex;
	u16 rezero_wait_ms;
	struct rmi_2d_sensor sensor;
	struct f11_2d_sensor_queries sens_query;
	struct f11_2d_data data;
	struct rmi_2d_sensor_platform_data sensor_pdata;
	unsigned long *abs_mask;
	unsigned long *rel_mask;
};

enum f11_finger_state {
	F11_NO_FINGER	= 0x00,
	F11_PRESENT	= 0x01,
	F11_INACCURATE	= 0x02,
	F11_RESERVED	= 0x03
};

static void rmi_f11_rel_pos_report(struct f11_data *f11, u8 n_finger)
{
	struct rmi_2d_sensor *sensor = &f11->sensor;
	struct f11_2d_data *data = &f11->data;
	s8 x, y;

	x = data->rel_pos[n_finger * RMI_F11_REL_BYTES];
	y = data->rel_pos[n_finger * RMI_F11_REL_BYTES + 1];

	rmi_2d_sensor_rel_report(sensor, x, y);
}

static void rmi_f11_abs_pos_process(struct f11_data *f11,
				   struct rmi_2d_sensor *sensor,
				   struct rmi_2d_sensor_abs_object *obj,
				   enum f11_finger_state finger_state,
				   u8 n_finger)
{
	struct f11_2d_data *data = &f11->data;
	u8 *pos_data = &data->abs_pos[n_finger * RMI_F11_ABS_BYTES];
	int tool_type = MT_TOOL_FINGER;

	switch (finger_state) {
	case F11_PRESENT:
		obj->type = RMI_2D_OBJECT_FINGER;
		break;
	default:
		obj->type = RMI_2D_OBJECT_NONE;
	}

	obj->mt_tool = tool_type;
	obj->x = (pos_data[0] << 4) | (pos_data[2] & 0x0F);
	obj->y = (pos_data[1] << 4) | (pos_data[2] >> 4);
	obj->z = pos_data[4];
	obj->wx = pos_data[3] & 0x0f;
	obj->wy = pos_data[3] >> 4;

	rmi_2d_sensor_abs_process(sensor, obj, n_finger);
}

static inline u8 rmi_f11_parse_finger_state(const u8 *f_state, u8 n_finger)
{
	return (f_state[n_finger / 4] >> (2 * (n_finger % 4))) &
							FINGER_STATE_MASK;
}

static void rmi_f11_finger_handler(struct f11_data *f11,
				   struct rmi_2d_sensor *sensor, int size)
{
	const u8 *f_state = f11->data.f_state;
	u8 finger_state;
	u8 i;
	int abs_fingers;
	int rel_fingers;
	int abs_size = sensor->nbr_fingers * RMI_F11_ABS_BYTES;

	if (sensor->report_abs) {
		if (abs_size > size)
			abs_fingers = size / RMI_F11_ABS_BYTES;
		else
			abs_fingers = sensor->nbr_fingers;

		for (i = 0; i < abs_fingers; i++) {
			 
			finger_state = rmi_f11_parse_finger_state(f_state, i);
			if (finger_state == F11_RESERVED) {
				pr_err("Invalid finger state[%d]: 0x%02x", i,
					finger_state);
				continue;
			}

			rmi_f11_abs_pos_process(f11, sensor, &sensor->objs[i],
							finger_state, i);
		}

		 
		if (sensor->kernel_tracking)
			input_mt_assign_slots(sensor->input,
					      sensor->tracking_slots,
					      sensor->tracking_pos,
					      sensor->nbr_fingers,
					      sensor->dmax);

		for (i = 0; i < abs_fingers; i++) {
			finger_state = rmi_f11_parse_finger_state(f_state, i);
			if (finger_state == F11_RESERVED)
				 
				continue;

			rmi_2d_sensor_abs_report(sensor, &sensor->objs[i], i);
		}

		input_mt_sync_frame(sensor->input);
	} else if (sensor->report_rel) {
		if ((abs_size + sensor->nbr_fingers * RMI_F11_REL_BYTES) > size)
			rel_fingers = (size - abs_size) / RMI_F11_REL_BYTES;
		else
			rel_fingers = sensor->nbr_fingers;

		for (i = 0; i < rel_fingers; i++)
			rmi_f11_rel_pos_report(f11, i);
	}

}

static int f11_2d_construct_data(struct f11_data *f11)
{
	struct rmi_2d_sensor *sensor = &f11->sensor;
	struct f11_2d_sensor_queries *query = &f11->sens_query;
	struct f11_2d_data *data = &f11->data;
	int i;

	sensor->nbr_fingers = (query->nr_fingers == 5 ? 10 :
				query->nr_fingers + 1);

	sensor->pkt_size = DIV_ROUND_UP(sensor->nbr_fingers, 4);

	if (query->has_abs) {
		sensor->pkt_size += (sensor->nbr_fingers * 5);
		sensor->attn_size = sensor->pkt_size;
	}

	if (query->has_rel)
		sensor->pkt_size +=  (sensor->nbr_fingers * 2);

	 
	if (query->query7_nonzero)
		sensor->pkt_size += sizeof(u8);

	 
	if (query->query7_nonzero || query->query8_nonzero)
		sensor->pkt_size += sizeof(u8);

	if (query->has_pinch || query->has_flick || query->has_rotate) {
		sensor->pkt_size += 3;
		if (!query->has_flick)
			sensor->pkt_size--;
		if (!query->has_rotate)
			sensor->pkt_size--;
	}

	if (query->has_touch_shapes)
		sensor->pkt_size +=
			DIV_ROUND_UP(query->nr_touch_shapes + 1, 8);

	sensor->data_pkt = devm_kzalloc(&sensor->fn->dev, sensor->pkt_size,
					GFP_KERNEL);
	if (!sensor->data_pkt)
		return -ENOMEM;

	data->f_state = sensor->data_pkt;
	i = DIV_ROUND_UP(sensor->nbr_fingers, 4);

	if (query->has_abs) {
		data->abs_pos = &sensor->data_pkt[i];
		i += (sensor->nbr_fingers * RMI_F11_ABS_BYTES);
	}

	if (query->has_rel) {
		data->rel_pos = &sensor->data_pkt[i];
		i += (sensor->nbr_fingers * RMI_F11_REL_BYTES);
	}

	if (query->query7_nonzero) {
		data->gest_1 = &sensor->data_pkt[i];
		i++;
	}

	if (query->query7_nonzero || query->query8_nonzero) {
		data->gest_2 = &sensor->data_pkt[i];
		i++;
	}

	if (query->has_pinch) {
		data->pinch = &sensor->data_pkt[i];
		i++;
	}

	if (query->has_flick) {
		if (query->has_pinch) {
			data->flick = data->pinch;
			i += 2;
		} else {
			data->flick = &sensor->data_pkt[i];
			i += 3;
		}
	}

	if (query->has_rotate) {
		if (query->has_flick) {
			data->rotate = data->flick + 1;
		} else {
			data->rotate = &sensor->data_pkt[i];
			i += 2;
		}
	}

	if (query->has_touch_shapes)
		data->shapes = &sensor->data_pkt[i];

	return 0;
}

static int f11_read_control_regs(struct rmi_function *fn,
				struct f11_2d_ctrl *ctrl, u16 ctrl_base_addr) {
	struct rmi_device *rmi_dev = fn->rmi_dev;
	int error = 0;

	ctrl->ctrl0_11_address = ctrl_base_addr;
	error = rmi_read_block(rmi_dev, ctrl_base_addr, ctrl->ctrl0_11,
				RMI_F11_CTRL_REG_COUNT);
	if (error < 0) {
		dev_err(&fn->dev, "Failed to read ctrl0, code: %d.\n", error);
		return error;
	}

	return 0;
}

static int f11_write_control_regs(struct rmi_function *fn,
					struct f11_2d_sensor_queries *query,
					struct f11_2d_ctrl *ctrl,
					u16 ctrl_base_addr)
{
	struct rmi_device *rmi_dev = fn->rmi_dev;
	int error;

	error = rmi_write_block(rmi_dev, ctrl_base_addr, ctrl->ctrl0_11,
				RMI_F11_CTRL_REG_COUNT);
	if (error < 0)
		return error;

	return 0;
}

static int rmi_f11_get_query_parameters(struct rmi_device *rmi_dev,
			struct f11_data *f11,
			struct f11_2d_sensor_queries *sensor_query,
			u16 query_base_addr)
{
	int query_size;
	int rc;
	u8 query_buf[RMI_F11_QUERY_SIZE];
	bool has_query36 = false;

	rc = rmi_read_block(rmi_dev, query_base_addr, query_buf,
				RMI_F11_QUERY_SIZE);
	if (rc < 0)
		return rc;

	sensor_query->nr_fingers = query_buf[0] & RMI_F11_NR_FINGERS_MASK;
	sensor_query->has_rel = !!(query_buf[0] & RMI_F11_HAS_REL);
	sensor_query->has_abs = !!(query_buf[0] & RMI_F11_HAS_ABS);
	sensor_query->has_gestures = !!(query_buf[0] & RMI_F11_HAS_GESTURES);
	sensor_query->has_sensitivity_adjust =
		!!(query_buf[0] & RMI_F11_HAS_SENSITIVITY_ADJ);
	sensor_query->configurable = !!(query_buf[0] & RMI_F11_CONFIGURABLE);

	sensor_query->nr_x_electrodes =
				query_buf[1] & RMI_F11_NR_ELECTRODES_MASK;
	sensor_query->nr_y_electrodes =
				query_buf[2] & RMI_F11_NR_ELECTRODES_MASK;
	sensor_query->max_electrodes =
				query_buf[3] & RMI_F11_NR_ELECTRODES_MASK;

	query_size = RMI_F11_QUERY_SIZE;

	if (sensor_query->has_abs) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size, query_buf);
		if (rc < 0)
			return rc;

		sensor_query->abs_data_size =
			query_buf[0] & RMI_F11_ABS_DATA_SIZE_MASK;
		sensor_query->has_anchored_finger =
			!!(query_buf[0] & RMI_F11_HAS_ANCHORED_FINGER);
		sensor_query->has_adj_hyst =
			!!(query_buf[0] & RMI_F11_HAS_ADJ_HYST);
		sensor_query->has_dribble =
			!!(query_buf[0] & RMI_F11_HAS_DRIBBLE);
		sensor_query->has_bending_correction =
			!!(query_buf[0] & RMI_F11_HAS_BENDING_CORRECTION);
		sensor_query->has_large_object_suppression =
			!!(query_buf[0] & RMI_F11_HAS_LARGE_OBJECT_SUPPRESSION);
		sensor_query->has_jitter_filter =
			!!(query_buf[0] & RMI_F11_HAS_JITTER_FILTER);
		query_size++;
	}

	if (sensor_query->has_rel) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size,
					&sensor_query->f11_2d_query6);
		if (rc < 0)
			return rc;
		query_size++;
	}

	if (sensor_query->has_gestures) {
		rc = rmi_read_block(rmi_dev, query_base_addr + query_size,
					query_buf, RMI_F11_QUERY_GESTURE_SIZE);
		if (rc < 0)
			return rc;

		sensor_query->has_single_tap =
			!!(query_buf[0] & RMI_F11_HAS_SINGLE_TAP);
		sensor_query->has_tap_n_hold =
			!!(query_buf[0] & RMI_F11_HAS_TAP_AND_HOLD);
		sensor_query->has_double_tap =
			!!(query_buf[0] & RMI_F11_HAS_DOUBLE_TAP);
		sensor_query->has_early_tap =
			!!(query_buf[0] & RMI_F11_HAS_EARLY_TAP);
		sensor_query->has_flick =
			!!(query_buf[0] & RMI_F11_HAS_FLICK);
		sensor_query->has_press =
			!!(query_buf[0] & RMI_F11_HAS_PRESS);
		sensor_query->has_pinch =
			!!(query_buf[0] & RMI_F11_HAS_PINCH);
		sensor_query->has_chiral =
			!!(query_buf[0] & RMI_F11_HAS_CHIRAL);

		 
		sensor_query->has_palm_det =
			!!(query_buf[1] & RMI_F11_HAS_PALM_DET);
		sensor_query->has_rotate =
			!!(query_buf[1] & RMI_F11_HAS_ROTATE);
		sensor_query->has_touch_shapes =
			!!(query_buf[1] & RMI_F11_HAS_TOUCH_SHAPES);
		sensor_query->has_scroll_zones =
			!!(query_buf[1] & RMI_F11_HAS_SCROLL_ZONES);
		sensor_query->has_individual_scroll_zones =
			!!(query_buf[1] & RMI_F11_HAS_INDIVIDUAL_SCROLL_ZONES);
		sensor_query->has_mf_scroll =
			!!(query_buf[1] & RMI_F11_HAS_MF_SCROLL);
		sensor_query->has_mf_edge_motion =
			!!(query_buf[1] & RMI_F11_HAS_MF_EDGE_MOTION);
		sensor_query->has_mf_scroll_inertia =
			!!(query_buf[1] & RMI_F11_HAS_MF_SCROLL_INERTIA);

		sensor_query->query7_nonzero = !!(query_buf[0]);
		sensor_query->query8_nonzero = !!(query_buf[1]);

		query_size += 2;
	}

	if (f11->has_query9) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size, query_buf);
		if (rc < 0)
			return rc;

		sensor_query->has_pen =
			!!(query_buf[0] & RMI_F11_HAS_PEN);
		sensor_query->has_proximity =
			!!(query_buf[0] & RMI_F11_HAS_PROXIMITY);
		sensor_query->has_palm_det_sensitivity =
			!!(query_buf[0] & RMI_F11_HAS_PALM_DET_SENSITIVITY);
		sensor_query->has_suppress_on_palm_detect =
			!!(query_buf[0] & RMI_F11_HAS_SUPPRESS_ON_PALM_DETECT);
		sensor_query->has_two_pen_thresholds =
			!!(query_buf[0] & RMI_F11_HAS_TWO_PEN_THRESHOLDS);
		sensor_query->has_contact_geometry =
			!!(query_buf[0] & RMI_F11_HAS_CONTACT_GEOMETRY);
		sensor_query->has_pen_hover_discrimination =
			!!(query_buf[0] & RMI_F11_HAS_PEN_HOVER_DISCRIMINATION);
		sensor_query->has_pen_filters =
			!!(query_buf[0] & RMI_F11_HAS_PEN_FILTERS);

		query_size++;
	}

	if (sensor_query->has_touch_shapes) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size, query_buf);
		if (rc < 0)
			return rc;

		sensor_query->nr_touch_shapes = query_buf[0] &
				RMI_F11_NR_TOUCH_SHAPES_MASK;

		query_size++;
	}

	if (f11->has_query11) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size, query_buf);
		if (rc < 0)
			return rc;

		sensor_query->has_z_tuning =
			!!(query_buf[0] & RMI_F11_HAS_Z_TUNING);
		sensor_query->has_algorithm_selection =
			!!(query_buf[0] & RMI_F11_HAS_ALGORITHM_SELECTION);
		sensor_query->has_w_tuning =
			!!(query_buf[0] & RMI_F11_HAS_W_TUNING);
		sensor_query->has_pitch_info =
			!!(query_buf[0] & RMI_F11_HAS_PITCH_INFO);
		sensor_query->has_finger_size =
			!!(query_buf[0] & RMI_F11_HAS_FINGER_SIZE);
		sensor_query->has_segmentation_aggressiveness =
			!!(query_buf[0] &
				RMI_F11_HAS_SEGMENTATION_AGGRESSIVENESS);
		sensor_query->has_XY_clip =
			!!(query_buf[0] & RMI_F11_HAS_XY_CLIP);
		sensor_query->has_drumming_filter =
			!!(query_buf[0] & RMI_F11_HAS_DRUMMING_FILTER);

		query_size++;
	}

	if (f11->has_query12) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size, query_buf);
		if (rc < 0)
			return rc;

		sensor_query->has_gapless_finger =
			!!(query_buf[0] & RMI_F11_HAS_GAPLESS_FINGER);
		sensor_query->has_gapless_finger_tuning =
			!!(query_buf[0] & RMI_F11_HAS_GAPLESS_FINGER_TUNING);
		sensor_query->has_8bit_w =
			!!(query_buf[0] & RMI_F11_HAS_8BIT_W);
		sensor_query->has_adjustable_mapping =
			!!(query_buf[0] & RMI_F11_HAS_ADJUSTABLE_MAPPING);
		sensor_query->has_info2 =
			!!(query_buf[0] & RMI_F11_HAS_INFO2);
		sensor_query->has_physical_props =
			!!(query_buf[0] & RMI_F11_HAS_PHYSICAL_PROPS);
		sensor_query->has_finger_limit =
			!!(query_buf[0] & RMI_F11_HAS_FINGER_LIMIT);
		sensor_query->has_linear_coeff_2 =
			!!(query_buf[0] & RMI_F11_HAS_LINEAR_COEFF);

		query_size++;
	}

	if (sensor_query->has_jitter_filter) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size, query_buf);
		if (rc < 0)
			return rc;

		sensor_query->jitter_window_size = query_buf[0] &
			RMI_F11_JITTER_WINDOW_MASK;
		sensor_query->jitter_filter_type = (query_buf[0] &
			RMI_F11_JITTER_FILTER_MASK) >>
			RMI_F11_JITTER_FILTER_SHIFT;

		query_size++;
	}

	if (sensor_query->has_info2) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size, query_buf);
		if (rc < 0)
			return rc;

		sensor_query->light_control =
			query_buf[0] & RMI_F11_LIGHT_CONTROL_MASK;
		sensor_query->is_clear =
			!!(query_buf[0] & RMI_F11_IS_CLEAR);
		sensor_query->clickpad_props =
			(query_buf[0] & RMI_F11_CLICKPAD_PROPS_MASK) >>
			RMI_F11_CLICKPAD_PROPS_SHIFT;
		sensor_query->mouse_buttons =
			(query_buf[0] & RMI_F11_MOUSE_BUTTONS_MASK) >>
			RMI_F11_MOUSE_BUTTONS_SHIFT;
		sensor_query->has_advanced_gestures =
			!!(query_buf[0] & RMI_F11_HAS_ADVANCED_GESTURES);

		query_size++;
	}

	if (sensor_query->has_physical_props) {
		rc = rmi_read_block(rmi_dev, query_base_addr
			+ query_size, query_buf, 4);
		if (rc < 0)
			return rc;

		sensor_query->x_sensor_size_mm =
			(query_buf[0] | (query_buf[1] << 8)) / 10;
		sensor_query->y_sensor_size_mm =
			(query_buf[2] | (query_buf[3] << 8)) / 10;

		 
		query_size += 12;
	}

	if (f11->has_query27)
		++query_size;

	if (f11->has_query28) {
		rc = rmi_read(rmi_dev, query_base_addr + query_size,
				query_buf);
		if (rc < 0)
			return rc;

		has_query36 = !!(query_buf[0] & BIT(6));
	}

	if (has_query36) {
		query_size += 2;
		rc = rmi_read(rmi_dev, query_base_addr + query_size,
				query_buf);
		if (rc < 0)
			return rc;

		if (!!(query_buf[0] & BIT(5)))
			f11->has_acm = true;
	}

	return query_size;
}

static int rmi_f11_initialize(struct rmi_function *fn)
{
	struct rmi_device *rmi_dev = fn->rmi_dev;
	struct f11_data *f11;
	struct f11_2d_ctrl *ctrl;
	u8 query_offset;
	u16 query_base_addr;
	u16 control_base_addr;
	u16 max_x_pos, max_y_pos;
	int rc;
	const struct rmi_device_platform_data *pdata =
				rmi_get_platform_data(rmi_dev);
	struct rmi_driver_data *drvdata = dev_get_drvdata(&rmi_dev->dev);
	struct rmi_2d_sensor *sensor;
	u8 buf;
	int mask_size;

	rmi_dbg(RMI_DEBUG_FN, &fn->dev, "Initializing F11 values.\n");

	mask_size = BITS_TO_LONGS(drvdata->irq_count) * sizeof(unsigned long);

	 
	f11 = devm_kzalloc(&fn->dev, sizeof(struct f11_data) + mask_size * 2,
			GFP_KERNEL);
	if (!f11)
		return -ENOMEM;

	if (fn->dev.of_node) {
		rc = rmi_2d_sensor_of_probe(&fn->dev, &f11->sensor_pdata);
		if (rc)
			return rc;
	} else {
		f11->sensor_pdata = pdata->sensor_pdata;
	}

	f11->rezero_wait_ms = f11->sensor_pdata.rezero_wait;

	f11->abs_mask = (unsigned long *)((char *)f11
			+ sizeof(struct f11_data));
	f11->rel_mask = (unsigned long *)((char *)f11
			+ sizeof(struct f11_data) + mask_size);

	set_bit(fn->irq_pos, f11->abs_mask);
	set_bit(fn->irq_pos + 1, f11->rel_mask);

	query_base_addr = fn->fd.query_base_addr;
	control_base_addr = fn->fd.control_base_addr;

	rc = rmi_read(rmi_dev, query_base_addr, &buf);
	if (rc < 0)
		return rc;

	f11->has_query9 = !!(buf & RMI_F11_HAS_QUERY9);
	f11->has_query11 = !!(buf & RMI_F11_HAS_QUERY11);
	f11->has_query12 = !!(buf & RMI_F11_HAS_QUERY12);
	f11->has_query27 = !!(buf & RMI_F11_HAS_QUERY27);
	f11->has_query28 = !!(buf & RMI_F11_HAS_QUERY28);

	query_offset = (query_base_addr + 1);
	sensor = &f11->sensor;
	sensor->fn = fn;

	rc = rmi_f11_get_query_parameters(rmi_dev, f11,
			&f11->sens_query, query_offset);
	if (rc < 0)
		return rc;
	query_offset += rc;

	rc = f11_read_control_regs(fn, &f11->dev_controls,
			control_base_addr);
	if (rc < 0) {
		dev_err(&fn->dev,
			"Failed to read F11 control params.\n");
		return rc;
	}

	if (f11->sens_query.has_info2) {
		if (f11->sens_query.is_clear)
			f11->sensor.sensor_type = rmi_sensor_touchscreen;
		else
			f11->sensor.sensor_type = rmi_sensor_touchpad;
	}

	sensor->report_abs = f11->sens_query.has_abs;

	sensor->axis_align =
		f11->sensor_pdata.axis_align;

	sensor->topbuttonpad = f11->sensor_pdata.topbuttonpad;
	sensor->kernel_tracking = f11->sensor_pdata.kernel_tracking;
	sensor->dmax = f11->sensor_pdata.dmax;
	sensor->dribble = f11->sensor_pdata.dribble;
	sensor->palm_detect = f11->sensor_pdata.palm_detect;

	if (f11->sens_query.has_physical_props) {
		sensor->x_mm = f11->sens_query.x_sensor_size_mm;
		sensor->y_mm = f11->sens_query.y_sensor_size_mm;
	} else {
		sensor->x_mm = f11->sensor_pdata.x_mm;
		sensor->y_mm = f11->sensor_pdata.y_mm;
	}

	if (sensor->sensor_type == rmi_sensor_default)
		sensor->sensor_type =
			f11->sensor_pdata.sensor_type;

	sensor->report_abs = sensor->report_abs
		&& !(f11->sensor_pdata.disable_report_mask
			& RMI_F11_DISABLE_ABS_REPORT);

	if (!sensor->report_abs)
		 
		sensor->report_rel = f11->sens_query.has_rel;

	rc = rmi_read_block(rmi_dev,
		control_base_addr + F11_CTRL_SENSOR_MAX_X_POS_OFFSET,
		(u8 *)&max_x_pos, sizeof(max_x_pos));
	if (rc < 0)
		return rc;

	rc = rmi_read_block(rmi_dev,
		control_base_addr + F11_CTRL_SENSOR_MAX_Y_POS_OFFSET,
		(u8 *)&max_y_pos, sizeof(max_y_pos));
	if (rc < 0)
		return rc;

	sensor->max_x = max_x_pos;
	sensor->max_y = max_y_pos;

	rc = f11_2d_construct_data(f11);
	if (rc < 0)
		return rc;

	if (f11->has_acm)
		f11->sensor.attn_size += f11->sensor.nbr_fingers * 2;

	 
	sensor->tracking_pos = devm_kcalloc(&fn->dev,
			sensor->nbr_fingers, sizeof(struct input_mt_pos),
			GFP_KERNEL);
	sensor->tracking_slots = devm_kcalloc(&fn->dev,
			sensor->nbr_fingers, sizeof(int), GFP_KERNEL);
	sensor->objs = devm_kcalloc(&fn->dev,
			sensor->nbr_fingers,
			sizeof(struct rmi_2d_sensor_abs_object),
			GFP_KERNEL);
	if (!sensor->tracking_pos || !sensor->tracking_slots || !sensor->objs)
		return -ENOMEM;

	ctrl = &f11->dev_controls;
	if (sensor->axis_align.delta_x_threshold)
		ctrl->ctrl0_11[RMI_F11_DELTA_X_THRESHOLD] =
			sensor->axis_align.delta_x_threshold;

	if (sensor->axis_align.delta_y_threshold)
		ctrl->ctrl0_11[RMI_F11_DELTA_Y_THRESHOLD] =
			sensor->axis_align.delta_y_threshold;

	 
	if (sensor->axis_align.delta_x_threshold ||
	    sensor->axis_align.delta_y_threshold) {
		ctrl->ctrl0_11[0] &= ~RMI_F11_REPORT_MODE_MASK;
		ctrl->ctrl0_11[0] |= RMI_F11_REPORT_MODE_REDUCED;
	}

	if (f11->sens_query.has_dribble) {
		switch (sensor->dribble) {
		case RMI_REG_STATE_OFF:
			ctrl->ctrl0_11[0] &= ~BIT(6);
			break;
		case RMI_REG_STATE_ON:
			ctrl->ctrl0_11[0] |= BIT(6);
			break;
		case RMI_REG_STATE_DEFAULT:
		default:
			break;
		}
	}

	if (f11->sens_query.has_palm_det) {
		switch (sensor->palm_detect) {
		case RMI_REG_STATE_OFF:
			ctrl->ctrl0_11[11] &= ~BIT(0);
			break;
		case RMI_REG_STATE_ON:
			ctrl->ctrl0_11[11] |= BIT(0);
			break;
		case RMI_REG_STATE_DEFAULT:
		default:
			break;
		}
	}

	rc = f11_write_control_regs(fn, &f11->sens_query,
			   &f11->dev_controls, fn->fd.control_base_addr);
	if (rc)
		dev_warn(&fn->dev, "Failed to write control registers\n");

	mutex_init(&f11->dev_controls_mutex);

	dev_set_drvdata(&fn->dev, f11);

	return 0;
}

static int rmi_f11_config(struct rmi_function *fn)
{
	struct f11_data *f11 = dev_get_drvdata(&fn->dev);
	struct rmi_driver *drv = fn->rmi_dev->driver;
	struct rmi_2d_sensor *sensor = &f11->sensor;
	int rc;

	if (!sensor->report_abs)
		drv->clear_irq_bits(fn->rmi_dev, f11->abs_mask);
	else
		drv->set_irq_bits(fn->rmi_dev, f11->abs_mask);

	if (!sensor->report_rel)
		drv->clear_irq_bits(fn->rmi_dev, f11->rel_mask);
	else
		drv->set_irq_bits(fn->rmi_dev, f11->rel_mask);

	rc = f11_write_control_regs(fn, &f11->sens_query,
			   &f11->dev_controls, fn->fd.query_base_addr);
	if (rc < 0)
		return rc;

	return 0;
}

static irqreturn_t rmi_f11_attention(int irq, void *ctx)
{
	struct rmi_function *fn = ctx;
	struct rmi_device *rmi_dev = fn->rmi_dev;
	struct rmi_driver_data *drvdata = dev_get_drvdata(&rmi_dev->dev);
	struct f11_data *f11 = dev_get_drvdata(&fn->dev);
	u16 data_base_addr = fn->fd.data_base_addr;
	int error;
	int valid_bytes = f11->sensor.pkt_size;

	if (drvdata->attn_data.data) {
		 
		if (f11->sensor.attn_size > drvdata->attn_data.size)
			valid_bytes = drvdata->attn_data.size;
		else
			valid_bytes = f11->sensor.attn_size;
		memcpy(f11->sensor.data_pkt, drvdata->attn_data.data,
			valid_bytes);
		drvdata->attn_data.data += valid_bytes;
		drvdata->attn_data.size -= valid_bytes;
	} else {
		error = rmi_read_block(rmi_dev,
				data_base_addr, f11->sensor.data_pkt,
				f11->sensor.pkt_size);
		if (error < 0)
			return IRQ_RETVAL(error);
	}

	rmi_f11_finger_handler(f11, &f11->sensor, valid_bytes);

	return IRQ_HANDLED;
}

static int rmi_f11_resume(struct rmi_function *fn)
{
	struct f11_data *f11 = dev_get_drvdata(&fn->dev);
	int error;

	rmi_dbg(RMI_DEBUG_FN, &fn->dev, "Resuming...\n");
	if (!f11->rezero_wait_ms)
		return 0;

	mdelay(f11->rezero_wait_ms);

	error = rmi_write(fn->rmi_dev, fn->fd.command_base_addr,
				RMI_F11_REZERO);
	if (error) {
		dev_err(&fn->dev,
			"%s: failed to issue rezero command, error = %d.",
			__func__, error);
		return error;
	}

	return 0;
}

static int rmi_f11_probe(struct rmi_function *fn)
{
	int error;
	struct f11_data *f11;

	error = rmi_f11_initialize(fn);
	if (error)
		return error;

	f11 = dev_get_drvdata(&fn->dev);
	error = rmi_2d_sensor_configure_input(fn, &f11->sensor);
	if (error)
		return error;

	return 0;
}

struct rmi_function_handler rmi_f11_handler = {
	.driver = {
		.name	= "rmi4_f11",
	},
	.func		= 0x11,
	.probe		= rmi_f11_probe,
	.config		= rmi_f11_config,
	.attention	= rmi_f11_attention,
	.resume		= rmi_f11_resume,
};
