

#ifndef CPUCP_IF_H
#define CPUCP_IF_H

#include <linux/types.h>
#include <linux/if_ether.h>

#include "hl_boot_if.h"

#define NUM_HBM_PSEUDO_CH				2
#define NUM_HBM_CH_PER_DEV				8
#define CPUCP_PKT_HBM_ECC_INFO_WR_PAR_SHIFT		0
#define CPUCP_PKT_HBM_ECC_INFO_WR_PAR_MASK		0x00000001
#define CPUCP_PKT_HBM_ECC_INFO_RD_PAR_SHIFT		1
#define CPUCP_PKT_HBM_ECC_INFO_RD_PAR_MASK		0x00000002
#define CPUCP_PKT_HBM_ECC_INFO_CA_PAR_SHIFT		2
#define CPUCP_PKT_HBM_ECC_INFO_CA_PAR_MASK		0x00000004
#define CPUCP_PKT_HBM_ECC_INFO_DERR_SHIFT		3
#define CPUCP_PKT_HBM_ECC_INFO_DERR_MASK		0x00000008
#define CPUCP_PKT_HBM_ECC_INFO_SERR_SHIFT		4
#define CPUCP_PKT_HBM_ECC_INFO_SERR_MASK		0x00000010
#define CPUCP_PKT_HBM_ECC_INFO_TYPE_SHIFT		5
#define CPUCP_PKT_HBM_ECC_INFO_TYPE_MASK		0x00000020
#define CPUCP_PKT_HBM_ECC_INFO_HBM_CH_SHIFT		6
#define CPUCP_PKT_HBM_ECC_INFO_HBM_CH_MASK		0x000007C0

#define PLL_MAP_MAX_BITS	128
#define PLL_MAP_LEN		(PLL_MAP_MAX_BITS / 8)


struct cpucp_pkt_sync_err {
	__le32 pi;
	__le32 ci;
};

struct hl_eq_hbm_ecc_data {
	
	__le32 sec_cnt;
	
	__le32 dec_cnt;
	
	__le32 hbm_ecc_info;
	
	__le32 first_addr;
	
	__le32 sec_cont_cnt;
	__le32 pad;
};



struct hl_eq_header {
	__le32 reserved;
	__le32 ctl;
};

struct hl_eq_ecc_data {
	__le64 ecc_address;
	__le64 ecc_syndrom;
	__u8 memory_wrapper_idx;
	__u8 is_critical;
	__u8 pad[6];
};

enum hl_sm_sei_cause {
	SM_SEI_SO_OVERFLOW,
	SM_SEI_LBW_4B_UNALIGNED,
	SM_SEI_AXI_RESPONSE_ERR
};

struct hl_eq_sm_sei_data {
	__le32 sei_log;
	
	__u8 sei_cause;
	__u8 pad[3];
};

enum hl_fw_alive_severity {
	FW_ALIVE_SEVERITY_MINOR,
	FW_ALIVE_SEVERITY_CRITICAL
};

struct hl_eq_fw_alive {
	__le64 uptime_seconds;
	__le32 process_id;
	__le32 thread_id;
	
	__u8 severity;
	__u8 pad[7];
};

struct hl_eq_intr_cause {
	__le64 intr_cause_data;
};

struct hl_eq_pcie_drain_ind_data {
	struct hl_eq_intr_cause intr_cause;
	__le64 drain_wr_addr_lbw;
	__le64 drain_rd_addr_lbw;
	__le64 drain_wr_addr_hbw;
	__le64 drain_rd_addr_hbw;
};

struct hl_eq_razwi_lbw_info_regs {
	__le32 rr_aw_razwi_reg;
	__le32 rr_aw_razwi_id_reg;
	__le32 rr_ar_razwi_reg;
	__le32 rr_ar_razwi_id_reg;
};

struct hl_eq_razwi_hbw_info_regs {
	__le32 rr_aw_razwi_hi_reg;
	__le32 rr_aw_razwi_lo_reg;
	__le32 rr_aw_razwi_id_reg;
	__le32 rr_ar_razwi_hi_reg;
	__le32 rr_ar_razwi_lo_reg;
	__le32 rr_ar_razwi_id_reg;
};


