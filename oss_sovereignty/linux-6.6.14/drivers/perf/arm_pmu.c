
#undef DEBUG

 
#define pr_fmt(fmt) "hw perfevents: " fmt

#include <linux/bitmap.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/perf/arm_pmu.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>

#include <asm/irq_regs.h>

static int armpmu_count_irq_users(const int irq);

struct pmu_irq_ops {
	void (*enable_pmuirq)(unsigned int irq);
	void (*disable_pmuirq)(unsigned int irq);
	void (*free_pmuirq)(unsigned int irq, int cpu, void __percpu *devid);
};

static void armpmu_free_pmuirq(unsigned int irq, int cpu, void __percpu *devid)
{
	free_irq(irq, per_cpu_ptr(devid, cpu));
}

static const struct pmu_irq_ops pmuirq_ops = {
	.enable_pmuirq = enable_irq,
	.disable_pmuirq = disable_irq_nosync,
	.free_pmuirq = armpmu_free_pmuirq
};

static void armpmu_free_pmunmi(unsigned int irq, int cpu, void __percpu *devid)
{
	free_nmi(irq, per_cpu_ptr(devid, cpu));
}

static const struct pmu_irq_ops pmunmi_ops = {
	.enable_pmuirq = enable_nmi,
	.disable_pmuirq = disable_nmi_nosync,
	.free_pmuirq = armpmu_free_pmunmi
};

static void armpmu_enable_percpu_pmuirq(unsigned int irq)
{
	enable_percpu_irq(irq, IRQ_TYPE_NONE);
}

static void armpmu_free_percpu_pmuirq(unsigned int irq, int cpu,
				   void __percpu *devid)
{
	if (armpmu_count_irq_users(irq) == 1)
		free_percpu_irq(irq, devid);
}

static const struct pmu_irq_ops percpu_pmuirq_ops = {
	.enable_pmuirq = armpmu_enable_percpu_pmuirq,
	.disable_pmuirq = disable_percpu_irq,
	.free_pmuirq = armpmu_free_percpu_pmuirq
};

static void armpmu_enable_percpu_pmunmi(unsigned int irq)
{
	if (!prepare_percpu_nmi(irq))
		enable_percpu_nmi(irq, IRQ_TYPE_NONE);
}

static void armpmu_disable_percpu_pmunmi(unsigned int irq)
{
	disable_percpu_nmi(irq);
	teardown_percpu_nmi(irq);
}

static void armpmu_free_percpu_pmunmi(unsigned int irq, int cpu,
				      void __percpu *devid)
{
	if (armpmu_count_irq_users(irq) == 1)
		free_percpu_nmi(irq, devid);
}

static const struct pmu_irq_ops percpu_pmunmi_ops = {
	.enable_pmuirq = armpmu_enable_percpu_pmunmi,
	.disable_pmuirq = armpmu_disable_percpu_pmunmi,
	.free_pmuirq = armpmu_free_percpu_pmunmi
};

static DEFINE_PER_CPU(struct arm_pmu *, cpu_armpmu);
static DEFINE_PER_CPU(int, cpu_irq);
static DEFINE_PER_CPU(const struct pmu_irq_ops *, cpu_irq_ops);

static bool has_nmi;

static inline u64 arm_pmu_event_max_period(struct perf_event *event)
{
	if (event->hw.flags & ARMPMU_EVT_64BIT)
		return GENMASK_ULL(63, 0);
	else if (event->hw.flags & ARMPMU_EVT_63BIT)
		return GENMASK_ULL(62, 0);
	else if (event->hw.flags & ARMPMU_EVT_47BIT)
		return GENMASK_ULL(46, 0);
	else
		return GENMASK_ULL(31, 0);
}

