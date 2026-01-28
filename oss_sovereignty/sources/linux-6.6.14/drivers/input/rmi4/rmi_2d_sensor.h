


#ifndef _RMI_2D_SENSOR_H
#define _RMI_2D_SENSOR_H

enum rmi_2d_sensor_object_type {
	RMI_2D_OBJECT_NONE,
	RMI_2D_OBJECT_FINGER,
	RMI_2D_OBJECT_STYLUS,
	RMI_2D_OBJECT_PALM,
	RMI_2D_OBJECT_UNCLASSIFIED,
};

struct rmi_2d_sensor_abs_object {
	enum rmi_2d_sensor_object_type type;
	int mt_tool;
	u16 x;
	u16 y;
	u8 z;
	u8 wx;
	u8 wy;
};


struct rmi_2d_sensor {
	struct rmi_2d_axis_alignment axis_align;
	struct input_mt_pos *tracking_pos;
	int *tracking_slots;
	bool kernel_tracking;
	struct rmi_2d_sensor_abs_object *objs;
	int dmax;
	u16 min_x;
	u16 max_x;
	u16 min_y;
	u16 max_y;
	u8 nbr_fingers;
	u8 *data_pkt;
	int pkt_size;
	int attn_size;
	bool topbuttonpad;
	enum rmi_sensor_type sensor_type;
	struct input_dev *input;
	struct rmi_function *fn;
	char input_phys[32];
	u8 report_abs;
	u8 report_rel;
	u8 x_mm;
	u8 y_mm;
	enum rmi_reg_state dribble;
	enum rmi_reg_state palm_detect;
};

int rmi_2d_sensor_of_probe(struct device *dev,
				struct rmi_2d_sensor_platform_data *pdata);

void rmi_2d_sensor_abs_process(struct rmi_2d_sensor *sensor,
				struct rmi_2d_sensor_abs_object *obj,
				int slot);

void rmi_2d_sensor_abs_report(struct rmi_2d_sensor *sensor,
				struct rmi_2d_sensor_abs_object *obj,
				int slot);

void rmi_2d_sensor_rel_report(struct rmi_2d_sensor *sensor, int x, int y);

int rmi_2d_sensor_configure_input(struct rmi_function *fn,
					struct rmi_2d_sensor *sensor);
#endif 