#define RAZWI_HAPPENED_HBW	0x1
#define RAZWI_HAPPENED_LBW	0x2
#define RAZWI_HAPPENED_AW	0x4
#define RAZWI_HAPPENED_AR	0x8

struct hl_eq_razwi_info {
	__le32 razwi_happened_mask;
	union {
		struct hl_eq_razwi_lbw_info_regs lbw;
		struct hl_eq_razwi_hbw_info_regs hbw;
	};
	__le32 pad;
};

struct hl_eq_razwi_with_intr_cause {
	struct hl_eq_razwi_info razwi_info;
	struct hl_eq_intr_cause intr_cause;
};

#define HBM_CA_ERR_CMD_LIFO_LEN		8
#define HBM_RD_ERR_DATA_LIFO_LEN	8
#define HBM_WR_PAR_CMD_LIFO_LEN		11

enum hl_hbm_sei_cause {
	
	HBM_SEI_CMD_PARITY_EVEN,
	HBM_SEI_CMD_PARITY_ODD,
	
	HBM_SEI_READ_ERR,
	HBM_SEI_WRITE_DATA_PARITY_ERR,
	HBM_SEI_CATTRIP,
	HBM_SEI_MEM_BIST_FAIL,
	HBM_SEI_DFI,
	HBM_SEI_INV_TEMP_READ_OUT,
	HBM_SEI_BIST_FAIL,
};


#define HBM_ECC_SERR_CNTR_MASK		0xFF
#define HBM_ECC_DERR_CNTR_MASK		0xFF00
#define HBM_RD_PARITY_CNTR_MASK		0xFF0000


struct hl_hbm_sei_header {
	union {
		
		struct {
			__u8 ecc_serr_cnt;
			__u8 ecc_derr_cnt;
			__u8 read_par_cnt;
			__u8 reserved;
		};
		
		__le32 cnt;
	};
	__u8 sei_cause;		
	__u8 mc_channel;		
	__u8 mc_pseudo_channel;	
	__u8 is_critical;
};

#define HBM_RD_ADDR_SID_SHIFT		0
#define HBM_RD_ADDR_SID_MASK		0x1
#define HBM_RD_ADDR_BG_SHIFT		1
#define HBM_RD_ADDR_BG_MASK		0x6
#define HBM_RD_ADDR_BA_SHIFT		3
#define HBM_RD_ADDR_BA_MASK		0x18
#define HBM_RD_ADDR_COL_SHIFT		5
#define HBM_RD_ADDR_COL_MASK		0x7E0
#define HBM_RD_ADDR_ROW_SHIFT		11
#define HBM_RD_ADDR_ROW_MASK		0x3FFF800

struct hbm_rd_addr {
	union {
		
		struct {
			u32 dbg_rd_err_addr_sid:1;
			u32 dbg_rd_err_addr_bg:2;
			u32 dbg_rd_err_addr_ba:2;
			u32 dbg_rd_err_addr_col:6;
			u32 dbg_rd_err_addr_row:15;
			u32 reserved:6;
		};
		__le32 rd_addr_val;
	};
};

#define HBM_RD_ERR_BEAT_SHIFT		2


#define HBM_RD_ERR_PAR_ERR_BEAT0_SHIFT	0
#define HBM_RD_ERR_PAR_ERR_BEAT0_MASK	0x3
#define HBM_RD_ERR_PAR_DATA_BEAT0_SHIFT	8
#define HBM_RD_ERR_PAR_DATA_BEAT0_MASK	0x300

#define HBM_RD_ERR_SERR_BEAT0_SHIFT	16
#define HBM_RD_ERR_SERR_BEAT0_MASK	0x10000
#define HBM_RD_ERR_DERR_BEAT0_SHIFT	24
#define HBM_RD_ERR_DERR_BEAT0_MASK	0x100000

struct hl_eq_hbm_sei_read_err_intr_info {
	
	struct hbm_rd_addr dbg_rd_err_addr;
	
	union {
		struct {
			
			u32 dbg_rd_err_par:8;
			u32 dbg_rd_err_par_data:8;
			u32 dbg_rd_err_serr:4;
			u32 dbg_rd_err_derr:4;
			u32 reserved:8;
		};
		__le32 dbg_rd_err_misc;
	};
	
	__le32 dbg_rd_err_dm;
	
	__le32 dbg_rd_err_syndrome;
	
