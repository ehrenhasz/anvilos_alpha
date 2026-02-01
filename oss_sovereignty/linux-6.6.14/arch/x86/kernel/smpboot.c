
  

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/smp.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/sched/topology.h>
#include <linux/sched/hotplug.h>
#include <linux/sched/task_stack.h>
#include <linux/percpu.h>
#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/nmi.h>
#include <linux/tboot.h>
#include <linux/gfp.h>
#include <linux/cpuidle.h>
#include <linux/kexec.h>
#include <linux/numa.h>
#include <linux/pgtable.h>
#include <linux/overflow.h>
#include <linux/stackprotector.h>
#include <linux/cpuhotplug.h>
#include <linux/mc146818rtc.h>

#include <asm/acpi.h>
#include <asm/cacheinfo.h>
#include <asm/desc.h>
#include <asm/nmi.h>
#include <asm/irq.h>
#include <asm/realmode.h>
#include <asm/cpu.h>
#include <asm/numa.h>
#include <asm/tlbflush.h>
#include <asm/mtrr.h>
#include <asm/mwait.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/fpu/api.h>
#include <asm/setup.h>
#include <asm/uv/uv.h>
#include <asm/microcode.h>
#include <asm/i8259.h>
#include <asm/misc.h>
#include <asm/qspinlock.h>
#include <asm/intel-family.h>
#include <asm/cpu_device_id.h>
#include <asm/spec-ctrl.h>
#include <asm/hw_irq.h>
#include <asm/stackprotector.h>
#include <asm/sev.h>

 
DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);

 
DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_core_map);
EXPORT_PER_CPU_SYMBOL(cpu_core_map);

 
DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_die_map);
EXPORT_PER_CPU_SYMBOL(cpu_die_map);

 
DEFINE_PER_CPU_READ_MOSTLY(struct cpuinfo_x86, cpu_info);
EXPORT_PER_CPU_SYMBOL(cpu_info);

 
struct cpumask __cpu_primary_thread_mask __read_mostly;

 
static cpumask_var_t cpu_sibling_setup_mask;

struct mwait_cpu_dead {
	unsigned int	control;
	unsigned int	status;
};

#define CPUDEAD_MWAIT_WAIT	0xDEADBEEF
#define CPUDEAD_MWAIT_KEXEC_HLT	0x4A17DEAD

 
static DEFINE_PER_CPU_ALIGNED(struct mwait_cpu_dead, mwait_cpu_dead);

 
unsigned int __max_logical_packages __read_mostly;
EXPORT_SYMBOL(__max_logical_packages);
static unsigned int logical_packages __read_mostly;
static unsigned int logical_die __read_mostly;

 
int __read_mostly __max_smt_threads = 1;

 
bool x86_topology_update;

int arch_update_cpu_topology(void)
{
	int retval = x86_topology_update;

	x86_topology_update = false;
	return retval;
}

static unsigned int smpboot_warm_reset_vector_count;

static inline void smpboot_setup_warm_reset_vector(unsigned long start_eip)
{
	unsigned long flags;

	spin_lock_irqsave(&rtc_lock, flags);
	if (!smpboot_warm_reset_vector_count++) {
		CMOS_WRITE(0xa, 0xf);
		*((volatile unsigned short *)phys_to_virt(TRAMPOLINE_PHYS_HIGH)) = start_eip >> 4;
		*((volatile unsigned short *)phys_to_virt(TRAMPOLINE_PHYS_LOW)) = start_eip & 0xf;
	}
	spin_unlock_irqrestore(&rtc_lock, flags);
}

static inline void smpboot_restore_warm_reset_vector(void)
{
	unsigned long flags;

	 
	spin_lock_irqsave(&rtc_lock, flags);
	if (!--smpboot_warm_reset_vector_count) {
		CMOS_WRITE(0, 0xf);
		*((volatile u32 *)phys_to_virt(TRAMPOLINE_PHYS_LOW)) = 0;
	}
	spin_unlock_irqrestore(&rtc_lock, flags);

}

 
static void ap_starting(void)
{
	int cpuid = smp_processor_id();

	 
	this_cpu_write(mwait_cpu_dead.status, 0);
	this_cpu_write(mwait_cpu_dead.control, 0);

	 
	apic_ap_setup();

	 
	smp_store_cpu_info(cpuid);

	 
	set_cpu_sibling_map(cpuid);

	ap_init_aperfmperf();

	pr_debug("Stack at about %p\n", &cpuid);

	wmb();

	 
	notify_cpu_starting(cpuid);
}

