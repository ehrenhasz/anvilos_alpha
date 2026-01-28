#define sys_mmap2 sys_mmap_pgoff
#define __ARCH_WANT_RENAMEAT
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_SET_GET_RLIMIT
#define __ARCH_WANT_SYS_FORK
#define __ARCH_WANT_SYS_CLONE
#define __ARCH_WANT_SYS_CLONE3
#define __ARCH_WANT_TIME32_SYSCALLS
#include <asm-generic/unistd.h>
#define __NR_or1k_atomic __NR_arch_specific_syscall
__SYSCALL(__NR_or1k_atomic, sys_or1k_atomic)