	__le32 dbg_rd_err_data[HBM_RD_ERR_DATA_LIFO_LEN];
};

struct hl_eq_hbm_sei_ca_par_intr_info {
	
	__le16 dbg_row[HBM_CA_ERR_CMD_LIFO_LEN];
	
	__le32 dbg_col[HBM_CA_ERR_CMD_LIFO_LEN];
};

#define WR_PAR_LAST_CMD_COL_SHIFT	0
#define WR_PAR_LAST_CMD_COL_MASK	0x3F
#define WR_PAR_LAST_CMD_BG_SHIFT	6
#define WR_PAR_LAST_CMD_BG_MASK		0xC0
#define WR_PAR_LAST_CMD_BA_SHIFT	8
#define WR_PAR_LAST_CMD_BA_MASK		0x300
#define WR_PAR_LAST_CMD_SID_SHIFT	10
#define WR_PAR_LAST_CMD_SID_MASK	0x400


struct hbm_sei_wr_cmd_address {
	
	union {
		struct {
			
			u32 col:6;
			u32 bg:2;
			u32 ba:2;
			u32 sid:1;
			u32 reserved:21;
		};
		__le32 dbg_wr_cmd_addr;
	};
};

struct hl_eq_hbm_sei_wr_par_intr_info {
	
	struct hbm_sei_wr_cmd_address dbg_last_wr_cmds[HBM_WR_PAR_CMD_LIFO_LEN];
	
	__u8 dbg_derr;
	
	__u8 pad[3];
};


struct hl_eq_hbm_sei_data {
	struct hl_hbm_sei_header hdr;
	union {
		struct hl_eq_hbm_sei_ca_par_intr_info ca_parity_even_info;
		struct hl_eq_hbm_sei_ca_par_intr_info ca_parity_odd_info;
		struct hl_eq_hbm_sei_read_err_intr_info read_err_info;
		struct hl_eq_hbm_sei_wr_par_intr_info wr_parity_info;
	};
};


enum hl_engine_arc_interrupt_type {
	
	ENGINE_ARC_DCCM_QUEUE_FULL_IRQ = 1
};


struct hl_engine_arc_dccm_queue_full_irq {
	
	__le32 queue_index;
	__le32 pad;
};


struct hl_eq_engine_arc_intr_data {
	
	__le32 engine_id;
	__le32 intr_type; 
	
	__le64 payload;
	__le64 pad[5];
};

#define ADDR_DEC_ADDRESS_COUNT_MAX 4


struct hl_eq_addr_dec_intr_data {
	struct hl_eq_intr_cause intr_cause;
	__le64 addr[ADDR_DEC_ADDRESS_COUNT_MAX];
	__u8 addr_cnt;
	__u8 pad[7];
};

struct hl_eq_entry {
	struct hl_eq_header hdr;
	union {
		__le64 data_placeholder;
		struct hl_eq_ecc_data ecc_data;
		struct hl_eq_hbm_ecc_data hbm_ecc_data;	
		struct hl_eq_sm_sei_data sm_sei_data;
		struct cpucp_pkt_sync_err pkt_sync_err;
		struct hl_eq_fw_alive fw_alive;
		struct hl_eq_intr_cause intr_cause;
		struct hl_eq_pcie_drain_ind_data pcie_drain_ind_data;
		struct hl_eq_razwi_info razwi_info;
		struct hl_eq_razwi_with_intr_cause razwi_with_intr_cause;
		struct hl_eq_hbm_sei_data sei_data;	
		struct hl_eq_engine_arc_intr_data arc_data;
		struct hl_eq_addr_dec_intr_data addr_dec;
		__le64 data[7];
	};
};

#define HL_EQ_ENTRY_SIZE		sizeof(struct hl_eq_entry)

#define EQ_CTL_READY_SHIFT		31
#define EQ_CTL_READY_MASK		0x80000000

#define EQ_CTL_EVENT_TYPE_SHIFT		16
#define EQ_CTL_EVENT_TYPE_MASK		0x0FFF0000

#define EQ_CTL_INDEX_SHIFT		0
#define EQ_CTL_INDEX_MASK		0x0000FFFF