static void ap_calibrate_delay(void)
{
	 
	calibrate_delay();
	cpu_data(smp_processor_id()).loops_per_jiffy = loops_per_jiffy;
}

 
static void notrace start_secondary(void *unused)
{
	 
	cr4_init();

	 
	if (IS_ENABLED(CONFIG_X86_32)) {
		 
		load_cr3(swapper_pg_dir);
		__flush_tlb_all();
	}

	cpu_init_exception_handling();

	 
	if (IS_ENABLED(CONFIG_X86_64))
		load_ucode_ap();

	 
	cpuhp_ap_sync_alive();

	cpu_init();
	fpu__init_cpu();
	rcu_cpu_starting(raw_smp_processor_id());
	x86_cpuinit.early_percpu_clock_init();

	ap_starting();

	 
	check_tsc_sync_target();

	 
	ap_calibrate_delay();

	speculative_store_bypass_ht_init();

	 
	lock_vector_lock();
	set_cpu_online(smp_processor_id(), true);
	lapic_online();
	unlock_vector_lock();
	x86_platform.nmi_init();

	 
	local_irq_enable();

	x86_cpuinit.setup_percpu_clockev();

	wmb();
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}

 
int topology_phys_to_logical_pkg(unsigned int phys_pkg)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct cpuinfo_x86 *c = &cpu_data(cpu);

		if (c->initialized && c->phys_proc_id == phys_pkg)
			return c->logical_proc_id;
	}
	return -1;
}
EXPORT_SYMBOL(topology_phys_to_logical_pkg);

 
static int topology_phys_to_logical_die(unsigned int die_id, unsigned int cur_cpu)
{
	int cpu, proc_id = cpu_data(cur_cpu).phys_proc_id;

	for_each_possible_cpu(cpu) {
		struct cpuinfo_x86 *c = &cpu_data(cpu);

		if (c->initialized && c->cpu_die_id == die_id &&
		    c->phys_proc_id == proc_id)
			return c->logical_die_id;
	}
	return -1;
}

 
int topology_update_package_map(unsigned int pkg, unsigned int cpu)
{
	int new;

	 
	new = topology_phys_to_logical_pkg(pkg);
	if (new >= 0)
		goto found;

	new = logical_packages++;
	if (new != pkg) {
		pr_info("CPU %u Converting physical %u to logical package %u\n",
			cpu, pkg, new);
	}
found:
	cpu_data(cpu).logical_proc_id = new;
	return 0;
}
 
int topology_update_die_map(unsigned int die, unsigned int cpu)
{
	int new;

	 
	new = topology_phys_to_logical_die(die, cpu);
	if (new >= 0)
		goto found;

	new = logical_die++;
	if (new != die) {
		pr_info("CPU %u Converting physical %u to logical die %u\n",
			cpu, die, new);
	}
found:
	cpu_data(cpu).logical_die_id = new;
	return 0;
}

static void __init smp_store_boot_cpu_info(void)
{
	int id = 0;  
	struct cpuinfo_x86 *c = &cpu_data(id);

	*c = boot_cpu_data;
	c->cpu_index = id;
	topology_update_package_map(c->phys_proc_id, id);
	topology_update_die_map(c->cpu_die_id, id);
	c->initialized = true;
}

 
void smp_store_cpu_info(int id)
{
	struct cpuinfo_x86 *c = &cpu_data(id);

	 
	if (!c->initialized)
		*c = boot_cpu_data;
	c->cpu_index = id;
	 
	identify_secondary_cpu(c);
	c->initialized = true;
}

static bool
topology_same_node(struct cpuinfo_x86 *c, struct cpuinfo_x86 *o)
{
	int cpu1 = c->cpu_index, cpu2 = o->cpu_index;

	return (cpu_to_node(cpu1) == cpu_to_node(cpu2));
}

static bool
topology_sane(struct cpuinfo_x86 *c, struct cpuinfo_x86 *o, const char *name)
{
	int cpu1 = c->cpu_index, cpu2 = o->cpu_index;

	return !WARN_ONCE(!topology_same_node(c, o),
		"sched: CPU #%d's %s-sibling CPU #%d is not on the same node! "
		"[node: %d != %d]. Ignoring dependency.\n",
		cpu1, name, cpu2, cpu_to_node(cpu1), cpu_to_node(cpu2));
}

#define link_mask(mfunc, c1, c2)					\
do {									\
	cpumask_set_cpu((c1), mfunc(c2));				\
	cpumask_set_cpu((c2), mfunc(c1));				\
} while (0)

