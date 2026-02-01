
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <acpi/hed.h>

static const struct acpi_device_id acpi_hed_ids[] = {
	{"PNP0C33", 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, acpi_hed_ids);

static acpi_handle hed_handle;

static BLOCKING_NOTIFIER_HEAD(acpi_hed_notify_list);

int register_acpi_hed_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&acpi_hed_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_acpi_hed_notifier);

void unregister_acpi_hed_notifier(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&acpi_hed_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_acpi_hed_notifier);

 
static void acpi_hed_notify(acpi_handle handle, u32 event, void *data)
{
	blocking_notifier_call_chain(&acpi_hed_notify_list, 0, NULL);
}

static int acpi_hed_add(struct acpi_device *device)
{
	int err;

	 
	if (hed_handle)
		return -EINVAL;
	hed_handle = device->handle;

	err = acpi_dev_install_notify_handler(device, ACPI_DEVICE_NOTIFY,
					      acpi_hed_notify);
	if (err)
		hed_handle = NULL;

	return err;
}

static void acpi_hed_remove(struct acpi_device *device)
{
	acpi_dev_remove_notify_handler(device, ACPI_DEVICE_NOTIFY,
				       acpi_hed_notify);
	hed_handle = NULL;
}

static struct acpi_driver acpi_hed_driver = {
	.name = "hardware_error_device",
	.class = "hardware_error",
	.ids = acpi_hed_ids,
	.ops = {
		.add = acpi_hed_add,
		.remove = acpi_hed_remove,
	},
};
module_acpi_driver(acpi_hed_driver);

MODULE_AUTHOR("Huang Ying");
MODULE_DESCRIPTION("ACPI Hardware Error Device Driver");
MODULE_LICENSE("GPL");
