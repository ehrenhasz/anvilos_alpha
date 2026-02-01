 

#ifndef __AMDGPU_DM_IRQ_H__
#define __AMDGPU_DM_IRQ_H__

#include "irq_types.h"  

 

 
int amdgpu_dm_irq_init(struct amdgpu_device *adev);

 
void amdgpu_dm_irq_fini(struct amdgpu_device *adev);

 
void *amdgpu_dm_irq_register_interrupt(struct amdgpu_device *adev,
				       struct dc_interrupt_params *int_params,
				       void (*ih)(void *),
				       void *handler_args);

 
void amdgpu_dm_irq_unregister_interrupt(struct amdgpu_device *adev,
					enum dc_irq_source irq_source,
					void *ih_index);

void amdgpu_dm_set_irq_funcs(struct amdgpu_device *adev);

void amdgpu_dm_outbox_init(struct amdgpu_device *adev);
void amdgpu_dm_hpd_init(struct amdgpu_device *adev);
void amdgpu_dm_hpd_fini(struct amdgpu_device *adev);

 
int amdgpu_dm_irq_suspend(struct amdgpu_device *adev);

 
int amdgpu_dm_irq_resume_early(struct amdgpu_device *adev);
int amdgpu_dm_irq_resume_late(struct amdgpu_device *adev);

#endif  
