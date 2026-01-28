

#ifndef __QLA_MR_H
#define __QLA_MR_H

#include "qla_dsd.h"


#define PCI_DEVICE_ID_QLOGIC_ISPF001		0xF001



#define FX00_COMMAND_TYPE_7	0x07	
struct cmd_type_7_fx00 {
	uint8_t entry_type;		
	uint8_t entry_count;		
	uint8_t sys_define;		
	uint8_t entry_status;		

	uint32_t handle;		
	uint8_t reserved_0;
	uint8_t port_path_ctrl;
	uint16_t reserved_1;

	__le16 tgt_idx;		
	uint16_t timeout;		

	__le16 dseg_count;		
	uint8_t	scsi_rsp_dsd_len;
	uint8_t reserved_2;

	struct scsi_lun lun;		

	uint8_t cntrl_flags;

	uint8_t task_mgmt_flags;	

	uint8_t task;

	uint8_t crn;

	uint8_t fcp_cdb[MAX_CMDSZ];	
	__le32 byte_count;		

	struct dsd64 dsd;
};

#define	STATUS_TYPE_FX00	0x01		
struct sts_entry_fx00 {
	uint8_t entry_type;		
	uint8_t entry_count;		
	uint8_t sys_define;		
	uint8_t entry_status;		

	uint32_t handle;		
	uint32_t reserved_3;		

	__le16 comp_status;		
	uint16_t reserved_0;		

	__le32 residual_len;		

	uint16_t reserved_1;
	uint16_t state_flags;		

	uint16_t reserved_2;
	__le16 scsi_status;		

	uint32_t sense_len;		
	uint8_t data[32];		
};


#define MAX_HANDLE_COUNT	15
#define MULTI_STATUS_TYPE_FX00	0x0D

struct multi_sts_entry_fx00 {
	uint8_t entry_type;		
	uint8_t entry_count;		
	uint8_t handle_count;
	uint8_t entry_status;

	__le32 handles[MAX_HANDLE_COUNT];
};

#define TSK_MGMT_IOCB_TYPE_FX00		0x05
struct tsk_mgmt_entry_fx00 {
	uint8_t entry_type;		
	uint8_t entry_count;		
	uint8_t sys_define;
	uint8_t entry_status;		

	uint32_t handle;		

	uint32_t reserved_0;

	__le16 tgt_id;		

	uint16_t reserved_1;
	uint16_t reserved_3;
	uint16_t reserved_4;

	struct scsi_lun lun;		

	__le32 control_flags;		

	uint8_t reserved_2[32];
};


#define	ABORT_IOCB_TYPE_FX00	0x08		
struct abort_iocb_entry_fx00 {
	uint8_t entry_type;		
	uint8_t entry_count;		
	uint8_t sys_define;		
	uint8_t entry_status;		

	uint32_t handle;		
	__le32 reserved_0;

	__le16 tgt_id_sts;		
	__le16 options;

	uint32_t abort_handle;		
	__le32 reserved_2;

	__le16 req_que_no;
	uint8_t reserved_1[38];
};

#define IOCTL_IOSB_TYPE_FX00	0x0C
struct ioctl_iocb_entry_fx00 {
	uint8_t entry_type;		
	uint8_t entry_count;		
	uint8_t sys_define;		
	uint8_t entry_status;		

	uint32_t handle;		
	uint32_t reserved_0;		

	uint16_t comp_func_num;
	__le16 fw_iotcl_flags;

	__le32 dataword_r;		
	uint32_t adapid;		
	uint32_t dataword_r_extra;

	__le32 seq_no;
	uint8_t reserved_2[20];
	uint32_t residuallen;
	__le32 status;
};

#define STATUS_CONT_TYPE_FX00 0x04

#define FX00_IOCB_TYPE		0x0B
struct fxdisc_entry_fx00 {
	uint8_t entry_type;		
	uint8_t entry_count;		
	uint8_t sys_define;		
	uint8_t entry_status;		

	uint32_t handle;		
	__le32 reserved_0;		

	__le16 func_num;
	__le16 req_xfrcnt;
	__le16 req_dsdcnt;
	__le16 rsp_xfrcnt;
	__le16 rsp_dsdcnt;
	uint8_t flags;
	uint8_t reserved_1;

	
	struct dsd64 dseg_rq[1];
	struct dsd64 dseg_rsp[1];

