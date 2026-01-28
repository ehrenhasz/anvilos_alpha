#ifndef _S390_KEXEC_H
#define _S390_KEXEC_H
#include <linux/module.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/setup.h>
#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)
#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)
#define KEXEC_CONTROL_MEMORY_LIMIT (1UL<<31)
#define KEXEC_CONTROL_MEMORY_GFP (GFP_DMA | __GFP_NORETRY)
#define KEXEC_CRASH_CONTROL_MEMORY_LIMIT (-1UL)
#define KEXEC_CONTROL_PAGE_SIZE 4096
#define KEXEC_CRASH_MEM_ALIGN HPAGE_SIZE
#define KEXEC_ARCH KEXEC_ARCH_S390
#define KEXEC_BUF_MEM_UNKNOWN -1
static inline void crash_setup_regs(struct pt_regs *newregs,
					struct pt_regs *oldregs) { }
struct kimage;
struct s390_load_data {
	void *kernel_buf;
	unsigned long kernel_mem;
	struct parmarea *parm;
	size_t memsz;
	struct ipl_report *report;
};
int s390_verify_sig(const char *kernel, unsigned long kernel_len);
void *kexec_file_add_components(struct kimage *image,
				int (*add_kernel)(struct kimage *image,
						  struct s390_load_data *data));
int arch_kexec_do_relocs(int r_type, void *loc, unsigned long val,
			 unsigned long addr);
#define ARCH_HAS_KIMAGE_ARCH
struct kimage_arch {
	void *ipl_buf;
};
extern const struct kexec_file_ops s390_kexec_image_ops;
extern const struct kexec_file_ops s390_kexec_elf_ops;
#ifdef CONFIG_CRASH_DUMP
void crash_free_reserved_phys_range(unsigned long begin, unsigned long end);
#define crash_free_reserved_phys_range crash_free_reserved_phys_range
void arch_kexec_protect_crashkres(void);
#define arch_kexec_protect_crashkres arch_kexec_protect_crashkres
void arch_kexec_unprotect_crashkres(void);
#define arch_kexec_unprotect_crashkres arch_kexec_unprotect_crashkres
#endif
#ifdef CONFIG_KEXEC_FILE
struct purgatory_info;
int arch_kexec_apply_relocations_add(struct purgatory_info *pi,
				     Elf_Shdr *section,
				     const Elf_Shdr *relsec,
				     const Elf_Shdr *symtab);
#define arch_kexec_apply_relocations_add arch_kexec_apply_relocations_add
int arch_kimage_file_post_load_cleanup(struct kimage *image);
#define arch_kimage_file_post_load_cleanup arch_kimage_file_post_load_cleanup
#endif
#endif  
