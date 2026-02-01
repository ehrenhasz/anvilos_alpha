 

 

#ifndef _ACPI_FAN_H_
#define _ACPI_FAN_H_

#define ACPI_FAN_DEVICE_IDS	\
	{"INT3404", },   \
	{"INTC1044", },   \
	{"INTC1048", },   \
	{"INTC1063", },   \
	{"INTC10A2", },   \
	{"PNP0C0B", }  

#define ACPI_FPS_NAME_LEN	20

struct acpi_fan_fps {
	u64 control;
	u64 trip_point;
	u64 speed;
	u64 noise_level;
	u64 power;
	char name[ACPI_FPS_NAME_LEN];
	struct device_attribute dev_attr;
};

struct acpi_fan_fif {
	u8 revision;
	u8 fine_grain_ctrl;
	u8 step_size;
	u8 low_speed_notification;
};

struct acpi_fan_fst {
	u64 revision;
	u64 control;
	u64 speed;
};

struct acpi_fan {
	bool acpi4;
	struct acpi_fan_fif fif;
	struct acpi_fan_fps *fps;
	int fps_count;
	struct thermal_cooling_device *cdev;
	struct device_attribute fst_speed;
	struct device_attribute fine_grain_control;
};

int acpi_fan_get_fst(struct acpi_device *device, struct acpi_fan_fst *fst);
int acpi_fan_create_attributes(struct acpi_device *device);
void acpi_fan_delete_attributes(struct acpi_device *device);
#endif
