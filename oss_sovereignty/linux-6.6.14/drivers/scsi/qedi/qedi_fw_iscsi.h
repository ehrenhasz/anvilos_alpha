#ifndef _QEDI_FW_ISCSI_H_
#define _QEDI_FW_ISCSI_H_
#include "qedi_fw_scsi.h"
struct iscsi_task_params {
	struct iscsi_task_context *context;
	struct iscsi_wqe	  *sqe;
	u32			  tx_io_size;
	u32			  rx_io_size;
	u16			  conn_icid;
	u16			  itid;
	u8			  cq_rss_number;
};
struct iscsi_conn_params {
	u32	first_burst_length;
	u32	max_send_pdu_length;
	u32	max_burst_length;
	bool	initial_r2t;
	bool	immediate_data;
};
int init_initiator_rw_iscsi_task(struct iscsi_task_params *task_params,
				 struct iscsi_conn_params *conn_params,
				 struct scsi_initiator_cmd_params *cmd_params,
				 struct iscsi_cmd_hdr *cmd_pdu_header,
				 struct scsi_sgl_task_params *tx_sgl_params,
				 struct scsi_sgl_task_params *rx_sgl_params,
				 struct scsi_dif_task_params *dif_task_params);
int init_initiator_login_request_task(struct iscsi_task_params *task_params,
				      struct iscsi_login_req_hdr *login_header,
				      struct scsi_sgl_task_params *tx_params,
				      struct scsi_sgl_task_params *rx_params);
int init_initiator_nop_out_task(struct iscsi_task_params *task_params,
				struct iscsi_nop_out_hdr *nop_out_pdu_header,
				struct scsi_sgl_task_params *tx_sgl_params,
				struct scsi_sgl_task_params *rx_sgl_params);
int init_initiator_logout_request_task(struct iscsi_task_params *task_params,
				       struct iscsi_logout_req_hdr *logout_hdr,
				       struct scsi_sgl_task_params *tx_params,
				       struct scsi_sgl_task_params *rx_params);
int init_initiator_tmf_request_task(struct iscsi_task_params *task_params,
				    struct iscsi_tmf_request_hdr *tmf_header);
int init_initiator_text_request_task(struct iscsi_task_params *task_params,
				     struct iscsi_text_request_hdr *text_header,
				     struct scsi_sgl_task_params *tx_params,
				     struct scsi_sgl_task_params *rx_params);
int init_cleanup_task(struct iscsi_task_params *task_params);
#endif
