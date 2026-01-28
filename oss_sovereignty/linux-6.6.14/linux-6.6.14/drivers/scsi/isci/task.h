#ifndef _ISCI_TASK_H_
#define _ISCI_TASK_H_
#include <scsi/sas_ata.h>
#include "host.h"
#define ISCI_TERMINATION_TIMEOUT_MSEC 500
struct isci_request;
enum isci_tmf_function_codes {
	isci_tmf_func_none      = 0,
	isci_tmf_ssp_task_abort = TMF_ABORT_TASK,
	isci_tmf_ssp_lun_reset  = TMF_LU_RESET,
};
struct isci_tmf {
	struct completion *complete;
	enum sas_protocol proto;
	union {
		struct ssp_response_iu resp_iu;
		struct dev_to_host_fis d2h_fis;
		u8 rsp_buf[SSP_RESP_IU_MAX_SIZE];
	} resp;
	unsigned char lun[8];
	u16 io_tag;
	enum isci_tmf_function_codes tmf_code;
	int status;
};
static inline void isci_print_tmf(struct isci_host *ihost, struct isci_tmf *tmf)
{
	if (SAS_PROTOCOL_SATA == tmf->proto)
		dev_dbg(&ihost->pdev->dev,
			"%s: status = %x\n"
			"tmf->resp.d2h_fis.status = %x\n"
			"tmf->resp.d2h_fis.error = %x\n",
			__func__,
			tmf->status,
			tmf->resp.d2h_fis.status,
			tmf->resp.d2h_fis.error);
	else
		dev_dbg(&ihost->pdev->dev,
			"%s: status = %x\n"
			"tmf->resp.resp_iu.data_present = %x\n"
			"tmf->resp.resp_iu.status = %x\n"
			"tmf->resp.resp_iu.data_length = %x\n"
			"tmf->resp.resp_iu.data[0] = %x\n"
			"tmf->resp.resp_iu.data[1] = %x\n"
			"tmf->resp.resp_iu.data[2] = %x\n"
			"tmf->resp.resp_iu.data[3] = %x\n",
			__func__,
			tmf->status,
			tmf->resp.resp_iu.datapres,
			tmf->resp.resp_iu.status,
			be32_to_cpu(tmf->resp.resp_iu.response_data_len),
			tmf->resp.resp_iu.resp_data[0],
			tmf->resp.resp_iu.resp_data[1],
			tmf->resp.resp_iu.resp_data[2],
			tmf->resp.resp_iu.resp_data[3]);
}
int isci_task_execute_task(
	struct sas_task *task,
	gfp_t gfp_flags);
int isci_task_abort_task(
	struct sas_task *task);
int isci_task_abort_task_set(
	struct domain_device *d_device,
	u8 *lun);
int isci_task_clear_task_set(
	struct domain_device *d_device,
	u8 *lun);
int isci_task_query_task(
	struct sas_task *task);
int isci_task_lu_reset(
	struct domain_device *d_device,
	u8 *lun);
int isci_task_clear_nexus_port(
	struct asd_sas_port *port);
int isci_task_clear_nexus_ha(
	struct sas_ha_struct *ha);
int isci_task_I_T_nexus_reset(
	struct domain_device *d_device);
void isci_task_request_complete(
	struct isci_host *isci_host,
	struct isci_request *request,
	enum sci_task_status completion_status);
u16 isci_task_ssp_request_get_io_tag_to_manage(
	struct isci_request *request);
u8 isci_task_ssp_request_get_function(
	struct isci_request *request);
void *isci_task_ssp_request_get_response_data_address(
	struct isci_request *request);
u32 isci_task_ssp_request_get_response_data_length(
	struct isci_request *request);
#endif  
