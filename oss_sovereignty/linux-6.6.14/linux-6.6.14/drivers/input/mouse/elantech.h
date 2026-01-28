#ifndef _ELANTECH_H
#define _ELANTECH_H
#define ETP_FW_ID_QUERY			0x00
#define ETP_FW_VERSION_QUERY		0x01
#define ETP_CAPABILITIES_QUERY		0x02
#define ETP_SAMPLE_QUERY		0x03
#define ETP_RESOLUTION_QUERY		0x04
#define ETP_ICBODY_QUERY		0x05
#define ETP_REGISTER_READ		0x10
#define ETP_REGISTER_WRITE		0x11
#define ETP_REGISTER_READWRITE		0x00
#define ETP_PS2_CUSTOM_COMMAND		0xf8
#define ETP_PS2_COMMAND_TRIES		3
#define ETP_PS2_COMMAND_DELAY		500
#define ETP_READ_BACK_TRIES		5
#define ETP_READ_BACK_DELAY		2000
#define ETP_R10_ABSOLUTE_MODE		0x04
#define ETP_R11_4_BYTE_MODE		0x02
#define ETP_CAP_HAS_ROCKER		0x04
#define ETP_EDGE_FUZZ_V1		32
#define ETP_XMIN_V1			(  0 + ETP_EDGE_FUZZ_V1)
#define ETP_XMAX_V1			(576 - ETP_EDGE_FUZZ_V1)
#define ETP_YMIN_V1			(  0 + ETP_EDGE_FUZZ_V1)
#define ETP_YMAX_V1			(384 - ETP_EDGE_FUZZ_V1)
#define ETP_XMIN_V2			0
#define ETP_XMAX_V2			1152
#define ETP_YMIN_V2			0
#define ETP_YMAX_V2			768
#define ETP_PMIN_V2			0
#define ETP_PMAX_V2			255
#define ETP_WMIN_V2			0
#define ETP_WMAX_V2			15
#define PACKET_UNKNOWN			0x01
#define PACKET_DEBOUNCE			0x02
#define PACKET_V3_HEAD			0x03
#define PACKET_V3_TAIL			0x04
#define PACKET_V4_HEAD			0x05
#define PACKET_V4_MOTION		0x06
#define PACKET_V4_STATUS		0x07
#define PACKET_TRACKPOINT		0x08
#define ETP_MAX_FINGERS			5
#define ETP_WEIGHT_VALUE		5
#define ETP_BUS_PS2_ONLY		0
#define ETP_BUS_SMB_ALERT_ONLY		1
#define ETP_BUS_SMB_HST_NTFY_ONLY	2
#define ETP_BUS_PS2_SMB_ALERT		3
#define ETP_BUS_PS2_SMB_HST_NTFY	4
#define ETP_NEW_IC_SMBUS_HOST_NOTIFY(fw_version)	\
		((((fw_version) & 0x0f2000) == 0x0f2000) && \
		 ((fw_version) & 0x0000ff) > 0)
struct finger_pos {
	unsigned int x;
	unsigned int y;
};
struct elantech_device_info {
	unsigned char capabilities[3];
	unsigned char samples[3];
	unsigned char debug;
	unsigned char hw_version;
	unsigned char pattern;
	unsigned int fw_version;
	unsigned int ic_version;
	unsigned int product_id;
	unsigned int x_min;
	unsigned int y_min;
	unsigned int x_max;
	unsigned int y_max;
	unsigned int x_res;
	unsigned int y_res;
	unsigned int x_traces;
	unsigned int y_traces;
	unsigned int width;
	unsigned int bus;
	bool paritycheck;
	bool jumpy_cursor;
	bool reports_pressure;
	bool crc_enabled;
	bool set_hw_resolution;
	bool has_trackpoint;
	bool has_middle_button;
	int (*send_cmd)(struct psmouse *psmouse, unsigned char c,
			unsigned char *param);
};
struct elantech_data {
	struct input_dev *tp_dev;	 
	char tp_phys[32];
	unsigned char reg_07;
	unsigned char reg_10;
	unsigned char reg_11;
	unsigned char reg_20;
	unsigned char reg_21;
	unsigned char reg_22;
	unsigned char reg_23;
	unsigned char reg_24;
	unsigned char reg_25;
	unsigned char reg_26;
	unsigned int single_finger_reports;
	unsigned int y_max;
	unsigned int width;
	struct finger_pos mt[ETP_MAX_FINGERS];
	unsigned char parity[256];
	struct elantech_device_info info;
	void (*original_set_rate)(struct psmouse *psmouse, unsigned int rate);
};
int elantech_detect(struct psmouse *psmouse, bool set_properties);
int elantech_init_ps2(struct psmouse *psmouse);
#ifdef CONFIG_MOUSE_PS2_ELANTECH
int elantech_init(struct psmouse *psmouse);
#else
static inline int elantech_init(struct psmouse *psmouse)
{
	return -ENOSYS;
}
#endif  
int elantech_init_smbus(struct psmouse *psmouse);
#endif