	__le32 dataword;
	__le32 adapid;
	__le32 adapid_hi;
	__le32 dataword_extra;
};

struct qlafx00_tgt_node_info {
	uint8_t tgt_node_wwpn[WWN_SIZE];
	uint8_t tgt_node_wwnn[WWN_SIZE];
	uint32_t tgt_node_state;
	uint8_t reserved[128];
	uint32_t reserved_1[8];
	uint64_t reserved_2[4];
} __packed;

#define QLAFX00_TGT_NODE_INFO sizeof(struct qlafx00_tgt_node_info)

#define QLAFX00_LINK_STATUS_DOWN	0x10
#define QLAFX00_LINK_STATUS_UP		0x11

#define QLAFX00_PORT_SPEED_2G	0x2
#define QLAFX00_PORT_SPEED_4G	0x4
#define QLAFX00_PORT_SPEED_8G	0x8
#define QLAFX00_PORT_SPEED_10G	0xa
struct port_info_data {
	uint8_t         port_state;
	uint8_t         port_type;
	uint16_t        port_identifier;
	uint32_t        up_port_state;
	uint8_t         fw_ver_num[32];
	uint8_t         portal_attrib;
	uint16_t        host_option;
	uint8_t         reset_delay;
	uint8_t         pdwn_retry_cnt;
	uint16_t        max_luns2tgt;
	uint8_t         risc_ver;
	uint8_t         pconn_option;
	uint16_t        risc_option;
	uint16_t        max_frame_len;
	uint16_t        max_iocb_alloc;
	uint16_t        exec_throttle;
	uint8_t         retry_cnt;
	uint8_t         retry_delay;
	uint8_t         port_name[8];
	uint8_t         port_id[3];
	uint8_t         link_status;
	uint8_t         plink_rate;
	uint32_t        link_config;
	uint16_t        adap_haddr;
	uint8_t         tgt_disc;
	uint8_t         log_tout;
	uint8_t         node_name[8];
	uint16_t        erisc_opt1;
	uint8_t         resp_acc_tmr;
	uint8_t         intr_del_tmr;
	uint8_t         erisc_opt2;
	uint8_t         alt_port_name[8];
	uint8_t         alt_node_name[8];
	uint8_t         link_down_tout;
	uint8_t         conn_type;
	uint8_t         fc_fw_mode;
	uint32_t        uiReserved[48];
} __packed;


#define OS_TYPE_UNKNOWN             0
#define OS_TYPE_LINUX               2


#define SYSNAME_LENGTH              128
#define NODENAME_LENGTH             64
#define RELEASE_LENGTH              64
#define VERSION_LENGTH              64
#define MACHINE_LENGTH              64
#define DOMNAME_LENGTH              64

struct host_system_info {
	uint32_t os_type;
	char    sysname[SYSNAME_LENGTH];
	char    nodename[NODENAME_LENGTH];
	char    release[RELEASE_LENGTH];
	char    version[VERSION_LENGTH];
	char    machine[MACHINE_LENGTH];
	char    domainname[DOMNAME_LENGTH];
	char    hostdriver[VERSION_LENGTH];
	uint32_t reserved[64];
} __packed;

struct register_host_info {
	struct host_system_info     hsi;	
	uint64_t        utc;			
	uint32_t        reserved[64];		
} __packed;


#define QLAFX00_PORT_DATA_INFO (sizeof(struct port_info_data))
#define QLAFX00_TGT_NODE_LIST_SIZE (sizeof(uint32_t) * 32)

struct config_info_data {
	uint8_t		model_num[16];
	uint8_t		model_description[80];
	uint8_t		reserved0[160];
	uint8_t		symbolic_name[64];
	uint8_t		serial_num[32];
	uint8_t		hw_version[16];
	uint8_t		fw_version[16];
	uint8_t		uboot_version[16];
	uint8_t		fru_serial_num[32];

	uint8_t		fc_port_count;
	uint8_t		iscsi_port_count;
	uint8_t		reserved1[2];

	uint8_t		mode;
	uint8_t		log_level;
	uint8_t		reserved2[2];

	uint32_t	log_size;

	uint8_t		tgt_pres_mode;
	uint8_t		iqn_flags;
	uint8_t		lun_mapping;

