


#ifndef _MYRS_H
#define _MYRS_H

#define MYRS_MAILBOX_TIMEOUT 1000000

#define MYRS_DCMD_TAG 1
#define MYRS_MCMD_TAG 2

#define MYRS_LINE_BUFFER_SIZE 128

#define MYRS_PRIMARY_MONITOR_INTERVAL (10 * HZ)
#define MYRS_SECONDARY_MONITOR_INTERVAL (60 * HZ)


#define MYRS_SG_LIMIT		128


#define MYRS_MAX_CMD_MBOX		512
#define MYRS_MAX_STAT_MBOX		512

#define MYRS_DCDB_SIZE			16
#define MYRS_SENSE_SIZE			14


enum myrs_cmd_opcode {
	MYRS_CMD_OP_MEMCOPY		= 0x01,
	MYRS_CMD_OP_SCSI_10_PASSTHRU	= 0x02,
	MYRS_CMD_OP_SCSI_255_PASSTHRU	= 0x03,
	MYRS_CMD_OP_SCSI_10		= 0x04,
	MYRS_CMD_OP_SCSI_256		= 0x05,
	MYRS_CMD_OP_IOCTL		= 0x20,
} __packed;


enum myrs_ioctl_opcode {
	MYRS_IOCTL_GET_CTLR_INFO	= 0x01,
	MYRS_IOCTL_GET_LDEV_INFO_VALID	= 0x03,
	MYRS_IOCTL_GET_PDEV_INFO_VALID	= 0x05,
	MYRS_IOCTL_GET_HEALTH_STATUS	= 0x11,
	MYRS_IOCTL_GET_EVENT		= 0x15,
	MYRS_IOCTL_START_DISCOVERY	= 0x81,
	MYRS_IOCTL_SET_DEVICE_STATE	= 0x82,
	MYRS_IOCTL_INIT_PDEV_START	= 0x84,
	MYRS_IOCTL_INIT_PDEV_STOP	= 0x85,
	MYRS_IOCTL_INIT_LDEV_START	= 0x86,
	MYRS_IOCTL_INIT_LDEV_STOP	= 0x87,
	MYRS_IOCTL_RBLD_DEVICE_START	= 0x88,
	MYRS_IOCTL_RBLD_DEVICE_STOP	= 0x89,
	MYRS_IOCTL_MAKE_CONSISTENT_START = 0x8A,
	MYRS_IOCTL_MAKE_CONSISTENT_STOP = 0x8B,
	MYRS_IOCTL_CC_START		= 0x8C,
	MYRS_IOCTL_CC_STOP		= 0x8D,
	MYRS_IOCTL_SET_MEM_MBOX		= 0x8E,
	MYRS_IOCTL_RESET_DEVICE		= 0x90,
	MYRS_IOCTL_FLUSH_DEVICE_DATA	= 0x91,
	MYRS_IOCTL_PAUSE_DEVICE		= 0x92,
	MYRS_IOCTL_UNPAUS_EDEVICE	= 0x93,
	MYRS_IOCTL_LOCATE_DEVICE	= 0x94,
	MYRS_IOCTL_CREATE_CONFIGURATION = 0xC0,
	MYRS_IOCTL_DELETE_LDEV		= 0xC1,
	MYRS_IOCTL_REPLACE_INTERNALDEVICE = 0xC2,
	MYRS_IOCTL_RENAME_LDEV		= 0xC3,
	MYRS_IOCTL_ADD_CONFIGURATION	= 0xC4,
	MYRS_IOCTL_XLATE_PDEV_TO_LDEV	= 0xC5,
	MYRS_IOCTL_CLEAR_CONFIGURATION	= 0xCA,
} __packed;


#define MYRS_STATUS_SUCCESS			0x00
#define MYRS_STATUS_FAILED			0x02
#define MYRS_STATUS_DEVICE_BUSY			0x08
#define MYRS_STATUS_DEVICE_NON_RESPONSIVE	0x0E
#define MYRS_STATUS_DEVICE_NON_RESPONSIVE2	0x0F
#define MYRS_STATUS_RESERVATION_CONFLICT	0x18


struct myrs_mem_type {
	enum {
		MYRS_MEMTYPE_RESERVED	= 0x00,
		MYRS_MEMTYPE_DRAM	= 0x01,
		MYRS_MEMTYPE_EDRAM	= 0x02,
		MYRS_MEMTYPE_EDO	= 0x03,
		MYRS_MEMTYPE_SDRAM	= 0x04,
		MYRS_MEMTYPE_LAST	= 0x1F,
	} __packed mem_type:5;	
	unsigned rsvd:1;			
	unsigned mem_parity:1;			
	unsigned mem_ecc:1;			
};


