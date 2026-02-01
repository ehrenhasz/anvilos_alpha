 

#include "vm_helper.h"
#include "dc.h"

void vm_helper_mark_vmid_used(struct vm_helper *vm_helper, unsigned int pos, uint8_t hubp_idx)
{
	struct vmid_usage vmids = vm_helper->hubp_vmid_usage[hubp_idx];

	vmids.vmid_usage[0] = vmids.vmid_usage[1];
	vmids.vmid_usage[1] = 1 << pos;
}

int dc_setup_system_context(struct dc *dc, struct dc_phy_addr_space_config *pa_config)
{
	int num_vmids = 0;

	 
	if (dc->hwss.init_sys_ctx) {
		num_vmids = dc->hwss.init_sys_ctx(dc->hwseq, dc, pa_config);

		 
		memcpy(&dc->vm_pa_config, pa_config, sizeof(struct dc_phy_addr_space_config));
		dc->vm_pa_config.valid = true;
		dc_z10_save_init(dc);
	}

	return num_vmids;
}

void dc_setup_vm_context(struct dc *dc, struct dc_virtual_addr_space_config *va_config, int vmid)
{
	dc->hwss.init_vm_ctx(dc->hwseq, dc, va_config, vmid);
}

int dc_get_vmid_use_vector(struct dc *dc)
{
	int i;
	int in_use = 0;

	for (i = 0; i < MAX_HUBP; i++)
		in_use |= dc->vm_helper->hubp_vmid_usage[i].vmid_usage[0]
			| dc->vm_helper->hubp_vmid_usage[i].vmid_usage[1];
	return in_use;
}

void vm_helper_init(struct vm_helper *vm_helper, unsigned int num_vmid)
{
	vm_helper->num_vmid = num_vmid;

	memset(vm_helper->hubp_vmid_usage, 0, sizeof(vm_helper->hubp_vmid_usage[0]) * MAX_HUBP);
}
