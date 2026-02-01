
 

 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <asm/unaligned.h>
#include <asm/byteorder.h>

#include <linux/hid.h>

static struct hid_driver hid_generic;

static int __check_hid_generic(struct device_driver *drv, void *data)
{
	struct hid_driver *hdrv = to_hid_driver(drv);
	struct hid_device *hdev = data;

	if (hdrv == &hid_generic)
		return 0;

	return hid_match_device(hdev, hdrv) != NULL;
}

static bool hid_generic_match(struct hid_device *hdev,
			      bool ignore_special_driver)
{
	if (ignore_special_driver)
		return true;

	if (hdev->quirks & HID_QUIRK_HAVE_SPECIAL_DRIVER)
		return false;

	 
	if (bus_for_each_drv(&hid_bus_type, NULL, hdev, __check_hid_generic))
		return false;

	return true;
}

static int hid_generic_probe(struct hid_device *hdev,
			     const struct hid_device_id *id)
{
	int ret;

	hdev->quirks |= HID_QUIRK_INPUT_PER_APP;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	return hid_hw_start(hdev, HID_CONNECT_DEFAULT);
}

static const struct hid_device_id hid_table[] = {
	{ HID_DEVICE(HID_BUS_ANY, HID_GROUP_ANY, HID_ANY_ID, HID_ANY_ID) },
	{ }
};
MODULE_DEVICE_TABLE(hid, hid_table);

static struct hid_driver hid_generic = {
	.name = "hid-generic",
	.id_table = hid_table,
	.match = hid_generic_match,
	.probe = hid_generic_probe,
};
module_hid_driver(hid_generic);

MODULE_AUTHOR("Henrik Rydberg");
MODULE_DESCRIPTION("HID generic driver");
MODULE_LICENSE("GPL");
