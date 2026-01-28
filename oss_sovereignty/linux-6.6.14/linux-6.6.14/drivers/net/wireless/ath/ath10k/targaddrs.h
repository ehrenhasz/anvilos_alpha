#ifndef __TARGADDRS_H__
#define __TARGADDRS_H__
#include "hw.h"
#define QCA988X_HOST_INTEREST_ADDRESS    0x00400800
#define HOST_INTEREST_MAX_SIZE          0x200
struct host_interest {
	u32 hi_app_host_interest;			 
	u32 hi_failure_state;				 
	u32 hi_dbglog_hdr;				 
	u32 hi_unused0c;				 
	u32 hi_option_flag;				 
	u32 hi_serial_enable;				 
	u32 hi_dset_list_head;				 
	u32 hi_app_start;				 
	u32 hi_skip_clock_init;				 
	u32 hi_core_clock_setting;			 
	u32 hi_cpu_clock_setting;			 
	u32 hi_system_sleep_setting;			 
	u32 hi_xtal_control_setting;			 
	u32 hi_pll_ctrl_setting_24ghz;			 
	u32 hi_pll_ctrl_setting_5ghz;			 
	u32 hi_ref_voltage_trim_setting;		 
	u32 hi_clock_info;				 
	u32 hi_be;					 
	u32 hi_stack;	 			 
	u32 hi_err_stack;  		 
	u32 hi_desired_cpu_speed_hz;			 
	u32 hi_board_data;				 
	u32 hi_board_data_initialized;			 
	u32 hi_dset_ram_index_table;			 
	u32 hi_desired_baud_rate;			 
	u32 hi_dbglog_config;				 
	u32 hi_end_ram_reserve_sz;			 
	u32 hi_mbox_io_block_sz;			 
	u32 hi_num_bpatch_streams;			 
	u32 hi_mbox_isr_yield_limit;			 
	u32 hi_refclk_hz;				 
	u32 hi_ext_clk_detected;			 
	u32 hi_dbg_uart_txpin;				 
	u32 hi_dbg_uart_rxpin;				 
	u32 hi_hci_uart_baud;				 
	u32 hi_hci_uart_pin_assignments;		 
	u32 hi_hci_uart_baud_scale_val;			 
	u32 hi_hci_uart_baud_step_val;			 
	u32 hi_allocram_start;				 
	u32 hi_allocram_sz;				 
	u32 hi_hci_bridge_flags;			 
	u32 hi_hci_uart_support_pins;			 
	u32 hi_hci_uart_pwr_mgmt_params;		 
	u32 hi_board_ext_data;				 
	u32 hi_board_ext_data_config;			 
	u32  hi_reset_flag;				 
	u32  hi_reset_flag_valid;			 
	u32 hi_hci_uart_pwr_mgmt_params_ext;		 
	u32 hi_acs_flags;				 
	u32 hi_console_flags;				 
	u32 hi_nvram_state;				 
	u32 hi_option_flag2;				 
	u32 hi_sw_version_override;			 
	u32 hi_abi_version_override;			 
	u32 hi_hp_rx_traffic_ratio;			 
	u32 hi_test_apps_related;			 
	u32 hi_ota_testscript;				 
	u32 hi_cal_data;				 
	u32 hi_pktlog_num_buffers;			 
	u32 hi_wow_ext_config;				 
	u32 hi_pwr_save_flags;				 
	u32 hi_smps_options;				 
	u32 hi_interconnect_state;			 
	u32 hi_coex_config;				 
	u32 hi_early_alloc;				 
	u32 hi_fw_swap;					 
	u32 hi_dynamic_mem_arenas_addr;			 
	u32 hi_dynamic_mem_allocated;			 
	u32 hi_dynamic_mem_remaining;			 
	u32 hi_dynamic_mem_track_max;			 
	u32 hi_minidump;				 
	u32 hi_bd_sig_key;				 
} __packed;
#define HI_ITEM(item)  offsetof(struct host_interest, item)
#define HI_OPTION_TIMER_WAR         0x01
#define HI_OPTION_BMI_CRED_LIMIT    0x02
#define HI_OPTION_RELAY_DOT11_HDR   0x04
#define HI_OPTION_MAC_ADDR_METHOD   0x08
#define HI_OPTION_FW_BRIDGE         0x10
#define HI_OPTION_ENABLE_PROFILE    0x20
#define HI_OPTION_DISABLE_DBGLOG    0x40
#define HI_OPTION_SKIP_ERA_TRACKING 0x80
#define HI_OPTION_PAPRD_DISABLE     0x100
#define HI_OPTION_NUM_DEV_LSB       0x200
#define HI_OPTION_NUM_DEV_MSB       0x800
#define HI_OPTION_DEV_MODE_LSB      0x1000
#define HI_OPTION_DEV_MODE_MSB      0x8000000
#define HI_OPTION_NO_LFT_STBL       0x10000000
#define HI_OPTION_SKIP_REG_SCAN     0x20000000
#define HI_OPTION_INIT_REG_SCAN     0x40000000
#define HI_OPTION_SKIP_MEMMAP       0x80000000
#define HI_OPTION_MAC_ADDR_METHOD_SHIFT 3
#define HI_OPTION_FW_MODE_IBSS    0x0  
#define HI_OPTION_FW_MODE_BSS_STA 0x1  
#define HI_OPTION_FW_MODE_AP      0x2  
#define HI_OPTION_FW_MODE_BT30AMP 0x3  
#define HI_OPTION_FW_SUBMODE_NONE    0x0   
#define HI_OPTION_FW_SUBMODE_P2PDEV  0x1   
#define HI_OPTION_FW_SUBMODE_P2PCLIENT 0x2  
#define HI_OPTION_FW_SUBMODE_P2PGO   0x3  
#define HI_OPTION_NUM_DEV_MASK    0x7
#define HI_OPTION_NUM_DEV_SHIFT   0x9
#define HI_OPTION_FW_BRIDGE_SHIFT 0x04
#define HI_OPTION_FW_MODE_BITS         0x2
#define HI_OPTION_FW_MODE_MASK         0x3
#define HI_OPTION_FW_MODE_SHIFT        0xC
#define HI_OPTION_ALL_FW_MODE_MASK     0xFF
#define HI_OPTION_FW_SUBMODE_BITS      0x2
#define HI_OPTION_FW_SUBMODE_MASK      0x3
#define HI_OPTION_FW_SUBMODE_SHIFT     0x14
#define HI_OPTION_ALL_FW_SUBMODE_MASK  0xFF00
#define HI_OPTION_ALL_FW_SUBMODE_SHIFT 0x8
#define HI_OPTION_OFFLOAD_AMSDU     0x01
#define HI_OPTION_DFS_SUPPORT       0x02  
#define HI_OPTION_ENABLE_RFKILL     0x04  
#define HI_OPTION_RADIO_RETENTION_DISABLE 0x08  
#define HI_OPTION_EARLY_CFG_DONE    0x10  
#define HI_OPTION_RF_KILL_SHIFT     0x2
#define HI_OPTION_RF_KILL_MASK      0x1
#define HI_RESET_FLAG_PRESERVE_APP_START         0x01
#define HI_RESET_FLAG_PRESERVE_HOST_INTEREST     0x02
#define HI_RESET_FLAG_PRESERVE_ROMDATA           0x04
#define HI_RESET_FLAG_PRESERVE_NVRAM_STATE       0x08
#define HI_RESET_FLAG_PRESERVE_BOOT_INFO         0x10
#define HI_RESET_FLAG_WARM_RESET	0x20
#define HI_DESC_IN_FW_BIT	0x01
#define HI_RESET_FLAG_IS_VALID  0x12345678
#define HI_ACS_FLAGS_ENABLED        (1 << 0)
#define HI_ACS_FLAGS_USE_WWAN       (1 << 1)
#define HI_ACS_FLAGS_TEST_VAP       (1 << 2)
#define HI_ACS_FLAGS_SDIO_SWAP_MAILBOX_SET       (1 << 0)
#define HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_SET    (1 << 1)
#define HI_ACS_FLAGS_ALT_DATA_CREDIT_SIZE        (1 << 2)
#define HI_ACS_FLAGS_SDIO_SWAP_MAILBOX_FW_ACK    (1 << 16)
#define HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_FW_ACK (1 << 17)
#define HI_OPTION_SDIO_CRASH_DUMP_ENHANCEMENT_HOST 0x400
#define HI_OPTION_SDIO_CRASH_DUMP_ENHANCEMENT_FW   0x800
#define HI_CONSOLE_FLAGS_ENABLE       (1 << 31)
#define HI_CONSOLE_FLAGS_UART_MASK    (0x7)
#define HI_CONSOLE_FLAGS_UART_SHIFT   0
#define HI_CONSOLE_FLAGS_BAUD_SELECT  (1 << 3)
#define HI_SMPS_ALLOW_MASK            (0x00000001)
#define HI_SMPS_MODE_MASK             (0x00000002)
#define HI_SMPS_MODE_STATIC           (0x00000000)
#define HI_SMPS_MODE_DYNAMIC          (0x00000002)
#define HI_SMPS_DISABLE_AUTO_MODE     (0x00000004)
#define HI_SMPS_DATA_THRESH_MASK      (0x000007f8)
#define HI_SMPS_DATA_THRESH_SHIFT     (3)
#define HI_SMPS_RSSI_THRESH_MASK      (0x0007f800)
#define HI_SMPS_RSSI_THRESH_SHIFT     (11)
#define HI_SMPS_LOWPWR_CM_MASK        (0x00380000)
#define HI_SMPS_LOWPWR_CM_SHIFT       (15)
#define HI_SMPS_HIPWR_CM_MASK         (0x03c00000)
#define HI_SMPS_HIPWR_CM_SHIFT        (19)
#define HI_WOW_EXT_ENABLED_MASK        (1 << 31)
#define HI_WOW_EXT_NUM_LIST_SHIFT      16
#define HI_WOW_EXT_NUM_LIST_MASK       (0x3 << HI_WOW_EXT_NUM_LIST_SHIFT)
#define HI_WOW_EXT_NUM_PATTERNS_SHIFT  9
#define HI_WOW_EXT_NUM_PATTERNS_MASK   (0x7F << HI_WOW_EXT_NUM_PATTERNS_SHIFT)
#define HI_WOW_EXT_PATTERN_SIZE_SHIFT  0
#define HI_WOW_EXT_PATTERN_SIZE_MASK   (0x1FF << HI_WOW_EXT_PATTERN_SIZE_SHIFT)
#define HI_WOW_EXT_MAKE_CONFIG(num_lists, count, size) \
	((((num_lists) << HI_WOW_EXT_NUM_LIST_SHIFT) & \
		HI_WOW_EXT_NUM_LIST_MASK) | \
	(((count) << HI_WOW_EXT_NUM_PATTERNS_SHIFT) & \
		HI_WOW_EXT_NUM_PATTERNS_MASK) | \
	(((size) << HI_WOW_EXT_PATTERN_SIZE_SHIFT) & \
		HI_WOW_EXT_PATTERN_SIZE_MASK))