static bool match_smt(struct cpuinfo_x86 *c, struct cpuinfo_x86 *o)
{
	if (boot_cpu_has(X86_FEATURE_TOPOEXT)) {
		int cpu1 = c->cpu_index, cpu2 = o->cpu_index;

		if (c->phys_proc_id == o->phys_proc_id &&
		    c->cpu_die_id == o->cpu_die_id &&
		    per_cpu(cpu_llc_id, cpu1) == per_cpu(cpu_llc_id, cpu2)) {
			if (c->cpu_core_id == o->cpu_core_id)
				return topology_sane(c, o, "smt");

			if ((c->cu_id != 0xff) &&
			    (o->cu_id != 0xff) &&
			    (c->cu_id == o->cu_id))
				return topology_sane(c, o, "smt");
		}

	} else if (c->phys_proc_id == o->phys_proc_id &&
		   c->cpu_die_id == o->cpu_die_id &&
		   c->cpu_core_id == o->cpu_core_id) {
		return topology_sane(c, o, "smt");
	}

	return false;
}

static bool match_die(struct cpuinfo_x86 *c, struct cpuinfo_x86 *o)
{
	if (c->phys_proc_id == o->phys_proc_id &&
	    c->cpu_die_id == o->cpu_die_id)
		return true;
	return false;
}

static bool match_l2c(struct cpuinfo_x86 *c, struct cpuinfo_x86 *o)
{
	int cpu1 = c->cpu_index, cpu2 = o->cpu_index;

	 
	if (per_cpu(cpu_l2c_id, cpu1) == BAD_APICID)
		return match_smt(c, o);

	 
	if (per_cpu(cpu_l2c_id, cpu1) != per_cpu(cpu_l2c_id, cpu2))
		return false;

	return topology_sane(c, o, "l2c");
}

 
static bool match_pkg(struct cpuinfo_x86 *c, struct cpuinfo_x86 *o)
{
	if (c->phys_proc_id == o->phys_proc_id)
		return true;
	return false;
}

 

static const struct x86_cpu_id intel_cod_cpu[] = {
	X86_MATCH_INTEL_FAM6_MODEL(HASWELL_X, 0),	 
	X86_MATCH_INTEL_FAM6_MODEL(BROADWELL_X, 0),	 
	X86_MATCH_INTEL_FAM6_MODEL(ANY, 1),		 
	{}
};

static bool match_llc(struct cpuinfo_x86 *c, struct cpuinfo_x86 *o)
{
	const struct x86_cpu_id *id = x86_match_cpu(intel_cod_cpu);
	int cpu1 = c->cpu_index, cpu2 = o->cpu_index;
	bool intel_snc = id && id->driver_data;

	 
	if (per_cpu(cpu_llc_id, cpu1) == BAD_APICID)
		return false;

	 
	if (per_cpu(cpu_llc_id, cpu1) != per_cpu(cpu_llc_id, cpu2))
		return false;

	 
	if (match_pkg(c, o) && !topology_same_node(c, o) && intel_snc)
		return false;

	return topology_sane(c, o, "llc");
}


static inline int x86_sched_itmt_flags(void)
{
	return sysctl_sched_itmt_enabled ? SD_ASYM_PACKING : 0;
}

#ifdef CONFIG_SCHED_MC
static int x86_core_flags(void)
{
	return cpu_core_flags() | x86_sched_itmt_flags();
}
#endif
#ifdef CONFIG_SCHED_SMT
static int x86_smt_flags(void)
{
	return cpu_smt_flags();
}
#endif
#ifdef CONFIG_SCHED_CLUSTER
static int x86_cluster_flags(void)
{
	return cpu_cluster_flags() | x86_sched_itmt_flags();
}
#endif

static int x86_die_flags(void)
{
	if (cpu_feature_enabled(X86_FEATURE_HYBRID_CPU))
	       return x86_sched_itmt_flags();

	return 0;
}

 
static bool x86_has_numa_in_package;

static struct sched_domain_topology_level x86_topology[6];

static void __init build_sched_topology(void)
{
	int i = 0;

#ifdef CONFIG_SCHED_SMT
	x86_topology[i++] = (struct sched_domain_topology_level){
		cpu_smt_mask, x86_smt_flags, SD_INIT_NAME(SMT)
	};
#endif
#ifdef CONFIG_SCHED_CLUSTER
	x86_topology[i++] = (struct sched_domain_topology_level){
		cpu_clustergroup_mask, x86_cluster_flags, SD_INIT_NAME(CLS)
	};
#endif
#ifdef CONFIG_SCHED_MC
	x86_topology[i++] = (struct sched_domain_topology_level){
		cpu_coregroup_mask, x86_core_flags, SD_INIT_NAME(MC)
	};
#endif
	 
	if (!x86_has_numa_in_package) {
		x86_topology[i++] = (struct sched_domain_topology_level){
			cpu_cpu_mask, x86_die_flags, SD_INIT_NAME(DIE)
		};
	}

	 
	BUG_ON(i >= ARRAY_SIZE(x86_topology)-1);

	set_sched_topology(x86_topology);
}

