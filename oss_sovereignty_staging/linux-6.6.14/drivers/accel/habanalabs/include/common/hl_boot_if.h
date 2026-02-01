 

#ifndef HL_BOOT_IF_H
#define HL_BOOT_IF_H

#define LKD_HARD_RESET_MAGIC		0xED7BD694  
#define HL_POWER9_HOST_MAGIC		0x1DA30009

#define BOOT_FIT_SRAM_OFFSET		0x200000

#define VERSION_MAX_LEN			128

enum cpu_boot_err {
	CPU_BOOT_ERR_DRAM_INIT_FAIL = 0,
	CPU_BOOT_ERR_FIT_CORRUPTED = 1,
	CPU_BOOT_ERR_TS_INIT_FAIL = 2,
	CPU_BOOT_ERR_DRAM_SKIPPED = 3,
	CPU_BOOT_ERR_BMC_WAIT_SKIPPED = 4,
	CPU_BOOT_ERR_NIC_DATA_NOT_RDY = 5,
	CPU_BOOT_ERR_NIC_FW_FAIL = 6,
	CPU_BOOT_ERR_SECURITY_NOT_RDY = 7,
	CPU_BOOT_ERR_SECURITY_FAIL = 8,
	CPU_BOOT_ERR_EFUSE_FAIL = 9,
	CPU_BOOT_ERR_PRI_IMG_VER_FAIL = 10,
	CPU_BOOT_ERR_SEC_IMG_VER_FAIL = 11,
	CPU_BOOT_ERR_PLL_FAIL = 12,
	CPU_BOOT_ERR_DEVICE_UNUSABLE_FAIL = 13,
	CPU_BOOT_ERR_BOOT_FW_CRIT_ERR = 18,
	CPU_BOOT_ERR_BINNING_FAIL = 19,
	CPU_BOOT_ERR_TPM_FAIL = 20,
	CPU_BOOT_ERR_TMP_THRESH_INIT_FAIL = 21,
	CPU_BOOT_ERR_EEPROM_FAIL = 22,
	CPU_BOOT_ERR_ENG_ARC_MEM_SCRUB_FAIL = 23,
	CPU_BOOT_ERR_ENABLED = 31,
	CPU_BOOT_ERR_SCND_EN = 63,
	CPU_BOOT_ERR_LAST = 64  
};

 
#define CPU_BOOT_ERR_FATAL_MASK					\
		((1 << CPU_BOOT_ERR_DRAM_INIT_FAIL) |		\
		 (1 << CPU_BOOT_ERR_PLL_FAIL) |			\
		 (1 << CPU_BOOT_ERR_DEVICE_UNUSABLE_FAIL) |	\
		 (1 << CPU_BOOT_ERR_BINNING_FAIL) |		\
		 (1 << CPU_BOOT_ERR_DRAM_SKIPPED) |		\
		 (1 << CPU_BOOT_ERR_ENG_ARC_MEM_SCRUB_FAIL) |	\
		 (1 << CPU_BOOT_ERR_EEPROM_FAIL))

 
