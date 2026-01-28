#ifndef _UAPI_ASM_NIOS2_ELF_H
#define _UAPI_ASM_NIOS2_ELF_H
#include <linux/ptrace.h>
#define R_NIOS2_NONE		0
#define R_NIOS2_S16		1
#define R_NIOS2_U16		2
#define R_NIOS2_PCREL16		3
#define R_NIOS2_CALL26		4
#define R_NIOS2_IMM5		5
#define R_NIOS2_CACHE_OPX	6
#define R_NIOS2_IMM6		7
#define R_NIOS2_IMM8		8
#define R_NIOS2_HI16		9
#define R_NIOS2_LO16		10
#define R_NIOS2_HIADJ16		11
#define R_NIOS2_BFD_RELOC_32	12
#define R_NIOS2_BFD_RELOC_16	13
#define R_NIOS2_BFD_RELOC_8	14
#define R_NIOS2_GPREL		15
#define R_NIOS2_GNU_VTINHERIT	16
#define R_NIOS2_GNU_VTENTRY	17
#define R_NIOS2_UJMP		18
#define R_NIOS2_CJMP		19
#define R_NIOS2_CALLR		20
#define R_NIOS2_ALIGN		21
#define R_NIOS2_NUM		22
typedef unsigned long elf_greg_t;
#define ELF_NGREG		49
typedef elf_greg_t elf_gregset_t[ELF_NGREG];
typedef unsigned long elf_fpregset_t;
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_ALTERA_NIOS2
#endif  
