
 

#define pr_fmt(fmt)  "Hyper-V: " fmt

#include <linux/efi.h>
#include <linux/types.h>
#include <linux/bitfield.h>
#include <linux/io.h>
#include <asm/apic.h>
#include <asm/desc.h>
#include <asm/e820/api.h>
#include <asm/sev.h>
#include <asm/ibt.h>
#include <asm/hypervisor.h>
#include <asm/hyperv-tlfs.h>
#include <asm/mshyperv.h>
#include <asm/idtentry.h>
#include <asm/set_memory.h>
#include <linux/kexec.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/hyperv.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/cpuhotplug.h>
#include <linux/syscore_ops.h>
#include <clocksource/hyperv_timer.h>
#include <linux/highmem.h>

int hyperv_init_cpuhp;
u64 hv_current_partition_id = ~0ull;
EXPORT_SYMBOL_GPL(hv_current_partition_id);

void *hv_hypercall_pg;
EXPORT_SYMBOL_GPL(hv_hypercall_pg);

union hv_ghcb * __percpu *hv_ghcb_pg;

 
static void *hv_hypercall_pg_saved;

struct hv_vp_assist_page **hv_vp_assist_page;
EXPORT_SYMBOL_GPL(hv_vp_assist_page);

static int hyperv_init_ghcb(void)
{
	u64 ghcb_gpa;
	void *ghcb_va;
	void **ghcb_base;

	if (!ms_hyperv.paravisor_present || !hv_isolation_type_snp())
		return 0;

	if (!hv_ghcb_pg)
		return -EINVAL;

	 
	rdmsrl(MSR_AMD64_SEV_ES_GHCB, ghcb_gpa);

	 
	ghcb_gpa &= ~ms_hyperv.shared_gpa_boundary;
	ghcb_va = (void *)ioremap_cache(ghcb_gpa, HV_HYP_PAGE_SIZE);
	if (!ghcb_va)
		return -ENOMEM;

	ghcb_base = (void **)this_cpu_ptr(hv_ghcb_pg);
	*ghcb_base = ghcb_va;

	return 0;
}

static int hv_cpu_init(unsigned int cpu)
{
	union hv_vp_assist_msr_contents msr = { 0 };
	struct hv_vp_assist_page **hvp;
	int ret;

	ret = hv_common_cpu_init(cpu);
	if (ret)
		return ret;

	if (!hv_vp_assist_page)
		return 0;

	hvp = &hv_vp_assist_page[cpu];
	if (hv_root_partition) {
		 
		rdmsrl(HV_X64_MSR_VP_ASSIST_PAGE, msr.as_uint64);
		*hvp = memremap(msr.pfn << HV_X64_MSR_VP_ASSIST_PAGE_ADDRESS_SHIFT,
				PAGE_SIZE, MEMREMAP_WB);
	} else {
		 
		if (!*hvp) {
			*hvp = __vmalloc(PAGE_SIZE, GFP_KERNEL | __GFP_ZERO);

			 
			if (*hvp && !ms_hyperv.paravisor_present && hv_isolation_type_snp()) {
				WARN_ON_ONCE(set_memory_decrypted((unsigned long)(*hvp), 1));
				memset(*hvp, 0, PAGE_SIZE);
			}
		}

		if (*hvp)
			msr.pfn = vmalloc_to_pfn(*hvp);

	}
	if (!WARN_ON(!(*hvp))) {
		msr.enable = 1;
		wrmsrl(HV_X64_MSR_VP_ASSIST_PAGE, msr.as_uint64);
	}

	return hyperv_init_ghcb();
}

static void (*hv_reenlightenment_cb)(void);

static void hv_reenlightenment_notify(struct work_struct *dummy)
{
	struct hv_tsc_emulation_status emu_status;

	rdmsrl(HV_X64_MSR_TSC_EMULATION_STATUS, *(u64 *)&emu_status);

	 
	if (hv_reenlightenment_cb && emu_status.inprogress)
		hv_reenlightenment_cb();
}
static DECLARE_DELAYED_WORK(hv_reenlightenment_work, hv_reenlightenment_notify);

