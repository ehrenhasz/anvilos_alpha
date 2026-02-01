 

#ifndef MOD_VMID_H_
#define MOD_VMID_H_

#define MAX_VMID 16

#include "dc.h"

struct mod_vmid {
	int dummy;
};

uint8_t mod_vmid_get_for_ptb(struct mod_vmid *mod_vmid, uint64_t ptb);
void mod_vmid_reset(struct mod_vmid *mod_vmid);
struct mod_vmid *mod_vmid_create(
		struct dc *dc,
		unsigned int num_vmid,
		struct dc_virtual_addr_space_config *va_config);

void mod_vmid_destroy(struct mod_vmid *mod_vmid);

#endif  
