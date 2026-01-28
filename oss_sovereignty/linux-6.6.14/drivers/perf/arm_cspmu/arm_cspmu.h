#ifndef __ARM_CSPMU_H__
#define __ARM_CSPMU_H__
#include <linux/bitfield.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#define to_arm_cspmu(p) (container_of(p, struct arm_cspmu, pmu))
#define ARM_CSPMU_EXT_ATTR(_name, _func, _config)			\
	(&((struct dev_ext_attribute[]){				\
		{							\
			.attr = __ATTR(_name, 0444, _func, NULL),	\
			.var = (void *)_config				\
		}							\
	})[0].attr.attr)
#define ARM_CSPMU_FORMAT_ATTR(_name, _config)				\
	ARM_CSPMU_EXT_ATTR(_name, arm_cspmu_sysfs_format_show, (char *)_config)
#define ARM_CSPMU_EVENT_ATTR(_name, _config)				\
	PMU_EVENT_ATTR_ID(_name, arm_cspmu_sysfs_event_show, _config)
#define ARM_CSPMU_EVENT_MASK	GENMASK_ULL(63, 0)
#define ARM_CSPMU_FILTER_MASK	GENMASK_ULL(63, 0)
#define ARM_CSPMU_FORMAT_EVENT_ATTR	\
	ARM_CSPMU_FORMAT_ATTR(event, "config:0-32")
#define ARM_CSPMU_FORMAT_FILTER_ATTR	\
	ARM_CSPMU_FORMAT_ATTR(filter, "config1:0-31")
#define ARM_CSPMU_EVT_CYCLES_DEFAULT	(0x1ULL << 32)
#define ARM_CSPMU_MAX_HW_CNTRS		256
#define ARM_CSPMU_CYCLE_CNTR_IDX	31
#define ARM_CSPMU_PMIIDR_IMPLEMENTER	GENMASK(11, 0)
#define ARM_CSPMU_PMIIDR_PRODUCTID	GENMASK(31, 20)
struct arm_cspmu;
struct arm_cspmu_hw_events {
	struct perf_event **events;
	DECLARE_BITMAP(used_ctrs, ARM_CSPMU_MAX_HW_CNTRS);
};
struct arm_cspmu_impl_ops {
	struct attribute **(*get_event_attrs)(const struct arm_cspmu *cspmu);
	struct attribute **(*get_format_attrs)(const struct arm_cspmu *cspmu);
	const char *(*get_identifier)(const struct arm_cspmu *cspmu);
	const char *(*get_name)(const struct arm_cspmu *cspmu);
	bool (*is_cycle_counter_event)(const struct perf_event *event);
	u32 (*event_type)(const struct perf_event *event);
	u32 (*event_filter)(const struct perf_event *event);
	umode_t (*event_attr_is_visible)(struct kobject *kobj,
					 struct attribute *attr, int unused);
};
struct arm_cspmu_impl {
	u32 pmiidr;
	struct arm_cspmu_impl_ops ops;
	void *ctx;
};
struct arm_cspmu {
	struct pmu pmu;
	struct device *dev;
	const char *name;
	const char *identifier;
	void __iomem *base0;
	void __iomem *base1;
	cpumask_t associated_cpus;
	cpumask_t active_cpu;
	struct hlist_node cpuhp_node;
	int irq;
	bool has_atomic_dword;
	u32 pmcfgr;
	u32 num_logical_ctrs;
	u32 num_set_clr_reg;
	int cycle_counter_logical_idx;
	struct arm_cspmu_hw_events hw_events;
	struct arm_cspmu_impl impl;
};
ssize_t arm_cspmu_sysfs_event_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf);
ssize_t arm_cspmu_sysfs_format_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf);
#endif  