enum myrs_cpu_type {
	MYRS_CPUTYPE_i960CA	= 0x01,
	MYRS_CPUTYPE_i960RD	= 0x02,
	MYRS_CPUTYPE_i960RN	= 0x03,
	MYRS_CPUTYPE_i960RP	= 0x04,
	MYRS_CPUTYPE_NorthBay	= 0x05,
	MYRS_CPUTYPE_StrongArm	= 0x06,
	MYRS_CPUTYPE_i960RM	= 0x07,
} __packed;


struct myrs_ctlr_info {
	unsigned char rsvd1;				
	enum {
		MYRS_SCSI_BUS	= 0x00,
		MYRS_Fibre_BUS	= 0x01,
		MYRS_PCI_BUS	= 0x03
	} __packed bus;	
	enum {
		MYRS_CTLR_DAC960E	= 0x01,
		MYRS_CTLR_DAC960M	= 0x08,
		MYRS_CTLR_DAC960PD	= 0x10,
		MYRS_CTLR_DAC960PL	= 0x11,
		MYRS_CTLR_DAC960PU	= 0x12,
		MYRS_CTLR_DAC960PE	= 0x13,
		MYRS_CTLR_DAC960PG	= 0x14,
		MYRS_CTLR_DAC960PJ	= 0x15,
		MYRS_CTLR_DAC960PTL0	= 0x16,
		MYRS_CTLR_DAC960PR	= 0x17,
		MYRS_CTLR_DAC960PRL	= 0x18,
		MYRS_CTLR_DAC960PT	= 0x19,
		MYRS_CTLR_DAC1164P	= 0x1A,
		MYRS_CTLR_DAC960PTL1	= 0x1B,
		MYRS_CTLR_EXR2000P	= 0x1C,
		MYRS_CTLR_EXR3000P	= 0x1D,
		MYRS_CTLR_ACCELERAID352 = 0x1E,
		MYRS_CTLR_ACCELERAID170 = 0x1F,
		MYRS_CTLR_ACCELERAID160 = 0x20,
		MYRS_CTLR_DAC960S	= 0x60,
		MYRS_CTLR_DAC960SU	= 0x61,
		MYRS_CTLR_DAC960SX	= 0x62,
		MYRS_CTLR_DAC960SF	= 0x63,
		MYRS_CTLR_DAC960SS	= 0x64,
		MYRS_CTLR_DAC960FL	= 0x65,
		MYRS_CTLR_DAC960LL	= 0x66,
		MYRS_CTLR_DAC960FF	= 0x67,
		MYRS_CTLR_DAC960HP	= 0x68,
		MYRS_CTLR_RAIDBRICK	= 0x69,
		MYRS_CTLR_METEOR_FL	= 0x6A,
		MYRS_CTLR_METEOR_FF	= 0x6B
	} __packed ctlr_type;	
	unsigned char rsvd2;			
	unsigned short bus_speed_mhz;		
	unsigned char bus_width;		
	unsigned char flash_code;		
	unsigned char ports_present;		
	unsigned char rsvd3[7];			
	unsigned char bus_name[16];		
	unsigned char ctlr_name[16];		
	unsigned char rsvd4[16];		
	
	unsigned char fw_major_version;		
	unsigned char fw_minor_version;		
	unsigned char fw_turn_number;		
	unsigned char fw_build_number;		
	unsigned char fw_release_day;		
	unsigned char fw_release_month;		
	unsigned char fw_release_year_hi;	
	unsigned char fw_release_year_lo;	
	
	unsigned char hw_rev;			
	unsigned char rsvd5[3];			
	unsigned char hw_release_day;		
	unsigned char hw_release_month;		
	unsigned char hw_release_year_hi;	
	unsigned char hw_release_year_lo;	
	
	unsigned char manuf_batch_num;		
	unsigned char rsvd6;			
	unsigned char manuf_plant_num;		
	unsigned char rsvd7;			
	unsigned char hw_manuf_day;		
	unsigned char hw_manuf_month;		
	unsigned char hw_manuf_year_hi;		
	unsigned char hw_manuf_year_lo;		
	unsigned char max_pd_per_xld;		
	unsigned char max_ild_per_xld;		
	unsigned short nvram_size_kb;		
	unsigned char max_xld;			
	unsigned char rsvd8[3];			
	
	unsigned char serial_number[16];	
	unsigned char rsvd9[16];		
	
	unsigned char rsvd10[3];		
	unsigned char oem_code;			
	unsigned char vendor[16];		
	
	unsigned char bbu_present:1;		
	unsigned char cluster_mode:1;		
	unsigned char rsvd11:6;			
	unsigned char rsvd12[3];		
	
