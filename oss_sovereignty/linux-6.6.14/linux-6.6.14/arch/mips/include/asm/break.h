#ifndef __ASM_BREAK_H
#define __ASM_BREAK_H
#ifdef __UAPI_ASM_BREAK_H
#error "Error: Do not directly include <uapi/asm/break.h>"
#endif
#include <uapi/asm/break.h>
#define BRK_KDB		513	 
#define BRK_MEMU	514	 
#define BRK_KPROBE_BP	515	 
#define BRK_KPROBE_SSTEPBP 516	 
#define BRK_MULOVF	1023	 
#endif  
