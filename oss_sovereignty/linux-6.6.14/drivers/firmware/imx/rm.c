
 

#include <linux/firmware/imx/svc/rm.h>

struct imx_sc_msg_rm_rsrc_owned {
	struct imx_sc_rpc_msg hdr;
	u16 resource;
} __packed __aligned(4);

 
bool imx_sc_rm_is_resource_owned(struct imx_sc_ipc *ipc, u16 resource)
{
	struct imx_sc_msg_rm_rsrc_owned msg;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = IMX_SC_RPC_SVC_RM;
	hdr->func = IMX_SC_RM_FUNC_IS_RESOURCE_OWNED;
	hdr->size = 2;

	msg.resource = resource;

	 
	imx_scu_call_rpc(ipc, &msg, true);

	return hdr->func;
}
EXPORT_SYMBOL(imx_sc_rm_is_resource_owned);

struct imx_sc_msg_rm_get_resource_owner {
	struct imx_sc_rpc_msg hdr;
	union {
		struct {
			u16 resource;
		} req;
		struct {
			u8 val;
		} resp;
	} data;
} __packed __aligned(4);

 
int imx_sc_rm_get_resource_owner(struct imx_sc_ipc *ipc, u16 resource, u8 *pt)
{
	struct imx_sc_msg_rm_get_resource_owner msg;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;
	int ret;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = IMX_SC_RPC_SVC_RM;
	hdr->func = IMX_SC_RM_FUNC_GET_RESOURCE_OWNER;
	hdr->size = 2;

	msg.data.req.resource = resource;

	ret = imx_scu_call_rpc(ipc, &msg, true);
	if (ret)
		return ret;

	if (pt)
		*pt = msg.data.resp.val;

	return 0;
}
EXPORT_SYMBOL(imx_sc_rm_get_resource_owner);
