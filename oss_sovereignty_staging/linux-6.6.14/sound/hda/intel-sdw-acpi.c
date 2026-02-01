


 

#include <linux/acpi.h>
#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/fwnode.h>
#include <linux/module.h>
#include <linux/soundwire/sdw_intel.h>
#include <linux/string.h>

#define SDW_LINK_TYPE		4  
#define SDW_MAX_LINKS		4

static int ctrl_link_mask;
module_param_named(sdw_link_mask, ctrl_link_mask, int, 0444);
MODULE_PARM_DESC(sdw_link_mask, "Intel link mask (one bit per link)");

static bool is_link_enabled(struct fwnode_handle *fw_node, u8 idx)
{
	struct fwnode_handle *link;
	char name[32];
	u32 quirk_mask = 0;

	 
	snprintf(name, sizeof(name),
		 "mipi-sdw-link-%hhu-subproperties", idx);

	link = fwnode_get_named_child_node(fw_node, name);
	if (!link)
		return false;

	fwnode_property_read_u32(link,
				 "intel-quirk-mask",
				 &quirk_mask);

	if (quirk_mask & SDW_INTEL_QUIRK_MASK_BUS_DISABLE)
		return false;

	return true;
}

static int
sdw_intel_scan_controller(struct sdw_intel_acpi_info *info)
{
	struct acpi_device *adev = acpi_fetch_acpi_dev(info->handle);
	u8 count, i;
	int ret;

	if (!adev)
		return -EINVAL;

	 
	count = 0;
	ret = fwnode_property_read_u8_array(acpi_fwnode_handle(adev),
					    "mipi-sdw-master-count", &count, 1);

	 

	if (ret) {
		dev_err(&adev->dev,
			"Failed to read mipi-sdw-master-count: %d\n", ret);
		return -EINVAL;
	}

	 
	if (count > SDW_MAX_LINKS) {
		dev_err(&adev->dev, "Link count %d exceeds max %d\n",
			count, SDW_MAX_LINKS);
		return -EINVAL;
	}

	if (!count) {
		dev_warn(&adev->dev, "No SoundWire links detected\n");
		return -EINVAL;
	}
	dev_dbg(&adev->dev, "ACPI reports %d SDW Link devices\n", count);

	info->count = count;
	info->link_mask = 0;

	for (i = 0; i < count; i++) {
		if (ctrl_link_mask && !(ctrl_link_mask & BIT(i))) {
			dev_dbg(&adev->dev,
				"Link %d masked, will not be enabled\n", i);
			continue;
		}

		if (!is_link_enabled(acpi_fwnode_handle(adev), i)) {
			dev_dbg(&adev->dev,
				"Link %d not selected in firmware\n", i);
			continue;
		}

		info->link_mask |= BIT(i);
	}

	return 0;
}

static acpi_status sdw_intel_acpi_cb(acpi_handle handle, u32 level,
				     void *cdata, void **return_value)
{
	struct sdw_intel_acpi_info *info = cdata;
	acpi_status status;
	u64 adr;

	status = acpi_evaluate_integer(handle, METHOD_NAME__ADR, NULL, &adr);
	if (ACPI_FAILURE(status))
		return AE_OK;  

	if (!acpi_fetch_acpi_dev(handle)) {
		pr_err("%s: Couldn't find ACPI handle\n", __func__);
		return AE_NOT_FOUND;
	}

	 
	if (FIELD_GET(GENMASK(31, 28), adr) != SDW_LINK_TYPE)
		return AE_OK;  

	 
	info->handle = handle;

	 
	return AE_CTRL_TERMINATE;
}

 
int sdw_intel_acpi_scan(acpi_handle *parent_handle,
			struct sdw_intel_acpi_info *info)
{
	acpi_status status;

	info->handle = NULL;
	 
	status = acpi_walk_namespace(ACPI_TYPE_DEVICE,
				     parent_handle, 2,
				     sdw_intel_acpi_cb,
				     NULL, info, NULL);
	if (ACPI_FAILURE(status) || info->handle == NULL)
		return -ENODEV;

	return sdw_intel_scan_controller(info);
}
EXPORT_SYMBOL_NS(sdw_intel_acpi_scan, SND_INTEL_SOUNDWIRE_ACPI);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Intel Soundwire ACPI helpers");
