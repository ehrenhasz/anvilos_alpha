#ifndef __ASMS390_ELF_H
#define __ASMS390_ELF_H
#define R_390_NONE		0	 
#define R_390_8			1	 
#define R_390_12		2	 
#define R_390_16		3	 
#define R_390_32		4	 
#define R_390_PC32		5	 
#define R_390_GOT12		6	 
#define R_390_GOT32		7	 
#define R_390_PLT32		8	 
#define R_390_COPY		9	 
#define R_390_GLOB_DAT		10	 
#define R_390_JMP_SLOT		11	 
#define R_390_RELATIVE		12	 
#define R_390_GOTOFF32		13	 
#define R_390_GOTPC		14	 
#define R_390_GOT16		15	 
#define R_390_PC16		16	 
#define R_390_PC16DBL		17	 
#define R_390_PLT16DBL		18	 
#define R_390_PC32DBL		19	 
#define R_390_PLT32DBL		20	 
#define R_390_GOTPCDBL		21	 
#define R_390_64		22	 
#define R_390_PC64		23	 
#define R_390_GOT64		24	 
#define R_390_PLT64		25	 
#define R_390_GOTENT		26	 
#define R_390_GOTOFF16		27	 
#define R_390_GOTOFF64		28	 
#define R_390_GOTPLT12		29	 
#define R_390_GOTPLT16		30	 
#define R_390_GOTPLT32		31	 
#define R_390_GOTPLT64		32	 
#define R_390_GOTPLTENT		33	 
#define R_390_PLTOFF16		34	 
#define R_390_PLTOFF32		35	 
#define R_390_PLTOFF64		36	 
#define R_390_TLS_LOAD		37	 
#define R_390_TLS_GDCALL	38	 
#define R_390_TLS_LDCALL	39	 
#define R_390_TLS_GD32		40	 
#define R_390_TLS_GD64		41	 
#define R_390_TLS_GOTIE12	42	 
#define R_390_TLS_GOTIE32	43	 
#define R_390_TLS_GOTIE64	44	 
#define R_390_TLS_LDM32		45	 
#define R_390_TLS_LDM64		46	 
#define R_390_TLS_IE32		47	 
#define R_390_TLS_IE64		48	 
#define R_390_TLS_IEENT		49	 
#define R_390_TLS_LE32		50	 
#define R_390_TLS_LE64		51	 
#define R_390_TLS_LDO32		52	 
#define R_390_TLS_LDO64		53	 
#define R_390_TLS_DTPMOD	54	 
#define R_390_TLS_DTPOFF	55	 
#define R_390_TLS_TPOFF		56	 
#define R_390_20		57	 
#define R_390_GOT20		58	 
#define R_390_GOTPLT20		59	 
#define R_390_TLS_GOTIE20	60	 
#define R_390_NUM	61
enum {
	HWCAP_NR_ESAN3		= 0,
	HWCAP_NR_ZARCH		= 1,
	HWCAP_NR_STFLE		= 2,
	HWCAP_NR_MSA		= 3,
	HWCAP_NR_LDISP		= 4,
	HWCAP_NR_EIMM		= 5,
	HWCAP_NR_DFP		= 6,
	HWCAP_NR_HPAGE		= 7,
	HWCAP_NR_ETF3EH		= 8,
	HWCAP_NR_HIGH_GPRS	= 9,
	HWCAP_NR_TE		= 10,
	HWCAP_NR_VXRS		= 11,
	HWCAP_NR_VXRS_BCD	= 12,
	HWCAP_NR_VXRS_EXT	= 13,
	HWCAP_NR_GS		= 14,
	HWCAP_NR_VXRS_EXT2	= 15,
	HWCAP_NR_VXRS_PDE	= 16,
	HWCAP_NR_SORT		= 17,
	HWCAP_NR_DFLT		= 18,
	HWCAP_NR_VXRS_PDE2	= 19,
	HWCAP_NR_NNPA		= 20,
	HWCAP_NR_PCI_MIO	= 21,
	HWCAP_NR_SIE		= 22,
	HWCAP_NR_MAX
};
#define HWCAP_ESAN3		BIT(HWCAP_NR_ESAN3)
#define HWCAP_ZARCH		BIT(HWCAP_NR_ZARCH)
#define HWCAP_STFLE		BIT(HWCAP_NR_STFLE)
#define HWCAP_MSA		BIT(HWCAP_NR_MSA)
#define HWCAP_LDISP		BIT(HWCAP_NR_LDISP)
#define HWCAP_EIMM		BIT(HWCAP_NR_EIMM)
#define HWCAP_DFP		BIT(HWCAP_NR_DFP)
#define HWCAP_HPAGE		BIT(HWCAP_NR_HPAGE)
#define HWCAP_ETF3EH		BIT(HWCAP_NR_ETF3EH)
#define HWCAP_HIGH_GPRS		BIT(HWCAP_NR_HIGH_GPRS)
#define HWCAP_TE		BIT(HWCAP_NR_TE)
#define HWCAP_VXRS		BIT(HWCAP_NR_VXRS)
#define HWCAP_VXRS_BCD		BIT(HWCAP_NR_VXRS_BCD)
#define HWCAP_VXRS_EXT		BIT(HWCAP_NR_VXRS_EXT)
#define HWCAP_GS		BIT(HWCAP_NR_GS)
#define HWCAP_VXRS_EXT2		BIT(HWCAP_NR_VXRS_EXT2)
#define HWCAP_VXRS_PDE		BIT(HWCAP_NR_VXRS_PDE)
#define HWCAP_SORT		BIT(HWCAP_NR_SORT)
#define HWCAP_DFLT		BIT(HWCAP_NR_DFLT)
#define HWCAP_VXRS_PDE2		BIT(HWCAP_NR_VXRS_PDE2)
#define HWCAP_NNPA		BIT(HWCAP_NR_NNPA)
#define HWCAP_PCI_MIO		BIT(HWCAP_NR_PCI_MIO)
#define HWCAP_SIE		BIT(HWCAP_NR_SIE)
#define ELF_CLASS	ELFCLASS64
#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_S390
#define PT_S390_PGSTE	0x70000000
#include <linux/compat.h>
#include <asm/ptrace.h>
#include <asm/syscall.h>
#include <asm/user.h>
typedef s390_fp_regs elf_fpregset_t;
typedef s390_regs elf_gregset_t;
typedef s390_fp_regs compat_elf_fpregset_t;
typedef s390_compat_regs compat_elf_gregset_t;
#include <linux/sched/mm.h>	 
#include <asm/mmu_context.h>
#define elf_check_arch(x) \
	(((x)->e_machine == EM_S390 || (x)->e_machine == EM_S390_OLD) \
         && (x)->e_ident[EI_CLASS] == ELF_CLASS) 
