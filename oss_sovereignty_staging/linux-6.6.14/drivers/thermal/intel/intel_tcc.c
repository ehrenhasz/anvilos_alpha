
 

#include <linux/errno.h>
#include <linux/intel_tcc.h>
#include <asm/msr.h>

 
int intel_tcc_get_tjmax(int cpu)
{
	u32 low, high;
	int val, err;

	if (cpu < 0)
		err = rdmsr_safe(MSR_IA32_TEMPERATURE_TARGET, &low, &high);
	else
		err = rdmsr_safe_on_cpu(cpu, MSR_IA32_TEMPERATURE_TARGET, &low, &high);
	if (err)
		return err;

	val = (low >> 16) & 0xff;

	return val ? val : -ENODATA;
}
EXPORT_SYMBOL_NS_GPL(intel_tcc_get_tjmax, INTEL_TCC);

 
int intel_tcc_get_offset(int cpu)
{
	u32 low, high;
	int err;

	if (cpu < 0)
		err = rdmsr_safe(MSR_IA32_TEMPERATURE_TARGET, &low, &high);
	else
		err = rdmsr_safe_on_cpu(cpu, MSR_IA32_TEMPERATURE_TARGET, &low, &high);
	if (err)
		return err;

	return (low >> 24) & 0x3f;
}
EXPORT_SYMBOL_NS_GPL(intel_tcc_get_offset, INTEL_TCC);

 

int intel_tcc_set_offset(int cpu, int offset)
{
	u32 low, high;
	int err;

	if (offset < 0 || offset > 0x3f)
		return -EINVAL;

	if (cpu < 0)
		err = rdmsr_safe(MSR_IA32_TEMPERATURE_TARGET, &low, &high);
	else
		err = rdmsr_safe_on_cpu(cpu, MSR_IA32_TEMPERATURE_TARGET, &low, &high);
	if (err)
		return err;

	 
	if (low & BIT(31))
		return -EPERM;

	low &= ~(0x3f << 24);
	low |= offset << 24;

	if (cpu < 0)
		return wrmsr_safe(MSR_IA32_TEMPERATURE_TARGET, low, high);
	else
		return wrmsr_safe_on_cpu(cpu, MSR_IA32_TEMPERATURE_TARGET, low, high);
}
EXPORT_SYMBOL_NS_GPL(intel_tcc_set_offset, INTEL_TCC);

 
int intel_tcc_get_temp(int cpu, bool pkg)
{
	u32 low, high;
	u32 msr = pkg ? MSR_IA32_PACKAGE_THERM_STATUS : MSR_IA32_THERM_STATUS;
	int tjmax, temp, err;

	tjmax = intel_tcc_get_tjmax(cpu);
	if (tjmax < 0)
		return tjmax;

	if (cpu < 0)
		err = rdmsr_safe(msr, &low, &high);
	else
		err = rdmsr_safe_on_cpu(cpu, msr, &low, &high);
	if (err)
		return err;

	 
	if (!(low & BIT(31)))
		return -ENODATA;

	temp = tjmax - ((low >> 16) & 0x7f);

	 
	return temp >= 0 ? temp : -ENODATA;
}
EXPORT_SYMBOL_NS_GPL(intel_tcc_get_temp, INTEL_TCC);