void hyperv_stop_tsc_emulation(void)
{
	u64 freq;
	struct hv_tsc_emulation_status emu_status;

	rdmsrl(HV_X64_MSR_TSC_EMULATION_STATUS, *(u64 *)&emu_status);
	emu_status.inprogress = 0;
	wrmsrl(HV_X64_MSR_TSC_EMULATION_STATUS, *(u64 *)&emu_status);

	rdmsrl(HV_X64_MSR_TSC_FREQUENCY, freq);
	tsc_khz = div64_u64(freq, 1000);
}
EXPORT_SYMBOL_GPL(hyperv_stop_tsc_emulation);

static inline bool hv_reenlightenment_available(void)
{
	 
	return ms_hyperv.features & HV_ACCESS_FREQUENCY_MSRS &&
		ms_hyperv.misc_features & HV_FEATURE_FREQUENCY_MSRS_AVAILABLE &&
		ms_hyperv.features & HV_ACCESS_REENLIGHTENMENT;
}

DEFINE_IDTENTRY_SYSVEC(sysvec_hyperv_reenlightenment)
{
	apic_eoi();
	inc_irq_stat(irq_hv_reenlightenment_count);
	schedule_delayed_work(&hv_reenlightenment_work, HZ/10);
}

void set_hv_tscchange_cb(void (*cb)(void))
{
	struct hv_reenlightenment_control re_ctrl = {
		.vector = HYPERV_REENLIGHTENMENT_VECTOR,
		.enabled = 1,
	};
	struct hv_tsc_emulation_control emu_ctrl = {.enabled = 1};

	if (!hv_reenlightenment_available()) {
		pr_warn("reenlightenment support is unavailable\n");
		return;
	}

	if (!hv_vp_index)
		return;

	hv_reenlightenment_cb = cb;

	 
	wmb();

	re_ctrl.target_vp = hv_vp_index[get_cpu()];

	wrmsrl(HV_X64_MSR_REENLIGHTENMENT_CONTROL, *((u64 *)&re_ctrl));
	wrmsrl(HV_X64_MSR_TSC_EMULATION_CONTROL, *((u64 *)&emu_ctrl));

	put_cpu();
}
EXPORT_SYMBOL_GPL(set_hv_tscchange_cb);

void clear_hv_tscchange_cb(void)
{
	struct hv_reenlightenment_control re_ctrl;

	if (!hv_reenlightenment_available())
		return;

	rdmsrl(HV_X64_MSR_REENLIGHTENMENT_CONTROL, *(u64 *)&re_ctrl);
	re_ctrl.enabled = 0;
	wrmsrl(HV_X64_MSR_REENLIGHTENMENT_CONTROL, *(u64 *)&re_ctrl);

	hv_reenlightenment_cb = NULL;
}
EXPORT_SYMBOL_GPL(clear_hv_tscchange_cb);