	unsigned char pscan_active:1;		
	unsigned char rsvd13:7;			
	unsigned char pscan_chan;		
	unsigned char pscan_target;		
	unsigned char pscan_lun;		
	
	unsigned short max_transfer_size;	
	unsigned short max_sge;			
	
	unsigned short ldev_present;		
	unsigned short ldev_critical;		
	unsigned short ldev_offline;		
	unsigned short pdev_present;		
	unsigned short pdisk_present;		
	unsigned short pdisk_critical;		
	unsigned short pdisk_offline;		
	unsigned short max_tcq;			
	
	unsigned char physchan_present;		
	unsigned char virtchan_present;		
	unsigned char physchan_max;		
	unsigned char virtchan_max;		
	unsigned char max_targets[16];		
	unsigned char rsvd14[12];		
	
	unsigned short mem_size_mb;		
	unsigned short cache_size_mb;		
	unsigned int valid_cache_bytes;		
	unsigned int dirty_cache_bytes;		
	unsigned short mem_speed_mhz;		
	unsigned char mem_data_width;		
	struct myrs_mem_type mem_type;		
	unsigned char cache_mem_type_name[16];	
	
	unsigned short exec_mem_size_mb;	
	unsigned short exec_l2_cache_size_mb;	
	unsigned char rsvd15[8];		
	unsigned short exec_mem_speed_mhz;	
	unsigned char exec_mem_data_width;	
	struct myrs_mem_type exec_mem_type;	
	unsigned char exec_mem_type_name[16];	
	
	struct {				
		unsigned short cpu_speed_mhz;
		enum myrs_cpu_type cpu_type;
		unsigned char cpu_count;
		unsigned char rsvd16[12];
		unsigned char cpu_name[16];
	} __packed cpu[2];
	
	unsigned short cur_prof_page_num;	
	unsigned short num_prof_waiters;	
	unsigned short cur_trace_page_num;	
	unsigned short num_trace_waiters;	
	unsigned char rsvd18[8];		
	
	unsigned short pdev_bus_resets;		
	unsigned short pdev_parity_errors;	
	unsigned short pdev_soft_errors;	
	unsigned short pdev_cmds_failed;	
	unsigned short pdev_misc_errors;	
	unsigned short pdev_cmd_timeouts;	
	unsigned short pdev_sel_timeouts;	
	unsigned short pdev_retries_done;	
	unsigned short pdev_aborts_done;	
	unsigned short pdev_host_aborts_done;	
	unsigned short pdev_predicted_failures;	
	unsigned short pdev_host_cmds_failed;	
	unsigned short pdev_hard_errors;	
	unsigned char rsvd19[6];		
	
	unsigned short ldev_soft_errors;	
	unsigned short ldev_cmds_failed;	
	unsigned short ldev_host_aborts_done;	
	unsigned char rsvd20[2];		
	
	unsigned short ctlr_mem_errors;		
	unsigned short ctlr_host_aborts_done;	
	unsigned char rsvd21[4];		
	
	unsigned short bg_init_active;		
	unsigned short ldev_init_active;	
	unsigned short pdev_init_active;	
	unsigned short cc_active;		
	unsigned short rbld_active;		
	unsigned short exp_active;		
	unsigned short patrol_active;		
	unsigned char rsvd22[2];		
	
	unsigned char flash_type;		
	unsigned char rsvd23;			
	unsigned short flash_size_MB;		
	unsigned int flash_limit;		
	unsigned int flash_count;		
	unsigned char rsvd24[4];		
	unsigned char flash_type_name[16];	
	
	unsigned char rbld_rate;		
	unsigned char bg_init_rate;		
	unsigned char fg_init_rate;		
	unsigned char cc_rate;			
	unsigned char rsvd25[4];		
	unsigned int max_dp;			
	unsigned int free_dp;			
	unsigned int max_iop;			
	unsigned int free_iop;			
	unsigned short max_combined_len;	
	unsigned short num_cfg_groups;		
	unsigned installation_abort_status:1;	
	unsigned maint_mode_status:1;		
	unsigned rsvd26:6;			
	unsigned char rsvd27[6];		
	unsigned char rsvd28[512];		
};


enum myrs_devstate {
	MYRS_DEVICE_UNCONFIGURED	= 0x00,
	MYRS_DEVICE_ONLINE		= 0x01,
	MYRS_DEVICE_REBUILD		= 0x03,
	MYRS_DEVICE_MISSING		= 0x04,
	MYRS_DEVICE_SUSPECTED_CRITICAL	= 0x05,
	MYRS_DEVICE_OFFLINE		= 0x08,
	MYRS_DEVICE_CRITICAL		= 0x09,
	MYRS_DEVICE_SUSPECTED_DEAD	= 0x0C,
	MYRS_DEVICE_COMMANDED_OFFLINE	= 0x10,
	MYRS_DEVICE_STANDBY		= 0x21,
	MYRS_DEVICE_INVALID_STATE	= 0xFF,
} __packed;