void set_cpu_sibling_map(int cpu)
{
	bool has_smt = smp_num_siblings > 1;
	bool has_mp = has_smt || boot_cpu_data.x86_max_cores > 1;
	struct cpuinfo_x86 *c = &cpu_data(cpu);
	struct cpuinfo_x86 *o;
	int i, threads;

	cpumask_set_cpu(cpu, cpu_sibling_setup_mask);

	if (!has_mp) {
		cpumask_set_cpu(cpu, topology_sibling_cpumask(cpu));
		cpumask_set_cpu(cpu, cpu_llc_shared_mask(cpu));
		cpumask_set_cpu(cpu, cpu_l2c_shared_mask(cpu));
		cpumask_set_cpu(cpu, topology_core_cpumask(cpu));
		cpumask_set_cpu(cpu, topology_die_cpumask(cpu));
		c->booted_cores = 1;
		return;
	}

	for_each_cpu(i, cpu_sibling_setup_mask) {
		o = &cpu_data(i);

		if (match_pkg(c, o) && !topology_same_node(c, o))
			x86_has_numa_in_package = true;

		if ((i == cpu) || (has_smt && match_smt(c, o)))
			link_mask(topology_sibling_cpumask, cpu, i);

		if ((i == cpu) || (has_mp && match_llc(c, o)))
			link_mask(cpu_llc_shared_mask, cpu, i);

		if ((i == cpu) || (has_mp && match_l2c(c, o)))
			link_mask(cpu_l2c_shared_mask, cpu, i);

		if ((i == cpu) || (has_mp && match_die(c, o)))
			link_mask(topology_die_cpumask, cpu, i);
	}

	threads = cpumask_weight(topology_sibling_cpumask(cpu));
	if (threads > __max_smt_threads)
		__max_smt_threads = threads;

	for_each_cpu(i, topology_sibling_cpumask(cpu))
		cpu_data(i).smt_active = threads > 1;

	 
	for_each_cpu(i, cpu_sibling_setup_mask) {
		o = &cpu_data(i);

		if ((i == cpu) || (has_mp && match_pkg(c, o))) {
			link_mask(topology_core_cpumask, cpu, i);

			 
			if (threads == 1) {
				 
				if (cpumask_first(
				    topology_sibling_cpumask(i)) == i)
					c->booted_cores++;
				 
				if (i != cpu)
					cpu_data(i).booted_cores++;
			} else if (i != cpu && !c->booted_cores)
				c->booted_cores = cpu_data(i).booted_cores;
		}
	}
}

 
const struct cpumask *cpu_coregroup_mask(int cpu)
{
	return cpu_llc_shared_mask(cpu);
}

const struct cpumask *cpu_clustergroup_mask(int cpu)
{
	return cpu_l2c_shared_mask(cpu);
}

static void impress_friends(void)
{
	int cpu;
	unsigned long bogosum = 0;
	 
	pr_debug("Before bogomips\n");
	for_each_online_cpu(cpu)
		bogosum += cpu_data(cpu).loops_per_jiffy;

	pr_info("Total of %d processors activated (%lu.%02lu BogoMIPS)\n",
		num_online_cpus(),
		bogosum/(500000/HZ),
		(bogosum/(5000/HZ))%100);

	pr_debug("Before bogocount - setting activated=1\n");
}

 
#define UDELAY_10MS_DEFAULT 10000

static unsigned int init_udelay = UINT_MAX;

static int __init cpu_init_udelay(char *str)
{
	get_option(&str, &init_udelay);

	return 0;
}
early_param("cpu_init_udelay", cpu_init_udelay);

