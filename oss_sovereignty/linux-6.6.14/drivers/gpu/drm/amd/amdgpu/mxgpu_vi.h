 

#ifndef __MXGPU_VI_H__
#define __MXGPU_VI_H__

#define VI_MAILBOX_TIMEDOUT	12000
#define VI_MAILBOX_RESET_TIME	12

 
enum idh_request {
	IDH_REQ_GPU_INIT_ACCESS	= 1,
	IDH_REL_GPU_INIT_ACCESS,
	IDH_REQ_GPU_FINI_ACCESS,
	IDH_REL_GPU_FINI_ACCESS,
	IDH_REQ_GPU_RESET_ACCESS,

	IDH_LOG_VF_ERROR       = 200,
};

 
enum idh_event {
	IDH_CLR_MSG_BUF = 0,
	IDH_READY_TO_ACCESS_GPU,
	IDH_FLR_NOTIFICATION,
	IDH_FLR_NOTIFICATION_CMPL,

	IDH_TEXT_MESSAGE = 255
};

extern const struct amdgpu_virt_ops xgpu_vi_virt_ops;

void xgpu_vi_init_golden_registers(struct amdgpu_device *adev);
void xgpu_vi_mailbox_set_irq_funcs(struct amdgpu_device *adev);
int xgpu_vi_mailbox_add_irq_id(struct amdgpu_device *adev);
int xgpu_vi_mailbox_get_irq(struct amdgpu_device *adev);
void xgpu_vi_mailbox_put_irq(struct amdgpu_device *adev);

#endif
