
 

#include <linux/types.h>
#include <linux/time.h>
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/hardirq.h>
#include <linux/efi.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kexec.h>
#include <linux/i8253.h>
#include <linux/random.h>
#include <asm/processor.h>
#include <asm/hypervisor.h>
#include <asm/hyperv-tlfs.h>
#include <asm/mshyperv.h>
#include <asm/desc.h>
#include <asm/idtentry.h>
#include <asm/irq_regs.h>
#include <asm/i8259.h>
#include <asm/apic.h>
#include <asm/timer.h>
#include <asm/reboot.h>
#include <asm/nmi.h>
#include <clocksource/hyperv_timer.h>
#include <asm/numa.h>
#include <asm/svm.h>

 
bool hv_root_partition;
 
bool hv_nested;
struct ms_hyperv_info ms_hyperv;

 
bool hyperv_paravisor_present __ro_after_init;
EXPORT_SYMBOL_GPL(hyperv_paravisor_present);

#if IS_ENABLED(CONFIG_HYPERV)
static inline unsigned int hv_get_nested_reg(unsigned int reg)
{
	if (hv_is_sint_reg(reg))
		return reg - HV_REGISTER_SINT0 + HV_REGISTER_NESTED_SINT0;

	switch (reg) {
	case HV_REGISTER_SIMP:
		return HV_REGISTER_NESTED_SIMP;
	case HV_REGISTER_SIEFP:
		return HV_REGISTER_NESTED_SIEFP;
	case HV_REGISTER_SVERSION:
		return HV_REGISTER_NESTED_SVERSION;
	case HV_REGISTER_SCONTROL:
		return HV_REGISTER_NESTED_SCONTROL;
	case HV_REGISTER_EOM:
		return HV_REGISTER_NESTED_EOM;
	default:
		return reg;
	}
}

u64 hv_get_non_nested_register(unsigned int reg)
{
	u64 value;

	if (hv_is_synic_reg(reg) && ms_hyperv.paravisor_present)
		hv_ivm_msr_read(reg, &value);
	else
		rdmsrl(reg, value);
	return value;
}
EXPORT_SYMBOL_GPL(hv_get_non_nested_register);

void hv_set_non_nested_register(unsigned int reg, u64 value)
{
	if (hv_is_synic_reg(reg) && ms_hyperv.paravisor_present) {
		hv_ivm_msr_write(reg, value);

		 
		if (hv_is_sint_reg(reg))
			wrmsrl(reg, value | 1 << 20);
	} else {
		wrmsrl(reg, value);
	}
}
EXPORT_SYMBOL_GPL(hv_set_non_nested_register);

u64 hv_get_register(unsigned int reg)
{
	if (hv_nested)
		reg = hv_get_nested_reg(reg);

	return hv_get_non_nested_register(reg);
}
EXPORT_SYMBOL_GPL(hv_get_register);

void hv_set_register(unsigned int reg, u64 value)
{
	if (hv_nested)
		reg = hv_get_nested_reg(reg);

	hv_set_non_nested_register(reg, value);
}
EXPORT_SYMBOL_GPL(hv_set_register);

static void (*vmbus_handler)(void);
static void (*hv_stimer0_handler)(void);
static void (*hv_kexec_handler)(void);
static void (*hv_crash_handler)(struct pt_regs *regs);

DEFINE_IDTENTRY_SYSVEC(sysvec_hyperv_callback)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	inc_irq_stat(irq_hv_callback_count);
	if (vmbus_handler)
		vmbus_handler();

	if (ms_hyperv.hints & HV_DEPRECATING_AEOI_RECOMMENDED)
		apic_eoi();

	set_irq_regs(old_regs);
}

void hv_setup_vmbus_handler(void (*handler)(void))
{
	vmbus_handler = handler;
}