#define HI_WOW_EXT_GET_NUM_LISTS(config) \
	(((config) & HI_WOW_EXT_NUM_LIST_MASK) >> HI_WOW_EXT_NUM_LIST_SHIFT)
#define HI_WOW_EXT_GET_NUM_PATTERNS(config) \
	(((config) & HI_WOW_EXT_NUM_PATTERNS_MASK) >> \
		HI_WOW_EXT_NUM_PATTERNS_SHIFT)
#define HI_WOW_EXT_GET_PATTERN_SIZE(config) \
	(((config) & HI_WOW_EXT_PATTERN_SIZE_MASK) >> \
		HI_WOW_EXT_PATTERN_SIZE_SHIFT)
#define HI_EARLY_ALLOC_MAGIC		0x6d8a
#define HI_EARLY_ALLOC_MAGIC_MASK	0xffff0000
#define HI_EARLY_ALLOC_MAGIC_SHIFT	16
#define HI_EARLY_ALLOC_IRAM_BANKS_MASK	0x0000000f
#define HI_EARLY_ALLOC_IRAM_BANKS_SHIFT	0
#define HI_EARLY_ALLOC_VALID() \
	((((HOST_INTEREST->hi_early_alloc) & HI_EARLY_ALLOC_MAGIC_MASK) >> \
	HI_EARLY_ALLOC_MAGIC_SHIFT) == (HI_EARLY_ALLOC_MAGIC))