static int hv_cpu_die(unsigned int cpu)
{
	struct hv_reenlightenment_control re_ctrl;
	unsigned int new_cpu;
	void **ghcb_va;

	if (hv_ghcb_pg) {
		ghcb_va = (void **)this_cpu_ptr(hv_ghcb_pg);
		if (*ghcb_va)
			iounmap(*ghcb_va);
		*ghcb_va = NULL;
	}

	hv_common_cpu_die(cpu);

	if (hv_vp_assist_page && hv_vp_assist_page[cpu]) {
		union hv_vp_assist_msr_contents msr = { 0 };
		if (hv_root_partition) {
			 
			memunmap(hv_vp_assist_page[cpu]);
			hv_vp_assist_page[cpu] = NULL;
			rdmsrl(HV_X64_MSR_VP_ASSIST_PAGE, msr.as_uint64);
			msr.enable = 0;
		}
		wrmsrl(HV_X64_MSR_VP_ASSIST_PAGE, msr.as_uint64);
	}

	if (hv_reenlightenment_cb == NULL)
		return 0;

	rdmsrl(HV_X64_MSR_REENLIGHTENMENT_CONTROL, *((u64 *)&re_ctrl));
	if (re_ctrl.target_vp == hv_vp_index[cpu]) {
		 
		new_cpu = cpumask_any_but(cpu_online_mask, cpu);

		if (new_cpu < nr_cpu_ids)
			re_ctrl.target_vp = hv_vp_index[new_cpu];
		else
			re_ctrl.enabled = 0;

		wrmsrl(HV_X64_MSR_REENLIGHTENMENT_CONTROL, *((u64 *)&re_ctrl));
	}

	return 0;
}

static int __init hv_pci_init(void)
{
	bool gen2vm = efi_enabled(EFI_BOOT);

	 
	if (gen2vm) {
		e820__reserve_resources_late();
		return 0;
	}

	 
	return 1;
}

static int hv_suspend(void)
{
	union hv_x64_msr_hypercall_contents hypercall_msr;
	int ret;

	if (hv_root_partition)
		return -EPERM;

	 
	hv_hypercall_pg_saved = hv_hypercall_pg;
	hv_hypercall_pg = NULL;

	 
	rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
	hypercall_msr.enable = 0;
	wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);

	ret = hv_cpu_die(0);
	return ret;
}

static void hv_resume(void)
{
	union hv_x64_msr_hypercall_contents hypercall_msr;
	int ret;

	ret = hv_cpu_init(0);
	WARN_ON(ret);

	 
	rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
	hypercall_msr.enable = 1;
	hypercall_msr.guest_physical_address =
		vmalloc_to_pfn(hv_hypercall_pg_saved);
	wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);

	hv_hypercall_pg = hv_hypercall_pg_saved;
	hv_hypercall_pg_saved = NULL;

	 
	if (hv_reenlightenment_cb)
		set_hv_tscchange_cb(hv_reenlightenment_cb);
}

 
static struct syscore_ops hv_syscore_ops = {
	.suspend	= hv_suspend,
	.resume		= hv_resume,
};

static void (* __initdata old_setup_percpu_clockev)(void);

static void __init hv_stimer_setup_percpu_clockev(void)
{
	 
	(void)hv_stimer_alloc(false);

	 
	if (old_setup_percpu_clockev)
		old_setup_percpu_clockev();
}

static void __init hv_get_partition_id(void)
{
	struct hv_get_partition_id *output_page;
	u64 status;
	unsigned long flags;

	local_irq_save(flags);
	output_page = *this_cpu_ptr(hyperv_pcpu_output_arg);
	status = hv_do_hypercall(HVCALL_GET_PARTITION_ID, NULL, output_page);
	if (!hv_result_success(status)) {
		 
		pr_err("Failed to get partition ID: %lld\n", status);
		BUG();
	}
	hv_current_partition_id = output_page->partition_id;
	local_irq_restore(flags);
}

