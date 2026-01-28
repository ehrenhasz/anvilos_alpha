#ifndef INTEL_SAR_H
#define INTEL_SAR_H
#define COMMAND_ID_DEV_MODE 1
#define COMMAND_ID_CONFIG_TABLE 2
#define DRVNAME "intc_sar"
#define MAX_DEV_MODES 50
#define MAX_REGULATORY 3
#define SAR_DSM_UUID "82737E72-3A33-4C45-A9C7-57C0411A5F13"
#define SAR_EVENT 0x80
#define SYSFS_DATANAME "intc_data"
#define TOTAL_DATA 4
struct wwan_device_mode_info {
	int device_mode;
	int bandtable_index;
	int antennatable_index;
	int sartable_index;
};
struct wwan_device_mode_configuration {
	int version;
	int total_dev_mode;
	struct wwan_device_mode_info *device_mode_info;
};
struct wwan_supported_info {
	int reg_mode_needed;
	int bios_table_revision;
	int num_supported_modes;
};
struct wwan_sar_context {
	guid_t guid;
	acpi_handle handle;
	int reg_value;
	struct platform_device *sar_device;
	struct wwan_supported_info supported_data;
	struct wwan_device_mode_info sar_data;
	struct wwan_device_mode_configuration config_data[MAX_REGULATORY];
};
#endif  