enum pq_init_status {
	PQ_INIT_STATUS_NA = 0,
	PQ_INIT_STATUS_READY_FOR_CP,
	PQ_INIT_STATUS_READY_FOR_HOST,
	PQ_INIT_STATUS_READY_FOR_CP_SINGLE_MSI,
	PQ_INIT_STATUS_LEN_NOT_POWER_OF_TWO_ERR,
	PQ_INIT_STATUS_ILLEGAL_Q_ADDR_ERR
};



enum cpucp_packet_id {
	CPUCP_PACKET_DISABLE_PCI_ACCESS = 1,	
	CPUCP_PACKET_ENABLE_PCI_ACCESS,		
	CPUCP_PACKET_TEMPERATURE_GET,		
	CPUCP_PACKET_VOLTAGE_GET,		
	CPUCP_PACKET_CURRENT_GET,		
	CPUCP_PACKET_FAN_SPEED_GET,		
	CPUCP_PACKET_PWM_GET,			
	CPUCP_PACKET_PWM_SET,			
	CPUCP_PACKET_FREQUENCY_SET,		
	CPUCP_PACKET_FREQUENCY_GET,		
	CPUCP_PACKET_LED_SET,			
	CPUCP_PACKET_I2C_WR,			
	CPUCP_PACKET_I2C_RD,			
	CPUCP_PACKET_INFO_GET,			
	CPUCP_PACKET_FLASH_PROGRAM_REMOVED,
	CPUCP_PACKET_UNMASK_RAZWI_IRQ,		
	CPUCP_PACKET_UNMASK_RAZWI_IRQ_ARRAY,	
	CPUCP_PACKET_TEST,			
	CPUCP_PACKET_FREQUENCY_CURR_GET,	
	CPUCP_PACKET_MAX_POWER_GET,		
	CPUCP_PACKET_MAX_POWER_SET,		
	CPUCP_PACKET_EEPROM_DATA_GET,		
	CPUCP_PACKET_NIC_INFO_GET,		
	CPUCP_PACKET_TEMPERATURE_SET,		
	CPUCP_PACKET_VOLTAGE_SET,		
	CPUCP_PACKET_CURRENT_SET,		
	CPUCP_PACKET_PCIE_THROUGHPUT_GET,	
	CPUCP_PACKET_PCIE_REPLAY_CNT_GET,	
	CPUCP_PACKET_TOTAL_ENERGY_GET,		
	CPUCP_PACKET_PLL_INFO_GET,		
	CPUCP_PACKET_NIC_STATUS,		
	CPUCP_PACKET_POWER_GET,			
	CPUCP_PACKET_NIC_PFC_SET,		
	CPUCP_PACKET_NIC_FAULT_GET,		
	CPUCP_PACKET_NIC_LPBK_SET,		
	CPUCP_PACKET_NIC_MAC_CFG,		
	CPUCP_PACKET_MSI_INFO_SET,		
	CPUCP_PACKET_NIC_XPCS91_REGS_GET,	
	CPUCP_PACKET_NIC_STAT_REGS_GET,		
	CPUCP_PACKET_NIC_STAT_REGS_CLR,		
	CPUCP_PACKET_NIC_STAT_REGS_ALL_GET,	
	CPUCP_PACKET_IS_IDLE_CHECK,		
	CPUCP_PACKET_HBM_REPLACED_ROWS_INFO_GET,
	CPUCP_PACKET_HBM_PENDING_ROWS_STATUS,	
	CPUCP_PACKET_POWER_SET,			
	CPUCP_PACKET_RESERVED,			
	CPUCP_PACKET_ENGINE_CORE_ASID_SET,	
	CPUCP_PACKET_RESERVED2,			
	CPUCP_PACKET_SEC_ATTEST_GET,		
	CPUCP_PACKET_RESERVED3,			
	CPUCP_PACKET_RESERVED4,			
	CPUCP_PACKET_MONITOR_DUMP_GET,		
	CPUCP_PACKET_RESERVED5,			
	CPUCP_PACKET_RESERVED6,			
	CPUCP_PACKET_RESERVED7,			
	CPUCP_PACKET_GENERIC_PASSTHROUGH,	
	CPUCP_PACKET_RESERVED8,			
	CPUCP_PACKET_ACTIVE_STATUS_SET,		
	CPUCP_PACKET_RESERVED9,			
	CPUCP_PACKET_RESERVED10,		
	CPUCP_PACKET_RESERVED11,		
	CPUCP_PACKET_RESERVED12,		
	CPUCP_PACKET_REGISTER_INTERRUPTS,	
	CPUCP_PACKET_SOFT_RESET,		
	CPUCP_PACKET_ID_MAX			
};

