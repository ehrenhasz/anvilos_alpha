#ifndef _ASM_UNISTD_H
#define _ASM_UNISTD_H
#include <uapi/asm/unistd.h>
#include <asm/unistd_nr_n32.h>
#include <asm/unistd_nr_n64.h>
#include <asm/unistd_nr_o32.h>
#define __NR_N32_Linux	6000
#define __NR_64_Linux	5000
#define __NR_O32_Linux	4000
#ifdef CONFIG_MIPS32_N32
#define NR_syscalls  (__NR_N32_Linux + __NR_N32_Linux_syscalls)
#elif defined(CONFIG_64BIT)
#define NR_syscalls  (__NR_64_Linux + __NR_64_Linux_syscalls)
#else
#define NR_syscalls  (__NR_O32_Linux + __NR_O32_Linux_syscalls)
#endif
#ifndef __ASSEMBLY__
#define __ARCH_WANT_NEW_STAT
#define __ARCH_WANT_OLD_READDIR
#define __ARCH_WANT_SYS_ALARM
#define __ARCH_WANT_SYS_GETHOSTNAME
#define __ARCH_WANT_SYS_IPC
#define __ARCH_WANT_SYS_PAUSE
#define __ARCH_WANT_SYS_UTIME
#define __ARCH_WANT_SYS_UTIME32
#define __ARCH_WANT_SYS_WAITPID
#define __ARCH_WANT_SYS_SOCKETCALL
#define __ARCH_WANT_SYS_GETPGRP
#define __ARCH_WANT_SYS_NICE
#define __ARCH_WANT_SYS_OLD_UNAME
#define __ARCH_WANT_SYS_OLDUMOUNT
#define __ARCH_WANT_SYS_SIGPENDING
#define __ARCH_WANT_SYS_SIGPROCMASK
# ifdef CONFIG_32BIT
#  define __ARCH_WANT_STAT64
#  define __ARCH_WANT_SYS_TIME32
# else
#  define __ARCH_WANT_COMPAT_STAT
# endif
# ifdef CONFIG_MIPS32_O32
#  define __ARCH_WANT_SYS_TIME32
# endif
#define __ARCH_WANT_SYS_FORK
#define __ARCH_WANT_SYS_CLONE
#define __ARCH_WANT_SYS_CLONE3
#define __IGNORE_fadvise64_64
#endif  
#endif  