static void __init smp_quirk_init_udelay(void)
{
	 
	if (init_udelay != UINT_MAX)
		return;

	 
	if (((boot_cpu_data.x86_vendor == X86_VENDOR_INTEL) && (boot_cpu_data.x86 == 6)) ||
	    ((boot_cpu_data.x86_vendor == X86_VENDOR_HYGON) && (boot_cpu_data.x86 >= 0x18)) ||
	    ((boot_cpu_data.x86_vendor == X86_VENDOR_AMD) && (boot_cpu_data.x86 >= 0xF))) {
		init_udelay = 0;
		return;
	}
	 
	init_udelay = UDELAY_10MS_DEFAULT;
}

 
static void send_init_sequence(int phys_apicid)
{
	int maxlvt = lapic_get_maxlvt();

	 
	if (APIC_INTEGRATED(boot_cpu_apic_version)) {
		 
		if (maxlvt > 3)
			apic_write(APIC_ESR, 0);
		apic_read(APIC_ESR);
	}

	 
	apic_icr_write(APIC_INT_LEVELTRIG | APIC_INT_ASSERT | APIC_DM_INIT, phys_apicid);
	safe_apic_wait_icr_idle();

	udelay(init_udelay);

	 
	apic_icr_write(APIC_INT_LEVELTRIG | APIC_DM_INIT, phys_apicid);
	safe_apic_wait_icr_idle();
}

 
static int wakeup_secondary_cpu_via_init(int phys_apicid, unsigned long start_eip)
{
	unsigned long send_status = 0, accept_status = 0;
	int num_starts, j, maxlvt;

	preempt_disable();
	maxlvt = lapic_get_maxlvt();
	send_init_sequence(phys_apicid);

	mb();

	 
	if (APIC_INTEGRATED(boot_cpu_apic_version))
		num_starts = 2;
	else
		num_starts = 0;

	 
	pr_debug("#startup loops: %d\n", num_starts);

	for (j = 1; j <= num_starts; j++) {
		pr_debug("Sending STARTUP #%d\n", j);
		if (maxlvt > 3)		 
			apic_write(APIC_ESR, 0);
		apic_read(APIC_ESR);
		pr_debug("After apic_write\n");

		 

		 
		 
		 
		apic_icr_write(APIC_DM_STARTUP | (start_eip >> 12),
			       phys_apicid);

		 
		if (init_udelay == 0)
			udelay(10);
		else
			udelay(300);

		pr_debug("Startup point 1\n");

		pr_debug("Waiting for send to finish...\n");
		send_status = safe_apic_wait_icr_idle();

		 
		if (init_udelay == 0)
			udelay(10);
		else
			udelay(200);

		if (maxlvt > 3)		 
			apic_write(APIC_ESR, 0);
		accept_status = (apic_read(APIC_ESR) & 0xEF);
		if (send_status || accept_status)
			break;
	}
	pr_debug("After Startup\n");

	if (send_status)
		pr_err("APIC never delivered???\n");
	if (accept_status)
		pr_err("APIC delivery error (%lx)\n", accept_status);

	preempt_enable();
	return (send_status | accept_status);
}

 
static void announce_cpu(int cpu, int apicid)
{
	static int width, node_width, first = 1;
	static int current_node = NUMA_NO_NODE;
	int node = early_cpu_to_node(cpu);

	if (!width)
		width = num_digits(num_possible_cpus()) + 1;  

	if (!node_width)
		node_width = num_digits(num_possible_nodes()) + 1;  

	if (system_state < SYSTEM_RUNNING) {
		if (first)
			pr_info("x86: Booting SMP configuration:\n");

		if (node != current_node) {
			if (current_node > (-1))
				pr_cont("\n");
			current_node = node;

			printk(KERN_INFO ".... node %*s#%d, CPUs:  ",
			       node_width - num_digits(node), " ", node);
		}

		 
		if (first)
			pr_cont("%*s", width + 1, " ");
		first = 0;

		pr_cont("%*s#%d", width - num_digits(cpu), " ", cpu);
	} else
		pr_info("Booting Node %d Processor %d APIC 0x%x\n",
			node, cpu, apicid);
}

int common_cpu_up(unsigned int cpu, struct task_struct *idle)
{
	int ret;

	 
	alternatives_enable_smp();

	per_cpu(pcpu_hot.current_task, cpu) = idle;
	cpu_init_stack_canary(cpu, idle);

	 
	ret = irq_init_percpu_irqstack(cpu);
	if (ret)
		return ret;

#ifdef CONFIG_X86_32
	 
	per_cpu(pcpu_hot.top_of_stack, cpu) = task_top_of_stack(idle);
#endif
	return 0;
}

 
static int do_boot_cpu(int apicid, int cpu, struct task_struct *idle)
{
	unsigned long start_ip = real_mode_header->trampoline_start;
	int ret;

#ifdef CONFIG_X86_64
	 
	if (apic->wakeup_secondary_cpu_64)
		start_ip = real_mode_header->trampoline_start64;
#endif
	idle->thread.sp = (unsigned long)task_pt_regs(idle);
	initial_code = (unsigned long)start_secondary;

	if (IS_ENABLED(CONFIG_X86_32)) {
		early_gdt_descr.address = (unsigned long)get_cpu_gdt_rw(cpu);
		initial_stack  = idle->thread.sp;
	} else if (!(smpboot_control & STARTUP_PARALLEL_MASK)) {
		smpboot_control = cpu;
	}

	 
	init_espfix_ap(cpu);

	 
	announce_cpu(cpu, apicid);

	 
	if (x86_platform.legacy.warm_reset) {

		pr_debug("Setting warm reset code and vector.\n");

		smpboot_setup_warm_reset_vector(start_ip);
		 
		if (APIC_INTEGRATED(boot_cpu_apic_version)) {
			apic_write(APIC_ESR, 0);
			apic_read(APIC_ESR);
		}
	}

	smp_mb();

	 
	if (apic->wakeup_secondary_cpu_64)
		ret = apic->wakeup_secondary_cpu_64(apicid, start_ip);
	else if (apic->wakeup_secondary_cpu)
		ret = apic->wakeup_secondary_cpu(apicid, start_ip);
	else
		ret = wakeup_secondary_cpu_via_init(apicid, start_ip);

	 
	if (ret)
		arch_cpuhp_cleanup_kick_cpu(cpu);
	return ret;
}

