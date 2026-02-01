

 

#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/qrtr.h>
#include <linux/soc/qcom/qmi.h>

#include "ipa.h"
#include "ipa_endpoint.h"
#include "ipa_mem.h"
#include "ipa_table.h"
#include "ipa_modem.h"
#include "ipa_qmi_msg.h"

 

#define IPA_HOST_SERVICE_SVC_ID		0x31
#define IPA_HOST_SVC_VERS		1
#define IPA_HOST_SERVICE_INS_ID		1

#define IPA_MODEM_SERVICE_SVC_ID	0x31
#define IPA_MODEM_SERVICE_INS_ID	2
#define IPA_MODEM_SVC_VERS		1

#define QMI_INIT_DRIVER_TIMEOUT		60000	 

 
static void ipa_server_init_complete(struct ipa_qmi *ipa_qmi)
{
	struct ipa *ipa = container_of(ipa_qmi, struct ipa, qmi);
	struct qmi_handle *qmi = &ipa_qmi->server_handle;
	struct sockaddr_qrtr *sq = &ipa_qmi->modem_sq;
	struct ipa_init_complete_ind ind = { };
	int ret;

	ind.status.result = QMI_RESULT_SUCCESS_V01;
	ind.status.error = QMI_ERR_NONE_V01;

	ret = qmi_send_indication(qmi, sq, IPA_QMI_INIT_COMPLETE,
				   IPA_QMI_INIT_COMPLETE_IND_SZ,
				   ipa_init_complete_ind_ei, &ind);
	if (ret)
		dev_err(&ipa->pdev->dev,
			"error %d sending init complete indication\n", ret);
	else
		ipa_qmi->indication_sent = true;
}

 
static void ipa_qmi_indication(struct ipa_qmi *ipa_qmi)
{
	if (!ipa_qmi->indication_requested)
		return;

	if (ipa_qmi->indication_sent)
		return;

	ipa_server_init_complete(ipa_qmi);
}

 
static void ipa_qmi_ready(struct ipa_qmi *ipa_qmi)
{
	struct ipa *ipa;
	int ret;

	 
	if (!ipa_qmi->modem_ready || !ipa_qmi->uc_ready)
		return;

	 
	ipa_qmi_indication(ipa_qmi);

	 
	if (ipa_qmi->initial_boot) {
		if (!ipa_qmi->indication_sent)
			return;

		 
		ipa_qmi->initial_boot = false;
	}

	 
	ipa = container_of(ipa_qmi, struct ipa, qmi);
	ret = ipa_modem_start(ipa);
	if (ret)
		dev_err(&ipa->pdev->dev, "error %d starting modem\n", ret);
}

 
static void ipa_server_bye(struct qmi_handle *qmi, unsigned int node)
{
	struct ipa_qmi *ipa_qmi;

	ipa_qmi = container_of(qmi, struct ipa_qmi, server_handle);

	 
	memset(&ipa_qmi->modem_sq, 0, sizeof(ipa_qmi->modem_sq));

	 
	 
	ipa_qmi->modem_ready = false;
	ipa_qmi->indication_requested = false;
	ipa_qmi->indication_sent = false;
}

