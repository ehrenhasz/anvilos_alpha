
 

#include <linux/acpi.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/types.h>

#include <linux/surface_aggregator/device.h>


 

 

 
static const struct software_node ssam_node_root = {
	.name = "ssam_platform_hub",
};

 
static const struct software_node ssam_node_hub_kip = {
	.name = "ssam:00:00:01:0e:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hub_base = {
	.name = "ssam:00:00:01:11:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_bat_ac = {
	.name = "ssam:01:02:01:01:01",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_bat_main = {
	.name = "ssam:01:02:01:01:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_bat_sb3base = {
	.name = "ssam:01:02:02:01:00",
	.parent = &ssam_node_hub_base,
};

 
static const struct software_node ssam_node_tmp_pprof = {
	.name = "ssam:01:03:01:00:01",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_kip_tablet_switch = {
	.name = "ssam:01:0e:01:00:01",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_bas_dtx = {
	.name = "ssam:01:11:01:00:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_sam_keyboard = {
	.name = "ssam:01:15:01:01:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_sam_penstash = {
	.name = "ssam:01:15:01:02:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_sam_touchpad = {
	.name = "ssam:01:15:01:03:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_sam_sensors = {
	.name = "ssam:01:15:01:06:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_sam_ucm_ucsi = {
	.name = "ssam:01:15:01:07:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_sam_sysctrl = {
	.name = "ssam:01:15:01:08:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_main_keyboard = {
	.name = "ssam:01:15:02:01:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_main_touchpad = {
	.name = "ssam:01:15:02:03:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_main_iid5 = {
	.name = "ssam:01:15:02:05:00",
	.parent = &ssam_node_root,
};

 
static const struct software_node ssam_node_hid_base_keyboard = {
	.name = "ssam:01:15:02:01:00",
	.parent = &ssam_node_hub_base,
};

 
static const struct software_node ssam_node_hid_base_touchpad = {
	.name = "ssam:01:15:02:03:00",
	.parent = &ssam_node_hub_base,
};

 
static const struct software_node ssam_node_hid_base_iid5 = {
	.name = "ssam:01:15:02:05:00",
	.parent = &ssam_node_hub_base,
};

 
static const struct software_node ssam_node_hid_base_iid6 = {
	.name = "ssam:01:15:02:06:00",
	.parent = &ssam_node_hub_base,
};

 
static const struct software_node ssam_node_hid_kip_keyboard = {
	.name = "ssam:01:15:02:01:00",
	.parent = &ssam_node_hub_kip,
};

 
static const struct software_node ssam_node_hid_kip_penstash = {
	.name = "ssam:01:15:02:02:00",
	.parent = &ssam_node_hub_kip,
};

 
static const struct software_node ssam_node_hid_kip_touchpad = {
	.name = "ssam:01:15:02:03:00",
	.parent = &ssam_node_hub_kip,
};

 
static const struct software_node ssam_node_hid_kip_fwupd = {
	.name = "ssam:01:15:02:05:00",
	.parent = &ssam_node_hub_kip,
};

 
static const struct software_node ssam_node_pos_tablet_switch = {
	.name = "ssam:01:26:01:00:01",
	.parent = &ssam_node_root,
};

 
static const struct software_node *ssam_node_group_gen5[] = {
	&ssam_node_root,
	&ssam_node_tmp_pprof,
	NULL,
};

 
static const struct software_node *ssam_node_group_sb3[] = {
	&ssam_node_root,
	&ssam_node_hub_base,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_bat_sb3base,
	&ssam_node_tmp_pprof,
	&ssam_node_bas_dtx,
	&ssam_node_hid_base_keyboard,
	&ssam_node_hid_base_touchpad,
	&ssam_node_hid_base_iid5,
	&ssam_node_hid_base_iid6,
	NULL,
};

 
static const struct software_node *ssam_node_group_sl3[] = {
	&ssam_node_root,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_tmp_pprof,
	&ssam_node_hid_main_keyboard,
	&ssam_node_hid_main_touchpad,
	&ssam_node_hid_main_iid5,
	NULL,
};

 
static const struct software_node *ssam_node_group_sl5[] = {
	&ssam_node_root,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_tmp_pprof,
	&ssam_node_hid_main_keyboard,
	&ssam_node_hid_main_touchpad,
	&ssam_node_hid_main_iid5,
	&ssam_node_hid_sam_ucm_ucsi,
	NULL,
};

 
static const struct software_node *ssam_node_group_sls[] = {
	&ssam_node_root,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_tmp_pprof,
	&ssam_node_pos_tablet_switch,
	&ssam_node_hid_sam_keyboard,
	&ssam_node_hid_sam_penstash,
	&ssam_node_hid_sam_touchpad,
	&ssam_node_hid_sam_sensors,
	&ssam_node_hid_sam_ucm_ucsi,
	&ssam_node_hid_sam_sysctrl,
	NULL,
};

 
static const struct software_node *ssam_node_group_slg1[] = {
	&ssam_node_root,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_tmp_pprof,
	NULL,
};

 
static const struct software_node *ssam_node_group_sp7[] = {
	&ssam_node_root,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_tmp_pprof,
	NULL,
};

 
static const struct software_node *ssam_node_group_sp8[] = {
	&ssam_node_root,
	&ssam_node_hub_kip,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_tmp_pprof,
	&ssam_node_kip_tablet_switch,
	&ssam_node_hid_kip_keyboard,
	&ssam_node_hid_kip_penstash,
	&ssam_node_hid_kip_touchpad,
	&ssam_node_hid_kip_fwupd,
	&ssam_node_hid_sam_sensors,
	&ssam_node_hid_sam_ucm_ucsi,
	NULL,
};

 
static const struct software_node *ssam_node_group_sp9[] = {
	&ssam_node_root,
	&ssam_node_hub_kip,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_tmp_pprof,
	&ssam_node_pos_tablet_switch,
	&ssam_node_hid_kip_keyboard,
	&ssam_node_hid_kip_penstash,
	&ssam_node_hid_kip_touchpad,
	&ssam_node_hid_kip_fwupd,
	&ssam_node_hid_sam_sensors,
	&ssam_node_hid_sam_ucm_ucsi,
	NULL,
};


 

static const struct acpi_device_id ssam_platform_hub_match[] = {
	 
	{ "MSHW0081", (unsigned long)ssam_node_group_gen5 },

	 
	{ "MSHW0111", (unsigned long)ssam_node_group_gen5 },

	 
	{ "MSHW0116", (unsigned long)ssam_node_group_sp7 },

	 
	{ "MSHW0119", (unsigned long)ssam_node_group_sp7 },

	 
	{ "MSHW0263", (unsigned long)ssam_node_group_sp8 },

	 
	{ "MSHW0343", (unsigned long)ssam_node_group_sp9 },

	 
	{ "MSHW0107", (unsigned long)ssam_node_group_gen5 },

	 
	{ "MSHW0117", (unsigned long)ssam_node_group_sb3 },

	 
	{ "MSHW0086", (unsigned long)ssam_node_group_gen5 },

	 
	{ "MSHW0112", (unsigned long)ssam_node_group_gen5 },

	 
	{ "MSHW0114", (unsigned long)ssam_node_group_sl3 },

	 
	{ "MSHW0110", (unsigned long)ssam_node_group_sl3 },

	 
	{ "MSHW0250", (unsigned long)ssam_node_group_sl3 },

	 
	{ "MSHW0350", (unsigned long)ssam_node_group_sl5 },

	 
	{ "MSHW0118", (unsigned long)ssam_node_group_slg1 },

	 
	{ "MSHW0290", (unsigned long)ssam_node_group_slg1 },

	 
	{ "MSHW0123", (unsigned long)ssam_node_group_sls },

	{ },
};
MODULE_DEVICE_TABLE(acpi, ssam_platform_hub_match);

static int ssam_platform_hub_probe(struct platform_device *pdev)
{
	const struct software_node **nodes;
	struct ssam_controller *ctrl;
	struct fwnode_handle *root;
	int status;

	nodes = (const struct software_node **)acpi_device_get_match_data(&pdev->dev);
	if (!nodes)
		return -ENODEV;

	 
	ctrl = ssam_client_bind(&pdev->dev);
	if (IS_ERR(ctrl))
		return PTR_ERR(ctrl) == -ENODEV ? -EPROBE_DEFER : PTR_ERR(ctrl);

	status = software_node_register_node_group(nodes);
	if (status)
		return status;

	root = software_node_fwnode(&ssam_node_root);
	if (!root) {
		software_node_unregister_node_group(nodes);
		return -ENOENT;
	}

	set_secondary_fwnode(&pdev->dev, root);

	status = __ssam_register_clients(&pdev->dev, ctrl, root);
	if (status) {
		set_secondary_fwnode(&pdev->dev, NULL);
		software_node_unregister_node_group(nodes);
	}

	platform_set_drvdata(pdev, nodes);
	return status;
}

static int ssam_platform_hub_remove(struct platform_device *pdev)
{
	const struct software_node **nodes = platform_get_drvdata(pdev);

	ssam_remove_clients(&pdev->dev);
	set_secondary_fwnode(&pdev->dev, NULL);
	software_node_unregister_node_group(nodes);
	return 0;
}

static struct platform_driver ssam_platform_hub_driver = {
	.probe = ssam_platform_hub_probe,
	.remove = ssam_platform_hub_remove,
	.driver = {
		.name = "surface_aggregator_platform_hub",
		.acpi_match_table = ssam_platform_hub_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_platform_driver(ssam_platform_hub_driver);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Device-registry for Surface System Aggregator Module");
MODULE_LICENSE("GPL");
