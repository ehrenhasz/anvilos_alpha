

 

#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <asm/cpu_device_id.h>
#include <asm/intel-family.h>

#include <xen/xen.h>

static void intel_pmc_core_release(struct device *dev)
{
	kfree(dev);
}

static struct platform_device *pmc_core_device;

 
static const struct x86_cpu_id intel_pmc_core_platform_ids[] = {
	X86_MATCH_INTEL_FAM6_MODEL(SKYLAKE_L,		&pmc_core_device),
	X86_MATCH_INTEL_FAM6_MODEL(SKYLAKE,		&pmc_core_device),
	X86_MATCH_INTEL_FAM6_MODEL(KABYLAKE_L,		&pmc_core_device),
	X86_MATCH_INTEL_FAM6_MODEL(KABYLAKE,		&pmc_core_device),
	X86_MATCH_INTEL_FAM6_MODEL(CANNONLAKE_L,	&pmc_core_device),
	X86_MATCH_INTEL_FAM6_MODEL(ICELAKE_L,		&pmc_core_device),
	X86_MATCH_INTEL_FAM6_MODEL(COMETLAKE,		&pmc_core_device),
	X86_MATCH_INTEL_FAM6_MODEL(COMETLAKE_L,		&pmc_core_device),
	{}
};
MODULE_DEVICE_TABLE(x86cpu, intel_pmc_core_platform_ids);

static int __init pmc_core_platform_init(void)
{
	int retval;

	 
	if (acpi_dev_present("INT33A1", NULL, -1))
		return -ENODEV;

	 
	if (cpu_feature_enabled(X86_FEATURE_HYPERVISOR) && !xen_initial_domain())
		return -ENODEV;

	if (!x86_match_cpu(intel_pmc_core_platform_ids))
		return -ENODEV;

	pmc_core_device = kzalloc(sizeof(*pmc_core_device), GFP_KERNEL);
	if (!pmc_core_device)
		return -ENOMEM;

	pmc_core_device->name = "intel_pmc_core";
	pmc_core_device->dev.release = intel_pmc_core_release;

	retval = platform_device_register(pmc_core_device);
	if (retval)
		platform_device_put(pmc_core_device);

	return retval;
}

static void __exit pmc_core_platform_exit(void)
{
	platform_device_unregister(pmc_core_device);
}

module_init(pmc_core_platform_init);
module_exit(pmc_core_platform_exit);
MODULE_LICENSE("GPL v2");
