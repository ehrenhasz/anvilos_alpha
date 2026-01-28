#ifndef _ASM_SEMBUF_H
#define _ASM_SEMBUF_H
#include <asm/ipcbuf.h>
#ifdef __mips64
struct semid64_ds {
	struct ipc64_perm sem_perm;		 
	long		 sem_otime;		 
	long		 sem_ctime;		 
	unsigned long	sem_nsems;		 
	unsigned long	__unused1;
	unsigned long	__unused2;
};
#else
struct semid64_ds {
	struct ipc64_perm sem_perm;		 
	unsigned long   sem_otime;		 
	unsigned long   sem_ctime;		 
	unsigned long	sem_nsems;		 
	unsigned long	sem_otime_high;
	unsigned long	sem_ctime_high;
};
#endif
#endif  