#define HI_EARLY_ALLOC_GET_IRAM_BANKS() \
	(((HOST_INTEREST->hi_early_alloc) & HI_EARLY_ALLOC_IRAM_BANKS_MASK) \
	>> HI_EARLY_ALLOC_IRAM_BANKS_SHIFT)
#define HI_PWR_SAVE_LPL_ENABLED   0x1
#define HI_PWR_SAVE_LPL_DEV0_LSB   4
#define HI_PWR_SAVE_LPL_DEV_MASK   0x3
#define HI_LPL_ENABLED() \
	((HOST_INTEREST->hi_pwr_save_flags & HI_PWR_SAVE_LPL_ENABLED))
#define HI_DEV_LPL_TYPE_GET(_devix) \
	(HOST_INTEREST->hi_pwr_save_flags & ((HI_PWR_SAVE_LPL_DEV_MASK) << \
	 (HI_PWR_SAVE_LPL_DEV0_LSB + (_devix) * 2)))
#define HOST_INTEREST_SMPS_IS_ALLOWED() \
	((HOST_INTEREST->hi_smps_options & HI_SMPS_ALLOW_MASK))
#define QCA988X_BOARD_DATA_SZ     7168
#define QCA988X_BOARD_EXT_DATA_SZ 0
#define QCA9887_BOARD_DATA_SZ     7168
#define QCA9887_BOARD_EXT_DATA_SZ 0
#define QCA6174_BOARD_DATA_SZ     8192
#define QCA6174_BOARD_EXT_DATA_SZ 0
#define QCA9377_BOARD_DATA_SZ     QCA6174_BOARD_DATA_SZ
#define QCA9377_BOARD_EXT_DATA_SZ 0
#define QCA99X0_BOARD_DATA_SZ	  12288
#define QCA99X0_BOARD_EXT_DATA_SZ 0
#define QCA99X0_EXT_BOARD_DATA_SZ 2048
#define EXT_BOARD_ADDRESS_OFFSET 0x3000
#define QCA4019_BOARD_DATA_SZ	  12064
#define QCA4019_BOARD_EXT_DATA_SZ 0
#endif  
