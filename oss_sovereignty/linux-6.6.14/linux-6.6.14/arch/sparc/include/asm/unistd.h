#ifndef _SPARC_UNISTD_H
#define _SPARC_UNISTD_H
#include <uapi/asm/unistd.h>
#define NR_syscalls	__NR_syscalls
#ifdef __32bit_syscall_numbers__
#else
#define __NR_time		231  
#endif
#define __ARCH_WANT_NEW_STAT
#define __ARCH_WANT_OLD_READDIR
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_SYS_ALARM
#define __ARCH_WANT_SYS_GETHOSTNAME
#define __ARCH_WANT_SYS_PAUSE
#define __ARCH_WANT_SYS_SIGNAL
#define __ARCH_WANT_SYS_TIME32
#define __ARCH_WANT_SYS_UTIME32
#define __ARCH_WANT_SYS_WAITPID
#define __ARCH_WANT_SYS_SOCKETCALL
#define __ARCH_WANT_SYS_FADVISE64
#define __ARCH_WANT_SYS_GETPGRP
#define __ARCH_WANT_SYS_NICE
#define __ARCH_WANT_SYS_OLDUMOUNT
#define __ARCH_WANT_SYS_SIGPENDING
#define __ARCH_WANT_SYS_SIGPROCMASK
#ifdef __32bit_syscall_numbers__
#define __ARCH_WANT_SYS_IPC
#else
#define __ARCH_WANT_SYS_TIME
#define __ARCH_WANT_SYS_UTIME
#define __ARCH_WANT_COMPAT_SYS_SENDFILE
#define __ARCH_WANT_COMPAT_STAT
#endif
#ifdef __32bit_syscall_numbers__
#define __IGNORE_setresuid
#define __IGNORE_getresuid
#define __IGNORE_setresgid
#define __IGNORE_getresgid
#endif
#endif  
