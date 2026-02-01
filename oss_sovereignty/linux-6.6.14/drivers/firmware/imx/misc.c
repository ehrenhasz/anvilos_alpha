
 

#include <linux/firmware/imx/svc/misc.h>

struct imx_sc_msg_req_misc_set_ctrl {
	struct imx_sc_rpc_msg hdr;
	u32 ctrl;
	u32 val;
	u16 resource;
} __packed __aligned(4);

struct imx_sc_msg_req_cpu_start {
	struct imx_sc_rpc_msg hdr;
	u32 address_hi;
	u32 address_lo;
	u16 resource;
	u8 enable;
} __packed __aligned(4);

struct imx_sc_msg_req_misc_get_ctrl {
	struct imx_sc_rpc_msg hdr;
	u32 ctrl;
	u16 resource;
} __packed __aligned(4);

struct imx_sc_msg_resp_misc_get_ctrl {
	struct imx_sc_rpc_msg hdr;
	u32 val;
} __packed __aligned(4);

 

int imx_sc_misc_set_control(struct imx_sc_ipc *ipc, u32 resource,
			    u8 ctrl, u32 val)
{
	struct imx_sc_msg_req_misc_set_ctrl msg;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = (uint8_t)IMX_SC_RPC_SVC_MISC;
	hdr->func = (uint8_t)IMX_SC_MISC_FUNC_SET_CONTROL;
	hdr->size = 4;

	msg.ctrl = ctrl;
	msg.val = val;
	msg.resource = resource;

	return imx_scu_call_rpc(ipc, &msg, true);
}
EXPORT_SYMBOL(imx_sc_misc_set_control);

 

int imx_sc_misc_get_control(struct imx_sc_ipc *ipc, u32 resource,
			    u8 ctrl, u32 *val)
{
	struct imx_sc_msg_req_misc_get_ctrl msg;
	struct imx_sc_msg_resp_misc_get_ctrl *resp;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;
	int ret;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = (uint8_t)IMX_SC_RPC_SVC_MISC;
	hdr->func = (uint8_t)IMX_SC_MISC_FUNC_GET_CONTROL;
	hdr->size = 3;

	msg.ctrl = ctrl;
	msg.resource = resource;

	ret = imx_scu_call_rpc(ipc, &msg, true);
	if (ret)
		return ret;

	resp = (struct imx_sc_msg_resp_misc_get_ctrl *)&msg;
	if (val != NULL)
		*val = resp->val;

	return 0;
}
EXPORT_SYMBOL(imx_sc_misc_get_control);

 
int imx_sc_pm_cpu_start(struct imx_sc_ipc *ipc, u32 resource,
			bool enable, u64 phys_addr)
{
	struct imx_sc_msg_req_cpu_start msg;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = IMX_SC_RPC_SVC_PM;
	hdr->func = IMX_SC_PM_FUNC_CPU_START;
	hdr->size = 4;

	msg.address_hi = phys_addr >> 32;
	msg.address_lo = phys_addr;
	msg.resource = resource;
	msg.enable = enable;

	return imx_scu_call_rpc(ipc, &msg, true);
}
EXPORT_SYMBOL(imx_sc_pm_cpu_start);
