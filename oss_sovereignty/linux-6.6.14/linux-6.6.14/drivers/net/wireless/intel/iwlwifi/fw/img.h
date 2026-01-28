#ifndef __iwl_fw_img_h__
#define __iwl_fw_img_h__
#include <linux/types.h>
#include "api/dbg-tlv.h"
#include "file.h"
#include "error-dump.h"
enum iwl_ucode_type {
	IWL_UCODE_REGULAR,
	IWL_UCODE_INIT,
	IWL_UCODE_WOWLAN,
	IWL_UCODE_REGULAR_USNIFFER,
	IWL_UCODE_TYPE_MAX,
};
enum iwl_ucode_sec {
	IWL_UCODE_SECTION_DATA,
	IWL_UCODE_SECTION_INST,
};
struct iwl_ucode_capabilities {
	u32 max_probe_length;
	u32 n_scan_channels;
	u32 standard_phy_calibration_size;
	u32 flags;
	u32 error_log_addr;
	u32 error_log_size;
	u32 num_stations;
	u32 num_beacons;
	unsigned long _api[BITS_TO_LONGS(NUM_IWL_UCODE_TLV_API)];
	unsigned long _capa[BITS_TO_LONGS(NUM_IWL_UCODE_TLV_CAPA)];
	const struct iwl_fw_cmd_version *cmd_versions;
	u32 n_cmd_versions;
};
static inline bool
fw_has_api(const struct iwl_ucode_capabilities *capabilities,
	   iwl_ucode_tlv_api_t api)
{
	return test_bit((__force long)api, capabilities->_api);
}
static inline bool
fw_has_capa(const struct iwl_ucode_capabilities *capabilities,
	    iwl_ucode_tlv_capa_t capa)
{
	return test_bit((__force long)capa, capabilities->_capa);
}
struct fw_desc {
	const void *data;	 
	u32 len;		 
	u32 offset;		 
};
struct fw_img {
	struct fw_desc *sec;
	int num_sec;
	bool is_dual_cpus;
	u32 paging_mem_size;
};
#define PAGE_2_EXP_SIZE 12  
#define FW_PAGING_SIZE BIT(PAGE_2_EXP_SIZE)  
#define PAGE_PER_GROUP_2_EXP_SIZE 3
#define NUM_OF_PAGE_PER_GROUP BIT(PAGE_PER_GROUP_2_EXP_SIZE)
#define PAGING_BLOCK_SIZE (NUM_OF_PAGE_PER_GROUP * FW_PAGING_SIZE)
#define BLOCK_2_EXP_SIZE (PAGE_2_EXP_SIZE + PAGE_PER_GROUP_2_EXP_SIZE)
#define BLOCK_PER_IMAGE_2_EXP_SIZE 5
#define NUM_OF_BLOCK_PER_IMAGE BIT(BLOCK_PER_IMAGE_2_EXP_SIZE)
#define MAX_PAGING_IMAGE_SIZE (NUM_OF_BLOCK_PER_IMAGE * PAGING_BLOCK_SIZE)
#define PAGING_ADDR_SIG 0xAA000000
#define PAGING_CMD_IS_SECURED BIT(9)
#define PAGING_CMD_IS_ENABLED BIT(8)
#define PAGING_CMD_NUM_OF_PAGES_IN_LAST_GRP_POS	0
#define PAGING_TLV_SECURE_MASK 1
#define FW_ADDR_CACHE_CONTROL 0xC0000000UL
struct iwl_fw_paging {
	dma_addr_t fw_paging_phys;
	struct page *fw_paging_block;
	u32 fw_paging_size;
	u32 fw_offs;
};
enum iwl_fw_type {
	IWL_FW_DVM,
	IWL_FW_MVM,
};
struct iwl_fw_dbg {
	struct iwl_fw_dbg_dest_tlv_v1 *dest_tlv;
	u8 n_dest_reg;
	struct iwl_fw_dbg_conf_tlv *conf_tlv[FW_DBG_CONF_MAX];
	struct iwl_fw_dbg_trigger_tlv *trigger_tlv[FW_DBG_TRIGGER_MAX];
	size_t trigger_tlv_len[FW_DBG_TRIGGER_MAX];
	struct iwl_fw_dbg_mem_seg_tlv *mem_tlv;
	size_t n_mem_tlv;
	u32 dump_mask;
};
struct iwl_dump_exclude {
	u32 addr, size;
};
struct iwl_fw {
	u32 ucode_ver;
	char fw_version[64];
	struct fw_img img[IWL_UCODE_TYPE_MAX];
	size_t iml_len;
	u8 *iml;
	struct iwl_ucode_capabilities ucode_capa;
	bool enhance_sensitivity_table;
	u32 init_evtlog_ptr, init_evtlog_size, init_errlog_ptr;
	u32 inst_evtlog_ptr, inst_evtlog_size, inst_errlog_ptr;
	struct iwl_tlv_calib_ctrl default_calib[IWL_UCODE_TYPE_MAX];
	u32 phy_config;
	u8 valid_tx_ant;
	u8 valid_rx_ant;
	enum iwl_fw_type type;
	u8 human_readable[FW_VER_HUMAN_READABLE_SZ];
	struct iwl_fw_dbg dbg;
	u8 *phy_integration_ver;
	u32 phy_integration_ver_len;
	struct iwl_dump_exclude dump_excl[2], dump_excl_wowlan[2];
};
static inline const char *get_fw_dbg_mode_string(int mode)
{
	switch (mode) {
	case SMEM_MODE:
		return "SMEM";
	case EXTERNAL_MODE:
		return "EXTERNAL_DRAM";
	case MARBH_MODE:
		return "MARBH";
	case MIPI_MODE:
		return "MIPI";
	default:
		return "UNKNOWN";
	}
}
static inline bool
iwl_fw_dbg_conf_usniffer(const struct iwl_fw *fw, u8 id)
{
	const struct iwl_fw_dbg_conf_tlv *conf_tlv = fw->dbg.conf_tlv[id];
	if (!conf_tlv)
		return false;
	return conf_tlv->usniffer;
}
static inline const struct fw_img *
iwl_get_ucode_image(const struct iwl_fw *fw, enum iwl_ucode_type ucode_type)
{
	if (ucode_type >= IWL_UCODE_TYPE_MAX)
		return NULL;
	return &fw->img[ucode_type];
}
u8 iwl_fw_lookup_cmd_ver(const struct iwl_fw *fw, u32 cmd_id, u8 def);
u8 iwl_fw_lookup_notif_ver(const struct iwl_fw *fw, u8 grp, u8 cmd, u8 def);
const char *iwl_fw_lookup_assert_desc(u32 num);
#define FW_SYSASSERT_CPU_MASK		0xf0000000
#define FW_SYSASSERT_PNVM_MISSING	0x0010070d
#endif   
