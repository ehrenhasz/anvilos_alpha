
 

#include <linux/acpi.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/percpu.h>
#include <linux/perf/arm_pmu.h>

#include <asm/cpu.h>
#include <asm/cputype.h>

static DEFINE_PER_CPU(struct arm_pmu *, probed_pmus);
static DEFINE_PER_CPU(int, pmu_irqs);

static int arm_pmu_acpi_register_irq(int cpu)
{
	struct acpi_madt_generic_interrupt *gicc;
	int gsi, trigger;

	gicc = acpi_cpu_get_madt_gicc(cpu);

	gsi = gicc->performance_interrupt;

	 
	if (!gsi)
		return 0;

	if (gicc->flags & ACPI_MADT_PERFORMANCE_IRQ_MODE)
		trigger = ACPI_EDGE_SENSITIVE;
	else
		trigger = ACPI_LEVEL_SENSITIVE;

	 
	return acpi_register_gsi(NULL, gsi, trigger, ACPI_ACTIVE_HIGH);
}

static void arm_pmu_acpi_unregister_irq(int cpu)
{
	struct acpi_madt_generic_interrupt *gicc;
	int gsi;

	gicc = acpi_cpu_get_madt_gicc(cpu);

	gsi = gicc->performance_interrupt;
	if (gsi)
		acpi_unregister_gsi(gsi);
}

static int __maybe_unused
arm_acpi_register_pmu_device(struct platform_device *pdev, u8 len,
			     u16 (*parse_gsi)(struct acpi_madt_generic_interrupt *))
{
	int cpu, this_hetid, hetid, irq, ret;
	u16 this_gsi = 0, gsi = 0;

	 
	if (pdev->num_resources != 1)
		return -ENXIO;

	if (pdev->resource[0].flags != IORESOURCE_IRQ)
		return -ENXIO;

	 
	for_each_possible_cpu(cpu) {
		struct acpi_madt_generic_interrupt *gicc;

		gicc = acpi_cpu_get_madt_gicc(cpu);
		if (gicc->header.length < len)
			return gsi ? -ENXIO : 0;

		this_gsi = parse_gsi(gicc);
		this_hetid = find_acpi_cpu_topology_hetero_id(cpu);
		if (!gsi) {
			hetid = this_hetid;
			gsi = this_gsi;
		} else if (hetid != this_hetid || gsi != this_gsi) {
			pr_warn("ACPI: %s: must be homogeneous\n", pdev->name);
			return -ENXIO;
		}
	}

	if (!this_gsi)
		return 0;

	irq = acpi_register_gsi(NULL, gsi, ACPI_LEVEL_SENSITIVE, ACPI_ACTIVE_HIGH);
	if (irq < 0) {
		pr_warn("ACPI: %s Unable to register interrupt: %d\n", pdev->name, gsi);
		return -ENXIO;
	}

	pdev->resource[0].start = irq;
	ret = platform_device_register(pdev);
	if (ret)
		acpi_unregister_gsi(gsi);

	return ret;
}

#if IS_ENABLED(CONFIG_ARM_SPE_PMU)
static struct resource spe_resources[] = {
	{
		 
		.flags          = IORESOURCE_IRQ,
	}
};

static struct platform_device spe_dev = {
	.name = ARMV8_SPE_PDEV_NAME,
	.id = -1,
	.resource = spe_resources,
	.num_resources = ARRAY_SIZE(spe_resources)
};

static u16 arm_spe_parse_gsi(struct acpi_madt_generic_interrupt *gicc)
{
	return gicc->spe_interrupt;
}

 
static void arm_spe_acpi_register_device(void)
{
	int ret = arm_acpi_register_pmu_device(&spe_dev, ACPI_MADT_GICC_SPE,
					       arm_spe_parse_gsi);
	if (ret)
		pr_warn("ACPI: SPE: Unable to register device\n");
}
#else
static inline void arm_spe_acpi_register_device(void)
{
}
#endif  

#if IS_ENABLED(CONFIG_CORESIGHT_TRBE)
static struct resource trbe_resources[] = {
	{
		 
		.flags          = IORESOURCE_IRQ,
	}
};

static struct platform_device trbe_dev = {
	.name = ARMV8_TRBE_PDEV_NAME,
	.id = -1,
	.resource = trbe_resources,
	.num_resources = ARRAY_SIZE(trbe_resources)
};

static u16 arm_trbe_parse_gsi(struct acpi_madt_generic_interrupt *gicc)
{
	return gicc->trbe_interrupt;
}

static void arm_trbe_acpi_register_device(void)
{
	int ret = arm_acpi_register_pmu_device(&trbe_dev, ACPI_MADT_GICC_TRBE,
					       arm_trbe_parse_gsi);
	if (ret)
		pr_warn("ACPI: TRBE: Unable to register device\n");
}
#else
static inline void arm_trbe_acpi_register_device(void)
{

}
#endif  

