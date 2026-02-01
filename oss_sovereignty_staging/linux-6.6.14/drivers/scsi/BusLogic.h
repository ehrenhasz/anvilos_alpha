 
 

#ifndef _BUSLOGIC_H
#define _BUSLOGIC_H


#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

 

#define BLOGIC_MAX_ADAPTERS		16


 

#define BLOGIC_MAXDEV			16


 

#define BLOGIC_SG_LIMIT		128


 

#define BLOGIC_MAX_TAG_DEPTH		64
#define BLOGIC_MAX_AUTO_TAG_DEPTH	28
#define BLOGIC_MIN_AUTO_TAG_DEPTH	7
#define BLOGIC_TAG_DEPTH_BB		3
#define BLOGIC_UNTAG_DEPTH		3
#define BLOGIC_UNTAG_DEPTH_BB		2


 

#define BLOGIC_BUS_SETTLE_TIME		2


 

#define BLOGIC_MAX_MAILBOX		211


 

#define BLOGIC_CCB_GRP_ALLOCSIZE	7


 

#define BLOGIC_LINEBUF_SIZE		100
#define BLOGIC_MSGBUF_SIZE		9700


 

enum blogic_msglevel {
	BLOGIC_ANNOUNCE_LEVEL = 0,
	BLOGIC_INFO_LEVEL = 1,
	BLOGIC_NOTICE_LEVEL = 2,
	BLOGIC_WARN_LEVEL = 3,
	BLOGIC_ERR_LEVEL = 4
};

static char *blogic_msglevelmap[] = { KERN_NOTICE, KERN_NOTICE, KERN_NOTICE, KERN_WARNING, KERN_ERR };


 

