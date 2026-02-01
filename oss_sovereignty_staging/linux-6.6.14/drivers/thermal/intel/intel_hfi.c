
 

#define pr_fmt(fmt)  "intel-hfi: " fmt

#include <linux/bitops.h>
#include <linux/cpufeature.h>
#include <linux/cpumask.h>
#include <linux/gfp.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/math.h>
#include <linux/mutex.h>
#include <linux/percpu-defs.h>
#include <linux/printk.h>
#include <linux/processor.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/topology.h>
#include <linux/workqueue.h>

#include <asm/msr.h>

#include "intel_hfi.h"
#include "thermal_interrupt.h"

#include "../thermal_netlink.h"

 
#define HW_FEEDBACK_PTR_VALID_BIT		BIT(0)
#define HW_FEEDBACK_CONFIG_HFI_ENABLE_BIT	BIT(0)

 

#define CPUID_HFI_LEAF 6

union hfi_capabilities {
	struct {
		u8	performance:1;
		u8	energy_efficiency:1;
		u8	__reserved:6;
	} split;
	u8 bits;
};

union cpuid6_edx {
	struct {
		union hfi_capabilities	capabilities;
		u32			table_pages:4;
		u32			__reserved:4;
		s32			index:16;
	} split;
	u32 full;
};

 
struct hfi_cpu_data {
	u8	perf_cap;
	u8	ee_cap;
} __packed;

 
struct hfi_hdr {
	u8	perf_updated;
	u8	ee_updated;
} __packed;

 
struct hfi_instance {
	union {
		void			*local_table;
		u64			*timestamp;
	};
	void			*hdr;
	void			*data;
	cpumask_var_t		cpus;
	void			*hw_table;
	struct delayed_work	update_work;
	raw_spinlock_t		table_lock;
	raw_spinlock_t		event_lock;
};

 
struct hfi_features {
	size_t		nr_table_pages;
	unsigned int	cpu_stride;
	unsigned int	hdr_size;
};

 
struct hfi_cpu_info {
	s16			index;
	struct hfi_instance	*hfi_instance;
};

static DEFINE_PER_CPU(struct hfi_cpu_info, hfi_cpu_info) = { .index = -1 };

static int max_hfi_instances;
static struct hfi_instance *hfi_instances;

static struct hfi_features hfi_features;
static DEFINE_MUTEX(hfi_instance_lock);

static struct workqueue_struct *hfi_updates_wq;
#define HFI_UPDATE_INTERVAL		HZ
#define HFI_MAX_THERM_NOTIFY_COUNT	16

static void get_hfi_caps(struct hfi_instance *hfi_instance,
			 struct thermal_genl_cpu_caps *cpu_caps)
{
	int cpu, i = 0;

	raw_spin_lock_irq(&hfi_instance->table_lock);
	for_each_cpu(cpu, hfi_instance->cpus) {
		struct hfi_cpu_data *caps;
		s16 index;

		index = per_cpu(hfi_cpu_info, cpu).index;
		caps = hfi_instance->data + index * hfi_features.cpu_stride;
		cpu_caps[i].cpu = cpu;

		 
		cpu_caps[i].performance = caps->perf_cap << 2;
		cpu_caps[i].efficiency = caps->ee_cap << 2;

		++i;
	}
	raw_spin_unlock_irq(&hfi_instance->table_lock);
}

 
static void update_capabilities(struct hfi_instance *hfi_instance)
{
	struct thermal_genl_cpu_caps *cpu_caps;
	int i = 0, cpu_count;

	 
	mutex_lock(&hfi_instance_lock);

	cpu_count = cpumask_weight(hfi_instance->cpus);

	 
	if (!cpu_count)
		goto out;

	cpu_caps = kcalloc(cpu_count, sizeof(*cpu_caps), GFP_KERNEL);
	if (!cpu_caps)
		goto out;

	get_hfi_caps(hfi_instance, cpu_caps);

	if (cpu_count < HFI_MAX_THERM_NOTIFY_COUNT)
		goto last_cmd;

	 
	for (i = 0;
	     (i + HFI_MAX_THERM_NOTIFY_COUNT) <= cpu_count;
	     i += HFI_MAX_THERM_NOTIFY_COUNT)
		thermal_genl_cpu_capability_event(HFI_MAX_THERM_NOTIFY_COUNT,
						  &cpu_caps[i]);

	cpu_count = cpu_count - i;

last_cmd:
	 
	if (cpu_count)
		thermal_genl_cpu_capability_event(cpu_count, &cpu_caps[i]);

	kfree(cpu_caps);
out:
	mutex_unlock(&hfi_instance_lock);
}