enum myrs_raid_level {
	MYRS_RAID_LEVEL0	= 0x0,     
	MYRS_RAID_LEVEL1	= 0x1,     
	MYRS_RAID_LEVEL3	= 0x3,     
	MYRS_RAID_LEVEL5	= 0x5,     
	MYRS_RAID_LEVEL6	= 0x6,     
	MYRS_RAID_JBOD		= 0x7,     
	MYRS_RAID_NEWSPAN	= 0x8,     
	MYRS_RAID_LEVEL3F	= 0x9,     
	MYRS_RAID_LEVEL3L	= 0xb,     
	MYRS_RAID_SPAN		= 0xc,     
	MYRS_RAID_LEVEL5L	= 0xd,     
	MYRS_RAID_LEVELE	= 0xe,     
	MYRS_RAID_PHYSICAL	= 0xf,     
} __packed;

enum myrs_stripe_size {
	MYRS_STRIPE_SIZE_0	= 0x0,	
	MYRS_STRIPE_SIZE_512B	= 0x1,
	MYRS_STRIPE_SIZE_1K	= 0x2,
	MYRS_STRIPE_SIZE_2K	= 0x3,
	MYRS_STRIPE_SIZE_4K	= 0x4,
	MYRS_STRIPE_SIZE_8K	= 0x5,
	MYRS_STRIPE_SIZE_16K	= 0x6,
	MYRS_STRIPE_SIZE_32K	= 0x7,
	MYRS_STRIPE_SIZE_64K	= 0x8,
	MYRS_STRIPE_SIZE_128K	= 0x9,
	MYRS_STRIPE_SIZE_256K	= 0xa,
	MYRS_STRIPE_SIZE_512K	= 0xb,
	MYRS_STRIPE_SIZE_1M	= 0xc,
} __packed;

enum myrs_cacheline_size {
	MYRS_CACHELINE_ZERO	= 0x0,	
	MYRS_CACHELINE_512B	= 0x1,
	MYRS_CACHELINE_1K	= 0x2,
	MYRS_CACHELINE_2K	= 0x3,
	MYRS_CACHELINE_4K	= 0x4,
	MYRS_CACHELINE_8K	= 0x5,
	MYRS_CACHELINE_16K	= 0x6,
	MYRS_CACHELINE_32K	= 0x7,
	MYRS_CACHELINE_64K	= 0x8,
} __packed;


struct myrs_ldev_info {
	unsigned char ctlr;			
	unsigned char channel;			
	unsigned char target;			
	unsigned char lun;			
	enum myrs_devstate dev_state;		
	unsigned char raid_level;		
	enum myrs_stripe_size stripe_size;	
	enum myrs_cacheline_size cacheline_size; 
	struct {
		enum {
			MYRS_READCACHE_DISABLED		= 0x0,
			MYRS_READCACHE_ENABLED		= 0x1,
			MYRS_READAHEAD_ENABLED		= 0x2,
			MYRS_INTELLIGENT_READAHEAD_ENABLED = 0x3,
			MYRS_READCACHE_LAST		= 0x7,
		} __packed rce:3; 
		enum {
			MYRS_WRITECACHE_DISABLED	= 0x0,
			MYRS_LOGICALDEVICE_RO		= 0x1,
			MYRS_WRITECACHE_ENABLED		= 0x2,
			MYRS_INTELLIGENT_WRITECACHE_ENABLED = 0x3,
			MYRS_WRITECACHE_LAST		= 0x7,
		} __packed wce:3; 
		unsigned rsvd1:1;		
		unsigned ldev_init_done:1;	
	} ldev_control;				
	
	unsigned char cc_active:1;		
	unsigned char rbld_active:1;		
	unsigned char bg_init_active:1;		
	unsigned char fg_init_active:1;		
	unsigned char migration_active:1;	
	unsigned char patrol_active:1;		
	unsigned char rsvd2:2;			
	unsigned char raid5_writeupdate;	
	unsigned char raid5_algo;		
	unsigned short ldev_num;		
	
	unsigned char bios_disabled:1;		
	unsigned char cdrom_boot:1;		
	unsigned char drv_coercion:1;		
	unsigned char write_same_disabled:1;	
	unsigned char hba_mode:1;		
	enum {
		MYRS_GEOMETRY_128_32	= 0x0,
		MYRS_GEOMETRY_255_63	= 0x1,
		MYRS_GEOMETRY_RSVD1	= 0x2,
		MYRS_GEOMETRY_RSVD2	= 0x3
	} __packed drv_geom:2;	
	unsigned char super_ra_enabled:1;	
	unsigned char rsvd3;			
	
