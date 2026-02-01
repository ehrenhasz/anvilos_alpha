 

#ifndef _ASM_X86_XEN_HYPERVISOR_H
#define _ASM_X86_XEN_HYPERVISOR_H

extern struct shared_info *HYPERVISOR_shared_info;
extern struct start_info *xen_start_info;

#include <asm/bug.h>
#include <asm/processor.h>

#define XEN_SIGNATURE "XenVMMXenVMM"

static inline uint32_t xen_cpuid_base(void)
{
	return hypervisor_cpuid_base(XEN_SIGNATURE, 2);
}

struct pci_dev;

#ifdef CONFIG_XEN_PV_DOM0
bool xen_initdom_restore_msi(struct pci_dev *dev);
#else
static inline bool xen_initdom_restore_msi(struct pci_dev *dev) { return true; }
#endif

#ifdef CONFIG_HOTPLUG_CPU
void xen_arch_register_cpu(int num);
void xen_arch_unregister_cpu(int num);
#endif

#ifdef CONFIG_PVH
void __init xen_pvh_init(struct boot_params *boot_params);
void __init mem_map_via_hcall(struct boot_params *boot_params_p);
#endif

 
enum xen_lazy_mode {
	XEN_LAZY_NONE,
	XEN_LAZY_MMU,
	XEN_LAZY_CPU,
};

DECLARE_PER_CPU(enum xen_lazy_mode, xen_lazy_mode);
DECLARE_PER_CPU(unsigned int, xen_lazy_nesting);

static inline void enter_lazy(enum xen_lazy_mode mode)
{
	enum xen_lazy_mode old_mode = this_cpu_read(xen_lazy_mode);

	if (mode == old_mode) {
		this_cpu_inc(xen_lazy_nesting);
		return;
	}

	BUG_ON(old_mode != XEN_LAZY_NONE);

	this_cpu_write(xen_lazy_mode, mode);
}

static inline void leave_lazy(enum xen_lazy_mode mode)
{
	BUG_ON(this_cpu_read(xen_lazy_mode) != mode);

	if (this_cpu_read(xen_lazy_nesting) == 0)
		this_cpu_write(xen_lazy_mode, XEN_LAZY_NONE);
	else
		this_cpu_dec(xen_lazy_nesting);
}

enum xen_lazy_mode xen_get_lazy_mode(void);

#if defined(CONFIG_XEN_DOM0) && defined(CONFIG_ACPI)
void xen_sanitize_proc_cap_bits(uint32_t *buf);
#else
static inline void xen_sanitize_proc_cap_bits(uint32_t *buf)
{
	BUG();
}
#endif

#endif  
