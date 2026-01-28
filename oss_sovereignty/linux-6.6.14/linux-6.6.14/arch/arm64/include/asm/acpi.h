#ifndef _ASM_ACPI_H
#define _ASM_ACPI_H
#include <linux/cpuidle.h>
#include <linux/efi.h>
#include <linux/memblock.h>
#include <linux/psci.h>
#include <linux/stddef.h>
#include <asm/cputype.h>
#include <asm/io.h>
#include <asm/ptrace.h>
#include <asm/smp_plat.h>
#include <asm/tlbflush.h>
#define ACPI_MADT_GICC_MIN_LENGTH   offsetof(  \
	struct acpi_madt_generic_interrupt, efficiency_class)
#define BAD_MADT_GICC_ENTRY(entry, end)					\
	(!(entry) || (entry)->header.length < ACPI_MADT_GICC_MIN_LENGTH || \
	(unsigned long)(entry) + (entry)->header.length > (end))
#define ACPI_MADT_GICC_SPE  (offsetof(struct acpi_madt_generic_interrupt, \
	spe_interrupt) + sizeof(u16))
#define ACPI_MADT_GICC_TRBE  (offsetof(struct acpi_madt_generic_interrupt, \
	trbe_interrupt) + sizeof(u16))
#define CPUIDLE_CORE_CTXT		BIT(0)  
static inline unsigned int arch_get_idle_state_flags(u32 arch_flags)
{
	if (arch_flags & CPUIDLE_CORE_CTXT)
		return CPUIDLE_FLAG_TIMER_STOP;
	return 0;
}
#define arch_get_idle_state_flags arch_get_idle_state_flags
#define CPUIDLE_TRACE_CTXT		BIT(1)  
#define CPUIDLE_GICR_CTXT		BIT(2)  
#define CPUIDLE_GICD_CTXT		BIT(3)  
#ifdef	CONFIG_ACPI
pgprot_t __acpi_get_mem_attribute(phys_addr_t addr);
void __iomem *acpi_os_ioremap(acpi_physical_address phys, acpi_size size);
#define acpi_os_ioremap acpi_os_ioremap
typedef u64 phys_cpuid_t;
#define PHYS_CPUID_INVALID INVALID_HWID
#define acpi_strict 1	 
extern int acpi_disabled;
extern int acpi_noirq;
extern int acpi_pci_disabled;
static inline void disable_acpi(void)
{
	acpi_disabled = 1;
	acpi_pci_disabled = 1;
	acpi_noirq = 1;
}
static inline void enable_acpi(void)
{
	acpi_disabled = 0;
	acpi_pci_disabled = 0;
	acpi_noirq = 0;
}
#define cpu_physical_id(cpu) cpu_logical_map(cpu)
static inline bool acpi_has_cpu_in_madt(void)
{
	return true;
}
struct acpi_madt_generic_interrupt *acpi_cpu_get_madt_gicc(int cpu);
static inline u32 get_acpi_id_for_cpu(unsigned int cpu)
{
	return	acpi_cpu_get_madt_gicc(cpu)->uid;
}
static inline void arch_fix_phys_package_id(int num, u32 slot) { }
void __init acpi_init_cpus(void);
int apei_claim_sea(struct pt_regs *regs);
#else
static inline void acpi_init_cpus(void) { }
static inline int apei_claim_sea(struct pt_regs *regs) { return -ENOENT; }
#endif  
#ifdef CONFIG_ARM64_ACPI_PARKING_PROTOCOL
bool acpi_parking_protocol_valid(int cpu);
void __init
acpi_set_mailbox_entry(int cpu, struct acpi_madt_generic_interrupt *processor);
#else
static inline bool acpi_parking_protocol_valid(int cpu) { return false; }
static inline void
acpi_set_mailbox_entry(int cpu, struct acpi_madt_generic_interrupt *processor)
{}
#endif
static inline const char *acpi_get_enable_method(int cpu)
{
	if (acpi_psci_present())
		return "psci";
	if (acpi_parking_protocol_valid(cpu))
		return "parking-protocol";
	return NULL;
}
#ifdef	CONFIG_ACPI_APEI
#define acpi_disable_cmcff 1
static inline pgprot_t arch_apei_get_mem_attribute(phys_addr_t addr)
{
	return __acpi_get_mem_attribute(addr);
}
#endif  
#ifdef CONFIG_ACPI_NUMA
int arm64_acpi_numa_init(void);
int acpi_numa_get_nid(unsigned int cpu);
void acpi_map_cpus_to_nodes(void);
#else
static inline int arm64_acpi_numa_init(void) { return -ENOSYS; }
static inline int acpi_numa_get_nid(unsigned int cpu) { return NUMA_NO_NODE; }
static inline void acpi_map_cpus_to_nodes(void) { }
#endif  
#define ACPI_TABLE_UPGRADE_MAX_PHYS MEMBLOCK_ALLOC_ACCESSIBLE
#endif  