	unsigned short soft_errs;		
	unsigned short cmds_failed;		
	unsigned short cmds_aborted;		
	unsigned short deferred_write_errs;	
	unsigned int rsvd4;			
	unsigned int rsvd5;			
	
	unsigned short rsvd6;			
	unsigned short devsize_bytes;		
	unsigned int orig_devsize;		
	unsigned int cfg_devsize;		
	unsigned int rsvd7;			
	unsigned char ldev_name[32];		
	unsigned char inquiry[36];		
	unsigned char rsvd8[12];		
	u64 last_read_lba;			
	u64 last_write_lba;			
	u64 cc_lba;				
	u64 rbld_lba;				
	u64 bg_init_lba;			
	u64 fg_init_lba;			
	u64 migration_lba;			
	u64 patrol_lba;				
	unsigned char rsvd9[64];		
};


struct myrs_pdev_info {
	unsigned char rsvd1;			
	unsigned char channel;			
	unsigned char target;			
	unsigned char lun;			
	
	unsigned char pdev_fault_tolerant:1;	
	unsigned char pdev_connected:1;		
	unsigned char pdev_local_to_ctlr:1;	
	unsigned char rsvd2:5;			
	
	unsigned char remote_host_dead:1;	
	unsigned char remove_ctlr_dead:1;	
	unsigned char rsvd3:6;			
	enum myrs_devstate dev_state;		
	unsigned char nego_data_width;		
	unsigned short nego_sync_rate;		
	
	unsigned char num_ports;		
	unsigned char drv_access_bitmap;	
	unsigned int rsvd4;			
	unsigned char ip_address[16];		
	unsigned short max_tags;		
	
	unsigned char cc_in_progress:1;		
	unsigned char rbld_in_progress:1;	
	unsigned char makecc_in_progress:1;	
	unsigned char pdevinit_in_progress:1;	
	unsigned char migration_in_progress:1;	
	unsigned char patrol_in_progress:1;	
	unsigned char rsvd5:2;			
	unsigned char long_op_status;		
	unsigned char parity_errs;		
	unsigned char soft_errs;		
	unsigned char hard_errs;		
	unsigned char misc_errs;		
	unsigned char cmd_timeouts;		
	unsigned char retries;			
	unsigned char aborts;			
	unsigned char pred_failures;		
	unsigned int rsvd6;			
	unsigned short rsvd7;			
	unsigned short devsize_bytes;		
	unsigned int orig_devsize;		
	unsigned int cfg_devsize;		
	unsigned int rsvd8;			
	unsigned char pdev_name[16];		
	unsigned char rsvd9[16];		
	unsigned char rsvd10[32];		
	unsigned char inquiry[36];		
	unsigned char rsvd11[20];		
	unsigned char rsvd12[8];		
	u64 last_read_lba;			
	u64 last_write_lba;			
	u64 cc_lba;				
	u64 rbld_lba;				
	u64 makecc_lba;				
	u64 devinit_lba;			
	u64 migration_lba;			
	u64 patrol_lba;				
	unsigned char rsvd13[256];		
};


struct myrs_fwstat {
	unsigned int uptime_usecs;		
	unsigned int uptime_msecs;		
	unsigned int seconds;			
	unsigned char rsvd1[4];			
	unsigned int epoch;			
	unsigned char rsvd2[4];			
	unsigned int dbg_msgbuf_idx;		
	unsigned int coded_msgbuf_idx;		
	unsigned int cur_timetrace_page;	
	unsigned int cur_prof_page;		
	unsigned int next_evseq;		
	unsigned char rsvd3[4];			
	unsigned char rsvd4[16];		
	unsigned char rsvd5[64];		
};


struct myrs_event {
	unsigned int ev_seq;			
	unsigned int ev_time;			
	unsigned int ev_code;			
	unsigned char rsvd1;			
	unsigned char channel;			
	unsigned char target;			
	unsigned char lun;			
	unsigned int rsvd2;			
	unsigned int ev_parm;			
	unsigned char sense_data[40];		
};


struct myrs_cmd_ctrl {
	unsigned char fua:1;			
	unsigned char disable_pgout:1;		
	unsigned char rsvd1:1;			
	unsigned char add_sge_mem:1;		
	unsigned char dma_ctrl_to_host:1;	
	unsigned char rsvd2:1;			
	unsigned char no_autosense:1;		
	unsigned char disc_prohibited:1;	
};


