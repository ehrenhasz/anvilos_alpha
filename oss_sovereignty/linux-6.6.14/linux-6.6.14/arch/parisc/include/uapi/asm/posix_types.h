#ifndef __ARCH_PARISC_POSIX_TYPES_H
#define __ARCH_PARISC_POSIX_TYPES_H
#ifndef __LP64__
typedef unsigned short		__kernel_mode_t;
#define __kernel_mode_t __kernel_mode_t
#endif
typedef unsigned short		__kernel_ipc_pid_t;
#define __kernel_ipc_pid_t __kernel_ipc_pid_t
typedef long long		__kernel_off64_t;
typedef unsigned long long	__kernel_ino64_t;
#include <asm-generic/posix_types.h>
#endif