#define blogic_announce(format, args...) \
	blogic_msg(BLOGIC_ANNOUNCE_LEVEL, format, ##args)

#define blogic_info(format, args...) \
	blogic_msg(BLOGIC_INFO_LEVEL, format, ##args)

#define blogic_notice(format, args...) \
	blogic_msg(BLOGIC_NOTICE_LEVEL, format, ##args)

#define blogic_warn(format, args...) \
	blogic_msg(BLOGIC_WARN_LEVEL, format, ##args)

#define blogic_err(format, args...) \
	blogic_msg(BLOGIC_ERR_LEVEL, format, ##args)


 

enum blogic_adapter_type {
	BLOGIC_MULTIMASTER = 1,
	BLOGIC_FLASHPOINT = 2
} PACKED;

#define BLOGIC_MULTIMASTER_ADDR_COUNT	4
#define BLOGIC_FLASHPOINT_ADDR_COUNT	256

static int blogic_adapter_addr_count[3] = { 0, BLOGIC_MULTIMASTER_ADDR_COUNT, BLOGIC_FLASHPOINT_ADDR_COUNT };


 

#ifdef CONFIG_SCSI_FLASHPOINT

#define blogic_multimaster_type(adapter) \
	(adapter->adapter_type == BLOGIC_MULTIMASTER)

#define blogic_flashpoint_type(adapter) \
	(adapter->adapter_type == BLOGIC_FLASHPOINT)

#else

#define blogic_multimaster_type(adapter)	(true)
#define blogic_flashpoint_type(adapter)		(false)

#endif


 

enum blogic_adapter_bus_type {
	BLOGIC_UNKNOWN_BUS = 0,
	BLOGIC_ISA_BUS = 1,
	BLOGIC_EISA_BUS = 2,
	BLOGIC_PCI_BUS = 3,
	BLOGIC_VESA_BUS = 4,
	BLOGIC_MCA_BUS = 5
} PACKED;

static char *blogic_adapter_busnames[] = { "Unknown", "ISA", "EISA", "PCI", "VESA", "MCA" };

static enum blogic_adapter_bus_type blogic_adater_bus_types[] = {
	BLOGIC_VESA_BUS,	 
	BLOGIC_ISA_BUS,		 
	BLOGIC_MCA_BUS,		 
	BLOGIC_EISA_BUS,	 
	BLOGIC_UNKNOWN_BUS,	 
	BLOGIC_PCI_BUS		 
};

 

enum blogic_bios_diskgeometry {
	BLOGIC_BIOS_NODISK = 0,
	BLOGIC_BIOS_DISK64x32 = 1,
	BLOGIC_BIOS_DISK128x32 = 2,
	BLOGIC_BIOS_DISK255x63 = 3
} PACKED;


 

struct blogic_byte_count {
	unsigned int units;
	unsigned int billions;
};


 

struct blogic_probeinfo {
	enum blogic_adapter_type adapter_type;
	enum blogic_adapter_bus_type adapter_bus_type;
	unsigned long io_addr;
	unsigned long pci_addr;
	struct pci_dev *pci_device;
	unsigned char bus;
	unsigned char dev;
	unsigned char irq_ch;
};

 

struct blogic_probe_options {
	bool noprobe:1;			 
	bool noprobe_pci:1;		 
	bool nosort_pci:1;		 
	bool multimaster_first:1;	 
	bool flashpoint_first:1;	 
};

 

struct blogic_global_options {
	bool trace_probe:1;	 
	bool trace_hw_reset:1;	 
	bool trace_config:1;	 
	bool trace_err:1;	 
};

 

#define BLOGIC_CNTRL_REG	0	 
#define BLOGIC_STATUS_REG	0	 
#define BLOGIC_CMD_PARM_REG	1	 
#define BLOGIC_DATAIN_REG	1	 
#define BLOGIC_INT_REG		2	 
#define BLOGIC_GEOMETRY_REG	3	 

 

union blogic_cntrl_reg {
	unsigned char all;
	struct {
		unsigned char:4;	 
		bool bus_reset:1;	 
		bool int_reset:1;	 
		bool soft_reset:1;	 
		bool hard_reset:1;	 
	} cr;
};

 

union blogic_stat_reg {
	unsigned char all;
	struct {
		bool cmd_invalid:1;	 
		bool rsvd:1;		 
		bool datain_ready:1;	 
		bool cmd_param_busy:1;	 
		bool adapter_ready:1;	 
		bool init_reqd:1;	 
		bool diag_failed:1;	 
		bool diag_active:1;	 
	} sr;
};

 

union blogic_int_reg {
	unsigned char all;
	struct {
		bool mailin_loaded:1;	 
		bool mailout_avail:1;	 
		bool cmd_complete:1;	 
		bool ext_busreset:1;	 
		unsigned char rsvd:3;	 
		bool int_valid:1;	 
	} ir;
};

 

union blogic_geo_reg {
	unsigned char all;
	struct {
		enum blogic_bios_diskgeometry d0_geo:2;	 
		enum blogic_bios_diskgeometry d1_geo:2;	 
		unsigned char:3;	 
		bool ext_trans_enable:1;	 
	} gr;
};

 

enum blogic_opcode {
	BLOGIC_TEST_CMP_COMPLETE = 0x00,
	BLOGIC_INIT_MBOX = 0x01,
	BLOGIC_EXEC_MBOX_CMD = 0x02,
	BLOGIC_EXEC_BIOS_CMD = 0x03,
	BLOGIC_GET_BOARD_ID = 0x04,
	BLOGIC_ENABLE_OUTBOX_AVAIL_INT = 0x05,
	BLOGIC_SET_SELECT_TIMEOUT = 0x06,
	BLOGIC_SET_PREEMPT_TIME = 0x07,
	BLOGIC_SET_TIMEOFF_BUS = 0x08,
	BLOGIC_SET_TXRATE = 0x09,
	BLOGIC_INQ_DEV0TO7 = 0x0A,
	BLOGIC_INQ_CONFIG = 0x0B,
	BLOGIC_TGT_MODE = 0x0C,
	BLOGIC_INQ_SETUPINFO = 0x0D,
	BLOGIC_WRITE_LOCALRAM = 0x1A,
	BLOGIC_READ_LOCALRAM = 0x1B,
	BLOGIC_WRITE_BUSMASTER_FIFO = 0x1C,
	BLOGIC_READ_BUSMASTER_FIFO = 0x1D,
	BLOGIC_ECHO_CMDDATA = 0x1F,
	BLOGIC_ADAPTER_DIAG = 0x20,
	BLOGIC_SET_OPTIONS = 0x21,
	BLOGIC_INQ_DEV8TO15 = 0x23,
	BLOGIC_INQ_DEV = 0x24,
	BLOGIC_DISABLE_INT = 0x25,
	BLOGIC_INIT_EXT_MBOX = 0x81,
	BLOGIC_EXEC_SCS_CMD = 0x83,
	BLOGIC_INQ_FWVER_D3 = 0x84,
	BLOGIC_INQ_FWVER_LETTER = 0x85,
	BLOGIC_INQ_PCI_INFO = 0x86,
	BLOGIC_INQ_MODELNO = 0x8B,
	BLOGIC_INQ_SYNC_PERIOD = 0x8C,
	BLOGIC_INQ_EXTSETUP = 0x8D,
	BLOGIC_STRICT_RR = 0x8F,
	BLOGIC_STORE_LOCALRAM = 0x90,
	BLOGIC_FETCH_LOCALRAM = 0x91,
	BLOGIC_STORE_TO_EEPROM = 0x92,
	BLOGIC_LOAD_AUTOSCSICODE = 0x94,
	BLOGIC_MOD_IOADDR = 0x95,
	BLOGIC_SETCCB_FMT = 0x96,
	BLOGIC_WRITE_INQBUF = 0x9A,
	BLOGIC_READ_INQBUF = 0x9B,
	BLOGIC_FLASH_LOAD = 0xA7,
	BLOGIC_READ_SCAMDATA = 0xA8,
	BLOGIC_WRITE_SCAMDATA = 0xA9
};

 

struct blogic_board_id {
	unsigned char type;		 
	unsigned char custom_features;	 
	unsigned char fw_ver_digit1;	 
	unsigned char fw_ver_digit2;	 
};

 

struct blogic_config {
	unsigned char:5;	 
	bool dma_ch5:1;		 
	bool dma_ch6:1;		 
	bool dma_ch7:1;		 
	bool irq_ch9:1;		 
	bool irq_ch10:1;	 
	bool irq_ch11:1;	 
	bool irq_ch12:1;	 
	unsigned char:1;	 
	bool irq_ch14:1;	 
	bool irq_ch15:1;	 
	unsigned char:1;	 
	unsigned char id:4;	 
	unsigned char:4;	 
};

 

struct blogic_syncval {
	unsigned char offset:4;		 
	unsigned char tx_period:3;	 
	bool sync:1;			 
};

struct blogic_setup_info {
	bool sync:1;				 
	bool parity:1;				 
	unsigned char:6;			 
	unsigned char tx_rate;			 
	unsigned char preempt_time;		 
	unsigned char timeoff_bus;		 
	unsigned char mbox_count;		 
	unsigned char mbox_addr[3];		 
	struct blogic_syncval sync0to7[8];	 
	unsigned char disconnect_ok0to7;	 
	unsigned char sig;			 
	unsigned char char_d;			 
	unsigned char bus_type;			 
	unsigned char wide_tx_ok0to7;		 
	unsigned char wide_tx_active0to7;	 
	struct blogic_syncval sync8to15[8];	 
	unsigned char disconnect_ok8to15;	 
	unsigned char:8;			 
	unsigned char wide_tx_ok8to15;		 
	unsigned char wide_tx_active8to15;	 
};

 

struct blogic_extmbox_req {
	unsigned char mbox_count;	 
	u32 base_mbox_addr;		 
} PACKED;


 

enum blogic_isa_ioport {
	BLOGIC_IO_330 = 0,
	BLOGIC_IO_334 = 1,
	BLOGIC_IO_230 = 2,
	BLOGIC_IO_234 = 3,
	BLOGIC_IO_130 = 4,
	BLOGIC_IO_134 = 5,
	BLOGIC_IO_DISABLE = 6,
	BLOGIC_IO_DISABLE2 = 7
} PACKED;

struct blogic_adapter_info {
	enum blogic_isa_ioport isa_port;	 
	unsigned char irq_ch;		 
	bool low_term:1;		 
	bool high_term:1;		 
	unsigned char:2;		 
	bool JP1:1;			 
	bool JP2:1;			 
	bool JP3:1;			 
	bool genericinfo_valid:1;	 
	unsigned char:8;		 
};

 

struct blogic_ext_setup {
	unsigned char bus_type;		 
	unsigned char bios_addr;	 
	unsigned short sg_limit;	 
	unsigned char mbox_count;	 
	u32 base_mbox_addr;		 
	struct {
		unsigned char:2;	 
		bool fast_on_eisa:1;	 
		unsigned char:3;	 
		bool level_int:1;	 
		unsigned char:1;	 
	} misc;
	unsigned char fw_rev[3];	 
	bool wide:1;			 
	bool differential:1;		 
	bool scam:1;			 
	bool ultra:1;			 
	bool smart_term:1;		 
	unsigned char:3;		 
} PACKED;

 

enum blogic_rr_req {
	BLOGIC_AGGRESSIVE_RR = 0,
	BLOGIC_STRICT_RR_MODE = 1
} PACKED;


 

#define BLOGIC_BIOS_BASE		0
#define BLOGIC_AUTOSCSI_BASE		64

struct blogic_fetch_localram {
	unsigned char offset;	 
	unsigned char count;	 
};

 

struct blogic_autoscsi {
	unsigned char factory_sig[2];		 
	unsigned char info_bytes;		 
	unsigned char adapter_type[6];		 
	unsigned char:8;			 
	bool floppy:1;				 
	bool floppy_sec:1;			 
	bool level_int:1;			 
	unsigned char:2;			 
	unsigned char systemram_bios:3;		 
	unsigned char dma_ch:7;			 
	bool dma_autoconf:1;			 
	unsigned char irq_ch:7;			 
	bool irq_autoconf:1;			 
	unsigned char dma_tx_rate;		 
	unsigned char scsi_id;			 
	bool low_term:1;			 
	bool parity:1;				 
	bool high_term:1;			 
	bool noisy_cable:1;			 
	bool fast_sync_neg:1;			 
	bool reset_enabled:1;			 
	bool:1;					 
	bool active_negation:1;			 
	unsigned char bus_on_delay;		 
	unsigned char bus_off_delay;		 
	bool bios_enabled:1;			 
	bool int19_redir_enabled:1;		 
	bool ext_trans_enable:1;		 
	bool removable_as_fixed:1;		 
	bool:1;					 
	bool morethan2_drives:1;		 
	bool bios_int:1;			 
	bool floptical:1;			 
	unsigned short dev_enabled;		 
	unsigned short wide_ok;			 
	unsigned short fast_ok;			 
	unsigned short sync_ok;			 
	unsigned short discon_ok;		 
	unsigned short send_start_unit;		 
	unsigned short ignore_bios_scan;	 
	unsigned char pci_int_pin:2;		 
	unsigned char adapter_ioport:2;		 
	bool strict_rr_enabled:1;		 
	bool vesabus_33mhzplus:1;		 
	bool vesa_burst_write:1;		 
	bool vesa_burst_read:1;			 
	unsigned short ultra_ok;		 
	unsigned int:32;			 
	unsigned char:8;			 
	unsigned char autoscsi_maxlun;		 
	bool:1;					 
	bool scam_dominant:1;			 
	bool scam_enabled:1;			 
	bool scam_lev2:1;			 
	unsigned char:4;			 
	bool int13_exten:1;			 
	bool:1;					 
	bool cd_boot:1;				 
	unsigned char:5;			 
	unsigned char boot_id:4;		 
	unsigned char boot_ch:4;		 
	unsigned char force_scan_order:1;	 
	unsigned char:7;			 
	unsigned short nontagged_to_alt_ok;	 
	unsigned short reneg_sync_on_check;	 
	unsigned char rsvd[10];			 
	unsigned char manuf_diag[2];		 
	unsigned short cksum;			 
} PACKED;

 

struct blogic_autoscsi_byte45 {
	unsigned char force_scan_order:1;	 
	unsigned char:7;	 
};

 

#define BLOGIC_BIOS_DRVMAP		17

struct blogic_bios_drvmap {
	unsigned char tgt_idbit3:1;			 
	unsigned char:2;				 
	enum blogic_bios_diskgeometry diskgeom:2;	 
	unsigned char tgt_id:3;				 
};

 

enum blogic_setccb_fmt {
	BLOGIC_LEGACY_LUN_CCB = 0,
	BLOGIC_EXT_LUN_CCB = 1
} PACKED;

 

enum blogic_action {
	BLOGIC_OUTBOX_FREE = 0x00,
	BLOGIC_MBOX_START = 0x01,
	BLOGIC_MBOX_ABORT = 0x02
} PACKED;


 

enum blogic_cmplt_code {
	BLOGIC_INBOX_FREE = 0x00,
	BLOGIC_CMD_COMPLETE_GOOD = 0x01,
	BLOGIC_CMD_ABORT_BY_HOST = 0x02,
	BLOGIC_CMD_NOTFOUND = 0x03,
	BLOGIC_CMD_COMPLETE_ERROR = 0x04,
	BLOGIC_INVALID_CCB = 0x05
} PACKED;

 

enum blogic_ccb_opcode {
	BLOGIC_INITIATOR_CCB = 0x00,
	BLOGIC_TGT_CCB = 0x01,
	BLOGIC_INITIATOR_CCB_SG = 0x02,
	BLOGIC_INITIATOR_CCBB_RESIDUAL = 0x03,
	BLOGIC_INITIATOR_CCB_SG_RESIDUAL = 0x04,
	BLOGIC_BDR = 0x81
} PACKED;


 

enum blogic_datadir {
	BLOGIC_UNCHECKED_TX = 0,
	BLOGIC_DATAIN_CHECKED = 1,
	BLOGIC_DATAOUT_CHECKED = 2,
	BLOGIC_NOTX = 3
};


 

enum blogic_adapter_status {
	BLOGIC_CMD_CMPLT_NORMAL = 0x00,
	BLOGIC_LINK_CMD_CMPLT = 0x0A,
	BLOGIC_LINK_CMD_CMPLT_FLAG = 0x0B,
	BLOGIC_DATA_UNDERRUN = 0x0C,
	BLOGIC_SELECT_TIMEOUT = 0x11,
	BLOGIC_DATA_OVERRUN = 0x12,
	BLOGIC_NOEXPECT_BUSFREE = 0x13,
	BLOGIC_INVALID_BUSPHASE = 0x14,
	BLOGIC_INVALID_OUTBOX_CODE = 0x15,
	BLOGIC_INVALID_CMD_CODE = 0x16,
	BLOGIC_LINKCCB_BADLUN = 0x17,
	BLOGIC_BAD_CMD_PARAM = 0x1A,
	BLOGIC_AUTOREQSENSE_FAIL = 0x1B,
	BLOGIC_TAGQUEUE_REJECT = 0x1C,
	BLOGIC_BAD_MSG_RCVD = 0x1D,
	BLOGIC_HW_FAIL = 0x20,
	BLOGIC_NORESPONSE_TO_ATN = 0x21,
	BLOGIC_HW_RESET = 0x22,
	BLOGIC_RST_FROM_OTHERDEV = 0x23,
	BLOGIC_BAD_RECONNECT = 0x24,
	BLOGIC_HW_BDR = 0x25,
	BLOGIC_ABRT_QUEUE = 0x26,
	BLOGIC_ADAPTER_SW_ERROR = 0x27,
	BLOGIC_HW_TIMEOUT = 0x30,
	BLOGIC_PARITY_ERR = 0x34
} PACKED;


 

enum blogic_tgt_status {
	BLOGIC_OP_GOOD = 0x00,
	BLOGIC_CHECKCONDITION = 0x02,
	BLOGIC_DEVBUSY = 0x08
} PACKED;

 

enum blogic_queuetag {
	BLOGIC_SIMPLETAG = 0,
	BLOGIC_HEADTAG = 1,
	BLOGIC_ORDEREDTAG = 2,
	BLOGIC_RSVDTAG = 3
};

 

#define BLOGIC_CDB_MAXLEN			12


 

struct blogic_sg_seg {
	u32 segbytes;	 
	u32 segdata;	 
};

 

enum blogic_ccb_status {
	BLOGIC_CCB_FREE = 0,
	BLOGIC_CCB_ACTIVE = 1,
	BLOGIC_CCB_COMPLETE = 2,
	BLOGIC_CCB_RESET = 3
} PACKED;


 

struct blogic_ccb {
	 
	enum blogic_ccb_opcode opcode;			 
	unsigned char:3;				 
	enum blogic_datadir datadir:2;			 
	bool tag_enable:1;				 
	enum blogic_queuetag queuetag:2;		 
	unsigned char cdblen;				 
	unsigned char sense_datalen;			 
	u32 datalen;					 
	u32 data;					 
	unsigned char:8;				 
	unsigned char:8;				 
	enum blogic_adapter_status adapter_status;	 
	enum blogic_tgt_status tgt_status;		 
	unsigned char tgt_id;				 
	unsigned char lun:5;				 
	bool legacytag_enable:1;			 
	enum blogic_queuetag legacy_tag:2;		 
	unsigned char cdb[BLOGIC_CDB_MAXLEN];		 
	unsigned char:8;				 
	unsigned char:8;				 
	u32 rsvd_int;					 
	u32 sensedata;					 
	 
	void (*callback) (struct blogic_ccb *);		 
	u32 base_addr;					 
	enum blogic_cmplt_code comp_code;		 
#ifdef CONFIG_SCSI_FLASHPOINT
	unsigned char:8;				 
	u16 os_flags;					 
	unsigned char private[24];			 
	void *rsvd1;
	void *rsvd2;
	unsigned char private2[16];
#endif
	 
	dma_addr_t allocgrp_head;
	unsigned int allocgrp_size;
	u32 dma_handle;
	enum blogic_ccb_status status;
	unsigned long serial;
	struct scsi_cmnd *command;
	struct blogic_adapter *adapter;
	struct blogic_ccb *next;
	struct blogic_ccb *next_all;
	struct blogic_sg_seg sglist[BLOGIC_SG_LIMIT];
};

 

struct blogic_outbox {
	u32 ccb;			 
	u32:24;				 
	enum blogic_action action;	 
};

 

struct blogic_inbox {
	u32 ccb;					 
	enum blogic_adapter_status adapter_status;	 
	enum blogic_tgt_status tgt_status;		 
	unsigned char:8;				 
	enum blogic_cmplt_code comp_code;		 
};


 

struct blogic_drvr_options {
	unsigned short tagq_ok;
	unsigned short tagq_ok_mask;
	unsigned short bus_settle_time;
	unsigned short stop_tgt_inquiry;
	unsigned char common_qdepth;
	unsigned char qdepth[BLOGIC_MAXDEV];
};

 

struct blogic_tgt_flags {
	bool tgt_exists:1;
	bool tagq_ok:1;
	bool wide_ok:1;
	bool tagq_active:1;
	bool wide_active:1;
	bool cmd_good:1;
	bool tgt_info_in:1;
};

 

#define BLOGIC_SZ_BUCKETS			10

struct blogic_tgt_stats {
	unsigned int cmds_tried;
	unsigned int cmds_complete;
	unsigned int read_cmds;
	unsigned int write_cmds;
	struct blogic_byte_count bytesread;
	struct blogic_byte_count byteswritten;
	unsigned int read_sz_buckets[BLOGIC_SZ_BUCKETS];
	unsigned int write_sz_buckets[BLOGIC_SZ_BUCKETS];
	unsigned short aborts_request;
	unsigned short aborts_tried;
	unsigned short aborts_done;
	unsigned short bdr_request;
	unsigned short bdr_tried;
	unsigned short bdr_done;
	unsigned short adapter_reset_req;
	unsigned short adapter_reset_attempt;
	unsigned short adapter_reset_done;
};

 

#define FPOINT_BADCARD_HANDLE		0xFFFFFFFFL


 

struct fpoint_info {
	u32 base_addr;				 
	bool present;				 
	unsigned char irq_ch;			 
	unsigned char scsi_id;			 
	unsigned char scsi_lun;			 
	u16 fw_rev;				 
	u16 sync_ok;				 
	u16 fast_ok;				 
	u16 ultra_ok;				 
	u16 discon_ok;				 
	u16 wide_ok;				 
	bool parity:1;				 
	bool wide:1;				 
	bool softreset:1;			 
	bool ext_trans_enable:1;		 
	bool low_term:1;			 
	bool high_term:1;			 
	bool report_underrun:1;			 
	bool scam_enabled:1;			 
	bool scam_lev2:1;			 
	unsigned char:7;			 
	unsigned char family;			 
	unsigned char bus_type;			 
	unsigned char model[3];			 
	unsigned char relative_cardnum;		 
	unsigned char rsvd[4];			 
	u32 os_rsvd;				 
	unsigned char translation_info[4];	 
	u32 rsvd2[5];				 
	u32 sec_range;				 
};

 

struct blogic_adapter {
	struct Scsi_Host *scsi_host;
	struct pci_dev *pci_device;
	enum blogic_adapter_type adapter_type;
	enum blogic_adapter_bus_type adapter_bus_type;
	unsigned long io_addr;
	unsigned long pci_addr;
	unsigned short addr_count;
	unsigned char host_no;
	unsigned char model[9];
	unsigned char fw_ver[6];
	unsigned char full_model[18];
	unsigned char bus;
	unsigned char dev;
	unsigned char irq_ch;
	unsigned char scsi_id;
	bool irq_acquired:1;
	bool ext_trans_enable:1;
	bool parity:1;
	bool reset_enabled:1;
	bool level_int:1;
	bool wide:1;
	bool differential:1;
	bool scam:1;
	bool ultra:1;
	bool ext_lun:1;
	bool terminfo_valid:1;
	bool low_term:1;
	bool high_term:1;
	bool strict_rr:1;
	bool scam_enabled:1;
	bool scam_lev2:1;
	bool adapter_initd:1;
	bool adapter_extreset:1;
	bool adapter_intern_err:1;
	bool processing_ccbs;
	volatile bool adapter_cmd_complete;
	unsigned short adapter_sglimit;
	unsigned short drvr_sglimit;
	unsigned short maxdev;
	unsigned short maxlun;
	unsigned short mbox_count;
	unsigned short initccbs;
	unsigned short inc_ccbs;
	unsigned short alloc_ccbs;
	unsigned short drvr_qdepth;
	unsigned short adapter_qdepth;
	unsigned short untag_qdepth;
	unsigned short common_qdepth;
	unsigned short bus_settle_time;
	unsigned short sync_ok;
	unsigned short fast_ok;
	unsigned short ultra_ok;
	unsigned short wide_ok;
	unsigned short discon_ok;
	unsigned short tagq_ok;
	unsigned short ext_resets;
	unsigned short adapter_intern_errors;
	unsigned short tgt_count;
	unsigned short msgbuflen;
	u32 bios_addr;
	struct blogic_drvr_options *drvr_opts;
	struct fpoint_info fpinfo;
	void *cardhandle;
	struct list_head host_list;
	struct blogic_ccb *all_ccbs;
	struct blogic_ccb *free_ccbs;
	struct blogic_ccb *firstccb;
	struct blogic_ccb *lastccb;
	struct blogic_ccb *bdr_pend[BLOGIC_MAXDEV];
	struct blogic_tgt_flags tgt_flags[BLOGIC_MAXDEV];
	unsigned char qdepth[BLOGIC_MAXDEV];
	unsigned char sync_period[BLOGIC_MAXDEV];
	unsigned char sync_offset[BLOGIC_MAXDEV];
	unsigned char active_cmds[BLOGIC_MAXDEV];
	unsigned int cmds_since_rst[BLOGIC_MAXDEV];
	unsigned long last_seqpoint[BLOGIC_MAXDEV];
	unsigned long last_resettried[BLOGIC_MAXDEV];
	unsigned long last_resetdone[BLOGIC_MAXDEV];
	struct blogic_outbox *first_outbox;
	struct blogic_outbox *last_outbox;
	struct blogic_outbox *next_outbox;
	struct blogic_inbox *first_inbox;
	struct blogic_inbox *last_inbox;
	struct blogic_inbox *next_inbox;
	struct blogic_tgt_stats tgt_stats[BLOGIC_MAXDEV];
	unsigned char *mbox_space;
	dma_addr_t mbox_space_handle;
	unsigned int mbox_sz;
	unsigned long ccb_offset;
	char msgbuf[BLOGIC_MSGBUF_SIZE];
};

 

struct bios_diskparam {
	int heads;
	int sectors;
	int cylinders;
};

 

struct scsi_inquiry {
	unsigned char devtype:5;	 
	unsigned char dev_qual:3;	 
	unsigned char dev_modifier:7;	 
	bool rmb:1;			 
	unsigned char ansi_ver:3;	 
	unsigned char ecma_ver:3;	 
	unsigned char iso_ver:2;	 
	unsigned char resp_fmt:4;	 
	unsigned char:2;		 
	bool TrmIOP:1;			 
	bool AENC:1;			 
	unsigned char addl_len;		 
	unsigned char:8;		 
	unsigned char:8;		 
	bool SftRe:1;			 
	bool CmdQue:1;			 
	bool:1;				 
	bool linked:1;			 
	bool sync:1;			 
	bool WBus16:1;			 
	bool WBus32:1;			 
	bool RelAdr:1;			 
	unsigned char vendor[8];	 
	unsigned char product[16];	 
	unsigned char product_rev[4];	 
};


 

static inline void blogic_busreset(struct blogic_adapter *adapter)
{
	union blogic_cntrl_reg cr;
	cr.all = 0;
	cr.cr.bus_reset = true;
	outb(cr.all, adapter->io_addr + BLOGIC_CNTRL_REG);
}

static inline void blogic_intreset(struct blogic_adapter *adapter)
{
	union blogic_cntrl_reg cr;
	cr.all = 0;
	cr.cr.int_reset = true;
	outb(cr.all, adapter->io_addr + BLOGIC_CNTRL_REG);
}

static inline void blogic_softreset(struct blogic_adapter *adapter)
{
	union blogic_cntrl_reg cr;
	cr.all = 0;
	cr.cr.soft_reset = true;
	outb(cr.all, adapter->io_addr + BLOGIC_CNTRL_REG);
}

static inline void blogic_hardreset(struct blogic_adapter *adapter)
{
	union blogic_cntrl_reg cr;
	cr.all = 0;
	cr.cr.hard_reset = true;
	outb(cr.all, adapter->io_addr + BLOGIC_CNTRL_REG);
}

static inline unsigned char blogic_rdstatus(struct blogic_adapter *adapter)
{
	return inb(adapter->io_addr + BLOGIC_STATUS_REG);
}

static inline void blogic_setcmdparam(struct blogic_adapter *adapter,
					unsigned char value)
{
	outb(value, adapter->io_addr + BLOGIC_CMD_PARM_REG);
}

static inline unsigned char blogic_rddatain(struct blogic_adapter *adapter)
{
	return inb(adapter->io_addr + BLOGIC_DATAIN_REG);
}

static inline unsigned char blogic_rdint(struct blogic_adapter *adapter)
{
	return inb(adapter->io_addr + BLOGIC_INT_REG);
}

static inline unsigned char blogic_rdgeom(struct blogic_adapter *adapter)
{
	return inb(adapter->io_addr + BLOGIC_GEOMETRY_REG);
}

 

static inline void blogic_execmbox(struct blogic_adapter *adapter)
{
	blogic_setcmdparam(adapter, BLOGIC_EXEC_MBOX_CMD);
}

 

static inline void blogic_delay(int seconds)
{
	mdelay(1000 * seconds);
}

 

static inline u32 virt_to_32bit_virt(void *virt_addr)
{
	return (u32) (unsigned long) virt_addr;
}

 

static inline void blogic_inc_count(unsigned short *count)
{
	if (*count < 65535)
		(*count)++;
}

 

static inline void blogic_addcount(struct blogic_byte_count *bytecount,
					unsigned int amount)
{
	bytecount->units += amount;
	if (bytecount->units > 999999999) {
		bytecount->units -= 1000000000;
		bytecount->billions++;
	}
}

 

static inline void blogic_incszbucket(unsigned int *cmdsz_buckets,
					unsigned int amount)
{
	int index = 0;
	if (amount < 8 * 1024) {
		if (amount < 2 * 1024)
			index = (amount < 1 * 1024 ? 0 : 1);
		else
			index = (amount < 4 * 1024 ? 2 : 3);
	} else if (amount < 128 * 1024) {
		if (amount < 32 * 1024)
			index = (amount < 16 * 1024 ? 4 : 5);
		else
			index = (amount < 64 * 1024 ? 6 : 7);
	} else
		index = (amount < 256 * 1024 ? 8 : 9);
	cmdsz_buckets[index]++;
}

 

#define FLASHPOINT_FW_VER		"5.02"

 

#define FPOINT_NORMAL_INT		0x00
#define FPOINT_INTERN_ERR		0xFE
#define FPOINT_EXT_RESET		0xFF

 

static const char *blogic_drvr_info(struct Scsi_Host *);
static int blogic_qcmd(struct Scsi_Host *h, struct scsi_cmnd *);
static int blogic_diskparam(struct scsi_device *, struct block_device *, sector_t, int *);
static int blogic_slaveconfig(struct scsi_device *);
static void blogic_qcompleted_ccb(struct blogic_ccb *);
static irqreturn_t blogic_inthandler(int, void *);
static int blogic_resetadapter(struct blogic_adapter *, bool hard_reset);
static void blogic_msg(enum blogic_msglevel, char *, struct blogic_adapter *, ...);
static int __init blogic_setup(char *);

#endif				 