struct myrs_cmd_tmo {
	unsigned char tmo_val:6;			
	enum {
		MYRS_TMO_SCALE_SECONDS	= 0,
		MYRS_TMO_SCALE_MINUTES	= 1,
		MYRS_TMO_SCALE_HOURS	= 2,
		MYRS_TMO_SCALE_RESERVED = 3
	} __packed tmo_scale:2;		
};


struct myrs_pdev {
	unsigned char lun;			
	unsigned char target;			
	unsigned char channel:3;		
	unsigned char ctlr:5;			
} __packed;


struct myrs_ldev {
	unsigned short ldev_num;		
	unsigned char rsvd:3;			
	unsigned char ctlr:5;			
} __packed;


enum myrs_opdev {
	MYRS_PHYSICAL_DEVICE	= 0x00,
	MYRS_RAID_DEVICE	= 0x01,
	MYRS_PHYSICAL_CHANNEL	= 0x02,
	MYRS_RAID_CHANNEL	= 0x03,
	MYRS_PHYSICAL_CONTROLLER = 0x04,
	MYRS_RAID_CONTROLLER	= 0x05,
	MYRS_CONFIGURATION_GROUP = 0x10,
	MYRS_ENCLOSURE		= 0x11,
} __packed;


struct myrs_devmap {
	unsigned short ldev_num;		
	unsigned short rsvd;			
	unsigned char prev_boot_ctlr;		
	unsigned char prev_boot_channel;	
	unsigned char prev_boot_target;		
	unsigned char prev_boot_lun;		
};


struct myrs_sge {
	u64 sge_addr;			
	u64 sge_count;			
};


union myrs_sgl {
	struct myrs_sge sge[2]; 
	struct {
		unsigned short sge0_len;	
		unsigned short sge1_len;	
		unsigned short sge2_len;	
		unsigned short rsvd;		
		u64 sge0_addr;			
		u64 sge1_addr;			
		u64 sge2_addr;			
	} ext;
};


union myrs_cmd_mbox {
	unsigned int words[16];				
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size:24;			
		unsigned char dma_num;			
		u64 sense_addr;				
		unsigned int rsvd1:24;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		unsigned char rsvd2[10];		
		union myrs_sgl dma_addr;		
	} common;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size;				
		u64 sense_addr;				
		struct myrs_pdev pdev;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		unsigned char cdb_len;			
		unsigned char cdb[10];			
		union myrs_sgl dma_addr;		
	} SCSI_10;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size;				
		u64 sense_addr;				
		struct myrs_pdev pdev;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		unsigned char cdb_len;			
		unsigned short rsvd;			
		u64 cdb_addr;				
		union myrs_sgl dma_addr;		
	} SCSI_255;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size:24;			
		unsigned char dma_num;			
		u64 sense_addr;				
		unsigned short rsvd1;			
		unsigned char ctlr_num;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		unsigned char rsvd2[10];		
		union myrs_sgl dma_addr;		
	} ctlr_info;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size:24;			
		unsigned char dma_num;			
		u64 sense_addr;				
		struct myrs_ldev ldev;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		unsigned char rsvd[10];			
		union myrs_sgl dma_addr;		
	} ldev_info;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size:24;			
		unsigned char dma_num;			
		u64 sense_addr;				
		struct myrs_pdev pdev;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		unsigned char rsvd[10];			
		union myrs_sgl dma_addr;		
	} pdev_info;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size:24;			
		unsigned char dma_num;			
		u64 sense_addr;				
		unsigned short evnum_upper;		
		unsigned char ctlr_num;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		unsigned short evnum_lower;		
		unsigned char rsvd[8];			
		union myrs_sgl dma_addr;		
	} get_event;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size:24;			
		unsigned char dma_num;			
		u64 sense_addr;				
		union {
			struct myrs_ldev ldev;		
			struct myrs_pdev pdev;		
		};
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		enum myrs_devstate state;		
		unsigned char rsvd[9];			
		union myrs_sgl dma_addr;		
	} set_devstate;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size:24;			
		unsigned char dma_num;			
		u64 sense_addr;				
		struct myrs_ldev ldev;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		unsigned char restore_consistency:1;	
		unsigned char initialized_area_only:1;	
		unsigned char rsvd1:6;			
		unsigned char rsvd2[9];			
		union myrs_sgl dma_addr;		
	} cc;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		unsigned char first_cmd_mbox_size_kb;	
		unsigned char first_stat_mbox_size_kb;	
		unsigned char second_cmd_mbox_size_kb;	
		unsigned char second_stat_mbox_size_kb;	
		u64 sense_addr;				
		unsigned int rsvd1:24;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		unsigned char fwstat_buf_size_kb;	
		unsigned char rsvd2;			
		u64 fwstat_buf_addr;			
		u64 first_cmd_mbox_addr;		
		u64 first_stat_mbox_addr;		
		u64 second_cmd_mbox_addr;		
		u64 second_stat_mbox_addr;		
	} set_mbox;
	struct {
		unsigned short id;			
		enum myrs_cmd_opcode opcode;		
		struct myrs_cmd_ctrl control;		
		u32 dma_size:24;			
		unsigned char dma_num;			
		u64 sense_addr;				
		struct myrs_pdev pdev;			
		struct myrs_cmd_tmo tmo;		
		unsigned char sense_len;		
		enum myrs_ioctl_opcode ioctl_opcode;	
		enum myrs_opdev opdev;			
		unsigned char rsvd[9];			
		union myrs_sgl dma_addr;		
	} dev_op;
};


