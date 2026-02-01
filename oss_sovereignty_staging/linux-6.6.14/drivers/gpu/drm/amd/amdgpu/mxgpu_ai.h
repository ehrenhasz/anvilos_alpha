 

#ifndef __MXGPU_AI_H__
#define __MXGPU_AI_H__

#define AI_MAILBOX_POLL_ACK_TIMEDOUT	500
#define AI_MAILBOX_POLL_MSG_TIMEDOUT	6000
#define AI_MAILBOX_POLL_FLR_TIMEDOUT	10000
#define AI_MAILBOX_POLL_MSG_REP_MAX	11

enum idh_request {
	IDH_REQ_GPU_INIT_ACCESS = 1,
	IDH_REL_GPU_INIT_ACCESS,
	IDH_REQ_GPU_FINI_ACCESS,
	IDH_REL_GPU_FINI_ACCESS,
	IDH_REQ_GPU_RESET_ACCESS,
	IDH_REQ_GPU_INIT_DATA,

	IDH_LOG_VF_ERROR       = 200,
	IDH_READY_TO_RESET 	= 201,
	IDH_RAS_POISON  = 202,
};

enum idh_event {
	IDH_CLR_MSG_BUF	= 0,
	IDH_READY_TO_ACCESS_GPU,
	IDH_FLR_NOTIFICATION,
	IDH_FLR_NOTIFICATION_CMPL,
	IDH_SUCCESS,
	IDH_FAIL,
	IDH_QUERY_ALIVE,
	IDH_REQ_GPU_INIT_DATA_READY,

	IDH_TEXT_MESSAGE = 255,
};

extern const struct amdgpu_virt_ops xgpu_ai_virt_ops;

void xgpu_ai_mailbox_set_irq_funcs(struct amdgpu_device *adev);
int xgpu_ai_mailbox_add_irq_id(struct amdgpu_device *adev);
int xgpu_ai_mailbox_get_irq(struct amdgpu_device *adev);
void xgpu_ai_mailbox_put_irq(struct amdgpu_device *adev);

#define AI_MAIBOX_CONTROL_TRN_OFFSET_BYTE SOC15_REG_OFFSET(NBIO, 0, mmBIF_BX_PF0_MAILBOX_CONTROL) * 4
#define AI_MAIBOX_CONTROL_RCV_OFFSET_BYTE SOC15_REG_OFFSET(NBIO, 0, mmBIF_BX_PF0_MAILBOX_CONTROL) * 4 + 1

#endif
