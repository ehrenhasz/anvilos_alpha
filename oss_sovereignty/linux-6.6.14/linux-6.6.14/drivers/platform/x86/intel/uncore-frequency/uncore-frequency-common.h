#ifndef __INTEL_UNCORE_FREQ_COMMON_H
#define __INTEL_UNCORE_FREQ_COMMON_H
#include <linux/device.h>
struct uncore_data {
	u64 stored_uncore_data;
	u32 initial_min_freq_khz;
	u32 initial_max_freq_khz;
	int control_cpu;
	bool valid;
	int package_id;
	int die_id;
	int domain_id;
	int cluster_id;
	int instance_id;
	char name[32];
	struct attribute_group uncore_attr_group;
	struct device_attribute max_freq_khz_dev_attr;
	struct device_attribute min_freq_khz_dev_attr;
	struct device_attribute initial_max_freq_khz_dev_attr;
	struct device_attribute initial_min_freq_khz_dev_attr;
	struct device_attribute current_freq_khz_dev_attr;
	struct device_attribute domain_id_dev_attr;
	struct device_attribute fabric_cluster_id_dev_attr;
	struct device_attribute package_id_dev_attr;
	struct attribute *uncore_attrs[9];
};
#define UNCORE_DOMAIN_ID_INVALID	-1
int uncore_freq_common_init(int (*read_control_freq)(struct uncore_data *data, unsigned int *min, unsigned int *max),
			     int (*write_control_freq)(struct uncore_data *data, unsigned int input, unsigned int min_max),
			     int (*uncore_read_freq)(struct uncore_data *data, unsigned int *freq));
void uncore_freq_common_exit(void);
int uncore_freq_add_entry(struct uncore_data *data, int cpu);
void uncore_freq_remove_die_entry(struct uncore_data *data);
#endif
