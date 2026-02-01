
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acevents.h"

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwgpe")
#if (!ACPI_REDUCED_HARDWARE)	 
 
static acpi_status
acpi_hw_enable_wakeup_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
				struct acpi_gpe_block_info *gpe_block,
				void *context);

static acpi_status
acpi_hw_gpe_enable_write(u8 enable_mask,
			 struct acpi_gpe_register_info *gpe_register_info);

 

acpi_status acpi_hw_gpe_read(u64 *value, struct acpi_gpe_address *reg)
{
	acpi_status status;
	u32 value32;

	if (reg->space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
#ifdef ACPI_GPE_USE_LOGICAL_ADDRESSES
		*value = (u64)ACPI_GET8((unsigned long)reg->address);
		return_ACPI_STATUS(AE_OK);
#else
		return acpi_os_read_memory((acpi_physical_address)reg->address,
					    value, ACPI_GPE_REGISTER_WIDTH);
#endif
	}

	status = acpi_os_read_port((acpi_io_address)reg->address,
				   &value32, ACPI_GPE_REGISTER_WIDTH);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(status);

	*value = (u64)value32;

	return_ACPI_STATUS(AE_OK);
}

 

acpi_status acpi_hw_gpe_write(u64 value, struct acpi_gpe_address *reg)
{
	if (reg->space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
#ifdef ACPI_GPE_USE_LOGICAL_ADDRESSES
		ACPI_SET8((unsigned long)reg->address, value);
		return_ACPI_STATUS(AE_OK);
#else
		return acpi_os_write_memory((acpi_physical_address)reg->address,
					    value, ACPI_GPE_REGISTER_WIDTH);
#endif
	}

	return acpi_os_write_port((acpi_io_address)reg->address, (u32)value,
				  ACPI_GPE_REGISTER_WIDTH);
}

 

u32 acpi_hw_get_gpe_register_bit(struct acpi_gpe_event_info *gpe_event_info)
{

	return ((u32)1 <<
		(gpe_event_info->gpe_number -
		 gpe_event_info->register_info->base_gpe_number));
}

 

