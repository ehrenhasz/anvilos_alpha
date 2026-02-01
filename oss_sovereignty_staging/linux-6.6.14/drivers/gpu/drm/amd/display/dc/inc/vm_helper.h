 

#ifndef DC_INC_VM_HELPER_H_
#define DC_INC_VM_HELPER_H_

#include "dc_types.h"

#define MAX_HUBP 6

struct vmid_usage {
	int vmid_usage[2];
};

struct vm_helper {
	unsigned int num_vmid;
	struct vmid_usage hubp_vmid_usage[MAX_HUBP];
};

void vm_helper_mark_vmid_used(struct vm_helper *vm_helper, unsigned int pos, uint8_t hubp_idx);

void vm_helper_init(
	struct vm_helper *vm_helper,
	unsigned int num_vmid);

#endif  