static const struct qmi_ops ipa_server_ops = {
	.bye		= ipa_server_bye,
};

 
static void ipa_server_indication_register(struct qmi_handle *qmi,
					   struct sockaddr_qrtr *sq,
					   struct qmi_txn *txn,
					   const void *decoded)
{
	struct ipa_indication_register_rsp rsp = { };
	struct ipa_qmi *ipa_qmi;
	struct ipa *ipa;
	int ret;

	ipa_qmi = container_of(qmi, struct ipa_qmi, server_handle);
	ipa = container_of(ipa_qmi, struct ipa, qmi);

	rsp.rsp.result = QMI_RESULT_SUCCESS_V01;
	rsp.rsp.error = QMI_ERR_NONE_V01;

	ret = qmi_send_response(qmi, sq, txn, IPA_QMI_INDICATION_REGISTER,
				IPA_QMI_INDICATION_REGISTER_RSP_SZ,
				ipa_indication_register_rsp_ei, &rsp);
	if (!ret) {
		ipa_qmi->indication_requested = true;
		ipa_qmi_ready(ipa_qmi);		 
	} else {
		dev_err(&ipa->pdev->dev,
			"error %d sending register indication response\n", ret);
	}
}

 
static void ipa_server_driver_init_complete(struct qmi_handle *qmi,
					    struct sockaddr_qrtr *sq,
					    struct qmi_txn *txn,
					    const void *decoded)
{
	struct ipa_driver_init_complete_rsp rsp = { };
	struct ipa_qmi *ipa_qmi;
	struct ipa *ipa;
	int ret;

	ipa_qmi = container_of(qmi, struct ipa_qmi, server_handle);
	ipa = container_of(ipa_qmi, struct ipa, qmi);

	rsp.rsp.result = QMI_RESULT_SUCCESS_V01;
	rsp.rsp.error = QMI_ERR_NONE_V01;

	ret = qmi_send_response(qmi, sq, txn, IPA_QMI_DRIVER_INIT_COMPLETE,
				IPA_QMI_DRIVER_INIT_COMPLETE_RSP_SZ,
				ipa_driver_init_complete_rsp_ei, &rsp);
	if (!ret) {
		ipa_qmi->uc_ready = true;
		ipa_qmi_ready(ipa_qmi);		 
	} else {
		dev_err(&ipa->pdev->dev,
			"error %d sending init complete response\n", ret);
	}
}

 
static const struct qmi_msg_handler ipa_server_msg_handlers[] = {
	{
		.type		= QMI_REQUEST,
		.msg_id		= IPA_QMI_INDICATION_REGISTER,
		.ei		= ipa_indication_register_req_ei,
		.decoded_size	= IPA_QMI_INDICATION_REGISTER_REQ_SZ,
		.fn		= ipa_server_indication_register,
	},
	{
		.type		= QMI_REQUEST,
		.msg_id		= IPA_QMI_DRIVER_INIT_COMPLETE,
		.ei		= ipa_driver_init_complete_req_ei,
		.decoded_size	= IPA_QMI_DRIVER_INIT_COMPLETE_REQ_SZ,
		.fn		= ipa_server_driver_init_complete,
	},
	{ },
};

 
static void ipa_client_init_driver(struct qmi_handle *qmi,
				   struct sockaddr_qrtr *sq,
				   struct qmi_txn *txn, const void *decoded)
{
	txn->result = 0;	 
	complete(&txn->completion);
}

 
static const struct qmi_msg_handler ipa_client_msg_handlers[] = {
	{
		.type		= QMI_RESPONSE,
		.msg_id		= IPA_QMI_INIT_DRIVER,
		.ei		= ipa_init_modem_driver_rsp_ei,
		.decoded_size	= IPA_QMI_INIT_DRIVER_RSP_SZ,
		.fn		= ipa_client_init_driver,
	},
	{ },
};

 
static const struct ipa_init_modem_driver_req *
init_modem_driver_req(struct ipa_qmi *ipa_qmi)
{
	struct ipa *ipa = container_of(ipa_qmi, struct ipa, qmi);
	u32 modem_route_count = ipa->modem_route_count;
	static struct ipa_init_modem_driver_req req;
	const struct ipa_mem *mem;

	 
	req.skip_uc_load_valid = 1;
	req.skip_uc_load = ipa->uc_loaded ? 1 : 0;

	 
	if (req.platform_type_valid)
		return &req;

	req.platform_type_valid = 1;
	req.platform_type = IPA_QMI_PLATFORM_TYPE_MSM_ANDROID;

	mem = ipa_mem_find(ipa, IPA_MEM_MODEM_HEADER);
	if (mem->size) {
		req.hdr_tbl_info_valid = 1;
		req.hdr_tbl_info.start = ipa->mem_offset + mem->offset;
		req.hdr_tbl_info.end = req.hdr_tbl_info.start + mem->size - 1;
	}

	mem = ipa_mem_find(ipa, IPA_MEM_V4_ROUTE);
	req.v4_route_tbl_info_valid = 1;
	req.v4_route_tbl_info.start = ipa->mem_offset + mem->offset;
	req.v4_route_tbl_info.end = modem_route_count - 1;

	mem = ipa_mem_find(ipa, IPA_MEM_V6_ROUTE);
	req.v6_route_tbl_info_valid = 1;
	req.v6_route_tbl_info.start = ipa->mem_offset + mem->offset;
	req.v6_route_tbl_info.end = modem_route_count - 1;

	mem = ipa_mem_find(ipa, IPA_MEM_V4_FILTER);
	req.v4_filter_tbl_start_valid = 1;
	req.v4_filter_tbl_start = ipa->mem_offset + mem->offset;

	mem = ipa_mem_find(ipa, IPA_MEM_V6_FILTER);
	req.v6_filter_tbl_start_valid = 1;
	req.v6_filter_tbl_start = ipa->mem_offset + mem->offset;

	mem = ipa_mem_find(ipa, IPA_MEM_MODEM);
	if (mem->size) {
		req.modem_mem_info_valid = 1;
		req.modem_mem_info.start = ipa->mem_offset + mem->offset;
		req.modem_mem_info.size = mem->size;
	}

	req.ctrl_comm_dest_end_pt_valid = 1;
	req.ctrl_comm_dest_end_pt =
		ipa->name_map[IPA_ENDPOINT_AP_MODEM_RX]->endpoint_id;

	 

	mem = ipa_mem_find(ipa, IPA_MEM_MODEM_PROC_CTX);
	if (mem->size) {
		req.hdr_proc_ctx_tbl_info_valid = 1;
		req.hdr_proc_ctx_tbl_info.start =
			ipa->mem_offset + mem->offset;
		req.hdr_proc_ctx_tbl_info.end =
			req.hdr_proc_ctx_tbl_info.start + mem->size - 1;
	}

	 

	mem = ipa_mem_find(ipa, IPA_MEM_V4_ROUTE_HASHED);
	if (mem->size) {
		req.v4_hash_route_tbl_info_valid = 1;
		req.v4_hash_route_tbl_info.start =
				ipa->mem_offset + mem->offset;
		req.v4_hash_route_tbl_info.end = modem_route_count - 1;
	}

	mem = ipa_mem_find(ipa, IPA_MEM_V6_ROUTE_HASHED);
	if (mem->size) {
		req.v6_hash_route_tbl_info_valid = 1;
		req.v6_hash_route_tbl_info.start =
			ipa->mem_offset + mem->offset;
		req.v6_hash_route_tbl_info.end = modem_route_count - 1;
	}

	mem = ipa_mem_find(ipa, IPA_MEM_V4_FILTER_HASHED);
	if (mem->size) {
		req.v4_hash_filter_tbl_start_valid = 1;
		req.v4_hash_filter_tbl_start = ipa->mem_offset + mem->offset;
	}

	mem = ipa_mem_find(ipa, IPA_MEM_V6_FILTER_HASHED);
	if (mem->size) {
		req.v6_hash_filter_tbl_start_valid = 1;
		req.v6_hash_filter_tbl_start = ipa->mem_offset + mem->offset;
	}

	 
	if (ipa->version >= IPA_VERSION_4_0) {
		mem = ipa_mem_find(ipa, IPA_MEM_STATS_QUOTA_MODEM);
		if (mem->size) {
			req.hw_stats_quota_base_addr_valid = 1;
			req.hw_stats_quota_base_addr =
				ipa->mem_offset + mem->offset;
			req.hw_stats_quota_size_valid = 1;
			req.hw_stats_quota_size = ipa->mem_offset + mem->size;
		}

		 
		mem = ipa_mem_find(ipa, IPA_MEM_STATS_DROP);
		if (mem && mem->size) {
			req.hw_stats_drop_base_addr_valid = 1;
			req.hw_stats_drop_base_addr =
				ipa->mem_offset + mem->offset;
			req.hw_stats_drop_size_valid = 1;
			req.hw_stats_drop_size = ipa->mem_offset + mem->size;
		}
	}

	return &req;
}

 
static void ipa_client_init_driver_work(struct work_struct *work)
{
	unsigned long timeout = msecs_to_jiffies(QMI_INIT_DRIVER_TIMEOUT);
	const struct ipa_init_modem_driver_req *req;
	struct ipa_qmi *ipa_qmi;
	struct qmi_handle *qmi;
	struct qmi_txn txn;
	struct device *dev;
	struct ipa *ipa;
	int ret;

	ipa_qmi = container_of(work, struct ipa_qmi, init_driver_work);
	qmi = &ipa_qmi->client_handle;

	ipa = container_of(ipa_qmi, struct ipa, qmi);
	dev = &ipa->pdev->dev;

	ret = qmi_txn_init(qmi, &txn, NULL, NULL);
	if (ret < 0) {
		dev_err(dev, "error %d preparing init driver request\n", ret);
		return;
	}

	 
	req = init_modem_driver_req(ipa_qmi);
	ret = qmi_send_request(qmi, &ipa_qmi->modem_sq, &txn,
			       IPA_QMI_INIT_DRIVER, IPA_QMI_INIT_DRIVER_REQ_SZ,
			       ipa_init_modem_driver_req_ei, req);
	if (ret)
		dev_err(dev, "error %d sending init driver request\n", ret);
	else if ((ret = qmi_txn_wait(&txn, timeout)))
		dev_err(dev, "error %d awaiting init driver response\n", ret);

	if (!ret) {
		ipa_qmi->modem_ready = true;
		ipa_qmi_ready(ipa_qmi);		 
	} else {
		 
		qmi_txn_cancel(&txn);
	}
}

 
static int
ipa_client_new_server(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct ipa_qmi *ipa_qmi;

	ipa_qmi = container_of(qmi, struct ipa_qmi, client_handle);

	ipa_qmi->modem_sq.sq_family = AF_QIPCRTR;
	ipa_qmi->modem_sq.sq_node = svc->node;
	ipa_qmi->modem_sq.sq_port = svc->port;

	schedule_work(&ipa_qmi->init_driver_work);

	return 0;
}