acpi_status
acpi_hw_low_set_gpe(struct acpi_gpe_event_info *gpe_event_info, u32 action)
{
	struct acpi_gpe_register_info *gpe_register_info;
	acpi_status status = AE_OK;
	u64 enable_mask;
	u32 register_bit;

	ACPI_FUNCTION_ENTRY();

	 

	gpe_register_info = gpe_event_info->register_info;
	if (!gpe_register_info) {
		return (AE_NOT_EXIST);
	}

	 

	status = acpi_hw_gpe_read(&enable_mask,
				  &gpe_register_info->enable_address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	 

	register_bit = acpi_hw_get_gpe_register_bit(gpe_event_info);
	switch (action) {
	case ACPI_GPE_CONDITIONAL_ENABLE:

		 

		if (!(register_bit & gpe_register_info->enable_mask)) {
			return (AE_BAD_PARAMETER);
		}

		ACPI_FALLTHROUGH;

	case ACPI_GPE_ENABLE:

		ACPI_SET_BIT(enable_mask, register_bit);
		break;

	case ACPI_GPE_DISABLE:

		ACPI_CLEAR_BIT(enable_mask, register_bit);
		break;

	default:

		ACPI_ERROR((AE_INFO, "Invalid GPE Action, %u", action));
		return (AE_BAD_PARAMETER);
	}

	if (!(register_bit & gpe_register_info->mask_for_run)) {

		 

		status = acpi_hw_gpe_write(enable_mask,
					   &gpe_register_info->enable_address);
	}
	return (status);
}

 

acpi_status acpi_hw_clear_gpe(struct acpi_gpe_event_info *gpe_event_info)
{
	struct acpi_gpe_register_info *gpe_register_info;
	acpi_status status;
	u32 register_bit;

	ACPI_FUNCTION_ENTRY();

	 

	gpe_register_info = gpe_event_info->register_info;
	if (!gpe_register_info) {
		return (AE_NOT_EXIST);
	}

	 
	register_bit = acpi_hw_get_gpe_register_bit(gpe_event_info);

	status = acpi_hw_gpe_write(register_bit,
				   &gpe_register_info->status_address);
	return (status);
}

 

acpi_status
acpi_hw_get_gpe_status(struct acpi_gpe_event_info *gpe_event_info,
		       acpi_event_status *event_status)
{
	u64 in_byte;
	u32 register_bit;
	struct acpi_gpe_register_info *gpe_register_info;
	acpi_event_status local_event_status = 0;
	acpi_status status;

	ACPI_FUNCTION_ENTRY();

	if (!event_status) {
		return (AE_BAD_PARAMETER);
	}

	 

	if (ACPI_GPE_DISPATCH_TYPE(gpe_event_info->flags) !=
	    ACPI_GPE_DISPATCH_NONE) {
		local_event_status |= ACPI_EVENT_FLAG_HAS_HANDLER;
	}

	 

	gpe_register_info = gpe_event_info->register_info;

	 

	register_bit = acpi_hw_get_gpe_register_bit(gpe_event_info);

	 

	if (register_bit & gpe_register_info->enable_for_run) {
		local_event_status |= ACPI_EVENT_FLAG_ENABLED;
	}

	 

	if (register_bit & gpe_register_info->mask_for_run) {
		local_event_status |= ACPI_EVENT_FLAG_MASKED;
	}

	 

	if (register_bit & gpe_register_info->enable_for_wake) {
		local_event_status |= ACPI_EVENT_FLAG_WAKE_ENABLED;
	}

	 

	status = acpi_hw_gpe_read(&in_byte, &gpe_register_info->enable_address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (register_bit & in_byte) {
		local_event_status |= ACPI_EVENT_FLAG_ENABLE_SET;
	}

	 

	status = acpi_hw_gpe_read(&in_byte, &gpe_register_info->status_address);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	if (register_bit & in_byte) {
		local_event_status |= ACPI_EVENT_FLAG_STATUS_SET;
	}

	 

	(*event_status) = local_event_status;
	return (AE_OK);
}

 

static acpi_status
acpi_hw_gpe_enable_write(u8 enable_mask,
			 struct acpi_gpe_register_info *gpe_register_info)
{
	acpi_status status;

	gpe_register_info->enable_mask = enable_mask;

	status = acpi_hw_gpe_write(enable_mask,
				   &gpe_register_info->enable_address);
	return (status);
}

 

acpi_status
acpi_hw_disable_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
			  struct acpi_gpe_block_info *gpe_block, void *context)
{
	u32 i;
	acpi_status status;

	 

	for (i = 0; i < gpe_block->register_count; i++) {

		 

		status =
		    acpi_hw_gpe_enable_write(0x00,
					     &gpe_block->register_info[i]);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	return (AE_OK);
}

 

acpi_status
acpi_hw_clear_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
			struct acpi_gpe_block_info *gpe_block, void *context)
{
	u32 i;
	acpi_status status;

	 

	for (i = 0; i < gpe_block->register_count; i++) {

		 

		status = acpi_hw_gpe_write(0xFF,
					   &gpe_block->register_info[i].status_address);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	return (AE_OK);
}

 

acpi_status
acpi_hw_enable_runtime_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
				 struct acpi_gpe_block_info *gpe_block,
				 void *context)
{
	u32 i;
	acpi_status status;
	struct acpi_gpe_register_info *gpe_register_info;
	u8 enable_mask;

	 

	 

	for (i = 0; i < gpe_block->register_count; i++) {
		gpe_register_info = &gpe_block->register_info[i];
		if (!gpe_register_info->enable_for_run) {
			continue;
		}

		 

		enable_mask = gpe_register_info->enable_for_run &
		    ~gpe_register_info->mask_for_run;
		status =
		    acpi_hw_gpe_enable_write(enable_mask, gpe_register_info);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	return (AE_OK);
}

 

static acpi_status
acpi_hw_enable_wakeup_gpe_block(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
				struct acpi_gpe_block_info *gpe_block,
				void *context)
{
	u32 i;
	acpi_status status;
	struct acpi_gpe_register_info *gpe_register_info;

	 

	for (i = 0; i < gpe_block->register_count; i++) {
		gpe_register_info = &gpe_block->register_info[i];

		 

		status =
		    acpi_hw_gpe_enable_write(gpe_register_info->enable_for_wake,
					     gpe_register_info);
		if (ACPI_FAILURE(status)) {
			return (status);
		}
	}

	return (AE_OK);
}

struct acpi_gpe_block_status_context {
	struct acpi_gpe_register_info *gpe_skip_register_info;
	u8 gpe_skip_mask;
	u8 retval;
};

 

static acpi_status
acpi_hw_get_gpe_block_status(struct acpi_gpe_xrupt_info *gpe_xrupt_info,
			     struct acpi_gpe_block_info *gpe_block,
			     void *context)
{
	struct acpi_gpe_block_status_context *c = context;
	struct acpi_gpe_register_info *gpe_register_info;
	u64 in_enable, in_status;
	acpi_status status;
	u8 ret_mask;
	u32 i;

	 

	for (i = 0; i < gpe_block->register_count; i++) {
		gpe_register_info = &gpe_block->register_info[i];

		status = acpi_hw_gpe_read(&in_enable,
					  &gpe_register_info->enable_address);
		if (ACPI_FAILURE(status)) {
			continue;
		}

		status = acpi_hw_gpe_read(&in_status,
					  &gpe_register_info->status_address);
		if (ACPI_FAILURE(status)) {
			continue;
		}

		ret_mask = in_enable & in_status;
		if (ret_mask && c->gpe_skip_register_info == gpe_register_info) {
			ret_mask &= ~c->gpe_skip_mask;
		}
		c->retval |= ret_mask;
	}

	return (AE_OK);
}

 

acpi_status acpi_hw_disable_all_gpes(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_disable_all_gpes);

	status = acpi_ev_walk_gpe_list(acpi_hw_disable_gpe_block, NULL);
	return_ACPI_STATUS(status);
}

 

acpi_status acpi_hw_enable_all_runtime_gpes(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_enable_all_runtime_gpes);

	status = acpi_ev_walk_gpe_list(acpi_hw_enable_runtime_gpe_block, NULL);
	return_ACPI_STATUS(status);
}

 