static int
armpmu_map_cache_event(const unsigned (*cache_map)
				      [PERF_COUNT_HW_CACHE_MAX]
				      [PERF_COUNT_HW_CACHE_OP_MAX]
				      [PERF_COUNT_HW_CACHE_RESULT_MAX],
		       u64 config)
{
	unsigned int cache_type, cache_op, cache_result, ret;

	cache_type = (config >>  0) & 0xff;
	if (cache_type >= PERF_COUNT_HW_CACHE_MAX)
		return -EINVAL;

	cache_op = (config >>  8) & 0xff;
	if (cache_op >= PERF_COUNT_HW_CACHE_OP_MAX)
		return -EINVAL;

	cache_result = (config >> 16) & 0xff;
	if (cache_result >= PERF_COUNT_HW_CACHE_RESULT_MAX)
		return -EINVAL;

	if (!cache_map)
		return -ENOENT;

	ret = (int)(*cache_map)[cache_type][cache_op][cache_result];

	if (ret == CACHE_OP_UNSUPPORTED)
		return -ENOENT;

	return ret;
}

static int
armpmu_map_hw_event(const unsigned (*event_map)[PERF_COUNT_HW_MAX], u64 config)
{
	int mapping;

	if (config >= PERF_COUNT_HW_MAX)
		return -EINVAL;

	if (!event_map)
		return -ENOENT;

	mapping = (*event_map)[config];
	return mapping == HW_OP_UNSUPPORTED ? -ENOENT : mapping;
}

static int
armpmu_map_raw_event(u32 raw_event_mask, u64 config)
{
	return (int)(config & raw_event_mask);
}

int
armpmu_map_event(struct perf_event *event,
		 const unsigned (*event_map)[PERF_COUNT_HW_MAX],
		 const unsigned (*cache_map)
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX],
		 u32 raw_event_mask)
{
	u64 config = event->attr.config;
	int type = event->attr.type;

	if (type == event->pmu->type)
		return armpmu_map_raw_event(raw_event_mask, config);

	switch (type) {
	case PERF_TYPE_HARDWARE:
		return armpmu_map_hw_event(event_map, config);
	case PERF_TYPE_HW_CACHE:
		return armpmu_map_cache_event(cache_map, config);
	case PERF_TYPE_RAW:
		return armpmu_map_raw_event(raw_event_mask, config);
	}

	return -ENOENT;
}

int armpmu_event_set_period(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	s64 left = local64_read(&hwc->period_left);
	s64 period = hwc->sample_period;
	u64 max_period;
	int ret = 0;

	max_period = arm_pmu_event_max_period(event);
	if (unlikely(left <= -period)) {
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (unlikely(left <= 0)) {
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	 
	if (left > (max_period >> 1))
		left = (max_period >> 1);

	local64_set(&hwc->prev_count, (u64)-left);

	armpmu->write_counter(event, (u64)(-left) & max_period);

	perf_event_update_userpage(event);

	return ret;
}

u64 armpmu_event_update(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	u64 delta, prev_raw_count, new_raw_count;
	u64 max_period = arm_pmu_event_max_period(event);

again:
	prev_raw_count = local64_read(&hwc->prev_count);
	new_raw_count = armpmu->read_counter(event);

	if (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
			     new_raw_count) != prev_raw_count)
		goto again;

	delta = (new_raw_count - prev_raw_count) & max_period;

	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);

	return new_raw_count;
}

static void
armpmu_read(struct perf_event *event)
{
	armpmu_event_update(event);
}

static void
armpmu_stop(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	 
	if (!(hwc->state & PERF_HES_STOPPED)) {
		armpmu->disable(event);
		armpmu_event_update(event);
		hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
	}
}

static void armpmu_start(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	 
	if (flags & PERF_EF_RELOAD)
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));

	hwc->state = 0;
	 
	armpmu_event_set_period(event);
	armpmu->enable(event);
}

static void
armpmu_del(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *hw_events = this_cpu_ptr(armpmu->hw_events);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	armpmu_stop(event, PERF_EF_UPDATE);
	hw_events->events[idx] = NULL;
	armpmu->clear_event_idx(hw_events, event);
	perf_event_update_userpage(event);
	 
	hwc->idx = -1;
}

