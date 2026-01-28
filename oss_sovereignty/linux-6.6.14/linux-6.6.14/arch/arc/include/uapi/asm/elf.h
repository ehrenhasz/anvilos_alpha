#ifndef _UAPI__ASM_ARC_ELF_H
#define _UAPI__ASM_ARC_ELF_H
#include <asm/ptrace.h>		 
#define EF_ARC_OSABI_MSK	0x00000f00
#define EF_ARC_OSABI_V3		0x00000300    
#define EF_ARC_OSABI_V4		0x00000400    
#if __GNUC__ < 6
#define EF_ARC_OSABI_CURRENT	EF_ARC_OSABI_V3
#else
#define EF_ARC_OSABI_CURRENT	EF_ARC_OSABI_V4
#endif
typedef unsigned long elf_greg_t;
typedef unsigned long elf_fpregset_t;
#define ELF_NGREG	(sizeof(struct user_regs_struct) / sizeof(elf_greg_t))
#define ELF_ARCV2REG	(sizeof(struct user_regs_arcv2) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];
#endif
