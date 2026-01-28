#if !defined(_UAPI_ASM_ARC_UNISTD_H) || defined(__SYSCALL)
#define _UAPI_ASM_ARC_UNISTD_H
#define __ARCH_WANT_RENAMEAT
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_SET_GET_RLIMIT
#define __ARCH_WANT_SYS_EXECVE
#define __ARCH_WANT_SYS_CLONE
#define __ARCH_WANT_SYS_VFORK
#define __ARCH_WANT_SYS_FORK
#define __ARCH_WANT_TIME32_SYSCALLS
#define sys_mmap2 sys_mmap_pgoff
#include <asm-generic/unistd.h>
#define NR_syscalls	__NR_syscalls
#define __NR_sysfs		(__NR_arch_specific_syscall + 3)
#define __NR_cacheflush		(__NR_arch_specific_syscall + 0)
#define __NR_arc_settls		(__NR_arch_specific_syscall + 1)
#define __NR_arc_gettls		(__NR_arch_specific_syscall + 2)
#define __NR_arc_usr_cmpxchg	(__NR_arch_specific_syscall + 4)
__SYSCALL(__NR_cacheflush, sys_cacheflush)
__SYSCALL(__NR_arc_settls, sys_arc_settls)
__SYSCALL(__NR_arc_gettls, sys_arc_gettls)
__SYSCALL(__NR_arc_usr_cmpxchg, sys_arc_usr_cmpxchg)
__SYSCALL(__NR_sysfs, sys_sysfs)
#undef __SYSCALL
#endif