static int
armpmu_add(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *hw_events = this_cpu_ptr(armpmu->hw_events);
	struct hw_perf_event *hwc = &event->hw;
	int idx;

	 
	if (!cpumask_test_cpu(smp_processor_id(), &armpmu->supported_cpus))
		return -ENOENT;

	 
	idx = armpmu->get_event_idx(hw_events, event);
	if (idx < 0)
		return idx;

	 
	event->hw.idx = idx;
	armpmu->disable(event);
	hw_events->events[idx] = event;

	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	if (flags & PERF_EF_START)
		armpmu_start(event, PERF_EF_RELOAD);

	 
	perf_event_update_userpage(event);

	return 0;
}

static int
validate_event(struct pmu *pmu, struct pmu_hw_events *hw_events,
			       struct perf_event *event)
{
	struct arm_pmu *armpmu;

	if (is_software_event(event))
		return 1;

	 
	if (event->pmu != pmu)
		return 0;

	if (event->state < PERF_EVENT_STATE_OFF)
		return 1;

	if (event->state == PERF_EVENT_STATE_OFF && !event->attr.enable_on_exec)
		return 1;

	armpmu = to_arm_pmu(event->pmu);
	return armpmu->get_event_idx(hw_events, event) >= 0;
}

static int
validate_group(struct perf_event *event)
{
	struct perf_event *sibling, *leader = event->group_leader;
	struct pmu_hw_events fake_pmu;

	 
	memset(&fake_pmu.used_mask, 0, sizeof(fake_pmu.used_mask));

	if (!validate_event(event->pmu, &fake_pmu, leader))
		return -EINVAL;

	if (event == leader)
		return 0;

	for_each_sibling_event(sibling, leader) {
		if (!validate_event(event->pmu, &fake_pmu, sibling))
			return -EINVAL;
	}

	if (!validate_event(event->pmu, &fake_pmu, event))
		return -EINVAL;

	return 0;
}

static irqreturn_t armpmu_dispatch_irq(int irq, void *dev)
{
	struct arm_pmu *armpmu;
	int ret;
	u64 start_clock, finish_clock;

	 
	armpmu = *(void **)dev;
	if (WARN_ON_ONCE(!armpmu))
		return IRQ_NONE;

	start_clock = sched_clock();
	ret = armpmu->handle_irq(armpmu);
	finish_clock = sched_clock();

	perf_sample_event_took(finish_clock - start_clock);
	return ret;
}

static int
__hw_perf_event_init(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int mapping;

	hwc->flags = 0;
	mapping = armpmu->map_event(event);

	if (mapping < 0) {
		pr_debug("event %x:%llx not supported\n", event->attr.type,
			 event->attr.config);
		return mapping;
	}

	 
	hwc->idx		= -1;
	hwc->config_base	= 0;
	hwc->config		= 0;
	hwc->event_base		= 0;

	 
	if (armpmu->set_event_filter &&
	    armpmu->set_event_filter(hwc, &event->attr)) {
		pr_debug("ARM performance counters do not support "
			 "mode exclusion\n");
		return -EOPNOTSUPP;
	}

	 
	hwc->config_base	    |= (unsigned long)mapping;

	if (!is_sampling_event(event)) {
		 
		hwc->sample_period  = arm_pmu_event_max_period(event) >> 1;
		hwc->last_period    = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	return validate_group(event);
}

static int armpmu_event_init(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);

	 
	if (event->cpu != -1 &&
		!cpumask_test_cpu(event->cpu, &armpmu->supported_cpus))
		return -ENOENT;

	 
	if (has_branch_stack(event))
		return -EOPNOTSUPP;

	return __hw_perf_event_init(event);
}

static void armpmu_enable(struct pmu *pmu)
{
	struct arm_pmu *armpmu = to_arm_pmu(pmu);
	struct pmu_hw_events *hw_events = this_cpu_ptr(armpmu->hw_events);
	bool enabled = !bitmap_empty(hw_events->used_mask, armpmu->num_events);

	 
	if (!cpumask_test_cpu(smp_processor_id(), &armpmu->supported_cpus))
		return;

	if (enabled)
		armpmu->start(armpmu);
}

