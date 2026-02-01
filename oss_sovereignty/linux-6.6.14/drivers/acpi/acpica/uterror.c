
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("uterror")

 
#if !defined (ACPI_NO_ERROR_MESSAGES)
 
void ACPI_INTERNAL_VAR_XFACE
acpi_ut_predefined_warning(const char *module_name,
			   u32 line_number,
			   char *pathname,
			   u16 node_flags, const char *format, ...)
{
	va_list arg_list;

	 
	if (node_flags & ANOBJ_EVALUATED) {
		return;
	}

	acpi_os_printf(ACPI_MSG_WARNING "%s: ", pathname);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);
}

 

void ACPI_INTERNAL_VAR_XFACE
acpi_ut_predefined_info(const char *module_name,
			u32 line_number,
			char *pathname, u16 node_flags, const char *format, ...)
{
	va_list arg_list;

	 
	if (node_flags & ANOBJ_EVALUATED) {
		return;
	}

	acpi_os_printf(ACPI_MSG_INFO "%s: ", pathname);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);
}

 

void ACPI_INTERNAL_VAR_XFACE
acpi_ut_predefined_bios_error(const char *module_name,
			      u32 line_number,
			      char *pathname,
			      u16 node_flags, const char *format, ...)
{
	va_list arg_list;

	 
	if (node_flags & ANOBJ_EVALUATED) {
		return;
	}

	acpi_os_printf(ACPI_MSG_BIOS_ERROR "%s: ", pathname);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);
}

 

void
acpi_ut_prefixed_namespace_error(const char *module_name,
				 u32 line_number,
				 union acpi_generic_state *prefix_scope,
				 const char *internal_path,
				 acpi_status lookup_status)
{
	char *full_path;
	const char *message;

	 
	switch (lookup_status) {
	case AE_ALREADY_EXISTS:

		acpi_os_printf(ACPI_MSG_BIOS_ERROR);
		message = "Failure creating named object";
		break;

	case AE_NOT_FOUND:

		acpi_os_printf(ACPI_MSG_BIOS_ERROR);
		message = "Could not resolve symbol";
		break;

	default:

		acpi_os_printf(ACPI_MSG_ERROR);
		message = "Failure resolving symbol";
		break;
	}

	 

	full_path =
	    acpi_ns_build_prefixed_pathname(prefix_scope, internal_path);

	acpi_os_printf("%s [%s], %s", message,
		       full_path ? full_path : "Could not get pathname",
		       acpi_format_exception(lookup_status));

	if (full_path) {
		ACPI_FREE(full_path);
	}

	ACPI_MSG_SUFFIX;
}

#ifdef __OBSOLETE_FUNCTION
 

void
acpi_ut_namespace_error(const char *module_name,
			u32 line_number,
			const char *internal_name, acpi_status lookup_status)
{
	acpi_status status;
	u32 bad_name;
	char *name = NULL;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_ERROR);

	if (lookup_status == AE_BAD_CHARACTER) {

		 

		ACPI_MOVE_32_TO_32(&bad_name,
				   ACPI_CAST_PTR(u32, internal_name));
		acpi_os_printf("[0x%.8X] (NON-ASCII)", bad_name);
	} else {
		 

		status =
		    acpi_ns_externalize_name(ACPI_UINT32_MAX, internal_name,
					     NULL, &name);

		 

		if (ACPI_SUCCESS(status)) {
			acpi_os_printf("[%s]", name);
		} else {
			acpi_os_printf("[COULD NOT EXTERNALIZE NAME]");
		}

		if (name) {
			ACPI_FREE(name);
		}
	}

	acpi_os_printf(" Namespace lookup failure, %s",
		       acpi_format_exception(lookup_status));

	ACPI_MSG_SUFFIX;
	ACPI_MSG_REDIRECT_END;
}
#endif

 

void
acpi_ut_method_error(const char *module_name,
		     u32 line_number,
		     const char *message,
		     struct acpi_namespace_node *prefix_node,
		     const char *path, acpi_status method_status)
{
	acpi_status status;
	struct acpi_namespace_node *node = prefix_node;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_ERROR);

	if (path) {
		status = acpi_ns_get_node(prefix_node, path,
					  ACPI_NS_NO_UPSEARCH, &node);
		if (ACPI_FAILURE(status)) {
			acpi_os_printf("[Could not get node by pathname]");
		}
	}

	acpi_ns_print_node_pathname(node, message);
	acpi_os_printf(" due to previous error (%s)",
		       acpi_format_exception(method_status));

	ACPI_MSG_SUFFIX;
	ACPI_MSG_REDIRECT_END;
}

#endif				 