#define CPUCP_PACKET_FENCE_VAL	0xFE8CE7A5

#define CPUCP_PKT_CTL_RC_SHIFT		12
#define CPUCP_PKT_CTL_RC_MASK		0x0000F000

#define CPUCP_PKT_CTL_OPCODE_SHIFT	16
#define CPUCP_PKT_CTL_OPCODE_MASK	0x1FFF0000

#define CPUCP_PKT_RES_PLL_OUT0_SHIFT	0
#define CPUCP_PKT_RES_PLL_OUT0_MASK	0x000000000000FFFFull
#define CPUCP_PKT_RES_PLL_OUT1_SHIFT	16
#define CPUCP_PKT_RES_PLL_OUT1_MASK	0x00000000FFFF0000ull
#define CPUCP_PKT_RES_PLL_OUT2_SHIFT	32
#define CPUCP_PKT_RES_PLL_OUT2_MASK	0x0000FFFF00000000ull
#define CPUCP_PKT_RES_PLL_OUT3_SHIFT	48
#define CPUCP_PKT_RES_PLL_OUT3_MASK	0xFFFF000000000000ull

#define CPUCP_PKT_RES_EEPROM_OUT0_SHIFT	0
#define CPUCP_PKT_RES_EEPROM_OUT0_MASK	0x000000000000FFFFull
#define CPUCP_PKT_RES_EEPROM_OUT1_SHIFT	16
#define CPUCP_PKT_RES_EEPROM_OUT1_MASK	0x0000000000FF0000ull

#define CPUCP_PKT_VAL_PFC_IN1_SHIFT	0
#define CPUCP_PKT_VAL_PFC_IN1_MASK	0x0000000000000001ull
#define CPUCP_PKT_VAL_PFC_IN2_SHIFT	1
#define CPUCP_PKT_VAL_PFC_IN2_MASK	0x000000000000001Eull

#define CPUCP_PKT_VAL_LPBK_IN1_SHIFT	0
#define CPUCP_PKT_VAL_LPBK_IN1_MASK	0x0000000000000001ull
#define CPUCP_PKT_VAL_LPBK_IN2_SHIFT	1
#define CPUCP_PKT_VAL_LPBK_IN2_MASK	0x000000000000001Eull

#define CPUCP_PKT_VAL_MAC_CNT_IN1_SHIFT	0
#define CPUCP_PKT_VAL_MAC_CNT_IN1_MASK	0x0000000000000001ull
#define CPUCP_PKT_VAL_MAC_CNT_IN2_SHIFT	1
#define CPUCP_PKT_VAL_MAC_CNT_IN2_MASK	0x00000000FFFFFFFEull


#define CPUCP_PKT_HB_STATUS_EQ_FAULT_SHIFT		0
#define CPUCP_PKT_HB_STATUS_EQ_FAULT_MASK		0x00000001

struct cpucp_packet {
	union {
		__le64 value;	
		__le64 result;	
		__le64 addr;	
	};

	__le32 ctl;

	__le32 fence;		

	union {
		struct {
			__le16 sensor_index;
			__le16 type;
		};

		struct {	
			__u8 i2c_bus;
			__u8 i2c_addr;
			__u8 i2c_reg;
			
			__u8 i2c_len;
		};

		struct {
			__le16 pll_type;
			
			__le16 pll_reg;
		};

		
		__le32 index;

		
		__le32 pll_index;

		
		__le32 led_index;

		
		__le32 data_max_size;

		
		__le32 status_mask;

		
		__le32 nonce;
	};

	union {
		
		__le32 port_index;

		
		__le32 pkt_subidx;
	};
};

struct cpucp_unmask_irq_arr_packet {
	struct cpucp_packet cpucp_pkt;
	__le32 length;
	__le32 irqs[];
};

struct cpucp_nic_status_packet {
	struct cpucp_packet cpucp_pkt;
	__le32 length;
	__le32 data[];
};

struct cpucp_array_data_packet {
	struct cpucp_packet cpucp_pkt;
	__le32 length;
	__le32 data[];
};