static void armpmu_disable(struct pmu *pmu)
{
	struct arm_pmu *armpmu = to_arm_pmu(pmu);

	 
	if (!cpumask_test_cpu(smp_processor_id(), &armpmu->supported_cpus))
		return;

	armpmu->stop(armpmu);
}

 
static bool armpmu_filter(struct pmu *pmu, int cpu)
{
	struct arm_pmu *armpmu = to_arm_pmu(pmu);
	return !cpumask_test_cpu(cpu, &armpmu->supported_cpus);
}

static ssize_t cpus_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct arm_pmu *armpmu = to_arm_pmu(dev_get_drvdata(dev));
	return cpumap_print_to_pagebuf(true, buf, &armpmu->supported_cpus);
}

static DEVICE_ATTR_RO(cpus);

static struct attribute *armpmu_common_attrs[] = {
	&dev_attr_cpus.attr,
	NULL,
};

static const struct attribute_group armpmu_common_attr_group = {
	.attrs = armpmu_common_attrs,
};

static int armpmu_count_irq_users(const int irq)
{
	int cpu, count = 0;

	for_each_possible_cpu(cpu) {
		if (per_cpu(cpu_irq, cpu) == irq)
			count++;
	}

	return count;
}

static const struct pmu_irq_ops *armpmu_find_irq_ops(int irq)
{
	const struct pmu_irq_ops *ops = NULL;
	int cpu;

	for_each_possible_cpu(cpu) {
		if (per_cpu(cpu_irq, cpu) != irq)
			continue;

		ops = per_cpu(cpu_irq_ops, cpu);
		if (ops)
			break;
	}

	return ops;
}

void armpmu_free_irq(int irq, int cpu)
{
	if (per_cpu(cpu_irq, cpu) == 0)
		return;
	if (WARN_ON(irq != per_cpu(cpu_irq, cpu)))
		return;

	per_cpu(cpu_irq_ops, cpu)->free_pmuirq(irq, cpu, &cpu_armpmu);

	per_cpu(cpu_irq, cpu) = 0;
	per_cpu(cpu_irq_ops, cpu) = NULL;
}

int armpmu_request_irq(int irq, int cpu)
{
	int err = 0;
	const irq_handler_t handler = armpmu_dispatch_irq;
	const struct pmu_irq_ops *irq_ops;

	if (!irq)
		return 0;

	if (!irq_is_percpu_devid(irq)) {
		unsigned long irq_flags;

		err = irq_force_affinity(irq, cpumask_of(cpu));

		if (err && num_possible_cpus() > 1) {
			pr_warn("unable to set irq affinity (irq=%d, cpu=%u)\n",
				irq, cpu);
			goto err_out;
		}

		irq_flags = IRQF_PERCPU |
			    IRQF_NOBALANCING | IRQF_NO_AUTOEN |
			    IRQF_NO_THREAD;

		err = request_nmi(irq, handler, irq_flags, "arm-pmu",
				  per_cpu_ptr(&cpu_armpmu, cpu));

		 
		if (err) {
			err = request_irq(irq, handler, irq_flags, "arm-pmu",
					  per_cpu_ptr(&cpu_armpmu, cpu));
			irq_ops = &pmuirq_ops;
		} else {
			has_nmi = true;
			irq_ops = &pmunmi_ops;
		}
	} else if (armpmu_count_irq_users(irq) == 0) {
		err = request_percpu_nmi(irq, handler, "arm-pmu", &cpu_armpmu);

		 
		if (err) {
			err = request_percpu_irq(irq, handler, "arm-pmu",
						 &cpu_armpmu);
			irq_ops = &percpu_pmuirq_ops;
		} else {
			has_nmi = true;
			irq_ops = &percpu_pmunmi_ops;
		}
	} else {
		 
		irq_ops = armpmu_find_irq_ops(irq);

		if (WARN_ON(!irq_ops))
			err = -EINVAL;
	}

	if (err)
		goto err_out;

	per_cpu(cpu_irq, cpu) = irq;
	per_cpu(cpu_irq_ops, cpu) = irq_ops;
	return 0;

err_out:
	pr_err("unable to request IRQ%d for ARM PMU counters\n", irq);
	return err;
}

