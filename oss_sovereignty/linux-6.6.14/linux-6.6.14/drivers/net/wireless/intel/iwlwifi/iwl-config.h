#ifndef __IWL_CONFIG_H__
#define __IWL_CONFIG_H__
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/ieee80211.h>
#include <linux/nl80211.h>
#include "iwl-csr.h"
enum iwl_device_family {
	IWL_DEVICE_FAMILY_UNDEFINED,
	IWL_DEVICE_FAMILY_1000,
	IWL_DEVICE_FAMILY_100,
	IWL_DEVICE_FAMILY_2000,
	IWL_DEVICE_FAMILY_2030,
	IWL_DEVICE_FAMILY_105,
	IWL_DEVICE_FAMILY_135,
	IWL_DEVICE_FAMILY_5000,
	IWL_DEVICE_FAMILY_5150,
	IWL_DEVICE_FAMILY_6000,
	IWL_DEVICE_FAMILY_6000i,
	IWL_DEVICE_FAMILY_6005,
	IWL_DEVICE_FAMILY_6030,
	IWL_DEVICE_FAMILY_6050,
	IWL_DEVICE_FAMILY_6150,
	IWL_DEVICE_FAMILY_7000,
	IWL_DEVICE_FAMILY_8000,
	IWL_DEVICE_FAMILY_9000,
	IWL_DEVICE_FAMILY_22000,
	IWL_DEVICE_FAMILY_AX210,
	IWL_DEVICE_FAMILY_BZ,
	IWL_DEVICE_FAMILY_SC,
};
enum iwl_led_mode {
	IWL_LED_DEFAULT,
	IWL_LED_RF_STATE,
	IWL_LED_BLINK,
	IWL_LED_DISABLE,
};
enum iwl_nvm_type {
	IWL_NVM,
	IWL_NVM_EXT,
	IWL_NVM_SDP,
};
#define IWL_MAX_PLCP_ERR_THRESHOLD_MIN		1
#define IWL_MAX_PLCP_ERR_THRESHOLD_DEF		50
#define IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF	100
#define IWL_MAX_PLCP_ERR_EXT_LONG_THRESHOLD_DEF	200
#define IWL_MAX_PLCP_ERR_THRESHOLD_MAX		255
#define IWL_MAX_PLCP_ERR_THRESHOLD_DISABLE	0
#define IWL_WATCHDOG_DISABLED	0
#define IWL_DEF_WD_TIMEOUT	2500
#define IWL_LONG_WD_TIMEOUT	10000
#define IWL_MAX_WD_TIMEOUT	120000
#define IWL_DEFAULT_MAX_TX_POWER 22
#define IWL_TX_CSUM_NETIF_FLAGS (NETIF_F_IPV6_CSUM | NETIF_F_IP_CSUM |\
				 NETIF_F_TSO | NETIF_F_TSO6)
