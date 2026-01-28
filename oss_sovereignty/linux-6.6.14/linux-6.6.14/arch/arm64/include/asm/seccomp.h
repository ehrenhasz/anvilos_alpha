#ifndef _ASM_SECCOMP_H
#define _ASM_SECCOMP_H
#include <asm/unistd.h>
#ifdef CONFIG_COMPAT
#define __NR_seccomp_read_32		__NR_compat_read
#define __NR_seccomp_write_32		__NR_compat_write
#define __NR_seccomp_exit_32		__NR_compat_exit
#define __NR_seccomp_sigreturn_32	__NR_compat_rt_sigreturn
#endif  
#include <asm-generic/seccomp.h>
#define SECCOMP_ARCH_NATIVE		AUDIT_ARCH_AARCH64
#define SECCOMP_ARCH_NATIVE_NR		NR_syscalls
#define SECCOMP_ARCH_NATIVE_NAME	"aarch64"
#ifdef CONFIG_COMPAT
# define SECCOMP_ARCH_COMPAT		AUDIT_ARCH_ARM
# define SECCOMP_ARCH_COMPAT_NR	__NR_compat_syscalls
# define SECCOMP_ARCH_COMPAT_NAME	"arm"
#endif
#endif  