enum cpucp_led_index {
	CPUCP_LED0_INDEX = 0,
	CPUCP_LED1_INDEX,
	CPUCP_LED2_INDEX,
	CPUCP_LED_MAX_INDEX = CPUCP_LED2_INDEX
};


enum cpucp_packet_rc {
	cpucp_packet_success,
	cpucp_packet_invalid,
	cpucp_packet_fault,
	cpucp_packet_invalid_pkt,
	cpucp_packet_invalid_params,
	cpucp_packet_rc_max
};


enum cpucp_temp_type {
	cpucp_temp_input,
	cpucp_temp_min = 4,
	cpucp_temp_min_hyst,
	cpucp_temp_max = 6,
	cpucp_temp_max_hyst,
	cpucp_temp_crit,
	cpucp_temp_crit_hyst,
	cpucp_temp_offset = 19,
	cpucp_temp_lowest = 21,
	cpucp_temp_highest = 22,
	cpucp_temp_reset_history = 23,
	cpucp_temp_warn = 24,
	cpucp_temp_max_crit = 25,
	cpucp_temp_max_warn = 26,
};

enum cpucp_in_attributes {
	cpucp_in_input,
	cpucp_in_min,
	cpucp_in_max,
	cpucp_in_lowest = 6,
	cpucp_in_highest = 7,
	cpucp_in_reset_history,
	cpucp_in_intr_alarm_a,
	cpucp_in_intr_alarm_b,
};

enum cpucp_curr_attributes {
	cpucp_curr_input,
	cpucp_curr_min,
	cpucp_curr_max,
	cpucp_curr_lowest = 6,
	cpucp_curr_highest = 7,
	cpucp_curr_reset_history
};

enum cpucp_fan_attributes {
	cpucp_fan_input,
	cpucp_fan_min = 2,
	cpucp_fan_max
};

enum cpucp_pwm_attributes {
	cpucp_pwm_input,
	cpucp_pwm_enable
};

enum cpucp_pcie_throughput_attributes {
	cpucp_pcie_throughput_tx,
	cpucp_pcie_throughput_rx
};


enum cpucp_pll_reg_attributes {
	cpucp_pll_nr_reg,
	cpucp_pll_nf_reg,
	cpucp_pll_od_reg,
	cpucp_pll_div_factor_reg,
	cpucp_pll_div_sel_reg
};


enum cpucp_pll_type_attributes {
	cpucp_pll_cpu,
	cpucp_pll_pci,
};


enum cpucp_power_type {
	CPUCP_POWER_INPUT = 8,
	CPUCP_POWER_INPUT_HIGHEST = 9,
	CPUCP_POWER_RESET_INPUT_HISTORY = 11
};


enum cpucp_msi_type {
	CPUCP_EVENT_QUEUE_MSI_TYPE,
	CPUCP_NIC_PORT1_MSI_TYPE,
	CPUCP_NIC_PORT3_MSI_TYPE,
	CPUCP_NIC_PORT5_MSI_TYPE,
	CPUCP_NIC_PORT7_MSI_TYPE,
	CPUCP_NIC_PORT9_MSI_TYPE,
	CPUCP_NUM_OF_MSI_TYPES
};


enum pll_index {
	CPU_PLL = 0,
	PCI_PLL = 1,
	NIC_PLL = 2,
	DMA_PLL = 3,
	MESH_PLL = 4,
	MME_PLL = 5,
	TPC_PLL = 6,
	IF_PLL = 7,
	SRAM_PLL = 8,
	NS_PLL = 9,
	HBM_PLL = 10,
	MSS_PLL = 11,
	DDR_PLL = 12,
	VID_PLL = 13,
	BANK_PLL = 14,
	MMU_PLL = 15,
	IC_PLL = 16,
	MC_PLL = 17,
	EMMC_PLL = 18,
	D2D_PLL = 19,
	CS_PLL = 20,
	C2C_PLL = 21,
	NCH_PLL = 22,
	C2M_PLL = 23,
	PLL_MAX
};

enum rl_index {
	TPC_RL = 0,
	MME_RL,
	EDMA_RL,
};

enum pvt_index {
	PVT_SW,
	PVT_SE,
	PVT_NW,
	PVT_NE
};



