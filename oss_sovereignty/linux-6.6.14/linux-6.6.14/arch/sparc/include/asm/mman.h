#ifndef __SPARC_MMAN_H__
#define __SPARC_MMAN_H__
#include <uapi/asm/mman.h>
#ifndef __ASSEMBLY__
#define arch_mmap_check(addr,len,flags)	sparc_mmap_check(addr,len)
int sparc_mmap_check(unsigned long addr, unsigned long len);
#ifdef CONFIG_SPARC64
#include <asm/adi_64.h>
static inline void ipi_set_tstate_mcde(void *arg)
{
	struct mm_struct *mm = arg;
	if (current->mm == mm) {
		struct pt_regs *regs;
		regs = task_pt_regs(current);
		regs->tstate |= TSTATE_MCDE;
	}
}
#define arch_calc_vm_prot_bits(prot, pkey) sparc_calc_vm_prot_bits(prot)
static inline unsigned long sparc_calc_vm_prot_bits(unsigned long prot)
{
	if (adi_capable() && (prot & PROT_ADI)) {
		struct pt_regs *regs;
		if (!current->mm->context.adi) {
			regs = task_pt_regs(current);
			regs->tstate |= TSTATE_MCDE;
			current->mm->context.adi = true;
			on_each_cpu_mask(mm_cpumask(current->mm),
					 ipi_set_tstate_mcde, current->mm, 0);
		}
		return VM_SPARC_ADI;
	} else {
		return 0;
	}
}
#define arch_validate_prot(prot, addr) sparc_validate_prot(prot, addr)
static inline int sparc_validate_prot(unsigned long prot, unsigned long addr)
{
	if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC | PROT_SEM | PROT_ADI))
		return 0;
	return 1;
}
#define arch_validate_flags(vm_flags) arch_validate_flags(vm_flags)
static inline bool arch_validate_flags(unsigned long vm_flags)
{
	if (vm_flags & VM_SPARC_ADI) {
		if (!adi_capable())
			return false;
		if (vm_flags & (VM_PFNMAP | VM_MIXEDMAP))
			return false;
		if (vm_flags & VM_MERGEABLE)
			return false;
	}
	return true;
}
#endif  
#endif  
#endif  