#define compat_elf_check_arch(x) \
	(((x)->e_machine == EM_S390 || (x)->e_machine == EM_S390_OLD) \
	 && (x)->e_ident[EI_CLASS] == ELF_CLASS)
#define compat_start_thread	start_thread31
struct arch_elf_state {
	int rc;
};
#define INIT_ARCH_ELF_STATE { .rc = 0 }
#define arch_check_elf(ehdr, interp, interp_ehdr, state) (0)
#ifdef CONFIG_PGSTE
#define arch_elf_pt_proc(ehdr, phdr, elf, interp, state)	\
({								\
	struct arch_elf_state *_state = state;			\
	if ((phdr)->p_type == PT_S390_PGSTE &&			\
	    !page_table_allocate_pgste &&			\
	    !test_thread_flag(TIF_PGSTE) &&			\
	    !current->mm->context.alloc_pgste) {		\
		set_thread_flag(TIF_PGSTE);			\
		set_pt_regs_flag(task_pt_regs(current),		\
				 PIF_EXECVE_PGSTE_RESTART);	\
		_state->rc = -EAGAIN;				\
	}							\
	_state->rc;						\
})
#else
#define arch_elf_pt_proc(ehdr, phdr, elf, interp, state)	\
({								\
	(state)->rc;						\
})
#endif
#define ELF_PLAT_INIT(_r, load_addr) \
	do { \
		_r->gprs[14] = 0; \
	} while (0)
#define CORE_DUMP_USE_REGSET
#define ELF_EXEC_PAGESIZE	PAGE_SIZE
#define ELF_ET_DYN_BASE (is_compat_task() ? \
				(STACK_TOP / 3 * 2) : \
				(STACK_TOP / 3 * 2) & ~((1UL << 32) - 1))
extern unsigned long elf_hwcap;
#define ELF_HWCAP (elf_hwcap)
#define ELF_PLATFORM_SIZE 8
extern char elf_platform[];
#define ELF_PLATFORM (elf_platform)
#ifndef CONFIG_COMPAT
#define SET_PERSONALITY(ex) \
do {								\
	set_personality(PER_LINUX |				\
		(current->personality & (~PER_MASK)));		\
	current->thread.sys_call_table = sys_call_table;	\
} while (0)
#else  
#define SET_PERSONALITY(ex)					\
do {								\
	if (personality(current->personality) != PER_LINUX32)	\
		set_personality(PER_LINUX |			\
			(current->personality & ~PER_MASK));	\
	if ((ex).e_ident[EI_CLASS] == ELFCLASS32) {		\
		set_thread_flag(TIF_31BIT);			\
		current->thread.sys_call_table =		\
			sys_call_table_emu;			\
	} else {						\
		clear_thread_flag(TIF_31BIT);			\
		current->thread.sys_call_table =		\
			sys_call_table;				\
	}							\
} while (0)
#endif  
#define BRK_RND_MASK	(is_compat_task() ? 0x7ffUL : 0x1fffUL)
#define MMAP_RND_MASK	(is_compat_task() ? 0x7ffUL : 0x3ff80UL)
#define MMAP_ALIGN_MASK	(is_compat_task() ? 0 : 0x7fUL)
#define STACK_RND_MASK	MMAP_RND_MASK
#define ARCH_DLINFO							\
do {									\
	NEW_AUX_ENT(AT_SYSINFO_EHDR,					\
		    (unsigned long)current->mm->context.vdso_base);	\
} while (0)
struct linux_binprm;
#define ARCH_HAS_SETUP_ADDITIONAL_PAGES 1
int arch_setup_additional_pages(struct linux_binprm *, int);
#endif