static int armpmu_get_cpu_irq(struct arm_pmu *pmu, int cpu)
{
	struct pmu_hw_events __percpu *hw_events = pmu->hw_events;
	return per_cpu(hw_events->irq, cpu);
}

bool arm_pmu_irq_is_nmi(void)
{
	return has_nmi;
}

 
static int arm_perf_starting_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct arm_pmu *pmu = hlist_entry_safe(node, struct arm_pmu, node);
	int irq;

	if (!cpumask_test_cpu(cpu, &pmu->supported_cpus))
		return 0;
	if (pmu->reset)
		pmu->reset(pmu);

	per_cpu(cpu_armpmu, cpu) = pmu;

	irq = armpmu_get_cpu_irq(pmu, cpu);
	if (irq)
		per_cpu(cpu_irq_ops, cpu)->enable_pmuirq(irq);

	return 0;
}

static int arm_perf_teardown_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct arm_pmu *pmu = hlist_entry_safe(node, struct arm_pmu, node);
	int irq;

	if (!cpumask_test_cpu(cpu, &pmu->supported_cpus))
		return 0;

	irq = armpmu_get_cpu_irq(pmu, cpu);
	if (irq)
		per_cpu(cpu_irq_ops, cpu)->disable_pmuirq(irq);

	per_cpu(cpu_armpmu, cpu) = NULL;

	return 0;
}

#ifdef CONFIG_CPU_PM
static void cpu_pm_pmu_setup(struct arm_pmu *armpmu, unsigned long cmd)
{
	struct pmu_hw_events *hw_events = this_cpu_ptr(armpmu->hw_events);
	struct perf_event *event;
	int idx;

	for (idx = 0; idx < armpmu->num_events; idx++) {
		event = hw_events->events[idx];
		if (!event)
			continue;

		switch (cmd) {
		case CPU_PM_ENTER:
			 
			armpmu_stop(event, PERF_EF_UPDATE);
			break;
		case CPU_PM_EXIT:
		case CPU_PM_ENTER_FAILED:
			  
			armpmu_start(event, PERF_EF_RELOAD);
			break;
		default:
			break;
		}
	}
}