	uint64_t	adapter_id;

	uint32_t	cluster_key_len;
	uint8_t		cluster_key[16];

	uint64_t	cluster_master_id;
	uint64_t	cluster_slave_id;
	uint8_t		cluster_flags;
	uint32_t	enabled_capabilities;
	uint32_t	nominal_temp_value;
} __packed;

#define FXDISC_GET_CONFIG_INFO		0x01
#define FXDISC_GET_PORT_INFO		0x02
#define FXDISC_GET_TGT_NODE_INFO	0x80
#define FXDISC_GET_TGT_NODE_LIST	0x81
#define FXDISC_REG_HOST_INFO		0x99
#define FXDISC_ABORT_IOCTL		0xff

#define QLAFX00_HBA_ICNTRL_REG		0x20B08
#define QLAFX00_ICR_ENB_MASK            0x80000000
#define QLAFX00_ICR_DIS_MASK            0x7fffffff
#define QLAFX00_HST_RST_REG		0x18264
#define QLAFX00_SOC_TEMP_REG		0x184C4
#define QLAFX00_HST_TO_HBA_REG		0x20A04
#define QLAFX00_HBA_TO_HOST_REG		0x21B70
#define QLAFX00_HST_INT_STS_BITS	0x7
#define QLAFX00_BAR1_BASE_ADDR_REG	0x40018
#define QLAFX00_PEX0_WIN0_BASE_ADDR_REG	0x41824

#define QLAFX00_INTR_MB_CMPLT		0x1
#define QLAFX00_INTR_RSP_CMPLT		0x2
#define QLAFX00_INTR_ASYNC_CMPLT	0x4

#define QLAFX00_MBA_SYSTEM_ERR		0x8002
#define QLAFX00_MBA_TEMP_OVER		0x8005
#define QLAFX00_MBA_TEMP_NORM		0x8006
#define	QLAFX00_MBA_TEMP_CRIT		0x8007
#define QLAFX00_MBA_LINK_UP		0x8011
#define QLAFX00_MBA_LINK_DOWN		0x8012
#define QLAFX00_MBA_PORT_UPDATE		0x8014
#define QLAFX00_MBA_SHUTDOWN_RQSTD	0x8062

#define SOC_SW_RST_CONTROL_REG_CORE0     0x0020800
#define SOC_FABRIC_RST_CONTROL_REG       0x0020840
#define SOC_FABRIC_CONTROL_REG           0x0020200
#define SOC_FABRIC_CONFIG_REG            0x0020204
#define SOC_PWR_MANAGEMENT_PWR_DOWN_REG  0x001820C

#define SOC_INTERRUPT_SOURCE_I_CONTROL_REG     0x0020B00
#define SOC_CORE_TIMER_REG                     0x0021850
#define SOC_IRQ_ACK_REG                        0x00218b4

#define CONTINUE_A64_TYPE_FX00	0x03	

#define QLAFX00_SET_HST_INTR(ha, value) \
	wrt_reg_dword((ha)->cregbase + QLAFX00_HST_TO_HBA_REG, \
	value)

#define QLAFX00_CLR_HST_INTR(ha, value) \
	wrt_reg_dword((ha)->cregbase + QLAFX00_HBA_TO_HOST_REG, \
	~value)

#define QLAFX00_RD_INTR_REG(ha) \
	rd_reg_dword((ha)->cregbase + QLAFX00_HBA_TO_HOST_REG)

#define QLAFX00_CLR_INTR_REG(ha, value) \
	wrt_reg_dword((ha)->cregbase + QLAFX00_HBA_TO_HOST_REG, \
	~value)

#define QLAFX00_SET_HBA_SOC_REG(ha, off, val)\
	wrt_reg_dword((ha)->cregbase + off, val)

#define QLAFX00_GET_HBA_SOC_REG(ha, off)\
	rd_reg_dword((ha)->cregbase + off)

#define QLAFX00_HBA_RST_REG(ha, val)\
	wrt_reg_dword((ha)->cregbase + QLAFX00_HST_RST_REG, val)

#define QLAFX00_RD_ICNTRL_REG(ha) \
	rd_reg_dword((ha)->cregbase + QLAFX00_HBA_ICNTRL_REG)

