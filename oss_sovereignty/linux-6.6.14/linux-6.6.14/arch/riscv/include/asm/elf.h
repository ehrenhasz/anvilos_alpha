#ifndef _ASM_RISCV_ELF_H
#define _ASM_RISCV_ELF_H
#include <uapi/linux/elf.h>
#include <linux/compat.h>
#include <uapi/asm/elf.h>
#include <asm/auxvec.h>
#include <asm/byteorder.h>
#include <asm/cacheinfo.h>
#include <asm/hwcap.h>
#define ELF_ARCH	EM_RISCV
#ifndef ELF_CLASS
#ifdef CONFIG_64BIT
#define ELF_CLASS	ELFCLASS64
#else
#define ELF_CLASS	ELFCLASS32
#endif
#endif
#define ELF_DATA	ELFDATA2LSB
#define elf_check_arch(x) (((x)->e_machine == EM_RISCV) && \
			   ((x)->e_ident[EI_CLASS] == ELF_CLASS))
extern bool compat_elf_check_arch(Elf32_Ehdr *hdr);
#define compat_elf_check_arch	compat_elf_check_arch
#define CORE_DUMP_USE_REGSET
#define ELF_FDPIC_CORE_EFLAGS	0
#define ELF_EXEC_PAGESIZE	(PAGE_SIZE)
#define ELF_ET_DYN_BASE		((DEFAULT_MAP_WINDOW / 3) * 2)
#ifdef CONFIG_64BIT
#ifdef CONFIG_COMPAT
#define STACK_RND_MASK		(test_thread_flag(TIF_32BIT) ? \
				 0x7ff >> (PAGE_SHIFT - 12) : \
				 0x3ffff >> (PAGE_SHIFT - 12))
#else
#define STACK_RND_MASK		(0x3ffff >> (PAGE_SHIFT - 12))
#endif
#endif
#define ELF_HWCAP	riscv_get_elf_hwcap()
extern unsigned long elf_hwcap;
#define ELF_FDPIC_PLAT_INIT(_r, _exec_map_addr, _interp_map_addr, dynamic_addr) \
	do { \
		(_r)->a1 = _exec_map_addr; \
		(_r)->a2 = _interp_map_addr; \
		(_r)->a3 = dynamic_addr; \
	} while (0)
#define ELF_PLATFORM	(NULL)
#define COMPAT_ELF_PLATFORM	(NULL)
#define ARCH_DLINFO						\
do {								\
	 							\
	NEW_AUX_ENT(AT_SYSINFO_EHDR,				\
		(elf_addr_t)(ulong)current->mm->context.vdso);	\
	NEW_AUX_ENT(AT_L1I_CACHESIZE,				\
		get_cache_size(1, CACHE_TYPE_INST));		\
	NEW_AUX_ENT(AT_L1I_CACHEGEOMETRY,			\
		get_cache_geometry(1, CACHE_TYPE_INST));	\
	NEW_AUX_ENT(AT_L1D_CACHESIZE,				\
		get_cache_size(1, CACHE_TYPE_DATA));		\
	NEW_AUX_ENT(AT_L1D_CACHEGEOMETRY,			\
		get_cache_geometry(1, CACHE_TYPE_DATA));	\
	NEW_AUX_ENT(AT_L2_CACHESIZE,				\
		get_cache_size(2, CACHE_TYPE_UNIFIED));		\
	NEW_AUX_ENT(AT_L2_CACHEGEOMETRY,			\
		get_cache_geometry(2, CACHE_TYPE_UNIFIED));	\
	NEW_AUX_ENT(AT_L3_CACHESIZE,				\
		get_cache_size(3, CACHE_TYPE_UNIFIED));		\
	NEW_AUX_ENT(AT_L3_CACHEGEOMETRY,			\
		get_cache_geometry(3, CACHE_TYPE_UNIFIED));	\
	 							 \
	if (likely(signal_minsigstksz))				 \
		NEW_AUX_ENT(AT_MINSIGSTKSZ, signal_minsigstksz); \
	else							 \
		NEW_AUX_ENT(AT_IGNORE, 0);			 \
} while (0)
#ifdef CONFIG_MMU
#define ARCH_HAS_SETUP_ADDITIONAL_PAGES
struct linux_binprm;
extern int arch_setup_additional_pages(struct linux_binprm *bprm,
	int uses_interp);
#endif  
#define ELF_CORE_COPY_REGS(dest, regs)			\
do {							\
	*(struct user_regs_struct *)&(dest) =		\
		*(struct user_regs_struct *)regs;	\
} while (0);
#ifdef CONFIG_COMPAT
#define SET_PERSONALITY(ex)					\
do {    if ((ex).e_ident[EI_CLASS] == ELFCLASS32)		\
		set_thread_flag(TIF_32BIT);			\
	else							\
		clear_thread_flag(TIF_32BIT);			\
	if (personality(current->personality) != PER_LINUX32)	\
		set_personality(PER_LINUX |			\
			(current->personality & (~PER_MASK)));	\
} while (0)
#define COMPAT_ELF_ET_DYN_BASE		((TASK_SIZE_32 / 3) * 2)
typedef compat_ulong_t			compat_elf_greg_t;
typedef compat_elf_greg_t		compat_elf_gregset_t[ELF_NGREG];
extern int compat_arch_setup_additional_pages(struct linux_binprm *bprm,
					      int uses_interp);
#define compat_arch_setup_additional_pages \
				compat_arch_setup_additional_pages
#endif  
#endif  
