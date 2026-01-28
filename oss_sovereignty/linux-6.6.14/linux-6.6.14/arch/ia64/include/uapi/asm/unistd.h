#ifndef _UAPI_ASM_IA64_UNISTD_H
#define _UAPI_ASM_IA64_UNISTD_H
#include <asm/break.h>
#define __BREAK_SYSCALL	__IA64_BREAK_SYSCALL
#define __NR_Linux      1024
#define __NR_umount __NR_umount2
#include <asm/unistd_64.h>
#endif  
