
 
#include <linux/io.h>

#include <asm/apic.h>
#include <asm/cpu.h>
#include <asm/smp.h>
#include <asm/numa.h>
#include <asm/cacheinfo.h>
#include <asm/spec-ctrl.h>
#include <asm/delay.h>

#include "cpu.h"

#define APICID_SOCKET_ID_BIT 6

 
static u32 nodes_per_socket = 1;

#ifdef CONFIG_NUMA
 
static int nearby_node(int apicid)
{
	int i, node;

	for (i = apicid - 1; i >= 0; i--) {
		node = __apicid_to_node[i];
		if (node != NUMA_NO_NODE && node_online(node))
			return node;
	}
	for (i = apicid + 1; i < MAX_LOCAL_APIC; i++) {
		node = __apicid_to_node[i];
		if (node != NUMA_NO_NODE && node_online(node))
			return node;
	}
	return first_node(node_online_map);  
}
#endif

static void hygon_get_topology_early(struct cpuinfo_x86 *c)
{
	if (cpu_has(c, X86_FEATURE_TOPOEXT))
		smp_num_siblings = ((cpuid_ebx(0x8000001e) >> 8) & 0xff) + 1;
}

 
static void hygon_get_topology(struct cpuinfo_x86 *c)
{
	int cpu = smp_processor_id();

	 
	if (boot_cpu_has(X86_FEATURE_TOPOEXT)) {
		int err;
		u32 eax, ebx, ecx, edx;

		cpuid(0x8000001e, &eax, &ebx, &ecx, &edx);

		c->cpu_die_id  = ecx & 0xff;

		c->cpu_core_id = ebx & 0xff;

		if (smp_num_siblings > 1)
			c->x86_max_cores /= smp_num_siblings;

		 
		err = detect_extended_topology(c);
		if (!err)
			c->x86_coreid_bits = get_count_order(c->x86_max_cores);

		 
		if (!boot_cpu_has(X86_FEATURE_HYPERVISOR) && c->x86_model <= 0x3)
			c->phys_proc_id = c->apicid >> APICID_SOCKET_ID_BIT;

		cacheinfo_hygon_init_llc_id(c, cpu);
	} else if (cpu_has(c, X86_FEATURE_NODEID_MSR)) {
		u64 value;

		rdmsrl(MSR_FAM10H_NODE_ID, value);
		c->cpu_die_id = value & 7;

		per_cpu(cpu_llc_id, cpu) = c->cpu_die_id;
	} else
		return;

	if (nodes_per_socket > 1)
		set_cpu_cap(c, X86_FEATURE_AMD_DCM);
}

 
static void hygon_detect_cmp(struct cpuinfo_x86 *c)
{
	unsigned int bits;
	int cpu = smp_processor_id();

	bits = c->x86_coreid_bits;
	 
	c->cpu_core_id = c->initial_apicid & ((1 << bits)-1);
	 
	c->phys_proc_id = c->initial_apicid >> bits;
	 
	per_cpu(cpu_llc_id, cpu) = c->cpu_die_id = c->phys_proc_id;
}

static void srat_detect_node(struct cpuinfo_x86 *c)
{
#ifdef CONFIG_NUMA
	int cpu = smp_processor_id();
	int node;
	unsigned int apicid = c->apicid;

	node = numa_cpu_node(cpu);
	if (node == NUMA_NO_NODE)
		node = per_cpu(cpu_llc_id, cpu);

	 
	if (x86_cpuinit.fixup_cpu_id)
		x86_cpuinit.fixup_cpu_id(c, node);

	if (!node_online(node)) {
		 
		int ht_nodeid = c->initial_apicid;

		if (__apicid_to_node[ht_nodeid] != NUMA_NO_NODE)
			node = __apicid_to_node[ht_nodeid];
		 
		if (!node_online(node))
			node = nearby_node(apicid);
	}
	numa_set_node(cpu, node);
#endif
}

static void early_init_hygon_mc(struct cpuinfo_x86 *c)
{
#ifdef CONFIG_SMP
	unsigned int bits, ecx;

	 
	if (c->extended_cpuid_level < 0x80000008)
		return;

	ecx = cpuid_ecx(0x80000008);

	c->x86_max_cores = (ecx & 0xff) + 1;

	 
	bits = (ecx >> 12) & 0xF;

	 
	if (bits == 0) {
		while ((1 << bits) < c->x86_max_cores)
			bits++;
	}

	c->x86_coreid_bits = bits;
#endif
}

