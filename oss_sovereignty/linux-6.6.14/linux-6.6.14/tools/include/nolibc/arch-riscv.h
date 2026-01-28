#ifndef _NOLIBC_ARCH_RISCV_H
#define _NOLIBC_ARCH_RISCV_H
#include "compiler.h"
#include "crt.h"
#define __ARCH_WANT_SYS_PSELECT6
#define my_syscall0(num)                                                      \
({                                                                            \
	register long _num  __asm__ ("a7") = (num);                           \
	register long _arg1 __asm__ ("a0");                                   \
									      \
	__asm__ volatile (                                                    \
		"ecall\n\t"                                                   \
		: "=r"(_arg1)                                                 \
		: "r"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_arg1;                                                                \
})
#define my_syscall1(num, arg1)                                                \
({                                                                            \
	register long _num  __asm__ ("a7") = (num);                           \
	register long _arg1 __asm__ ("a0") = (long)(arg1);		      \
									      \
	__asm__ volatile (                                                    \
		"ecall\n"                                                     \
		: "+r"(_arg1)                                                 \
		: "r"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_arg1;                                                                \
})
#define my_syscall2(num, arg1, arg2)                                          \
({                                                                            \
	register long _num  __asm__ ("a7") = (num);                           \
	register long _arg1 __asm__ ("a0") = (long)(arg1);                    \
	register long _arg2 __asm__ ("a1") = (long)(arg2);                    \
									      \
	__asm__ volatile (                                                    \
		"ecall\n"                                                     \
		: "+r"(_arg1)                                                 \
		: "r"(_arg2),                                                 \
		  "r"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_arg1;                                                                \
})
#define my_syscall3(num, arg1, arg2, arg3)                                    \
({                                                                            \
	register long _num  __asm__ ("a7") = (num);                           \
	register long _arg1 __asm__ ("a0") = (long)(arg1);                    \
	register long _arg2 __asm__ ("a1") = (long)(arg2);                    \
	register long _arg3 __asm__ ("a2") = (long)(arg3);                    \
									      \
	__asm__ volatile (                                                    \
		"ecall\n\t"                                                   \
		: "+r"(_arg1)                                                 \
		: "r"(_arg2), "r"(_arg3),                                     \
		  "r"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_arg1;                                                                \
})
#define my_syscall4(num, arg1, arg2, arg3, arg4)                              \
({                                                                            \
	register long _num  __asm__ ("a7") = (num);                           \
	register long _arg1 __asm__ ("a0") = (long)(arg1);                    \
	register long _arg2 __asm__ ("a1") = (long)(arg2);                    \
	register long _arg3 __asm__ ("a2") = (long)(arg3);                    \
	register long _arg4 __asm__ ("a3") = (long)(arg4);                    \
									      \
	__asm__ volatile (                                                    \
		"ecall\n"                                                     \
		: "+r"(_arg1)                                                 \
		: "r"(_arg2), "r"(_arg3), "r"(_arg4),                         \
		  "r"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_arg1;                                                                \
})
#define my_syscall5(num, arg1, arg2, arg3, arg4, arg5)                        \
({                                                                            \
	register long _num  __asm__ ("a7") = (num);                           \
	register long _arg1 __asm__ ("a0") = (long)(arg1);                    \
	register long _arg2 __asm__ ("a1") = (long)(arg2);                    \
	register long _arg3 __asm__ ("a2") = (long)(arg3);                    \
	register long _arg4 __asm__ ("a3") = (long)(arg4);                    \
	register long _arg5 __asm__ ("a4") = (long)(arg5);                    \
									      \
	__asm__ volatile (                                                    \
		"ecall\n"                                                     \
		: "+r"(_arg1)                                                 \
		: "r"(_arg2), "r"(_arg3), "r"(_arg4), "r"(_arg5),             \
		  "r"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_arg1;                                                                \
})
#define my_syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6)                  \
({                                                                            \
	register long _num  __asm__ ("a7") = (num);                           \
	register long _arg1 __asm__ ("a0") = (long)(arg1);                    \
	register long _arg2 __asm__ ("a1") = (long)(arg2);                    \
	register long _arg3 __asm__ ("a2") = (long)(arg3);                    \
	register long _arg4 __asm__ ("a3") = (long)(arg4);                    \
	register long _arg5 __asm__ ("a4") = (long)(arg5);                    \
	register long _arg6 __asm__ ("a5") = (long)(arg6);                    \
									      \
	__asm__ volatile (                                                    \
		"ecall\n"                                                     \
		: "+r"(_arg1)                                                 \
		: "r"(_arg2), "r"(_arg3), "r"(_arg4), "r"(_arg5), "r"(_arg6), \
		  "r"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_arg1;                                                                \
})
void __attribute__((weak, noreturn, optimize("Os", "omit-frame-pointer"))) __no_stack_protector _start(void)
{
	__asm__ volatile (
		".option push\n"
		".option norelax\n"
		"lla  gp, __global_pointer$\n"
		".option pop\n"
		"mv   a0, sp\n"            
		"andi sp, a0, -16\n"       
		"call _start_c\n"          
	);
	__builtin_unreachable();
}
#endif  
