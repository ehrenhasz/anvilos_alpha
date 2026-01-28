#ifndef _NOLIBC_ARCH_POWERPC_H
#define _NOLIBC_ARCH_POWERPC_H
#include "compiler.h"
#include "crt.h"
#define _NOLIBC_SYSCALL_CLOBBERLIST \
	"memory", "cr0", "r12", "r11", "r10", "r9"
#define my_syscall0(num)                                                     \
({                                                                           \
	register long _ret  __asm__ ("r3");                                  \
	register long _num  __asm__ ("r0") = (num);                          \
									     \
	__asm__ volatile (                                                   \
		"	sc\n"                                                \
		"	bns+ 1f\n"                                           \
		"	neg  %0, %0\n"                                       \
		"1:\n"                                                       \
		: "=r"(_ret), "+r"(_num)                                     \
		:                                                            \
		: _NOLIBC_SYSCALL_CLOBBERLIST, "r8", "r7", "r6", "r5", "r4"  \
	);                                                                   \
	_ret;                                                                \
})
#define my_syscall1(num, arg1)                                               \
({                                                                           \
	register long _ret  __asm__ ("r3");                                  \
	register long _num  __asm__ ("r0") = (num);                          \
	register long _arg1 __asm__ ("r3") = (long)(arg1);                   \
									     \
	__asm__ volatile (                                                   \
		"	sc\n"                                                \
		"	bns+ 1f\n"                                           \
		"	neg  %0, %0\n"                                       \
		"1:\n"                                                       \
		: "=r"(_ret), "+r"(_num)                                     \
		: "0"(_arg1)                                                 \
		: _NOLIBC_SYSCALL_CLOBBERLIST, "r8", "r7", "r6", "r5", "r4"  \
	);                                                                   \
	_ret;                                                                \
})
#define my_syscall2(num, arg1, arg2)                                         \
({                                                                           \
	register long _ret  __asm__ ("r3");                                  \
	register long _num  __asm__ ("r0") = (num);                          \
	register long _arg1 __asm__ ("r3") = (long)(arg1);                   \
	register long _arg2 __asm__ ("r4") = (long)(arg2);                   \
									     \
	__asm__ volatile (                                                   \
		"	sc\n"                                                \
		"	bns+ 1f\n"                                           \
		"	neg  %0, %0\n"                                       \
		"1:\n"                                                       \
		: "=r"(_ret), "+r"(_num), "+r"(_arg2)                        \
		: "0"(_arg1)                                                 \
		: _NOLIBC_SYSCALL_CLOBBERLIST, "r8", "r7", "r6", "r5"        \
	);                                                                   \
	_ret;                                                                \
})
#define my_syscall3(num, arg1, arg2, arg3)                                   \
({                                                                           \
	register long _ret  __asm__ ("r3");                                  \
	register long _num  __asm__ ("r0") = (num);                          \
	register long _arg1 __asm__ ("r3") = (long)(arg1);                   \
	register long _arg2 __asm__ ("r4") = (long)(arg2);                   \
	register long _arg3 __asm__ ("r5") = (long)(arg3);                   \
									     \
	__asm__ volatile (                                                   \
		"	sc\n"                                                \
		"	bns+ 1f\n"                                           \
		"	neg  %0, %0\n"                                       \
		"1:\n"                                                       \
		: "=r"(_ret), "+r"(_num), "+r"(_arg2), "+r"(_arg3)           \
		: "0"(_arg1)                                                 \
		: _NOLIBC_SYSCALL_CLOBBERLIST, "r8", "r7", "r6"              \
	);                                                                   \
	_ret;                                                                \
})
#define my_syscall4(num, arg1, arg2, arg3, arg4)                             \
({                                                                           \
	register long _ret  __asm__ ("r3");                                  \
	register long _num  __asm__ ("r0") = (num);                          \
	register long _arg1 __asm__ ("r3") = (long)(arg1);                   \
	register long _arg2 __asm__ ("r4") = (long)(arg2);                   \
	register long _arg3 __asm__ ("r5") = (long)(arg3);                   \
	register long _arg4 __asm__ ("r6") = (long)(arg4);                   \
									     \
	__asm__ volatile (                                                   \
		"	sc\n"                                                \
		"	bns+ 1f\n"                                           \
		"	neg  %0, %0\n"                                       \
		"1:\n"                                                       \
		: "=r"(_ret), "+r"(_num), "+r"(_arg2), "+r"(_arg3),          \
		  "+r"(_arg4)                                                \
		: "0"(_arg1)                                                 \
		: _NOLIBC_SYSCALL_CLOBBERLIST, "r8", "r7"                    \
	);                                                                   \
	_ret;                                                                \
})
#define my_syscall5(num, arg1, arg2, arg3, arg4, arg5)                       \
({                                                                           \
	register long _ret  __asm__ ("r3");                                  \
	register long _num  __asm__ ("r0") = (num);                          \
	register long _arg1 __asm__ ("r3") = (long)(arg1);                   \
	register long _arg2 __asm__ ("r4") = (long)(arg2);                   \
	register long _arg3 __asm__ ("r5") = (long)(arg3);                   \
	register long _arg4 __asm__ ("r6") = (long)(arg4);                   \
	register long _arg5 __asm__ ("r7") = (long)(arg5);                   \
									     \
	__asm__ volatile (                                                   \
		"	sc\n"                                                \
		"	bns+ 1f\n"                                           \
		"	neg  %0, %0\n"                                       \
		"1:\n"                                                       \
		: "=r"(_ret), "+r"(_num), "+r"(_arg2), "+r"(_arg3),          \
		  "+r"(_arg4), "+r"(_arg5)                                   \
		: "0"(_arg1)                                                 \
		: _NOLIBC_SYSCALL_CLOBBERLIST, "r8"                          \
	);                                                                   \
	_ret;                                                                \
})
#define my_syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6)                 \
({                                                                           \
	register long _ret  __asm__ ("r3");                                  \
	register long _num  __asm__ ("r0") = (num);                          \
	register long _arg1 __asm__ ("r3") = (long)(arg1);                   \
	register long _arg2 __asm__ ("r4") = (long)(arg2);                   \
	register long _arg3 __asm__ ("r5") = (long)(arg3);                   \
	register long _arg4 __asm__ ("r6") = (long)(arg4);                   \
	register long _arg5 __asm__ ("r7") = (long)(arg5);                   \
	register long _arg6 __asm__ ("r8") = (long)(arg6);                   \
									     \
	__asm__ volatile (                                                   \
		"	sc\n"                                                \
		"	bns+ 1f\n"                                           \
		"	neg  %0, %0\n"                                       \
		"1:\n"                                                       \
		: "=r"(_ret), "+r"(_num), "+r"(_arg2), "+r"(_arg3),          \
		  "+r"(_arg4), "+r"(_arg5), "+r"(_arg6)                      \
		: "0"(_arg1)                                                 \
		: _NOLIBC_SYSCALL_CLOBBERLIST                                \
	);                                                                   \
	_ret;                                                                \
})
#ifndef __powerpc64__
#ifdef __no_stack_protector
#undef __no_stack_protector
#define __no_stack_protector __attribute__((__optimize__("-fno-stack-protector")))
#endif
#endif  
void __attribute__((weak, noreturn, optimize("Os", "omit-frame-pointer"))) __no_stack_protector _start(void)
{
#ifdef __powerpc64__
#if _CALL_ELF == 2
	__asm__ volatile (
		"addis  2, 12, .TOC. - _start@ha\n"
		"addi   2,  2, .TOC. - _start@l\n"
	);
#endif  
	__asm__ volatile (
		"mr     3, 1\n"          
		"clrrdi 1, 1, 4\n"       
		"li     0, 0\n"          
		"stdu   1, -32(1)\n"     
		"bl     _start_c\n"      
	);
#else
	__asm__ volatile (
		"mr     3, 1\n"          
		"clrrwi 1, 1, 4\n"       
		"li     0, 0\n"          
		"stwu   1, -16(1)\n"     
		"bl     _start_c\n"      
	);
#endif
	__builtin_unreachable();
}
#endif  
