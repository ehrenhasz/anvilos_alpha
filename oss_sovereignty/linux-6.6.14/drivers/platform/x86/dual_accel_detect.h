#include <linux/acpi.h>
#include <linux/i2c.h>
static bool dual_accel_detect_bosc0200(void)
{
	struct acpi_device *adev;
	int count;
	adev = acpi_dev_get_first_match_dev("BOSC0200", NULL, -1);
	if (!adev)
		return false;
	count = i2c_acpi_client_count(adev);
	acpi_dev_put(adev);
	return count == 2;
}
static bool dual_accel_detect(void)
{
	if (acpi_dev_present("KIOX010A", NULL, -1) &&
	    acpi_dev_present("KIOX020A", NULL, -1))
		return true;
	if (acpi_dev_present("DUAL250E", NULL, -1))
		return true;
	if (dual_accel_detect_bosc0200())
		return true;
	return false;
}