acpi_status acpi_hw_enable_all_wakeup_gpes(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(hw_enable_all_wakeup_gpes);

	status = acpi_ev_walk_gpe_list(acpi_hw_enable_wakeup_gpe_block, NULL);
	return_ACPI_STATUS(status);
}

 

u8 acpi_hw_check_all_gpes(acpi_handle gpe_skip_device, u32 gpe_skip_number)
{
	struct acpi_gpe_block_status_context context = {
		.gpe_skip_register_info = NULL,
		.retval = 0,
	};
	struct acpi_gpe_event_info *gpe_event_info;
	acpi_cpu_flags flags;

	ACPI_FUNCTION_TRACE(acpi_hw_check_all_gpes);

	flags = acpi_os_acquire_lock(acpi_gbl_gpe_lock);

	gpe_event_info = acpi_ev_get_gpe_event_info(gpe_skip_device,
						    gpe_skip_number);
	if (gpe_event_info) {
		context.gpe_skip_register_info = gpe_event_info->register_info;
		context.gpe_skip_mask = acpi_hw_get_gpe_register_bit(gpe_event_info);
	}

	acpi_os_release_lock(acpi_gbl_gpe_lock, flags);

	(void)acpi_ev_walk_gpe_list(acpi_hw_get_gpe_block_status, &context);
	return (context.retval != 0);
}

#endif				 