struct eq_generic_event {
	__le64 data[7];
};



#define CARD_NAME_MAX_LEN		16
#define CPUCP_MAX_SENSORS		128
#define CPUCP_MAX_NICS			128
#define CPUCP_LANES_PER_NIC		4
#define CPUCP_NIC_QSFP_EEPROM_MAX_LEN	1024
#define CPUCP_MAX_NIC_LANES		(CPUCP_MAX_NICS * CPUCP_LANES_PER_NIC)
#define CPUCP_NIC_MASK_ARR_LEN		((CPUCP_MAX_NICS + 63) / 64)
#define CPUCP_NIC_POLARITY_ARR_LEN	((CPUCP_MAX_NIC_LANES + 63) / 64)
#define CPUCP_HBM_ROW_REPLACE_MAX	32

struct cpucp_sensor {
	__le32 type;
	__le32 flags;
};


enum cpucp_card_types {
	cpucp_card_type_pci,
	cpucp_card_type_pmc
};

#define CPUCP_SEC_CONF_ENABLED_SHIFT	0
#define CPUCP_SEC_CONF_ENABLED_MASK	0x00000001

#define CPUCP_SEC_CONF_FLASH_WP_SHIFT	1
#define CPUCP_SEC_CONF_FLASH_WP_MASK	0x00000002

#define CPUCP_SEC_CONF_EEPROM_WP_SHIFT	2
#define CPUCP_SEC_CONF_EEPROM_WP_MASK	0x00000004


struct cpucp_security_info {
	__u8 config;
	__u8 keys_num;
	__u8 revoked_keys;
	__u8 min_svn;
};


struct cpucp_info {
	struct cpucp_sensor sensors[CPUCP_MAX_SENSORS];
	__u8 kernel_version[VERSION_MAX_LEN];
	__le32 reserved;
	__le32 card_type;
	__le32 card_location;
	__le32 cpld_version;
	__le32 infineon_version;
	__u8 fuse_version[VERSION_MAX_LEN];
	__u8 thermal_version[VERSION_MAX_LEN];
	__u8 cpucp_version[VERSION_MAX_LEN];
	__le32 infineon_second_stage_version;
	__le64 dram_size;
	char card_name[CARD_NAME_MAX_LEN];
	__le64 tpc_binning_mask;
	__le64 decoder_binning_mask;
	__u8 sram_binning;
	__u8 dram_binning_mask;
	__u8 memory_repair_flag;
	__u8 edma_binning_mask;
	__u8 xbar_binning_mask;
	__u8 interposer_version;
	__u8 substrate_version;
	__u8 reserved2;
	struct cpucp_security_info sec_info;
	__le32 fw_hbm_region_size;
	__u8 pll_map[PLL_MAP_LEN];
	__le64 mme_binning_mask;
	__u8 fw_os_version[VERSION_MAX_LEN];
};

struct cpucp_mac_addr {
	__u8 mac_addr[ETH_ALEN];
};

enum cpucp_serdes_type {
	TYPE_1_SERDES_TYPE,
	TYPE_2_SERDES_TYPE,
	HLS1_SERDES_TYPE,
	HLS1H_SERDES_TYPE,
	HLS2_SERDES_TYPE,
	HLS2_TYPE_1_SERDES_TYPE,
	MAX_NUM_SERDES_TYPE,		
	UNKNOWN_SERDES_TYPE = 0xFFFF	
};

struct cpucp_nic_info {
	struct cpucp_mac_addr mac_addrs[CPUCP_MAX_NICS];
	__le64 link_mask[CPUCP_NIC_MASK_ARR_LEN];
	__le64 pol_tx_mask[CPUCP_NIC_POLARITY_ARR_LEN];
	__le64 pol_rx_mask[CPUCP_NIC_POLARITY_ARR_LEN];
	__le64 link_ext_mask[CPUCP_NIC_MASK_ARR_LEN];
	__u8 qsfp_eeprom[CPUCP_NIC_QSFP_EEPROM_MAX_LEN];
	__le64 auto_neg_mask[CPUCP_NIC_MASK_ARR_LEN];
	__le16 serdes_type; 
	__le16 tx_swap_map[CPUCP_MAX_NICS];
	__u8 reserved[6];
};

#define PAGE_DISCARD_MAX	64