int native_kick_ap(unsigned int cpu, struct task_struct *tidle)
{
	int apicid = apic->cpu_present_to_apicid(cpu);
	int err;

	lockdep_assert_irqs_enabled();

	pr_debug("++++++++++++++++++++=_---CPU UP  %u\n", cpu);

	if (apicid == BAD_APICID || !physid_isset(apicid, phys_cpu_present_map) ||
	    !apic_id_valid(apicid)) {
		pr_err("%s: bad cpu %d\n", __func__, cpu);
		return -EINVAL;
	}

	 
	mtrr_save_state();

	 
	per_cpu(fpu_fpregs_owner_ctx, cpu) = NULL;

	err = common_cpu_up(cpu, tidle);
	if (err)
		return err;

	err = do_boot_cpu(apicid, cpu, tidle);
	if (err)
		pr_err("do_boot_cpu failed(%d) to wakeup CPU#%u\n", err, cpu);

	return err;
}

int arch_cpuhp_kick_ap_alive(unsigned int cpu, struct task_struct *tidle)
{
	return smp_ops.kick_ap_alive(cpu, tidle);
}

void arch_cpuhp_cleanup_kick_cpu(unsigned int cpu)
{
	 
	if (smp_ops.kick_ap_alive == native_kick_ap && x86_platform.legacy.warm_reset)
		smpboot_restore_warm_reset_vector();
}

void arch_cpuhp_cleanup_dead_cpu(unsigned int cpu)
{
	if (smp_ops.cleanup_dead_cpu)
		smp_ops.cleanup_dead_cpu(cpu);

	if (system_state == SYSTEM_RUNNING)
		pr_info("CPU %u is now offline\n", cpu);
}

void arch_cpuhp_sync_state_poll(void)
{
	if (smp_ops.poll_sync_state)
		smp_ops.poll_sync_state();
}

 
void __init arch_disable_smp_support(void)
{
	disable_ioapic_support();
}

 
static __init void disable_smp(void)
{
	pr_info("SMP disabled\n");

	disable_ioapic_support();

	init_cpu_present(cpumask_of(0));
	init_cpu_possible(cpumask_of(0));

	if (smp_found_config)
		physid_set_mask_of_physid(boot_cpu_physical_apicid, &phys_cpu_present_map);
	else
		physid_set_mask_of_physid(0, &phys_cpu_present_map);
	cpumask_set_cpu(0, topology_sibling_cpumask(0));
	cpumask_set_cpu(0, topology_core_cpumask(0));
	cpumask_set_cpu(0, topology_die_cpumask(0));
}

static void __init smp_cpu_index_default(void)
{
	int i;
	struct cpuinfo_x86 *c;

	for_each_possible_cpu(i) {
		c = &cpu_data(i);
		 
		c->cpu_index = nr_cpu_ids;
	}
}

void __init smp_prepare_cpus_common(void)
{
	unsigned int i;

	smp_cpu_index_default();

	 
	smp_store_boot_cpu_info();  
	mb();

	for_each_possible_cpu(i) {
		zalloc_cpumask_var(&per_cpu(cpu_sibling_map, i), GFP_KERNEL);
		zalloc_cpumask_var(&per_cpu(cpu_core_map, i), GFP_KERNEL);
		zalloc_cpumask_var(&per_cpu(cpu_die_map, i), GFP_KERNEL);
		zalloc_cpumask_var(&per_cpu(cpu_llc_shared_map, i), GFP_KERNEL);
		zalloc_cpumask_var(&per_cpu(cpu_l2c_shared_map, i), GFP_KERNEL);
	}

	set_cpu_sibling_map(0);
}

#ifdef CONFIG_X86_64
 
bool __init arch_cpuhp_init_parallel_bringup(void)
{
	if (!x86_cpuinit.parallel_bringup) {
		pr_info("Parallel CPU startup disabled by the platform\n");
		return false;
	}

	smpboot_control = STARTUP_READ_APICID;
	pr_debug("Parallel CPU startup enabled: 0x%08x\n", smpboot_control);
	return true;
}
#endif

 
void __init native_smp_prepare_cpus(unsigned int max_cpus)
{
	smp_prepare_cpus_common();

	switch (apic_intr_mode) {
	case APIC_PIC:
	case APIC_VIRTUAL_WIRE_NO_CONFIG:
		disable_smp();
		return;
	case APIC_SYMMETRIC_IO_NO_ROUTING:
		disable_smp();
		 
		x86_init.timers.setup_percpu_clockev();
		return;
	case APIC_VIRTUAL_WIRE:
	case APIC_SYMMETRIC_IO:
		break;
	}

	 
	x86_init.timers.setup_percpu_clockev();

	pr_info("CPU0: ");
	print_cpu_info(&cpu_data(0));

	uv_system_init();

	smp_quirk_init_udelay();

	speculative_store_bypass_ht_init();

	snp_set_wakeup_secondary_cpu();
}