static const struct qmi_ops ipa_client_ops = {
	.new_server	= ipa_client_new_server,
};

 
int ipa_qmi_setup(struct ipa *ipa)
{
	struct ipa_qmi *ipa_qmi = &ipa->qmi;
	int ret;

	ipa_qmi->initial_boot = true;

	 
	ret = qmi_handle_init(&ipa_qmi->server_handle,
			      IPA_QMI_SERVER_MAX_RCV_SZ, &ipa_server_ops,
			      ipa_server_msg_handlers);
	if (ret)
		return ret;

	ret = qmi_add_server(&ipa_qmi->server_handle, IPA_HOST_SERVICE_SVC_ID,
			     IPA_HOST_SVC_VERS, IPA_HOST_SERVICE_INS_ID);
	if (ret)
		goto err_server_handle_release;

	 
	ret = qmi_handle_init(&ipa_qmi->client_handle,
			      IPA_QMI_CLIENT_MAX_RCV_SZ, &ipa_client_ops,
			      ipa_client_msg_handlers);
	if (ret)
		goto err_server_handle_release;

	 
	INIT_WORK(&ipa_qmi->init_driver_work, ipa_client_init_driver_work);

	ret = qmi_add_lookup(&ipa_qmi->client_handle, IPA_MODEM_SERVICE_SVC_ID,
			     IPA_MODEM_SVC_VERS, IPA_MODEM_SERVICE_INS_ID);
	if (ret)
		goto err_client_handle_release;

	return 0;

err_client_handle_release:
	 
	qmi_handle_release(&ipa_qmi->client_handle);
	memset(&ipa_qmi->client_handle, 0, sizeof(ipa_qmi->client_handle));
err_server_handle_release:
	 
	qmi_handle_release(&ipa_qmi->server_handle);
	memset(&ipa_qmi->server_handle, 0, sizeof(ipa_qmi->server_handle));

	return ret;
}

 
void ipa_qmi_teardown(struct ipa *ipa)
{
	cancel_work_sync(&ipa->qmi.init_driver_work);

	qmi_handle_release(&ipa->qmi.client_handle);
	memset(&ipa->qmi.client_handle, 0, sizeof(ipa->qmi.client_handle));

	qmi_handle_release(&ipa->qmi.server_handle);
	memset(&ipa->qmi.server_handle, 0, sizeof(ipa->qmi.server_handle));
}
