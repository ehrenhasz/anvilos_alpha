
 

#define EXPORT_ACPI_INTERFACES

#include <acpi/acpi.h>
#include "accommon.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utxferror")

 
#ifndef ACPI_NO_ERROR_MESSAGES	 
 
void ACPI_INTERNAL_VAR_XFACE
acpi_error(const char *module_name, u32 line_number, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_ERROR);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_error)

 
void ACPI_INTERNAL_VAR_XFACE
acpi_exception(const char *module_name,
	       u32 line_number, acpi_status status, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;

	 

	if (ACPI_SUCCESS(status)) {
		acpi_os_printf(ACPI_MSG_ERROR);

	} else {
		acpi_os_printf(ACPI_MSG_ERROR "%s, ",
			       acpi_format_exception(status));
	}

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_exception)

 
void ACPI_INTERNAL_VAR_XFACE
acpi_warning(const char *module_name, u32 line_number, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_WARNING);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_warning)

 
void ACPI_INTERNAL_VAR_XFACE acpi_info(const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_INFO);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	acpi_os_printf("\n");
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_info)

 
void ACPI_INTERNAL_VAR_XFACE
acpi_bios_error(const char *module_name,
		u32 line_number, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_BIOS_ERROR);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_bios_error)

 
void ACPI_INTERNAL_VAR_XFACE
acpi_bios_exception(const char *module_name,
		    u32 line_number,
		    acpi_status status, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;

	 

	if (ACPI_SUCCESS(status)) {
		acpi_os_printf(ACPI_MSG_BIOS_ERROR);

	} else {
		acpi_os_printf(ACPI_MSG_BIOS_ERROR "%s, ",
			       acpi_format_exception(status));
	}

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_bios_exception)

 
void ACPI_INTERNAL_VAR_XFACE
acpi_bios_warning(const char *module_name,
		  u32 line_number, const char *format, ...)
{
	va_list arg_list;

	ACPI_MSG_REDIRECT_BEGIN;
	acpi_os_printf(ACPI_MSG_BIOS_WARNING);

	va_start(arg_list, format);
	acpi_os_vprintf(format, arg_list);
	ACPI_MSG_SUFFIX;
	va_end(arg_list);

	ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL(acpi_bios_warning)
#endif				 