void hv_remove_vmbus_handler(void)
{
	 
	vmbus_handler = NULL;
}

 
DEFINE_IDTENTRY_SYSVEC(sysvec_hyperv_stimer0)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	inc_irq_stat(hyperv_stimer0_count);
	if (hv_stimer0_handler)
		hv_stimer0_handler();
	add_interrupt_randomness(HYPERV_STIMER0_VECTOR);
	apic_eoi();

	set_irq_regs(old_regs);
}

 
void hv_setup_stimer0_handler(void (*handler)(void))
{
	hv_stimer0_handler = handler;
}

void hv_remove_stimer0_handler(void)
{
	 
	hv_stimer0_handler = NULL;
}

void hv_setup_kexec_handler(void (*handler)(void))
{
	hv_kexec_handler = handler;
}

void hv_remove_kexec_handler(void)
{
	hv_kexec_handler = NULL;
}

void hv_setup_crash_handler(void (*handler)(struct pt_regs *regs))
{
	hv_crash_handler = handler;
}

void hv_remove_crash_handler(void)
{
	hv_crash_handler = NULL;
}

#ifdef CONFIG_KEXEC_CORE
static void hv_machine_shutdown(void)
{
	if (kexec_in_progress && hv_kexec_handler)
		hv_kexec_handler();

	 
	if (kexec_in_progress && hyperv_init_cpuhp > 0)
		cpuhp_remove_state(hyperv_init_cpuhp);

	 
	native_machine_shutdown();

	 
	if (kexec_in_progress)
		hyperv_cleanup();
}

static void hv_machine_crash_shutdown(struct pt_regs *regs)
{
	if (hv_crash_handler)
		hv_crash_handler(regs);

	 
	native_machine_crash_shutdown(regs);

	 
	hyperv_cleanup();
}
#endif  
#endif  

static uint32_t  __init ms_hyperv_platform(void)
{
	u32 eax;
	u32 hyp_signature[3];

	if (!boot_cpu_has(X86_FEATURE_HYPERVISOR))
		return 0;

	cpuid(HYPERV_CPUID_VENDOR_AND_MAX_FUNCTIONS,
	      &eax, &hyp_signature[0], &hyp_signature[1], &hyp_signature[2]);

	if (eax < HYPERV_CPUID_MIN || eax > HYPERV_CPUID_MAX ||
	    memcmp("Microsoft Hv", hyp_signature, 12))
		return 0;

	 
	eax = cpuid_eax(HYPERV_CPUID_FEATURES);
	if (!(eax & HV_MSR_HYPERCALL_AVAILABLE)) {
		pr_warn("x86/hyperv: HYPERCALL MSR not available.\n");
		return 0;
	}
	if (!(eax & HV_MSR_VP_INDEX_AVAILABLE)) {
		pr_warn("x86/hyperv: VP_INDEX MSR not available.\n");
		return 0;
	}

	return HYPERV_CPUID_VENDOR_AND_MAX_FUNCTIONS;
}

#ifdef CONFIG_X86_LOCAL_APIC
 
static int hv_nmi_unknown(unsigned int val, struct pt_regs *regs)
{
	static atomic_t nmi_cpu = ATOMIC_INIT(-1);

	if (!unknown_nmi_panic)
		return NMI_DONE;

	if (atomic_cmpxchg(&nmi_cpu, -1, raw_smp_processor_id()) != -1)
		return NMI_HANDLED;

	return NMI_DONE;
}
#endif

static unsigned long hv_get_tsc_khz(void)
{
	unsigned long freq;

	rdmsrl(HV_X64_MSR_TSC_FREQUENCY, freq);

	return freq / 1000;
}

#if defined(CONFIG_SMP) && IS_ENABLED(CONFIG_HYPERV)
static void __init hv_smp_prepare_boot_cpu(void)
{
	native_smp_prepare_boot_cpu();
#if defined(CONFIG_X86_64) && defined(CONFIG_PARAVIRT_SPINLOCKS)
	hv_init_spinlocks();
#endif
}

