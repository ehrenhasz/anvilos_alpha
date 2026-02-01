
 
#include <linux/acpi.h>
#include <linux/units.h>

#include "thermal_core.h"

 
#define TEMP_MIN_DECIK	2180
#define TEMP_MAX_DECIK	4480

static int thermal_acpi_trip_temp(struct acpi_device *adev, char *obj_name,
				  int *ret_temp)
{
	unsigned long long temp;
	acpi_status status;

	status = acpi_evaluate_integer(adev->handle, obj_name, NULL, &temp);
	if (ACPI_FAILURE(status)) {
		acpi_handle_debug(adev->handle, "%s evaluation failed\n", obj_name);
		return -ENODATA;
	}

	if (temp >= TEMP_MIN_DECIK && temp <= TEMP_MAX_DECIK) {
		*ret_temp = deci_kelvin_to_millicelsius(temp);
	} else {
		acpi_handle_debug(adev->handle, "%s result %llu out of range\n",
				  obj_name, temp);
		*ret_temp = THERMAL_TEMP_INVALID;
	}

	return 0;
}

 
int thermal_acpi_active_trip_temp(struct acpi_device *adev, int id, int *ret_temp)
{
	char obj_name[] = {'_', 'A', 'C', '0' + id, '\0'};

	if (id < 0 || id > 9)
		return -EINVAL;

	return thermal_acpi_trip_temp(adev, obj_name, ret_temp);
}
EXPORT_SYMBOL_GPL(thermal_acpi_active_trip_temp);

 
int thermal_acpi_passive_trip_temp(struct acpi_device *adev, int *ret_temp)
{
	return thermal_acpi_trip_temp(adev, "_PSV", ret_temp);
}
EXPORT_SYMBOL_GPL(thermal_acpi_passive_trip_temp);

 
int thermal_acpi_hot_trip_temp(struct acpi_device *adev, int *ret_temp)
{
	return thermal_acpi_trip_temp(adev, "_HOT", ret_temp);
}
EXPORT_SYMBOL_GPL(thermal_acpi_hot_trip_temp);

 
int thermal_acpi_critical_trip_temp(struct acpi_device *adev, int *ret_temp)
{
	return thermal_acpi_trip_temp(adev, "_CRT", ret_temp);
}
EXPORT_SYMBOL_GPL(thermal_acpi_critical_trip_temp);