#define CPU_BOOT_ERR0_DRAM_INIT_FAIL		(1 << CPU_BOOT_ERR_DRAM_INIT_FAIL)
#define CPU_BOOT_ERR0_FIT_CORRUPTED		(1 << CPU_BOOT_ERR_FIT_CORRUPTED)
#define CPU_BOOT_ERR0_TS_INIT_FAIL		(1 << CPU_BOOT_ERR_TS_INIT_FAIL)
#define CPU_BOOT_ERR0_DRAM_SKIPPED		(1 << CPU_BOOT_ERR_DRAM_SKIPPED)
#define CPU_BOOT_ERR0_BMC_WAIT_SKIPPED		(1 << CPU_BOOT_ERR_BMC_WAIT_SKIPPED)
#define CPU_BOOT_ERR0_NIC_DATA_NOT_RDY		(1 << CPU_BOOT_ERR_NIC_DATA_NOT_RDY)
#define CPU_BOOT_ERR0_NIC_FW_FAIL		(1 << CPU_BOOT_ERR_NIC_FW_FAIL)
#define CPU_BOOT_ERR0_SECURITY_NOT_RDY		(1 << CPU_BOOT_ERR_SECURITY_NOT_RDY)
#define CPU_BOOT_ERR0_SECURITY_FAIL		(1 << CPU_BOOT_ERR_SECURITY_FAIL)
#define CPU_BOOT_ERR0_EFUSE_FAIL		(1 << CPU_BOOT_ERR_EFUSE_FAIL)
#define CPU_BOOT_ERR0_PRI_IMG_VER_FAIL		(1 << CPU_BOOT_ERR_PRI_IMG_VER_FAIL)
#define CPU_BOOT_ERR0_SEC_IMG_VER_FAIL		(1 << CPU_BOOT_ERR_SEC_IMG_VER_FAIL)
#define CPU_BOOT_ERR0_PLL_FAIL			(1 << CPU_BOOT_ERR_PLL_FAIL)
#define CPU_BOOT_ERR0_DEVICE_UNUSABLE_FAIL	(1 << CPU_BOOT_ERR_DEVICE_UNUSABLE_FAIL)
#define CPU_BOOT_ERR0_BOOT_FW_CRIT_ERR		(1 << CPU_BOOT_ERR_BOOT_FW_CRIT_ERR)
#define CPU_BOOT_ERR0_BINNING_FAIL		(1 << CPU_BOOT_ERR_BINNING_FAIL)
#define CPU_BOOT_ERR0_TPM_FAIL			(1 << CPU_BOOT_ERR_TPM_FAIL)
#define CPU_BOOT_ERR0_TMP_THRESH_INIT_FAIL	(1 << CPU_BOOT_ERR_TMP_THRESH_INIT_FAIL)
#define CPU_BOOT_ERR0_EEPROM_FAIL		(1 << CPU_BOOT_ERR_EEPROM_FAIL)
#define CPU_BOOT_ERR0_ENG_ARC_MEM_SCRUB_FAIL	(1 << CPU_BOOT_ERR_ENG_ARC_MEM_SCRUB_FAIL)
#define CPU_BOOT_ERR0_ENABLED			(1 << CPU_BOOT_ERR_ENABLED)
#define CPU_BOOT_ERR1_ENABLED			(1 << CPU_BOOT_ERR_ENABLED)

