#ifndef __ASM_OPENRISC_ELF_H
#define __ASM_OPENRISC_ELF_H
#include <linux/types.h>
#include <uapi/asm/elf.h>
#define elf_check_arch(x) \
	(((x)->e_machine == EM_OR32) || ((x)->e_machine == EM_OPENRISC))
#define ELF_ET_DYN_BASE         (0x08000000)
#define CORE_DUMP_USE_REGSET
#define ELF_EXEC_PAGESIZE	8192
extern void dump_elf_thread(elf_greg_t *dest, struct pt_regs *pt);
#define ELF_CORE_COPY_REGS(dest, regs) dump_elf_thread(dest, regs);
#define ELF_HWCAP	(0)
#define ELF_PLATFORM	(NULL)
#endif
