#ifndef _UAPI_ALPHA_UNISTD_H
#define _UAPI_ALPHA_UNISTD_H
#define __NR_umount	__NR_umount2
#define __NR_osf_shmat	__NR_shmat
#define __NR_getpid	__NR_getxpid
#define __NR_getuid	__NR_getxuid
#define __NR_getgid	__NR_getxgid
#include <asm/unistd_32.h>
#endif  
