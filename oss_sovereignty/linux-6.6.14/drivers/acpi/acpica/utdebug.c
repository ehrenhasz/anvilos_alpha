
 

#define EXPORT_ACPI_INTERFACES

#include <acpi/acpi.h>
#include "accommon.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utdebug")

#ifdef ACPI_DEBUG_OUTPUT
static acpi_thread_id acpi_gbl_previous_thread_id = (acpi_thread_id) 0xFFFFFFFF;
static const char *acpi_gbl_function_entry_prefix = "----Entry";
static const char *acpi_gbl_function_exit_prefix = "----Exit-";

 

void acpi_ut_init_stack_ptr_trace(void)
{
	acpi_size current_sp;

#pragma GCC diagnostic push
#if defined(__GNUC__) && __GNUC__ >= 12
#pragma GCC diagnostic ignored "-Wdangling-pointer="
#endif
	acpi_gbl_entry_stack_pointer = &current_sp;
#pragma GCC diagnostic pop
}

 

void acpi_ut_track_stack_ptr(void)
{
	acpi_size current_sp;

	if (&current_sp < acpi_gbl_lowest_stack_pointer) {
		acpi_gbl_lowest_stack_pointer = &current_sp;
	}

	if (acpi_gbl_nesting_level > acpi_gbl_deepest_nesting) {
		acpi_gbl_deepest_nesting = acpi_gbl_nesting_level;
	}
}

 

static const char *acpi_ut_trim_function_name(const char *function_name)
{

	 

	if (*(ACPI_CAST_PTR(u32, function_name)) == ACPI_PREFIX_MIXED) {

		 

		return (function_name + 4);
	}

	if (*(ACPI_CAST_PTR(u32, function_name)) == ACPI_PREFIX_LOWER) {

		 

		return (function_name + 5);
	}

	return (function_name);
}

 

void ACPI_INTERNAL_VAR_XFACE
acpi_debug_print(u32 requested_debug_level,
		 u32 line_number,
		 const char *function_name,
		 const char *module_name,
		 u32 component_id, const char *format, ...)
{
	acpi_thread_id thread_id;
	va_list args;
#ifdef ACPI_APPLICATION
	int fill_count;
#endif

	 

	if (!ACPI_IS_DEBUG_ENABLED(requested_debug_level, component_id)) {
		return;
	}

	 
	thread_id = acpi_os_get_thread_id();
	if (thread_id != acpi_gbl_previous_thread_id) {
		if (ACPI_LV_THREADS & acpi_dbg_level) {
			acpi_os_printf
			    ("\n**** Context Switch from TID %u to TID %u ****\n\n",
			     (u32)acpi_gbl_previous_thread_id, (u32)thread_id);
		}

		acpi_gbl_previous_thread_id = thread_id;
		acpi_gbl_nesting_level = 0;
	}

	 
	acpi_os_printf("%9s-%04d ", module_name, line_number);

#ifdef ACPI_APPLICATION
	 
	if (ACPI_LV_THREADS & acpi_dbg_level) {
		acpi_os_printf("[%u] ", (u32)thread_id);
	}

	fill_count = 48 - acpi_gbl_nesting_level -
	    strlen(acpi_ut_trim_function_name(function_name));
	if (fill_count < 0) {
		fill_count = 0;
	}

	acpi_os_printf("[%02d] %*s",
		       acpi_gbl_nesting_level, acpi_gbl_nesting_level + 1, " ");
	acpi_os_printf("%s%*s: ",
		       acpi_ut_trim_function_name(function_name), fill_count,
		       " ");

#else
	acpi_os_printf("%-22.22s: ", acpi_ut_trim_function_name(function_name));
#endif

	va_start(args, format);
	acpi_os_vprintf(format, args);
	va_end(args);
}

ACPI_EXPORT_SYMBOL(acpi_debug_print)

 
void ACPI_INTERNAL_VAR_XFACE
acpi_debug_print_raw(u32 requested_debug_level,
		     u32 line_number,
		     const char *function_name,
		     const char *module_name,
		     u32 component_id, const char *format, ...)
{
	va_list args;

	 

	if (!ACPI_IS_DEBUG_ENABLED(requested_debug_level, component_id)) {
		return;
	}

	va_start(args, format);
	acpi_os_vprintf(format, args);
	va_end(args);
}

ACPI_EXPORT_SYMBOL(acpi_debug_print_raw)

 
void
acpi_ut_trace(u32 line_number,
	      const char *function_name,
	      const char *module_name, u32 component_id)
{

	acpi_gbl_nesting_level++;
	acpi_ut_track_stack_ptr();

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		acpi_debug_print(ACPI_LV_FUNCTIONS,
				 line_number, function_name, module_name,
				 component_id, "%s\n",
				 acpi_gbl_function_entry_prefix);
	}
}

