 

#include "mod_vmid.h"

struct core_vmid {
	struct mod_vmid public;
	struct dc *dc;

	unsigned int num_vmid;
	unsigned int num_vmids_available;
	uint64_t ptb_assigned_to_vmid[MAX_VMID];
	struct dc_virtual_addr_space_config base_config;
};

#define MOD_VMID_TO_CORE(mod_vmid)\
		container_of(mod_vmid, struct core_vmid, public)

static void add_ptb_to_table(struct core_vmid *core_vmid, unsigned int vmid, uint64_t ptb)
{
	if (vmid < MAX_VMID) {
		core_vmid->ptb_assigned_to_vmid[vmid] = ptb;
		core_vmid->num_vmids_available--;
	}
}

static void clear_entry_from_vmid_table(struct core_vmid *core_vmid, unsigned int vmid)
{
	if (vmid < MAX_VMID) {
		core_vmid->ptb_assigned_to_vmid[vmid] = 0;
		core_vmid->num_vmids_available++;
	}
}

static void evict_vmids(struct core_vmid *core_vmid)
{
	int i;
	uint16_t ord = dc_get_vmid_use_vector(core_vmid->dc);

	
	for (i = 1; i < core_vmid->num_vmid; i++) {
		if (!(ord & (1u << i)))
			clear_entry_from_vmid_table(core_vmid, i);
	}
}


static int get_existing_vmid_for_ptb(struct core_vmid *core_vmid, uint64_t ptb)
{
	int i;

	for (i = 0; i < core_vmid->num_vmid; i++) {
		if (core_vmid->ptb_assigned_to_vmid[i] == ptb)
			return i;
	}

	return -1;
}


static int get_next_available_vmid(struct core_vmid *core_vmid)
{
	int i;

	for (i = 1; i < core_vmid->num_vmid; i++) {
		if (core_vmid->ptb_assigned_to_vmid[i] == 0)
			return i;
	}

	return -1;
}

uint8_t mod_vmid_get_for_ptb(struct mod_vmid *mod_vmid, uint64_t ptb)
{
	struct core_vmid *core_vmid = MOD_VMID_TO_CORE(mod_vmid);
	int vmid = 0;

	
	if (ptb == 0)
		return 0;

	vmid = get_existing_vmid_for_ptb(core_vmid, ptb);

	if (vmid == -1) {
		struct dc_virtual_addr_space_config va_config = core_vmid->base_config;

		va_config.page_table_base_addr = ptb;

		if (core_vmid->num_vmids_available == 0)
			evict_vmids(core_vmid);

		vmid = get_next_available_vmid(core_vmid);
		if (vmid != -1) {
			add_ptb_to_table(core_vmid, vmid, ptb);

			dc_setup_vm_context(core_vmid->dc, &va_config, vmid);
		} else
			ASSERT(0);
	}

	return vmid;
}

void mod_vmid_reset(struct mod_vmid *mod_vmid)
{
	struct core_vmid *core_vmid = MOD_VMID_TO_CORE(mod_vmid);

	core_vmid->num_vmids_available = core_vmid->num_vmid - 1;
	memset(core_vmid->ptb_assigned_to_vmid, 0, sizeof(core_vmid->ptb_assigned_to_vmid[0]) * MAX_VMID);
}

struct mod_vmid *mod_vmid_create(
		struct dc *dc,
		unsigned int num_vmid,
		struct dc_virtual_addr_space_config *va_config)
{
	struct core_vmid *core_vmid;

	if (num_vmid <= 1)
		goto fail_no_vm_ctx;

	if (dc == NULL)
		goto fail_dc_null;

	core_vmid = kzalloc(sizeof(struct core_vmid), GFP_KERNEL);

	if (core_vmid == NULL)
		goto fail_alloc_context;

	core_vmid->dc = dc;
	core_vmid->num_vmid = num_vmid;
	core_vmid->num_vmids_available = num_vmid - 1;
	core_vmid->base_config = *va_config;

	memset(core_vmid->ptb_assigned_to_vmid, 0, sizeof(core_vmid->ptb_assigned_to_vmid[0]) * MAX_VMID);

	return &core_vmid->public;

fail_no_vm_ctx:
fail_alloc_context:
fail_dc_null:
	return NULL;
}

void mod_vmid_destroy(struct mod_vmid *mod_vmid)
{
	if (mod_vmid != NULL) {
		struct core_vmid *core_vmid = MOD_VMID_TO_CORE(mod_vmid);

		kfree(core_vmid);
	}
}
