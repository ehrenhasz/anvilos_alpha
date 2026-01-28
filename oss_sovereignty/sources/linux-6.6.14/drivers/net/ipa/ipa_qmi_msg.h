


#ifndef _IPA_QMI_MSG_H_
#define _IPA_QMI_MSG_H_



#include <linux/types.h>
#include <linux/soc/qcom/qmi.h>


#define IPA_QMI_INDICATION_REGISTER	0x20	
#define IPA_QMI_INIT_DRIVER		0x21	
#define IPA_QMI_INIT_COMPLETE		0x22	
#define IPA_QMI_DRIVER_INIT_COMPLETE	0x35	


#define IPA_QMI_INDICATION_REGISTER_REQ_SZ	20	
#define IPA_QMI_INDICATION_REGISTER_RSP_SZ	7	
#define IPA_QMI_INIT_DRIVER_REQ_SZ		162	
#define IPA_QMI_INIT_DRIVER_RSP_SZ		25	
#define IPA_QMI_INIT_COMPLETE_IND_SZ		7	
#define IPA_QMI_DRIVER_INIT_COMPLETE_REQ_SZ	4	
#define IPA_QMI_DRIVER_INIT_COMPLETE_RSP_SZ	7	


#define IPA_QMI_SERVER_MAX_RCV_SZ		8
#define IPA_QMI_CLIENT_MAX_RCV_SZ		25


struct ipa_indication_register_req {
	u8 master_driver_init_complete_valid;
	u8 master_driver_init_complete;
	u8 data_usage_quota_reached_valid;
	u8 data_usage_quota_reached;
	u8 ipa_mhi_ready_ind_valid;
	u8 ipa_mhi_ready_ind;
	u8 endpoint_desc_ind_valid;
	u8 endpoint_desc_ind;
	u8 bw_change_ind_valid;
	u8 bw_change_ind;
};


struct ipa_indication_register_rsp {
	struct qmi_response_type_v01 rsp;
};


struct ipa_driver_init_complete_req {
	u8 status;
};


struct ipa_driver_init_complete_rsp {
	struct qmi_response_type_v01 rsp;
};


struct ipa_init_complete_ind {
	struct qmi_response_type_v01 status;
};


enum ipa_platform_type {
	IPA_QMI_PLATFORM_TYPE_INVALID		= 0x0,	
	IPA_QMI_PLATFORM_TYPE_TN		= 0x1,	
	IPA_QMI_PLATFORM_TYPE_LE		= 0x2,	
	IPA_QMI_PLATFORM_TYPE_MSM_ANDROID	= 0x3,	
	IPA_QMI_PLATFORM_TYPE_MSM_WINDOWS	= 0x4,	
	IPA_QMI_PLATFORM_TYPE_MSM_QNX_V01	= 0x5,	
};


struct ipa_mem_bounds {
	u32 start;
	u32 end;
};


struct ipa_mem_array {
	u32 start;
	u32 count;
};


struct ipa_mem_range {
	u32 start;
	u32 size;
};


struct ipa_init_modem_driver_req {
	u8			platform_type_valid;
	u32			platform_type;	

	
	u8			hdr_tbl_info_valid;
	struct ipa_mem_bounds	hdr_tbl_info;

	
	u8			v4_route_tbl_info_valid;
	struct ipa_mem_bounds	v4_route_tbl_info;
	u8			v6_route_tbl_info_valid;
	struct ipa_mem_bounds	v6_route_tbl_info;

	
	u8			v4_filter_tbl_start_valid;
	u32			v4_filter_tbl_start;
	u8			v6_filter_tbl_start_valid;
	u32			v6_filter_tbl_start;

	
	u8			modem_mem_info_valid;
	struct ipa_mem_range	modem_mem_info;

	
	u8			ctrl_comm_dest_end_pt_valid;
	u32			ctrl_comm_dest_end_pt;

	
	u8			skip_uc_load_valid;
	u8			skip_uc_load;

	
	u8			hdr_proc_ctx_tbl_info_valid;
	struct ipa_mem_bounds	hdr_proc_ctx_tbl_info;

	
	u8			zip_tbl_info_valid;
	struct ipa_mem_bounds	zip_tbl_info;

	
	u8			v4_hash_route_tbl_info_valid;
	struct ipa_mem_bounds	v4_hash_route_tbl_info;
	u8			v6_hash_route_tbl_info_valid;
	struct ipa_mem_bounds	v6_hash_route_tbl_info;

	
	u8			v4_hash_filter_tbl_start_valid;
	u32			v4_hash_filter_tbl_start;
	u8			v6_hash_filter_tbl_start_valid;
	u32			v6_hash_filter_tbl_start;

	
	u8			hw_stats_quota_base_addr_valid;
	u32			hw_stats_quota_base_addr;
	u8			hw_stats_quota_size_valid;
	u32			hw_stats_quota_size;
	u8			hw_stats_drop_base_addr_valid;
	u32			hw_stats_drop_base_addr;
	u8			hw_stats_drop_size_valid;
	u32			hw_stats_drop_size;
};


struct ipa_init_modem_driver_rsp {
	struct qmi_response_type_v01	rsp;

	
	u8				ctrl_comm_dest_end_pt_valid;
	u32				ctrl_comm_dest_end_pt;

	
	u8				default_end_pt_valid;
	u32				default_end_pt;

	
	u8				modem_driver_init_pending_valid;
	u8				modem_driver_init_pending;
};


extern const struct qmi_elem_info ipa_indication_register_req_ei[];
extern const struct qmi_elem_info ipa_indication_register_rsp_ei[];
extern const struct qmi_elem_info ipa_driver_init_complete_req_ei[];
extern const struct qmi_elem_info ipa_driver_init_complete_rsp_ei[];
extern const struct qmi_elem_info ipa_init_complete_ind_ei[];
extern const struct qmi_elem_info ipa_mem_bounds_ei[];
extern const struct qmi_elem_info ipa_mem_array_ei[];
extern const struct qmi_elem_info ipa_mem_range_ei[];
extern const struct qmi_elem_info ipa_init_modem_driver_req_ei[];
extern const struct qmi_elem_info ipa_init_modem_driver_rsp_ei[];

#endif 