static int cpu_pm_pmu_notify(struct notifier_block *b, unsigned long cmd,
			     void *v)
{
	struct arm_pmu *armpmu = container_of(b, struct arm_pmu, cpu_pm_nb);
	struct pmu_hw_events *hw_events = this_cpu_ptr(armpmu->hw_events);
	bool enabled = !bitmap_empty(hw_events->used_mask, armpmu->num_events);

	if (!cpumask_test_cpu(smp_processor_id(), &armpmu->supported_cpus))
		return NOTIFY_DONE;

	 
	if (cmd == CPU_PM_EXIT && armpmu->reset)
		armpmu->reset(armpmu);

	if (!enabled)
		return NOTIFY_OK;

	switch (cmd) {
	case CPU_PM_ENTER:
		armpmu->stop(armpmu);
		cpu_pm_pmu_setup(armpmu, cmd);
		break;
	case CPU_PM_EXIT:
	case CPU_PM_ENTER_FAILED:
		cpu_pm_pmu_setup(armpmu, cmd);
		armpmu->start(armpmu);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static int cpu_pm_pmu_register(struct arm_pmu *cpu_pmu)
{
	cpu_pmu->cpu_pm_nb.notifier_call = cpu_pm_pmu_notify;
	return cpu_pm_register_notifier(&cpu_pmu->cpu_pm_nb);
}

static void cpu_pm_pmu_unregister(struct arm_pmu *cpu_pmu)
{
	cpu_pm_unregister_notifier(&cpu_pmu->cpu_pm_nb);
}
#else
static inline int cpu_pm_pmu_register(struct arm_pmu *cpu_pmu) { return 0; }
static inline void cpu_pm_pmu_unregister(struct arm_pmu *cpu_pmu) { }
#endif

static int cpu_pmu_init(struct arm_pmu *cpu_pmu)
{
	int err;

	err = cpuhp_state_add_instance(CPUHP_AP_PERF_ARM_STARTING,
				       &cpu_pmu->node);
	if (err)
		goto out;

	err = cpu_pm_pmu_register(cpu_pmu);
	if (err)
		goto out_unregister;

	return 0;

out_unregister:
	cpuhp_state_remove_instance_nocalls(CPUHP_AP_PERF_ARM_STARTING,
					    &cpu_pmu->node);
out:
	return err;
}

static void cpu_pmu_destroy(struct arm_pmu *cpu_pmu)
{
	cpu_pm_pmu_unregister(cpu_pmu);
	cpuhp_state_remove_instance_nocalls(CPUHP_AP_PERF_ARM_STARTING,
					    &cpu_pmu->node);
}

struct arm_pmu *armpmu_alloc(void)
{
	struct arm_pmu *pmu;
	int cpu;

	pmu = kzalloc(sizeof(*pmu), GFP_KERNEL);
	if (!pmu)
		goto out;

	pmu->hw_events = alloc_percpu_gfp(struct pmu_hw_events, GFP_KERNEL);
	if (!pmu->hw_events) {
		pr_info("failed to allocate per-cpu PMU data.\n");
		goto out_free_pmu;
	}

	pmu->pmu = (struct pmu) {
		.pmu_enable	= armpmu_enable,
		.pmu_disable	= armpmu_disable,
		.event_init	= armpmu_event_init,
		.add		= armpmu_add,
		.del		= armpmu_del,
		.start		= armpmu_start,
		.stop		= armpmu_stop,
		.read		= armpmu_read,
		.filter		= armpmu_filter,
		.attr_groups	= pmu->attr_groups,
		 
		.capabilities	= PERF_PMU_CAP_EXTENDED_REGS |
				  PERF_PMU_CAP_EXTENDED_HW_TYPE,
	};

	pmu->attr_groups[ARMPMU_ATTR_GROUP_COMMON] =
		&armpmu_common_attr_group;

	for_each_possible_cpu(cpu) {
		struct pmu_hw_events *events;

		events = per_cpu_ptr(pmu->hw_events, cpu);
		raw_spin_lock_init(&events->pmu_lock);
		events->percpu_pmu = pmu;
	}

	return pmu;

out_free_pmu:
	kfree(pmu);
out:
	return NULL;
}

void armpmu_free(struct arm_pmu *pmu)
{
	free_percpu(pmu->hw_events);
	kfree(pmu);
}

int armpmu_register(struct arm_pmu *pmu)
{
	int ret;

	ret = cpu_pmu_init(pmu);
	if (ret)
		return ret;

	if (!pmu->set_event_filter)
		pmu->pmu.capabilities |= PERF_PMU_CAP_NO_EXCLUDE;

	ret = perf_pmu_register(&pmu->pmu, pmu->name, -1);
	if (ret)
		goto out_destroy;

	pr_info("enabled with %s PMU driver, %d counters available%s\n",
		pmu->name, pmu->num_events,
		has_nmi ? ", using NMIs" : "");

	kvm_host_pmu_init(pmu);

	return 0;

out_destroy:
	cpu_pmu_destroy(pmu);
	return ret;
}

static int arm_pmu_hp_init(void)
{
	int ret;

	ret = cpuhp_setup_state_multi(CPUHP_AP_PERF_ARM_STARTING,
				      "perf/arm/pmu:starting",
				      arm_perf_starting_cpu,
				      arm_perf_teardown_cpu);
	if (ret)
		pr_err("CPU hotplug notifier for ARM PMU could not be registered: %d\n",
		       ret);
	return ret;
}
subsys_initcall(arm_pmu_hp_init);
