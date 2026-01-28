#ifndef _ASM_PARISC_KEXEC_H
#define _ASM_PARISC_KEXEC_H
#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)
#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)
#define KEXEC_CONTROL_MEMORY_LIMIT (-1UL)
#define KEXEC_CONTROL_PAGE_SIZE	4096
#define KEXEC_ARCH KEXEC_ARCH_PARISC
#define ARCH_HAS_KIMAGE_ARCH
#ifndef __ASSEMBLY__
struct kimage_arch {
	unsigned long initrd_start;
	unsigned long initrd_end;
	unsigned long cmdline;
};
static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
}
#endif  
#endif  