static void hfi_update_work_fn(struct work_struct *work)
{
	struct hfi_instance *hfi_instance;

	hfi_instance = container_of(to_delayed_work(work), struct hfi_instance,
				    update_work);

	update_capabilities(hfi_instance);
}

void intel_hfi_process_event(__u64 pkg_therm_status_msr_val)
{
	struct hfi_instance *hfi_instance;
	int cpu = smp_processor_id();
	struct hfi_cpu_info *info;
	u64 new_timestamp, msr, hfi;

	if (!pkg_therm_status_msr_val)
		return;

	info = &per_cpu(hfi_cpu_info, cpu);
	if (!info)
		return;

	 
	hfi_instance = info->hfi_instance;
	if (unlikely(!hfi_instance)) {
		pr_debug("Received event on CPU %d but instance was null", cpu);
		return;
	}

	 
	if (!raw_spin_trylock(&hfi_instance->event_lock))
		return;

	rdmsrl(MSR_IA32_PACKAGE_THERM_STATUS, msr);
	hfi = msr & PACKAGE_THERM_STATUS_HFI_UPDATED;
	if (!hfi) {
		raw_spin_unlock(&hfi_instance->event_lock);
		return;
	}

	 
	new_timestamp = *(u64 *)hfi_instance->hw_table;
	if (*hfi_instance->timestamp == new_timestamp) {
		thermal_clear_package_intr_status(PACKAGE_LEVEL, PACKAGE_THERM_STATUS_HFI_UPDATED);
		raw_spin_unlock(&hfi_instance->event_lock);
		return;
	}

	raw_spin_lock(&hfi_instance->table_lock);

	 
	memcpy(hfi_instance->local_table, hfi_instance->hw_table,
	       hfi_features.nr_table_pages << PAGE_SHIFT);

	 
	thermal_clear_package_intr_status(PACKAGE_LEVEL, PACKAGE_THERM_STATUS_HFI_UPDATED);

	raw_spin_unlock(&hfi_instance->table_lock);
	raw_spin_unlock(&hfi_instance->event_lock);

	queue_delayed_work(hfi_updates_wq, &hfi_instance->update_work,
			   HFI_UPDATE_INTERVAL);
}

