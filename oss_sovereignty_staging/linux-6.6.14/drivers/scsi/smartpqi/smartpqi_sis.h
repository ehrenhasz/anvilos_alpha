 
 

#if !defined(_SMARTPQI_SIS_H)
#define _SMARTPQI_SIS_H

void sis_verify_structures(void);
int sis_wait_for_ctrl_ready(struct pqi_ctrl_info *ctrl_info);
int sis_wait_for_ctrl_ready_resume(struct pqi_ctrl_info *ctrl_info);
bool sis_is_firmware_running(struct pqi_ctrl_info *ctrl_info);
bool sis_is_kernel_up(struct pqi_ctrl_info *ctrl_info);
int sis_get_ctrl_properties(struct pqi_ctrl_info *ctrl_info);
int sis_get_pqi_capabilities(struct pqi_ctrl_info *ctrl_info);
int sis_init_base_struct_addr(struct pqi_ctrl_info *ctrl_info);
void sis_enable_msix(struct pqi_ctrl_info *ctrl_info);
void sis_enable_intx(struct pqi_ctrl_info *ctrl_info);
void sis_shutdown_ctrl(struct pqi_ctrl_info *ctrl_info,
	enum pqi_ctrl_shutdown_reason ctrl_shutdown_reason);
int sis_pqi_reset_quiesce(struct pqi_ctrl_info *ctrl_info);
int sis_reenable_sis_mode(struct pqi_ctrl_info *ctrl_info);
void sis_write_driver_scratch(struct pqi_ctrl_info *ctrl_info, u32 value);
u32 sis_read_driver_scratch(struct pqi_ctrl_info *ctrl_info);
void sis_soft_reset(struct pqi_ctrl_info *ctrl_info);
u32 sis_get_product_id(struct pqi_ctrl_info *ctrl_info);
int sis_wait_for_fw_triage_completion(struct pqi_ctrl_info *ctrl_info);

extern unsigned int sis_ctrl_ready_timeout_secs;

#endif	 