static void bsp_init_hygon(struct cpuinfo_x86 *c)
{
	if (cpu_has(c, X86_FEATURE_CONSTANT_TSC)) {
		u64 val;

		rdmsrl(MSR_K7_HWCR, val);
		if (!(val & BIT(24)))
			pr_warn(FW_BUG "TSC doesn't count with P0 frequency!\n");
	}

	if (cpu_has(c, X86_FEATURE_MWAITX))
		use_mwaitx_delay();

	if (boot_cpu_has(X86_FEATURE_TOPOEXT)) {
		u32 ecx;

		ecx = cpuid_ecx(0x8000001e);
		__max_die_per_package = nodes_per_socket = ((ecx >> 8) & 7) + 1;
	} else if (boot_cpu_has(X86_FEATURE_NODEID_MSR)) {
		u64 value;

		rdmsrl(MSR_FAM10H_NODE_ID, value);
		__max_die_per_package = nodes_per_socket = ((value >> 3) & 7) + 1;
	}

	if (!boot_cpu_has(X86_FEATURE_AMD_SSBD) &&
	    !boot_cpu_has(X86_FEATURE_VIRT_SSBD)) {
		 
		if (!rdmsrl_safe(MSR_AMD64_LS_CFG, &x86_amd_ls_cfg_base)) {
			setup_force_cpu_cap(X86_FEATURE_LS_CFG_SSBD);
			setup_force_cpu_cap(X86_FEATURE_SSBD);
			x86_amd_ls_cfg_ssbd_mask = 1ULL << 10;
		}
	}
}

static void early_init_hygon(struct cpuinfo_x86 *c)
{
	u32 dummy;

	early_init_hygon_mc(c);

	set_cpu_cap(c, X86_FEATURE_K8);

	rdmsr_safe(MSR_AMD64_PATCH_LEVEL, &c->microcode, &dummy);

	 
	if (c->x86_power & (1 << 8)) {
		set_cpu_cap(c, X86_FEATURE_CONSTANT_TSC);
		set_cpu_cap(c, X86_FEATURE_NONSTOP_TSC);
	}

	 
	if (c->x86_power & BIT(12))
		set_cpu_cap(c, X86_FEATURE_ACC_POWER);

	 
	if (c->x86_power & BIT(14))
		set_cpu_cap(c, X86_FEATURE_RAPL);

#ifdef CONFIG_X86_64
	set_cpu_cap(c, X86_FEATURE_SYSCALL32);
#endif

#if defined(CONFIG_X86_LOCAL_APIC) && defined(CONFIG_PCI)
	 
	if (boot_cpu_has(X86_FEATURE_APIC))
		set_cpu_cap(c, X86_FEATURE_EXTD_APICID);
#endif

	 
	set_cpu_cap(c, X86_FEATURE_VMMCALL);

	hygon_get_topology_early(c);
}

static void init_hygon(struct cpuinfo_x86 *c)
{
	early_init_hygon(c);

	 
	clear_cpu_cap(c, 0*32+31);

	set_cpu_cap(c, X86_FEATURE_REP_GOOD);

	 
	c->apicid = read_apic_id();

	 

	set_cpu_cap(c, X86_FEATURE_ZEN);
	set_cpu_cap(c, X86_FEATURE_CPB);

	cpu_detect_cache_sizes(c);

	hygon_detect_cmp(c);
	hygon_get_topology(c);
	srat_detect_node(c);

	init_hygon_cacheinfo(c);

	if (cpu_has(c, X86_FEATURE_XMM2)) {
		 
		msr_set_bit(MSR_AMD64_DE_CFG,
			    MSR_AMD64_DE_CFG_LFENCE_SERIALIZE_BIT);

		 
		set_cpu_cap(c, X86_FEATURE_LFENCE_RDTSC);
	}

	 
	set_cpu_cap(c, X86_FEATURE_ARAT);

	 
	if (!cpu_feature_enabled(X86_FEATURE_XENPV))
		set_cpu_bug(c, X86_BUG_SYSRET_SS_ATTRS);

	check_null_seg_clears_base(c);
}

static void cpu_detect_tlb_hygon(struct cpuinfo_x86 *c)
{
	u32 ebx, eax, ecx, edx;
	u16 mask = 0xfff;

	if (c->extended_cpuid_level < 0x80000006)
		return;

	cpuid(0x80000006, &eax, &ebx, &ecx, &edx);

	tlb_lld_4k[ENTRIES] = (ebx >> 16) & mask;
	tlb_lli_4k[ENTRIES] = ebx & mask;

	 
	if (!((eax >> 16) & mask))
		tlb_lld_2m[ENTRIES] = (cpuid_eax(0x80000005) >> 16) & 0xff;
	else
		tlb_lld_2m[ENTRIES] = (eax >> 16) & mask;

	 
	tlb_lld_4m[ENTRIES] = tlb_lld_2m[ENTRIES] >> 1;

	 
	if (!(eax & mask)) {
		cpuid(0x80000005, &eax, &ebx, &ecx, &edx);
		tlb_lli_2m[ENTRIES] = eax & 0xff;
	} else
		tlb_lli_2m[ENTRIES] = eax & mask;

	tlb_lli_4m[ENTRIES] = tlb_lli_2m[ENTRIES] >> 1;
}

static const struct cpu_dev hygon_cpu_dev = {
	.c_vendor	= "Hygon",
	.c_ident	= { "HygonGenuine" },
	.c_early_init   = early_init_hygon,
	.c_detect_tlb	= cpu_detect_tlb_hygon,
	.c_bsp_init	= bsp_init_hygon,
	.c_init		= init_hygon,
	.c_x86_vendor	= X86_VENDOR_HYGON,
};

cpu_dev_register(hygon_cpu_dev);