ACPI_EXPORT_SYMBOL(acpi_ut_trace)

 
void
acpi_ut_trace_ptr(u32 line_number,
		  const char *function_name,
		  const char *module_name,
		  u32 component_id, const void *pointer)
{

	acpi_gbl_nesting_level++;
	acpi_ut_track_stack_ptr();

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		acpi_debug_print(ACPI_LV_FUNCTIONS,
				 line_number, function_name, module_name,
				 component_id, "%s %p\n",
				 acpi_gbl_function_entry_prefix, pointer);
	}
}

 

void
acpi_ut_trace_str(u32 line_number,
		  const char *function_name,
		  const char *module_name, u32 component_id, const char *string)
{

	acpi_gbl_nesting_level++;
	acpi_ut_track_stack_ptr();

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		acpi_debug_print(ACPI_LV_FUNCTIONS,
				 line_number, function_name, module_name,
				 component_id, "%s %s\n",
				 acpi_gbl_function_entry_prefix, string);
	}
}

 

void
acpi_ut_trace_u32(u32 line_number,
		  const char *function_name,
		  const char *module_name, u32 component_id, u32 integer)
{

	acpi_gbl_nesting_level++;
	acpi_ut_track_stack_ptr();

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		acpi_debug_print(ACPI_LV_FUNCTIONS,
				 line_number, function_name, module_name,
				 component_id, "%s %08X\n",
				 acpi_gbl_function_entry_prefix, integer);
	}
}

 

void
acpi_ut_exit(u32 line_number,
	     const char *function_name,
	     const char *module_name, u32 component_id)
{

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		acpi_debug_print(ACPI_LV_FUNCTIONS,
				 line_number, function_name, module_name,
				 component_id, "%s\n",
				 acpi_gbl_function_exit_prefix);
	}

	if (acpi_gbl_nesting_level) {
		acpi_gbl_nesting_level--;
	}
}

ACPI_EXPORT_SYMBOL(acpi_ut_exit)

 
void
acpi_ut_status_exit(u32 line_number,
		    const char *function_name,
		    const char *module_name,
		    u32 component_id, acpi_status status)
{

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		if (ACPI_SUCCESS(status)) {
			acpi_debug_print(ACPI_LV_FUNCTIONS,
					 line_number, function_name,
					 module_name, component_id, "%s %s\n",
					 acpi_gbl_function_exit_prefix,
					 acpi_format_exception(status));
		} else {
			acpi_debug_print(ACPI_LV_FUNCTIONS,
					 line_number, function_name,
					 module_name, component_id,
					 "%s ****Exception****: %s\n",
					 acpi_gbl_function_exit_prefix,
					 acpi_format_exception(status));
		}
	}

	if (acpi_gbl_nesting_level) {
		acpi_gbl_nesting_level--;
	}
}

ACPI_EXPORT_SYMBOL(acpi_ut_status_exit)

 
void
acpi_ut_value_exit(u32 line_number,
		   const char *function_name,
		   const char *module_name, u32 component_id, u64 value)
{

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		acpi_debug_print(ACPI_LV_FUNCTIONS,
				 line_number, function_name, module_name,
				 component_id, "%s %8.8X%8.8X\n",
				 acpi_gbl_function_exit_prefix,
				 ACPI_FORMAT_UINT64(value));
	}

	if (acpi_gbl_nesting_level) {
		acpi_gbl_nesting_level--;
	}
}

ACPI_EXPORT_SYMBOL(acpi_ut_value_exit)

 
void
acpi_ut_ptr_exit(u32 line_number,
		 const char *function_name,
		 const char *module_name, u32 component_id, u8 *ptr)
{

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		acpi_debug_print(ACPI_LV_FUNCTIONS,
				 line_number, function_name, module_name,
				 component_id, "%s %p\n",
				 acpi_gbl_function_exit_prefix, ptr);
	}

	if (acpi_gbl_nesting_level) {
		acpi_gbl_nesting_level--;
	}
}

 

void
acpi_ut_str_exit(u32 line_number,
		 const char *function_name,
		 const char *module_name, u32 component_id, const char *string)
{

	 

	if (ACPI_IS_DEBUG_ENABLED(ACPI_LV_FUNCTIONS, component_id)) {
		acpi_debug_print(ACPI_LV_FUNCTIONS,
				 line_number, function_name, module_name,
				 component_id, "%s %s\n",
				 acpi_gbl_function_exit_prefix, string);
	}

	if (acpi_gbl_nesting_level) {
		acpi_gbl_nesting_level--;
	}
}

 

void
acpi_trace_point(acpi_trace_event_type type, u8 begin, u8 *aml, char *pathname)
{

	ACPI_FUNCTION_ENTRY();

	acpi_ex_trace_point(type, begin, aml, pathname);

#ifdef ACPI_USE_SYSTEM_TRACER
	acpi_os_trace_point(type, begin, aml, pathname);
#endif
}

ACPI_EXPORT_SYMBOL(acpi_trace_point)

#endif