void arch_thaw_secondary_cpus_begin(void)
{
	set_cache_aps_delayed_init(true);
}

void arch_thaw_secondary_cpus_end(void)
{
	cache_aps_init();
}

 
void __init native_smp_prepare_boot_cpu(void)
{
	int me = smp_processor_id();

	 
	if (!IS_ENABLED(CONFIG_SMP))
		switch_gdt_and_percpu_base(me);

	native_pv_lock_init();
}

void __init calculate_max_logical_packages(void)
{
	int ncpus;

	 
	ncpus = cpu_data(0).booted_cores * topology_max_smt_threads();
	__max_logical_packages = DIV_ROUND_UP(total_cpus, ncpus);
	pr_info("Max logical packages: %u\n", __max_logical_packages);
}

void __init native_smp_cpus_done(unsigned int max_cpus)
{
	pr_debug("Boot done\n");

	calculate_max_logical_packages();
	build_sched_topology();
	nmi_selftest();
	impress_friends();
	cache_aps_init();
}

static int __initdata setup_possible_cpus = -1;
static int __init _setup_possible_cpus(char *str)
{
	get_option(&str, &setup_possible_cpus);
	return 0;
}
early_param("possible_cpus", _setup_possible_cpus);


 
__init void prefill_possible_map(void)
{
	int i, possible;

	i = setup_max_cpus ?: 1;
	if (setup_possible_cpus == -1) {
		possible = num_processors;
#ifdef CONFIG_HOTPLUG_CPU
		if (setup_max_cpus)
			possible += disabled_cpus;
#else
		if (possible > i)
			possible = i;
#endif
	} else
		possible = setup_possible_cpus;

	total_cpus = max_t(int, possible, num_processors + disabled_cpus);

	 
	if (possible > nr_cpu_ids) {
		pr_warn("%d Processors exceeds NR_CPUS limit of %u\n",
			possible, nr_cpu_ids);
		possible = nr_cpu_ids;
	}

#ifdef CONFIG_HOTPLUG_CPU
	if (!setup_max_cpus)
#endif
	if (possible > i) {
		pr_warn("%d Processors exceeds max_cpus limit of %u\n",
			possible, setup_max_cpus);
		possible = i;
	}

	set_nr_cpu_ids(possible);

	pr_info("Allowing %d CPUs, %d hotplug CPUs\n",
		possible, max_t(int, possible - num_processors, 0));

	reset_cpu_possible_mask();

	for (i = 0; i < possible; i++)
		set_cpu_possible(i, true);
}

 
void __init setup_cpu_local_masks(void)
{
	alloc_bootmem_cpumask_var(&cpu_sibling_setup_mask);
}

#ifdef CONFIG_HOTPLUG_CPU

 
static void recompute_smt_state(void)
{
	int max_threads, cpu;

	max_threads = 0;
	for_each_online_cpu (cpu) {
		int threads = cpumask_weight(topology_sibling_cpumask(cpu));

		if (threads > max_threads)
			max_threads = threads;
	}
	__max_smt_threads = max_threads;
}

static void remove_siblinginfo(int cpu)
{
	int sibling;
	struct cpuinfo_x86 *c = &cpu_data(cpu);

	for_each_cpu(sibling, topology_core_cpumask(cpu)) {
		cpumask_clear_cpu(cpu, topology_core_cpumask(sibling));
		 
		if (cpumask_weight(topology_sibling_cpumask(cpu)) == 1)
			cpu_data(sibling).booted_cores--;
	}

	for_each_cpu(sibling, topology_die_cpumask(cpu))
		cpumask_clear_cpu(cpu, topology_die_cpumask(sibling));

	for_each_cpu(sibling, topology_sibling_cpumask(cpu)) {
		cpumask_clear_cpu(cpu, topology_sibling_cpumask(sibling));
		if (cpumask_weight(topology_sibling_cpumask(sibling)) == 1)
			cpu_data(sibling).smt_active = false;
	}

	for_each_cpu(sibling, cpu_llc_shared_mask(cpu))
		cpumask_clear_cpu(cpu, cpu_llc_shared_mask(sibling));
	for_each_cpu(sibling, cpu_l2c_shared_mask(cpu))
		cpumask_clear_cpu(cpu, cpu_l2c_shared_mask(sibling));
	cpumask_clear(cpu_llc_shared_mask(cpu));
	cpumask_clear(cpu_l2c_shared_mask(cpu));
	cpumask_clear(topology_sibling_cpumask(cpu));
	cpumask_clear(topology_core_cpumask(cpu));
	cpumask_clear(topology_die_cpumask(cpu));
	c->cpu_core_id = 0;
	c->booted_cores = 0;
	cpumask_clear_cpu(cpu, cpu_sibling_setup_mask);
	recompute_smt_state();
}

