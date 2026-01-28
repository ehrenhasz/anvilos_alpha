

#ifndef _SCSI_FW_FUNCS_H
#define _SCSI_FW_FUNCS_H
#include <linux/qed/common_hsi.h>
#include <linux/qed/storage_common.h>
#include <linux/qed/fcoe_common.h>

struct scsi_sgl_task_params {
	struct scsi_sge *sgl;
	struct regpair sgl_phys_addr;
	u32 total_buffer_size;
	u16 num_sges;

	 
	bool small_mid_sge;
};

struct scsi_dif_task_params {
	u32 initial_ref_tag;
	bool initial_ref_tag_is_valid;
	u16 application_tag;
	u16 application_tag_mask;
	u16 dif_block_size_log;
	bool dif_on_network;
	bool dif_on_host;
	u8 host_guard_type;
	u8 protection_type;
	u8 ref_tag_mask;
	bool crc_seed;

	 
	bool tx_dif_conn_err_en;
	bool ignore_app_tag;
	bool keep_ref_tag_const;
	bool validate_guard;
	bool validate_app_tag;
	bool validate_ref_tag;
	bool forward_guard;
	bool forward_app_tag;
	bool forward_ref_tag;
	bool forward_app_tag_with_mask;
	bool forward_ref_tag_with_mask;
};

struct scsi_initiator_cmd_params {
	 
	struct scsi_sge extended_cdb_sge;

	
	struct regpair sense_data_buffer_phys_addr;
};


bool scsi_is_slow_sgl(u16 num_sges, bool small_mid_sge);


void init_scsi_sgl_context(struct scsi_sgl_params *sgl_params,
	struct scsi_cached_sges *ctx_data_desc,
	struct scsi_sgl_task_params *sgl_task_params);
#endif