#if IS_ENABLED(CONFIG_HYPERV_VTL_MODE)
static u8 __init get_vtl(void)
{
	u64 control = HV_HYPERCALL_REP_COMP_1 | HVCALL_GET_VP_REGISTERS;
	struct hv_get_vp_registers_input *input;
	struct hv_get_vp_registers_output *output;
	unsigned long flags;
	u64 ret;

	local_irq_save(flags);
	input = *this_cpu_ptr(hyperv_pcpu_input_arg);
	output = (struct hv_get_vp_registers_output *)input;

	memset(input, 0, struct_size(input, element, 1));
	input->header.partitionid = HV_PARTITION_ID_SELF;
	input->header.vpindex = HV_VP_INDEX_SELF;
	input->header.inputvtl = 0;
	input->element[0].name0 = HV_X64_REGISTER_VSM_VP_STATUS;

	ret = hv_do_hypercall(control, input, output);
	if (hv_result_success(ret)) {
		ret = output->as64.low & HV_X64_VTL_MASK;
	} else {
		pr_err("Failed to get VTL(error: %lld) exiting...\n", ret);
		BUG();
	}

	local_irq_restore(flags);
	return ret;
}
#else
static inline u8 get_vtl(void) { return 0; }
#endif

 
void __init hyperv_init(void)
{
	u64 guest_id;
	union hv_x64_msr_hypercall_contents hypercall_msr;
	int cpuhp;

	if (x86_hyper_type != X86_HYPER_MS_HYPERV)
		return;

	if (hv_common_init())
		return;

	 
	if (hv_isolation_type_tdx())
		hv_vp_assist_page = NULL;
	else
		hv_vp_assist_page = kcalloc(num_possible_cpus(),
					    sizeof(*hv_vp_assist_page),
					    GFP_KERNEL);
	if (!hv_vp_assist_page) {
		ms_hyperv.hints &= ~HV_X64_ENLIGHTENED_VMCS_RECOMMENDED;

		if (!hv_isolation_type_tdx())
			goto common_free;
	}

	if (ms_hyperv.paravisor_present && hv_isolation_type_snp()) {
		 
		if (!hv_ghcb_negotiate_protocol())
			hv_ghcb_terminate(SEV_TERM_SET_GEN,
					  GHCB_SEV_ES_PROT_UNSUPPORTED);

		hv_ghcb_pg = alloc_percpu(union hv_ghcb *);
		if (!hv_ghcb_pg)
			goto free_vp_assist_page;
	}

	cpuhp = cpuhp_setup_state(CPUHP_AP_HYPERV_ONLINE, "x86/hyperv_init:online",
				  hv_cpu_init, hv_cpu_die);
	if (cpuhp < 0)
		goto free_ghcb_page;

	 
	guest_id = hv_generate_guest_id(LINUX_VERSION_CODE);
	wrmsrl(HV_X64_MSR_GUEST_OS_ID, guest_id);

	 
	hv_ivm_msr_write(HV_X64_MSR_GUEST_OS_ID, guest_id);

	 
	if (hv_isolation_type_tdx() && !ms_hyperv.paravisor_present)
		goto skip_hypercall_pg_init;

	hv_hypercall_pg = __vmalloc_node_range(PAGE_SIZE, 1, VMALLOC_START,
			VMALLOC_END, GFP_KERNEL, PAGE_KERNEL_ROX,
			VM_FLUSH_RESET_PERMS, NUMA_NO_NODE,
			__builtin_return_address(0));
	if (hv_hypercall_pg == NULL)
		goto clean_guest_os_id;

	rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
	hypercall_msr.enable = 1;

	if (hv_root_partition) {
		struct page *pg;
		void *src;

		 
		wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);

		pg = vmalloc_to_page(hv_hypercall_pg);
		src = memremap(hypercall_msr.guest_physical_address << PAGE_SHIFT, PAGE_SIZE,
				MEMREMAP_WB);
		BUG_ON(!src);
		memcpy_to_page(pg, 0, src, HV_HYP_PAGE_SIZE);
		memunmap(src);

		hv_remap_tsc_clocksource();
	} else {
		hypercall_msr.guest_physical_address = vmalloc_to_pfn(hv_hypercall_pg);
		wrmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);
	}

skip_hypercall_pg_init:
	 
#ifdef CONFIG_X86_KERNEL_IBT
	if (cpu_feature_enabled(X86_FEATURE_IBT) &&
	    *(u32 *)hv_hypercall_pg != gen_endbr()) {
		setup_clear_cpu_cap(X86_FEATURE_IBT);
		pr_warn("Disabling IBT because of Hyper-V bug\n");
	}
