 
 
#ifndef __iwl_context_info_file_h__
#define __iwl_context_info_file_h__

 
#define IWL_MAX_DRAM_ENTRY	64
#define CSR_CTXT_INFO_BA	0x40

 
enum iwl_context_info_flags {
	IWL_CTXT_INFO_AUTO_FUNC_INIT	= 0x0001,
	IWL_CTXT_INFO_EARLY_DEBUG	= 0x0002,
	IWL_CTXT_INFO_ENABLE_CDMP	= 0x0004,
	IWL_CTXT_INFO_RB_CB_SIZE	= 0x00f0,
	IWL_CTXT_INFO_TFD_FORMAT_LONG	= 0x0100,
	IWL_CTXT_INFO_RB_SIZE		= 0x1e00,
	IWL_CTXT_INFO_RB_SIZE_1K	= 0x1,
	IWL_CTXT_INFO_RB_SIZE_2K	= 0x2,
	IWL_CTXT_INFO_RB_SIZE_4K	= 0x4,
	IWL_CTXT_INFO_RB_SIZE_8K	= 0x8,
	IWL_CTXT_INFO_RB_SIZE_12K	= 0x9,
	IWL_CTXT_INFO_RB_SIZE_16K	= 0xa,
	IWL_CTXT_INFO_RB_SIZE_20K	= 0xb,
	IWL_CTXT_INFO_RB_SIZE_24K	= 0xc,
	IWL_CTXT_INFO_RB_SIZE_28K	= 0xd,
	IWL_CTXT_INFO_RB_SIZE_32K	= 0xe,
};

 
struct iwl_context_info_version {
	__le16 mac_id;
	__le16 version;
	__le16 size;
	__le16 reserved;
} __packed;

 
struct iwl_context_info_control {
	__le32 control_flags;
	__le32 reserved;
} __packed;

 
struct iwl_context_info_dram {
	__le64 umac_img[IWL_MAX_DRAM_ENTRY];
	__le64 lmac_img[IWL_MAX_DRAM_ENTRY];
	__le64 virtual_img[IWL_MAX_DRAM_ENTRY];
} __packed;

 
struct iwl_context_info_rbd_cfg {
	__le64 free_rbd_addr;
	__le64 used_rbd_addr;
	__le64 status_wr_ptr;
} __packed;

 
struct iwl_context_info_hcmd_cfg {
	__le64 cmd_queue_addr;
	u8 cmd_queue_size;
	u8 reserved[7];
} __packed;

 
struct iwl_context_info_dump_cfg {
	__le64 core_dump_addr;
	__le32 core_dump_size;
	__le32 reserved;
} __packed;

 
struct iwl_context_info_pnvm_cfg {
	__le64 platform_nvm_addr;
	__le32 platform_nvm_size;
	__le32 reserved;
} __packed;

 
struct iwl_context_info_early_dbg_cfg {
	__le64 early_debug_addr;
	__le32 early_debug_size;
	__le32 reserved;
} __packed;

 
struct iwl_context_info {
	struct iwl_context_info_version version;
	struct iwl_context_info_control control;
	__le64 reserved0;
	struct iwl_context_info_rbd_cfg rbd_cfg;
	struct iwl_context_info_hcmd_cfg hcmd_cfg;
	__le32 reserved1[4];
	struct iwl_context_info_dump_cfg dump_cfg;
	struct iwl_context_info_early_dbg_cfg edbg_cfg;
	struct iwl_context_info_pnvm_cfg pnvm_cfg;
	__le32 reserved2[16];
	struct iwl_context_info_dram dram;
	__le32 reserved3[16];
} __packed;

int iwl_pcie_ctxt_info_init(struct iwl_trans *trans, const struct fw_img *fw);
void iwl_pcie_ctxt_info_free(struct iwl_trans *trans);
void iwl_pcie_ctxt_info_free_paging(struct iwl_trans *trans);
int iwl_pcie_init_fw_sec(struct iwl_trans *trans,
			 const struct fw_img *fw,
			 struct iwl_context_info_dram *ctxt_dram);
void *iwl_pcie_ctxt_info_dma_alloc_coherent(struct iwl_trans *trans,
					    size_t size,
					    dma_addr_t *phys);
int iwl_pcie_ctxt_info_alloc_dma(struct iwl_trans *trans,
				 const void *data, u32 len,
				 struct iwl_dram_data *dram);

#endif  
