 

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/uuid.h>

#include "i2c-hid.h"

struct i2c_hid_acpi {
	struct i2chid_ops ops;
	struct acpi_device *adev;
};

static const struct acpi_device_id i2c_hid_acpi_blacklist[] = {
	 
	{ "CHPN0001" },
	 
	{ "IDEA5002" },
	{ }
};

 
static guid_t i2c_hid_guid =
	GUID_INIT(0x3CDFF6F7, 0x4267, 0x4555,
		  0xAD, 0x05, 0xB3, 0x0A, 0x3D, 0x89, 0x38, 0xDE);

static int i2c_hid_acpi_get_descriptor(struct i2c_hid_acpi *ihid_acpi)
{
	struct acpi_device *adev = ihid_acpi->adev;
	acpi_handle handle = acpi_device_handle(adev);
	union acpi_object *obj;
	u16 hid_descriptor_address;

	if (acpi_match_device_ids(adev, i2c_hid_acpi_blacklist) == 0)
		return -ENODEV;

	obj = acpi_evaluate_dsm_typed(handle, &i2c_hid_guid, 1, 1, NULL,
				      ACPI_TYPE_INTEGER);
	if (!obj) {
		acpi_handle_err(handle, "Error _DSM call to get HID descriptor address failed\n");
		return -ENODEV;
	}

	hid_descriptor_address = obj->integer.value;
	ACPI_FREE(obj);

	return hid_descriptor_address;
}

static void i2c_hid_acpi_shutdown_tail(struct i2chid_ops *ops)
{
	struct i2c_hid_acpi *ihid_acpi = container_of(ops, struct i2c_hid_acpi, ops);

	acpi_device_set_power(ihid_acpi->adev, ACPI_STATE_D3_COLD);
}

static int i2c_hid_acpi_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct i2c_hid_acpi *ihid_acpi;
	u16 hid_descriptor_address;
	int ret;

	ihid_acpi = devm_kzalloc(&client->dev, sizeof(*ihid_acpi), GFP_KERNEL);
	if (!ihid_acpi)
		return -ENOMEM;

	ihid_acpi->adev = ACPI_COMPANION(dev);
	ihid_acpi->ops.shutdown_tail = i2c_hid_acpi_shutdown_tail;

	ret = i2c_hid_acpi_get_descriptor(ihid_acpi);
	if (ret < 0)
		return ret;
	hid_descriptor_address = ret;

	acpi_device_fix_up_power(ihid_acpi->adev);

	return i2c_hid_core_probe(client, &ihid_acpi->ops,
				  hid_descriptor_address, 0);
}

static const struct acpi_device_id i2c_hid_acpi_match[] = {
	{ "ACPI0C50" },
	{ "PNP0C50" },
	{ }
};
MODULE_DEVICE_TABLE(acpi, i2c_hid_acpi_match);

static struct i2c_driver i2c_hid_acpi_driver = {
	.driver = {
		.name	= "i2c_hid_acpi",
		.pm	= &i2c_hid_core_pm,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.acpi_match_table = i2c_hid_acpi_match,
	},

	.probe		= i2c_hid_acpi_probe,
	.remove		= i2c_hid_core_remove,
	.shutdown	= i2c_hid_core_shutdown,
};

module_i2c_driver(i2c_hid_acpi_driver);

MODULE_DESCRIPTION("HID over I2C ACPI driver");
MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@gmail.com>");
MODULE_LICENSE("GPL");
