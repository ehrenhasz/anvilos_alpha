 
 
#ifndef ISHTP_HID__H
#define	ISHTP_HID__H

 
#define	ISH_HID_VENDOR	0x8086
#define	ISH_HID_PRODUCT	0x22D8
#define	ISH_HID_VERSION	0x0200

#define	CMD_MASK	0x7F
#define	IS_RESPONSE	0x80

 
extern ishtp_print_log ishtp_hid_print_trace;
#define hid_ishtp_trace(client, ...) \
	(ishtp_hid_print_trace)(NULL, __VA_ARGS__)

 
struct hostif_msg_hdr {
	uint8_t	command;  
	uint8_t	device_id;
	uint8_t	status;
	uint8_t	flags;
	uint16_t size;
} __packed;

struct hostif_msg {
	struct hostif_msg_hdr	hdr;
} __packed;

struct hostif_msg_to_sensor {
	struct hostif_msg_hdr	hdr;
	uint8_t	report_id;
} __packed;

struct device_info {
	uint32_t dev_id;
	uint8_t dev_class;
	uint16_t pid;
	uint16_t vid;
} __packed;

struct ishtp_version {
	uint8_t	major;
	uint8_t	minor;
	uint8_t	hotfix;
	uint16_t build;
} __packed;

 
struct report_list {
	uint16_t total_size;
	uint8_t	num_of_reports;
	uint8_t	flags;
	struct {
		uint16_t	size_of_report;
		uint8_t report[1];
	} __packed reports[1];
} __packed;

 
#define	HOSTIF_HID_COMMAND_BASE			0
#define	HOSTIF_GET_HID_DESCRIPTOR		0
#define	HOSTIF_GET_REPORT_DESCRIPTOR		1
#define HOSTIF_GET_FEATURE_REPORT		2
#define	HOSTIF_SET_FEATURE_REPORT		3
#define	HOSTIF_GET_INPUT_REPORT			4
#define	HOSTIF_PUBLISH_INPUT_REPORT		5
#define	HOSTIF_PUBLISH_INPUT_REPORT_LIST	6
#define	HOSTIF_DM_COMMAND_BASE			32
#define	HOSTIF_DM_ENUM_DEVICES			33
#define	HOSTIF_DM_ADD_DEVICE			34

#define	MAX_HID_DEVICES				32

 
struct ishtp_cl_data {
	 
	bool enum_devices_done;
	bool hid_descr_done;
	bool report_descr_done;
	bool init_done;
	bool suspended;

	unsigned int num_hid_devices;
	unsigned int cur_hid_dev;
	unsigned int hid_dev_count;

	struct device_info *hid_devices;
	unsigned char *report_descr[MAX_HID_DEVICES];
	int report_descr_size[MAX_HID_DEVICES];
	struct hid_device *hid_sensor_hubs[MAX_HID_DEVICES];
	unsigned char *hid_descr[MAX_HID_DEVICES];
	int hid_descr_size[MAX_HID_DEVICES];

	wait_queue_head_t init_wait;
	wait_queue_head_t ishtp_resume_wait;
	struct ishtp_cl *hid_ishtp_cl;

	 
	unsigned int bad_recv_cnt;
	int multi_packet_cnt;

	struct work_struct work;
	struct work_struct resume_work;
	struct ishtp_cl_device *cl_device;
};

 
struct ishtp_hid_data {
	int index;
	bool request_done;
	struct ishtp_cl_data *client_data;
	wait_queue_head_t hid_wait;

	 
	bool raw_get_req;
	u8 *raw_buf;
	size_t raw_buf_size;
};

 
void hid_ishtp_set_feature(struct hid_device *hid, char *buf, unsigned int len,
			   int report_id);
void hid_ishtp_get_report(struct hid_device *hid, int report_id,
			  int report_type);
int ishtp_hid_probe(unsigned int cur_hid_dev,
		    struct ishtp_cl_data *client_data);
void ishtp_hid_remove(struct ishtp_cl_data *client_data);
int ishtp_hid_link_ready_wait(struct ishtp_cl_data *client_data);
void ishtp_hid_wakeup(struct hid_device *hid);

#endif	 
