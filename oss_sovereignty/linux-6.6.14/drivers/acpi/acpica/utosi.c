
 

#include <acpi/acpi.h>
#include "accommon.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utosi")

 
 
static struct acpi_interface_info acpi_default_supported_interfaces[] = {
	 

	{"Windows 2000", NULL, 0, ACPI_OSI_WIN_2000},	 
	{"Windows 2001", NULL, 0, ACPI_OSI_WIN_XP},	 
	{"Windows 2001 SP1", NULL, 0, ACPI_OSI_WIN_XP_SP1},	 
	{"Windows 2001.1", NULL, 0, ACPI_OSI_WINSRV_2003},	 
	{"Windows 2001 SP2", NULL, 0, ACPI_OSI_WIN_XP_SP2},	 
	{"Windows 2001.1 SP1", NULL, 0, ACPI_OSI_WINSRV_2003_SP1},	 
	{"Windows 2006", NULL, 0, ACPI_OSI_WIN_VISTA},	 
	{"Windows 2006.1", NULL, 0, ACPI_OSI_WINSRV_2008},	 
	{"Windows 2006 SP1", NULL, 0, ACPI_OSI_WIN_VISTA_SP1},	 
	{"Windows 2006 SP2", NULL, 0, ACPI_OSI_WIN_VISTA_SP2},	 
	{"Windows 2009", NULL, 0, ACPI_OSI_WIN_7},	 
	{"Windows 2012", NULL, 0, ACPI_OSI_WIN_8},	 
	{"Windows 2013", NULL, 0, ACPI_OSI_WIN_8_1},	 
	{"Windows 2015", NULL, 0, ACPI_OSI_WIN_10},	 
	{"Windows 2016", NULL, 0, ACPI_OSI_WIN_10_RS1},	 
	{"Windows 2017", NULL, 0, ACPI_OSI_WIN_10_RS2},	 
	{"Windows 2017.2", NULL, 0, ACPI_OSI_WIN_10_RS3},	 
	{"Windows 2018", NULL, 0, ACPI_OSI_WIN_10_RS4},	 
	{"Windows 2018.2", NULL, 0, ACPI_OSI_WIN_10_RS5},	 
	{"Windows 2019", NULL, 0, ACPI_OSI_WIN_10_19H1},	 
	{"Windows 2020", NULL, 0, ACPI_OSI_WIN_10_20H1},	 
	{"Windows 2021", NULL, 0, ACPI_OSI_WIN_11},	 

	 

	{"Extended Address Space Descriptor", NULL, ACPI_OSI_FEATURE, 0},

	 

	{"Module Device", NULL, ACPI_OSI_OPTIONAL_FEATURE, 0},
	{"Processor Device", NULL, ACPI_OSI_OPTIONAL_FEATURE, 0},
	{"3.0 Thermal Model", NULL, ACPI_OSI_OPTIONAL_FEATURE, 0},
	{"3.0 _SCP Extensions", NULL, ACPI_OSI_OPTIONAL_FEATURE, 0},
	{"Processor Aggregator Device", NULL, ACPI_OSI_OPTIONAL_FEATURE, 0}
};

 

acpi_status acpi_ut_initialize_interfaces(void)
{
	acpi_status status;
	u32 i;

	status = acpi_os_acquire_mutex(acpi_gbl_osi_mutex, ACPI_WAIT_FOREVER);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	acpi_gbl_supported_interfaces = acpi_default_supported_interfaces;

	 

	for (i = 0;
	     i < (ACPI_ARRAY_LENGTH(acpi_default_supported_interfaces) - 1);
	     i++) {
		acpi_default_supported_interfaces[i].next =
		    &acpi_default_supported_interfaces[(acpi_size)i + 1];
	}

	acpi_os_release_mutex(acpi_gbl_osi_mutex);
	return (AE_OK);
}

 

acpi_status acpi_ut_interface_terminate(void)
{
	acpi_status status;
	struct acpi_interface_info *next_interface;

	status = acpi_os_acquire_mutex(acpi_gbl_osi_mutex, ACPI_WAIT_FOREVER);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	next_interface = acpi_gbl_supported_interfaces;
	while (next_interface) {
		acpi_gbl_supported_interfaces = next_interface->next;

		if (next_interface->flags & ACPI_OSI_DYNAMIC) {

			 

			ACPI_FREE(next_interface->name);
			ACPI_FREE(next_interface);
		} else {
			 

			if (next_interface->flags & ACPI_OSI_DEFAULT_INVALID) {
				next_interface->flags |= ACPI_OSI_INVALID;
			} else {
				next_interface->flags &= ~ACPI_OSI_INVALID;
			}
		}

		next_interface = acpi_gbl_supported_interfaces;
	}

	acpi_os_release_mutex(acpi_gbl_osi_mutex);
	return (AE_OK);
}

 

