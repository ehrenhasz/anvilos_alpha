
 

#define EXPORT_ACPI_INTERFACES

#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"
#include "acevents.h"

#define _COMPONENT          ACPI_EVENTS
ACPI_MODULE_NAME("evxfregn")

 
static acpi_status
acpi_install_address_space_handler_internal(acpi_handle device,
					    acpi_adr_space_type space_id,
					    acpi_adr_space_handler handler,
					    acpi_adr_space_setup setup,
					    void *context, u8 run_reg)
{
	struct acpi_namespace_node *node;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_install_address_space_handler);

	 

	if (!device) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	 

	node = acpi_ns_validate_handle(device);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	 

	status =
	    acpi_ev_install_space_handler(node, space_id, handler, setup,
					  context);
	if (ACPI_FAILURE(status)) {
		goto unlock_and_exit;
	}

	 

	if (run_reg) {
		acpi_ev_execute_reg_methods(node, space_id, ACPI_REG_CONNECT);
	}

unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(status);
}

acpi_status
acpi_install_address_space_handler(acpi_handle device,
				   acpi_adr_space_type space_id,
				   acpi_adr_space_handler handler,
				   acpi_adr_space_setup setup, void *context)
{
	return acpi_install_address_space_handler_internal(device, space_id,
							   handler, setup,
							   context, TRUE);
}

ACPI_EXPORT_SYMBOL(acpi_install_address_space_handler)
acpi_status
acpi_install_address_space_handler_no_reg(acpi_handle device,
					  acpi_adr_space_type space_id,
					  acpi_adr_space_handler handler,
					  acpi_adr_space_setup setup,
					  void *context)
{
	return acpi_install_address_space_handler_internal(device, space_id,
							   handler, setup,
							   context, FALSE);
}

ACPI_EXPORT_SYMBOL(acpi_install_address_space_handler_no_reg)

 
acpi_status
acpi_remove_address_space_handler(acpi_handle device,
				  acpi_adr_space_type space_id,
				  acpi_adr_space_handler handler)
{
	union acpi_operand_object *obj_desc;
	union acpi_operand_object *handler_obj;
	union acpi_operand_object *region_obj;
	union acpi_operand_object **last_obj_ptr;
	struct acpi_namespace_node *node;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_remove_address_space_handler);

	 

	if (!device) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	 

	node = acpi_ns_validate_handle(device);
	if (!node ||
	    ((node->type != ACPI_TYPE_DEVICE) &&
	     (node->type != ACPI_TYPE_PROCESSOR) &&
	     (node->type != ACPI_TYPE_THERMAL) &&
	     (node != acpi_gbl_root_node))) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	 

	obj_desc = acpi_ns_get_attached_object(node);
	if (!obj_desc) {
		status = AE_NOT_EXIST;
		goto unlock_and_exit;
	}

	 

	handler_obj = obj_desc->common_notify.handler;
	last_obj_ptr = &obj_desc->common_notify.handler;
	while (handler_obj) {

		 

		if (handler_obj->address_space.space_id == space_id) {

			 

			if (handler_obj->address_space.handler != handler) {
				status = AE_BAD_PARAMETER;
				goto unlock_and_exit;
			}

			 

			ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
					  "Removing address handler %p(%p) for region %s "
					  "on Device %p(%p)\n",
					  handler_obj, handler,
					  acpi_ut_get_region_name(space_id),
					  node, obj_desc));

			region_obj = handler_obj->address_space.region_list;

			 

			while (region_obj) {
				 
				acpi_ev_detach_region(region_obj, TRUE);

				 
				region_obj =
				    handler_obj->address_space.region_list;
			}

			 

			*last_obj_ptr = handler_obj->address_space.next;

			 

			acpi_os_release_mutex(handler_obj->address_space.
					      context_mutex);
			acpi_ut_remove_reference(handler_obj);
			goto unlock_and_exit;
		}

		 

		last_obj_ptr = &handler_obj->address_space.next;
		handler_obj = handler_obj->address_space.next;
	}

	 

	ACPI_DEBUG_PRINT((ACPI_DB_OPREGION,
			  "Unable to remove address handler %p for %s(%X), DevNode %p, obj %p\n",
			  handler, acpi_ut_get_region_name(space_id), space_id,
			  node, obj_desc));

	status = AE_NOT_EXIST;

unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_remove_address_space_handler)
 
acpi_status
acpi_execute_reg_methods(acpi_handle device, acpi_adr_space_type space_id)
{
	struct acpi_namespace_node *node;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_execute_reg_methods);

	 

	if (!device) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	 

	node = acpi_ns_validate_handle(device);
	if (node) {

		 

		acpi_ev_execute_reg_methods(node, space_id, ACPI_REG_CONNECT);
	} else {
		status = AE_BAD_PARAMETER;
	}

	(void)acpi_ut_release_mutex(ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_execute_reg_methods)