static int arm_pmu_acpi_parse_irqs(void)
{
	int irq, cpu, irq_cpu, err;

	for_each_possible_cpu(cpu) {
		irq = arm_pmu_acpi_register_irq(cpu);
		if (irq < 0) {
			err = irq;
			pr_warn("Unable to parse ACPI PMU IRQ for CPU%d: %d\n",
				cpu, err);
			goto out_err;
		} else if (irq == 0) {
			pr_warn("No ACPI PMU IRQ for CPU%d\n", cpu);
		}

		 
		per_cpu(pmu_irqs, cpu) = irq;
		err = armpmu_request_irq(irq, cpu);
		if (err)
			goto out_err;
	}

	return 0;

out_err:
	for_each_possible_cpu(cpu) {
		irq = per_cpu(pmu_irqs, cpu);
		if (!irq)
			continue;

		arm_pmu_acpi_unregister_irq(cpu);

		 
		for_each_possible_cpu(irq_cpu) {
			if (per_cpu(pmu_irqs, irq_cpu) == irq)
				per_cpu(pmu_irqs, irq_cpu) = 0;
		}
	}

	return err;
}

static struct arm_pmu *arm_pmu_acpi_find_pmu(void)
{
	unsigned long cpuid = read_cpuid_id();
	struct arm_pmu *pmu;
	int cpu;

	for_each_possible_cpu(cpu) {
		pmu = per_cpu(probed_pmus, cpu);
		if (!pmu || pmu->acpi_cpuid != cpuid)
			continue;

		return pmu;
	}

	return NULL;
}

 
static bool pmu_irq_matches(struct arm_pmu *pmu, int irq)
{
	struct pmu_hw_events __percpu *hw_events = pmu->hw_events;
	int cpu;

	if (!irq)
		return true;

	for_each_cpu(cpu, &pmu->supported_cpus) {
		int other_irq = per_cpu(hw_events->irq, cpu);
		if (!other_irq)
			continue;

		if (irq == other_irq)
			continue;
		if (!irq_is_percpu_devid(irq) && !irq_is_percpu_devid(other_irq))
			continue;

		pr_warn("mismatched PPIs detected\n");
		return false;
	}

	return true;
}

static void arm_pmu_acpi_associate_pmu_cpu(struct arm_pmu *pmu,
					   unsigned int cpu)
{
	int irq = per_cpu(pmu_irqs, cpu);

	per_cpu(probed_pmus, cpu) = pmu;

	if (pmu_irq_matches(pmu, irq)) {
		struct pmu_hw_events __percpu *hw_events;
		hw_events = pmu->hw_events;
		per_cpu(hw_events->irq, cpu) = irq;
	}

	cpumask_set_cpu(cpu, &pmu->supported_cpus);
}

 
static int arm_pmu_acpi_cpu_starting(unsigned int cpu)
{
	struct arm_pmu *pmu;

	 
	if (per_cpu(probed_pmus, cpu))
		return 0;

	pmu = arm_pmu_acpi_find_pmu();
	if (!pmu) {
		pr_warn_ratelimited("Unable to associate CPU%d with a PMU\n",
				    cpu);
		return 0;
	}

	arm_pmu_acpi_associate_pmu_cpu(pmu, cpu);
	return 0;
}

static void arm_pmu_acpi_probe_matching_cpus(struct arm_pmu *pmu,
					     unsigned long cpuid)
{
	int cpu;

	for_each_online_cpu(cpu) {
		unsigned long cpu_cpuid = per_cpu(cpu_data, cpu).reg_midr;

		if (cpu_cpuid == cpuid)
			arm_pmu_acpi_associate_pmu_cpu(pmu, cpu);
	}
}

int arm_pmu_acpi_probe(armpmu_init_fn init_fn)
{
	int pmu_idx = 0;
	unsigned int cpu;
	int ret;

	ret = arm_pmu_acpi_parse_irqs();
	if (ret)
		return ret;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_PERF_ARM_ACPI_STARTING,
					"perf/arm/pmu_acpi:starting",
					arm_pmu_acpi_cpu_starting, NULL);
	if (ret)
		return ret;

	 
	for_each_online_cpu(cpu) {
		struct arm_pmu *pmu = per_cpu(probed_pmus, cpu);
		unsigned long cpuid;
		char *base_name;

		 
		if (pmu)
			continue;

		pmu = armpmu_alloc();
		if (!pmu) {
			pr_warn("Unable to allocate PMU for CPU%d\n",
				cpu);
			return -ENOMEM;
		}

		cpuid = per_cpu(cpu_data, cpu).reg_midr;
		pmu->acpi_cpuid = cpuid;

		arm_pmu_acpi_probe_matching_cpus(pmu, cpuid);

		ret = init_fn(pmu);
		if (ret == -ENODEV) {
			 
			continue;
		} else if (ret) {
			pr_warn("Unable to initialise PMU for CPU%d\n", cpu);
			return ret;
		}

		base_name = pmu->name;
		pmu->name = kasprintf(GFP_KERNEL, "%s_%d", base_name, pmu_idx++);
		if (!pmu->name) {
			pr_warn("Unable to allocate PMU name for CPU%d\n", cpu);
			return -ENOMEM;
		}

		ret = armpmu_register(pmu);
		if (ret) {
			pr_warn("Failed to register PMU for CPU%d\n", cpu);
			kfree(pmu->name);
			return ret;
		}
	}

	return ret;
}

static int arm_pmu_acpi_init(void)
{
	if (acpi_disabled)
		return 0;

	arm_spe_acpi_register_device();
	arm_trbe_acpi_register_device();

	return 0;
}
subsys_initcall(arm_pmu_acpi_init)
