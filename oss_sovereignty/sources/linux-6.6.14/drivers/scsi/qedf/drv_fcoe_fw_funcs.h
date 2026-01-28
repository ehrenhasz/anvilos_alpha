

#ifndef _FCOE_FW_FUNCS_H
#define _FCOE_FW_FUNCS_H
#include "drv_scsi_fw_funcs.h"
#include "qedf_hsi.h"
#include <linux/qed/qed_if.h>

struct fcoe_task_params {
	
	struct fcoe_task_context *context;

	
	struct fcoe_wqe *sqe;
	enum fcoe_task_type task_type;
	u32 tx_io_size; 
	u32 rx_io_size; 
	u32 conn_cid;
	u16 itid;
	u8 cq_rss_number;

	 
	u8 is_tape_device;
};


int init_initiator_rw_fcoe_task(struct fcoe_task_params *task_params,
	struct scsi_sgl_task_params *sgl_task_params,
	struct regpair sense_data_buffer_phys_addr,
	u32 task_retry_id,
	u8 fcp_cmd_payload[32]);


int init_initiator_midpath_unsolicited_fcoe_task(
	struct fcoe_task_params *task_params,
	struct fcoe_tx_mid_path_params *mid_path_fc_header,
	struct scsi_sgl_task_params *tx_sgl_task_params,
	struct scsi_sgl_task_params *rx_sgl_task_params,
	u8 fw_to_place_fc_header);


int init_initiator_abort_fcoe_task(struct fcoe_task_params *task_params);


int init_initiator_cleanup_fcoe_task(struct fcoe_task_params *task_params);


int init_initiator_sequence_recovery_fcoe_task(
	struct fcoe_task_params *task_params,
	u32 desired_offset);
#endif
