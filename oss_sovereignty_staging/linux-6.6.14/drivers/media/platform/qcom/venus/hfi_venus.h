 
 
#ifndef __VENUS_HFI_VENUS_H__
#define __VENUS_HFI_VENUS_H__

struct venus_core;

void venus_hfi_destroy(struct venus_core *core);
int venus_hfi_create(struct venus_core *core);
void venus_hfi_queues_reinit(struct venus_core *core);

#endif