static void __init hv_smp_prepare_cpus(unsigned int max_cpus)
{
#ifdef CONFIG_X86_64
	int i;
	int ret;
#endif

	native_smp_prepare_cpus(max_cpus);

	 
	if (!ms_hyperv.paravisor_present && hv_isolation_type_snp()) {
		apic->wakeup_secondary_cpu_64 = hv_snp_boot_ap;
		return;
	}

#ifdef CONFIG_X86_64
	for_each_present_cpu(i) {
		if (i == 0)
			continue;
		ret = hv_call_add_logical_proc(numa_cpu_node(i), i, cpu_physical_id(i));
		BUG_ON(ret);
	}

	for_each_present_cpu(i) {
		if (i == 0)
			continue;
		ret = hv_call_create_vp(numa_cpu_node(i), hv_current_partition_id, i, i);
		BUG_ON(ret);
	}
#endif
}
#endif

 
static void __init reduced_hw_init(void)
{
	x86_init.timers.timer_init	= x86_init_noop;
	x86_init.irqs.pre_vector_init	= x86_init_noop;
}

static void __init ms_hyperv_init_platform(void)
{
	int hv_max_functions_eax;
	int hv_host_info_eax;
	int hv_host_info_ebx;
	int hv_host_info_ecx;
	int hv_host_info_edx;

#ifdef CONFIG_PARAVIRT
	pv_info.name = "Hyper-V";
#endif

	 
	ms_hyperv.features = cpuid_eax(HYPERV_CPUID_FEATURES);
	ms_hyperv.priv_high = cpuid_ebx(HYPERV_CPUID_FEATURES);
	ms_hyperv.misc_features = cpuid_edx(HYPERV_CPUID_FEATURES);
	ms_hyperv.hints    = cpuid_eax(HYPERV_CPUID_ENLIGHTMENT_INFO);

	hv_max_functions_eax = cpuid_eax(HYPERV_CPUID_VENDOR_AND_MAX_FUNCTIONS);

	pr_info("Hyper-V: privilege flags low 0x%x, high 0x%x, hints 0x%x, misc 0x%x\n",
		ms_hyperv.features, ms_hyperv.priv_high, ms_hyperv.hints,
		ms_hyperv.misc_features);

	ms_hyperv.max_vp_index = cpuid_eax(HYPERV_CPUID_IMPLEMENT_LIMITS);
	ms_hyperv.max_lp_index = cpuid_ebx(HYPERV_CPUID_IMPLEMENT_LIMITS);

	pr_debug("Hyper-V: max %u virtual processors, %u logical processors\n",
		 ms_hyperv.max_vp_index, ms_hyperv.max_lp_index);

	 
	if ((ms_hyperv.priv_high & HV_CPU_MANAGEMENT) &&
	    !(ms_hyperv.priv_high & HV_ISOLATION)) {
		hv_root_partition = true;
		pr_info("Hyper-V: running as root partition\n");
	}

	if (ms_hyperv.hints & HV_X64_HYPERV_NESTED) {
		hv_nested = true;
		pr_info("Hyper-V: running on a nested hypervisor\n");
	}

	 
	if (hv_max_functions_eax >= HYPERV_CPUID_VERSION) {
		hv_host_info_eax = cpuid_eax(HYPERV_CPUID_VERSION);
		hv_host_info_ebx = cpuid_ebx(HYPERV_CPUID_VERSION);
		hv_host_info_ecx = cpuid_ecx(HYPERV_CPUID_VERSION);
		hv_host_info_edx = cpuid_edx(HYPERV_CPUID_VERSION);

		pr_info("Hyper-V: Host Build %d.%d.%d.%d-%d-%d\n",
			hv_host_info_ebx >> 16, hv_host_info_ebx & 0xFFFF,
			hv_host_info_eax, hv_host_info_edx & 0xFFFFFF,
			hv_host_info_ecx, hv_host_info_edx >> 24);
	}

	if (ms_hyperv.features & HV_ACCESS_FREQUENCY_MSRS &&
	    ms_hyperv.misc_features & HV_FEATURE_FREQUENCY_MSRS_AVAILABLE) {
		x86_platform.calibrate_tsc = hv_get_tsc_khz;
		x86_platform.calibrate_cpu = hv_get_tsc_khz;
	}

	if (ms_hyperv.priv_high & HV_ISOLATION) {
		ms_hyperv.isolation_config_a = cpuid_eax(HYPERV_CPUID_ISOLATION_CONFIG);
		ms_hyperv.isolation_config_b = cpuid_ebx(HYPERV_CPUID_ISOLATION_CONFIG);

		if (ms_hyperv.shared_gpa_boundary_active)
			ms_hyperv.shared_gpa_boundary =
				BIT_ULL(ms_hyperv.shared_gpa_boundary_bits);

		hyperv_paravisor_present = !!ms_hyperv.paravisor_present;

		pr_info("Hyper-V: Isolation Config: Group A 0x%x, Group B 0x%x\n",
			ms_hyperv.isolation_config_a, ms_hyperv.isolation_config_b);


		if (hv_get_isolation_type() == HV_ISOLATION_TYPE_SNP) {
			static_branch_enable(&isolation_type_snp);
		} else if (hv_get_isolation_type() == HV_ISOLATION_TYPE_TDX) {
			static_branch_enable(&isolation_type_tdx);

			 
			ms_hyperv.hints &= ~HV_X64_APIC_ACCESS_RECOMMENDED;

			if (!ms_hyperv.paravisor_present) {
				 
				ms_hyperv.features &= ~HV_MSR_REFERENCE_TSC_AVAILABLE;

				 
				ms_hyperv.misc_features &= ~HV_FEATURE_GUEST_CRASH_MSR_AVAILABLE;

				 
				ms_hyperv.hints &= ~HV_X64_REMOTE_TLB_FLUSH_RECOMMENDED;

				x86_init.acpi.reduced_hw_early_init = reduced_hw_init;
			}
		}
	}

	if (hv_max_functions_eax >= HYPERV_CPUID_NESTED_FEATURES) {
		ms_hyperv.nested_features =
			cpuid_eax(HYPERV_CPUID_NESTED_FEATURES);
		pr_info("Hyper-V: Nested features: 0x%x\n",
			ms_hyperv.nested_features);
	}

#ifdef CONFIG_X86_LOCAL_APIC
	if (ms_hyperv.features & HV_ACCESS_FREQUENCY_MSRS &&
	    ms_hyperv.misc_features & HV_FEATURE_FREQUENCY_MSRS_AVAILABLE) {
		 
		u64	hv_lapic_frequency;

		rdmsrl(HV_X64_MSR_APIC_FREQUENCY, hv_lapic_frequency);
		hv_lapic_frequency = div_u64(hv_lapic_frequency, HZ);
		lapic_timer_period = hv_lapic_frequency;
		pr_info("Hyper-V: LAPIC Timer Frequency: %#x\n",
			lapic_timer_period);
	}

	register_nmi_handler(NMI_UNKNOWN, hv_nmi_unknown, NMI_FLAG_FIRST,
			     "hv_nmi_unknown");
#endif

#ifdef CONFIG_X86_IO_APIC
	no_timer_check = 1;
#endif

#if IS_ENABLED(CONFIG_HYPERV) && defined(CONFIG_KEXEC_CORE)
	machine_ops.shutdown = hv_machine_shutdown;
	machine_ops.crash_shutdown = hv_machine_crash_shutdown;
#endif
	if (ms_hyperv.features & HV_ACCESS_TSC_INVARIANT) {
		 
		wrmsrl(HV_X64_MSR_TSC_INVARIANT_CONTROL, HV_EXPOSE_INVARIANT_TSC);
		setup_force_cpu_cap(X86_FEATURE_TSC_RELIABLE);
	}

	 
	if (efi_enabled(EFI_BOOT))
		x86_platform.get_nmi_reason = hv_get_nmi_reason;

	 
	i8253_clear_counter_on_shutdown = false;

#if IS_ENABLED(CONFIG_HYPERV)
	if ((hv_get_isolation_type() == HV_ISOLATION_TYPE_VBS) ||
	    ms_hyperv.paravisor_present)
		hv_vtom_init();
	 
	x86_platform.apic_post_init = hyperv_init;
	hyperv_setup_mmu_ops();
	 
	alloc_intr_gate(HYPERVISOR_CALLBACK_VECTOR, asm_sysvec_hyperv_callback);

	 
	if (ms_hyperv.features & HV_ACCESS_REENLIGHTENMENT) {
		alloc_intr_gate(HYPERV_REENLIGHTENMENT_VECTOR,
				asm_sysvec_hyperv_reenlightenment);
	}

	 
	if (ms_hyperv.misc_features & HV_STIMER_DIRECT_MODE_AVAILABLE) {
		alloc_intr_gate(HYPERV_STIMER0_VECTOR,
				asm_sysvec_hyperv_stimer0);
	}

# ifdef CONFIG_SMP
	smp_ops.smp_prepare_boot_cpu = hv_smp_prepare_boot_cpu;
	if (hv_root_partition ||
	    (!ms_hyperv.paravisor_present && hv_isolation_type_snp()))
		smp_ops.smp_prepare_cpus = hv_smp_prepare_cpus;
# endif

	 
# ifdef CONFIG_X86_X2APIC
	if (x2apic_supported())
		x2apic_phys = 1;
# endif

	 
	hv_init_clocksource();
	hv_vtl_init_platform();
#endif
	 
	if (!(ms_hyperv.features & HV_ACCESS_TSC_INVARIANT))
		mark_tsc_unstable("running on Hyper-V");

	hardlockup_detector_disable();
}