struct myrs_stat_mbox {
	unsigned short id;		
	unsigned char status;		
	unsigned char sense_len;	
	int residual;			
};

struct myrs_cmdblk {
	union myrs_cmd_mbox mbox;
	unsigned char status;
	unsigned char sense_len;
	int residual;
	struct completion *complete;
	struct myrs_sge *sgl;
	dma_addr_t sgl_addr;
	unsigned char *dcdb;
	dma_addr_t dcdb_dma;
	unsigned char *sense;
	dma_addr_t sense_addr;
};


struct myrs_hba {
	void __iomem *io_base;
	void __iomem *mmio_base;
	phys_addr_t io_addr;
	phys_addr_t pci_addr;
	unsigned int irq;

	unsigned char model_name[28];
	unsigned char fw_version[12];

	struct Scsi_Host *host;
	struct pci_dev *pdev;

	unsigned int epoch;
	unsigned int next_evseq;
	
	bool needs_update;
	bool disable_enc_msg;

	struct workqueue_struct *work_q;
	char work_q_name[20];
	struct delayed_work monitor_work;
	unsigned long primary_monitor_time;
	unsigned long secondary_monitor_time;

	spinlock_t queue_lock;

	struct dma_pool *sg_pool;
	struct dma_pool *sense_pool;
	struct dma_pool *dcdb_pool;

	void (*write_cmd_mbox)(union myrs_cmd_mbox *next_mbox,
			       union myrs_cmd_mbox *cmd_mbox);
	void (*get_cmd_mbox)(void __iomem *base);
	void (*disable_intr)(void __iomem *base);
	void (*reset)(void __iomem *base);

	dma_addr_t cmd_mbox_addr;
	size_t cmd_mbox_size;
	union myrs_cmd_mbox *first_cmd_mbox;
	union myrs_cmd_mbox *last_cmd_mbox;
	union myrs_cmd_mbox *next_cmd_mbox;
	union myrs_cmd_mbox *prev_cmd_mbox1;
	union myrs_cmd_mbox *prev_cmd_mbox2;

	dma_addr_t stat_mbox_addr;
	size_t stat_mbox_size;
	struct myrs_stat_mbox *first_stat_mbox;
	struct myrs_stat_mbox *last_stat_mbox;
	struct myrs_stat_mbox *next_stat_mbox;

	struct myrs_cmdblk dcmd_blk;
	struct myrs_cmdblk mcmd_blk;
	struct mutex dcmd_mutex;

	struct myrs_fwstat *fwstat_buf;
	dma_addr_t fwstat_addr;

	struct myrs_ctlr_info *ctlr_info;
	struct mutex cinfo_mutex;

	struct myrs_event *event_buf;
};

typedef unsigned char (*enable_mbox_t)(void __iomem *base, dma_addr_t addr);
typedef int (*myrs_hwinit_t)(struct pci_dev *pdev,
			     struct myrs_hba *c, void __iomem *base);

struct myrs_privdata {
	myrs_hwinit_t		hw_init;
	irq_handler_t		irq_handler;
	unsigned int		mmio_size;
};



#define DAC960_GEM_mmio_size	0x600

enum DAC960_GEM_reg_offset {
	DAC960_GEM_IDB_READ_OFFSET	= 0x214,
	DAC960_GEM_IDB_CLEAR_OFFSET	= 0x218,
	DAC960_GEM_ODB_READ_OFFSET	= 0x224,
	DAC960_GEM_ODB_CLEAR_OFFSET	= 0x228,
	DAC960_GEM_IRQSTS_OFFSET	= 0x208,
	DAC960_GEM_IRQMASK_READ_OFFSET	= 0x22C,
	DAC960_GEM_IRQMASK_CLEAR_OFFSET	= 0x230,
	DAC960_GEM_CMDMBX_OFFSET	= 0x510,
	DAC960_GEM_CMDSTS_OFFSET	= 0x518,
	DAC960_GEM_ERRSTS_READ_OFFSET	= 0x224,
	DAC960_GEM_ERRSTS_CLEAR_OFFSET	= 0x228,
};