enum cpu_boot_dev_sts {
	CPU_BOOT_DEV_STS_SECURITY_EN = 0,
	CPU_BOOT_DEV_STS_DEBUG_EN = 1,
	CPU_BOOT_DEV_STS_WATCHDOG_EN = 2,
	CPU_BOOT_DEV_STS_DRAM_INIT_EN = 3,
	CPU_BOOT_DEV_STS_BMC_WAIT_EN = 4,
	CPU_BOOT_DEV_STS_E2E_CRED_EN = 5,
	CPU_BOOT_DEV_STS_HBM_CRED_EN = 6,
	CPU_BOOT_DEV_STS_RL_EN = 7,
	CPU_BOOT_DEV_STS_SRAM_SCR_EN = 8,
	CPU_BOOT_DEV_STS_DRAM_SCR_EN = 9,
	CPU_BOOT_DEV_STS_FW_HARD_RST_EN = 10,
	CPU_BOOT_DEV_STS_PLL_INFO_EN = 11,
	CPU_BOOT_DEV_STS_SP_SRAM_EN = 12,
	CPU_BOOT_DEV_STS_CLK_GATE_EN = 13,
	CPU_BOOT_DEV_STS_HBM_ECC_EN = 14,
	CPU_BOOT_DEV_STS_PKT_PI_ACK_EN = 15,
	CPU_BOOT_DEV_STS_FW_LD_COM_EN = 16,
	CPU_BOOT_DEV_STS_FW_IATU_CONF_EN = 17,
	CPU_BOOT_DEV_STS_FW_NIC_MAC_EN = 18,
	CPU_BOOT_DEV_STS_DYN_PLL_EN = 19,
	CPU_BOOT_DEV_STS_GIC_PRIVILEGED_EN = 20,
	CPU_BOOT_DEV_STS_EQ_INDEX_EN = 21,
	CPU_BOOT_DEV_STS_MULTI_IRQ_POLL_EN = 22,
	CPU_BOOT_DEV_STS_FW_NIC_STAT_XPCS91_EN = 23,
	CPU_BOOT_DEV_STS_FW_NIC_STAT_EXT_EN = 24,
	CPU_BOOT_DEV_STS_IS_IDLE_CHECK_EN = 25,
	CPU_BOOT_DEV_STS_MAP_HWMON_EN = 26,
	CPU_BOOT_DEV_STS_ENABLED = 31,
	CPU_BOOT_DEV_STS_SCND_EN = 63,
	CPU_BOOT_DEV_STS_LAST = 64  
};

 
#define CPU_BOOT_DEV_STS0_SECURITY_EN		(1 << CPU_BOOT_DEV_STS_SECURITY_EN)
#define CPU_BOOT_DEV_STS0_DEBUG_EN		(1 << CPU_BOOT_DEV_STS_DEBUG_EN)
#define CPU_BOOT_DEV_STS0_WATCHDOG_EN		(1 << CPU_BOOT_DEV_STS_WATCHDOG_EN)
#define CPU_BOOT_DEV_STS0_DRAM_INIT_EN		(1 << CPU_BOOT_DEV_STS_DRAM_INIT_EN)
#define CPU_BOOT_DEV_STS0_BMC_WAIT_EN		(1 << CPU_BOOT_DEV_STS_BMC_WAIT_EN)
#define CPU_BOOT_DEV_STS0_E2E_CRED_EN		(1 << CPU_BOOT_DEV_STS_E2E_CRED_EN)
#define CPU_BOOT_DEV_STS0_HBM_CRED_EN		(1 << CPU_BOOT_DEV_STS_HBM_CRED_EN)
#define CPU_BOOT_DEV_STS0_RL_EN			(1 << CPU_BOOT_DEV_STS_RL_EN)
#define CPU_BOOT_DEV_STS0_SRAM_SCR_EN		(1 << CPU_BOOT_DEV_STS_SRAM_SCR_EN)
#define CPU_BOOT_DEV_STS0_DRAM_SCR_EN		(1 << CPU_BOOT_DEV_STS_DRAM_SCR_EN)
#define CPU_BOOT_DEV_STS0_FW_HARD_RST_EN	(1 << CPU_BOOT_DEV_STS_FW_HARD_RST_EN)
#define CPU_BOOT_DEV_STS0_PLL_INFO_EN		(1 << CPU_BOOT_DEV_STS_PLL_INFO_EN)
#define CPU_BOOT_DEV_STS0_SP_SRAM_EN		(1 << CPU_BOOT_DEV_STS_SP_SRAM_EN)
#define CPU_BOOT_DEV_STS0_CLK_GATE_EN		(1 << CPU_BOOT_DEV_STS_CLK_GATE_EN)
#define CPU_BOOT_DEV_STS0_HBM_ECC_EN		(1 << CPU_BOOT_DEV_STS_HBM_ECC_EN)
#define CPU_BOOT_DEV_STS0_PKT_PI_ACK_EN		(1 << CPU_BOOT_DEV_STS_PKT_PI_ACK_EN)
#define CPU_BOOT_DEV_STS0_FW_LD_COM_EN		(1 << CPU_BOOT_DEV_STS_FW_LD_COM_EN)
#define CPU_BOOT_DEV_STS0_FW_IATU_CONF_EN	(1 << CPU_BOOT_DEV_STS_FW_IATU_CONF_EN)
#define CPU_BOOT_DEV_STS0_FW_NIC_MAC_EN		(1 << CPU_BOOT_DEV_STS_FW_NIC_MAC_EN)
#define CPU_BOOT_DEV_STS0_DYN_PLL_EN		(1 << CPU_BOOT_DEV_STS_DYN_PLL_EN)
#define CPU_BOOT_DEV_STS0_GIC_PRIVILEGED_EN	(1 << CPU_BOOT_DEV_STS_GIC_PRIVILEGED_EN)
#define CPU_BOOT_DEV_STS0_EQ_INDEX_EN		(1 << CPU_BOOT_DEV_STS_EQ_INDEX_EN)
#define CPU_BOOT_DEV_STS0_MULTI_IRQ_POLL_EN	(1 << CPU_BOOT_DEV_STS_MULTI_IRQ_POLL_EN)
#define CPU_BOOT_DEV_STS0_FW_NIC_STAT_XPCS91_EN	(1 << CPU_BOOT_DEV_STS_FW_NIC_STAT_XPCS91_EN)
#define CPU_BOOT_DEV_STS0_FW_NIC_STAT_EXT_EN	(1 << CPU_BOOT_DEV_STS_FW_NIC_STAT_EXT_EN)
#define CPU_BOOT_DEV_STS0_IS_IDLE_CHECK_EN	(1 << CPU_BOOT_DEV_STS_IS_IDLE_CHECK_EN)
#define CPU_BOOT_DEV_STS0_MAP_HWMON_EN		(1 << CPU_BOOT_DEV_STS_MAP_HWMON_EN)
#define CPU_BOOT_DEV_STS0_ENABLED		(1 << CPU_BOOT_DEV_STS_ENABLED)
#define CPU_BOOT_DEV_STS1_ENABLED		(1 << CPU_BOOT_DEV_STS_ENABLED)