static void remove_cpu_from_maps(int cpu)
{
	set_cpu_online(cpu, false);
	numa_remove_cpu(cpu);
}

void cpu_disable_common(void)
{
	int cpu = smp_processor_id();

	remove_siblinginfo(cpu);

	 
	lock_vector_lock();
	remove_cpu_from_maps(cpu);
	unlock_vector_lock();
	fixup_irqs();
	lapic_offline();
}

int native_cpu_disable(void)
{
	int ret;

	ret = lapic_can_unplug_cpu();
	if (ret)
		return ret;

	cpu_disable_common();

         
	apic_soft_disable();

	return 0;
}

void play_dead_common(void)
{
	idle_task_exit();

	cpuhp_ap_report_dead();

	local_irq_disable();
}

 
static inline void mwait_play_dead(void)
{
	struct mwait_cpu_dead *md = this_cpu_ptr(&mwait_cpu_dead);
	unsigned int eax, ebx, ecx, edx;
	unsigned int highest_cstate = 0;
	unsigned int highest_subcstate = 0;
	int i;

	if (boot_cpu_data.x86_vendor == X86_VENDOR_AMD ||
	    boot_cpu_data.x86_vendor == X86_VENDOR_HYGON)
		return;
	if (!this_cpu_has(X86_FEATURE_MWAIT))
		return;
	if (!this_cpu_has(X86_FEATURE_CLFLUSH))
		return;
	if (__this_cpu_read(cpu_info.cpuid_level) < CPUID_MWAIT_LEAF)
		return;

	eax = CPUID_MWAIT_LEAF;
	ecx = 0;
	native_cpuid(&eax, &ebx, &ecx, &edx);

	 
	if (!(ecx & CPUID5_ECX_EXTENSIONS_SUPPORTED)) {
		eax = 0;
	} else {
		edx >>= MWAIT_SUBSTATE_SIZE;
		for (i = 0; i < 7 && edx; i++, edx >>= MWAIT_SUBSTATE_SIZE) {
			if (edx & MWAIT_SUBSTATE_MASK) {
				highest_cstate = i;
				highest_subcstate = edx & MWAIT_SUBSTATE_MASK;
			}
		}
		eax = (highest_cstate << MWAIT_SUBSTATE_SIZE) |
			(highest_subcstate - 1);
	}

	 
	md->status = CPUDEAD_MWAIT_WAIT;
	md->control = CPUDEAD_MWAIT_WAIT;

	wbinvd();

	while (1) {
		 
		mb();
		clflush(md);
		mb();
		__monitor(md, 0, 0);
		mb();
		__mwait(eax, 0);

		if (READ_ONCE(md->control) == CPUDEAD_MWAIT_KEXEC_HLT) {
			 
			WRITE_ONCE(md->status, CPUDEAD_MWAIT_KEXEC_HLT);
			while(1)
				native_halt();
		}
	}
}

 
void smp_kick_mwait_play_dead(void)
{
	u32 newstate = CPUDEAD_MWAIT_KEXEC_HLT;
	struct mwait_cpu_dead *md;
	unsigned int cpu, i;

	for_each_cpu_andnot(cpu, cpu_present_mask, cpu_online_mask) {
		md = per_cpu_ptr(&mwait_cpu_dead, cpu);

		 
		if (READ_ONCE(md->status) != CPUDEAD_MWAIT_WAIT)
			continue;

		 
		for (i = 0; READ_ONCE(md->status) != newstate && i < 1000; i++) {
			 
			WRITE_ONCE(md->control, newstate);
			udelay(5);
		}

		if (READ_ONCE(md->status) != newstate)
			pr_err_once("CPU%u is stuck in mwait_play_dead()\n", cpu);
	}
}

void __noreturn hlt_play_dead(void)
{
	if (__this_cpu_read(cpu_info.x86) >= 4)
		wbinvd();

	while (1)
		native_halt();
}

void native_play_dead(void)
{
	play_dead_common();
	tboot_shutdown(TB_SHUTDOWN_WFS);

	mwait_play_dead();
	if (cpuidle_play_dead())
		hlt_play_dead();
}

#else  
int native_cpu_disable(void)
{
	return -ENOSYS;
}

void native_play_dead(void)
{
	BUG();
}

#endif
