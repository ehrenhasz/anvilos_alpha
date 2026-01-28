#ifndef __KUNPENG_HCCS_H__
#define __KUNPENG_HCCS_H__
#define HCCS_DIE_MAX_PORT_ID	254
struct hccs_port_info {
	u8 port_id;
	u8 port_type;
	u8 lane_mode;
	bool enable;  
	struct kobject kobj;
	bool dir_created;
	struct hccs_die_info *die;  
};
struct hccs_die_info {
	u8 die_id;
	u8 port_num;
	u8 min_port_id;
	u8 max_port_id;
	struct hccs_port_info *ports;
	struct kobject kobj;
	bool dir_created;
	struct hccs_chip_info *chip;  
};
struct hccs_chip_info {
	u8 chip_id;
	u8 die_num;
	struct hccs_die_info *dies;
	struct kobject kobj;
	struct hccs_dev *hdev;
};
struct hccs_mbox_client_info {
	struct mbox_client client;
	struct mbox_chan *mbox_chan;
	struct pcc_mbox_chan *pcc_chan;
	u64 deadline_us;
	void __iomem *pcc_comm_addr;
};
struct hccs_dev {
	struct device *dev;
	struct acpi_device *acpi_dev;
	u64 caps;
	u8 chip_num;
	struct hccs_chip_info *chips;
	u8 chan_id;
	struct mutex lock;
	struct hccs_mbox_client_info cl_info;
};
#define HCCS_SERDES_MODULE_CODE 0x32
enum hccs_subcmd_type {
	HCCS_GET_CHIP_NUM = 0x1,
	HCCS_GET_DIE_NUM,
	HCCS_GET_DIE_INFO,
	HCCS_GET_DIE_PORT_INFO,
	HCCS_GET_DEV_CAP,
	HCCS_GET_PORT_LINK_STATUS,
	HCCS_GET_PORT_CRC_ERR_CNT,
	HCCS_GET_DIE_PORTS_LANE_STA,
	HCCS_GET_DIE_PORTS_LINK_STA,
	HCCS_GET_DIE_PORTS_CRC_ERR_CNT,
	HCCS_SUB_CMD_MAX = 255,
};
struct hccs_die_num_req_param {
	u8 chip_id;
};
struct hccs_die_info_req_param {
	u8 chip_id;
	u8 die_idx;
};
struct hccs_die_info_rsp_data {
	u8 die_id;
	u8 port_num;
	u8 min_port_id;
	u8 max_port_id;
};
struct hccs_port_attr {
	u8 port_id;
	u8 port_type;
	u8 lane_mode;
	u8 enable : 1;  
	u16 rsv[2];
};
struct hccs_die_comm_req_param {
	u8 chip_id;
	u8 die_id;  
};
struct hccs_port_comm_req_param {
	u8 chip_id;
	u8 die_id;
	u8 port_id;
};
#define HCCS_PORT_RESET         1
#define HCCS_PORT_SETUP         2
#define HCCS_PORT_CONFIG        3
#define HCCS_PORT_READY         4
struct hccs_link_status {
	u8 lane_mask;  
	u8 link_fsm : 3;  
	u8 lane_num : 5;  
};
struct hccs_req_head {
	u8 module_code;  
	u8 start_id;
	u8 rsv[2];
};
struct hccs_rsp_head {
	u8 data_len;
	u8 next_id;
	u8 rsv[2];
};
struct hccs_fw_inner_head {
	u8 retStatus;  
	u8 rsv[7];
};
#define HCCS_PCC_SHARE_MEM_BYTES	64
#define HCCS_FW_INNER_HEAD_BYTES	8
#define HCCS_RSP_HEAD_BYTES		4
#define HCCS_MAX_RSP_DATA_BYTES		(HCCS_PCC_SHARE_MEM_BYTES - \
					 HCCS_FW_INNER_HEAD_BYTES - \
					 HCCS_RSP_HEAD_BYTES)
#define HCCS_MAX_RSP_DATA_SIZE_MAX	(HCCS_MAX_RSP_DATA_BYTES / 4)
struct hccs_rsp_desc {
	struct hccs_fw_inner_head fw_inner_head;  
	struct hccs_rsp_head rsp_head;  
	u32 data[HCCS_MAX_RSP_DATA_SIZE_MAX];
};
#define HCCS_REQ_HEAD_BYTES		4
#define HCCS_MAX_REQ_DATA_BYTES		(HCCS_PCC_SHARE_MEM_BYTES - \
					 HCCS_REQ_HEAD_BYTES)
#define HCCS_MAX_REQ_DATA_SIZE_MAX	(HCCS_MAX_REQ_DATA_BYTES / 4)
struct hccs_req_desc {
	struct hccs_req_head req_head;  
	u32 data[HCCS_MAX_REQ_DATA_SIZE_MAX];
};
struct hccs_desc {
	union {
		struct hccs_req_desc req;
		struct hccs_rsp_desc rsp;
	};
};
#endif  