static void init_hfi_cpu_index(struct hfi_cpu_info *info)
{
	union cpuid6_edx edx;

	 
	if (info->index > -1)
		return;

	edx.full = cpuid_edx(CPUID_HFI_LEAF);
	info->index = edx.split.index;
}

 
static void init_hfi_instance(struct hfi_instance *hfi_instance)
{
	 
	hfi_instance->hdr = hfi_instance->local_table +
			    sizeof(*hfi_instance->timestamp);

	 
	hfi_instance->data = hfi_instance->hdr + hfi_features.hdr_size;
}

 
void intel_hfi_online(unsigned int cpu)
{
	struct hfi_instance *hfi_instance;
	struct hfi_cpu_info *info;
	phys_addr_t hw_table_pa;
	u64 msr_val;
	u16 die_id;

	 
	if (!hfi_instances)
		return;

	 
	info = &per_cpu(hfi_cpu_info, cpu);
	die_id = topology_logical_die_id(cpu);
	hfi_instance = info->hfi_instance;
	if (!hfi_instance) {
		if (die_id >= max_hfi_instances)
			return;

		hfi_instance = &hfi_instances[die_id];
		info->hfi_instance = hfi_instance;
	}

	init_hfi_cpu_index(info);

	 
	mutex_lock(&hfi_instance_lock);
	if (hfi_instance->hdr) {
		cpumask_set_cpu(cpu, hfi_instance->cpus);
		goto unlock;
	}

	 
	hfi_instance->hw_table = alloc_pages_exact(hfi_features.nr_table_pages,
						   GFP_KERNEL | __GFP_ZERO);
	if (!hfi_instance->hw_table)
		goto unlock;

	hw_table_pa = virt_to_phys(hfi_instance->hw_table);

	 
	hfi_instance->local_table = kzalloc(hfi_features.nr_table_pages << PAGE_SHIFT,
					    GFP_KERNEL);
	if (!hfi_instance->local_table)
		goto free_hw_table;

	 
	msr_val = hw_table_pa | HW_FEEDBACK_PTR_VALID_BIT;
	wrmsrl(MSR_IA32_HW_FEEDBACK_PTR, msr_val);

	init_hfi_instance(hfi_instance);

	INIT_DELAYED_WORK(&hfi_instance->update_work, hfi_update_work_fn);
	raw_spin_lock_init(&hfi_instance->table_lock);
	raw_spin_lock_init(&hfi_instance->event_lock);

	cpumask_set_cpu(cpu, hfi_instance->cpus);

	 
	rdmsrl(MSR_IA32_HW_FEEDBACK_CONFIG, msr_val);
	msr_val |= HW_FEEDBACK_CONFIG_HFI_ENABLE_BIT;
	wrmsrl(MSR_IA32_HW_FEEDBACK_CONFIG, msr_val);

unlock:
	mutex_unlock(&hfi_instance_lock);
	return;

free_hw_table:
	free_pages_exact(hfi_instance->hw_table, hfi_features.nr_table_pages);
	goto unlock;
}

 
void intel_hfi_offline(unsigned int cpu)
{
	struct hfi_cpu_info *info = &per_cpu(hfi_cpu_info, cpu);
	struct hfi_instance *hfi_instance;

	 
	hfi_instance = info->hfi_instance;
	if (!hfi_instance)
		return;

	if (!hfi_instance->hdr)
		return;

	mutex_lock(&hfi_instance_lock);
	cpumask_clear_cpu(cpu, hfi_instance->cpus);
	mutex_unlock(&hfi_instance_lock);
}

static __init int hfi_parse_features(void)
{
	unsigned int nr_capabilities;
	union cpuid6_edx edx;

	if (!boot_cpu_has(X86_FEATURE_HFI))
		return -ENODEV;

	 
	edx.full = cpuid_edx(CPUID_HFI_LEAF);

	if (!edx.split.capabilities.split.performance) {
		pr_debug("Performance reporting not supported! Not using HFI\n");
		return -ENODEV;
	}

	 
	edx.split.capabilities.split.__reserved = 0;
	nr_capabilities = hweight8(edx.split.capabilities.bits);

	 
	hfi_features.nr_table_pages = edx.split.table_pages + 1;

	 
	hfi_features.hdr_size = DIV_ROUND_UP(nr_capabilities, 8) * 8;

	 
	hfi_features.cpu_stride = DIV_ROUND_UP(nr_capabilities, 8) * 8;

	return 0;
}

void __init intel_hfi_init(void)
{
	struct hfi_instance *hfi_instance;
	int i, j;

	if (hfi_parse_features())
		return;

	 
	max_hfi_instances = topology_max_packages() *
			    topology_max_die_per_package();

	 
	hfi_instances = kcalloc(max_hfi_instances, sizeof(*hfi_instances),
				GFP_KERNEL);
	if (!hfi_instances)
		return;

	for (i = 0; i < max_hfi_instances; i++) {
		hfi_instance = &hfi_instances[i];
		if (!zalloc_cpumask_var(&hfi_instance->cpus, GFP_KERNEL))
			goto err_nomem;
	}

	hfi_updates_wq = create_singlethread_workqueue("hfi-updates");
	if (!hfi_updates_wq)
		goto err_nomem;

	return;

err_nomem:
	for (j = 0; j < i; ++j) {
		hfi_instance = &hfi_instances[j];
		free_cpumask_var(hfi_instance->cpus);
	}

	kfree(hfi_instances);
	hfi_instances = NULL;
}
