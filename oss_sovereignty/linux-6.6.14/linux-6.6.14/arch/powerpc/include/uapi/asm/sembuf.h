#ifndef _ASM_POWERPC_SEMBUF_H
#define _ASM_POWERPC_SEMBUF_H
#include <asm/ipcbuf.h>
struct semid64_ds {
	struct ipc64_perm sem_perm;	 
#ifndef __powerpc64__
	unsigned long	sem_otime_high;
	unsigned long	sem_otime;	 
	unsigned long	sem_ctime_high;
	unsigned long	sem_ctime;	 
#else
	long		sem_otime;	 
	long		sem_ctime;	 
#endif
	unsigned long	sem_nsems;	 
	unsigned long	__unused3;
	unsigned long	__unused4;
};
#endif	 
