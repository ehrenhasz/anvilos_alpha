#ifndef __ASM_CSKY_ELF_H
#define __ASM_CSKY_ELF_H
#include <asm/ptrace.h>
#include <abi/regdef.h>
#define ELF_ARCH EM_CSKY
#define EM_CSKY_OLD 39
#define R_CSKY_NONE               0
#define R_CSKY_32                 1
#define R_CSKY_PCIMM8BY4          2
#define R_CSKY_PCIMM11BY2         3
#define R_CSKY_PCIMM4BY2          4
#define R_CSKY_PC32               5
#define R_CSKY_PCRELJSR_IMM11BY2  6
#define R_CSKY_GNU_VTINHERIT      7
#define R_CSKY_GNU_VTENTRY        8
#define R_CSKY_RELATIVE           9
#define R_CSKY_COPY               10
#define R_CSKY_GLOB_DAT           11
#define R_CSKY_JUMP_SLOT          12
#define R_CSKY_ADDR_HI16          24
#define R_CSKY_ADDR_LO16          25
#define R_CSKY_PCRELJSR_IMM26BY2  40
typedef unsigned long elf_greg_t;
typedef struct user_fp elf_fpregset_t;
#define ELF_NGREG ((sizeof(struct pt_regs) / sizeof(elf_greg_t)) - 2)
typedef elf_greg_t elf_gregset_t[ELF_NGREG];
#define elf_check_arch(x) (((x)->e_machine == ELF_ARCH) || \
			   ((x)->e_machine == EM_CSKY_OLD))
#define ELF_EXEC_PAGESIZE		4096
#define ELF_CLASS			ELFCLASS32
#define ELF_PLAT_INIT(_r, load_addr)	{ _r->a0 = 0; }
#ifdef __cskyBE__
#define ELF_DATA	ELFDATA2MSB
#else
#define ELF_DATA	ELFDATA2LSB
#endif
#define ELF_ET_DYN_BASE	0x0UL
#include <abi/elf.h>
struct task_struct;
extern int dump_task_regs(struct task_struct *tsk, elf_gregset_t *elf_regs);
#define ELF_CORE_COPY_TASK_REGS(tsk, elf_regs) dump_task_regs(tsk, elf_regs)
#define ELF_HWCAP	(0)
#define ELF_PLATFORM		(NULL)
#define SET_PERSONALITY(ex)	set_personality(PER_LINUX)
#define ARCH_HAS_SETUP_ADDITIONAL_PAGES 1
struct linux_binprm;
extern int arch_setup_additional_pages(struct linux_binprm *bprm,
				       int uses_interp);
#endif  
