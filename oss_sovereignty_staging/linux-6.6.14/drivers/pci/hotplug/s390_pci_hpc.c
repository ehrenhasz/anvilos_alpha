
 

#define KMSG_COMPONENT "zpci"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <asm/pci_debug.h>
#include <asm/sclp.h>

#define SLOT_NAME_SIZE	10

static int enable_slot(struct hotplug_slot *hotplug_slot)
{
	struct zpci_dev *zdev = container_of(hotplug_slot, struct zpci_dev,
					     hotplug_slot);
	int rc;

	if (zdev->state != ZPCI_FN_STATE_STANDBY)
		return -EIO;

	rc = sclp_pci_configure(zdev->fid);
	zpci_dbg(3, "conf fid:%x, rc:%d\n", zdev->fid, rc);
	if (rc)
		return rc;
	zdev->state = ZPCI_FN_STATE_CONFIGURED;

	return zpci_scan_configured_device(zdev, zdev->fh);
}

static int disable_slot(struct hotplug_slot *hotplug_slot)
{
	struct zpci_dev *zdev = container_of(hotplug_slot, struct zpci_dev,
					     hotplug_slot);
	struct pci_dev *pdev;

	if (zdev->state != ZPCI_FN_STATE_CONFIGURED)
		return -EIO;

	pdev = pci_get_slot(zdev->zbus->bus, zdev->devfn);
	if (pdev && pci_num_vf(pdev)) {
		pci_dev_put(pdev);
		return -EBUSY;
	}
	pci_dev_put(pdev);

	return zpci_deconfigure_device(zdev);
}

static int reset_slot(struct hotplug_slot *hotplug_slot, bool probe)
{
	struct zpci_dev *zdev = container_of(hotplug_slot, struct zpci_dev,
					     hotplug_slot);

	if (zdev->state != ZPCI_FN_STATE_CONFIGURED)
		return -EIO;
	 

	 
	if (probe)
		return 0;

	return zpci_hot_reset_device(zdev);
}

static int get_power_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct zpci_dev *zdev = container_of(hotplug_slot, struct zpci_dev,
					     hotplug_slot);

	*value = zpci_is_device_configured(zdev) ? 1 : 0;
	return 0;
}

static int get_adapter_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	 
	*value = 1;
	return 0;
}

static const struct hotplug_slot_ops s390_hotplug_slot_ops = {
	.enable_slot =		enable_slot,
	.disable_slot =		disable_slot,
	.reset_slot =		reset_slot,
	.get_power_status =	get_power_status,
	.get_adapter_status =	get_adapter_status,
};

int zpci_init_slot(struct zpci_dev *zdev)
{
	char name[SLOT_NAME_SIZE];
	struct zpci_bus *zbus = zdev->zbus;

	zdev->hotplug_slot.ops = &s390_hotplug_slot_ops;

	snprintf(name, SLOT_NAME_SIZE, "%08x", zdev->fid);
	return pci_hp_register(&zdev->hotplug_slot, zbus->bus,
			       zdev->devfn, name);
}

void zpci_exit_slot(struct zpci_dev *zdev)
{
	pci_hp_deregister(&zdev->hotplug_slot);
}