struct page_discard_info {
	__u8 num_entries;
	__u8 reserved[7];
	__le32 mmu_page_idx[PAGE_DISCARD_MAX];
};


struct frac_val {
	union {
		struct {
			__le16 integer;
			__le16 frac;
		};
		__le32 val;
	};
};


struct ser_val {
	__le16 integer;
	__le16 exp;
};


struct cpucp_nic_status {
	__le32 port;
	__le32 bad_format_cnt;
	__le32 responder_out_of_sequence_psn_cnt;
	__le32 high_ber_reinit;
	__le32 correctable_err_cnt;
	__le32 uncorrectable_err_cnt;
	__le32 retraining_cnt;
	__u8 up;
	__u8 pcs_link;
	__u8 phy_ready;
	__u8 auto_neg;
	__le32 timeout_retransmission_cnt;
	__le32 high_ber_cnt;
	struct ser_val pre_fec_ser;
	struct ser_val post_fec_ser;
	struct frac_val bandwidth;
	struct frac_val lat;
};

enum cpucp_hbm_row_replace_cause {
	REPLACE_CAUSE_DOUBLE_ECC_ERR,
	REPLACE_CAUSE_MULTI_SINGLE_ECC_ERR,
};

struct cpucp_hbm_row_info {
	__u8 hbm_idx;
	__u8 pc;
	__u8 sid;
	__u8 bank_idx;
	__le16 row_addr;
	__u8 replaced_row_cause; 
	__u8 pad;
};

struct cpucp_hbm_row_replaced_rows_info {
	__le16 num_replaced_rows;
	__u8 pad[6];
	struct cpucp_hbm_row_info replaced_rows[CPUCP_HBM_ROW_REPLACE_MAX];
};

enum cpu_reset_status {
	CPU_RST_STATUS_NA = 0,
	CPU_RST_STATUS_SOFT_RST_DONE = 1,
};

#define SEC_PCR_DATA_BUF_SZ	256
#define SEC_PCR_QUOTE_BUF_SZ	510	
#define SEC_SIGNATURE_BUF_SZ	255	
#define SEC_PUB_DATA_BUF_SZ	510	
#define SEC_CERTIFICATE_BUF_SZ	2046	


struct cpucp_sec_attest_info {
	__u8 pcr_data[SEC_PCR_DATA_BUF_SZ];
	__u8 pcr_num_reg;
	__u8 pcr_reg_len;
	__le16 pad0;
	__le32 nonce;
	__le16 pcr_quote_len;
	__u8 pcr_quote[SEC_PCR_QUOTE_BUF_SZ];
	__u8 quote_sig_len;
	__u8 quote_sig[SEC_SIGNATURE_BUF_SZ];
	__le16 pub_data_len;
	__u8 public_data[SEC_PUB_DATA_BUF_SZ];
	__le16 certificate_len;
	__u8 certificate[SEC_CERTIFICATE_BUF_SZ];
};


struct cpucp_dev_info_signed {
	struct cpucp_info info;	
	__le32 nonce;
	__le32 pad0;
	__u8 info_sig_len;
	__u8 info_sig[SEC_SIGNATURE_BUF_SZ];
	__le16 pub_data_len;
	__u8 public_data[SEC_PUB_DATA_BUF_SZ];
	__le16 certificate_len;
	__u8 certificate[SEC_CERTIFICATE_BUF_SZ];
};

#define DCORE_MON_REGS_SZ	512

struct dcore_monitor_regs_data {
	__le32 mon_pay_addrl[DCORE_MON_REGS_SZ];
	__le32 mon_pay_addrh[DCORE_MON_REGS_SZ];
	__le32 mon_pay_data[DCORE_MON_REGS_SZ];
	__le32 mon_arm[DCORE_MON_REGS_SZ];
	__le32 mon_status[DCORE_MON_REGS_SZ];
};


struct cpucp_monitor_dump {
	struct dcore_monitor_regs_data sync_mngr_w_s;
	struct dcore_monitor_regs_data sync_mngr_e_s;
	struct dcore_monitor_regs_data sync_mngr_w_n;
	struct dcore_monitor_regs_data sync_mngr_e_n;
};


enum hl_passthrough_type {
	HL_PASSTHROUGH_VERSIONS,
};

#endif 