#define IWL_CSUM_NETIF_FLAGS_MASK (IWL_TX_CSUM_NETIF_FLAGS | NETIF_F_RXCSUM)
#define	ANT_NONE	0x0
#define	ANT_INVALID	0xff
#define	ANT_A		BIT(0)
#define	ANT_B		BIT(1)
#define ANT_C		BIT(2)
#define	ANT_AB		(ANT_A | ANT_B)
#define	ANT_AC		(ANT_A | ANT_C)
#define ANT_BC		(ANT_B | ANT_C)
#define ANT_ABC		(ANT_A | ANT_B | ANT_C)
static inline u8 num_of_ant(u8 mask)
{
	return  !!((mask) & ANT_A) +
		!!((mask) & ANT_B) +
		!!((mask) & ANT_C);
}
struct iwl_base_params {
	unsigned int wd_timeout;
	u16 eeprom_size;
	u16 max_event_log_size;
	u8 pll_cfg:1,  
	   shadow_ram_support:1,
	   shadow_reg_enable:1,
	   pcie_l1_allowed:1,
	   apmg_wake_up_wa:1,
	   scd_chain_ext_wa:1;
	u16 num_of_queues;	 
	u32 max_tfd_queue_size;	 
	u8 max_ll_items;
	u8 led_compensation;
};
struct iwl_ht_params {
	u8 ht_greenfield_support:1,
	   stbc:1,
	   ldpc:1,
	   use_rts_for_aggregation:1;
	u8 ht40_bands;
};
struct iwl_tt_tx_backoff {
	s32 temperature;
	u32 backoff;
};
#define TT_TX_BACKOFF_SIZE 6
struct iwl_tt_params {
	u32 ct_kill_entry;
	u32 ct_kill_exit;
	u32 ct_kill_duration;
	u32 dynamic_smps_entry;
	u32 dynamic_smps_exit;
	u32 tx_protection_entry;
	u32 tx_protection_exit;
	struct iwl_tt_tx_backoff tx_backoff[TT_TX_BACKOFF_SIZE];
	u8 support_ct_kill:1,
	   support_dynamic_smps:1,
	   support_tx_protection:1,
	   support_tx_backoff:1;
};
#define EEPROM_REG_BAND_1_CHANNELS		0x08
#define EEPROM_REG_BAND_2_CHANNELS		0x26
#define EEPROM_REG_BAND_3_CHANNELS		0x42
#define EEPROM_REG_BAND_4_CHANNELS		0x5C
#define EEPROM_REG_BAND_5_CHANNELS		0x74
#define EEPROM_REG_BAND_24_HT40_CHANNELS	0x82
#define EEPROM_REG_BAND_52_HT40_CHANNELS	0x92
#define EEPROM_6000_REG_BAND_24_HT40_CHANNELS	0x80
#define EEPROM_REGULATORY_BAND_NO_HT40		0
#define OTP_LOW_IMAGE_SIZE_2K		(2 * 512 * sizeof(u16))   
#define OTP_LOW_IMAGE_SIZE_16K		(16 * 512 * sizeof(u16))  
#define OTP_LOW_IMAGE_SIZE_32K		(32 * 512 * sizeof(u16))  
struct iwl_eeprom_params {
	const u8 regulatory_bands[7];
	bool enhanced_txpower;
};
struct iwl_pwr_tx_backoff {
	u32 pwr;
	u32 backoff;
};
enum iwl_cfg_trans_ltr_delay {
	IWL_CFG_TRANS_LTR_DELAY_NONE	= 0,
	IWL_CFG_TRANS_LTR_DELAY_200US	= 1,
	IWL_CFG_TRANS_LTR_DELAY_2500US	= 2,
	IWL_CFG_TRANS_LTR_DELAY_1820US	= 3,
};
struct iwl_cfg_trans_params {
	const struct iwl_base_params *base_params;
	enum iwl_device_family device_family;
	u32 umac_prph_offset;
	u32 xtal_latency;
	u32 extra_phy_cfg_flags;
	u32 rf_id:1,
	    gen2:1,
	    mq_rx_supported:1,
	    integrated:1,
	    low_latency_xtal:1,
	    bisr_workaround:1,
	    ltr_delay:2,
	    imr_enabled:1;
};
struct iwl_fw_mon_reg {
	u32 addr;
	u32 mask;
};
struct iwl_fw_mon_regs {
	struct iwl_fw_mon_reg write_ptr;
	struct iwl_fw_mon_reg cycle_cnt;
	struct iwl_fw_mon_reg cur_frag;
};
struct iwl_cfg {
	struct iwl_cfg_trans_params trans;
	const char *name;
	const char *fw_name_pre;
	const char *fw_name_mac;
	const struct iwl_ht_params *ht_params;
	const struct iwl_eeprom_params *eeprom_params;
	const struct iwl_pwr_tx_backoff *pwr_tx_backoffs;
	const char *default_nvm_file_C_step;
	const struct iwl_tt_params *thermal_params;
	enum iwl_led_mode led_mode;
	enum iwl_nvm_type nvm_type;
	u32 max_data_size;
	u32 max_inst_size;
	netdev_features_t features;
	u32 dccm_offset;
	u32 dccm_len;
	u32 dccm2_offset;
	u32 dccm2_len;
	u32 smem_offset;
	u32 smem_len;
	u16 nvm_ver;
	u16 nvm_calib_ver;
	u32 rx_with_siso_diversity:1,
	    tx_with_siso_diversity:1,
	    bt_shared_single_ant:1,
	    internal_wimax_coex:1,
	    host_interrupt_operation_mode:1,
	    high_temp:1,
	    mac_addr_from_csr:10,
	    lp_xtal_workaround:1,
	    apmg_not_supported:1,
	    vht_mu_mimo_supported:1,
	    cdb:1,
	    dbgc_supported:1,
	    uhb_supported:1;
	u8 valid_tx_ant;
	u8 valid_rx_ant;
	u8 non_shared_ant;
	u8 nvm_hw_section_num;
	u8 max_tx_agg_size;
	u8 ucode_api_max;
	u8 ucode_api_min;
	u16 num_rbds;
	u32 min_umac_error_event_table;
	u32 d3_debug_data_base_addr;
	u32 d3_debug_data_length;
	u32 min_txq_size;
	u32 gp2_reg_addr;
	u32 min_ba_txq_size;
	const struct iwl_fw_mon_regs mon_dram_regs;
	const struct iwl_fw_mon_regs mon_smem_regs;
	const struct iwl_fw_mon_regs mon_dbgi_regs;
};
#define IWL_CFG_ANY (~0)
#define IWL_CFG_MAC_TYPE_PU		0x31
#define IWL_CFG_MAC_TYPE_TH		0x32
#define IWL_CFG_MAC_TYPE_QU		0x33
#define IWL_CFG_MAC_TYPE_QUZ		0x35
#define IWL_CFG_MAC_TYPE_SO		0x37
#define IWL_CFG_MAC_TYPE_SOF		0x43
#define IWL_CFG_MAC_TYPE_MA		0x44
#define IWL_CFG_MAC_TYPE_BZ		0x46
#define IWL_CFG_MAC_TYPE_GL		0x47
#define IWL_CFG_MAC_TYPE_SC		0x48
#define IWL_CFG_RF_TYPE_TH		0x105
#define IWL_CFG_RF_TYPE_TH1		0x108
#define IWL_CFG_RF_TYPE_JF2		0x105
#define IWL_CFG_RF_TYPE_JF1		0x108
#define IWL_CFG_RF_TYPE_HR2		0x10A
#define IWL_CFG_RF_TYPE_HR1		0x10C
#define IWL_CFG_RF_TYPE_GF		0x10D
#define IWL_CFG_RF_TYPE_MR		0x110
#define IWL_CFG_RF_TYPE_MS		0x111
#define IWL_CFG_RF_TYPE_FM		0x112
#define IWL_CFG_RF_TYPE_WH		0x113
#define IWL_CFG_RF_ID_TH		0x1
#define IWL_CFG_RF_ID_TH1		0x1
#define IWL_CFG_RF_ID_JF		0x3
#define IWL_CFG_RF_ID_JF1		0x6
#define IWL_CFG_RF_ID_JF1_DIV		0xA
#define IWL_CFG_RF_ID_HR		0x7
#define IWL_CFG_RF_ID_HR1		0x4
#define IWL_CFG_NO_160			0x1
#define IWL_CFG_160			0x0
#define IWL_CFG_CORES_BT		0x0
#define IWL_CFG_CORES_BT_GNSS		0x5
#define IWL_CFG_NO_CDB			0x0
#define IWL_CFG_CDB			0x1
#define IWL_CFG_NO_JACKET		0x0
#define IWL_CFG_IS_JACKET		0x1
#define IWL_SUBDEVICE_RF_ID(subdevice)	((u16)((subdevice) & 0x00F0) >> 4)
#define IWL_SUBDEVICE_NO_160(subdevice)	((u16)((subdevice) & 0x0200) >> 9)
#define IWL_SUBDEVICE_CORES(subdevice)	((u16)((subdevice) & 0x1C00) >> 10)
struct iwl_dev_info {
	u16 device;
	u16 subdevice;
	u16 mac_type;
	u16 rf_type;
	u8 mac_step;
	u8 rf_step;
	u8 rf_id;
	u8 no_160;
	u8 cores;
	u8 cdb;
	u8 jacket;
	const struct iwl_cfg *cfg;
	const char *name;
};
extern const struct iwl_cfg_trans_params iwl9000_trans_cfg;
extern const struct iwl_cfg_trans_params iwl9560_trans_cfg;
extern const struct iwl_cfg_trans_params iwl9560_long_latency_trans_cfg;
extern const struct iwl_cfg_trans_params iwl9560_shared_clk_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_qu_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_qu_medium_latency_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_qu_long_latency_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_ax200_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_so_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_so_long_latency_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_so_long_latency_imr_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_ma_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_bz_trans_cfg;
extern const struct iwl_cfg_trans_params iwl_sc_trans_cfg;
extern const char iwl9162_name[];
extern const char iwl9260_name[];
extern const char iwl9260_1_name[];
extern const char iwl9270_name[];
extern const char iwl9461_name[];
extern const char iwl9462_name[];
extern const char iwl9560_name[];
extern const char iwl9162_160_name[];
extern const char iwl9260_160_name[];
extern const char iwl9270_160_name[];
extern const char iwl9461_160_name[];
extern const char iwl9462_160_name[];
extern const char iwl9560_160_name[];
extern const char iwl9260_killer_1550_name[];
extern const char iwl9560_killer_1550i_name[];
extern const char iwl9560_killer_1550s_name[];
extern const char iwl_ax200_name[];
extern const char iwl_ax203_name[];
extern const char iwl_ax204_name[];
extern const char iwl_ax201_name[];
extern const char iwl_ax101_name[];
extern const char iwl_ax200_killer_1650w_name[];
extern const char iwl_ax200_killer_1650x_name[];
extern const char iwl_ax201_killer_1650s_name[];
extern const char iwl_ax201_killer_1650i_name[];
extern const char iwl_ax210_killer_1675w_name[];
extern const char iwl_ax210_killer_1675x_name[];
extern const char iwl9560_killer_1550i_160_name[];
extern const char iwl9560_killer_1550s_160_name[];
extern const char iwl_ax211_killer_1675s_name[];
extern const char iwl_ax211_killer_1675i_name[];
extern const char iwl_ax411_killer_1690s_name[];
extern const char iwl_ax411_killer_1690i_name[];
extern const char iwl_ax211_name[];
extern const char iwl_ax221_name[];
extern const char iwl_ax231_name[];
extern const char iwl_ax411_name[];
extern const char iwl_bz_name[];
extern const char iwl_sc_name[];
#if IS_ENABLED(CONFIG_IWLDVM)
extern const struct iwl_cfg iwl5300_agn_cfg;
extern const struct iwl_cfg iwl5100_agn_cfg;
extern const struct iwl_cfg iwl5350_agn_cfg;
extern const struct iwl_cfg iwl5100_bgn_cfg;
extern const struct iwl_cfg iwl5100_abg_cfg;
extern const struct iwl_cfg iwl5150_agn_cfg;
extern const struct iwl_cfg iwl5150_abg_cfg;
extern const struct iwl_cfg iwl6005_2agn_cfg;
extern const struct iwl_cfg iwl6005_2abg_cfg;
extern const struct iwl_cfg iwl6005_2bg_cfg;
extern const struct iwl_cfg iwl6005_2agn_sff_cfg;
extern const struct iwl_cfg iwl6005_2agn_d_cfg;
extern const struct iwl_cfg iwl6005_2agn_mow1_cfg;
extern const struct iwl_cfg iwl6005_2agn_mow2_cfg;
extern const struct iwl_cfg iwl1030_bgn_cfg;
extern const struct iwl_cfg iwl1030_bg_cfg;
extern const struct iwl_cfg iwl6030_2agn_cfg;
extern const struct iwl_cfg iwl6030_2abg_cfg;
extern const struct iwl_cfg iwl6030_2bgn_cfg;
extern const struct iwl_cfg iwl6030_2bg_cfg;
extern const struct iwl_cfg iwl6000i_2agn_cfg;
extern const struct iwl_cfg iwl6000i_2abg_cfg;
extern const struct iwl_cfg iwl6000i_2bg_cfg;
extern const struct iwl_cfg iwl6000_3agn_cfg;
extern const struct iwl_cfg iwl6050_2agn_cfg;
extern const struct iwl_cfg iwl6050_2abg_cfg;
extern const struct iwl_cfg iwl6150_bgn_cfg;
extern const struct iwl_cfg iwl6150_bg_cfg;
extern const struct iwl_cfg iwl1000_bgn_cfg;
extern const struct iwl_cfg iwl1000_bg_cfg;
extern const struct iwl_cfg iwl100_bgn_cfg;
extern const struct iwl_cfg iwl100_bg_cfg;
extern const struct iwl_cfg iwl130_bgn_cfg;
extern const struct iwl_cfg iwl130_bg_cfg;
extern const struct iwl_cfg iwl2000_2bgn_cfg;
extern const struct iwl_cfg iwl2000_2bgn_d_cfg;
extern const struct iwl_cfg iwl2030_2bgn_cfg;
extern const struct iwl_cfg iwl6035_2agn_cfg;
extern const struct iwl_cfg iwl6035_2agn_sff_cfg;
extern const struct iwl_cfg iwl105_bgn_cfg;
extern const struct iwl_cfg iwl105_bgn_d_cfg;
extern const struct iwl_cfg iwl135_bgn_cfg;
#endif  
#if IS_ENABLED(CONFIG_IWLMVM)
extern const struct iwl_ht_params iwl_22000_ht_params;
extern const struct iwl_cfg iwl7260_2ac_cfg;
extern const struct iwl_cfg iwl7260_2ac_cfg_high_temp;
extern const struct iwl_cfg iwl7260_2n_cfg;
extern const struct iwl_cfg iwl7260_n_cfg;
extern const struct iwl_cfg iwl3160_2ac_cfg;
extern const struct iwl_cfg iwl3160_2n_cfg;
extern const struct iwl_cfg iwl3160_n_cfg;
extern const struct iwl_cfg iwl3165_2ac_cfg;
extern const struct iwl_cfg iwl3168_2ac_cfg;
extern const struct iwl_cfg iwl7265_2ac_cfg;
extern const struct iwl_cfg iwl7265_2n_cfg;
extern const struct iwl_cfg iwl7265_n_cfg;
extern const struct iwl_cfg iwl7265d_2ac_cfg;
extern const struct iwl_cfg iwl7265d_2n_cfg;
extern const struct iwl_cfg iwl7265d_n_cfg;
extern const struct iwl_cfg iwl8260_2n_cfg;
extern const struct iwl_cfg iwl8260_2ac_cfg;
extern const struct iwl_cfg iwl8265_2ac_cfg;
extern const struct iwl_cfg iwl8275_2ac_cfg;
extern const struct iwl_cfg iwl4165_2ac_cfg;
extern const struct iwl_cfg iwl9260_2ac_cfg;
extern const struct iwl_cfg iwl9560_qu_b0_jf_b0_cfg;
extern const struct iwl_cfg iwl9560_qu_c0_jf_b0_cfg;
extern const struct iwl_cfg iwl9560_quz_a0_jf_b0_cfg;
extern const struct iwl_cfg iwl9560_2ac_cfg_soc;
extern const struct iwl_cfg iwl_qu_b0_hr1_b0;
extern const struct iwl_cfg iwl_qu_c0_hr1_b0;
extern const struct iwl_cfg iwl_quz_a0_hr1_b0;
extern const struct iwl_cfg iwl_qu_b0_hr_b0;
extern const struct iwl_cfg iwl_qu_c0_hr_b0;
extern const struct iwl_cfg iwl_ax200_cfg_cc;
extern const struct iwl_cfg iwl_ax201_cfg_qu_hr;
extern const struct iwl_cfg iwl_ax201_cfg_qu_c0_hr_b0;
extern const struct iwl_cfg iwl_ax201_cfg_quz_hr;
extern const struct iwl_cfg iwl_ax1650i_cfg_quz_hr;
extern const struct iwl_cfg iwl_ax1650s_cfg_quz_hr;
extern const struct iwl_cfg killer1650s_2ax_cfg_qu_b0_hr_b0;
extern const struct iwl_cfg killer1650i_2ax_cfg_qu_b0_hr_b0;
extern const struct iwl_cfg killer1650s_2ax_cfg_qu_c0_hr_b0;
extern const struct iwl_cfg killer1650i_2ax_cfg_qu_c0_hr_b0;
extern const struct iwl_cfg killer1650x_2ax_cfg;
extern const struct iwl_cfg killer1650w_2ax_cfg;
extern const struct iwl_cfg iwlax210_2ax_cfg_so_jf_b0;
extern const struct iwl_cfg iwlax211_2ax_cfg_so_gf_a0;
extern const struct iwl_cfg iwlax211_2ax_cfg_so_gf_a0_long;
extern const struct iwl_cfg iwlax210_2ax_cfg_ty_gf_a0;
extern const struct iwl_cfg iwlax411_2ax_cfg_so_gf4_a0;
extern const struct iwl_cfg iwlax411_2ax_cfg_so_gf4_a0_long;
extern const struct iwl_cfg iwl_cfg_ma;
extern const struct iwl_cfg iwl_cfg_so_a0_hr_a0;
extern const struct iwl_cfg iwl_cfg_so_a0_ms_a0;
extern const struct iwl_cfg iwl_cfg_quz_a0_hr_b0;
extern const struct iwl_cfg iwl_cfg_bz;
extern const struct iwl_cfg iwl_cfg_gl;
extern const struct iwl_cfg iwl_cfg_sc;
#endif  
#endif  