enum cpu_boot_status {
	CPU_BOOT_STATUS_NA = 0,		 
	CPU_BOOT_STATUS_IN_WFE = 1,
	CPU_BOOT_STATUS_DRAM_RDY = 2,
	CPU_BOOT_STATUS_SRAM_AVAIL = 3,
	CPU_BOOT_STATUS_IN_BTL = 4,	 
	CPU_BOOT_STATUS_IN_PREBOOT = 5,
	CPU_BOOT_STATUS_IN_SPL,		 
	CPU_BOOT_STATUS_IN_UBOOT = 7,
	CPU_BOOT_STATUS_DRAM_INIT_FAIL,	 
	CPU_BOOT_STATUS_FIT_CORRUPTED,	 
	 
	CPU_BOOT_STATUS_UBOOT_NOT_READY = 10,
	 
	CPU_BOOT_STATUS_NIC_FW_RDY = 11,
	CPU_BOOT_STATUS_TS_INIT_FAIL,	 
	CPU_BOOT_STATUS_DRAM_SKIPPED,	 
	CPU_BOOT_STATUS_BMC_WAITING_SKIPPED,  
	 
	CPU_BOOT_STATUS_READY_TO_BOOT = 15,
	 
	CPU_BOOT_STATUS_WAITING_FOR_BOOT_FIT = 16,
	 
	CPU_BOOT_STATUS_SECURITY_READY = 17,
};

enum kmd_msg {
	KMD_MSG_NA = 0,
	KMD_MSG_GOTO_WFE,
	KMD_MSG_FIT_RDY,
	KMD_MSG_SKIP_BMC,
	RESERVED,
	KMD_MSG_RST_DEV,
	KMD_MSG_LAST
};

enum cpu_msg_status {
	CPU_MSG_CLR = 0,
	CPU_MSG_OK,
	CPU_MSG_ERR,
};

 
struct cpu_dyn_regs {
	__le32 cpu_pq_base_addr_low;
	__le32 cpu_pq_base_addr_high;
	__le32 cpu_pq_length;
	__le32 cpu_pq_init_status;
	__le32 cpu_eq_base_addr_low;
	__le32 cpu_eq_base_addr_high;
	__le32 cpu_eq_length;
	__le32 cpu_eq_ci;
	__le32 cpu_cq_base_addr_low;
	__le32 cpu_cq_base_addr_high;
	__le32 cpu_cq_length;
	__le32 cpu_pf_pq_pi;
	__le32 cpu_boot_dev_sts0;
	__le32 cpu_boot_dev_sts1;
	__le32 cpu_boot_err0;
	__le32 cpu_boot_err1;
	__le32 cpu_boot_status;
	__le32 fw_upd_sts;
	__le32 fw_upd_cmd;
	__le32 fw_upd_pending_sts;
	__le32 fuse_ver_offset;
	__le32 preboot_ver_offset;
	__le32 uboot_ver_offset;
	__le32 hw_state;
	__le32 kmd_msg_to_cpu;
	__le32 cpu_cmd_status_to_host;
	__le32 gic_host_pi_upd_irq;
	__le32 gic_tpc_qm_irq_ctrl;
	__le32 gic_mme_qm_irq_ctrl;
	__le32 gic_dma_qm_irq_ctrl;
	__le32 gic_nic_qm_irq_ctrl;
	__le32 gic_dma_core_irq_ctrl;
	__le32 gic_host_halt_irq;
	__le32 gic_host_ints_irq;
	__le32 gic_host_soft_rst_irq;
	__le32 gic_rot_qm_irq_ctrl;
	__le32 cpu_rst_status;
	__le32 eng_arc_irq_ctrl;
	__le32 reserved1[20];		 
};

 
 
