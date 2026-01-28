#include "apic.h"
void apic_disable(void)
{
	wrmsr(MSR_IA32_APICBASE,
	      rdmsr(MSR_IA32_APICBASE) &
		~(MSR_IA32_APICBASE_ENABLE | MSR_IA32_APICBASE_EXTD));
}
void xapic_enable(void)
{
	uint64_t val = rdmsr(MSR_IA32_APICBASE);
	if (val & MSR_IA32_APICBASE_EXTD) {
		apic_disable();
		wrmsr(MSR_IA32_APICBASE,
		      rdmsr(MSR_IA32_APICBASE) | MSR_IA32_APICBASE_ENABLE);
	} else if (!(val & MSR_IA32_APICBASE_ENABLE)) {
		wrmsr(MSR_IA32_APICBASE, val | MSR_IA32_APICBASE_ENABLE);
	}
	val = xapic_read_reg(APIC_SPIV) | APIC_SPIV_APIC_ENABLED;
	xapic_write_reg(APIC_SPIV, val);
}
void x2apic_enable(void)
{
	wrmsr(MSR_IA32_APICBASE, rdmsr(MSR_IA32_APICBASE) |
	      MSR_IA32_APICBASE_ENABLE | MSR_IA32_APICBASE_EXTD);
	x2apic_write_reg(APIC_SPIV,
			 x2apic_read_reg(APIC_SPIV) | APIC_SPIV_APIC_ENABLED);
}