#define DAC960_GEM_IDB_HWMBOX_NEW_CMD	0x01
#define DAC960_GEM_IDB_HWMBOX_ACK_STS	0x02
#define DAC960_GEM_IDB_GEN_IRQ		0x04
#define DAC960_GEM_IDB_CTRL_RESET	0x08
#define DAC960_GEM_IDB_MMBOX_NEW_CMD	0x10

#define DAC960_GEM_IDB_HWMBOX_FULL	0x01
#define DAC960_GEM_IDB_INIT_IN_PROGRESS	0x02


#define DAC960_GEM_ODB_HWMBOX_ACK_IRQ	0x01
#define DAC960_GEM_ODB_MMBOX_ACK_IRQ	0x02
#define DAC960_GEM_ODB_HWMBOX_STS_AVAIL 0x01
#define DAC960_GEM_ODB_MMBOX_STS_AVAIL	0x02


#define DAC960_GEM_IRQMASK_HWMBOX_IRQ	0x01
#define DAC960_GEM_IRQMASK_MMBOX_IRQ	0x02


#define DAC960_GEM_ERRSTS_PENDING	0x20


static inline
void dma_addr_writeql(dma_addr_t addr, void __iomem *write_address)
{
	union {
		u64 wq;
		uint wl[2];
	} u;

	u.wq = addr;

	writel(u.wl[0], write_address);
	writel(u.wl[1], write_address + 4);
}



#define DAC960_BA_mmio_size		0x80

enum DAC960_BA_reg_offset {
	DAC960_BA_IRQSTS_OFFSET	= 0x30,
	DAC960_BA_IRQMASK_OFFSET = 0x34,
	DAC960_BA_CMDMBX_OFFSET = 0x50,
	DAC960_BA_CMDSTS_OFFSET = 0x58,
	DAC960_BA_IDB_OFFSET	= 0x60,
	DAC960_BA_ODB_OFFSET	= 0x61,
	DAC960_BA_ERRSTS_OFFSET = 0x63,
};


#define DAC960_BA_IDB_HWMBOX_NEW_CMD	0x01
#define DAC960_BA_IDB_HWMBOX_ACK_STS	0x02
#define DAC960_BA_IDB_GEN_IRQ		0x04
#define DAC960_BA_IDB_CTRL_RESET	0x08
#define DAC960_BA_IDB_MMBOX_NEW_CMD	0x10

#define DAC960_BA_IDB_HWMBOX_EMPTY	0x01
#define DAC960_BA_IDB_INIT_DONE		0x02


#define DAC960_BA_ODB_HWMBOX_ACK_IRQ	0x01
#define DAC960_BA_ODB_MMBOX_ACK_IRQ	0x02

#define DAC960_BA_ODB_HWMBOX_STS_AVAIL	0x01
#define DAC960_BA_ODB_MMBOX_STS_AVAIL	0x02


#define DAC960_BA_IRQMASK_DISABLE_IRQ	0x04
#define DAC960_BA_IRQMASK_DISABLEW_I2O	0x08


#define DAC960_BA_ERRSTS_PENDING	0x04



#define DAC960_LP_mmio_size		0x80

enum DAC960_LP_reg_offset {
	DAC960_LP_CMDMBX_OFFSET = 0x10,
	DAC960_LP_CMDSTS_OFFSET = 0x18,
	DAC960_LP_IDB_OFFSET	= 0x20,
	DAC960_LP_ODB_OFFSET	= 0x2C,
	DAC960_LP_ERRSTS_OFFSET = 0x2E,
	DAC960_LP_IRQSTS_OFFSET	= 0x30,
	DAC960_LP_IRQMASK_OFFSET = 0x34,
};


#define DAC960_LP_IDB_HWMBOX_NEW_CMD	0x01
#define DAC960_LP_IDB_HWMBOX_ACK_STS	0x02
#define DAC960_LP_IDB_GEN_IRQ		0x04
#define DAC960_LP_IDB_CTRL_RESET	0x08
#define DAC960_LP_IDB_MMBOX_NEW_CMD	0x10

#define DAC960_LP_IDB_HWMBOX_FULL	0x01
#define DAC960_LP_IDB_INIT_IN_PROGRESS	0x02


#define DAC960_LP_ODB_HWMBOX_ACK_IRQ	0x01
#define DAC960_LP_ODB_MMBOX_ACK_IRQ	0x02

#define DAC960_LP_ODB_HWMBOX_STS_AVAIL	0x01
#define DAC960_LP_ODB_MMBOX_STS_AVAIL	0x02


#define DAC960_LP_IRQMASK_DISABLE_IRQ	0x04


#define DAC960_LP_ERRSTS_PENDING	0x04

#endif 