#define HL_COMMS_DESC_MAGIC	0x4843444D
#define HL_COMMS_DESC_VER	3

 
#define HL_COMMS_MSG_MAGIC_VALUE	0x48434D00
#define HL_COMMS_MSG_MAGIC_MASK		0xFFFFFF00
#define HL_COMMS_MSG_MAGIC_VER_MASK	0xFF

#define HL_COMMS_MSG_MAGIC_VER(ver)	(HL_COMMS_MSG_MAGIC_VALUE |	\
					((ver) & HL_COMMS_MSG_MAGIC_VER_MASK))
#define HL_COMMS_MSG_MAGIC_V0		HL_COMMS_DESC_MAGIC
#define HL_COMMS_MSG_MAGIC_V1		HL_COMMS_MSG_MAGIC_VER(1)
#define HL_COMMS_MSG_MAGIC_V2		HL_COMMS_MSG_MAGIC_VER(2)
#define HL_COMMS_MSG_MAGIC_V3		HL_COMMS_MSG_MAGIC_VER(3)

#define HL_COMMS_MSG_MAGIC		HL_COMMS_MSG_MAGIC_V3

#define HL_COMMS_MSG_MAGIC_VALIDATE_MAGIC(magic)			\
		(((magic) & HL_COMMS_MSG_MAGIC_MASK) ==			\
		HL_COMMS_MSG_MAGIC_VALUE)

#define HL_COMMS_MSG_MAGIC_VALIDATE_VERSION(magic, ver)			\
		(((magic) & HL_COMMS_MSG_MAGIC_VER_MASK) >=		\
		((ver) & HL_COMMS_MSG_MAGIC_VER_MASK))

#define HL_COMMS_MSG_MAGIC_VALIDATE(magic, ver)				\
		(HL_COMMS_MSG_MAGIC_VALIDATE_MAGIC((magic)) &&		\
		HL_COMMS_MSG_MAGIC_VALIDATE_VERSION((magic), (ver)))

enum comms_msg_type {
	HL_COMMS_DESC_TYPE = 0,
	HL_COMMS_RESET_CAUSE_TYPE = 1,
	HL_COMMS_FW_CFG_SKIP_TYPE = 2,
	HL_COMMS_BINNING_CONF_TYPE = 3,
};

 
struct lkd_fw_binning_info {
	__le64 tpc_mask_l;
	__le32 dec_mask;
	__le32 dram_mask;
	__le32 edma_mask;
	__le32 mme_mask_l;
	__le32 mme_mask_h;
	__le32 rot_mask;
	__le32 xbar_mask;
	__le32 reserved0;
	__le64 tpc_mask_h;
	__le64 nic_mask;
	__le32 reserved1[8];
};

 
 
struct comms_desc_header {
	__le32 magic;		 
	__le32 crc32;		 
	__le16 size;		 
	__u8 version;	 
	__u8 reserved[5];	 
};

 
struct comms_msg_header {
	__le32 magic;		 
	__le32 crc32;		 
	__le16 size;		 
	__u8 version;	 
	__u8 type;		 
	__u8 reserved[4];	 
};

enum lkd_fw_ascii_msg_lvls {
	LKD_FW_ASCII_MSG_ERR = 0,
	LKD_FW_ASCII_MSG_WRN = 1,
	LKD_FW_ASCII_MSG_INF = 2,
	LKD_FW_ASCII_MSG_DBG = 3,
};

#define LKD_FW_ASCII_MSG_MAX_LEN	128
#define LKD_FW_ASCII_MSG_MAX		4	 

struct lkd_fw_ascii_msg {
	__u8 valid;
	__u8 msg_lvl;
	__u8 reserved[6];
	char msg[LKD_FW_ASCII_MSG_MAX_LEN];
};

 
struct lkd_fw_comms_desc {
	struct comms_desc_header header;
	struct cpu_dyn_regs cpu_dyn_regs;
	char fuse_ver[VERSION_MAX_LEN];
	char cur_fw_ver[VERSION_MAX_LEN];
	 
