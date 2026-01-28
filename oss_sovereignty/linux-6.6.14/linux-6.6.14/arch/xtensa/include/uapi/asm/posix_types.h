#ifndef _XTENSA_POSIX_TYPES_H
#define _XTENSA_POSIX_TYPES_H
typedef unsigned short	__kernel_ipc_pid_t;
#define __kernel_ipc_pid_t __kernel_ipc_pid_t
typedef unsigned int	__kernel_size_t;
typedef int		__kernel_ssize_t;
typedef long		__kernel_ptrdiff_t;
#define __kernel_size_t __kernel_size_t
typedef unsigned short	__kernel_old_uid_t;
typedef unsigned short	__kernel_old_gid_t;
#define __kernel_old_uid_t __kernel_old_uid_t
typedef unsigned short	__kernel_old_dev_t;
#define __kernel_old_dev_t __kernel_old_dev_t
#include <asm-generic/posix_types.h>
#endif  
