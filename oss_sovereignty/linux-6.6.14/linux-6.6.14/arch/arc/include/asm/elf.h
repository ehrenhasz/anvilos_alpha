#ifndef __ASM_ARC_ELF_H
#define __ASM_ARC_ELF_H
#include <linux/types.h>
#include <linux/elf-em.h>
#include <uapi/asm/elf.h>
#define EM_ARC_INUSE		(IS_ENABLED(CONFIG_ISA_ARCOMPACT) ? \
					EM_ARCOMPACT : EM_ARCV2)
#define  R_ARC_32		0x4
#define  R_ARC_32_ME		0x1B
#define  R_ARC_32_PCREL		0x31
#define ELF_ARCH		EM_ARC_INUSE
#define ELF_CLASS		ELFCLASS32
#ifdef CONFIG_CPU_BIG_ENDIAN
#define ELF_DATA		ELFDATA2MSB
#else
#define ELF_DATA		ELFDATA2LSB
#endif
struct elf32_hdr;
extern int elf_check_arch(const struct elf32_hdr *);
#define elf_check_arch	elf_check_arch
#define CORE_DUMP_USE_REGSET
#define ELF_EXEC_PAGESIZE	PAGE_SIZE
#define ELF_ET_DYN_BASE		(2UL * TASK_SIZE / 3)
#define ELF_PLAT_INIT(_r, load_addr)	((_r)->r0 = 0)
#define ELF_HWCAP	(0)
#define ELF_PLATFORM	(NULL)
#endif