	char reserved0[VERSION_MAX_LEN];
	__le64 img_addr;	 
	struct lkd_fw_binning_info binning_info;
	struct lkd_fw_ascii_msg ascii_msg[LKD_FW_ASCII_MSG_MAX];
};

enum comms_reset_cause {
	HL_RESET_CAUSE_UNKNOWN = 0,
	HL_RESET_CAUSE_HEARTBEAT = 1,
	HL_RESET_CAUSE_TDR = 2,
};

 
#define lkd_msg_comms lkd_fw_comms_msg

 
struct lkd_fw_comms_msg {
	struct comms_msg_header header;
	 
	union {
		struct {
			struct cpu_dyn_regs cpu_dyn_regs;
			char fuse_ver[VERSION_MAX_LEN];
			char cur_fw_ver[VERSION_MAX_LEN];
			 
			char reserved0[VERSION_MAX_LEN];
			 
			__le64 img_addr;
			struct lkd_fw_binning_info binning_info;
			struct lkd_fw_ascii_msg ascii_msg[LKD_FW_ASCII_MSG_MAX];
		};
		struct {
			__u8 reset_cause;
		};
		struct {
			__u8 fw_cfg_skip;  
		};
		struct lkd_fw_binning_info binning_conf;
	};
};

 
enum comms_cmd {
	COMMS_NOOP = 0,
	COMMS_CLR_STS = 1,
	COMMS_RST_STATE = 2,
	COMMS_PREP_DESC = 3,
	COMMS_DATA_RDY = 4,
	COMMS_EXEC = 5,
	COMMS_RST_DEV = 6,
	COMMS_GOTO_WFE = 7,
	COMMS_SKIP_BMC = 8,
	COMMS_PREP_DESC_ELBI = 10,
	COMMS_INVLD_LAST
};

#define COMMS_COMMAND_SIZE_SHIFT	0
#define COMMS_COMMAND_SIZE_MASK		0x1FFFFFF
#define COMMS_COMMAND_CMD_SHIFT		27
#define COMMS_COMMAND_CMD_MASK		0xF8000000

 
struct comms_command {
	union {		 
		struct {
			u32 size :25;		 
			u32 reserved :2;
			enum comms_cmd cmd :5;		 
		};
		__le32 val;
	};
};

 
enum comms_sts {
	COMMS_STS_NOOP = 0,
	COMMS_STS_ACK = 1,
	COMMS_STS_OK = 2,
	COMMS_STS_ERR = 3,
	COMMS_STS_VALID_ERR = 4,
	COMMS_STS_TIMEOUT_ERR = 5,
	COMMS_STS_INVLD_LAST
};

 
enum comms_ram_types {
	COMMS_SRAM = 0,
	COMMS_DRAM = 1,
};

#define COMMS_STATUS_OFFSET_SHIFT	0
#define COMMS_STATUS_OFFSET_MASK	0x03FFFFFF
#define COMMS_STATUS_OFFSET_ALIGN_SHIFT	2
#define COMMS_STATUS_RAM_TYPE_SHIFT	26
#define COMMS_STATUS_RAM_TYPE_MASK	0x0C000000
#define COMMS_STATUS_STATUS_SHIFT	28
#define COMMS_STATUS_STATUS_MASK	0xF0000000

 
struct comms_status {
	union {		 
		struct {
			u32 offset :26;
			enum comms_ram_types ram_type :2;
			enum comms_sts status :4;	 
		};
		__le32 val;
	};
};

#define NAME_MAX_LEN	32  
struct hl_module_data {
	__u8 name[NAME_MAX_LEN];
	__u8 version[VERSION_MAX_LEN];
};

 
struct hl_component_versions {
	__le16 struct_size;
	__le16 modules_offset;
	__u8 component[VERSION_MAX_LEN];
	__u8 fw_os[VERSION_MAX_LEN];
	__u8 comp_name[NAME_MAX_LEN];
	__u8 modules_counter;
	__u8 reserved[3];
	struct hl_module_data modules[];
};

 
#define HL_FW_VERSIONS_FIT_SIZE	4096

#endif  
