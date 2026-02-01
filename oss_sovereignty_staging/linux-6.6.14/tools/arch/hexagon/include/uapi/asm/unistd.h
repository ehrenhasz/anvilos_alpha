 
 

 

#define sys_mmap2 sys_mmap_pgoff
#define __ARCH_WANT_RENAMEAT
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_SET_GET_RLIMIT
#define __ARCH_WANT_SYS_EXECVE
#define __ARCH_WANT_SYS_CLONE
#define __ARCH_WANT_SYS_VFORK
#define __ARCH_WANT_SYS_FORK
#define __ARCH_WANT_TIME32_SYSCALLS

#include <asm-generic/unistd.h>
