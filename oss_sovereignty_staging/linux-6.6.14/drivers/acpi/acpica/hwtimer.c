
 

#define EXPORT_ACPI_INTERFACES

#include <acpi/acpi.h>
#include "accommon.h"

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwtimer")

#if (!ACPI_REDUCED_HARDWARE)	 
 
acpi_status acpi_get_timer_resolution(u32 * resolution)
{
	ACPI_FUNCTION_TRACE(acpi_get_timer_resolution);

	if (!resolution) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if ((acpi_gbl_FADT.flags & ACPI_FADT_32BIT_TIMER) == 0) {
		*resolution = 24;
	} else {
		*resolution = 32;
	}

	return_ACPI_STATUS(AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_get_timer_resolution)

 
acpi_status acpi_get_timer(u32 * ticks)
{
	acpi_status status;
	u64 timer_value;

	ACPI_FUNCTION_TRACE(acpi_get_timer);

	if (!ticks) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	 

	if (!acpi_gbl_FADT.xpm_timer_block.address) {
		return_ACPI_STATUS(AE_SUPPORT);
	}

	status = acpi_hw_read(&timer_value, &acpi_gbl_FADT.xpm_timer_block);
	if (ACPI_SUCCESS(status)) {

		 

		*ticks = (u32)timer_value;
	}

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_timer)

 
acpi_status
acpi_get_timer_duration(u32 start_ticks, u32 end_ticks, u32 *time_elapsed)
{
	acpi_status status;
	u64 delta_ticks;
	u64 quotient;

	ACPI_FUNCTION_TRACE(acpi_get_timer_duration);

	if (!time_elapsed) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	 

	if (!acpi_gbl_FADT.xpm_timer_block.address) {
		return_ACPI_STATUS(AE_SUPPORT);
	}

	if (start_ticks == end_ticks) {
		*time_elapsed = 0;
		return_ACPI_STATUS(AE_OK);
	}

	 
	delta_ticks = end_ticks;
	if (start_ticks > end_ticks) {
		if ((acpi_gbl_FADT.flags & ACPI_FADT_32BIT_TIMER) == 0) {

			 

			delta_ticks |= (u64)1 << 24;
		} else {
			 

			delta_ticks |= (u64)1 << 32;
		}
	}
	delta_ticks -= start_ticks;

	 
	status = acpi_ut_short_divide(delta_ticks * ACPI_USEC_PER_SEC,
				      ACPI_PM_TIMER_FREQUENCY, &quotient, NULL);

	*time_elapsed = (u32)quotient;
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_timer_duration)
#endif				 
