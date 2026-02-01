
 

#include <linux/acpi.h>
#include <linux/efi.h>
#include <linux/pci.h>

static long __init parse_acpi_path(const struct efi_dev_path *node,
				   struct device *parent, struct device **child)
{
	struct acpi_device *adev;
	struct device *phys_dev;
	char hid[ACPI_ID_LEN];
	u64 uid;
	int ret;

	if (node->header.length != 12)
		return -EINVAL;

	sprintf(hid, "%c%c%c%04X",
		'A' + ((node->acpi.hid >> 10) & 0x1f) - 1,
		'A' + ((node->acpi.hid >>  5) & 0x1f) - 1,
		'A' + ((node->acpi.hid >>  0) & 0x1f) - 1,
			node->acpi.hid >> 16);

	for_each_acpi_dev_match(adev, hid, NULL, -1) {
		ret = acpi_dev_uid_to_integer(adev, &uid);
		if (ret == 0 && node->acpi.uid == uid)
			break;
		if (ret == -ENODATA && node->acpi.uid == 0)
			break;
	}
	if (!adev)
		return -ENODEV;

	phys_dev = acpi_get_first_physical_node(adev);
	if (phys_dev) {
		*child = get_device(phys_dev);
		acpi_dev_put(adev);
	} else
		*child = &adev->dev;

	return 0;
}

static int __init match_pci_dev(struct device *dev, void *data)
{
	unsigned int devfn = *(unsigned int *)data;

	return dev_is_pci(dev) && to_pci_dev(dev)->devfn == devfn;
}

static long __init parse_pci_path(const struct efi_dev_path *node,
				  struct device *parent, struct device **child)
{
	unsigned int devfn;

	if (node->header.length != 6)
		return -EINVAL;
	if (!parent)
		return -EINVAL;

	devfn = PCI_DEVFN(node->pci.dev, node->pci.fn);

	*child = device_find_child(parent, &devfn, match_pci_dev);
	if (!*child)
		return -ENODEV;

	return 0;
}

 

static long __init parse_end_path(const struct efi_dev_path *node,
				  struct device *parent, struct device **child)
{
	if (node->header.length != 4)
		return -EINVAL;
	if (node->header.sub_type != EFI_DEV_END_INSTANCE &&
	    node->header.sub_type != EFI_DEV_END_ENTIRE)
		return -EINVAL;
	if (!parent)
		return -ENODEV;

	*child = get_device(parent);
	return node->header.sub_type;
}

 
struct device * __init efi_get_device_by_path(const struct efi_dev_path **node,
					      size_t *len)
{
	struct device *parent = NULL, *child;
	long ret = 0;

	if (!*len)
		return NULL;

	while (!ret) {
		if (*len < 4 || *len < (*node)->header.length)
			ret = -EINVAL;
		else if ((*node)->header.type		== EFI_DEV_ACPI &&
			 (*node)->header.sub_type	== EFI_DEV_BASIC_ACPI)
			ret = parse_acpi_path(*node, parent, &child);
		else if ((*node)->header.type		== EFI_DEV_HW &&
			 (*node)->header.sub_type	== EFI_DEV_PCI)
			ret = parse_pci_path(*node, parent, &child);
		else if (((*node)->header.type		== EFI_DEV_END_PATH ||
			  (*node)->header.type		== EFI_DEV_END_PATH2))
			ret = parse_end_path(*node, parent, &child);
		else
			ret = -ENOTSUPP;

		put_device(parent);
		if (ret < 0)
			return ERR_PTR(ret);

		parent = child;
		*node  = (void *)*node + (*node)->header.length;
		*len  -= (*node)->header.length;
	}

	if (ret == EFI_DEV_END_ENTIRE)
		*len = 0;

	return child;
}