#endif

	 
	old_setup_percpu_clockev = x86_init.timers.setup_percpu_clockev;
	x86_init.timers.setup_percpu_clockev = hv_stimer_setup_percpu_clockev;

	hv_apic_init();

	x86_init.pci.arch_init = hv_pci_init;

	register_syscore_ops(&hv_syscore_ops);

	hyperv_init_cpuhp = cpuhp;

	if (cpuid_ebx(HYPERV_CPUID_FEATURES) & HV_ACCESS_PARTITION_ID)
		hv_get_partition_id();

	BUG_ON(hv_root_partition && hv_current_partition_id == ~0ull);

#ifdef CONFIG_PCI_MSI
	 
	if (hv_root_partition)
		x86_init.irqs.create_pci_msi_domain = hv_create_pci_msi_domain;
#endif

	 
	hv_query_ext_cap(0);

	 
	ms_hyperv.vtl = get_vtl();

	if (ms_hyperv.vtl > 0)  
		hv_vtl_early_init();

	return;

clean_guest_os_id:
	wrmsrl(HV_X64_MSR_GUEST_OS_ID, 0);
	hv_ivm_msr_write(HV_X64_MSR_GUEST_OS_ID, 0);
	cpuhp_remove_state(cpuhp);
free_ghcb_page:
	free_percpu(hv_ghcb_pg);
free_vp_assist_page:
	kfree(hv_vp_assist_page);
	hv_vp_assist_page = NULL;
common_free:
	hv_common_free();
}

 
void hyperv_cleanup(void)
{
	union hv_x64_msr_hypercall_contents hypercall_msr;
	union hv_reference_tsc_msr tsc_msr;

	 
	wrmsrl(HV_X64_MSR_GUEST_OS_ID, 0);
	hv_ivm_msr_write(HV_X64_MSR_GUEST_OS_ID, 0);

	 
	hv_hypercall_pg = NULL;

	 
	hypercall_msr.as_uint64 = hv_get_register(HV_X64_MSR_HYPERCALL);
	hypercall_msr.enable = 0;
	hv_set_register(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);

	 
	tsc_msr.as_uint64 = hv_get_register(HV_X64_MSR_REFERENCE_TSC);
	tsc_msr.enable = 0;
	hv_set_register(HV_X64_MSR_REFERENCE_TSC, tsc_msr.as_uint64);
}

void hyperv_report_panic(struct pt_regs *regs, long err, bool in_die)
{
	static bool panic_reported;
	u64 guest_id;

	if (in_die && !panic_on_oops)
		return;

	 
	if (panic_reported)
		return;
	panic_reported = true;

	rdmsrl(HV_X64_MSR_GUEST_OS_ID, guest_id);

	wrmsrl(HV_X64_MSR_CRASH_P0, err);
	wrmsrl(HV_X64_MSR_CRASH_P1, guest_id);
	wrmsrl(HV_X64_MSR_CRASH_P2, regs->ip);
	wrmsrl(HV_X64_MSR_CRASH_P3, regs->ax);
	wrmsrl(HV_X64_MSR_CRASH_P4, regs->sp);

	 
	wrmsrl(HV_X64_MSR_CRASH_CTL, HV_CRASH_CTL_CRASH_NOTIFY);
}
EXPORT_SYMBOL_GPL(hyperv_report_panic);

bool hv_is_hyperv_initialized(void)
{
	union hv_x64_msr_hypercall_contents hypercall_msr;

	 
	if (x86_hyper_type != X86_HYPER_MS_HYPERV)
		return false;

	 
	if (hv_isolation_type_tdx() && !ms_hyperv.paravisor_present)
		return true;
	 
	hypercall_msr.as_uint64 = 0;
	rdmsrl(HV_X64_MSR_HYPERCALL, hypercall_msr.as_uint64);

	return hypercall_msr.enable;
}
EXPORT_SYMBOL_GPL(hv_is_hyperv_initialized);
