 
 
#ifndef __iwl_context_info_file_gen3_h__
#define __iwl_context_info_file_gen3_h__

#include "iwl-context-info.h"

#define CSR_CTXT_INFO_BOOT_CTRL         0x0
#define CSR_CTXT_INFO_ADDR              0x118
#define CSR_IML_DATA_ADDR               0x120
#define CSR_IML_SIZE_ADDR               0x128
#define CSR_IML_RESP_ADDR               0x12c

#define UNFRAGMENTED_PNVM_PAYLOADS_NUMBER 2

 
#define CSR_AUTO_FUNC_BOOT_ENA          BIT(1)
 
#define CSR_AUTO_FUNC_INIT              BIT(7)

 
enum iwl_prph_scratch_mtr_format {
	IWL_PRPH_MTR_FORMAT_16B = 0x0,
	IWL_PRPH_MTR_FORMAT_32B = 0x40000,
	IWL_PRPH_MTR_FORMAT_64B = 0x80000,
	IWL_PRPH_MTR_FORMAT_256B = 0xC0000,
};

 
enum iwl_prph_scratch_flags {
	IWL_PRPH_SCRATCH_IMR_DEBUG_EN		= BIT(1),
	IWL_PRPH_SCRATCH_EARLY_DEBUG_EN		= BIT(4),
	IWL_PRPH_SCRATCH_EDBG_DEST_DRAM		= BIT(8),
	IWL_PRPH_SCRATCH_EDBG_DEST_INTERNAL	= BIT(9),
	IWL_PRPH_SCRATCH_EDBG_DEST_ST_ARBITER	= BIT(10),
	IWL_PRPH_SCRATCH_EDBG_DEST_TB22DTF	= BIT(11),
	IWL_PRPH_SCRATCH_RB_SIZE_4K		= BIT(16),
	IWL_PRPH_SCRATCH_MTR_MODE		= BIT(17),
	IWL_PRPH_SCRATCH_MTR_FORMAT		= BIT(18) | BIT(19),
	IWL_PRPH_SCRATCH_RB_SIZE_EXT_MASK	= 0xf << 20,
	IWL_PRPH_SCRATCH_RB_SIZE_EXT_8K		= 8 << 20,
	IWL_PRPH_SCRATCH_RB_SIZE_EXT_12K	= 9 << 20,
	IWL_PRPH_SCRATCH_RB_SIZE_EXT_16K	= 10 << 20,
};

 
struct iwl_prph_scratch_version {
	__le16 mac_id;
	__le16 version;
	__le16 size;
	__le16 reserved;
} __packed;  

 
struct iwl_prph_scratch_control {
	__le32 control_flags;
	__le32 reserved;
} __packed;  

 
struct iwl_prph_scratch_pnvm_cfg {
	__le64 pnvm_base_addr;
	__le32 pnvm_size;
	__le32 reserved;
} __packed;  

 
struct iwl_prph_scrath_mem_desc_addr_array {
	__le64 mem_descs[IPC_DRAM_MAP_ENTRY_NUM_MAX];
} __packed;  
 
struct iwl_prph_scratch_hwm_cfg {
	__le64 hwm_base_addr;
	__le32 hwm_size;
	__le32 debug_token_config;
} __packed;  

 
struct iwl_prph_scratch_rbd_cfg {
	__le64 free_rbd_addr;
	__le32 reserved;
} __packed;  

 
struct iwl_prph_scratch_uefi_cfg {
	__le64 base_addr;
	__le32 size;
	__le32 reserved;
} __packed;  

 

struct iwl_prph_scratch_step_cfg {
	__le32 mbx_addr_0;
	__le32 mbx_addr_1;
} __packed;

 
struct iwl_prph_scratch_ctrl_cfg {
	struct iwl_prph_scratch_version version;
	struct iwl_prph_scratch_control control;
	struct iwl_prph_scratch_pnvm_cfg pnvm_cfg;
	struct iwl_prph_scratch_hwm_cfg hwm_cfg;
	struct iwl_prph_scratch_rbd_cfg rbd_cfg;
	struct iwl_prph_scratch_uefi_cfg reduce_power_cfg;
	struct iwl_prph_scratch_step_cfg step_cfg;
} __packed;  

 
struct iwl_prph_scratch {
	struct iwl_prph_scratch_ctrl_cfg ctrl_cfg;
	__le32 reserved[10];
	struct iwl_context_info_dram dram;
} __packed;  

 
struct iwl_prph_info {
	__le32 boot_stage_mirror;
	__le32 ipc_status_mirror;
	__le32 sleep_notif;
	__le32 reserved;
} __packed;  

 
struct iwl_context_info_gen3 {
	__le16 version;
	__le16 size;
	__le32 config;
	__le64 prph_info_base_addr;
	__le64 cr_head_idx_arr_base_addr;
	__le64 tr_tail_idx_arr_base_addr;
	__le64 cr_tail_idx_arr_base_addr;
	__le64 tr_head_idx_arr_base_addr;
	__le16 cr_idx_arr_size;
	__le16 tr_idx_arr_size;
	__le64 mtr_base_addr;
	__le64 mcr_base_addr;
	__le16 mtr_size;
	__le16 mcr_size;
	__le16 mtr_doorbell_vec;
	__le16 mcr_doorbell_vec;
	__le16 mtr_msi_vec;
	__le16 mcr_msi_vec;
	u8 mtr_opt_header_size;
	u8 mtr_opt_footer_size;
	u8 mcr_opt_header_size;
	u8 mcr_opt_footer_size;
	__le16 msg_rings_ctrl_flags;
	__le16 prph_info_msi_vec;
	__le64 prph_scratch_base_addr;
	__le32 prph_scratch_size;
	__le32 reserved;
} __packed;  

int iwl_pcie_ctxt_info_gen3_init(struct iwl_trans *trans,
				 const struct fw_img *fw);
void iwl_pcie_ctxt_info_gen3_free(struct iwl_trans *trans, bool alive);

int iwl_trans_pcie_ctx_info_gen3_load_pnvm(struct iwl_trans *trans,
					   const struct iwl_pnvm_image *pnvm_payloads,
					   const struct iwl_ucode_capabilities *capa);
void iwl_trans_pcie_ctx_info_gen3_set_pnvm(struct iwl_trans *trans,
					   const struct iwl_ucode_capabilities *capa);
int
iwl_trans_pcie_ctx_info_gen3_load_reduce_power(struct iwl_trans *trans,
					       const struct iwl_pnvm_image *payloads,
					       const struct iwl_ucode_capabilities *capa);
void
iwl_trans_pcie_ctx_info_gen3_set_reduce_power(struct iwl_trans *trans,
					      const struct iwl_ucode_capabilities *capa);
int iwl_trans_pcie_ctx_info_gen3_set_step(struct iwl_trans *trans,
					  u32 mbx_addr_0_step, u32 mbx_addr_1_step);
#endif  
