
 

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/acpi.h>
#include <linux/pci-acpi.h>
#include <linux/slab.h>

#define MY_NAME	"acpi_pcihp"

#define dbg(fmt, arg...) do { if (debug_acpi) printk(KERN_DEBUG "%s: %s: " fmt, MY_NAME, __func__, ## arg); } while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format, MY_NAME, ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format, MY_NAME, ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format, MY_NAME, ## arg)

#define	METHOD_NAME__SUN	"_SUN"
#define	METHOD_NAME_OSHP	"OSHP"

static bool debug_acpi;

 
static acpi_status acpi_run_oshp(acpi_handle handle)
{
	acpi_status		status;
	struct acpi_buffer	string = { ACPI_ALLOCATE_BUFFER, NULL };

	acpi_get_name(handle, ACPI_FULL_PATHNAME, &string);

	 
	status = acpi_evaluate_object(handle, METHOD_NAME_OSHP, NULL, NULL);
	if (ACPI_FAILURE(status))
		if (status != AE_NOT_FOUND)
			printk(KERN_ERR "%s:%s OSHP fails=0x%x\n",
			       __func__, (char *)string.pointer, status);
		else
			dbg("%s:%s OSHP not found\n",
			    __func__, (char *)string.pointer);
	else
		pr_debug("%s:%s OSHP passes\n", __func__,
			(char *)string.pointer);

	kfree(string.pointer);
	return status;
}

 
int acpi_get_hp_hw_control_from_firmware(struct pci_dev *pdev)
{
	const struct pci_host_bridge *host;
	const struct acpi_pci_root *root;
	acpi_status status;
	acpi_handle chandle, handle;
	struct acpi_buffer string = { ACPI_ALLOCATE_BUFFER, NULL };

	 
	host = pci_find_host_bridge(pdev->bus);
	root = acpi_pci_find_root(ACPI_HANDLE(&host->dev));
	if (!root)
		return 0;

	 
	if (root->osc_support_set) {
		if (host->native_shpc_hotplug)
			return 0;
		return -ENODEV;
	}

	 
	handle = ACPI_HANDLE(&pdev->dev);
	if (!handle) {
		 
		struct pci_bus *pbus;
		for (pbus = pdev->bus; pbus; pbus = pbus->parent) {
			handle = acpi_pci_get_bridge_handle(pbus);
			if (handle)
				break;
		}
	}

	while (handle) {
		acpi_get_name(handle, ACPI_FULL_PATHNAME, &string);
		pci_info(pdev, "Requesting control of SHPC hotplug via OSHP (%s)\n",
			 (char *)string.pointer);
		status = acpi_run_oshp(handle);
		if (ACPI_SUCCESS(status))
			goto got_one;
		if (acpi_is_root_bridge(handle))
			break;
		chandle = handle;
		status = acpi_get_parent(chandle, &handle);
		if (ACPI_FAILURE(status))
			break;
	}

	pci_info(pdev, "Cannot get control of SHPC hotplug\n");
	kfree(string.pointer);
	return -ENODEV;
got_one:
	pci_info(pdev, "Gained control of SHPC hotplug (%s)\n",
		 (char *)string.pointer);
	kfree(string.pointer);
	return 0;
}
EXPORT_SYMBOL(acpi_get_hp_hw_control_from_firmware);

static int pcihp_is_ejectable(acpi_handle handle)
{
	acpi_status status;
	unsigned long long removable;
	if (!acpi_has_method(handle, "_ADR"))
		return 0;
	if (acpi_has_method(handle, "_EJ0"))
		return 1;
	status = acpi_evaluate_integer(handle, "_RMV", NULL, &removable);
	if (ACPI_SUCCESS(status) && removable)
		return 1;
	return 0;
}

 
int acpi_pci_check_ejectable(struct pci_bus *pbus, acpi_handle handle)
{
	acpi_handle bridge_handle, parent_handle;

	bridge_handle = acpi_pci_get_bridge_handle(pbus);
	if (!bridge_handle)
		return 0;
	if ((ACPI_FAILURE(acpi_get_parent(handle, &parent_handle))))
		return 0;
	if (bridge_handle != parent_handle)
		return 0;
	return pcihp_is_ejectable(handle);
}
EXPORT_SYMBOL_GPL(acpi_pci_check_ejectable);

static acpi_status
check_hotplug(acpi_handle handle, u32 lvl, void *context, void **rv)
{
	int *found = (int *)context;
	if (pcihp_is_ejectable(handle)) {
		*found = 1;
		return AE_CTRL_TERMINATE;
	}
	return AE_OK;
}

 
int acpi_pci_detect_ejectable(acpi_handle handle)
{
	int found = 0;

	if (!handle)
		return found;

	acpi_walk_namespace(ACPI_TYPE_DEVICE, handle, 1,
			    check_hotplug, NULL, (void *)&found, NULL);
	return found;
}
EXPORT_SYMBOL_GPL(acpi_pci_detect_ejectable);

module_param(debug_acpi, bool, 0644);
MODULE_PARM_DESC(debug_acpi, "Debugging mode for ACPI enabled or not");