#define QLAFX00_ENABLE_ICNTRL_REG(ha) \
	wrt_reg_dword((ha)->cregbase + QLAFX00_HBA_ICNTRL_REG, \
	(QLAFX00_GET_HBA_SOC_REG(ha, QLAFX00_HBA_ICNTRL_REG) | \
	 QLAFX00_ICR_ENB_MASK))

#define QLAFX00_DISABLE_ICNTRL_REG(ha) \
	wrt_reg_dword((ha)->cregbase + QLAFX00_HBA_ICNTRL_REG, \
	(QLAFX00_GET_HBA_SOC_REG(ha, QLAFX00_HBA_ICNTRL_REG) & \
	 QLAFX00_ICR_DIS_MASK))

#define QLAFX00_RD_REG(ha, off) \
	rd_reg_dword((ha)->cregbase + off)

#define QLAFX00_WR_REG(ha, off, val) \
	wrt_reg_dword((ha)->cregbase + off, val)

struct qla_mt_iocb_rqst_fx00 {
	__le32 reserved_0;

	__le16 func_type;
	uint8_t flags;
	uint8_t reserved_1;

	__le32 dataword;

	__le32 adapid;
	__le32 adapid_hi;

	__le32 dataword_extra;

	__le16 req_len;
	__le16 reserved_2;

	__le16 rsp_len;
	__le16 reserved_3;
};

struct qla_mt_iocb_rsp_fx00 {
	uint32_t reserved_1;

	uint16_t func_type;
	__le16 ioctl_flags;

	__le32 ioctl_data;

	uint32_t adapid;
	uint32_t adapid_hi;

	uint32_t reserved_2;
	__le32 seq_number;

	uint8_t reserved_3[20];

	int32_t res_count;

	__le32 status;
};


#define MAILBOX_REGISTER_COUNT_FX00	16
#define AEN_MAILBOX_REGISTER_COUNT_FX00	8
#define MAX_FIBRE_DEVICES_FX00	512
#define MAX_LUNS_FX00		0x1024
#define MAX_TARGETS_FX00	MAX_ISA_DEVICES
#define REQUEST_ENTRY_CNT_FX00		512	
#define RESPONSE_ENTRY_CNT_FX00		256	


#define FSTATE_FX00_CONFIG_WAIT     0x0000	
#define FSTATE_FX00_INITIALIZED     0x1000	

#define FX00_DEF_RATOV	10

struct mr_data_fx00 {
	uint8_t	symbolic_name[64];
	uint8_t	serial_num[32];
	uint8_t	hw_version[16];
	uint8_t	fw_version[16];
	uint8_t	uboot_version[16];
	uint8_t	fru_serial_num[32];
	fc_port_t       fcport;		
	uint8_t fw_hbt_en;
	uint8_t fw_hbt_cnt;
	uint8_t fw_hbt_miss_cnt;
	uint32_t old_fw_hbt_cnt;
	uint16_t fw_reset_timer_tick;
	uint8_t fw_reset_timer_exp;
	uint16_t fw_critemp_timer_tick;
	uint32_t old_aenmbx0_state;
	uint32_t critical_temperature;
	bool extended_io_enabled;
	bool host_info_resend;
	uint8_t hinfo_resend_timer_tick;
};

#define QLAFX00_EXTENDED_IO_EN_MASK    0x20


#define QLAFX00_GET_TEMPERATURE(ha) ((3153000 - (10000 * \
	((QLAFX00_RD_REG(ha, QLAFX00_SOC_TEMP_REG) & 0x3FE) >> 1))) / 13825)


#define QLAFX00_LOOP_DOWN_TIME		615     
#define QLAFX00_HEARTBEAT_INTERVAL	6	
#define QLAFX00_HEARTBEAT_MISS_CNT	3	
#define QLAFX00_RESET_INTERVAL		120	
#define QLAFX00_MAX_RESET_INTERVAL	600	
#define QLAFX00_CRITEMP_INTERVAL	60	
#define QLAFX00_HINFO_RESEND_INTERVAL	60	

#define QLAFX00_CRITEMP_THRSHLD		80	


#define QLAFX00_MAX_CANQUEUE		1024


#define QLAFX00_IOCTL_ICOB_ABORT_SUCCESS	0x68

#endif