acpi_status acpi_ut_install_interface(acpi_string interface_name)
{
	struct acpi_interface_info *interface_info;

	 

	interface_info =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_interface_info));
	if (!interface_info) {
		return (AE_NO_MEMORY);
	}

	interface_info->name = ACPI_ALLOCATE_ZEROED(strlen(interface_name) + 1);
	if (!interface_info->name) {
		ACPI_FREE(interface_info);
		return (AE_NO_MEMORY);
	}

	 

	strcpy(interface_info->name, interface_name);
	interface_info->flags = ACPI_OSI_DYNAMIC;
	interface_info->next = acpi_gbl_supported_interfaces;

	acpi_gbl_supported_interfaces = interface_info;
	return (AE_OK);
}

 

acpi_status acpi_ut_remove_interface(acpi_string interface_name)
{
	struct acpi_interface_info *previous_interface;
	struct acpi_interface_info *next_interface;

	previous_interface = next_interface = acpi_gbl_supported_interfaces;
	while (next_interface) {
		if (!strcmp(interface_name, next_interface->name)) {
			 
			if (next_interface->flags & ACPI_OSI_DYNAMIC) {

				 

				if (previous_interface == next_interface) {
					acpi_gbl_supported_interfaces =
					    next_interface->next;
				} else {
					previous_interface->next =
					    next_interface->next;
				}

				ACPI_FREE(next_interface->name);
				ACPI_FREE(next_interface);
			} else {
				 
				if (next_interface->flags & ACPI_OSI_INVALID) {
					return (AE_NOT_EXIST);
				}

				next_interface->flags |= ACPI_OSI_INVALID;
			}

			return (AE_OK);
		}

		previous_interface = next_interface;
		next_interface = next_interface->next;
	}

	 

	return (AE_NOT_EXIST);
}

 

acpi_status acpi_ut_update_interfaces(u8 action)
{
	struct acpi_interface_info *next_interface;

	next_interface = acpi_gbl_supported_interfaces;
	while (next_interface) {
		if (((next_interface->flags & ACPI_OSI_FEATURE) &&
		     (action & ACPI_FEATURE_STRINGS)) ||
		    (!(next_interface->flags & ACPI_OSI_FEATURE) &&
		     (action & ACPI_VENDOR_STRINGS))) {
			if (action & ACPI_DISABLE_INTERFACES) {

				 

				next_interface->flags |= ACPI_OSI_INVALID;
			} else {
				 

				next_interface->flags &= ~ACPI_OSI_INVALID;
			}
		}

		next_interface = next_interface->next;
	}

	return (AE_OK);
}

 

struct acpi_interface_info *acpi_ut_get_interface(acpi_string interface_name)
{
	struct acpi_interface_info *next_interface;

	next_interface = acpi_gbl_supported_interfaces;
	while (next_interface) {
		if (!strcmp(interface_name, next_interface->name)) {
			return (next_interface);
		}

		next_interface = next_interface->next;
	}

	return (NULL);
}

 

acpi_status acpi_ut_osi_implementation(struct acpi_walk_state *walk_state)
{
	union acpi_operand_object *string_desc;
	union acpi_operand_object *return_desc;
	struct acpi_interface_info *interface_info;
	acpi_interface_handler interface_handler;
	acpi_status status;
	u64 return_value;

	ACPI_FUNCTION_TRACE(ut_osi_implementation);

	 

	string_desc = walk_state->arguments[0].object;
	if (!string_desc || (string_desc->common.type != ACPI_TYPE_STRING)) {
		return_ACPI_STATUS(AE_TYPE);
	}

	 

	return_desc = acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
	if (!return_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	 

	return_value = 0;
	status = acpi_os_acquire_mutex(acpi_gbl_osi_mutex, ACPI_WAIT_FOREVER);
	if (ACPI_FAILURE(status)) {
		acpi_ut_remove_reference(return_desc);
		return_ACPI_STATUS(status);
	}

	 

	interface_info = acpi_ut_get_interface(string_desc->string.pointer);
	if (interface_info && !(interface_info->flags & ACPI_OSI_INVALID)) {
		 
		if (interface_info->value > acpi_gbl_osi_data) {
			acpi_gbl_osi_data = interface_info->value;
		}

		return_value = ACPI_UINT64_MAX;
	}

	acpi_os_release_mutex(acpi_gbl_osi_mutex);

	 
	interface_handler = acpi_gbl_interface_handler;
	if (interface_handler) {
		if (interface_handler
		    (string_desc->string.pointer, (u32)return_value)) {
			return_value = ACPI_UINT64_MAX;
		}
	}

	ACPI_DEBUG_PRINT_RAW((ACPI_DB_INFO,
			      "ACPI: BIOS _OSI(\"%s\") is %ssupported\n",
			      string_desc->string.pointer,
			      return_value == 0 ? "not " : ""));

	 

	return_desc->integer.value = return_value;
	walk_state->return_desc = return_desc;
	return_ACPI_STATUS(AE_OK);
}