static bool __init ms_hyperv_x2apic_available(void)
{
	return x2apic_supported();
}

 
static bool __init ms_hyperv_msi_ext_dest_id(void)
{
	u32 eax;

	eax = cpuid_eax(HYPERV_CPUID_VIRT_STACK_INTERFACE);
	if (eax != HYPERV_VS_INTERFACE_EAX_SIGNATURE)
		return false;

	eax = cpuid_eax(HYPERV_CPUID_VIRT_STACK_PROPERTIES);
	return eax & HYPERV_VS_PROPERTIES_EAX_EXTENDED_IOAPIC_RTE;
}

#ifdef CONFIG_AMD_MEM_ENCRYPT
static void hv_sev_es_hcall_prepare(struct ghcb *ghcb, struct pt_regs *regs)
{
	 
	ghcb_set_rcx(ghcb, regs->cx);
	ghcb_set_rdx(ghcb, regs->dx);
	ghcb_set_r8(ghcb, regs->r8);
}

static bool hv_sev_es_hcall_finish(struct ghcb *ghcb, struct pt_regs *regs)
{
	 
	return true;
}
#endif

const __initconst struct hypervisor_x86 x86_hyper_ms_hyperv = {
	.name			= "Microsoft Hyper-V",
	.detect			= ms_hyperv_platform,
	.type			= X86_HYPER_MS_HYPERV,
	.init.x2apic_available	= ms_hyperv_x2apic_available,
	.init.msi_ext_dest_id	= ms_hyperv_msi_ext_dest_id,
	.init.init_platform	= ms_hyperv_init_platform,
#ifdef CONFIG_AMD_MEM_ENCRYPT
	.runtime.sev_es_hcall_prepare = hv_sev_es_hcall_prepare,
	.runtime.sev_es_hcall_finish = hv_sev_es_hcall_finish,
#endif
};
