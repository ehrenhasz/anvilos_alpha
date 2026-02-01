 
 

#ifndef _NOLIBC_ARCH_I386_H
#define _NOLIBC_ARCH_I386_H

#include "compiler.h"
#include "crt.h"

 
#define __ARCH_WANT_SYS_OLD_SELECT

#define my_syscall0(num)                                                      \
({                                                                            \
	long _ret;                                                            \
	register long _num __asm__ ("eax") = (num);                           \
									      \
	__asm__ volatile (                                                    \
		"int $0x80\n"                                                 \
		: "=a" (_ret)                                                 \
		: "0"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_ret;                                                                 \
})

#define my_syscall1(num, arg1)                                                \
({                                                                            \
	long _ret;                                                            \
	register long _num __asm__ ("eax") = (num);                           \
	register long _arg1 __asm__ ("ebx") = (long)(arg1);                   \
									      \
	__asm__ volatile (                                                    \
		"int $0x80\n"                                                 \
		: "=a" (_ret)                                                 \
		: "r"(_arg1),                                                 \
		  "0"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_ret;                                                                 \
})

#define my_syscall2(num, arg1, arg2)                                          \
({                                                                            \
	long _ret;                                                            \
	register long _num __asm__ ("eax") = (num);                           \
	register long _arg1 __asm__ ("ebx") = (long)(arg1);                   \
	register long _arg2 __asm__ ("ecx") = (long)(arg2);                   \
									      \
	__asm__ volatile (                                                    \
		"int $0x80\n"                                                 \
		: "=a" (_ret)                                                 \
		: "r"(_arg1), "r"(_arg2),                                     \
		  "0"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_ret;                                                                 \
})

#define my_syscall3(num, arg1, arg2, arg3)                                    \
({                                                                            \
	long _ret;                                                            \
	register long _num __asm__ ("eax") = (num);                           \
	register long _arg1 __asm__ ("ebx") = (long)(arg1);                   \
	register long _arg2 __asm__ ("ecx") = (long)(arg2);                   \
	register long _arg3 __asm__ ("edx") = (long)(arg3);                   \
									      \
	__asm__ volatile (                                                    \
		"int $0x80\n"                                                 \
		: "=a" (_ret)                                                 \
		: "r"(_arg1), "r"(_arg2), "r"(_arg3),                         \
		  "0"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_ret;                                                                 \
})

#define my_syscall4(num, arg1, arg2, arg3, arg4)                              \
({                                                                            \
	long _ret;                                                            \
	register long _num __asm__ ("eax") = (num);                           \
	register long _arg1 __asm__ ("ebx") = (long)(arg1);                   \
	register long _arg2 __asm__ ("ecx") = (long)(arg2);                   \
	register long _arg3 __asm__ ("edx") = (long)(arg3);                   \
	register long _arg4 __asm__ ("esi") = (long)(arg4);                   \
									      \
	__asm__ volatile (                                                    \
		"int $0x80\n"                                                 \
		: "=a" (_ret)                                                 \
		: "r"(_arg1), "r"(_arg2), "r"(_arg3), "r"(_arg4),             \
		  "0"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_ret;                                                                 \
})

#define my_syscall5(num, arg1, arg2, arg3, arg4, arg5)                        \
({                                                                            \
	long _ret;                                                            \
	register long _num __asm__ ("eax") = (num);                           \
	register long _arg1 __asm__ ("ebx") = (long)(arg1);                   \
	register long _arg2 __asm__ ("ecx") = (long)(arg2);                   \
	register long _arg3 __asm__ ("edx") = (long)(arg3);                   \
	register long _arg4 __asm__ ("esi") = (long)(arg4);                   \
	register long _arg5 __asm__ ("edi") = (long)(arg5);                   \
									      \
	__asm__ volatile (                                                    \
		"int $0x80\n"                                                 \
		: "=a" (_ret)                                                 \
		: "r"(_arg1), "r"(_arg2), "r"(_arg3), "r"(_arg4), "r"(_arg5), \
		  "0"(_num)                                                   \
		: "memory", "cc"                                              \
	);                                                                    \
	_ret;                                                                 \
})

#define my_syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6)	\
({								\
	long _eax  = (long)(num);				\
	long _arg6 = (long)(arg6);  	\
	__asm__ volatile (					\
		"pushl	%[_arg6]\n\t"				\
		"pushl	%%ebp\n\t"				\
		"movl	4(%%esp),%%ebp\n\t"			\
		"int	$0x80\n\t"				\
		"popl	%%ebp\n\t"				\
		"addl	$4,%%esp\n\t"				\
		: "+a"(_eax)		 		\
		: "b"(arg1),		 		\
		  "c"(arg2),		 		\
		  "d"(arg3),		 		\
		  "S"(arg4),		 		\
		  "D"(arg5),		 		\
		  [_arg6]"m"(_arg6)	 		\
		: "memory", "cc"				\
	);							\
	_eax;							\
})

 
 
void __attribute__((weak, noreturn, optimize("Os", "omit-frame-pointer"))) __no_stack_protector _start(void)
{
	__asm__ volatile (
		"xor  %ebp, %ebp\n"        
		"mov  %esp, %eax\n"        
		"add  $12, %esp\n"         
		"and  $-16, %esp\n"        
		"sub  $12, %esp\n"         
		"push %eax\n"              
		"call _start_c\n"          
		"hlt\n"                    
	);
	__builtin_unreachable();
}

#endif  
